/*
 *  Copyright (C) 2007 Badri Krishnan
 *  Copyright (C) 2008 Christine Chung, Badri Krishnan and John Whelan
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
#ifndef _PULSARCROSSCORR_H
#define _PULSARCROSSCORR_H

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * \defgroup PulsarCrossCorr_h Header PulsarCrossCorr.h
 * \ingroup lalpulsar_crosscorr
 * \author Christine Chung, Badri Krishnan, John Whelan
 * \date 2008
 * \brief Header-file for LAL routines for CW cross-correlation searches
 *
 */
/** @{ */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#if HAVE_GLOB_H
#include <glob.h>
#endif
#include <time.h>
#include <errno.h>

#include <lal/AVFactories.h>
#include <lal/Date.h>
#include <lal/DetectorSite.h>
#include <lal/LALDatatypes.h>
#include <lal/LALHough.h>
#include <lal/RngMedBias.h>
#include <lal/LALRunningMedian.h>
#include <lal/Velocity.h>
#include <lal/Statistics.h>
#include <lal/ComputeFstat.h>
#include <lal/SFTfileIO.h>
#include <lal/NormalizeSFTRngMed.h>
#include <lal/LALInitBarycenter.h>
#include <lal/SFTClean.h>
#include <gsl/gsl_cdf.h>
#include <lal/FrequencySeries.h>
#include <lal/Sequence.h>

/** \name Error codes */
/** @{ */
#define PULSARCROSSCORR_ENULL 1
#define PULSARCROSSCORR_ENONULL 2
#define PULSARCROSSCORR_EVAL 3

#define PULSARCROSSCORR_MSGENULL "Null pointer"
#define PULSARCROSSCORR_MSGENONULL "Non-null pointer"
#define PULSARCROSSCORR_MSGEVAL "Invalid value"
/** @} */

/* ******************************************************************
 *  Structure, enum, union, etc., typdefs.
 */

typedef enum tagDetChoice {
  SAME,
  DIFFERENT,
  ALL
} DetChoice;


/** struct holding info about skypoints */
typedef struct tagSkyPatchesInfo {
  UINT4 numSkyPatches;
  REAL8 *alpha;
  REAL8 *delta;
  REAL8 *alphaSize;
  REAL8 *deltaSize;
} SkyPatchesInfo;

typedef struct tagSFTDetectorInfo {
  COMPLEX8FrequencySeries *sft;
  REAL8 vDetector[3];
  REAL8 rDetector[3];
  REAL8 a;
  REAL8 b;
} SFTDetectorInfo;

/* define structs to hold combinations of F's and A's */
typedef struct tagCrossCorrAmps {
  REAL8 Aplussq;
  REAL8 Acrosssq;
  REAL8 AplusAcross;
} CrossCorrAmps;

typedef struct tagCrossCorrBeamFn {
  REAL8 a;
  REAL8 b;
} CrossCorrBeamFn;

typedef struct tagSFTListElement {
  SFTtype sft;
  struct tagSFTListElement *nextSFT;
} SFTListElement;

typedef struct tagPSDListElement {
  REAL8FrequencySeries psd;
  struct tagPSDListElement *nextPSD;
} PSDListElement;

typedef struct tagREAL8ListElement {
  REAL8 val;
  struct tagREAL8ListElement *nextVal;
} REAL8ListElement;

typedef struct tagCrossCorrBeamFnListElement {
  CrossCorrBeamFn beamfn;
  struct tagCrossCorrBeamFnListElement *nextBeamfn;
} CrossCorrBeamFnListElement;
/*
 *  Functions Declarations (i.e., prototypes).
 */

void LALCreateSFTPairsIndicesFrom2SFTvectors( LALStatus          *status,
    INT4VectorSequence **out,
    SFTListElement     *in,
    REAL8              lag,
    INT4               listLength,
    DetChoice          detChoice,
    BOOLEAN            autoCorrelate );

void LALCorrelateSingleSFTPair( LALStatus                *status,
                                COMPLEX16                *out,
                                COMPLEX8FrequencySeries  *sft1,
                                COMPLEX8FrequencySeries  *sft2,
                                REAL8FrequencySeries     *psd1,
                                REAL8FrequencySeries     *psd2,
                                UINT4                    bin1,
                                UINT4                    bin2 );

void LALGetSignalFrequencyInSFT( LALStatus                *status,
                                 REAL8                    *out,
                                 LIGOTimeGPS              *epoch,
                                 PulsarDopplerParams      *dopp,
                                 REAL8Vector              *vel );

void LALGetSignalPhaseInSFT( LALStatus               *status,
                             REAL8                   *out,
                             LIGOTimeGPS             *epoch,
                             PulsarDopplerParams     *dopp,
                             REAL8Vector             *pos );

void LALCalculateSigmaAlphaSq( LALStatus            *status,
                               REAL8                *out,
                               UINT4                bin1,
                               UINT4                bin2,
                               REAL8FrequencySeries *psd1,
                               REAL8FrequencySeries *psd2 );

void LALCalculateAveUalpha( LALStatus *status,
                            COMPLEX16 *out,
                            REAL8     phiI,
                            REAL8     phiJ,
                            REAL8     freqI,
                            REAL8     freqJ,
                            REAL8     deltaF,
                            CrossCorrBeamFn beamfnsI,
                            CrossCorrBeamFn beamfnsJ,
                            REAL8     sigmasq );

void LALCalculateUalpha( LALStatus *status,
                         COMPLEX16 *out,
                         CrossCorrAmps amplitudes,
                         REAL8     phiI,
                         REAL8     phiJ,
                         REAL8     freqI,
                         REAL8     freqJ,
                         REAL8     deltaF,
                         CrossCorrBeamFn beamfnsI,
                         CrossCorrBeamFn beamfnsJ,
                         REAL8     sigmasq,
                         REAL8     *psi,
                         COMPLEX16 *gplus,
                         COMPLEX16 *gcross );

void LALCalculateCrossCorrPower( LALStatus       *status,
                                 REAL8           *out,
                                 COMPLEX16Vector *yalpha,
                                 COMPLEX16Vector *ualpha );

void LALNormaliseCrossCorrPower( LALStatus        *status,
                                 REAL8            *out,
                                 COMPLEX16Vector  *ualpha,
                                 REAL8Vector      *sigmaAlphasq );

void LALCalculateEstimators( LALStatus    *status,
                             REAL8 *aplussq1,
                             REAL8   *aplussq2,
                             REAL8   *acrossq1,
                             REAL8 *acrossq2,
                             COMPLEX16Vector  *yalpha,
                             COMPLEX16Vector  *gplus,
                             COMPLEX16Vector  *gcross,
                             REAL8Vector      *sigmaAlphasq );

/** @} */

#ifdef  __cplusplus
}                /* Close C++ protection */
#endif


#endif     /* Close double-include protection _PULSARCROSSCORR_H */
