/*
*  Copyright (C) 2014 Matthew Pitkin, Colin Gill, 2016 Max Isi
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with with program; see the file COPYING. If not, write to the
*  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
*  MA  02110-1301  USA
*/


/**
 * \file
 * \ingroup lalpulsar_bin_HeterodyneSearch
 * \author Matthew Pitkin, Colin Gill, Max Isi
 *
 * \brief Pulsar model functions for use in parameter estimation codes for targeted pulsar searches.
 */

#include "config.h"
#include "ppe_models.h"
#include <lal/SinCosLUT.h>

#define SQUARE(x) ( (x) * (x) )

/******************************************************************************/
/*                            MODEL FUNCTIONS                                 */
/******************************************************************************/

/**
 * \brief Defines the pulsar model/template to use
 *
 * This function is the wrapper for functions defining the pulsar model template to be used in the analysis.
 * It places them into a \c PulsarParameters structure.
 *
 * Note: Any additional models should be added into this function.
 *
 * \param model [in] The model structure hold model information and current parameter info
 *
 * \sa add_pulsar_parameter
 * \sa pulsar_model
 */
void get_pulsar_model( LALInferenceModel *model )
{
  PulsarParameters *pars = XLALCalloc( sizeof( *pars ), 1 );

  /* set model parameters (including rescaling) */
  add_pulsar_parameter( model->params, pars, "PSI" );

  if ( ( LALInferenceCheckVariableNonFixed( model->params, "H0" ) || LALInferenceCheckVariableNonFixed( model->params, "Q22" ) || LALInferenceCheckVariable( model->ifo->params, "source_model" ) ) && !LALInferenceCheckVariable( model->ifo->params, "nonGR" ) ) {
    /* if searching in mass quadrupole, Q22, then check for distance and f0 and convert to h0 */
    if ( LALInferenceCheckVariableNonFixed( model->params, "Q22" ) && !LALInferenceCheckVariableNonFixed( model->params, "H0" ) ) {
      if ( LALInferenceCheckVariable( model->params, "F0" ) && LALInferenceCheckVariable( model->params, "DIST" ) ) {
        /* convert Q22, distance and frequency into strain h0 using eqn 3 of Aasi et al, Ap. J., 785, 119 (2014) */
        REAL8 Q22 = LALInferenceGetREAL8Variable( model->params, "Q22" );
        REAL8 dist = LALInferenceGetREAL8Variable( model->params, "DIST" );
        REAL8 f0 = LALInferenceGetREAL8Variable( model->params, "F0" );
        REAL8 h0val = Q22 * sqrt( 8.*LAL_PI / 15. ) * 16.*LAL_PI * LAL_PI * LAL_G_SI * f0 * f0 / ( LAL_C_SI * LAL_C_SI * LAL_C_SI * LAL_C_SI * dist );
        PulsarAddREAL8Param( pars, "H0", h0val );
      } else {
        XLAL_ERROR_VOID( XLAL_EINVAL, "Error... using mass quadrupole, Q22, but no distance or frequency given!" );
      }
    } else if ( LALInferenceCheckVariableNonFixed( model->params, "Q22" ) && LALInferenceCheckVariableNonFixed( model->params, "H0" ) ) {
      XLAL_ERROR_VOID( XLAL_EINVAL, "Error... cannot have both h0 and Q22 as variables." );
    } else if ( LALInferenceCheckVariableNonFixed( model->params, "H0" ) ) {
      add_pulsar_parameter( model->params, pars, "H0" );
    }

    /* use parameterisation from Ian Jones's original model */
    add_pulsar_parameter( model->params, pars, "I21" );
    add_pulsar_parameter( model->params, pars, "I31" );
    add_pulsar_parameter( model->params, pars, "LAMBDA" );

    /* check whether using cos(theta) or theta as the variable (defaulting to cos(theta) being the value that is set */
    if ( LALInferenceCheckVariableNonFixed( model->params, "THETA" ) ) {
      REAL8 costheta = cos( LALInferenceGetREAL8Variable( model->params, "THETA" ) );
      PulsarAddREAL8Param( pars, "COSTHETA", costheta );
    } else {
      add_pulsar_parameter( model->params, pars, "COSTHETA" );
    }
    add_pulsar_parameter( model->params, pars, "PHI0" ); /* note that this is the rotational phase */

    /* check whether using cos(iota) or iota as the variable (defaulting to cos(iota) being the value that is set) */
    if ( LALInferenceCheckVariableNonFixed( model->params, "IOTA" ) ) {
      REAL8 cosiota = cos( LALInferenceGetREAL8Variable( model->params, "IOTA" ) );
      PulsarAddREAL8Param( pars, "COSIOTA", cosiota );
    } else {
      add_pulsar_parameter( model->params, pars, "COSIOTA" );
    }

    invert_source_params( pars );
  } else if ( LALInferenceCheckVariable( model->ifo->params, "nonGR" ) ) {
    /* speed of GWs as (1 - fraction of speed of light LAL_C_SI) */
    add_pulsar_parameter( model->params, pars, "CGW" );

    /* amplitudes for use with non-GR searches (with emission at twice the rotation frequency) */
    /* tensor modes */
    add_pulsar_parameter( model->params, pars, "HPLUS" );
    add_pulsar_parameter( model->params, pars, "HCROSS" );
    /* scalar modes */
    add_pulsar_parameter( model->params, pars, "HSCALARB" );
    add_pulsar_parameter( model->params, pars, "HSCALARL" );
    /* vector modes */
    add_pulsar_parameter( model->params, pars, "HVECTORX" );
    add_pulsar_parameter( model->params, pars, "HVECTORY" );

    add_pulsar_parameter( model->params, pars, "PHI0SCALAR" );
    add_pulsar_parameter( model->params, pars, "PSISCALAR" );
    add_pulsar_parameter( model->params, pars, "PHI0VECTOR" );
    add_pulsar_parameter( model->params, pars, "PSIVECTOR" );
    add_pulsar_parameter( model->params, pars, "PHI0TENSOR" );
    add_pulsar_parameter( model->params, pars, "PSITENSOR" );

    /* amplitudes for use in non-GR searches with emission at the rotation frequency */
    /* tensor modes */
    add_pulsar_parameter( model->params, pars, "HPLUS_F" );
    add_pulsar_parameter( model->params, pars, "HCROSS_F" );
    /* scalar modes */
    add_pulsar_parameter( model->params, pars, "HSCALARB_F" );
    add_pulsar_parameter( model->params, pars, "HSCALARL_F" );
    /* vector modes */
    add_pulsar_parameter( model->params, pars, "HVECTORX_F" );
    add_pulsar_parameter( model->params, pars, "HVECTORY_F" );

    add_pulsar_parameter( model->params, pars, "PHI0SCALAR_F" );
    add_pulsar_parameter( model->params, pars, "PSISCALAR_F" );
    add_pulsar_parameter( model->params, pars, "PHI0VECTOR_F" );
    add_pulsar_parameter( model->params, pars, "PSIVECTOR_F" );
    add_pulsar_parameter( model->params, pars, "PHI0TENSOR_F" );
    add_pulsar_parameter( model->params, pars, "PSITENSOR_F" );

    /* parameters that might be needed for particular models */
    add_pulsar_parameter( model->params, pars, "H0" );
    add_pulsar_parameter( model->params, pars, "H0_F" );
    add_pulsar_parameter( model->params, pars, "IOTA" );
    add_pulsar_parameter( model->params, pars, "COSIOTA" );

    /* check whether a specific nonGR model was requested */
    if ( LALInferenceCheckVariable( model->ifo->params, "nonGRmodel" ) ) {
      char *nonGRmodel;
      nonGRmodel = *( char ** )LALInferenceGetVariable( model->ifo->params, "nonGRmodel" );
      set_nonGR_model_parameters( pars, nonGRmodel );
    }
  } else {
    /* amplitude for usual GR search */
    add_pulsar_parameter( model->params, pars, "C21" );
    add_pulsar_parameter( model->params, pars, "C22" );
    add_pulsar_parameter( model->params, pars, "PHI21" );

    /* check whether using cos(theta) or theta as the variable (defaulting to cos(theta) being the value that is set */
    if ( LALInferenceCheckVariableNonFixed( model->params, "IOTA" ) ) {
      REAL8 cosiota = cos( LALInferenceGetREAL8Variable( model->params, "IOTA" ) );
      PulsarAddREAL8Param( pars, "COSIOTA", cosiota );
    } else {
      add_pulsar_parameter( model->params, pars, "COSIOTA" );
    }

    if ( LALInferenceCheckVariable( model->ifo->params, "biaxial" ) ) {
      /* use complex amplitude parameterisation, but set up for a biaxial star */
      REAL8 phi22 = 2.*LALInferenceGetREAL8Variable( model->params, "PHI21" );
      PulsarAddREAL8Param( pars, "PHI22", phi22 );
    } else {
      add_pulsar_parameter( model->params, pars, "PHI22" );
    }
  }

  /* set the potentially variable parameters */
  add_pulsar_parameter( model->params, pars, "PEPOCH" );
  add_pulsar_parameter( model->params, pars, "POSEPOCH" );

  add_pulsar_parameter( model->params, pars, "RA" );
  add_pulsar_parameter( model->params, pars, "PMRA" );
  add_pulsar_parameter( model->params, pars, "DEC" );
  add_pulsar_parameter( model->params, pars, "PMDEC" );
  add_pulsar_parameter( model->params, pars, "PX" );

  /* check the number of frequency and frequency derivative parameters */
  if ( LALInferenceCheckVariable( model->params, "FREQNUM" ) ) {
    UINT4 freqnum = LALInferenceGetUINT4Variable( model->params, "FREQNUM" );
    REAL8Vector *freqs = XLALCreateREAL8Vector( freqnum );
    REAL8Vector *deltafreqs = XLALCreateREAL8Vector( freqnum );
    for ( UINT4 i = 0; i < freqnum; i++ ) {
      CHAR varname[256];
      snprintf( varname, sizeof( varname ), "F%u", i );
      REAL8 f0new = LALInferenceGetREAL8Variable( model->params, varname );
      freqs->data[i] = f0new; /* current frequency (derivative) */
      snprintf( varname, sizeof( varname ), "F%u_FIXED", i );
      REAL8 f0fixed = LALInferenceGetREAL8Variable( model->params, varname );
      deltafreqs->data[i] = f0new - f0fixed; /* frequency (derivative) difference */
    }
    PulsarAddREAL8VectorParam( pars, "F", ( const REAL8Vector * )freqs );
    PulsarAddREAL8VectorParam( pars, "DELTAF", ( const REAL8Vector * )deltafreqs );
    XLALDestroyREAL8Vector( freqs );
    XLALDestroyREAL8Vector( deltafreqs );
  }

  /* check if there are glitch parameters */
  if ( LALInferenceCheckVariable( model->ifo->params, "GLITCHES" ) ) {
    if ( LALInferenceCheckVariable( model->params, "GLNUM" ) ) { /* number of glitches */
      UINT4 glnum = LALInferenceGetUINT4Variable( model->params, "GLNUM" );
      for ( UINT4 i = 0; i < NUMGLITCHPARS; i++ ) {
        REAL8Vector *gl = NULL;
        gl = XLALCreateREAL8Vector( glnum );
        for ( UINT4 j = 0; j < glnum; j++ ) {
          CHAR varname[300];
          snprintf( varname, sizeof( varname ), "%s_%u", glitchpars[i], j + 1 );
          if ( LALInferenceCheckVariable( model->params, varname ) ) {
            gl->data[j] = LALInferenceGetREAL8Variable( model->params, varname );
          } else {
            gl->data[j] = 0.;
          }
        }
        PulsarAddREAL8VectorParam( pars, glitchpars[i], ( const REAL8Vector * )gl );
        XLALDestroyREAL8Vector( gl );
      }
    }
  }

  /* check if there are binary parameters */
  if ( LALInferenceCheckVariable( model->ifo->params, "BINARY" ) ) {
    /* binary system model - NOT pulsar model */
    CHAR binary[PULSAR_PARNAME_MAX];
    snprintf( binary, PULSAR_PARNAME_MAX * sizeof( CHAR ), "%s", *( CHAR ** )LALInferenceGetVariable( model->ifo->params, "BINARY" ) );
    PulsarAddStringParam( pars, "BINARY", binary );

    add_pulsar_parameter( model->params, pars, "ECC" );
    add_pulsar_parameter( model->params, pars, "OM" );
    add_pulsar_parameter( model->params, pars, "PB" );
    add_pulsar_parameter( model->params, pars, "A1" );
    add_pulsar_parameter( model->params, pars, "T0" );

    add_pulsar_parameter( model->params, pars, "ECC_2" );
    add_pulsar_parameter( model->params, pars, "OM_2" );
    add_pulsar_parameter( model->params, pars, "PB_2" );
    add_pulsar_parameter( model->params, pars, "A1_2" );
    add_pulsar_parameter( model->params, pars, "T0_2" );

    add_pulsar_parameter( model->params, pars, "ECC_3" );
    add_pulsar_parameter( model->params, pars, "OM_3" );
    add_pulsar_parameter( model->params, pars, "PB_3" );
    add_pulsar_parameter( model->params, pars, "A1_3" );
    add_pulsar_parameter( model->params, pars, "T0_3" );

    add_pulsar_parameter( model->params, pars, "XPBDOT" );
    add_pulsar_parameter( model->params, pars, "EPS1" );
    add_pulsar_parameter( model->params, pars, "EPS2" );
    add_pulsar_parameter( model->params, pars, "EPS1DOT" );
    add_pulsar_parameter( model->params, pars, "EPS2DOT" );
    add_pulsar_parameter( model->params, pars, "TASC" );

    add_pulsar_parameter( model->params, pars, "OMDOT" );
    add_pulsar_parameter( model->params, pars, "GAMMA" );
    add_pulsar_parameter( model->params, pars, "PBDOT" );
    add_pulsar_parameter( model->params, pars, "XDOT" );
    add_pulsar_parameter( model->params, pars, "EDOT" );

    add_pulsar_parameter( model->params, pars, "SINI" );
    add_pulsar_parameter( model->params, pars, "DR" );
    add_pulsar_parameter( model->params, pars, "DTHETA" );
    add_pulsar_parameter( model->params, pars, "A0" );
    add_pulsar_parameter( model->params, pars, "B0" );

    add_pulsar_parameter( model->params, pars, "MTOT" );
    add_pulsar_parameter( model->params, pars, "M2" );

    if ( LALInferenceCheckVariable( model->params, "FBNUM" ) ) {
      UINT4 fbnum = LALInferenceGetUINT4Variable( model->params, "FBNUM" );
      REAL8Vector *fb = NULL;
      fb = XLALCreateREAL8Vector( fbnum );
      for ( UINT4 i = 0; i < fbnum; i++ ) {
        CHAR varname[256];
        snprintf( varname, sizeof( varname ), "FB%u", i );
        fb->data[i] = LALInferenceGetREAL8Variable( model->params, varname );
      }
      PulsarAddREAL8VectorParam( pars, "FB", ( const REAL8Vector * )fb );
      XLALDestroyREAL8Vector( fb );
    }
  }

  pulsar_model( pars, model->ifo );

  PulsarFreeParams( pars ); /* free memory */
}

/**
 * \brief Set amplitude parameters for specific non-GR models.
 *
 * Turns physical parameters from a particular nonGR model into the corresponding antenna pattern amplitudes and phases. All nonGR models must be included here.
 * \param pars [in] parameter structure
 * \param nonGRmodel [in] name of model requested
 */
void set_nonGR_model_parameters( PulsarParameters *pars, char *nonGRmodel )
{
  /* determine what model was requested */
  int isG4v = strcmp( nonGRmodel, "G4v" ) * strcmp( nonGRmodel, "g4v" ) * strcmp( nonGRmodel, "G4V" );
  int isEGR = strcmp( nonGRmodel, "enhanced-GR" ) * strcmp( nonGRmodel, "EGR" ) * strcmp( nonGRmodel, "egr" ) * strcmp( nonGRmodel, "eGR" );
  REAL8 h0 = PulsarGetREAL8ParamOrZero( pars, "H0" );
  REAL8 h0_F = PulsarGetREAL8ParamOrZero( pars, "H0_F" );

  REAL8 iota = PulsarGetREAL8ParamOrZero( pars, "IOTA" );
  REAL8 cosiota = cos( iota );
  REAL8 siniota = sin( iota );

  /* check if COSIOTA was passed directly instead */
  if ( PulsarGetREAL8ParamOrZero( pars, "COSIOTA" ) != 0 ) {
    cosiota = PulsarGetREAL8ParamOrZero( pars, "COSIOTA" );
    siniota = sin( acos( cosiota ) );
  }

  if ( isG4v == 0 ) {
    /* \f$ h_{\rm x} = h_0 \sin \iota~,~\phi_{\rm x} = -\pi/2 \f$ */
    /* \f$ h_{\rm y} = h_0 \sin \iota \cos \iota~,~\phi_{\rm y} = 0 \f$ */
    REAL8 hVectorX = h0 * siniota;
    REAL8 hVectorY = h0 * siniota * cosiota;
    REAL8 psiVector = LAL_PI_2;
    PulsarAddREAL8Param( pars, "HVECTORX", hVectorX );
    PulsarAddREAL8Param( pars, "HVECTORY", hVectorY );
    PulsarAddREAL8Param( pars, "PSIVECTOR", psiVector );
  } else if ( isEGR == 0 ) {
    /** GR plus unconstrained non-GR modes
    * With different priors, this can be used to obtain GR+scalar (i.e.
    * scalar-tensor), GR+vector, or GR+scalar+vector. Note: this mode used to
    be called just "ST".*/
    REAL8 psiTensor = -LAL_PI_2;
    /* Set 1F components */
    REAL8 hPlus_F = 0.25 * h0_F * siniota * cosiota;
    REAL8 hCross_F = 0.5 * h0_F * siniota;
    PulsarAddREAL8Param( pars, "HPLUS_F", hPlus_F );
    PulsarAddREAL8Param( pars, "HCROSS_F", hCross_F );
    PulsarAddREAL8Param( pars, "PSITENSOR_F", psiTensor );
    /* Set 2F components */
    REAL8 hPlus = 0.5 * h0 * ( 1. + cosiota * cosiota );
    REAL8 hCross = h0 * cosiota;
    PulsarAddREAL8Param( pars, "HPLUS", hPlus );
    PulsarAddREAL8Param( pars, "HCROSS", hCross );
    PulsarAddREAL8Param( pars, "PSITENSOR", psiTensor );
  } else {
    XLAL_ERROR_VOID( XLAL_EINVAL, "Unrecognized non-GR model. Currently supported: enhanced GR (EGR), G4v, or no argument for full search." );
  }
}


/**
 * \brief Add a \c REAL8 parameter from a \c LALInferenceVariable into a \c PulsarParameters variable
 *
 * \param var [in] \c LALInferenceVariable structure
 * \param params [in] \c PulsarParameters structure
 * \param parname [in] name of the parameter
 */
void add_pulsar_parameter( LALInferenceVariables *var, PulsarParameters *params, const CHAR *parname )
{
  REAL8 par = LALInferenceGetREAL8Variable( var, parname );
  PulsarAddREAL8Param( params, parname, par );
}


/**
 * \brief Add a \c REAL8 parameter from a \c PulsarParameters variable into a \c LALInferenceVariable
 *
 * This function will rescale a parameter to its true value using the scale factor and minimum scale value.
 *
 * \param params [in] \c PulsarParameters structure
 * \param var [in] \c LALInferenceVariable structure
 * \param parname [in] name of the parameter
 * \param vary [in] the parameter \c LALInferenceParamVaryType
 */
void add_variable_parameter( PulsarParameters *params, LALInferenceVariables *var, const CHAR *parname, LALInferenceParamVaryType vary )
{
  REAL8 par = PulsarGetREAL8ParamOrZero( params, parname );
  LALInferenceAddVariable( var, parname, &par, LALINFERENCE_REAL8_t, vary );
}


/**
 * \brief Generate the model of the neutron star signal
 *
 * The function requires that the pulsar model is set using the \c model-type command line argument (this is set in \c
 * main, and if not specified defaults to a \c triaxial model). Currently the model can be \c triaxial for quadrupole
 * emission from a triaxial star at twice the rotation freqeuncy, or \c pinsf for a two component emission model with
 * emission at the rotation frequency <i>and</i> twice the rotation frequency. Depending on the specified model the
 * function calls the appropriate model function.
 *
 * Firstly the time varying amplitude of the signal will be calculated based on the antenna pattern and amplitude
 * parameters. Then, if searching over phase parameters, the phase evolution of the signal will be calculated. The
 * difference between the new phase model, \f$ \phi(t)_n \f$ , and that used to heterodyne the data, \f$ \phi(t)_h \f$ ,
 * will be calculated and the complex signal model, \f$ M \f$ , modified accordingly:
 * \f[
 * M'(t) = M(t)\exp{i((\phi(t)_n - \phi(t)_h))}.
 * \f]
 * This does not try to undo the signal modulation in the data, but instead replicates the modulation in the model,
 * hence the positive phase difference rather than a negative phase in the exponential function.
 *
 * \param params [in] A \c PulsarParameters structure containing the model parameters
 * \param ifo [in] The ifo model structure containing the detector paramters and buffers
 *
 * \sa get_amplitude_model
 * \sa get_phase_model
 */
void pulsar_model( PulsarParameters *params, LALInferenceIFOModel *ifo )
{
  INT4 i = 0, length = 0;
  UINT4 j = 0;
  LALInferenceIFOModel *ifomodel1 = ifo, *ifomodel2 = ifo;

  /* get the amplitude model */
  get_amplitude_model( params, ifomodel1 );

  /* check whether to search over the phase parameters or not - this only needs to be set for the
   * first ifo linked list in at set for a given detector (i.e. it doesn't need to be set for
   * different frequency streams */
  if ( LALInferenceCheckVariable( ifomodel2->params, "varyphase" ) ) {
    /* get difference in phase for f component and perform extra heterodyne */
    REAL8Vector *freqFactors = NULL;
    freqFactors = *( REAL8Vector ** )LALInferenceGetVariable( ifo->params, "freqfactors" );

    while ( ifomodel2 ) {
      for ( j = 0; j < freqFactors->length; j++ ) {
        REAL8Vector *dphi = NULL;
        COMPLEX16 M = 0., expp = 0.;

        length = ifomodel2->compTimeSignal->data->length;

        /* reheterodyne with the phase */
        if ( ( dphi = get_phase_model( params, ifomodel2, freqFactors->data[j] ) ) != NULL ) {
          for ( i = 0; i < length; i++ ) {
            /* phase factor by which to multiply the (almost) DC signal model. NOTE: this does not try to undo
             * the signal modulation in the data, but instead replicates it in the model, hence the positive
             * phase rather than a negative phase in the cexp function. */
            expp = cexp( LAL_TWOPI * I * dphi->data[i] );

            M = ifomodel2->compTimeSignal->data->data[i];

            /* heterodyne */
            ifomodel2->compTimeSignal->data->data[i] = M * expp;
          }

          XLALDestroyREAL8Vector( dphi );
        }

        ifomodel2 = ifomodel2->next;
      }
    }
  }
}


/**
 * \brief The phase evolution of a source
 *
 * This function will calculate the difference in the phase evolution of a source at a particular sky location as
 * observed at Earth compared with that used to heterodyne (or SpectralInterpolate) the data. If the phase
 * evolution is described by a Taylor expansion:
 * \f[
 * \phi(T) = \sum_{k=1}^n \frac{f^{(k-1)}}{k!} T^k,
 * \f]
 * where \f$ f^(x) \f$ is the xth time derivative of the gravitational wave frequency, and \f$ T \f$ is the pulsar proper
 * time, then the phase difference is given by
 * \f[
 * \Delta\phi(t) = \sum_{k=1}^n \left( \frac{\Delta f^{(k-1)}}{k!}(t+\delta t_1)^k + \frac{f^{(k-1)}_2}{k!} \sum_{i=0}^{i<k} \left(\begin{array}{c}k \\ i\end{array}\right) (\Delta t)^{k-i} (t+\delta t_1)^i \right),
 * \f]
 * where \f$ t \f$ is the signal arrival time at the detector minus the given pulsar period epoch, \f$ \delta t_1 \f$ is the barycentring time delay
 * (from both solar system and binary orbital effects) calculated at the heterodyned values, \f$ \Delta f^{(x)} = f_2^{(x)}-f1^{(x)} \f$
 * is the diffence in frequency (derivative) between the current value ( \f$ f_2^{(x)} \f$ ) and the heterodyne value ( \f$ \f_1^{(x)} \f$ ),
 * and \f$ \Delta t = \delta t_2 - \delta t_1 \f$ is the difference between the barycentring time delay calculated at the
 * current values ( \f$ \delta t_1 \f$ ) and the heterodyned values.
 * Frequency time derivatives are currently allowed up to the tenth derivative. The pulsar proper time is
 * calculated by correcting the time of arrival at Earth, \f$ t \f$ to the solar system barycentre and if necessary the
 * binary system barycenter, so \f$ T = t + \delta{}t_{\rm SSB} + \delta{}t_{\rm BSB} \f$ .
 *
 * In this function the time delay needed to correct to the solar system barycenter is only calculated if
 * required, i.e., if an update is required due to a change in the sky position.
 * The same is true for the binary system time delay, which is only calculated if it
 * needs updating due to a change in the binary system parameters.
 *
 * \param params [in] A set of pulsar parameters
 * \param ifo [in] The ifo model structure containing the detector parameters and buffers
 * \param freqFactor [in] the multiplicative factor on the pulsar frequency for a particular model
 *
 * \return A vector of rotational phase difference values
 *
 * \sa get_ssb_delay
 * \sa get_bsb_delay
 */
REAL8Vector *get_phase_model( PulsarParameters *params, LALInferenceIFOModel *ifo, REAL8 freqFactor )
{
  UINT4 i = 0, j = 0, k = 0, length = 0, isbinary = 0;

  REAL8 DT = 0., deltat = 0., deltatpow = 0., deltatpowinner = 1., taylorcoeff = 1., Ddelay = 0., Ddelaypow = 0.;

  REAL8Vector *phis = NULL, *dts = NULL, *fixdts = NULL, *bdts = NULL, *fixbdts = NULL, *glitchphase = NULL, *fixglitchphase = NULL;
  LIGOTimeGPSVector *datatimes = NULL;

  REAL8 pepoch = PulsarGetREAL8ParamOrZero( params, "PEPOCH" ); /* time of ephem info */
  REAL8 cgw = PulsarGetREAL8ParamOrZero( params, "CGW" );
  REAL8 T0 = pepoch;

  /* check if we want to calculate the phase at a the downsampled rate */
  datatimes = IFO_XTRA_DATA( ifo )->times;

  /* if edat is NULL then return a NULL pointer */
  if ( IFO_XTRA_DATA( ifo )->ephem == NULL ) {
    return NULL;
  }

  length = datatimes->length;

  /* allocate memory for phases */
  phis = XLALCreateREAL8Vector( length );

  /* get time delays */
  fixdts = LALInferenceGetREAL8VectorVariable( ifo->params, "ssb_delays" );
  if ( LALInferenceCheckVariable( ifo->params, "varyskypos" ) ) {
    dts = get_ssb_delay( params, datatimes, IFO_XTRA_DATA( ifo )->ephem, IFO_XTRA_DATA( ifo )->tdat, IFO_XTRA_DATA( ifo )->ttype, ifo->detector );
  }

  if ( LALInferenceCheckVariable( ifo->params, "varybinary" ) ) {
    /* get binary system time delays */
    if ( dts != NULL ) {
      bdts = get_bsb_delay( params, datatimes, dts, IFO_XTRA_DATA( ifo )->ephem );
    } else {
      bdts = get_bsb_delay( params, datatimes, fixdts, IFO_XTRA_DATA( ifo )->ephem );
    }
  }
  if ( LALInferenceCheckVariable( ifo->params, "bsb_delays" ) ) {
    fixbdts = LALInferenceGetREAL8VectorVariable( ifo->params, "bsb_delays" );
  }

  /* get vector of frequencies and frequency differences */
  const REAL8Vector *freqs = PulsarGetREAL8VectorParam( params, "F" );
  const REAL8Vector *deltafs = PulsarGetREAL8VectorParam( params, "DELTAF" );

  if ( PulsarCheckParam( params, "BINARY" ) ) {
    isbinary = 1;  /* see if pulsar is in binary */
  }

  if ( LALInferenceCheckVariable( ifo->params, "varyglitch" ) ) {
    /* get the phase (in cycles due to glitch parameters */
    if ( dts != NULL ) {
      if ( LALInferenceCheckVariable( ifo->params, "varybinary" ) ) {
        glitchphase = get_glitch_phase( params, datatimes, dts, bdts );
      } else {
        glitchphase = get_glitch_phase( params, datatimes, dts, fixbdts );
      }
    } else {
      if ( LALInferenceCheckVariable( ifo->params, "varybinary" ) ) {
        glitchphase = get_glitch_phase( params, datatimes, fixdts, bdts );
      } else {
        glitchphase = get_glitch_phase( params, datatimes, fixdts, fixbdts );
      }
    }
  }
  if ( LALInferenceCheckVariable( ifo->params, "glitch_phase" ) ) {
    fixglitchphase = LALInferenceGetREAL8VectorVariable( ifo->params, "glitch_phase" );
  }

  for ( i = 0; i < length; i++ ) {
    REAL8 deltaphi = 0., innerphi = 0.; /* change in phase */
    Ddelay = 0.;                        /* change in SSB/BSB delay */

    REAL8 realT = XLALGPSGetREAL8( &datatimes->data[i] ); /* time of data */
    DT = realT - T0; /* time diff between data and start of data */

    /* get difference in solar system barycentring time delays */
    if ( dts != NULL ) {
      Ddelay += ( dts->data[i] - fixdts->data[i] );
    }
    deltat = DT + fixdts->data[i];

    if ( isbinary ) {
      /* get difference in binary system barycentring time delays */
      if ( bdts != NULL && fixbdts != NULL ) {
        Ddelay += ( bdts->data[i] - fixbdts->data[i] );
      }
      deltat += fixbdts->data[i];
    }

    /* correct for speed of GW compared to speed of light */
    if ( cgw > 0.0 && cgw < 1. ) {
      deltat /= cgw;
      Ddelay /= cgw;
    }

    /* get the change in phase (compared to the heterodyned phase) */
    deltatpow = deltat;
    for ( j = 0; j < freqs->length; j++ ) {
      taylorcoeff = gsl_sf_fact( j + 1 );
      deltaphi += deltafs->data[j] * deltatpow / taylorcoeff;
      if ( Ddelay != 0. ) {
        innerphi = 0.;
        deltatpowinner = 1.; /* this starts as one as it is first raised to the power of zero */
        Ddelaypow = pow( Ddelay, j + 1 );
        for ( k = 0; k < j + 1; k++ ) {
          innerphi += gsl_sf_choose( j + 1, k ) * Ddelaypow * deltatpowinner;
          deltatpowinner *= deltat; /* raise power */
          Ddelaypow /= Ddelay;      /* reduce power */
        }
        deltaphi += innerphi * freqs->data[j] / taylorcoeff;
      }
      deltatpow *= deltat;
    }

    /* get the differences for glitch phases */
    if ( glitchphase !=  NULL ) {
      deltaphi += ( glitchphase->data[i] - fixglitchphase->data[i] );
    }

    deltaphi *= freqFactor; /* multiply by frequency factor */
    phis->data[i] = deltaphi - floor( deltaphi ); /* only need to keep the fractional part of the phase */
  }

  /* free memory */
  if ( dts != NULL ) {
    XLALDestroyREAL8Vector( dts );
  }
  if ( bdts != NULL ) {
    XLALDestroyREAL8Vector( bdts );
  }
  if ( glitchphase != NULL ) {
    XLALDestroyREAL8Vector( glitchphase );
  }

  return phis;
}


/**
 * \brief Computes the delay between a GPS time at Earth and the solar system barycentre
 *
 * This function calculate the time delay between a GPS time at a specific location (e.g. a gravitational wave detector)
 * on Earth and the solar system barycentre. The delay consists of three components: the geometric time delay (Roemer
 * delay) \f$ t_R = \mathbf{r}(t)\hat{n}/c \f$ (where \f$ \mathbf{r}(t) \f$ is the detector's position vector at time
 * \f$ t \f$ ), the special relativistic Einstein delay \f$ t_E \f$ , and the general relativistic Shapiro delay \f$ t_S \f$ .
 *
 * Rather than computing the time delay at every time stamp passed to the function it is instead (if requested) able to
 * perform linear interpolation to a point within a range given by \c interptime.
 *
 * \param pars [in] A set of pulsar parameters
 * \param datatimes [in] A vector of GPS times at Earth
 * \param ephem [in] Information on the solar system ephemeris
 * \param detector [in] Information on the detector position on the Earth
 * \param tdat *UNDOCUMENTED*
 * \param ttype *UNDOCUMENTED*
 * \return A vector of time delays in seconds
 *
 * \sa XLALBarycenter
 * \sa XLALBarycenterEarth
 */
REAL8Vector *get_ssb_delay( PulsarParameters *pars, LIGOTimeGPSVector *datatimes, EphemerisData *ephem,
                            TimeCorrectionData *tdat, TimeCorrectionType ttype, LALDetector *detector )
{
  INT4 i = 0, length = 0;

  BarycenterInput bary;

  REAL8Vector *dts = NULL;

  /* if edat is NULL then return a NULL poniter */
  if ( ephem == NULL ) {
    return NULL;
  }

  /* copy barycenter and ephemeris data */
  bary.site.location[0] = detector->location[0] / LAL_C_SI;
  bary.site.location[1] = detector->location[1] / LAL_C_SI;
  bary.site.location[2] = detector->location[2] / LAL_C_SI;

  REAL8 ra = 0.;
  if ( PulsarCheckParam( pars, "RA" ) ) {
    ra = PulsarGetREAL8Param( pars, "RA" );
  } else if ( PulsarCheckParam( pars, "RAJ" ) ) {
    ra = PulsarGetREAL8Param( pars, "RAJ" );
  } else {
    XLAL_ERROR_NULL( XLAL_EINVAL, "No source right ascension specified!" );
  }
  REAL8 dec = 0.;
  if ( PulsarCheckParam( pars, "DEC" ) ) {
    dec = PulsarGetREAL8Param( pars, "DEC" );
  } else if ( PulsarCheckParam( pars, "DECJ" ) ) {
    dec = PulsarGetREAL8Param( pars, "DECJ" );
  } else {
    XLAL_ERROR_NULL( XLAL_EINVAL, "No source declination specified!" );
  }
  REAL8 pmra = PulsarGetREAL8ParamOrZero( pars, "PMRA" );
  REAL8 pmdec = PulsarGetREAL8ParamOrZero( pars, "PMDEC" );
  REAL8 pepoch = PulsarGetREAL8ParamOrZero( pars, "PEPOCH" );
  REAL8 posepoch = PulsarGetREAL8ParamOrZero( pars, "POSEPOCH" );
  REAL8 px = PulsarGetREAL8ParamOrZero( pars, "PX" );     /* parallax */

  /* set the position and frequency epochs if not already set */
  if ( pepoch == 0. && posepoch != 0. ) {
    pepoch = posepoch;
  } else if ( posepoch == 0. && pepoch != 0. ) {
    posepoch = pepoch;
  }

  length = datatimes->length;

  /* allocate memory for times delays */
  dts = XLALCreateREAL8Vector( length );

  /* set 1/distance if parallax value is given (1/sec) */
  if ( px != 0. ) {
    bary.dInv = px * ( LAL_C_SI / LAL_AU_SI );
  } else {
    bary.dInv = 0.;
  }

  /* make sure ra and dec are wrapped within 0--2pi and -pi.2--pi/2 respectively */
  ra = fmod( ra, LAL_TWOPI );
  REAL8 absdec = fabs( dec );
  if ( absdec > LAL_PI_2 ) {
    UINT4 nwrap = floor( ( absdec + LAL_PI_2 ) / LAL_PI );
    dec = ( dec > 0 ? 1. : -1. ) * ( nwrap % 2 == 1 ? -1. : 1. ) * ( fmod( absdec + LAL_PI_2, LAL_PI ) - LAL_PI_2 );
    ra = fmod( ra + ( REAL8 )nwrap * LAL_PI, LAL_TWOPI ); /* move RA by pi */
  }

  EarthState earth;
  EmissionTime emit;
  for ( i = 0; i < length; i++ ) {
    REAL8 realT = XLALGPSGetREAL8( &datatimes->data[i] );

    bary.tgps = datatimes->data[i];
    bary.delta = dec + ( realT - posepoch ) * pmdec;
    bary.alpha = ra + ( realT - posepoch ) * pmra / cos( bary.delta );

    /* call barycentring routines */
    XLAL_CHECK_NULL( XLALBarycenterEarthNew( &earth, &bary.tgps, ephem, tdat, ttype ) == XLAL_SUCCESS, XLAL_EFUNC, "Barycentring routine failed" );
    XLAL_CHECK_NULL( XLALBarycenter( &emit, &bary, &earth ) == XLAL_SUCCESS, XLAL_EFUNC, "Barycentring routine failed" );

    dts->data[i] = emit.deltaT;
  }

  return dts;
}


/**
 * \brief Computes the delay between a pulsar in a binary system and the barycentre of the system
 *
 * This function uses \c XLALBinaryPulsarDeltaT to calculate the time delay between for a pulsar in a binary system
 * between the time at the pulsar and the time at the barycentre of the system. This includes Roemer delays and
 * relativistic delays. The orbit may be described by different models and can be purely Keplarian or include various
 * relativistic corrections.
 *
 * \param pars [in] A set of pulsar parameters
 * \param datatimes [in] A vector of GPS times
 * \param dts [in] A vector of solar system barycentre time delays
 * \param edat *UNDOCUMENTED*
 * \return A vector of time delays in seconds
 *
 * \sa XLALBinaryPulsarDeltaT
 */
REAL8Vector *get_bsb_delay( PulsarParameters *pars, LIGOTimeGPSVector *datatimes, REAL8Vector *dts, EphemerisData *edat )
{
  REAL8Vector *bdts = NULL;
  BinaryPulsarInput binput;
  BinaryPulsarOutput boutput;
  EarthState earth;

  INT4 i = 0, length = datatimes->length;

  /* check whether there's a binary model */
  if ( PulsarCheckParam( pars, "BINARY" ) ) {
    bdts = XLALCreateREAL8Vector( length );

    for ( i = 0; i < length; i++ ) {
      binput.tb = XLALGPSGetREAL8( &datatimes->data[i] ) + dts->data[i];

      get_earth_pos_vel( &earth, edat, &datatimes->data[i] );

      binput.earth = earth; /* current Earth state */
      XLALBinaryPulsarDeltaTNew( &boutput, &binput, pars );
      bdts->data[i] = boutput.deltaT;
    }
  }
  return bdts;
}


/**
 * \brief Computes the phase from the glitch model.
 *
 * \param pars [in] A set of pulsar parameters
 * \param datatimes [in] A vector of GPS times
 * \param dts [in] A vector of solar system barycentre time delays
 * \param bdts [in] A vector of binary system barycentre time delays
 * \return A vector of phases in cycles
 */
REAL8Vector *get_glitch_phase( PulsarParameters *pars, LIGOTimeGPSVector *datatimes, REAL8Vector *dts, REAL8Vector *bdts )
{
  REAL8Vector *glphase = NULL;

  /* glitch parameters */
  REAL8 *glep = NULL, *glph = NULL, *glf0 = NULL, *glf1 = NULL, *glf2 = NULL, *glf0d = NULL, *gltd = NULL;
  UINT4 glnum = 0;

  UINT4 i = 0, j = 0, length = datatimes->length;

  REAL8 pepoch = PulsarGetREAL8ParamOrZero( pars, "PEPOCH" ); /* time of ephem info */
  REAL8 cgw = PulsarGetREAL8ParamOrZero( pars, "CGW" );
  REAL8 T0 = pepoch;

  if ( PulsarCheckParam( pars, "GLEP" ) ) { /* see if pulsar has glitch parameters */
    const REAL8Vector *gleppars = PulsarGetREAL8VectorParam( pars, "GLEP" );
    glnum = gleppars->length;

    /* get epochs */
    glep = XLALCalloc( glnum, sizeof( REAL8 ) ); /* initialise to zeros */
    for ( i = 0; i < gleppars->length; i++ ) {
      glep[i] = gleppars->data[i];
    }

    /* get phase offsets */
    glph = XLALCalloc( glnum, sizeof( REAL8 ) ); /* initialise to zeros */
    if ( PulsarCheckParam( pars, "GLPH" ) ) {
      const REAL8Vector *glpars = PulsarGetREAL8VectorParam( pars, "GLPH" );
      for ( i = 0; i < glpars->length; i++ ) {
        glph[i] = glpars->data[i];
      }
    }

    /* get frequencies offsets */
    glf0 = XLALCalloc( glnum, sizeof( REAL8 ) ); /* initialise to zeros */
    if ( PulsarCheckParam( pars, "GLF0" ) ) {
      const REAL8Vector *glpars = PulsarGetREAL8VectorParam( pars, "GLF0" );
      for ( i = 0; i < glpars->length; i++ ) {
        glf0[i] = glpars->data[i];
      }
    }

    /* get frequency derivative offsets */
    glf1 = XLALCalloc( glnum, sizeof( REAL8 ) ); /* initialise to zeros */
    if ( PulsarCheckParam( pars, "GLF1" ) ) {
      const REAL8Vector *glpars = PulsarGetREAL8VectorParam( pars, "GLF1" );
      for ( i = 0; i < glpars->length; i++ ) {
        glf1[i] = glpars->data[i];
      }
    }

    /* get second frequency derivative offsets */
    glf2 = XLALCalloc( glnum, sizeof( REAL8 ) ); /* initialise to zeros */
    if ( PulsarCheckParam( pars, "GLF2" ) ) {
      const REAL8Vector *glpars = PulsarGetREAL8VectorParam( pars, "GLF2" );
      for ( i = 0; i < glpars->length; i++ ) {
        glf2[i] = glpars->data[i];
      }
    }

    /* get decaying frequency component offset derivative */
    glf0d = XLALCalloc( glnum, sizeof( REAL8 ) ); /* initialise to zeros */
    if ( PulsarCheckParam( pars, "GLF0D" ) ) {
      const REAL8Vector *glpars = PulsarGetREAL8VectorParam( pars, "GLF0D" );
      for ( i = 0; i < glpars->length; i++ ) {
        glf0d[i] = glpars->data[i];
      }
    }

    /* get decaying frequency component decay time constant */
    gltd = XLALCalloc( glnum, sizeof( REAL8 ) ); /* initialise to zeros */
    if ( PulsarCheckParam( pars, "GLTD" ) ) {
      const REAL8Vector *glpars = PulsarGetREAL8VectorParam( pars, "GLTD" );
      for ( i = 0; i < glpars->length; i++ ) {
        gltd[i] = glpars->data[i];
      }
    }

    glphase = XLALCreateREAL8Vector( length );

    for ( i = 0; i < length; i++ ) {
      REAL8 deltaphi = 0., deltat = 0., DT = 0.;
      REAL8 realT = XLALGPSGetREAL8( &datatimes->data[i] ); /* time of data */
      DT = realT - T0; /* time diff between data and start of data */

      /* include solar system barycentring time delays */
      deltat = DT + dts->data[i];

      /* include binary system barycentring time delays */
      if ( bdts != NULL ) {
        deltat += bdts->data[i];
      }

      /* correct for speed of GW compared to speed of light */
      if ( cgw > 0.0 && cgw < 1. ) {
        deltat /= cgw;
      }

      /* get glitch phase - based on equations in formResiduals.C of TEMPO2 from Eqn 1 of Yu et al (2013) http://ukads.nottingham.ac.uk/abs/2013MNRAS.429..688Y */
      for ( j = 0; j < glnum; j++ ) {
        if ( deltat >= ( glep[j] - T0 ) ) {
          REAL8 dtg = 0, expd = 1.;
          dtg = deltat - ( glep[j] - T0 ); /* time since glitch */
          if ( gltd[j] != 0. ) {
            expd = exp( -dtg / gltd[j] );  /* decaying part of glitch */
          }
          deltaphi += glph[j] + glf0[j] * dtg + 0.5 * glf1[j] * dtg * dtg + ( 1. / 6. ) * glf2[j] * dtg * dtg * dtg + glf0d[j] * gltd[j] * ( 1. - expd );
        }
      }

      glphase->data[i] = deltaphi;
    }
  }

  return glphase;
}


/**
 * \brief The amplitude model of a complex heterodyned signal from the \f$ l=2, m=1,2 \f$ harmonics of a rotating neutron
 * star.
 *
 * This function calculates the complex heterodyned time series model for a rotating neutron star. It will currently
 * calculate the model for emission from the \f$ l=m=2 \f$ harmonic (which gives emission at twice the rotation frequency)
 * and/or the \f$ l=2 \f$ and \f$ m=1 \f$ harmonic (which gives emission at the rotation frequency). See LIGO T1200265-v3.
 * Further harmonics can be added and are defined by the \c freqFactor value, which is the multiple of the
 * spin-frequency at which emission is produced.
 *
 * The antenna pattern functions are contained in a 1D lookup table, so within this function the correct value for the
 * given time is interpolated from this lookup table using linear interpolation..
 *
 * \param pars [in] A set of pulsar parameters
 * \param ifo  [in] The ifo model containing detector-specific parameters
 *
 */
void get_amplitude_model( PulsarParameters *pars, LALInferenceIFOModel *ifo )
{
  UINT4 i = 0, j = 0, length;

  REAL8 T, twopsi;
  REAL8 cosiota = PulsarGetREAL8ParamOrZero( pars, "COSIOTA" );
  REAL8 siniota = sin( acos( cosiota ) );
  REAL8 s2psi = 0., c2psi = 0., spsi = 0., cpsi = 0.;
  UINT4 nonGR = 0;

  REAL8Vector *freqFactors = NULL;
  INT4 varyphase = 0, roq = 0;

  freqFactors = *( REAL8Vector ** )LALInferenceGetVariable( ifo->params, "freqfactors" );

  if ( LALInferenceCheckVariable( ifo->params, "varyphase" ) ) {
    varyphase = 1;
  }
  if ( LALInferenceCheckVariable( ifo->params, "roq" ) ) {
    roq = 1;
  }

  twopsi = 2.*PulsarGetREAL8ParamOrZero( pars, "PSI" );
  s2psi = sin( twopsi );
  c2psi = cos( twopsi );

  /* check for non-GR model */
  if ( LALInferenceCheckVariable( ifo->params, "nonGR" ) ) {
    nonGR = *( UINT4 * )LALInferenceGetVariable( ifo->params, "nonGR" );
  }

  if ( nonGR == 1 ) {
    spsi = sin( PulsarGetREAL8ParamOrZero( pars, "PSI" ) );
    cpsi = cos( PulsarGetREAL8ParamOrZero( pars, "PSI" ) );
  }

  /* loop over all detectors */
  while ( ifo ) {
    /* loop over components in data as given by the frequency factors */
    for ( j = 0; j < freqFactors->length; j++ ) {
      COMPLEX16 expPhi;
      COMPLEX16 Cplus = 0., Ccross = 0., Cx = 0., Cy = 0., Cl = 0., Cb = 0.;

      if ( !ifo ) {
        XLAL_ERROR_VOID( XLAL_EINVAL, "Error... ifo model not defined." );
      }

      /* get the amplitude and phase factors */
      if ( freqFactors->data[j] == 1. ) {
        /* the l=2, m=1 harmonic at the rotation frequency */
        if ( nonGR ) { /* amplitude if nonGR is specified */
          COMPLEX16 expPhiTensor, expPsiTensor, expPhiScalar, expPsiScalar, expPhiVector, expPsiVector;

          expPhiTensor = cexp( I * PulsarGetREAL8ParamOrZero( pars, "PHI0TENSOR_F" ) );
          expPsiTensor = cexp( I * PulsarGetREAL8ParamOrZero( pars, "PSITENSOR_F" ) );
          expPhiScalar = cexp( I * PulsarGetREAL8ParamOrZero( pars, "PHI0SCALAR_F" ) );
          expPsiScalar = cexp( I * PulsarGetREAL8ParamOrZero( pars, "PSISCALAR_F" ) );
          expPhiVector = cexp( I * PulsarGetREAL8ParamOrZero( pars, "PHI0VECTOR_F" ) );
          expPsiVector = cexp( I * PulsarGetREAL8ParamOrZero( pars, "PSIVECTOR_F" ) );

          Cplus = 0.5 * expPhiTensor * PulsarGetREAL8ParamOrZero( pars, "HPLUS_F" );
          Ccross = 0.5 * expPhiTensor * PulsarGetREAL8ParamOrZero( pars, "HCROSS_F" ) * expPsiTensor;
          Cx = 0.5 * expPhiVector * PulsarGetREAL8ParamOrZero( pars, "HVECTORX_F" );
          Cy = 0.5 * expPhiVector * PulsarGetREAL8ParamOrZero( pars, "HVECTORY_F" ) * expPsiVector;
          Cb = 0.5 * expPhiScalar * PulsarGetREAL8ParamOrZero( pars, "HSCALARB_F" );
          Cl = 0.5 * expPhiScalar * PulsarGetREAL8ParamOrZero( pars, "HSCALARL_F" ) * expPsiScalar;
        } else {
          expPhi = cexp( I * PulsarGetREAL8ParamOrZero( pars, "PHI21" ) );
          Cplus = -0.25 * PulsarGetREAL8ParamOrZero( pars, "C21" ) * siniota * cosiota * expPhi;
          Ccross = 0.25 * I * PulsarGetREAL8ParamOrZero( pars, "C21" ) * siniota * expPhi;
        }
      } else if ( freqFactors->data[j] == 2. ) {
        /* the l=2, m=2 harmonic at twice the rotation frequency */
        if ( nonGR ) { /* amplitude if nonGR is specified */
          COMPLEX16 expPhiTensor, expPsiTensor, expPhiScalar, expPsiScalar, expPhiVector, expPsiVector;

          expPhiTensor = cexp( I * PulsarGetREAL8ParamOrZero( pars, "PHI0TENSOR" ) );
          expPsiTensor = cexp( I * PulsarGetREAL8ParamOrZero( pars, "PSITENSOR" ) );
          expPhiScalar = cexp( I * PulsarGetREAL8ParamOrZero( pars, "PHI0SCALAR" ) );
          expPsiScalar = cexp( I * PulsarGetREAL8ParamOrZero( pars, "PSISCALAR" ) );
          expPhiVector = cexp( I * PulsarGetREAL8ParamOrZero( pars, "PHI0VECTOR" ) );
          expPsiVector = cexp( I * PulsarGetREAL8ParamOrZero( pars, "PSIVECTOR" ) );

          Cplus = 0.5 * expPhiTensor * PulsarGetREAL8ParamOrZero( pars, "HPLUS" );
          Ccross = 0.5 * expPhiTensor * PulsarGetREAL8ParamOrZero( pars, "HCROSS" ) * expPsiTensor;
          Cx = 0.5 * expPhiVector * PulsarGetREAL8ParamOrZero( pars, "HVECTORX" );
          Cy = 0.5 * expPhiVector * PulsarGetREAL8ParamOrZero( pars, "HVECTORY" ) * expPsiVector;
          Cb = 0.5 * expPhiScalar * PulsarGetREAL8ParamOrZero( pars, "HSCALARB" );
          Cl = 0.5 * expPhiScalar * PulsarGetREAL8ParamOrZero( pars, "HSCALARL" ) * expPsiScalar;
        } else { /* just GR tensor mode amplitudes */
          expPhi = cexp( I * PulsarGetREAL8ParamOrZero( pars, "PHI22" ) );
          Cplus = -0.5 * PulsarGetREAL8ParamOrZero( pars, "C22" ) * ( 1. + cosiota * cosiota ) * expPhi;
          Ccross = I * PulsarGetREAL8ParamOrZero( pars, "C22" ) * cosiota * expPhi;
        }
      } else {
        XLAL_ERROR_VOID( XLAL_EINVAL, "Error... currently unknown frequency factor (%.2lf) for models.", freqFactors->data[j] );
      }

      if ( varyphase || roq ) { /* have to compute the full time domain signal */
        REAL8Vector *sidDayFrac = NULL;
        REAL8 tsv, tsteps;

        REAL8Vector *LUfplus = NULL, *LUfcross = NULL, *LUfx = NULL, *LUfy = NULL, *LUfb = NULL, *LUfl = NULL;

        /* set lookup table parameters */
        tsteps = ( REAL8 )( *( INT4 * )LALInferenceGetVariable( ifo->params, "timeSteps" ) );
        tsv = LAL_DAYSID_SI / tsteps;

        LUfplus = *( REAL8Vector ** )LALInferenceGetVariable( ifo->params, "a_response_tensor" );
        LUfcross = *( REAL8Vector ** )LALInferenceGetVariable( ifo->params, "b_response_tensor" );

        if ( nonGR ) {
          LUfx = *( REAL8Vector ** )LALInferenceGetVariable( ifo->params, "a_response_vector" );
          LUfy = *( REAL8Vector ** )LALInferenceGetVariable( ifo->params, "b_response_vector" );
          LUfb = *( REAL8Vector ** )LALInferenceGetVariable( ifo->params, "a_response_scalar" );
          LUfl = *( REAL8Vector ** )LALInferenceGetVariable( ifo->params, "b_response_scalar" );
        }

        /* get the sidereal time since the initial data point % sidereal day */
        sidDayFrac = *( REAL8Vector ** )LALInferenceGetVariable( ifo->params, "siderealDay" );

        length = IFO_XTRA_DATA( ifo )->times->length;

        for ( i = 0; i < length; i++ ) {
          REAL8 plus00, plus01, cross00, cross01, plus = 0., cross = 0.;
          REAL8 x00, x01, y00, y01, b00, b01, l00, l01;
          REAL8 timeScaled, timeMin, timeMax;
          REAL8 plusT = 0., crossT = 0., x = 0., y = 0., xT = 0., yT = 0., b = 0., l = 0.;
          INT4 timebinMin, timebinMax;

          /* set the time bin for the lookup table */
          /* sidereal day in secs*/
          T = sidDayFrac->data[i];
          timebinMin = ( INT4 )fmod( floor( T / tsv ), tsteps );
          timeMin = timebinMin * tsv;
          timebinMax = ( INT4 )fmod( timebinMin + 1, tsteps );
          timeMax = timeMin + tsv;

          /* get values of matrix for linear interpolation */
          plus00 = LUfplus->data[timebinMin];
          plus01 = LUfplus->data[timebinMax];

          cross00 = LUfcross->data[timebinMin];
          cross01 = LUfcross->data[timebinMax];

          /* rescale time for linear interpolation on a unit square */
          timeScaled = ( T - timeMin ) / ( timeMax - timeMin );

          plus = plus00 + ( plus01 - plus00 ) * timeScaled;
          cross = cross00 + ( cross01 - cross00 ) * timeScaled;

          plusT = plus * c2psi + cross * s2psi;
          crossT = cross * c2psi - plus * s2psi;

          if ( nonGR ) {
            x00 = LUfx->data[timebinMin];
            x01 = LUfx->data[timebinMax];
            y00 = LUfy->data[timebinMin];
            y01 = LUfy->data[timebinMax];
            b00 = LUfb->data[timebinMin];
            b01 = LUfb->data[timebinMax];
            l00 = LUfl->data[timebinMin];
            l01 = LUfl->data[timebinMax];

            x = x00 + ( x01 - x00 ) * timeScaled;
            y = y00 + ( y01 - y00 ) * timeScaled;
            b = b00 + ( b01 - b00 ) * timeScaled;
            l = l00 + ( l01 - l00 ) * timeScaled;

            xT = x * cpsi + y * spsi;
            yT = y * cpsi - x * spsi;
          }

          /* create the complex signal amplitude model appropriate for the harmonic */
          ifo->compTimeSignal->data->data[i] = ( Cplus * plusT ) + ( Ccross * crossT );

          /* add non-GR components if required */
          if ( nonGR ) {
            ifo->compTimeSignal->data->data[i] += ( Cx * xT ) + ( Cy * yT ) + Cb * b + Cl * l;
          }
        }
      } else { /* just have to calculate the values to multiply the pre-summed data */
        /* for tensor-only models (e.g. the default of GR) calculate the two components of
         * the single model value - things multiplied by a(t) and things multiplied by b(t)
         * (both these will have real and imaginary components). NOTE: the values input into
         * ifo->compTimeSignal->data->data are not supposed to be identical to the above
         * relationships between the amplitudes and polarisation angles, as these are the
         * multiplicative coefficients of the a(t) and b(t) summations.
         */

        /* put multiples of a(t) in first value and b(t) in second */
        if ( !nonGR ) {
          /* first check that compTimeSignal has been reduced in size to just hold these two values */
          if ( ifo->compTimeSignal->data->length != 2 ) { /* otherwise resize it */
            ifo->compTimeSignal = XLALResizeCOMPLEX16TimeSeries( ifo->compTimeSignal, 0, 2 );
          }

          ifo->compTimeSignal->data->data[0] = ( Cplus * c2psi - Ccross * s2psi );
          ifo->compTimeSignal->data->data[1] = ( Cplus * s2psi + Ccross * c2psi );
        } else {
          /* first check that compTimeSignal has been reduced in size to just hold these size values */
          if ( ifo->compTimeSignal->data->length != 6 ) { /* otherwise resize it */
            ifo->compTimeSignal = XLALResizeCOMPLEX16TimeSeries( ifo->compTimeSignal, 0, 6 );
          }

          ifo->compTimeSignal->data->data[0] = ( Cplus * c2psi - Ccross * s2psi );
          ifo->compTimeSignal->data->data[1] = ( Cplus * s2psi + Ccross * c2psi );
          ifo->compTimeSignal->data->data[2] = ( Cx * cpsi - Cy * spsi );
          ifo->compTimeSignal->data->data[3] = ( Cx * spsi + Cy * cpsi );
          ifo->compTimeSignal->data->data[4] = Cb;
          ifo->compTimeSignal->data->data[5] = Cl;
        }
      }

      ifo = ifo->next;
    }
  }
}


/**
 * \brief Calculate the phase mismatch between two vectors of phases
 *
 * The function will calculate phase mismatch between two vectors of phases (with phases given in cycles rather than
 * radians).
 *
 * The mismatch is calculated as:
 * \f[
 * M = 1-\frac{1}{T}\int_0^T \cos{2\pi(\phi_1 - \phi_2)} dt.
 * \f]
 * In the function the integral is performed using the trapezium rule.
 *
 * PARAM phi1 [in] First phase vector
 * PARAM phi2 [in] Second phase vector
 * PARAM t [in] The time stamps of the phase points
 *
 * \return The mismatch
 */
REAL8 get_phase_mismatch( REAL8Vector *phi1, REAL8Vector *phi2, LIGOTimeGPSVector *t )
{
  REAL8 mismatch = 0., dp1 = 0., dp2 = 0.;
  REAL4 sp, cp1, cp2;
  UINT4 i = 0;

  REAL8 T = 0., dt = 0.;

  /* data time span */
  T = XLALGPSGetREAL8( &t->data[t->length - 1] ) - XLALGPSGetREAL8( &t->data[0] );

  if ( phi1->length != phi2->length ) {
    XLAL_ERROR_REAL8( XLAL_EBADLEN, "Phase lengths should be equal!" );
  }

  /* calculate mismatch - integrate with trapezium rule */
  for ( i = 0; i < phi1->length - 1; i++ ) {
    if ( i == 0 ) {
      dp1 = fmod( phi1->data[i] - phi2->data[i], 1. );
      XLAL_CHECK_REAL8( XLALSinCos2PiLUT( &sp, &cp1, dp1 ) == XLAL_SUCCESS, XLAL_EFUNC );
    } else {
      dp1 = dp2;
      cp1 = cp2;
    }

    dp2 = fmod( phi1->data[i + 1] - phi2->data[i + 1], 1. );

    dt = XLALGPSGetREAL8( &t->data[i + 1] ) - XLALGPSGetREAL8( &t->data[i] );

    XLAL_CHECK_REAL8( XLALSinCos2PiLUT( &sp, &cp2, dp2 ) == XLAL_SUCCESS, XLAL_EFUNC );

    mismatch += ( cp1 + cp2 ) * dt;
  }

  return ( 1. - fabs( mismatch ) / ( 2.*T ) );
}


/**
 * \brief Get the position and velocity of the Earth at a given time
 *
 * This function will get the position and velocity of the Earth from the ephemeris data at the time t. It will be
 * returned in an EarthState structure. This is based on the start of the XLALBarycenterEarth function.
 */
void get_earth_pos_vel( EarthState *earth, EphemerisData *edat, LIGOTimeGPS *tGPS )
{
  REAL8 tgps[2];

  REAL8 t0e;        /* time since first entry in Earth ephem. table */
  INT4 ientryE;     /* entry in look-up table closest to current time, tGPS */

  REAL8 tinitE;     /* time (GPS) of first entry in Earth ephem table */
  REAL8 tdiffE;     /* current time tGPS minus time of nearest entry in Earth ephem look-up table */
  REAL8 tdiff2E;    /* tdiff2 = tdiffE * tdiffE */

  INT4 j;

  /* check input */
  if ( !earth || !tGPS || !edat || !edat->ephemE || !edat->ephemS ) {
    XLAL_ERROR_VOID( XLAL_EINVAL, "Invalid NULL input 'earth', 'tGPS', 'edat','edat->ephemE' or 'edat->ephemS'" );
  }

  tgps[0] = ( REAL8 )tGPS->gpsSeconds; /* convert from INT4 to REAL8 */
  tgps[1] = ( REAL8 )tGPS->gpsNanoSeconds;

  tinitE = edat->ephemE[0].gps;

  t0e = tgps[0] - tinitE;
  ientryE = ROUND( t0e / edat->dtEtable ); /* finding Earth table entry */

  if ( ( ientryE < 0 ) || ( ientryE >=  edat->nentriesE ) ) {
    XLAL_ERROR_VOID( XLAL_EDOM, "Input GPS time %f outside of Earth ephem range [%f, %f]\n", tgps[0], tinitE, tinitE +
                     edat->nentriesE * edat->dtEtable );
  }

  /* tdiff is arrival time minus closest Earth table entry; tdiff can be pos. or neg. */
  tdiffE = t0e - edat->dtEtable * ientryE + tgps[1] * 1.e-9;
  tdiff2E = tdiffE * tdiffE;

  REAL8 *pos = edat->ephemE[ientryE].pos;
  REAL8 *vel = edat->ephemE[ientryE].vel;
  REAL8 *acc = edat->ephemE[ientryE].acc;

  for ( j = 0; j < 3; j++ ) {
    earth->posNow[j] = pos[j] + vel[j] * tdiffE + 0.5 * acc[j] * tdiff2E;
    earth->velNow[j] = vel[j] + acc[j] * tdiffE;
  }
}


/**
 * \brief Creates a lookup table of the detector antenna pattern
 *
 * This function creates a lookup table of the tensor, vector and scalar antenna patterns for a given
 * detector orientation and source sky position. For the tensor modes these are the functions given by
 * equations 10-13 in \cite JKS98 , whilst for the vector and scalar modes they are the \f$ \psi \f$
 * independent parts of e.g. equations 5-8 of \cite Nishizawa2009 . We remove the \f$ \psi \f$ dependent
 * by setting \f$ \psi=0 \f$ .
 *
 * If \c avedt is a value over 60 seconds then the antenna pattern will actually be the mean value from
 * 60 second intervals within that timespan. This accounts for the fact that each data point is actually an
 * averaged value over the given timespan.
 *
 * \param t0 [in] initial GPS time of the data
 * \param detNSource [in] structure containing the detector and source orientations and locations
 * \param timeSteps [in] the number of grid bins to use in time
 * \param avedt [in] average the antenna pattern over this timespan
 * \param aT [in] a vector into which the a(t) Fplus tensor antenna pattern lookup table will be output
 * \param bT [in] a vector into which the b(t) Fcross tensor antenna pattern lookup table will be output
 * \param aV [in] a vector into which the a(t) Fx vector antenna pattern lookup table will be output
 * \param bV [in] a vector into which the b(t) Fy vector antenna pattern lookup table will be output
 * \param aS [in] a vector into which the a(t) Fb scalar antenna pattern lookup table will be output
 * \param bS [in] a vector into which the b(t) Fl scalar antenna pattern lookup table will be output
 */
void response_lookup_table( REAL8 t0, LALDetAndSource detNSource, INT4 timeSteps, REAL8 avedt, REAL8Vector *aT,
                            REAL8Vector *bT, REAL8Vector *aV, REAL8Vector *bV, REAL8Vector *aS,
                            REAL8Vector *bS )
{
  LIGOTimeGPS gps;
  REAL8 T = 0, Tstart = 0., Tav = 0;

  REAL8 fplus = 0., fcross = 0., fx = 0., fy = 0., fb = 0., fl = 0.;
  REAL8 tsteps = ( REAL8 )timeSteps;

  INT4 j = 0, k = 0, nav = 0;

  /* number of points to average */
  if ( avedt == 60. ) {
    nav = 1;
  } else {
    nav = floor( avedt / 60. ) + 1;
  }

  /* set the polarisation angle to zero to get the a(t) and b(t) antenna pattern functions */
  detNSource.pSource->orientation = 0.0;

  for ( j = 0 ; j < timeSteps ; j++ ) {
    aT->data[j] = 0.;
    bT->data[j] = 0.;
    aV->data[j] = 0.;
    bV->data[j] = 0.;
    aS->data[j] = 0.;
    bS->data[j] = 0.;

    /* central time of lookup table point */
    T = t0 + ( REAL8 )j * LAL_DAYSID_SI / tsteps;

    if ( nav % 2 ) { /* is odd */
      Tstart = T - 0.5 * ( ( REAL8 )nav - 1. ) * 60.;
    } else { /* is even */
      Tstart = T - ( 0.5 * ( REAL8 )nav - 1. ) * 60. - 30.;
    }

    for ( k = 0; k < nav; k++ ) {
      Tav = Tstart + 60.*( REAL8 )k;

      XLALGPSSetREAL8( &gps, Tav );

      XLALComputeDetAMResponseExtraModes( &fplus, &fcross, &fb, &fl, &fx, &fy, detNSource.pDetector->response,
                                          detNSource.pSource->equatorialCoords.longitude,
                                          detNSource.pSource->equatorialCoords.latitude,
                                          detNSource.pSource->orientation, XLALGreenwichMeanSiderealTime( &gps ) );

      aT->data[j] += fplus;
      bT->data[j] += fcross;
      aV->data[j] += fx;
      bV->data[j] += fy;
      aS->data[j] += fb;
      bS->data[j] += fl;
    }

    aT->data[j] /= ( REAL8 )nav;
    bT->data[j] /= ( REAL8 )nav;
    aV->data[j] /= ( REAL8 )nav;
    bV->data[j] /= ( REAL8 )nav;
    aS->data[j] /= ( REAL8 )nav;
    bS->data[j] /= ( REAL8 )nav;
  }
}


/*------------------------ END OF MODEL FUNCTIONS ----------------------------*/


/*----------------- FUNCTIONS TO CONVERT BETWEEN PARAMETERS ------------------*/

/**
 * \brief Convert sources parameters into amplitude and phase notation parameters
 *
 * Convert the physical source parameters into the amplitude and phase notation given in Eqns
 * 62-65 of \cite Jones:2015 .
 *
 * Note that \c phi0 is essentially the rotational phase of the pulsar. Also, note that if using \f$ h_0 \f$ ,
 * and therefore the convention for a signal as defined in \cite JKS98 , the sign of the waveform model is
 * the opposite of that in \cite Jones:2015 , and therefore a sign flip is required in the amplitudes.
 */
void invert_source_params( PulsarParameters *params )
{
  REAL8 sinlambda, coslambda, sinlambda2, coslambda2, sin2lambda;
  REAL8 theta, sintheta, costheta2, sintheta2, sin2theta;
  REAL8 phi0 = PulsarGetREAL8ParamOrZero( params, "PHI0" );
  REAL8 h0 = PulsarGetREAL8ParamOrZero( params, "H0" );
  REAL8 I21 = PulsarGetREAL8ParamOrZero( params, "I21" );
  REAL8 I31 = PulsarGetREAL8ParamOrZero( params, "I31" );
  REAL8 C21 = PulsarGetREAL8ParamOrZero( params, "C21" );
  REAL8 C22 = PulsarGetREAL8ParamOrZero( params, "C22" );
  REAL8 phi21 = PulsarGetREAL8ParamOrZero( params, "PHI21" );
  REAL8 phi22 = PulsarGetREAL8ParamOrZero( params, "PHI22" );
  REAL8 lambda = PulsarGetREAL8ParamOrZero( params, "LAMBDA" );
  REAL8 costheta = PulsarGetREAL8ParamOrZero( params, "COSTHETA" );

  if ( h0 != 0. ) {
    phi22 = 2.*phi0;
    phi22 = phi22 - LAL_TWOPI * floor( phi22 / LAL_TWOPI );
    PulsarAddREAL8Param( params, "PHI22", phi22 );

    C22 = -0.5 * h0; /* note the change in sign so that the triaxial model conforms to the convertion in JKS98 */
    PulsarAddREAL8Param( params, "C22", C22 );
  } else if ( ( I21 != 0. || I31 != 0. ) && ( C22 == 0. && C21 == 0. ) ) {
    sinlambda = sin( lambda );
    coslambda = cos( lambda );
    sin2lambda = sin( 2. * lambda );
    sinlambda2 = SQUARE( sinlambda );
    coslambda2 = SQUARE( coslambda );

    theta = acos( costheta );
    sintheta = sin( theta );
    sin2theta = sin( 2. * theta );
    sintheta2 = SQUARE( sintheta );
    costheta2 = SQUARE( costheta );

    REAL8 A22 = I21 * ( sinlambda2 - coslambda2 * costheta2 ) - I31 * sintheta2;
    REAL8 B22 = I21 * sin2lambda * costheta;
    REAL8 A222 = SQUARE( A22 );
    REAL8 B222 = SQUARE( B22 );

    REAL8 A21 = I21 * sin2lambda * sintheta;
    REAL8 B21 = sin2theta * ( I21 * coslambda2 - I31 );
    REAL8 A212 = SQUARE( A21 );
    REAL8 B212 = SQUARE( B21 );

    C22 = 2.*sqrt( A222 + B222 );
    C21 = 2.*sqrt( A212 + B212 );

    PulsarAddREAL8Param( params, "C22", C22 );
    PulsarAddREAL8Param( params, "C21", C21 );

    phi22 = fmod( 2.*phi0 - atan2( B22, A22 ), LAL_TWOPI );
    phi21 = fmod( phi0 - atan2( B21, A21 ), LAL_TWOPI );

    PulsarAddREAL8Param( params, "PHI22", phi22 );
    PulsarAddREAL8Param( params, "PHI21", phi21 );
  }
}
