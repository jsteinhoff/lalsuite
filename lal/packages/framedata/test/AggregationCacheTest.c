/*
 * AggregationCacheTest.c - test online frame data aggregation routines
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
 *
 * Copyright (C) 2009 Adam Mercer
 */

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <lal/LALStdio.h>
#include <lal/LALDatatypes.h>
#include <lal/Aggregation.h>
#include <lal/XLALError.h>
#include <lal/LIGOMetadataTables.h>
#include <lal/FrameCache.h>
#include <lal/FrameStream.h>
#include <lal/PrintFTSeries.h>
#include <lal/TimeSeries.h>
#include <lal/XLALError.h>
#include <lal/Date.h>

/* flags for getopt_long */
int vrbflg;

/* global variables */
CHAR *ifo = NULL;
LIGOTimeGPS gps = {0, 0};
INT4 duration = 0;
INT4 timeout = 0;

/*
 * helper functions
 */

/* parse command line options */
static void parse_options(INT4 argc, CHAR *argv[])
{
  int c = -1;

  while(1)
  {
    static struct option long_options[] =
    {
      /* options that set a flag */
      {"verbose", no_argument, &vrbflg, 1},
      /* options that don't set a flag */
      {"help", no_argument, 0, 'a'},
      {"debug-level", required_argument, 0, 'b'},
      {"ifo", required_argument, 0, 'c'},
      {"gps-start-time", required_argument, 0, 'd'},
      {"duration", required_argument, 0, 'e'},
      {"timeout", required_argument, 0, 'f'},
      {0, 0, 0, 0}
    };

    /* getopt_long stores the option here */
    int option_index = 0;
    size_t optarg_len;

    /* parse options */
    c = getopt_long_only(argc, argv, "ab:c:d:e:f:", long_options, \
        &option_index);

    if (c == -1)
    {
      /* end of options, break loop */
      break;
    }

    switch(c)
    {
      case 0:
        /* if this option sets a flag, do nothing else now */
        if (long_options[option_index].flag != 0)
        {
          break;
        }
        else
        {
          fprintf(stderr, "error parsing option %s with argument %s\n", \
              long_options[option_index].name, optarg);
          exit(1);
        }
        break;

      case 'a':
        /* help */
        fprintf(stdout, "Usage: AggregationCacheTest [options]\n");
        fprintf(stdout, " --help                 print this message\n");
        fprintf(stdout, " --verbose              run in verbose mode\n");
        fprintf(stdout, " --debug-level N        set lalDebugLevel\n");
        fprintf(stdout, " --ifo IFO              set IFO\n");
        fprintf(stdout, " --gps-start-time GPS   set GPS start time\n");
        fprintf(stdout, " --duration TIME        set data duration\n");
        fprintf(stdout, " --timeout TIME         set timeout\n");
        exit(0);
        break;

      case 'b':
        /* set debug level */
        lalDebugLevel = atoi(optarg);
        break;

      case 'c':
        /* set ifo */
        optarg_len = strlen(optarg) + 1;
        ifo = (CHAR *)calloc(optarg_len, sizeof(CHAR));
        memcpy(ifo, optarg, optarg_len);
        break;

      case 'd':
        /* set gps start time */
        gps.gpsSeconds = atoi(optarg);
        gps.gpsNanoSeconds = 0;
        if (gps.gpsSeconds <= 0)
        {
          fprintf(stderr, "invalid argument to --%s: %d\n", \
              long_options[option_index].name, gps.gpsSeconds);
          exit(1);
        }
        break;

      case 'e':
        /* set duration */
        duration = atoi(optarg);
        if (duration <= 0)
        {
          fprintf(stderr, "invalid argument to --%s: %d\n", \
              long_options[option_index].name, duration);
          exit(1);
        }
        break;

      case 'f':
        /* get timeout */
        timeout = atoi(optarg);
        if (timeout < 0)
        {
          fprintf(stderr, "invalid argument to --%s: %d\n", \
              long_options[option_index].name, timeout);
          exit(1);
        }

      case '?':
        exit(1);
        break;

      default:
        fprintf(stderr, "unknown error while parsing options\n");
        exit(1);
    }
  }

  if (optind < argc)
  {
    fprintf(stderr, "extraneous command line arguments:\n");
    while(optind < argc)
    {
      fprintf(stderr, "%s\n", argv[optind++]);
    }
    exit(1);
  }

  /*
   * check for required arguments
   */

  /* ifo */
  if (ifo == NULL)
  {
    fprintf(stderr, "--ifo must be specified\n");
    exit(1);
  }

  /* gps start time */
  if (gps.gpsSeconds == 0)
  {
    fprintf(stderr, "--gps-start-time must be specified\n");
    exit(1);
  }

  /* duration */
  if (duration == 0)
  {
    fprintf(stderr, "--duration must be specified\n");
    exit(1);
  }

  return;
}


/* main program entry point */
INT4 main(INT4 argc, CHAR *argv[])
{
  /* declare variables */
  LIGOTimeGPS *latest;
  LIGOTimeGPS time_now;
  FrCache *cache;
  CHAR *type;
  CHAR filename[FILENAME_MAX];
  INT4 wait;
  CHAR *ptimeout;

  /* get maximum wait time from ONLINEHOFT_TIMEOUT */
  ptimeout = getenv("ONLINEHOFT_TIMEOUT");
  if (ptimeout != NULL)
  {
    timeout = atoi(ptimeout);
  }

  /* parse command line options */
  parse_options(argc, argv);

  /* get time of gps time of latest frame */
  latest = XLALAggregationLatestGPS(ifo);
  if (latest == NULL)
  {
    fprintf(stderr, "error: unable to determine latest GPS time\n");
    exit(1);
  }

  /* get current gps time */
  if (XLALGPSTimeNow(&time_now) == NULL)
  {
    fprintf(stderr, "error: unable to determine current time\n");
    exit(1);
  }

  /* report time info */
  if (vrbflg)
  {
    fprintf(stdout, "current time:          %d\n", time_now.gpsSeconds);
    fprintf(stdout, "latest data available: %d\n", latest->gpsSeconds);
    fprintf(stdout, "requested start        %d\n", gps.gpsSeconds);
    fprintf(stdout, "requested duration:    %9d\n", duration);
  }

  /* is requested data in the future? */
  if (XLALGPSCmp(&time_now, &gps) == -1)
  {
    /* determine wait time */
    wait = (INT4)floor(XLALGPSDiff(&gps, &time_now) + 0.5);

    /* wait for data to be available */
    fprintf(stdout, "requested data is in the future, waiting: %ds\n", wait);
    sleep(wait);
  }

  /* get frame cache */
  cache = XLALAggregationFrameCache(ifo, &gps, duration);
  if (cache == NULL)
  {
    fprintf(stderr, "error: failed to get frame cache\n");
    exit(xlalErrno);
  }

  /* get frame type */
  type = XLALAggregationFrameType(ifo);
  if (type == NULL)
  {
    fprintf(stderr, "error: failed to get frame type\n");
    exit(xlalErrno);
  }
  /* create name for cache file */
  LALSnprintf(filename, FILENAME_MAX, "%c-%s-%d-%d.cache", ifo[0], \
      type, gps.gpsSeconds, duration);

  /* save cache */
  XLALFrExportCache(cache, filename);

  /* free memory */
  free(ifo);

  /* check for memory leaks */
  LALCheckMemoryLeaks();

  return 0;
}
