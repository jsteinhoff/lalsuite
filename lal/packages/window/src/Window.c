/*
 * Copyright (C) 2007 Bruce Allen, Duncan Brown, Jolien Creighton, Kipp
 * Cannon, Patrick Brady, Teviet Creighton
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with with program; see the file COPYING. If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307  USA
 */


/*
 * ============================================================================
 *
 *                                  Preamble
 *
 * ============================================================================
 */


#include <math.h>
#include <string.h>
#include <gsl/gsl_sf_bessel.h>
#include <lal/LALConstants.h>
#include <lal/LALStdlib.h>
#include <lal/Sequence.h>
#include <lal/Window.h>
#include <lal/XLALError.h>


NRCSID(WINDOWC, "$Id$");


/*
 * ============================================================================
 *
 *                             Private utilities
 *
 * ============================================================================
 */


/*
 * Allocates a new REAL8Window.
 */


static REAL8Window *REAL8WindowNew(UINT4 length)
{
	REAL8Window *window;
	REAL8Sequence *data;

	window = XLALMalloc(sizeof(*window));
	data = XLALCreateREAL8Sequence(length);
	if(!window || !data) {
		XLALFree(window);
		XLALDestroyREAL8Sequence(data);
		return NULL;
	}

	window->data = data;
	/* for safety */
	window->sumofsquares = XLAL_REAL8_FAIL_NAN;
	window->sum = XLAL_REAL8_FAIL_NAN;

	return window;
}


/*
 * Constructs a REAL4Window from a REAL8Window by quantizing the
 * double-precision data to single-precision.  The REAL8Window is freed
 * unconditionally.  Intended to be used as a wrapper, to convert any
 * function that constructs a REAL8Window into a function to construct a
 * REAL4Window.
 */


static REAL4Window *XLALREAL4Window_from_REAL8Window(REAL8Window *orig)
{
	static const char func[] = "XLALREAL4Window_from_REAL8Window";
	REAL4Window *new;
	REAL4Sequence *data;
	UINT4 i;

	if(!orig)
		XLAL_ERROR_NULL(func, XLAL_EFUNC);
		
	new = XLALMalloc(sizeof(*new));
	data = XLALCreateREAL4Sequence(orig->data->length);

	if(!new || !data) {
		XLALDestroyREAL8Window(orig);
		XLALFree(new);
		XLALDestroyREAL4Sequence(data);
		XLAL_ERROR_NULL(func, XLAL_EFUNC);
	}

	for(i = 0; i < data->length; i++)
		data->data[i] = orig->data->data[i];
	new->data = data;
	new->sumofsquares = orig->sumofsquares;
	new->sum = orig->sum;

	XLALDestroyREAL8Window(orig);

	return new;
}


/*
 * Maps the length of a window and the offset within the window to the "y"
 * co-ordinate of the LAL documentation.
 *
 * Input:
 * length > 0,
 * 0 <= i < length
 *
 * Output:
 * length < 2 --> return 0.0
 * i == 0 --> return -1.0
 * i == length / 2 --> return 0.0
 * i == length - 1 --> return +1.0
 *
 * e.g., length = 5 (odd), then i == 2 --> return 0.0
 * if length = 6 (even), then i == 2.5 --> return 0.0
 *
 * (in the latter case, obviously i can't be a non-integer, but that's the
 * value it would have to be for this function to return 0.0)
 */


static double Y(int length, int i)
{
	length -= 1;
	return length > 0 ? (2 * i - length) / (double) length : 0;
}


/*
 * Computes the sum of squares, and sum, of the samples in a window
 * function.
 *
 * Two techniques are employed to achieve accurate results.  Firstly, the
 * loop iterates from the edges to the centre.  Generally, window functions
 * have smaller values at the edges and larger values in the middle, so
 * adding them in this order avoids adding small numbers to big numbers.
 * The loops also implement Kahan's compensated summation algorithm in
 * which a second variable is used to accumulate round-off errors and fold
 * them into later iterations.
 */


static REAL8 sum_squares(REAL8 *start, int length)
{

	REAL8 sum = 0.0;
	REAL8 e = 0.0;
	REAL8 *end = start + length - 1;

	/* Note:  don't try to simplify this loop, the statements must be
	 * done like this to induce the C compiler to perform the
	 * arithmetic in the correct order. */
	for(; start < end; start++, end--) {
		REAL8 temp = sum;
		/* what we want to add */
		REAL8 x = *start * *start + *end * *end + e;
		/* add */
		sum += x;
		/* negative of what was actually added */
		e = temp - sum;
		/* what didn't get added, add next time */
		e += x;
	}

	if(start == end)
		/* length is odd, get middle sample */
		sum += *start * *start + e;

	return sum;
}


static REAL8 sum_samples(REAL8 *start, int length)
{

	REAL8 sum = 0.0;
	REAL8 e = 0.0;
	REAL8 *end = start + length - 1;

	/* Note:  don't try to simplify this loop, the statements must be
	 * done like this to induce the C compiler to perform the
	 * arithmetic in the correct order. */
	for(; start < end; start++, end--) {
		REAL8 temp = sum;
		/* what we want to add */
		REAL8 x = *start + *end + e;
		/* add */
		sum += x;
		/* negative of what was actually added */
		e = temp - sum;
		/* what didn't get added, add next time */
		e += x;
	}

	if(start == end)
		/* length is odd, get middle sample */
		sum += *start + e;

	return sum;
}


/*
 * ============================================================================
 *
 *                                REAL8Window
 *
 * ============================================================================
 */


/* <lalVerbatim file="WindowCP"> */
REAL8Window *XLALCreateRectangularREAL8Window(UINT4 length)
/* </lalVerbatim> */
{
	static const char func[] = "XLALCreateRectangularREAL8Window";
	REAL8Window *window;
	UINT4 i;

	window = REAL8WindowNew(length);
	if(!window)
		XLAL_ERROR_NULL(func, XLAL_ENOMEM);

	/* flat, box-car, top-hat, rectangle, whatever */
	for(i = 0; i < length; i++)
		window->data->data[i] = 1;

	window->sumofsquares = length;
	window->sum = length;

	return window;
}


/* <lalVerbatim file="WindowCP"> */
REAL8Window *XLALCreateHannREAL8Window(UINT4 length)
/* </lalVerbatim> */
{
	static const char func[] = "XLALCreateHannREAL8Window";
	REAL8Window *window;
	UINT4 i;

	window = REAL8WindowNew(length);
	if(!window)
		XLAL_ERROR_NULL(func, XLAL_ENOMEM);

	/* cos^2, zero at both end points, 1 in the middle */
	for(i = 0; i < (length + 1) / 2; i++)
		window->data->data[i] = window->data->data[length - 1 - i] = pow(cos(LAL_PI_2 * Y(length, i)), 2);

	window->sumofsquares = sum_squares(window->data->data, window->data->length);
	window->sum = sum_samples(window->data->data, window->data->length);

	return window;
}


/* <lalVerbatim file="WindowCP"> */
REAL8Window *XLALCreateWelchREAL8Window(UINT4 length)
/* </lalVerbatim> */
{
	static const char func[] = "XLALCreateWelchREAL8Window";
	REAL8Window *window;
	UINT4 i;

	window = REAL8WindowNew(length);
	if(!window)
		XLAL_ERROR_NULL(func, XLAL_ENOMEM);

	/* downward-opening parabola, zero at both end points, 1 in the
	 * middle */
	for(i = 0; i < (length + 1) / 2; i++)
		window->data->data[i] = window->data->data[length - 1 - i] = 1 - pow(Y(length, i), 2.0);

	window->sumofsquares = sum_squares(window->data->data, window->data->length);
	window->sum = sum_samples(window->data->data, window->data->length);

	return window;
}


/* <lalVerbatim file="WindowCP"> */
REAL8Window *XLALCreateBartlettREAL8Window(UINT4 length)
/* </lalVerbatim> */
{
	static const char func[] = "XLALCreateBartlettREAL8Window";
	REAL8Window *window;
	UINT4 i;

	window = REAL8WindowNew(length);
	if(!window)
		XLAL_ERROR_NULL(func, XLAL_ENOMEM);

	/* downward-opening triangle, zero at both end points (non-zero end
	 * points is a different window called the "triangle" window), 1 in
	 * the middle */
	for(i = 0; i < (length + 1) / 2; i++)
		window->data->data[i] = window->data->data[length - 1 - i] = 1 + Y(length, i);

	window->sumofsquares = sum_squares(window->data->data, window->data->length);
	window->sum = sum_samples(window->data->data, window->data->length);

	return window;
}


/* <lalVerbatim file="WindowCP"> */
REAL8Window *XLALCreateParzenREAL8Window(UINT4 length)
/* </lalVerbatim> */
{
	static const char func[] = "XLALCreateParzenREAL8Window";
	REAL8Window *window;
	UINT4 i;

	window = REAL8WindowNew(length);
	if(!window)
		XLAL_ERROR_NULL(func, XLAL_ENOMEM);

	/* ?? Copied from LAL Software Description */
	for(i = 0; i < (length + 1) / 4; i++)
		window->data->data[i] = window->data->data[length - 1 - i] = 2 * pow(1 + Y(length, i), 3);
	for(; i < (length + 1) / 2; i++) {
		double y = Y(length, i);
		window->data->data[i] = window->data->data[length - 1 - i] = 1 - 6 * y * y * (1 + y);
	}

	window->sumofsquares = sum_squares(window->data->data, window->data->length);
	window->sum = sum_samples(window->data->data, window->data->length);

	return window;
}


/* <lalVerbatim file="WindowCP"> */
REAL8Window *XLALCreatePapoulisREAL8Window(UINT4 length)
/* </lalVerbatim> */
{
	static const char func[] = "XLALCreatePapoulisREAL8Window";
	REAL8Window *window;
	UINT4 i;

	window = REAL8WindowNew(length);
	if(!window)
		XLAL_ERROR_NULL(func, XLAL_ENOMEM);

	/* ?? Copied from LAL Software Description */
	for(i = 0; i < (length + 1) / 2; i++) {
		double y = Y(length, i);
		window->data->data[i] = window->data->data[length - 1 - i] = (1 + y) * cos(LAL_PI * y) - sin(LAL_PI * y) / LAL_PI;
	}

	window->sumofsquares = sum_squares(window->data->data, window->data->length);
	window->sum = sum_samples(window->data->data, window->data->length);

	return window;
}


/* <lalVerbatim file="WindowCP"> */
REAL8Window *XLALCreateHammingREAL8Window(UINT4 length)
/* </lalVerbatim> */
{
	static const char func[] = "XLALCreateHammingREAL8Window";
	REAL8Window *window;
	UINT4 i;

	window = REAL8WindowNew(length);
	if(!window)
		XLAL_ERROR_NULL(func, XLAL_ENOMEM);

	/* cos^2, like Hann window, but with a bias of 0.08 */
	for(i = 0; i < (length + 1) / 2; i++)
		window->data->data[i] = window->data->data[length - 1 - i] = 0.08 + 0.92 * pow(cos(LAL_PI_2 * Y(length, i)), 2);

	window->sumofsquares = sum_squares(window->data->data, window->data->length);
	window->sum = sum_samples(window->data->data, window->data->length);

	return window;
}


/* <lalVerbatim file="WindowCP"> */
REAL8Window *XLALCreateKaiserREAL8Window(UINT4 length, REAL8 beta)
/* </lalVerbatim> */
{
	static const char func[] = "XLALCreateKaiserREAL8Window";
	REAL8Window *window;
	REAL8 I0beta;
	UINT4 i;

	if(beta < 0)
		XLAL_ERROR_NULL(func, XLAL_ERANGE);

	window = REAL8WindowNew(length);
	if(!window)
		XLAL_ERROR_NULL(func, XLAL_ENOMEM);

	/* pre-compute I0(beta) */
	if(beta < 705)
		I0beta = gsl_sf_bessel_I0(beta);

	/* I0(beta sqrt(1 - y^2)) / I0(beta)
	 *
	 * note that in many places the window is defined with pi
	 * multiplying beta in the numerator and denominator, but not here.
	 *
	 * The asymptotic forms for large beta are derived from the
	 * asymptotic form of I0(x) which is
	 *
	 * I0(x) --> exp(x) / sqrt(2 pi x)
	 *
	 * Although beta may be large, beta sqrt(1 - y^2) can be small
	 * (when y ~= +/- 1), so there is a need for two asymptotic forms:
	 * one for large beta alone, and one for large beta sqrt(1 - y^2).
	 *
	 * When beta alone is large,
	 *
	 * w(y) = I0(beta sqrt(1 - y^2)) sqrt(2 pi beta) / exp(beta)
	 *
	 * and when beta sqrt(1 - y^2) is large,
	 *
	 * w(y) = exp(-beta * (1 - sqrt(1 - y^2))) / sqrt(1 - y^2)
	 *
	 * As a function of beta, the asymptotic approximation and the
	 * "exact" form are found to disagree by about 20% in the y = +/- 1
	 * bins near the edge of the "exact" form's domain of validity.  To
	 * smooth this out, a linear transition to the asymptotic form
	 * occurs between beta = 695 and beta = 705. */

	for(i = 0; i < (length + 1) / 2; i++) {
		double y = Y(length, i);
		double x = sqrt(1 - y * y);
		double w1, w2;

		if(beta < 705)
			w1 = gsl_sf_bessel_I0(beta * x) / I0beta;
		if(beta >= 695) {
			/* FIXME:  should an interpolation be done across
			 * the transition from small beta x to large beta
			 * x? */
			/* Note:  the inf * 0 when beta = inf and y = +/- 1
			 * needs to be hard-coded */
			if(beta * x < 700)
				w2 = y == 0 ? 1 : gsl_sf_bessel_I0(beta * x) * sqrt(LAL_2_PI * beta) / exp(beta);
			else
				/* Note:  when beta = inf, the inf * 0 in
				 * the y = 0 sample must be hard-coded,
				 * which we do by simply testing for y = 0.
				 * And when beta = inf and x = 0 (y = +/-
				 * 1), the conditional goes onto this
				 * branch, and results in a 0/0 which we
				 * have to hard-code */
				w2 = y == 0 ? 1 : x == 0 ? 0 : exp(-beta * (1 - x)) / sqrt(x);
		}

		if(beta < 695)
			window->data->data[i] = window->data->data[length - 1 - i] = w1;
		else if(beta < 705) {
			double r = (beta - 695) / (705 - 695);
			window->data->data[i] = window->data->data[length - 1 - i] = (1 - r) * w1 + r * w2;
		} else
			window->data->data[i] = window->data->data[length - 1 - i] = w2;
	}

	window->sumofsquares = sum_squares(window->data->data, window->data->length);
	window->sum = sum_samples(window->data->data, window->data->length);

	return window;
}


/* <lalVerbatim file="WindowCP"> */
REAL8Window *XLALCreateCreightonREAL8Window(UINT4 length, REAL8 beta)
/* </lalVerbatim> */
{
	static const char func[] = "XLALCreateCreightonREAL8Window";
	REAL8Window *window;
	UINT4 i;

	if(beta < 0)
		XLAL_ERROR_NULL(func, XLAL_ERANGE);

	window = REAL8WindowNew(length);
	if(!window)
		XLAL_ERROR_NULL(func, XLAL_ENOMEM);

	/* ?? Copied from LAL Software Description */
	for(i = 0; i < (length + 1) / 2; i++) {
		double y = Y(length, i);
		/* NOTE:  divide-by-zero in y^2 / (1 - y^2) when i = 0
		 * seems to work out OK.  It's well-defined algebraically,
		 * but I'm surprised the FPU doesn't complain.  The 0/0
		 * when beta = i = 0 has to be hard-coded, as does the inf
		 * * 0 when beta = inf and y = 0 (which is done by just
		 * checking for y = 0). The fabs() is there because Macs,
		 * with optimizations turned on, incorrectly state that
		 * 1-y^2 = -0 when y = 1, which converts the argument of
		 * exp() from -inf to +inf, and causes the window to
		 * evaluate to +inf instead of 0 at the end points. See
		 * also the -0 at the end points of the Welch window on
		 * Macs. */
		window->data->data[i] = window->data->data[length - 1 - i] = (beta == 0 && y == -1) || y == 0 ? 1 : exp(-beta * y * y / fabs(1 - y * y));
	}

	window->sumofsquares = sum_squares(window->data->data, window->data->length);
	window->sum = sum_samples(window->data->data, window->data->length);

	return window;
}


/* <lalVerbatim file="WindowCP"> */
REAL8Window *XLALCreateTukeyREAL8Window(UINT4 length, REAL8 beta)
/* </lalVerbatim> */
{
	static const char func[] = "XLALCreateTukeyREAL8Window";
	REAL8Window *window;
	UINT4 transition_length = beta * length + 0.5;
	UINT4 i;

	if(beta < 0 || beta > 1)
		XLAL_ERROR_NULL(func, XLAL_ERANGE);

	window = REAL8WindowNew(length);
	if(!window)
		XLAL_ERROR_NULL(func, XLAL_ENOMEM);

	/* 1.0 and flat in the middle, cos^2 transition at each end, zero
	 * at end points, 0.0 <= beta <= 1.0 sets what fraction of the
	 * window is transition (0 --> rectangle window, 1 --> Hann window)
	 * */
	for(i = 0; i < (transition_length + 1) / 2; i++)
		window->data->data[i] = window->data->data[length - 1 - i] = pow(cos(LAL_PI_2 * Y(transition_length, i)), 2);
	for(; i < (length + 1) / 2; i++)
		window->data->data[i] = window->data->data[length - 1 - i] = 1;

	window->sumofsquares = sum_squares(window->data->data, window->data->length);
	window->sum = sum_samples(window->data->data, window->data->length);

	return window;
}


/* <lalVerbatim file="WindowCP"> */
REAL8Window *XLALCreateGaussREAL8Window(UINT4 length, REAL8 beta)
/* </lalVerbatim> */
{
	static const char func[] = "XLALCreateGaussREAL8Window";
	REAL8Window *window;
	UINT4 i;

	if(beta < 0)
		XLAL_ERROR_NULL(func, XLAL_ERANGE);

	window = REAL8WindowNew(length);
	if(!window)
		XLAL_ERROR_NULL(func, XLAL_ENOMEM);

	/* pre-compute -1/2 beta^2 */
	beta = -0.5 * beta * beta;

	/* exp(-1/2 beta^2 y^2) */
	for(i = 0; i < (length + 1) / 2; i++) {
		double y = Y(length, i);
		/* Note:  we have to hard-code the 0 * inf when y = 0 and
		 * beta = inf, which we do by simply checking for y = 0 */
		window->data->data[i] = window->data->data[length - 1 - i] = y == 0 ? 1 : exp(y * y * beta);
	}

	window->sumofsquares = sum_squares(window->data->data, window->data->length);
	window->sum = sum_samples(window->data->data, window->data->length);

	return window;
}


/* <lalVerbatim file="WindowCP"> */
void XLALDestroyREAL8Window(REAL8Window * window)
/* </lalVerbatim> */
{
	if(window)
		XLALDestroyREAL8Sequence(window->data);
	XLALFree(window);
}


/*
 * ============================================================================
 *
 *                                REAL4Window
 *
 * ============================================================================
 */


/* <lalVerbatim file="WindowCP"> */
REAL4Window *XLALCreateRectangularREAL4Window(UINT4 length)
/* </lalVerbatim> */
{
	return XLALREAL4Window_from_REAL8Window(XLALCreateRectangularREAL8Window(length));
}


/* <lalVerbatim file="WindowCP"> */
REAL4Window *XLALCreateHannREAL4Window(UINT4 length)
/* </lalVerbatim> */
{
	return XLALREAL4Window_from_REAL8Window(XLALCreateHannREAL8Window(length));
}


/* <lalVerbatim file="WindowCP"> */
REAL4Window *XLALCreateWelchREAL4Window(UINT4 length)
/* </lalVerbatim> */
{
	return XLALREAL4Window_from_REAL8Window(XLALCreateWelchREAL8Window(length));
}


/* <lalVerbatim file="WindowCP"> */
REAL4Window *XLALCreateBartlettREAL4Window(UINT4 length)
/* </lalVerbatim> */
{
	return XLALREAL4Window_from_REAL8Window(XLALCreateBartlettREAL8Window(length));
}


/* <lalVerbatim file="WindowCP"> */
REAL4Window *XLALCreateParzenREAL4Window(UINT4 length)
/* </lalVerbatim> */
{
	return XLALREAL4Window_from_REAL8Window(XLALCreateParzenREAL8Window(length));
}


/* <lalVerbatim file="WindowCP"> */
REAL4Window *XLALCreatePapoulisREAL4Window(UINT4 length)
/* </lalVerbatim> */
{
	return XLALREAL4Window_from_REAL8Window(XLALCreatePapoulisREAL8Window(length));
}


/* <lalVerbatim file="WindowCP"> */
REAL4Window *XLALCreateHammingREAL4Window(UINT4 length)
/* </lalVerbatim> */
{
	return XLALREAL4Window_from_REAL8Window(XLALCreateHammingREAL8Window(length));
}


/* <lalVerbatim file="WindowCP"> */
REAL4Window *XLALCreateKaiserREAL4Window(UINT4 length, REAL4 beta)
/* </lalVerbatim> */
{
	return XLALREAL4Window_from_REAL8Window(XLALCreateKaiserREAL8Window(length, beta));
}


/* <lalVerbatim file="WindowCP"> */
REAL4Window *XLALCreateCreightonREAL4Window(UINT4 length, REAL4 beta)
/* </lalVerbatim> */
{
	return XLALREAL4Window_from_REAL8Window(XLALCreateCreightonREAL8Window(length, beta));
}


/* <lalVerbatim file="WindowCP"> */
REAL4Window *XLALCreateTukeyREAL4Window(UINT4 length, REAL4 beta)
/* </lalVerbatim> */
{
	return XLALREAL4Window_from_REAL8Window(XLALCreateTukeyREAL8Window(length, beta));
}


/* <lalVerbatim file="WindowCP"> */
REAL4Window *XLALCreateGaussREAL4Window(UINT4 length, REAL4 beta)
/* </lalVerbatim> */
{
	return XLALREAL4Window_from_REAL8Window(XLALCreateGaussREAL8Window(length, beta));
}


/* <lalVerbatim file="WindowCP"> */
void XLALDestroyREAL4Window(REAL4Window * window)
/* </lalVerbatim> */
{
	if(window)
		XLALDestroyREAL4Sequence(window->data);
	XLALFree(window);
}


/*
 * ============================================================================
 *
 *                                LEGACY CODE
 *
 * ============================================================================
 *
 * DO NOT USE!!!!!
 *
 * FIXME:  REMOVE AS SOON AS POSSIBLE.
 */


#define WINDOWH_EEALLOCATE 4
#define WINDOWH_MSGEEALLOCATE "unable to allocate vector to store window"


static REAL4Window *XLALCreateREAL4Window(UINT4 length, WindowType type, REAL4 beta)
{
	static const char func[] = "XLALCreateREAL4Window";
	REAL4Window *window;

	XLALPrintDeprecationWarning(func, "XLALCreateRectangularREAL4Window(), XLALCreateHannREAL4Window(), XLALCreateWelchREAL4Window(), XLALCreateBartlettREAL4Window(), XLALCreateParzenREAL4Window(), XLALCreatePapoulisREAL4Window(), XLALCreateHammingREAL4Window(), XLALCreateKaiserREAL4Window(), XLALCreateCreightonREAL4Window(), or XLALCreateTukeyREAL4Window");

	switch (type) {
	case Rectangular:
		window = XLALCreateRectangularREAL4Window(length);
		break;

	case Hann:
		window = XLALCreateHannREAL4Window(length);
		break;

	case Welch:
		window = XLALCreateWelchREAL4Window(length);
		break;

	case Bartlett:
		window = XLALCreateBartlettREAL4Window(length);
		break;

	case Parzen:
		window = XLALCreateParzenREAL4Window(length);
		break;

	case Papoulis:
		window = XLALCreatePapoulisREAL4Window(length);
		break;

	case Hamming:
		window = XLALCreateHammingREAL4Window(length);
		break;

	case Kaiser:
		window = XLALCreateKaiserREAL4Window(length, beta);
		break;

	case Creighton:
		window = XLALCreateCreightonREAL4Window(length, beta);
		break;

	case Tukey:
		window = XLALCreateTukeyREAL4Window(length, beta);
		break;

	default:
		XLAL_ERROR_NULL(func, XLAL_ETYPE);
	}

	if(!window)
		XLAL_ERROR_NULL(func, XLAL_EFUNC);
	return window;
}


void LALWindow(
	LALStatus * status,
	REAL4Vector * vector,
	LALWindowParams * parameters
)
{
	/* emulate legacy code */
	REAL4Window *window;

	INITSTATUS(status, "LALWindow", WINDOWC);
	ATTATCHSTATUSPTR(status);

	XLALPrintDeprecationWarning("LALWindow", "XLALCreateRectangularREAL4Window(), XLALCreateHannREAL4Window(), XLALCreateWelchREAL4Window(), XLALCreateBartlettREAL4Window(), XLALCreateParzenREAL4Window(), XLALCreatePapoulisREAL4Window(), XLALCreateHammingREAL4Window(), XLALCreateKaiserREAL4Window(), XLALCreateCreightonREAL4Window(), or XLALCreateTukeyREAL4Window");

	window = XLALCreateREAL4Window(parameters->length, parameters->type, parameters->beta);
	if(!window) {
		ABORT(status, WINDOWH_EEALLOCATE, WINDOWH_MSGEEALLOCATE);
	}
	memcpy(vector->data, window->data->data, vector->length * sizeof(*vector->data));
	XLALDestroyREAL4Window(window);

	DETATCHSTATUSPTR(status);
	RETURN(status);
}


void LALCreateREAL4Window(
	LALStatus * status,
	REAL4Window ** output,
	LALWindowParams * params
)
{
	/* emulate legacy code */
	INITSTATUS(status, "LALCreateREAL4Window", WINDOWC);
	ATTATCHSTATUSPTR(status);

	XLALPrintDeprecationWarning("LALCreateREAL4Window", "XLALCreateRectangularREAL4Window(), XLALCreateHannREAL4Window(), XLALCreateWelchREAL4Window(), XLALCreateBartlettREAL4Window(), XLALCreateParzenREAL4Window(), XLALCreatePapoulisREAL4Window(), XLALCreateHammingREAL4Window(), XLALCreateKaiserREAL4Window(), XLALCreateCreightonREAL4Window(), or XLALCreateTukeyREAL4Window");

	*output = XLALCreateREAL4Window(params->length, params->type, params->beta);
	if(!*output) {
		ABORT(status, WINDOWH_EEALLOCATE, WINDOWH_MSGEEALLOCATE);
	}

	DETATCHSTATUSPTR(status);
	RETURN(status);
}
