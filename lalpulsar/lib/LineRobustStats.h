/*
 * Copyright (C) 2011-2014 David Keitel
 * Copyright (C) 2014 Reinhard Prix
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
 * \defgroup LineRobustStats_h Header LineRobustStats.h
 * \ingroup lalpulsar_LR
 * \author David Keitel, Reinhard Prix
 *
 * \brief Functions to compute line-robust CW statistics
 */
/** @{ */

#ifndef _LINEROBUSTSTATS_H  /* Double-include protection. */
#define _LINEROBUSTSTATS_H

/* C++ protection. */
#ifdef  __cplusplus
extern "C" {
#endif

/*---------- exported INCLUDES ----------*/

/* lal includes */
#include <lal/LALStdlib.h>
#include <lal/PulsarDataTypes.h>
#include <lal/StringVector.h>
#include <lal/LALConstants.h>
#include <math.h>

/* additional includes */
typedef struct tagBSGLSetup BSGLSetup;  ///< internal storage for setup and pre-computed BSGL quantities

/*---------- exported DEFINES ----------*/

/*---------- exported types ----------*/

/*---------- exported Global variables ----------*/

/*---------- exported prototypes [API] ----------*/

BSGLSetup *
XLALCreateBSGLSetup( const UINT4 numDetectors,
                     const REAL4 Fstar0sc,
                     const REAL4 oLGX[PULSAR_MAX_DETECTORS],
                     const BOOLEAN useLogCorrection,
                     const UINT4 numSegments
                   );

void
XLALDestroyBSGLSetup( BSGLSetup *setup );

int
XLALParseLinePriors( REAL4 oLGX[PULSAR_MAX_DETECTORS],
                     const LALStringVector *oLGX_string
                   );

// ---------- vector BSGL functions ----------
int
XLALVectorComputeBSGL( REAL4 *outBSGL,
                       const REAL4 *twoF,
                       const REAL4 *twoFPerDet[PULSAR_MAX_DETECTORS],
                       const UINT4 len,
                       const BSGLSetup *setup
                     );

int
XLALVectorComputeBSGLtL( REAL4 *outBSGLtL,
                         const REAL4 *twoF,
                         const REAL4 *twoFPerDet[PULSAR_MAX_DETECTORS],
                         const REAL4 *maxTwoFSegPerDet[PULSAR_MAX_DETECTORS],
                         const UINT4 len,
                         const BSGLSetup *setup
                       );
int
XLALVectorComputeBtSGLtL( REAL4 *outBtSGLtL,
                          const REAL4 *maxTwoFSeg,
                          const REAL4 *twoFPerDet[PULSAR_MAX_DETECTORS],
                          const REAL4 *maxTwoFSegPerDet[PULSAR_MAX_DETECTORS],
                          const UINT4 len,
                          const BSGLSetup *setup
                        );

// ---------- single-bin BSGL function wrappers ----------
REAL4
XLALComputeBSGL( const REAL4 twoF,
                 const REAL4 twoFX[PULSAR_MAX_DETECTORS],
                 const BSGLSetup *setup
               );

REAL4
XLALComputeBSGLtL( const REAL4 twoF,
                   const REAL4 twoFX[PULSAR_MAX_DETECTORS],
                   const REAL4 maxtwoFXl[PULSAR_MAX_DETECTORS],
                   const BSGLSetup *setup
                 );

REAL4
XLALComputeBtSGLtL( const REAL4 maxtwoFl,
                    const REAL4 twoFX[PULSAR_MAX_DETECTORS],
                    const REAL4 maxtwoFXl[PULSAR_MAX_DETECTORS],
                    const BSGLSetup *setup
                  );

REAL4
XLALComputeBStSGLtL( const REAL4 twoF,
                     const REAL4 maxtwoFl,
                     const REAL4 twoFX[PULSAR_MAX_DETECTORS],
                     const REAL4 maxtwoFXl[PULSAR_MAX_DETECTORS],
                     const BSGLSetup *setup
                   );


/** @} */

#ifdef  __cplusplus
}
#endif
/* C++ protection. */

#endif  /* Double-include protection. */
