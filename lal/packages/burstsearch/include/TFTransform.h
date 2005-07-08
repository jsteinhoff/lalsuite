/********************************** <lalVerbatim file="TFTransformHV">
Author: Flanagan, E
$Id$
**************************************************** </lalVerbatim> */

#ifndef _TFTRANSFORM_H
#define _TFTRANSFORM_H

#include <lal/LALDatatypes.h>
#include <lal/Window.h>
#include <lal/RealFFT.h>
#include <lal/LALRCSID.h>

#ifdef  __cplusplus		/* C++ protection. */
extern "C" {
#endif

NRCSID(TFTRANSFORMH, "$Id$");


typedef struct tagTFPlaneParams {
	INT4 timeBins;	/* Number of time bins in TF plane    */
	INT4 freqBins;	/* Number of freq bins in TF plane    */
	REAL8 deltaT;	/* time resolution of the plane     */
	REAL8 deltaF;	/* freq. resolution of the plane */
	REAL8 flow;	/* minimum frequency to search for */
} TFPlaneParams;


typedef struct tagREAL4TimeFrequencyPlane {
	CHAR *name;
	LIGOTimeGPS epoch;
	CHARVector *sampleUnits;
	TFPlaneParams params;
	REAL4 *data;
	/*
	 * data[i*params->freqBins+j] is a real number
	 * corresponding to a time t_i = epoch + i*(deltaT)
	 * and a frequency f_j = flow + j / (deltaT)
	 */
} REAL4TimeFrequencyPlane;


int
XLALComputeFrequencySeries(
	COMPLEX8FrequencySeries *freqSeries,
	const REAL4TimeSeries *timeSeries,
	const REAL4Window *window,
	const REAL4FFTPlan *plan
);


REAL4TimeFrequencyPlane *
XLALCreateTFPlane(
	TFPlaneParams *params
);


void
XLALDestroyTFPlane(
	REAL4TimeFrequencyPlane *plane
);


int
XLALFreqSeriesToTFPlane(
	REAL4TimeFrequencyPlane *tfp,
	const COMPLEX8FrequencySeries *freqSeries,
	UINT4 windowShift,
	REAL4 *norm,
	const REAL4FrequencySeries *psd
);

#ifdef  __cplusplus
}
#endif				/* C++ protection. */
#endif
