/*-----------------------------------------------------------------------
 *
 * File Name: SFTfileIO.c
 *
 * Authors: Sintes, A.M.,  Prix, R., Krishnan, B. Machenschalk, B.
 *          inspired from Siemens, X.
 *
 * Revision: $Id$
 *
 * History:   Created by Sintes & Prix May 21, 2003
 *            Modified by Krishnan on Feb 22, 2004
 *            Modified by Machenschalk on Jun 16 2004
 *
 *-----------------------------------------------------------------------
 */

/************************************ <lalVerbatim file="SFTfileIOCV">
Author: Sintes, A.M., Krishnan, B., Prix, R., Machenschalk, B.
$Id$
************************************* </lalVerbatim> */

/* <lalLaTeX>  *******************************************************
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\subsection{Module \texttt{SFTfileIO.c}}
\label{ss:SFTfileIO.c}
Routines for reading and writing SFT binary files


\subsubsection*{Prototypes}
\idx{LALReadSFTheader()}
\idx{LALReadSFTdata()}
\idx{LALReadSFTfile()}
\idx{LALReadSFTfiles()}
\idx{LALWriteSFTfile()}

\input{SFTfileIOCP}

\subsubsection*{Description}

\begin{description}

\item{LALReadSFTfile():} basic SFT reading-function. Given filename
(\verb+fname+) and frequency-limits (\verb+fMin, fMax+), returns the
SFTtype containing the data.

\textbf{Note 1:} the \emph{actual} returned frequency-band is
\verb+[floor(fMin), ceil(fMax)]+, i.e. the requested
frequency-band is guaranteed to be contained in the output (if present
in the SFT-file), but can be slightly larger.

\textbf{Note 2:} The special input \verb+fMin=fMax=0+ means to read and
return the \emph{whole} frequency-band contained in the SFT-file.

\item{LALReadSFTfiles():} higher-level SFT-reading function to read a
list of SFT files and return an \verb+SFTvector+. The handling of
\verb+fMin, fMax+ is identical to \verb+LALReadSFTfile+.

\textbf{Note 1:} currently the argument of \verb+globdir+ is interpreted
a bit unconventionally, namely if you pass \verb+"path1/subdir/pattern"+,
this will be matched \verb+"path1/subdir/+\verb+*pattern*"+. This might be
changed in the near future to require you to specify the file-pattern
explicitly. 

\textbf{Note 2:} currently the SFTs matching the pattern are required
to have the same number of frequency bins, otherwise an error will be
returned. (This might be relaxed in the future).

\item{LALWriteSFTfile():} given an SFT (\verb+SFTtype *sft+), write it
into a file (\verb+CHAR *outfname+). 

\item{LALReadSFTheader():} lower-level function to read only the
SFT-header of a given file.

\item{LALReadSFTdata():} lower-level function to read an SFT-file. The
two main differences to \verb+LALReadSFTfile()+ are: 1) the memory for
SFTtype has to be allocated already with the number of frequency bins
to be read, and 2) one specifies the first frequency-index to be read,
instead of the physical frequencies.

\item{LALCopySFT():} copy an SFT-struct and its data from \verb+src+
to \verb+dest+. Note that the destination SFTtype has to be allocated
already and has to have the same size (number of frequency-bins) as 
\verb+src+.

\end{description}

\subsubsection*{Algorithm}

\subsubsection*{Uses}

\begin{verbatim}
LALCreateSFTtype	LALDestroySFTtype
LALCreateSFTVector	LALDestroySFTVector
LALOpenDataFile		LALPrintError
LALMalloc		LALCalloc
LALRealloc		LALFree

\end{verbatim}

\subsubsection*{Notes}

The current library only supports SFT-files conforming to the
SFT-version v1.0. The future API should mostly remain the same when
upgrading to v2.0 (impending), but minor changes/additions should be
expected. 

\vfill{\footnotesize\input{SFTfileIOCV}}

*********************************************** </lalLaTeX> */
#include <sys/types.h>
#ifndef _MSC_VER
#include <dirent.h>
#else
#include <io.h>
#endif

#include <lal/FileIO.h>
#include <lal/SFTfileIO.h>

NRCSID (SFTFILEIOC, "$Id$");

/* for private use: a vector of 'strings' */
typedef struct {
  UINT4 length;
  CHAR **data;
} StringVector;

/* prototypes for internal helper functions */
static StringVector *find_files (const CHAR *fpattern);
static void DestroyStringVector (StringVector *strings);
static void LALCopySFT (LALStatus *stat, SFTtype *dest, const SFTtype *src);
static void endian_swap(CHAR * pdata, size_t dsize, size_t nelements);
static void SortStringVector (StringVector *strings);

#if 0 /* REMOVED */
static void write_timeSeriesR4 (FILE *fp, const REAL4TimeSeries *series);
static void write_timeSeriesR8 (FILE *fp, const REAL8TimeSeries *series);
static void LALwriteSFTtoXMGR (LALStatus *stat, const SFTtype *sft, const CHAR *fname);
#endif /* REMOVED */

static int amatch(char *str, char *p);	/* glob pattern-matcher (public domain)*/

/* number of bytes in SFT-header v1.0 */
static const size_t header_len_v1 = 32;	



/***********************************************************************
 * The functions that make up the guts of this module
 ***********************************************************************/


/*----------------------------------------------------------------------
 * Read a given frequency-range from an SFT-file into a memory struct SFTtype
 *
 * NOTE: currently only SFT-spec v1.0 is supported! 
 *
 * NOTE2: this is a convenience wrapper for LALReadSFTheader() and LALReadSFTdata()
 *
 *----------------------------------------------------------------------*/
/* <lalVerbatim file="SFTfileIOCP"> */
void
LALReadSFTfile (LALStatus *stat, 
		SFTtype **sft, 		/* output SFT */
		REAL8 fMin, 		/* lower frequency-limit */
		REAL8 fMax,		/* upper frequency-limit */
		const CHAR *fname)	/* path+filename */
{ /* </lalVerbatim> */
  SFTHeader  header;		/* SFT file-header version1 */
  REAL8 deltaF;
  UINT4 readlen;
  INT4 fminBinIndex, fmaxBinIndex;
  SFTtype *outputSFT = NULL;
  UINT4 i;
  REAL4 renorm;

  INITSTATUS (stat, "LALReadSFTfile", SFTFILEIOC);
  ATTATCHSTATUSPTR (stat); 
  
  ASSERT (sft, stat, SFTFILEIOH_ENULL,  SFTFILEIOH_MSGENULL);
  ASSERT (*sft == NULL, stat, SFTFILEIOH_ENONULL, SFTFILEIOH_MSGENONULL);
  ASSERT (fname,  stat, SFTFILEIOH_ENULL,  SFTFILEIOH_MSGENULL);
  ASSERT (fMin <= fMax, stat, SFTFILEIOH_EVAL, SFTFILEIOH_MSGEVAL);

  /* read the header */
  TRY ( LALReadSFTheader (stat->statusPtr, &header, fname), stat);

  /* ----- figure out which data we want to read ----- */
  deltaF = 1.0 / header.timeBase;

  /* special case: fMin==fMax==0 means "read all" */
  if ( (fMin == 0) && (fMax == 0) )
    {
      fminBinIndex = header.fminBinIndex;
      fmaxBinIndex = fminBinIndex + header.length - 1;
    }
  else
    {
      /* find the right frequency-bin and number of bins
       * The rounding here is chosen such that the required 
       * frequency-interval is _guaranteed_ to lie within the 
       * returned range  */
      fminBinIndex = (INT4) floor (fMin * header.timeBase);  /* round this down */
      fmaxBinIndex = (INT4) ceil  (fMax * header.timeBase);  /* round up */
    }

  readlen = (UINT4)(fmaxBinIndex - fminBinIndex) + 1;	/* number of bins to read */

  /* allocate the final SFT to be returned */
  TRY ( LALCreateSFTtype (stat->statusPtr, &outputSFT, readlen), stat);


  /* and read it, using the lower-level function: */
  LALReadSFTdata (stat->statusPtr, outputSFT, fname, fminBinIndex);
  BEGINFAIL (stat) {
    LALDestroySFTtype (stat->statusPtr, &outputSFT);
  } ENDFAIL (stat);


  /***************************************************
   * FIXME: questionable renormalization follows... to be clarified
   ***************************************************/
  renorm = 1.0 * readlen / header.length;
  /* **************************************************/

  /* let's re-normalize and fill data into output-vector */
  if (renorm != 1)
    for (i=0; i < readlen; i++)
      {
	outputSFT->data->data[i].re *= renorm;
	outputSFT->data->data[i].im *= renorm;
      }    
  
  /* that's it: return */
  *sft = outputSFT;

  DETATCHSTATUSPTR (stat);
  RETURN(stat);

} /* LALReadSFTfile() */



/** Read a bunch of SFTs matching a given glob-like file-pattern.
 *
 * NOTE: this does not depend on glob, so it should be safe under condor.
 */
/* <lalVerbatim file="SFTfileIOCP"> */
void
LALReadSFTfiles (LALStatus *stat,
		 SFTVector **sftvect,	/**< output SFT vector */
		 REAL8 fMin,	       	/**< lower frequency-limit */
		 REAL8 fMax,		/**< upper frequency-limit */
		 UINT4 wingBins,	/**< number of frequency-bins to be added left and right. */
		 const CHAR *fpattern)	/**< path/filepattern */
{ /* </lalVerbatim> */

  UINT4 i, numSFTs;
  SFTVector *out = NULL;
  SFTtype *oneSFT = NULL;
  SFTHeader header;
  REAL8 dFreq; 		/**< frequency spacing in SFT */
  REAL8 fWing;		/**< frequency band to be added as "wings" to the 'physical band' */
  StringVector *fnames;

  INITSTATUS (stat, "LALReadSFTfiles", SFTFILEIOC);
  ATTATCHSTATUSPTR (stat); 
  
  ASSERT (sftvect, stat, SFTFILEIOH_ENULL,  SFTFILEIOH_MSGENULL);
  ASSERT (*sftvect == NULL, stat, SFTFILEIOH_ENONULL, SFTFILEIOH_MSGENONULL);
  ASSERT (fpattern,  stat, SFTFILEIOH_ENULL,  SFTFILEIOH_MSGENULL);
  ASSERT (fMin <= fMax, stat, SFTFILEIOH_EVAL, SFTFILEIOH_MSGEVAL);

  /* make filelist 
   * NOTE: we don't use glob() as it was reported to fail under condor */
  if ( (fnames = find_files (fpattern)) == NULL) {
    ABORT (stat, SFTFILEIOH_EGLOB, SFTFILEIOH_MSGEGLOB);
  }

  /* read header of first sft to determine Tsft, and therefore dfreq */
  LALReadSFTheader(stat->statusPtr, &header, fnames->data[0]);
  BEGINFAIL(stat) {
    DestroyStringVector (fnames);    
  } ENDFAIL(stat);

  numSFTs = fnames->length;
  dFreq = 1.0 / header.timeBase;
  fWing = wingBins * dFreq;

  /* main loop: load all SFTs and put them into the SFTvector */
  for (i=0; i < numSFTs; i++)
    {
      LALReadSFTfile (stat->statusPtr, &oneSFT, fMin-fWing, fMax+fWing, fnames->data[i]);
      BEGINFAIL (stat) {
	DestroyStringVector (fnames);    
	LALDestroySFTtype (stat->statusPtr, &oneSFT);
	if (out) LALDestroySFTVector (stat->statusPtr, &out);
      } ENDFAIL (stat);

      /* after the first time we can allocate the output-vector, 
       * as we now kpnow how many frequency-bins we need */
      if (out == NULL)
	{
	  LALCreateSFTVector(stat->statusPtr, &out, numSFTs, oneSFT->data->length);
	  BEGINFAIL(stat) {
	    DestroyStringVector (fnames);    
	    LALDestroySFTtype (stat->statusPtr, &oneSFT);
	  } ENDFAIL(stat);
	}

      /* make sure all SFTs have same length */
      if ( oneSFT->data->length != out->data->data->length)
	{
	  DestroyStringVector (fnames);    
	  LALDestroySFTtype (stat->statusPtr, &oneSFT);
	  LALDestroySFTVector (stat->statusPtr, &out);
	  ABORT (stat, SFTFILEIOH_EDIFFLENGTH, SFTFILEIOH_MSGEDIFFLENGTH);
	} /* if length(thisSFT) != common length */

      /* transfer the returned SFT into the SFTVector 
       * this is a bit complicated by the fact that it's a vector
       * of SFTtypes, not pointers: therefore we need to *COPY* the stuff !
       */
      LALCopySFT (stat->statusPtr, &(out->data[i]), oneSFT);
      BEGINFAIL (stat) {
	DestroyStringVector (fnames);    
	LALDestroySFTtype (stat->statusPtr, &oneSFT);
	LALDestroySFTVector (stat->statusPtr, &out);
      } ENDFAIL (stat);

      LALDestroySFTtype (stat->statusPtr, &oneSFT);
      oneSFT = NULL;	/* important for next call of LALReadSFTfile()! */

    } /* for i < numSFTs */

  DestroyStringVector (fnames);

  *sftvect = out;

  DETATCHSTATUSPTR (stat);
  RETURN (stat);

} /* LALReadSFTfiles () */



/* ----------------------------------------------------------------------*/
/* function to write an entire SFT to a file 
 * 
 * NOTE: currently only SFT-spec v1.0 is supported
 *
 *----------------------------------------------------------------------*/
/* <lalVerbatim file="SFTfileIOCP"> */
void
LALWriteSFTfile (LALStatus  *status,
		 const SFTtype *sft,		/* input: SFT to write to disk */
		 const CHAR *outfname)		/*  filename */
{ /* </lalVerbatim> */

  FILE  *fp = NULL;
  COMPLEX8  *inData;
  INT4  i;
  UINT4 datalen;
  REAL4  *rawdata;
  CHAR *rawheader, *ptr;
  SFTHeader header;

  INITSTATUS (status, "LALWriteSFTfile", SFTFILEIOC);
  ATTATCHSTATUSPTR (status);   
 
  /*   Make sure the arguments are not NULL and perform basic checks*/ 
  ASSERT (sft,   status, SFTFILEIOH_ENULL, SFTFILEIOH_MSGENULL);
  ASSERT (sft->data,  status, SFTFILEIOH_EVAL, SFTFILEIOH_MSGEVAL);
  ASSERT (sft->deltaF > 0, status, SFTFILEIOH_EVAL, SFTFILEIOH_MSGEVAL);
  ASSERT (outfname, status, SFTFILEIOH_ENULL, SFTFILEIOH_MSGENULL); 

  /* fill in the header information */
  header.version = 1.0;
  header.gpsSeconds = sft->epoch.gpsSeconds;
  header.gpsNanoSeconds = sft->epoch.gpsNanoSeconds;
  header.timeBase = 1.0 / sft->deltaF;
  header.fminBinIndex = (INT4) floor (sft->f0 / sft->deltaF + 0.5);	/* round to closest int! */
  header.length = sft->data->length; 

  /* build raw header for writing to disk */
  rawheader = LALCalloc (1, header_len_v1);
  if (rawheader == NULL) {
    ABORT (status, SFTFILEIOH_EMEM, SFTFILEIOH_MSGEMEM);    
  }
  ptr = rawheader;
  *(REAL8*) ptr = header.version;
  ptr += sizeof (REAL8);
  *(INT4*) ptr = header.gpsSeconds;
  ptr += sizeof (INT4);
  *(INT4*) ptr = header.gpsNanoSeconds;
  ptr += sizeof (INT4);
  *(REAL8*) ptr = header.timeBase;
  ptr += sizeof (REAL8);
  *(INT4*) ptr = header.fminBinIndex;
  ptr += sizeof (INT4);
  *(INT4*) ptr = header.length; 

  /* write data into a contiguous REAL4-array */
  datalen = 2 * header.length * sizeof(REAL4);	/* amount of bytes for SFT-data */

  rawdata = LALCalloc (1, datalen);
  if (rawdata == NULL) {
    LALFree (rawheader);
    ABORT (status, SFTFILEIOH_EMEM, SFTFILEIOH_MSGEMEM);    
  }

  inData = sft->data->data;
  for ( i = 0; i < header.length; i++)
    {
      rawdata[2 * i]     = inData[i].re;
      rawdata[2 * i + 1] = inData[i].im;
    } /* for i < length */


  /* open the file for writing */
  fp = fopen(outfname, "wb");
  if (fp == NULL) {
    LALFree (rawheader);
    LALFree (rawdata);
    ABORT (status, SFTFILEIOH_EFILE,  SFTFILEIOH_MSGEFILE);
  }

  /* write the header*/
  if( fwrite( rawheader, header_len_v1, 1, fp) != 1) {
    LALFree (rawheader);
    LALFree (rawdata);
    fclose (fp);
    ABORT (status, SFTFILEIOH_EFILE, SFTFILEIOH_MSGEFILE);
  }
  
  /* write the data */
  if (fwrite( rawdata, datalen, 1, fp) != 1) {
    LALFree (rawheader);
    LALFree (rawdata);
    fclose (fp);
    ABORT (status, SFTFILEIOH_EFILE, SFTFILEIOH_MSGEFILE);
  }

  /* done */
  fclose(fp);
  LALFree (rawheader);
  LALFree (rawdata);


  DETATCHSTATUSPTR (status);
  RETURN (status);

} /* WriteSFTtoFile() */



/*----------------------------------------------------------------------*/
/* <lalVerbatim file="SFTfileIOCP"> */
void 
LALReadSFTheader (LALStatus  *status,
		  SFTHeader   *header,	/* returned header */
		  const CHAR  *fname)	/* path+filename */
{ /* </lalVerbatim> */
  
  FILE *fp = NULL;
  SFTHeader  header1;
  CHAR *rawheader = NULL;
  CHAR *ptr = NULL;
  CHAR inVersion[8];
  REAL8 version;
  BOOLEAN swapEndian = 0;

  INITSTATUS (status, "LALReadSFTHeader", SFTFILEIOC);
  ATTATCHSTATUSPTR (status); 
  
  /*   Make sure the arguments are not NULL: */ 
  ASSERT (header, status, SFTFILEIOH_ENULL,  SFTFILEIOH_MSGENULL);
  ASSERT (fname,  status, SFTFILEIOH_ENULL,  SFTFILEIOH_MSGENULL);
  
  /* opening the SFT binary file */
  fp = LALOpenDataFile( fname );
  if (fp == NULL) {
    ABORT (status, SFTFILEIOH_EFILE,  SFTFILEIOH_MSGEFILE);
  }
  
  /* read version-number */
  if  (fread (inVersion, sizeof(inVersion), 1, fp) != 1) {
    fclose (fp);
    if (lalDebugLevel) LALPrintError ("\nInvalid SFT-file: %s\n\n", fname);
    ABORT (status, SFTFILEIOH_EHEADER,  SFTFILEIOH_MSGEHEADER);
  }

  /* try version 1.0 */
  version = 1.0;
  /* try swapping the version if it is not equal */
  if(memcmp(inVersion,&version,sizeof(version))){
    endian_swap (inVersion, sizeof(inVersion),1);
    swapEndian = 1;
    /* fail if still not true */
    if(memcmp(inVersion,&version,sizeof(version))){
      fclose (fp);
      if (lalDebugLevel) LALPrintError ("\nInvalid SFT-file: %s\n\n", fname);
      ABORT (status, SFTFILEIOH_EHEADER,  SFTFILEIOH_MSGEHEADER);
    }
  }

  /* read the whole header */
  rawheader = LALCalloc (1, header_len_v1);
  if (rawheader == NULL) {
    fclose (fp);
    ABORT (status, SFTFILEIOH_EMEM, SFTFILEIOH_MSGEMEM);    
  }
  
  rewind (fp);	/* go back to start */
  if (fread( rawheader, header_len_v1, 1, fp) != 1) {
    fclose (fp);
    LALFree (rawheader);
    if (lalDebugLevel) LALPrintError ("\nInvalid SFT-file: %s\n\n", fname);
    ABORT (status, SFTFILEIOH_EHEADER,  SFTFILEIOH_MSGEHEADER);
  }

  fclose(fp);

  /* now fill-in the header-struct with the appropriate fields */
  /* NOTE: we have to do it this way, because the struct can have 
   * padding in memory, so the fields are not guaranteed to lie 'close'
   * Endian-swapping ist done here if necessary
   */
  ptr = rawheader;
  if (swapEndian) endian_swap((CHAR*)ptr,sizeof(REAL8),1);
  header1.version	= *(REAL8*) ptr;
  ptr += sizeof(REAL8);
  if (swapEndian) endian_swap((CHAR*)ptr,sizeof(INT4),1);
  header1.gpsSeconds 	= *(INT4*) ptr;
  ptr += sizeof(INT4);
  if (swapEndian) endian_swap((CHAR*)ptr,sizeof(INT4),1);
  header1.gpsNanoSeconds= *(INT4*) ptr;
  ptr += sizeof(INT4);
  if (swapEndian) endian_swap((CHAR*)ptr,sizeof(REAL8),1);
  header1.timeBase      = *(REAL8*) ptr;
  ptr += sizeof(REAL8);
  if (swapEndian) endian_swap((CHAR*)ptr,sizeof(INT4),1);
  header1.fminBinIndex  = *(INT4*) ptr;
  ptr += sizeof(INT4);
  if (swapEndian) endian_swap((CHAR*)ptr,sizeof(INT4),1);
  header1.length     	= *(INT4*) ptr;

  LALFree (rawheader);

  /* ----- do some consistency-checks on the header-fields: ----- */

  /* gps_sec and gps_nsec >= 0 */
  if ( (header1.gpsSeconds < 0) || (header1.gpsNanoSeconds <0) ) {
    if (lalDebugLevel) LALPrintError ("\nInvalid SFT-file: %s\n\n", fname);
    ABORT (status, SFTFILEIOH_EHEADER,  SFTFILEIOH_MSGEHEADER);
  }

  /* tbase > 0 */
  if ( header1.timeBase <= 0 ) {
    if (lalDebugLevel) LALPrintError ("\nInvalid SFT-file: %s\n\n", fname);
    ABORT (status, SFTFILEIOH_EHEADER,  SFTFILEIOH_MSGEHEADER);
  }

  /* fminindex >= 0 */
  if (header1.fminBinIndex < 0) {
    if (lalDebugLevel) LALPrintError ("\nInvalid SFT-file: %s\n\n", fname);
    ABORT (status, SFTFILEIOH_EHEADER,  SFTFILEIOH_MSGEHEADER);
  }
  
  /* nsamples >= 0 */
  if (header1.length < 0) {
    if (lalDebugLevel) LALPrintError ("\nInvalid SFT-file: %s\n\n", fname);
    ABORT (status, SFTFILEIOH_EHEADER,  SFTFILEIOH_MSGEHEADER);
  }

  /* ok, the SFT-header seems consistent, so let's return it */
  *header = header1;
  
  DETATCHSTATUSPTR (status);
  RETURN (status);

} /* LALReadSFTheader() */



/* ------------------------------------------------------------
 * this is a function for low-level SFT data-reading:
 * the SFT-data is read starting from fminBinIndex and filled
 * into the pre-allocate vector sft of length N
 *
 * NOTE: !! NO re-normalization is done here!! this remains up
 *       to the caller of this function!!
 *
 *------------------------------------------------------------*/
/* <lalVerbatim file="SFTfileIOCP"> */
void
LALReadSFTdata(LALStatus *status,
	       SFTtype    *sft,    /* return SFT: assuming memory is allocated  */
	       const CHAR *fname,  /* path+filename */
	       INT4 fminBinIndex)  /* minimun frequency-index to read */
{ /* </lalVerbatim> */
  FILE        *fp = NULL;
  SFTHeader  header;
  UINT4 offset, readlen;
  REAL4 *rawdata = NULL;
  CHAR inVersion[8];
  REAL8 version;
  BOOLEAN swapEndian = 0;
  UINT4 i;

  INITSTATUS (status, "LALReadSFTdata", SFTFILEIOC);
  ATTATCHSTATUSPTR (status); 
  
  /*   Make sure the arguments are not NULL: */ 
  ASSERT (sft,   status, SFTFILEIOH_ENULL, SFTFILEIOH_MSGENULL);
  ASSERT (sft->data, status, SFTFILEIOH_ENULL, SFTFILEIOH_MSGENULL);
  ASSERT (fname, status, SFTFILEIOH_ENULL, SFTFILEIOH_MSGENULL);
  
  /* Read header */
  TRY ( LALReadSFTheader (status->statusPtr, &header, fname), status);

  /* check that the required frequency-interval is part of the SFT */
  readlen = sft->data->length;
  if ( (fminBinIndex < header.fminBinIndex) 
       || (fminBinIndex + (INT4)readlen > header.fminBinIndex + header.length) ) {
    ABORT (status, SFTFILEIOH_EFREQBAND, SFTFILEIOH_MSGEFREQBAND);
  }

  /* how many frequency-bins to skip */
  offset = fminBinIndex - header.fminBinIndex;

  /* open file for reading */
  if ( (fp = LALOpenDataFile( fname )) == NULL) {
    ABORT (status, SFTFILEIOH_EFILE, SFTFILEIOH_MSGEFILE);
  }

  /* read version-number */
  if  (fread (inVersion, sizeof(inVersion), 1, fp) != 1) {
    fclose (fp);
    if (lalDebugLevel) LALPrintError ("\nInvalid SFT-file: %s\n\n", fname);
    ABORT (status, SFTFILEIOH_EHEADER,  SFTFILEIOH_MSGEHEADER);
  }

  /* set invalid version */
  version = -1.0;

  if (version < 0) {
    /* try version 1.0 */
    version = 1.0;
    /* try swapping the version if it is not equal */
    if(memcmp(inVersion,&version,sizeof(version))){
      endian_swap (inVersion, sizeof(inVersion),1);
      swapEndian = 1;
      /* set invalid version if still not true */
      if(memcmp(inVersion,&version,sizeof(version))){
	version = -1;
	endian_swap (inVersion, sizeof(inVersion),1);
	swapEndian = 0;
      }
    }
  }

  /* fail if the version is invalid */
  if (version < 0) {
    fclose (fp);
    if (lalDebugLevel) LALPrintError ("\nInvalid SFT-file: %s\n\n", fname);
    ABORT (status, SFTFILEIOH_EHEADER,  SFTFILEIOH_MSGEHEADER);
  }

  /* check compatibility of version with this function */
  if (version != 1) {
    fclose (fp);
    ABORT (status, SFTFILEIOH_EVERSION, SFTFILEIOH_MSGEVERSION);
  }

  /* skip SFT-header in file */
  rewind (fp);
  if (fseek(fp, header_len_v1, SEEK_SET) != 0) {
    fclose (fp);
    ABORT (status, SFTFILEIOH_EFILE, SFTFILEIOH_MSGEFILE);
  }

  /* skip offset data points to the correct frequency-bin */
  if (fseek(fp, offset * 2 * sizeof(REAL4), SEEK_CUR) != 0) {
    fclose (fp);
    ABORT (status, SFTFILEIOH_EFILE, SFTFILEIOH_MSGEFILE);
  }

  /* ----- prepare memory for data-reading ----- */
  rawdata = LALCalloc (1, 2 * readlen *sizeof(REAL4) );
  if (rawdata == NULL) {
    fclose (fp);
    ABORT (status, SFTFILEIOH_EMEM, SFTFILEIOH_MSGEMEM);
  }

  /* we don't rely on memory-packing, so we read into a REAL4 array first */
  if (fread( rawdata, 2 * readlen * sizeof(REAL4), 1, fp) != 1) {
    fclose(fp);
    LALFree (rawdata);
    ABORT (status, SFTFILEIOH_EFILE, SFTFILEIOH_MSGEFILE);
  }
  fclose(fp);

  /* now fill data into output-vector */
  for (i=0; i < readlen; i++)
    {
      if (swapEndian)
	endian_swap((CHAR*)&rawdata[2*i], sizeof(REAL4), 2);
      sft->data->data[i].re = rawdata[2 * i];
      sft->data->data[i].im = rawdata[2 * i + 1];
    }    
  
  LALFree (rawdata);

  /* now fill in the header-info */
  strncpy (sft->name, fname, LALNameLength);
  sft->name[LALNameLength - 1 ] = '\0';	/* make sure it's 0-terminated */
  sft->deltaF  			= 1.0 / header.timeBase;
  sft->f0      			= fminBinIndex / header.timeBase;
  sft->epoch.gpsSeconds     	= header.gpsSeconds;
  sft->epoch.gpsNanoSeconds 	= header.gpsNanoSeconds;

  
  DETATCHSTATUSPTR (status);
  RETURN (status);

} /* LALReadSFTdata() */



/*----------------------------------------------------------------------
 *  copy an entire SFT-type into another
 *  we require the destination to have at least as many frequency-bins
 *  as the source, but it can have less..
 *----------------------------------------------------------------------*/
/* <lalVerbatim file="SFTfileIOCP"> */
void
LALCopySFT (LALStatus *stat, 
	    SFTtype *dest, 	/* output: copied SFT (has to be allocated) */
	    const SFTtype *src)	/* input-SFT to be copied */
{ /* </lalVerbatim> */

  INITSTATUS( stat, "LALDestroySFTVector", SFTFILEIOC);

  ASSERT (dest,  stat, SFTFILEIOH_ENULL,  SFTFILEIOH_MSGENULL);
  ASSERT (dest->data,  stat, SFTFILEIOH_ENULL,  SFTFILEIOH_MSGENULL);
  ASSERT (src, stat, SFTFILEIOH_ENULL,  SFTFILEIOH_MSGENULL);
  ASSERT (src->data, stat, SFTFILEIOH_ENULL,  SFTFILEIOH_MSGENULL);

  /* some hard requirements */
  if ( dest->data->length < src->data->length ) {
    ABORT (stat, SFTFILEIOH_ECOPYSIZE, SFTFILEIOH_MSGECOPYSIZE);
  }
  
  /* copy head */
  strcpy (dest->name, src->name);
  dest->epoch = src->epoch;
  dest->f0 = src->f0;
  dest->deltaF = src->deltaF;
  dest->sampleUnits = src->sampleUnits;
  /* copy data */
  memcpy (dest->data->data, src->data->data, dest->data->length * sizeof (dest->data->data[0]));
  
  RETURN (stat);

} /* LALCopySFT() */



/***********************************************************************
 * internal helper functions
 ***********************************************************************/
/* a little endian-swapper needed for SFT reading/writing */
static void 
endian_swap(CHAR * pdata, size_t dsize, size_t nelements)
{
  UINT4 i, j, indx;
  CHAR tempbyte;

  if (dsize <= 1) return;

  for (i=0; i<nelements; i++)
    {
      indx = dsize;
      for (j=0; j<dsize/2; j++)
	{
	  tempbyte = pdata[j];
	  indx = indx - 1;
	  pdata[j] = pdata[indx];
	  pdata[indx] = tempbyte;
	}
      
      pdata = pdata + dsize;
    }
  
  return;

} /* endian swap */

/*----------------------------------------------------------------------
 * glob() has been reported to fail under condor, so we use our own
 * function to get a filelist from a directory, using a glob-like pattern.
 *
 * looks pretty ugly with all the #ifdefs for the Microsoft C compiler
 *
 * NOTE: the list of filenames is returned SORTED ALPHABETICALLY !
 *
 *----------------------------------------------------------------------*/
static StringVector *
find_files (const CHAR *globdir)
{
#ifndef _MSC_VER
  DIR *dir;
  struct dirent *entry;
#else
  intptr_t dir;
  struct _finddata_t entry;
#endif
  CHAR *dname, *ptr1;
  CHAR *fpattern;
  size_t dirlen;
  CHAR **filelist = NULL; 
  UINT4 numFiles = 0;
  StringVector *ret = NULL;
  UINT4 j;
  UINT4 namelen;
  CHAR *thisFname;

  /* First we separate the globdir into directory-path and file-pattern */

#ifndef _MSC_VER
#define DIR_SEPARATOR '/'
#else
#define DIR_SEPARATOR '\\'
#endif

  /* any path specified or not ? */
  ptr1 = strrchr (globdir, DIR_SEPARATOR);
  if (ptr1)
    { /* yes, copy directory-path */
      dirlen = (size_t)(ptr1 - globdir) + 1;
      if ( (dname = LALCalloc (1, dirlen)) == NULL) 
	return (NULL);
      strncpy (dname, globdir, dirlen);
      dname[dirlen-1] = '\0';

      ptr1 ++; /* skip dir-separator */
      /* copy the rest as a glob-pattern for matching */
      if ( (fpattern = LALCalloc (1, strlen(ptr1) + 1)) == NULL )
	{
	  LALFree (dname);
	  return (NULL);
	}
      strcpy (fpattern, ptr1);   

    } /* if ptr1 */
  else /* no pathname given, assume "." */
    {
      if ( (dname = LALCalloc(1, 2)) == NULL) 
	return (NULL);
      strcpy (dname, ".");

      if ( (fpattern = LALCalloc(1, strlen(globdir)+1)) == NULL)
	{
	  LALFree (dname);
	  return (NULL);
	}
      strcpy (fpattern, globdir);	/* just file-pattern given */
    } /* if !ptr */
  

#ifndef _MSC_VER
  /* now go through the file-list in this directory */
  if ( (dir = opendir(dname)) == NULL) {
    LALPrintError ("Can't open data-directory `%s`\n", dname);
    LALFree (dname);
    return (NULL);
  }
#else
  if ((ptr1 = (CHAR*)LALMalloc(strlen(dname)+3)) == NULL)
    return(NULL);
  sprintf(ptr1,"%s\\*",dname);  
  dir = _findfirst(ptr1,&entry);
  LALFree(ptr1);
  if (dir == -1) {
    LALPrintError ("Can't find file for pattern `%s`\n", ptr1);
    LALFree (dname);
    return (NULL);
  }
#endif

#ifndef _MSC_VER
  while ( (entry = readdir (dir)) != NULL )
    {
      thisFname = entry->d_name;
#else
  do
    {
      thisFname = entry.name;
#endif
      /* now check if glob-pattern fpattern matches the current filename */
      if ( amatch(thisFname, fpattern) 
	   /* and check if we didnt' match some obvious garbage like "." or ".." : */
	   && strcmp( thisFname, ".") && strcmp( thisFname, "..") )
	{

	  numFiles ++;
	  if ( (filelist = LALRealloc (filelist, numFiles * sizeof(CHAR*))) == NULL) {
	    LALFree (dname);
	    LALFree (fpattern);
	    return (NULL);
	  }

	  namelen = strlen(thisFname) + strlen(dname) + 2 ;

	  if ( (filelist[ numFiles - 1 ] = LALCalloc (1, namelen)) == NULL) {
	    for (j=0; j < numFiles; j++)
	      LALFree (filelist[j]);
	    LALFree (filelist);
	    LALFree (dname);
	    LALFree (fpattern);
	    return (NULL);
	  }

	  sprintf(filelist[numFiles-1], "%s%c%s", dname, DIR_SEPARATOR, thisFname);

	} /* if filename matched pattern */

    } /* while more directory entries */
#ifdef _MSC_VER
  while ( _findnext (dir,&entry) == 0 );
#endif

#ifndef _MSC_VER
  closedir (dir);
#else
  _findclose(dir);
#endif

  LALFree (dname);
  LALFree (fpattern);

  /* ok, did we find anything? */
  if (numFiles == 0)
    return (NULL);

  if ( (ret = LALCalloc (1, sizeof (StringVector) )) == NULL) 
    {
      for (j=0; j<numFiles; j++)
	LALFree (filelist[j]);
      LALFree (filelist);
      return (NULL);
    }

  ret->length = numFiles;
  ret->data = filelist;

  /* sort this alphabetically (in-place) */
  SortStringVector (ret);

  return (ret);
} /* find_files() */


static void
DestroyStringVector (StringVector *strings)
{
  UINT4 i;

  for (i=0; i < strings->length; i++)
    LALFree (strings->data[i]);
  
  LALFree (strings->data);
  LALFree (strings);

} /* DestroyStringVector () */

/* comparison function for strings */
 typedef CHAR * charptr;	/* little trick to shut compiler-warnings up */
static int mycomp (const void *p1, const void *p2)
{
  const charptr *s1 = (const charptr *) p1;
  const charptr *s2 = (const charptr *) p2;
  return (strcmp ( *s1, *s2 ) );
}

/* sort string-vector alphabetically */
static void
SortStringVector (StringVector *strings)
{
  qsort ( (void*)(strings->data), (size_t)(strings->length), sizeof(CHAR*), mycomp );
} /* SortStringVector() */



/***********************************************************************
 *
 * the following are
 * DEBUG + testing functions: not meant for general use!! 
 *
 ***********************************************************************/

#if 0 /* REMOVED */

/* write a time-series into a text-file */
static void
write_timeSeriesR4 (FILE *fp, const REAL4TimeSeries *series)
{
  REAL8 timestamp; 
  UINT4 i;

  if (series == NULL)
    {
      printf ("\nempty input!\n");
      return; 
    }

  timestamp = 1.0*series->epoch.gpsSeconds + series->epoch.gpsNanoSeconds * 1.0e-9;

  for( i = 0; i < series->data->length; i++)
    {
      fprintf( fp, "%16.9f %e\n", timestamp, series->data->data[i] );
      timestamp += series->deltaT;
    }

  return;

} /* write_timeSeriesR4() */

static void
write_timeSeriesR8 (FILE *fp, const REAL8TimeSeries *series)
{
  REAL8 timestamp; 
  UINT4 i;

  timestamp = 1.0*series->epoch.gpsSeconds + series->epoch.gpsNanoSeconds * 1.0e-9;

  for( i = 0; i < series->data->length; i++)
    {
      fprintf( fp, "%f %e\n", timestamp, series->data->data[i] );
      timestamp += series->deltaT;
    }

  return;

} /* write_timeSeriesR4() */


/* write an SFT in xmgrace-readable format */
static void 
LALwriteSFTtoXMGR (LALStatus *stat, const SFTtype *sft, const CHAR *fname)
{
  FILE *fp;

  REAL4 val;
  UINT4 i;
  REAL8 Tsft, freqBand;
  REAL8 f0, df, ff;
  UINT4 set, nsamples;

  const CHAR *xmgrHeader = 
    "@version 50103\n"
    "@xaxis label \"f (Hz)\"\n";

  INITSTATUS( stat, "LALwriteSFTtoXMGR", SFTFILEIOC);
  ATTATCHSTATUSPTR( stat );

  fp = fopen (fname, "w");

  if( !fp ) {
    ABORT (stat, SFTFILEIOH_EFILE, SFTFILEIOH_MSGEFILE);
  }


  f0 = sft->f0;
  df = sft->deltaF;
  nsamples = sft->data->length;
  Tsft = 1.0 / sft->deltaF;
  freqBand = (nsamples-1.0) * df;

  fprintf (fp, xmgrHeader);
  fprintf (fp, "@subtitle \"epoch = (%d s, %d ns), Tsft = %f\"\n", 
	   sft->epoch.gpsSeconds, sft->epoch.gpsNanoSeconds, Tsft);

  set = 0;
  /* Print set header. */
  fprintf( fp, "\n@target G0.S%d\n@type xy\n", set);
  fprintf( fp, "@s%d color (0,0,1)\n", set );
  for (i=0; i < nsamples; i++)
    {
      ff = f0 + i*df;
      val = sft->data->data[i].re;

      fprintf(fp, "%f %e\n", ff, val);
        
    } /* for i < nsamples */

  set ++;
  /* Print set header. */
  fprintf( fp, "\n@target G0.S%d\n@type xy\n", set);
  fprintf( fp, "@s%d color (0,1,0)\n", set );
  for (i=0; i < nsamples; i++)
    {
      ff = f0 + i*df;
      val = sft->data->data[i].im;

      fprintf(fp, "%f %e\n", ff, val);
        
    } /* for i < nsamples */

  set ++;
  /* Print set header. */
  fprintf( fp, "\n@target G0.S%d\n@type xy\n", set);
  fprintf( fp, "@s%d color (1,0,0)\n", set );
  for (i=0; i < nsamples; i++)
    {
      ff = f0 + i*df;
      val = (sft->data->data[i].re * sft->data->data[i].re + sft->data->data[i].im*sft->data->data[i].im) / freqBand;
      
      fprintf(fp, "%f %e\n", ff, val);
        
    } /* for i < nsamples */

  
  fclose(fp);

  DETATCHSTATUSPTR( stat );
  RETURN (stat);
  
} /* write_SFT() */


/* dump an SFT into a text-file 
 * format: 0 = openDX (include header), 1 = xmgrace (no header)
 */
static void dump_SFT (FILE *fp, const SFTtype *sft, INT4 format)
{

  REAL4 valre, valim;
  UINT4 i;
  REAL8 Tsft, freqBand;
  REAL8 f0, df, ff;
  UINT4 nsamples;
  REAL4 norm;
  REAL4 P_k;

  f0 = sft->f0;
  df = sft->deltaF;
  nsamples = sft->data->length;
  Tsft = 1.0 / sft->deltaF;
  freqBand = (nsamples-1.0) * df;

  norm = (REAL4)( Tsft / nsamples);

  /* if openDX format: add a header with number of points..*/
  if ( format == 0)
    {
      fprintf (fp, "points = %d\n", nsamples);
      fprintf (fp, "format = ascii\n");
      fprintf (fp, "field = field0\n");
      fprintf (fp, "structure = 2-vector\n");
      fprintf (fp, "type = float\n");
      fprintf (fp, "dependency = positions\n");
      fprintf (fp, "header = marker \"SFT-data\\n\"\n");
      fprintf (fp, "positions = regular, %f, %f \n", f0, df);
      fprintf(fp, "end\n\n");
      
      /* write some SFT header-info */
      fprintf (fp, "SFT-header\n");
      fprintf (fp, "Name = %s\n", sft->name);
      fprintf (fp, "Timestamp = %d s, %d ns\n", sft->epoch.gpsSeconds, sft->epoch.gpsNanoSeconds);
      fprintf (fp, "Tsft = %f\n", Tsft);
      fprintf (fp, "Start-frequency = %f Hz\n", f0);
      fprintf (fp, "Frequency-step = %f Hz\n", df);
      fprintf (fp, "Frequency-band = %f Hz\n", freqBand);
      fprintf (fp, "Number of frequency-bins nsamples = %d\n", nsamples);
      
      /* write SFT-data */
      fprintf (fp, "\nSFT-data\n");
    }

  for (i=0; i < nsamples; i++)
    {
      ff = f0 + i*df;
      valre = norm * sft->data->data[i].re;
      valim = norm * sft->data->data[i].im;
      if ( (i==0) && (i == nsamples-1) )
	P_k = sqrt(valre*valre + valim*valim);
      else
	P_k = 2.0 * sqrt(valre*valre + valim*valim);

      if (format == 1) /* xmgrace */
	fprintf (fp, "%f %e %e %e\n", ff, valre, valim, P_k );
      else if (format == 0) /* openDX */
	fprintf (fp, "%e %e\n", valre, valim);
        
    } /* for i < nsamples */
  
  return;
  
} /* dump_SFT() */

#endif /* REMOVED */

/*
 * robust glob pattern matcher
 * ozan s. yigit/dec 1994
 * public domain
 *
 * glob patterns:
 *	*	matches zero or more characters
 *	?	matches any single character
 *	[set]	matches any character in the set
 *	[^set]	matches any character NOT in the set
 *		where a set is a group of characters or ranges. a range
 *		is written as two characters seperated with a hyphen: a-z denotes
 *		all characters between a to z inclusive.
 *	[-set]	set matches a literal hypen and any character in the set
 *	[]set]	matches a literal close bracket and any character in the set
 *
 *	char	matches itself except where char is '*' or '?' or '['
 *	\char	matches char, including any pattern character
 *
 * examples:
 *	a*c		ac abc abbc ...
 *	a?c		acc abc aXc ...
 *	a[a-z]c		aac abc acc ...
 *	a[-a-z]c	a-c aac abc ...
 *
 * $Log$
 * Revision 1.36  2005/06/17 21:49:04  jolien
 * moved glob.c code into SFTfileIO.c ; removed non-LAL functions from SFTfileIO.c
 *
 * Revision 1.2  2005/02/09 11:23:30  reinhard
 * squashed warnings
 *
 * Revision 1.1  2004/09/16 15:50:05  reinhard
 * add public-domain function amatch() that can do glob-like string matching.
 *
 * Revision 1.3  1995/09/14  23:24:23  oz
 * removed boring test/main code.
 *
 * Revision 1.2  94/12/11  10:38:15  oz
 * cset code fixed. it is now robust and interprets all
 * variations of cset [i think] correctly, including [z-a] etc.
 * 
 * Revision 1.1  94/12/08  12:45:23  oz
 * Initial revision
 */

#ifndef NEGATE
#define NEGATE	'^'			/* std cset negation char */
#endif

#define TRUE    1
#define FALSE   0

static int amatch(char *str, char *p);

static int
amatch(char *str, char *p)
{
	int negate;
	int match;
	int c;

	while (*p) {
		if (!*str && *p != '*')
			return FALSE;

		switch (c = *p++) {

		case '*':
			while (*p == '*')
				p++;

			if (!*p)
				return TRUE;

			if (*p != '?' && *p != '[' && *p != '\\')
				while (*str && *p != *str)
					str++;

			while (*str) {
				if (amatch(str, p))
					return TRUE;
				str++;
			}
			return FALSE;

		case '?':
			if (*str)
				break;
			return FALSE;
/*
 * set specification is inclusive, that is [a-z] is a, z and
 * everything in between. this means [z-a] may be interpreted
 * as a set that contains z, a and nothing in between.
 */
		case '[':
			if (*p != NEGATE)
				negate = FALSE;
			else {
				negate = TRUE;
				p++;
			}

			match = FALSE;

			while (!match && (c = *p++)) {
				if (!*p)
					return FALSE;
				if (*p == '-') {	/* c-c */
					if (!*++p)
						return FALSE;
					if (*p != ']') {
						if (*str == c || *str == *p ||
						    (*str > c && *str < *p))
							match = TRUE;
					}
					else {		/* c-] */
						if (*str >= c)
							match = TRUE;
						break;
					}
				}
				else {			/* cc or c] */
					if (c == *str)
						match = TRUE;
					if (*p != ']') {
						if (*p == *str)
							match = TRUE;
					}
					else
						break;
				}
			}

			if (negate == match)
				return FALSE;
/*
 * if there is a match, skip past the cset and continue on
 */
			while (*p && *p != ']')
				p++;
			if (!*p++)	/* oops! */
				return FALSE;
			break;

		case '\\':
			if (*p)
				c = *p++;
		default:
			if (c != *str)
				return FALSE;
			break;

		}
		str++;
	}

	return !*str;
}

