/*
*  Copyright (C) 2014 Matthew Pitkin
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

/******************************************************************************/
/*                            HELPER FUNCTIONS                                */
/******************************************************************************/

#include "config.h"
#include "ppe_utils.h"
#include "ppe_models.h"

/** \brief Compute the noise variance for each data segment
 *
 * Once the data has been split into segments calculate the noise variance (using
 * both the real and imaginary parts) in each segment and fill in the associated
 * noise vector. To calculate the noise the running median should first be
 * subtracted from the data.
 *
 * \param data [in] the LALInferenceIFOData variable
 * \param model [in] the LALInferenceIFOModel variable
 */
void compute_variance( LALInferenceIFOData *data, LALInferenceIFOModel *model )
{
  REAL8 chunkLength = 0.;

  INT4 i = 0, j = 0, length = 0, cl = 0, counter = 0;

  COMPLEX16Vector *meddata = NULL; /* data with running median removed */

  /* subtract a running median value from the data to remove any underlying
     trends (e.g. caused by a strong signal) */
  meddata = subtract_running_median( data->compTimeData->data );

  UINT4Vector *chunkLengths = NULL;
  chunkLengths = *( UINT4Vector ** )LALInferenceGetVariable( model->params, "chunkLength" );

  length = data->compTimeData->data->length;

  for ( i = 0, counter = 0; i < length; i += chunkLength, counter++ ) {
    REAL8 vari = 0., meani = 0.;

    chunkLength = ( REAL8 )chunkLengths->data[counter];
    cl = i + ( INT4 )chunkLength;

    /* get the mean (should be close to zero given the running median subtraction), but
     * probably worth doing anyway) */
    for ( j = i ; j < cl ; j++ ) {
      meani += ( creal( meddata->data[j] ) + cimag( meddata->data[j] ) );
    }

    meani /= ( 2.*chunkLength );

    for ( j = i ; j < cl ; j++ ) {
      vari += SQUARE( ( creal( meddata->data[j] ) - meani ) );
      vari += SQUARE( ( cimag( meddata->data[j] ) - meani ) );
    }

    vari /= ( 2.*chunkLength - 1. ); /* unbiased sample variance */

    /* fill in variance vector */
    for ( j = i ; j < cl ; j++ ) {
      data->varTimeData->data->data[j] = vari;
    }
  }
}


/**
 * \brief Split the data into segments
 *
 * This function is deprecated to \c chop_n_merge, but gives the functionality of the old code.
 *
 * It cuts the data into as many contiguous segments of data as possible of length \c chunkMax. Where contiguous is
 * defined as containing consecutive point within 180 seconds of each other. The length of segments that do not fit into
 * a \c chunkMax length are also included.
 *
 * \param ifo [in] the LALInferenceIFOModel variable
 * \param chunkMax [in] the maximum length of a data chunk/segment
 *
 * \return A vector of chunk/segment lengths
 */
UINT4Vector *get_chunk_lengths( LALInferenceIFOModel *ifo, UINT4 chunkMax )
{
  UINT4 i = 0, j = 0, count = 0;
  UINT4 length;

  REAL8 t1, t2;

  UINT4Vector *chunkLengths = NULL;

  length = IFO_XTRA_DATA( ifo )->times->length;

  chunkLengths = XLALCreateUINT4Vector( length );

  REAL8 dt = *( REAL8 * )LALInferenceGetVariable( ifo->params, "dt" );

  /* create vector of data segment length */
  while ( 1 ) {
    count++; /* counter */

    /* break clause */
    if ( i > length - 2 ) {
      /* set final value of chunkLength */
      chunkLengths->data[j] = count;
      j++;
      break;
    }

    i++;

    t1 = XLALGPSGetREAL8( &IFO_XTRA_DATA( ifo )->times->data[i - 1] );
    t2 = XLALGPSGetREAL8( &IFO_XTRA_DATA( ifo )->times->data[i] );

    /* if consecutive points are within two sample times of each other count as in the same chunk */
    if ( t2 - t1 > 2.*dt || count == chunkMax ) {
      chunkLengths->data[j] = count;
      count = 0; /* reset counter */

      j++;
    }
  }

  chunkLengths = XLALResizeUINT4Vector( chunkLengths, j );

  return chunkLengths;
}


/* function to use change point analysis to chop up and remerge the data to find stationary chunks (i.e. lengths of data
 * which look like they have the same statistics e.g. the same standard deviation) */
/**
 * \brief Chops and remerges data into stationary segments
 *
 * This function finds segments of data that appear to be stationary (have the same standard deviation).
 *
 * The function first attempts to chop up the data into as many stationary segments as possible. The splitting may not
 * be optimal, so it then tries remerging consecutive segments to see if the merged segments show more evidence of
 * stationarity. <b>[NOTE: Remerging is currently turned off and will make very little difference to the algorithm]</b>.
 * It then, if necessary, chops the segments again to make sure there are none greater than the required \c chunkMax.
 * The default \c chunkMax is 0, so this rechopping will not normally happen.
 *
 * This is all performed on data that has had a running median subtracted, to try and removed any underlying trends in
 * the data (e.g. those caused by a strong signal), which might affect the calculations (which assume the data is
 * Gaussian with zero mean).
 *
 * If the \c outputchunks is non-zero then a list of the segments will be output to a file called \c data_segment_list.txt,
 * with a prefix of the detector name.
 *
 * \param data [in] A data structure
 * \param chunkMin [in] The minimum length of a segment
 * \param chunkMax [in] The maximum length of a segment
 * \param outputchunks [in] A flag to check whether to output the segments
 *
 * \return A vector of segment/chunk lengths
 *
 * \sa subtract_running_median
 * \sa chop_data
 * \sa merge_data
 * \sa rechop_data
 */
UINT4Vector *chop_n_merge( LALInferenceIFOData *data, UINT4 chunkMin, UINT4 chunkMax, UINT4 outputchunks )
{
  UINT4 j = 0;

  UINT4Vector *chunkLengths = NULL;
  UINT4Vector *chunkIndex = NULL;

  COMPLEX16Vector *meddata = NULL;

  /* subtract a running median value from the data to remove any underlying trends (e.g. caused by a string signal) that
   * might affect the chunk calculations (which can assume the data is Gaussian with zero mean). */
  meddata = subtract_running_median( data->compTimeData->data );

  /* pass chop data a gsl_vector_view, so that internally it can use vector views rather than having to create new vectors */
  gsl_vector_complex_view meddatagsl = gsl_vector_complex_view_array( ( double * )meddata->data, meddata->length );
  chunkIndex = chop_data( &meddatagsl.vector, chunkMin );

  /* DON'T BOTHER WITH THE MERGING AS IT WILL MAKE VERY LITTLE DIFFERENCE */
  /* merge_data( meddata, &chunkIndex ); */

  XLALDestroyCOMPLEX16Vector( meddata ); /* free memory */

  /* if a maximum chunk length is defined then rechop up the data, to segment any chunks longer than this value */
  if ( chunkMax > chunkMin ) {
    rechop_data( &chunkIndex, chunkMax, chunkMin );
  }

  chunkLengths = XLALCreateUINT4Vector( chunkIndex->length );

  /* go through segments and turn into vector of chunk lengths */
  for ( j = 0; j < chunkIndex->length; j++ ) {
    if ( j == 0 ) {
      chunkLengths->data[j] = chunkIndex->data[j];
    } else {
      chunkLengths->data[j] = chunkIndex->data[j] - chunkIndex->data[j - 1];
    }
  }

  /* if required output the chunk end indices and lengths to a file */
  if ( outputchunks ) {
    FILE *fpsegs = NULL;

    /* set detector name as prefix */
    CHAR *outfile = NULL;
    outfile = XLALStringDuplicate( data->detector->frDetector.prefix );
    outfile = XLALStringAppend( outfile, "data_segment_list.txt" );

    /* check if file exists, i.e. given mutliple frequency harmonics, and if so open for appending */
    FILE *fpcheck = NULL;
    if ( ( fpcheck = fopen( outfile, "r" ) ) != NULL ) {
      fclose( fpcheck );
      if ( ( fpsegs = fopen( outfile, "a" ) ) == NULL ) {
        fprintf( stderr, "Non-fatal error open file to output segment list.\n" );
        return chunkLengths;
      }
    } else {
      if ( ( fpsegs = fopen( outfile, "w" ) ) == NULL ) {
        fprintf( stderr, "Non-fatal error open file to output segment list.\n" );
        return chunkLengths;
      }
    }
    XLALFree( outfile );

    for ( j = 0; j < chunkIndex->length; j++ ) {
      fprintf( fpsegs, "%u\t%u\n", chunkIndex->data[j], chunkLengths->data[j] );
    }

    /* add space at the end so that you can separate lists from different detector data streams */
    fprintf( fpsegs, "\n" );

    fclose( fpsegs );
  }

  XLALDestroyUINT4Vector( chunkIndex );

  return chunkLengths;
}


/**
 * \brief Subtract the running median from complex data
 *
 * This function uses \c gsl_stats_median_from_sorted_data to subtract a running median, calculated from the 30
 * consecutive point around a set point, from the data. At the start of the data running median is calculated from
 * 30-15+(i-1) points, and at the end it is calculated from 15+(N-i) points, where i is the point index and N is the
 * total number of data points.
 *
 * \param data [in] A complex data vector
 *
 * \return A complex vector containing data with the running median removed
 */
COMPLEX16Vector *subtract_running_median( COMPLEX16Vector *data )
{
  COMPLEX16Vector *submed = NULL;
  UINT4 length = data->length, i = 0, j = 0, n = 0;
  UINT4 mrange = 0;
  UINT4 N = 0;
  INT4 sidx = 0;

  if ( length > 30 ) {
    mrange = 30;  /* perform running median with 30 data points */
  } else {
    mrange = 2 * floor( ( length - 1 ) / 2 );  /* an even number less than length */
  }
  N = ( UINT4 )floor( mrange / 2 );

  submed = XLALCreateCOMPLEX16Vector( length );

  for ( i = 1; i < length + 1; i++ ) {
    REAL8 *dre = NULL;
    REAL8 *dim = NULL;

    /* get median of data within RANGE */
    if ( i < N ) {
      n = N + i;
      sidx = 0;
    } else {
      if ( i > length - N ) {
        n = length - i + N;
      } else {
        n = mrange;
      }

      sidx = i - N;
    }

    dre = XLALCalloc( n, sizeof( REAL8 ) );
    dim = XLALCalloc( n, sizeof( REAL8 ) );

    for ( j = 0; j < n; j++ ) {
      dre[j] = creal( data->data[j + sidx] );
      dim[j] = cimag( data->data[j + sidx] );
    }

    /* sort data */
    gsl_sort( dre, 1, n );
    gsl_sort( dim, 1, n );

    /* get median and subtract from data*/
    submed->data[i - 1] = ( creal( data->data[i - 1] ) - gsl_stats_median_from_sorted_data( dre, 1, n ) )
                          + I * ( cimag( data->data[i - 1] ) - gsl_stats_median_from_sorted_data( dim, 1, n ) );

    XLALFree( dre );
    XLALFree( dim );
  }

  return submed;
}


/**
 * \brief Chops the data into stationary segments based on Bayesian change point analysis
 *
 * This function splits data into two (and recursively runs on those two segments) if it is found that the odds ratio
 * for them being from two independent Gaussian distributions is greater than a certain threshold.
 *
 * The threshold for the natural logarithm of the odds ratio is empirically set to be
 * \f[
 * T = 4.07 + 1.33\log{}_{10}{N},
 * \f]
 * where \f$ N \f$ is the length in samples of the dataset. This is based on Monte Carlo simulations of
 * many realisations of Gaussian noise for data of different lengths. The threshold comes from a linear
 * fit to the log odds ratios required to give a 1% chance of splitting Gaussian data (drawn from a single
 * distribution) for data of various lengths.  Note, however, that this relation is not good for stretches of data
 * with lengths of less than about 30 points, and in fact is rather consevative for such short stretches
 * of data, i.e. such short stretches of data will require relatively larger odds ratios for splitting than
 * longer stretches.
 *
 * \param data [in] A complex data vector
 * \param chunkMin [in] The minimum allowed segment length
 *
 * \return A vector of segment lengths
 *
 * \sa find_change_point
 */
UINT4Vector *chop_data( gsl_vector_complex *data, UINT4 chunkMin )
{
  UINT4Vector *chunkIndex = NULL;

  UINT4 length = ( UINT4 )data->size;

  REAL8 logodds = 0.;
  UINT4 changepoint = 0;

  REAL8 threshold = 0.; /* may need tuning or setting globally */

  chunkIndex = XLALCreateUINT4Vector( 1 );

  changepoint = find_change_point( data, &logodds, chunkMin );

  /* threshold scaling for a 0.5% false alarm probability of splitting Gaussian data */
  threshold = 4.07 + 1.33 * log10( ( REAL8 )length );

  if ( logodds > threshold ) {
    UINT4Vector *cp1 = NULL;
    UINT4Vector *cp2 = NULL;

    gsl_vector_complex_view data1 = gsl_vector_complex_subvector( data, 0, changepoint );
    gsl_vector_complex_view data2 = gsl_vector_complex_subvector( data, changepoint, length - changepoint );

    UINT4 i = 0, l = 0;

    cp1 = chop_data( &data1.vector, chunkMin );
    cp2 = chop_data( &data2.vector, chunkMin );

    l = cp1->length + cp2->length;

    chunkIndex = XLALResizeUINT4Vector( chunkIndex, l );

    /* combine new chunks */
    for ( i = 0; i < cp1->length; i++ ) {
      chunkIndex->data[i] = cp1->data[i];
    }
    for ( i = 0; i < cp2->length; i++ ) {
      chunkIndex->data[i + cp1->length] = cp2->data[i] + changepoint;
    }

    XLALDestroyUINT4Vector( cp1 );
    XLALDestroyUINT4Vector( cp2 );
  } else {
    chunkIndex->data[0] = length;
  }

  return chunkIndex;
}


/**
 * \brief Find a change point in complex data
 *
 * This function is based in the Bayesian Blocks algorithm of \cite Scargle1998 that finds "change points" in data -
 * points at which the statistics of the data change. It is based on calculating evidence, or odds, ratios. The
 * function first computes the marginal likelihood (or evidence) that the whole of the data is described by a single
 * Gaussian (with mean of zero). This comes from taking a Gaussian likelihood function and analytically marginalising
 * over the standard deviation (using a prior on the standard deviation of \f$ 1/\sigma \f$ ), giving (see
 * [\cite DupuisWoan2005]) a Students-t distribution (see
 * <a href="https://wiki.ligo.org/foswiki/pub/CW/PulsarParameterEstimationNestedSampling/studentst.pdf">here</a>).
 * Following this the data is split into two segments (with lengths greater than, or equal to the minimum chunk length)
 * for all possible combinations, and the joint evidence for each of the two segments consisting of independent
 * Gaussian (basically multiplying the above equation calculated for each segment separately) is calculated and the
 * split point recorded. However, the value required for comparing to that for the whole data set, to give the odds
 * ratio, is the evidence that having any split is better than having no split, so the individual split data evidences
 * need to be added incoherently to give the total evidence for a split. The index at which the evidence for a single
 * split is maximum (i.e. the most favoured split point) is that which is returned.
 *
 * \param data [in] a complex data vector
 * \param logodds [in] a pointer to return the natural logarithm of the odds ratio/Bayes factor
 * \param minlength [in] the minimum chunk length
 *
 * \return The position of the change point
 */
UINT4 find_change_point( gsl_vector_complex *data, REAL8 *logodds, UINT4 minlength )
{
  UINT4 changepoint = 0, i = 0;
  UINT4 length = ( UINT4 )data->size, lsum = 0;

  REAL8 datasum = 0.;

  REAL8 logsingle = 0., logtot = -INFINITY;
  REAL8 logdouble = 0., logdouble_min = -INFINITY;
  REAL8 logratio = 0.;

  REAL8 sumforward = 0., sumback = 0.;
  gsl_complex dval;

  /* check that data is at least twice the minimum length, if not return an odds ratio of zero (log odds = -inf [or close to that!]) */
  if ( length < ( UINT4 )( 2 * minlength ) ) {
    logratio = -INFINITY;
    memcpy( logodds, &logratio, sizeof( REAL8 ) );
    return 0;
  }

  /* calculate the sum of the data squared */
  for ( i = 0; i < length; i++ ) {
    dval = gsl_vector_complex_get( data, i );
    datasum += SQUARE( gsl_complex_abs( dval ) );
  }

  /* calculate the evidence that the data consists of a Gaussian data with a single standard deviation */
  logsingle = -LAL_LN2 - ( REAL8 )length * LAL_LNPI + gsl_sf_lnfact( length - 1 ) - ( REAL8 )length * log( datasum );

  lsum = length - 2 * minlength + 1;

  for ( i = 0; i < length; i++ ) {
    dval = gsl_vector_complex_get( data, i );
    if ( i < minlength - 1 ) {
      sumforward += SQUARE( gsl_complex_abs( dval ) );
    } else {
      sumback += SQUARE( gsl_complex_abs( dval ) );
    }
  }

  /* go through each possible change point and calculate the evidence for the data consisting of two independent
   * Gaussian's either side of the change point. Also calculate the total evidence for any change point.
   * Don't allow single points, so start at the second data point. */
  for ( i = 0; i < lsum; i++ ) {
    UINT4 ln1 = i + minlength, ln2 = ( length - i - minlength );

    REAL8 log_1 = 0., log_2 = 0.;

    dval = gsl_vector_complex_get( data, ln1 - 1 );
    REAL8 adval = SQUARE( gsl_complex_abs( dval ) );
    sumforward += adval;
    sumback -= adval;

    /* get log evidences for the individual segments */
    log_1 = -LAL_LN2 - ( REAL8 )ln1 * LAL_LNPI + gsl_sf_lnfact( ln1 - 1 ) - ( REAL8 )ln1 * log( sumforward );
    log_2 = -LAL_LN2 - ( REAL8 )ln2 * LAL_LNPI + gsl_sf_lnfact( ln2 - 1 ) - ( REAL8 )ln2 * log( sumback );

    /* get evidence for the two segments */
    logdouble = log_1 + log_2;

    /* add to total evidence for a change point */
    logtot = LOGPLUS( logtot, logdouble );

    /* find maximum value of logdouble and record that as the change point */
    if ( logdouble > logdouble_min ) {
      changepoint = ln1;
      logdouble_min = logdouble;
    }
  }

  /* get the log odds ratio of segmented versus non-segmented model */
  logratio = logtot - logsingle;
  memcpy( logodds, &logratio, sizeof( REAL8 ) );

  return changepoint;
}


/**
 * \brief Chop up the data into chunks smaller the the maximum allowed length
 *
 * This function chops any chunks that are greater than \c chunkMax into chunks smaller than, or equal to \c chunkMax,
 * and greater than \c chunkMin. On some occasions this might result in a segment smaller than \c chunkMin, but these
 * are ignored in the likelihood calculation anyway.
 *
 * \param chunkIndex [in] a vector of segment split positions
 * \param chunkMax [in] the maximum allowed segment/chunk length
 * \param chunkMin [in] the minimum allowed segment/chunk length
 */
void rechop_data( UINT4Vector **chunkIndex, UINT4 chunkMax, UINT4 chunkMin )
{
  UINT4 i = 0, j = 0, count = 0;
  UINT4Vector *cip = *chunkIndex; /* pointer to chunkIndex */
  UINT4 length = cip->length;
  UINT4 endIndex = cip->data[length - 1];
  UINT4 startindex = 0, chunklength = 0;
  UINT4 newindex[( UINT4 )ceil( ( REAL8 )endIndex / ( REAL8 )chunkMin )];

  /* chop any chunks that are greater than chunkMax into chunks smaller than, or equal to chunkMax, and greater than chunkMin */
  for ( i = 0; i < length; i++ ) {
    if ( i == 0 ) {
      startindex = 0;
    } else {
      startindex = cip->data[i - 1];
    }

    chunklength = cip->data[i] - startindex;

    if ( chunklength > chunkMax ) {
      UINT4 remain = chunklength % chunkMax;

      /* cut segment into as many chunkMax chunks as possible */
      for ( j = 0; j < floor( chunklength / chunkMax ); j++ ) {
        newindex[count] = startindex + ( j + 1 ) * chunkMax;
        count++;
      }

      /* last chunk values */
      if ( remain != 0 ) {
        /* set final value */
        newindex[count] = startindex + j * chunkMax + remain;

        if ( remain < chunkMin ) {
          /* split the last two cells into one that is chunkMin long and one that is (chunkMax+remainder)-chunkMin long
           * - this may leave a cell shorter than chunkMin, but we'll have to live with that! */
          UINT4 n1 = ( chunkMax + remain ) - chunkMin;

          /* reset second to last value two values */
          newindex[count - 1] = newindex[count] - chunkMin;

          if ( n1 < chunkMin ) {
            XLAL_PRINT_WARNING( "Non-fatal error... segment no. %d is %d long, which is less than chunkMin = %d.\n", count, n1, chunkMin );
          }
        }

        count++;
      }
    } else {
      newindex[count] = cip->data[i];
      count++;
    }
  }

  cip = XLALResizeUINT4Vector( cip, count );

  for ( i = 0; i < count; i++ ) {
    cip->data[i] = newindex[i];
  }
}


/**
 * \brief Merge adjacent segments
 *
 * This function will attempt to remerge adjacent segments if statistically favourable (as calculated by the odds
 * ratio). For each pair of adjacent segments the joint likelihood of them being from two independent distributions is
 * compared to the likelihood that combined they are from one distribution. If the likelihood is highest for the
 * combined segments they are merged.
 *
 * \param data [in] A complex data vector
 * \param segments [in] A vector of split segment indexes
 */
void merge_data( COMPLEX16Vector *data, UINT4Vector **segments )
{
  UINT4 j = 0;
  REAL8 threshold = 0.; /* may need to be passed to function in the future, or defined globally */
  UINT4Vector *segs = *segments;

  /* loop until stopping criterion is reached */
  while ( 1 ) {
    UINT4 ncells = segs->length;

    UINT4 mergepoint = 0;
    REAL8 logodds = 0., minl = -LAL_REAL8_MAX;

    for ( j = 1; j < ncells; j++ ) {
      REAL8 summerged = 0., sum1 = 0., sum2 = 0.;
      UINT4 i = 0, n1 = 0, n2 = 0, nm = 0;
      UINT4 cellstarts1 = 0, cellends1 = 0, cellstarts2 = 0, cellends2 = 0;
      REAL8 log_merged = 0., log_individual = 0.;

      /* get the evidence for merged adjacent cells */
      if ( j == 1 ) {
        cellstarts1 = 0;
      } else {
        cellstarts1 = segs->data[j - 2];
      }

      cellends1 = segs->data[j - 1];

      cellstarts2 = segs->data[j - 1];
      cellends2 = segs->data[j];

      n1 = cellends1 - cellstarts1;
      n2 = cellends2 - cellstarts2;
      nm = cellends2 - cellstarts1;

      for ( i = cellstarts1; i < cellends1; i++ ) {
        sum1 += SQUARE( cabs( data->data[i] ) );
      }

      for ( i = cellstarts2; i < cellends2; i++ ) {
        sum2 += SQUARE( cabs( data->data[i] ) );
      }

      summerged = sum1 + sum2;

      /* calculated evidences */
      log_merged = -2 + gsl_sf_lnfact( nm - 1 ) - ( REAL8 )nm * log( summerged );

      log_individual = -2 + gsl_sf_lnfact( n1 - 1 ) - ( REAL8 )n1 * log( sum1 );
      log_individual += -2 + gsl_sf_lnfact( n2 - 1 ) - ( REAL8 )n2 * log( sum2 );

      logodds = log_merged - log_individual;

      if ( logodds > minl ) {
        mergepoint = j - 1;
        minl = logodds;
      }
    }

    /* set break criterion */
    if ( minl < threshold ) {
      break;
    } else { /* merge cells */
      /* remove the cell end value between the two being merged and shift */
      for ( UINT4 i = 0; i < ncells - ( mergepoint + 1 ); i++ ) {
        segs->data[mergepoint + i] = segs->data[mergepoint + i + 1];
      }

      segs = XLALResizeUINT4Vector( segs, ncells - 1 );
    }
  }
}


/**
 * \brief Counts the number of comma separated values in a string
 *
 * This function counts the number of comma separated values in a given input string.
 *
 * \param csvline [in] Any string
 *
 * \return The number of comma separated value in the input string
 */
INT4 count_csv( CHAR *csvline )
{
  CHAR *inputstr = NULL;
  INT4 count = 0;

  inputstr = XLALStringDuplicate( csvline );

  /* count number of commas */
  while ( 1 ) {
    if ( XLALStringToken( &inputstr, ",", 0 ) == NULL ) {
      XLAL_ERROR( XLAL_EFUNC, "Error... problem counting number of commas!" );
    }

    if ( inputstr == NULL ) {
      break;
    }

    count++;
  }

  XLALFree( inputstr );

  return count + 1;
}


/**
 * \brief Automatically set the solar system ephemeris file based on environment variables and data time span
 *
 * This function will attempt to construct the file name for Sun, Earth and time correction ephemeris files
 * based on the ephemeris used for the equivalent TEMPO(2) pulsar timing information. It assumes that the
 * ephemeris files are those constructed between 2000 and 2020. The path to the file is not required as this
 * will be found in \c XLALInitBarycenter.
 *
 * \param efile [in] a string that will return the Earth ephemeris file
 * \param sfile [in] a string that will return the Sun ephemeris file
 * \param tfile [in] a string that will return the time correction file
 * \param pulsar [in] the pulsar parameters read from a .par file
 * \param gpsstart [in] the GPS time of the start of the data
 * \param gpsend [in] the GPS time of the end of the data
 *
 * \return The TimeCorrectionType e.g. TDB or TCB
 */
TimeCorrectionType XLALAutoSetEphemerisFiles( CHAR **efile, CHAR **sfile, CHAR **tfile, PulsarParameters *pulsar,
    INT4 gpsstart, INT4 gpsend )
{
  /* set the times that the ephemeris files span */
  INT4 ephemstart = 630720013; /* GPS time of Jan 1, 2000, 00:00:00 UTC */
  INT4 ephemend = 1893024018; /* GPS time of Jan 1, 2040, 00:00:00 UTC */
  TimeCorrectionType ttype = TIMECORRECTION_NONE;

  if ( gpsstart < ephemstart || gpsend < ephemstart || gpsstart > ephemend || gpsend > ephemend ) {
    XLAL_ERROR( XLAL_EFUNC, "Start and end times are outside the ephemeris file ranges!" );
  }

  *efile = XLALStringDuplicate( "earth00-40-" );
  *sfile = XLALStringDuplicate( "sun00-40-" );

  if ( !PulsarCheckParam( pulsar, "EPHEM" ) ) {
    /* default to use DE405 */
    *efile = XLALStringAppend( *efile, "DE405" );
    *sfile = XLALStringAppend( *sfile, "DE405" );
  } else {
    if ( !strcmp( PulsarGetStringParam( pulsar, "EPHEM" ), "DE405" ) || !strcmp( PulsarGetStringParam( pulsar, "EPHEM" ), "DE200" ) ||
         !strcmp( PulsarGetStringParam( pulsar, "EPHEM" ), "DE414" ) || !strcmp( PulsarGetStringParam( pulsar, "EPHEM" ), "DE421" ) ||
         !strcmp( PulsarGetStringParam( pulsar, "EPHEM" ), "DE430" ) || !strcmp( PulsarGetStringParam( pulsar, "EPHEM" ), "DE436" ) ) {
      *efile = XLALStringAppend( *efile, PulsarGetStringParam( pulsar, "EPHEM" ) );
      *sfile = XLALStringAppend( *sfile, PulsarGetStringParam( pulsar, "EPHEM" ) );
    } else {
      XLAL_ERROR( XLAL_EFUNC, "Unknown ephemeris %s in par file.", PulsarGetStringParam( pulsar, "EPHEM" ) );
    }
  }

  /* add .dat.gz extension */
  *efile = XLALStringAppend( *efile, ".dat.gz" );
  *sfile = XLALStringAppend( *sfile, ".dat.gz" );

  if ( !PulsarCheckParam( pulsar, "UNITS" ) ) {
    /* default to using TCB units */
    *tfile = XLALStringDuplicate( "te405_2000-2040.dat.gz" );
    ttype = TIMECORRECTION_TCB;
  } else {
    if ( !strcmp( PulsarGetStringParam( pulsar, "UNITS" ), "TDB" ) ) {
      *tfile = XLALStringDuplicate( "tdb_2000-2040.dat.gz" );
      ttype = TIMECORRECTION_TDB;
    } else if ( !strcmp( PulsarGetStringParam( pulsar, "UNITS" ), "TCB" ) ) {
      *tfile = XLALStringDuplicate( "te405_2000-2040.dat.gz" );
      ttype = TIMECORRECTION_TCB;
    } else {
      XLAL_ERROR( XLAL_EFUNC, "Error... unknown units %s in par file!", PulsarGetStringParam( pulsar, "UNITS" ) );
    }
  }

  return ttype;
}

/**
 * \brief Add a variable, checking first if it already exists and is of type \c LALINFERENCE_PARAM_FIXED
 * and if so removing it before re-adding it
 *
 * This function is for use as an alternative to \c LALInferenceAddVariable, which does not
 * allow \c LALINFERENCE_PARAM_FIXED variables to be changed. If the variable already exists
 * and is of type  \c LALINFERENCE_PARAM_FIXED, then it will first be removed and then re-added
 */
void check_and_add_fixed_variable( LALInferenceVariables *vars, const char *name, void *value, LALInferenceVariableType type )
{
  if ( LALInferenceCheckVariable( vars, name ) ) {
    if ( LALInferenceGetVariableVaryType( vars, name ) == LALINFERENCE_PARAM_FIXED ) {
      LALInferenceRemoveVariable( vars, name );
    }
  }
  LALInferenceAddVariable( vars, name, value, type, LALINFERENCE_PARAM_FIXED );
}
