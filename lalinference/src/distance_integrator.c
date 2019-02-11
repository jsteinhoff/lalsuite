/*
 * Copyright (C) 2013-2017  Leo Singer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with with program; see the file COPYING. If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 */
#include <stdlib.h>
#include "bayestar_cosmology.h"
#include "omp_interruptible.h"

#include <assert.h>

#include <lal/LALError.h>
#include <lal/LALMalloc.h>

#include <gsl/gsl_integration.h>
#include <gsl/gsl_interp.h>
#include <gsl/gsl_sf_bessel.h>
#include <gsl/gsl_sf_exp.h>
#include <gsl/gsl_errno.h>


#include <lal/distance_integrator.h>

#ifndef _OPENMP
#define omp ignore
#endif





/* Uniform-in-comoving volume prior for the WMAP9 cosmology.
 * This is implemented as a cubic spline interpolant.
 *
 * The following static variables are defined in bayestar_cosmology.h, which
 * is automatically generated by bayestar_cosmology.py:
 *     - dVC_dVL_data
 *     - dVC_dVL_tmin
 *     - dVC_dVL_dt
 *     - dVC_dVL_high_z_slope
 *     - dVC_dVL_high_z_intercept
 */
static gsl_spline *dVC_dVL_interp = NULL;

double log_dVC_dVL(double DL)
{
    const double log_DL = log(DL);
    if (log_DL <= dVC_dVL_tmin)
    {
        return 0.0;
    } else if (log_DL >= dVC_dVL_tmax) {
        return dVC_dVL_high_z_slope * log_DL + dVC_dVL_high_z_intercept;
    } else {
        return gsl_spline_eval(dVC_dVL_interp, log_DL, NULL);
    }
}

void dVC_dVL_init(void)
{
    const size_t len = sizeof(dVC_dVL_data) / sizeof(*dVC_dVL_data);
    dVC_dVL_interp = gsl_spline_alloc(gsl_interp_cspline, len);
    assert(dVC_dVL_interp);
    double x[len];
    for (size_t i = 0; i < len; i ++)
        x[i] = dVC_dVL_tmin + i * dVC_dVL_dt;
    int ret = gsl_spline_init(dVC_dVL_interp, x, dVC_dVL_data, len);
    assert(ret == GSL_SUCCESS);
}

static double radial_integrand(double r, void *params)
{
    const radial_integrand_params *integrand_params = params;
    const double scale = integrand_params->scale;
    const double p = integrand_params->p;
    const double b = integrand_params->b;
    const int k = integrand_params->k;
    gsl_error_handler_t *old_handler = gsl_set_error_handler_off();
    double ret = scale - gsl_pow_2(p / r - 0.5 * b / p);
    int retval=GSL_SUCCESS;
    gsl_sf_result result;
    if (integrand_params->cosmology)
        ret += log_dVC_dVL(r);
    retval = gsl_sf_exp_mult_e(
        ret, gsl_sf_bessel_I0_scaled(b / r) * gsl_pow_int(r, k),
        &result);
    gsl_set_error_handler(old_handler);
    switch (retval)
    {
        case GSL_SUCCESS:
            return(result.val);
            break;
        case GSL_EUNDRFLW:
            return(0); /* Underflow */
            break;
        default:
            XLAL_ERROR(XLAL_EFUNC,"GSL error %s",gsl_strerror(retval));
    }
}


double log_radial_integrand(double r, void *params)
{
    const radial_integrand_params *integrand_params = params;
    const double scale = integrand_params->scale;
    const double p = integrand_params->p;
    const double b = integrand_params->b;
    const int k = integrand_params->k;
    double ret = log(gsl_sf_bessel_I0_scaled(b / r) * gsl_pow_int(r, k))
        + scale - gsl_pow_2(p / r - 0.5 * b / p);
    if (integrand_params->cosmology)
        ret += log_dVC_dVL(r);
    return ret;
}


double log_radial_integral(double r1, double r2, double p, double b, int k, int cosmology)
{
    radial_integrand_params params = {0, p, b, k, cosmology};
    double breakpoints[5];
    unsigned char nbreakpoints = 0;
    double result = 0, abserr, log_offset = -INFINITY;
    int ret;

    if (b != 0) {
        /* Calculate the approximate distance at which the integrand attains a
         * maximum (middle) and a fraction eta of the maximum (left and right).
         * This neglects the scaled Bessel function factors and the power-law
         * distance prior. It assumes that the likelihood is approximately of
         * the form
         *
         *    -p^2/r^2 + B/r.
         *
         * Then the middle breakpoint occurs at 1/r = -B/2A, and the left and
         * right breakpoints occur when
         *
         *   A/r^2 + B/r = log(eta) - B^2/4A.
         */

        static const double eta = 0.01;
        const double middle = 2 * gsl_pow_2(p) / b;
        const double left = 1 / (1 / middle + sqrt(-log(eta)) / p);
        const double right = 1 / (1 / middle - sqrt(-log(eta)) / p);

        /* Use whichever of the middle, left, and right points lie within the
         * integration limits as initial subdivisions for the adaptive
         * integrator. */

        breakpoints[nbreakpoints++] = r1;
        if(left > breakpoints[nbreakpoints-1] && left < r2)
            breakpoints[nbreakpoints++] = left;
        if(middle > breakpoints[nbreakpoints-1] && middle < r2)
            breakpoints[nbreakpoints++] = middle;
        if(right > breakpoints[nbreakpoints-1] && right < r2)
            breakpoints[nbreakpoints++] = right;
        breakpoints[nbreakpoints++] = r2;
    } else {
        /* Inner breakpoints are undefined because b = 0. */
        breakpoints[nbreakpoints++] = r1;
        breakpoints[nbreakpoints++] = r2;
    }

    /* Re-scale the integrand so that the maximum value at any of the
     * breakpoints is 1. Note that the initial value of the constant term
     * is overwritten. */

    for (unsigned char i = 0; i < nbreakpoints; i++)
    {
        double new_log_offset = log_radial_integrand(breakpoints[i], &params);
        if (new_log_offset > log_offset)
            log_offset = new_log_offset;
    }

    /* If the largest value of the log integrand was -INFINITY, then the
     * integrand is 0 everywhere. Set log_offset to 0, because subtracting
     * -INFINITY would make the integrand infinite. */
    if (log_offset == -INFINITY)
        log_offset = 0;

    params.scale = -log_offset;
    size_t n = 64;
    double abstol = 1e-8;
    double reltol = 1e-8;
    int relaxed=0;
    do {
        /* Maximum number of subdivisions for adaptive integration. */


        /* Allocate workspace on stack. Hopefully, a little bit faster than
         * using the heap in multi-threaded code. */

        double alist[n];
        double blist[n];
        double rlist[n];
        double elist[n];
        size_t order[n];
        size_t level[n];
        gsl_integration_workspace workspace = {
            .alist = alist,
            .blist = blist,
            .rlist = rlist,
            .elist = elist,
            .order = order,
            .level = level,
            .limit = n
        };

        /* Set up integrand data structure. */
        const gsl_function func = {radial_integrand, &params};
        //gsl_error_handler_t *old_handler = gsl_set_error_handler_off();

        /* Perform adaptive Gaussian quadrature. */
        ret = gsl_integration_qagp(&func, breakpoints, nbreakpoints,
            abstol, reltol, n, &workspace, &result, &abserr);
        switch(ret)
        {
		case GSL_EROUND:
			relaxed=1;
			abstol*=2;
			break;
		case GSL_EMAXITER:
			fprintf(stderr,"GSL error %s, increasing n to %li\n",gsl_strerror(ret),n*=2);
			break;
		case GSL_SUCCESS:
			break;
		default:
			fprintf(stderr,"%s unable to handle GSL error: %s\n",__func__,gsl_strerror(ret));
			exit(1);
			break;
        }
    }
    while(ret!=GSL_SUCCESS);
    if(relaxed) fprintf(stderr,"Warning: had to relax absolute tolerance to %le during distance integration for r1=%lf, r2=%lf, p=%lf, b=%lf, k=%i\n",
			abstol,r1,r2,p,b,k);
    /* Done! */
    return log_offset + log(result);
}


log_radial_integrator *log_radial_integrator_init(double r1, double r2, int k, int cosmology, double pmax, size_t size)
{
    log_radial_integrator *integrator = NULL;
    bicubic_interp *region0 = NULL;
    cubic_interp *region1 = NULL, *region2 = NULL;
    const double alpha = 4;
    const double p0 = 0.5 * (k >= 0 ? r2 : r1);
    const double xmax = log(pmax);
    const double x0 = GSL_MIN_DBL(log(p0), xmax);
    const double xmin = x0 - (1 + M_SQRT2) * alpha;
    const double ymax = x0 + alpha;
    const double ymin = 2 * x0 - M_SQRT2 * alpha - xmax;
    const double d = (xmax - xmin) / (size - 1); /* dx = dy = du */
    const double umin = - (1 + M_SQRT1_2) * alpha;
    const double vmax = x0 - M_SQRT1_2 * alpha;
    
    double *z1=calloc(size,sizeof(*z1));
    double *z2=calloc(size,sizeof(*z2));
    double *z0=calloc(size*size,sizeof(*z0));
    assert(z0 && z1 && z2);
    /*
    for (size_t i=0;i<size;i++)
    {
        z0[i]=calloc(size,sizeof(*z0[i]));
        assert(z0[i]);
    }
    */
    /* const double umax = xmax - vmax; */ /* unused */

    if(cosmology) dVC_dVL_init();
    
    int interrupted=0;
    OMP_BEGIN_INTERRUPTIBLE
    integrator = malloc(sizeof(*integrator));
    /* Temporarily turn off gsl_error handler which isn't thread safe. */
    gsl_error_handler_t *old_handler = gsl_set_error_handler_off();

    #pragma omp parallel for
    for (size_t i = 0; i < size * size; i ++)
    {

        if (OMP_WAS_INTERRUPTED)
            OMP_EXIT_LOOP_EARLY;

        const size_t ix = i / size;
        const size_t iy = i % size;
        const double x = xmin + ix * d;
        const double y = ymin + iy * d;
        const double p = exp(x);
        const double r0 = exp(y);
        const double b = 2 * gsl_pow_2(p) / r0;
        /* Note: using this where p > r0; could reduce evaluations by half */
        z0[ix*size + iy] = log_radial_integral(r1, r2, p, b, k, cosmology);
    }
    gsl_set_error_handler(old_handler);
    
	if (OMP_WAS_INTERRUPTED)
        goto done;

    region0 = bicubic_interp_init(z0, size, size, xmin, ymin, d, d);

    for (size_t i = 0; i < size; i ++)
        z1[i] = z0[i*size + (size - 1)];
    region1 = cubic_interp_init(z1, size, xmin, d);

    for (size_t i = 0; i < size; i ++)
        z2[i] = z0[i*size + (size - 1 - i)];
    region2 = cubic_interp_init(z2, size, umin, d);

done:
    interrupted = OMP_WAS_INTERRUPTED;
    OMP_END_INTERRUPTIBLE
    
    free(z2); free(z1); free(z0);
    if (interrupted || !(integrator && region0 && region1 && region2))
    {
        free(integrator);
        free(region0);
        free(region1);
        free(region2);
        XLAL_ERROR_NULL(XLAL_ENOMEM, "not enough memory to allocate integrator");
    }

    integrator->region0 = region0;
    integrator->region1 = region1;
    integrator->region2 = region2;
    integrator->xmax = xmax;
    integrator->ymax = ymax;
    integrator->vmax = vmax;
    integrator->r1 = r1;
    integrator->r2 = r2;
    integrator->k = k;
    return integrator;
}


void log_radial_integrator_free(log_radial_integrator *integrator)
{
    if (integrator)
    {
        bicubic_interp_free(integrator->region0);
        integrator->region0 = NULL;
        cubic_interp_free(integrator->region1);
        integrator->region1 = NULL;
        cubic_interp_free(integrator->region2);
        integrator->region2 = NULL;
    }
    free(integrator);
}


double log_radial_integrator_eval(const log_radial_integrator *integrator, double p, double b, double log_p, double log_b)
{
    const double x = log_p;
    const double y = M_LN2 + 2 * log_p - log_b;
    double result;
    assert(x <= integrator->xmax);

    if (p == 0) {
        /* note: p2 == 0 implies b == 0 */
        assert(b == 0);
        int k1 = integrator->k + 1;

        if (k1 == 0)
        {
            result = log(log(integrator->r2 / integrator->r1));
        } else {
            result = log((gsl_pow_int(integrator->r2, k1) - gsl_pow_int(integrator->r1, k1)) / k1);
        }
    } else {
        if (y >= integrator->ymax) {
            result = cubic_interp_eval(integrator->region1, x);
        } else {
            const double v = 0.5 * (x + y);
            if (v <= integrator->vmax)
            {
                const double u = 0.5 * (x - y);
                result = cubic_interp_eval(integrator->region2, u);
            } else {
                result = bicubic_interp_eval(integrator->region0, x, y);
            }
        }
        result += gsl_pow_2(0.5 * b / p);
    }

    return result;
}


