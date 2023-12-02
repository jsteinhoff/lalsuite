/* Matt Pitkin 18/10/12
   Code to check TOA files (generated by TEMPO2) against TOAs calculated from
   the given parameter file

   Compile with:
   gcc PulsarTOATest.c -o PulsarTOATest -L${LSCSOFT_LOCATION}/lib -I${LSCSOFT_LOCATION}/include -lm -llalsupport -llal -llalpulsar -std=c99
   */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include <lal/LALStdlib.h>
#include <lal/LALgetopt.h>
#include <lal/AVFactories.h>
#include <lal/LALBarycenter.h>
#include <lal/LALInitBarycenter.h>
#include <lal/LALConstants.h>
#include <lal/Date.h>
#include <lal/LALString.h>

#include <lal/ReadPulsarParFile.h>
#include <lal/BinaryPulsarTiming.h>

/* set the allowed error in the phase - 1 degree */
#define MAX_PHASE_ERR_DEGS 1.0

#define USAGE \
"Usage: %s [options]\n\n"\
" --help (-h)              display this message\n"\
" --verbose (-v)           display all error messages\n"\
" --par-file (-p)          TEMPO2 parameter (.par) file\n"\
" --tim-file (-t)          TEMPO2 TOA (.tim) file\n"\
" --ephem (-e)             Ephemeris type (DE200, DE405 [default], or DE421)\n"\
" --clock (-c)             Clock correction file (default is none)\n"\
" --simulated (-s)         Set if the TOA file is from simulated data\n\
                          e.g. created with the TEMPO2 'fake' plugin:\n\
                          tempo2 -gr fake -f pulsar.par -ndobs 1 -nobsd 5\n\
                            -start 54832 -end 55562 -ha 8 -randha n -rms 0\n"\
"\n"

int verbose = 0;

typedef struct tagInputParams {
  char *parfile;
  char *timfile;
  char *ephem;
  char *clock;

  int simulated;
} InputParams;

void get_input_args( InputParams *pars, int argc, char *argv[] );

int main( int argc, char *argv[] )
{
  FILE *fpin = NULL;
  FILE *fpout = NULL;

  char filestr[256];
  double radioFreq = 0.0, rf[10000];
  double TOA[10000];
  double num1;
  int telescope;
  int i = 0, j = 0, k = 0, n = 0, exceedPhaseErr = 0;

  double PPTime[10000]; /* Pulsar proper time - corrected for solar system and binary orbit delay times */
  const double D = 2.41e-4; /* dispersion constant from TEMPO */

  /* Binary pulsar variables */
  PulsarParameters *params = NULL;
  BinaryPulsarInput input;
  BinaryPulsarOutput output;

  InputParams par;

  /* LAL barycentring variables */
  BarycenterInput baryinput;
  EarthState earth;
  EmissionTime emit;
  EphemerisData *edat = NULL;
  TimeCorrectionData *tdat = NULL;

  double MJD_tcorr[10000];
  double tcorr[10000];

  double T = 0.;
  double DM;
  double phase0 = 0.;

  long offset;

  TimeCorrectionType ttype;

  XLALSetErrorHandler( XLALExitErrorHandler );

  get_input_args( &par, argc, argv );

  if ( verbose ) {
    fprintf( stderr, "\n" );
    fprintf( stderr, "*******************************************************\n" );
    fprintf( stderr, "** We are assuming that the TOAs where produced with **\n" );
    fprintf( stderr, "** TEMPO2 and are sited at the Parkes telescope.     **\n" );
    fprintf( stderr, "*******************************************************\n" );
  }
  if ( ( fpin = fopen( par.timfile, "r" ) ) == NULL ) {
    fprintf( stderr, "Error... can't open TOA file!\n" );
    return 1;
  }

  /* read in TOA and phase info - assuming in the format output by TEMPO2 .tim
     file */
  while ( !feof( fpin ) ) {
    offset = ftell( fpin );
    char firstchar[256];

    /* if line starts with FORMAT, MODE, or a C or # then skip it */
    if ( fscanf( fpin, "%s", firstchar ) != 1 ) {
      break;
    }
    if ( !strcmp( firstchar, "FORMAT" ) || !strcmp( firstchar, "MODE" ) ||
         firstchar[0] == '#' || firstchar[0] == 'C' ) {
      if ( fscanf( fpin, "%*[^\n]" ) == EOF ) {
        break;
      }
      continue;
    } else {
      fseek( fpin, offset, SEEK_SET );

      /* is data is simulated with TEMPO2 fake plugin then it only has 5 columns */
      if ( par.simulated ) {
        fscanf( fpin, "%s%lf%lf%lf%d", filestr, &radioFreq, &TOA[i], &num1, &telescope );
      } else {
        char randstr1[256], randstr2[256], randstr3[256];
        int randnum;

        fscanf( fpin, "%s%lf%lf%lf%d%s%s%s%d", filestr, &radioFreq, &TOA[i], &num1, &telescope, randstr1, randstr2, randstr3, &randnum );
      }
      rf[i] = radioFreq;

      i++;
    }
  }

  fclose( fpin );

  if ( verbose ) {
    fprintf( stderr, "I've read in the TOAs\n" );
  }

  /* read in telescope time corrections from file */
  if ( par.clock != NULL ) {
    if ( ( fpin = fopen( par.clock, "r" ) ) == NULL ) {
      fprintf( stderr, "Error... can't open clock file for reading!\n" );
      exit( 1 );
    }

    do {
      offset = ftell( fpin );
      char firstchar[256];

      /* if line starts with # then ignore as a comment */
      fscanf( fpin, "%s", firstchar );
      if ( firstchar[0] == '#' ) {
        if ( fscanf( fpin, "%*[^\n]" ) == EOF ) {
          break;
        }
        continue;
      } else {
        fseek( fpin, offset, SEEK_SET );

        fscanf( fpin, "%lf%lf", &MJD_tcorr[j], &tcorr[j] );
        j++;
      }
    } while ( !feof( fpin ) );

    fclose( fpin );
  }

  /* read in binary params from par file */
  params = XLALReadTEMPOParFile( par.parfile );

  if ( verbose ) {
    fprintf( stderr, "I've read in the parameter file\n" );
  }

  /* set telescope location - TEMPO2 defaults to Parkes, so use this x,y,z components are got from tempo */
  if ( telescope != 7 ) {
    fprintf( stderr, "Error, TOA file not using the Parkes telescope!\n" );
    exit( 1 );
  }

  /* location of the Parkes telescope */
  baryinput.site.location[0] = -4554231.5 / LAL_C_SI;
  baryinput.site.location[1] = 2816759.1 / LAL_C_SI;
  baryinput.site.location[2] = -3454036.3 / LAL_C_SI;

  /* initialise the solar system ephemerides */
  const char *earthFile = NULL, *sunFile = NULL;
  if ( par.ephem == NULL ) { /* default to DE405 */
    earthFile = TEST_PKG_DATA_DIR "earth00-40-DE405.dat.gz";
    sunFile   = TEST_PKG_DATA_DIR "sun00-40-DE405.dat.gz";
  } else if ( strcmp( par.ephem, "DE200" ) == 0 ) {
    earthFile = TEST_PKG_DATA_DIR "earth00-40-DE200.dat.gz";
    sunFile   = TEST_PKG_DATA_DIR "sun00-40-DE200.dat.gz";
  } else if ( strcmp( par.ephem, "DE405" ) == 0 ) {
    earthFile = TEST_PKG_DATA_DIR "earth00-40-DE405.dat.gz";
    sunFile   = TEST_PKG_DATA_DIR "sun00-40-DE405.dat.gz";
  } else if ( strcmp( par.ephem, "DE421" ) == 0 ) {
    earthFile = TEST_PKG_DATA_DIR "earth00-40-DE421.dat.gz";
    sunFile   = TEST_PKG_DATA_DIR "sun00-40-DE421.dat.gz";
  } else {
    XLAL_ERROR_MAIN( XLAL_EINVAL, "Invalid ephem='%s', allowed are 'DE200', 'DE405' or 'DE421'\n", par.ephem );
  }

  edat = XLALInitBarycenter( earthFile, sunFile );

  if ( verbose ) {
    fprintf( stderr, "I've set up the ephemeris files\n" );
  }

  if ( verbose ) {
    fpout = fopen( "pulsarPhase.txt", "w" );
  }

  DM = PulsarGetREAL8ParamOrZero( params, "DM" );

  if ( PulsarCheckParam( params, "PX" ) ) {
    baryinput.dInv = ( 3600. / LAL_PI_180 ) * PulsarGetREAL8Param( params, "PX" ) /
                     ( LAL_C_SI * LAL_PC_SI / LAL_LYR_SI );
  } else {
    baryinput.dInv = 0.0;  /* no parallax */
  }

  CHAR *units = NULL;
  if ( PulsarCheckParam( params, "UNITS" ) ) {
    units = XLALStringDuplicate( PulsarGetStringParam( params, "UNITS" ) );
  }
  if ( units == NULL ) {
    ttype = TIMECORRECTION_TEMPO2;
  } else if ( !strcmp( units, "TDB" ) ) {
    ttype = TIMECORRECTION_TDB;
  } else if ( !strcmp( units, "TCB" ) ) {
    ttype = TIMECORRECTION_TCB;  /* same as TYPE_TEMPO2 */
  } else {
    ttype = TIMECORRECTION_TEMPO2;  /*default */
  }
  XLALFree( units );

  /* read in the time correction file */
  const char *tcFile = NULL;
  if ( ttype == TIMECORRECTION_TEMPO2 || ttype == TIMECORRECTION_TCB ) {
    tcFile = TEST_PKG_DATA_DIR "te405_2000-2019.dat.gz";
  } else if ( ttype == TIMECORRECTION_TDB ) {
    tcFile = TEST_PKG_DATA_DIR "tdb_2000-2019.dat.gz";
  }

  tdat = XLALInitTimeCorrections( tcFile );

  const REAL8Vector *f0s = PulsarGetREAL8VectorParam( params, "F" );
  REAL8Vector *f0update = XLALCreateREAL8Vector( f0s->length );

  /* check for glitch parameters */
  REAL8 *glep = NULL, *glph = NULL, *glf0 = NULL, *glf1 = NULL, *glf2 = NULL, *glf0d = NULL, *gltd = NULL;
  UINT4 glnum = 0;
  if ( PulsarCheckParam( params, "GLEP" ) ) {
    const REAL8Vector *gleppars = PulsarGetREAL8VectorParam( params, "GLEP" );
    glnum = gleppars->length;

    /* get epochs */
    glep = XLALCalloc( glnum, sizeof( REAL8 ) ); /* initialise to zeros */
    for ( j = 0; j < ( INT4 )gleppars->length; j++ ) {
      glep[j] = gleppars->data[j];
    }

    /* get phase offsets */
    glph = XLALCalloc( glnum, sizeof( REAL8 ) ); /* initialise to zeros */
    if ( PulsarCheckParam( params, "GLPH" ) ) {
      const REAL8Vector *glpars = PulsarGetREAL8VectorParam( params, "GLPH" );
      for ( j = 0; j < ( INT4 )glpars->length; j++ ) {
        glph[j] = glpars->data[j];
      }
    }

    /* get frequencies offsets */
    glf0 = XLALCalloc( glnum, sizeof( REAL8 ) ); /* initialise to zeros */
    if ( PulsarCheckParam( params, "GLF0" ) ) {
      const REAL8Vector *glpars = PulsarGetREAL8VectorParam( params, "GLF0" );
      for ( j = 0; j < ( INT4 )glpars->length; j++ ) {
        glf0[j] = glpars->data[j];
      }
    }

    /* get frequency derivative offsets */
    glf1 = XLALCalloc( glnum, sizeof( REAL8 ) ); /* initialise to zeros */
    if ( PulsarCheckParam( params, "GLF1" ) ) {
      const REAL8Vector *glpars = PulsarGetREAL8VectorParam( params, "GLF1" );
      for ( j = 0; j < ( INT4 )glpars->length; j++ ) {
        glf1[j] = glpars->data[j];
      }
    }

    /* get second frequency derivative offsets */
    glf2 = XLALCalloc( glnum, sizeof( REAL8 ) ); /* initialise to zeros */
    if ( PulsarCheckParam( params, "GLF2" ) ) {
      const REAL8Vector *glpars = PulsarGetREAL8VectorParam( params, "GLF2" );
      for ( j = 0; j < ( INT4 )glpars->length; j++ ) {
        glf2[j] = glpars->data[j];
      }
    }

    /* get decaying frequency component offset derivative */
    glf0d = XLALCalloc( glnum, sizeof( REAL8 ) ); /* initialise to zeros */
    if ( PulsarCheckParam( params, "GLF0D" ) ) {
      const REAL8Vector *glpars = PulsarGetREAL8VectorParam( params, "GLF0D" );
      for ( j = 0; j < ( INT4 )glpars->length; j++ ) {
        glf0d[j] = glpars->data[j];
      }
    }

    /* get decaying frequency component decay time constant */
    gltd = XLALCalloc( glnum, sizeof( REAL8 ) ); /* initialise to zeros */
    if ( PulsarCheckParam( params, "GLTD" ) ) {
      const REAL8Vector *glpars = PulsarGetREAL8VectorParam( params, "GLTD" );
      for ( j = 0; j < ( INT4 )glpars->length; j++ ) {
        gltd[j] = glpars->data[j];
      }
    }
  }

  for ( j = 0; j < i; j++ ) {
    double t; /* DM for current pulsar - make more general */
    double deltaD_f2;
    double phase;
    double tt0;
    double phaseWave = 0., tWave = 0., phaseGlitch = 0.;
    double taylorcoeff = 1.;

    if ( par.clock != NULL ) {
      while ( MJD_tcorr[k] < TOA[j] ) {
        k++;
      }

      /* linearly interpolate between corrections */
      double grad = ( tcorr[k] - tcorr[k - 1] ) / ( MJD_tcorr[k] - MJD_tcorr[k - 1] );

      t = ( TOA[j] - 44244. ) * 86400. + ( tcorr[k - 1] + grad * ( TOA[j] - MJD_tcorr[k - 1] ) );
    } else {
      t = ( TOA[j] - 44244.0 ) * 86400.0;
    }

    /* convert time from UTC to GPS reference */
    t += ( double )XLALGPSLeapSeconds( ( UINT4 )t );

    /* set pulsar position */
    REAL8 posepoch = PulsarGetREAL8ParamOrZero( params, "POSEPOCH" );
    baryinput.delta = PulsarGetREAL8ParamOrZero( params, "DECJ" ) + PulsarGetREAL8ParamOrZero( params, "PMDEC" ) * ( t - posepoch );
    baryinput.alpha = PulsarGetREAL8ParamOrZero( params, "RAJ" ) + PulsarGetREAL8ParamOrZero( params, "PMRA" ) * ( t - posepoch ) / cos( baryinput.delta );

    /* recalculate the time delay at the dedispersed time */
    XLALGPSSetREAL8( &baryinput.tgps, t );

    /* calculate solar system barycentre time delay */
    XLALBarycenterEarthNew( &earth, &baryinput.tgps, edat, tdat, ttype );
    XLALBarycenter( &emit, &baryinput, &earth );

    /* correct to infinite observation frequency for dispersion measure */
    rf[j] = rf[j] + rf[j] * ( 1. - emit.tDot );

    deltaD_f2 = DM / ( D * rf[j] * rf[j] );
    t -= deltaD_f2; /* dedisperse times */

    /* calculate binary barycentre time delay */
    input.tb = t + ( double )emit.deltaT;

    if ( PulsarCheckParam( params, "BINARY" ) ) {
      XLALBinaryPulsarDeltaTNew( &output, &input, params );

      PPTime[j] = t + ( ( double )emit.deltaT + output.deltaT );
    } else {
      PPTime[j] = t + ( double )emit.deltaT;
    }

    if ( j == 0 ) {
      T = PPTime[0] - PulsarGetREAL8ParamOrZero( params, "PEPOCH" );
      for ( k = 0; k < ( INT4 )f0s->length; k++ ) {
        f0update->data[k] = f0s->data[k];
        REAL8 Tupdate = T;
        taylorcoeff = 1.;
        for ( n = k + 1; n < ( INT4 )f0s->length; n++ ) {
          taylorcoeff /= ( REAL8 )( n - k );
          f0update->data[k] += taylorcoeff * f0s->data[n] * Tupdate;
          Tupdate *= T;
        }
      }
    }

    tt0 = PPTime[j] - PPTime[0];

    if ( PulsarCheckParam( params, "WAVE_OM" ) ) {
      REAL8 dtWave = ( XLALGPSGetREAL8( &emit.te ) - PulsarGetREAL8ParamOrZero( params, "WAVEEPOCH" ) ) / 86400.;
      REAL8 om = PulsarGetREAL8ParamOrZero( params, "WAVE_OM" );

      const REAL8Vector *waveSin = PulsarGetREAL8VectorParam( params, "WAVESIN" );
      const REAL8Vector *waveCos = PulsarGetREAL8VectorParam( params, "WAVECOS" );

      for ( k = 0; k < ( INT4 )waveSin->length; k++ ) {
        tWave += waveSin->data[k] * sin( om * ( REAL8 )( k + 1. ) * dtWave ) + waveCos->data[k] * cos( om * ( REAL8 )( k + 1. ) * dtWave );
      }
      phaseWave = f0s->data[0] * tWave;
    }

    /* if glitches are present add on effects of glitch phase */
    if ( glnum > 0 ) {
      for ( k = 0; k < ( INT4 )glnum; k++ ) {
        if ( PPTime[j] >= glep[k] ) {
          REAL8 dtg = 0, expd = 1.;
          dtg = PPTime[j] - glep[k]; /* time since glitch */
          if ( gltd[k] != 0. ) {
            expd = exp( -dtg / gltd[k] );  /* decaying part of glitch */
          }

          /* add glitch phase - based on equations in formResiduals.C of TEMPO2 from Eqn 1 of Yu et al (2013) http://ukads.nottingham.ac.uk/abs/2013MNRAS.429..688Y */
          phaseGlitch += glph[k] + glf0[k] * dtg + 0.5 * glf1[k] * dtg * dtg + ( 1. / 6. ) * glf2[k] * dtg * dtg * dtg + glf0d[k] * gltd[k] * ( 1. - expd );
        }
      }
    }

    phase = 0., taylorcoeff = 1.;
    REAL8 tt0update = tt0;
    for ( k = 0; k < ( INT4 )f0update->length; k++ ) {
      taylorcoeff /= ( REAL8 )( k + 1 );
      phase += taylorcoeff * f0update->data[k] * tt0update;
      tt0update *= tt0;
    }

    phase = fmod( phase + phaseWave + phaseGlitch + 0.5, 1.0 ) - 0.5;

    if ( j == 0 ) {
      phase0 = phase;  /* get first phase */
    }

    if ( verbose ) {
      fprintf( fpout, "%.9lf\t%lf\n", tt0, phase - phase0 );
    }

    /* check the phase error (in degrees) */
    if ( fabs( phase - phase0 ) * 360. > MAX_PHASE_ERR_DEGS ) {
      exceedPhaseErr = 1;
    }
  }

  if ( verbose ) {
    fclose( fpout );
  }

  if ( exceedPhaseErr ) {
    return 1;  /* phase difference is too great, so fail */
  }

  // ----- clean up memory
  XLALDestroyEphemerisData( edat );
  XLALDestroyTimeCorrectionData( tdat );
  XLALFree( par.parfile );
  XLALFree( par.timfile );
  XLALFree( par.ephem );
  XLALFree( par.clock );
  XLALDestroyREAL8Vector( f0update );

  if ( glnum > 0 ) {
    XLALFree( glph );
    XLALFree( glep );
    XLALFree( glf0 );
    XLALFree( glf1 );
    XLALFree( glf2 );
    XLALFree( glf0d );
    XLALFree( gltd );
  }

  PulsarFreeParams( params );

  LALCheckMemoryLeaks();

  return 0;

} // main()

void get_input_args( InputParams *pars, int argc, char *argv[] )
{
  struct LALoption long_options[] = {
    { "help",                     no_argument,        0, 'h' },
    { "par-file",                 required_argument,  0, 'p' },
    { "tim-file",                 required_argument,  0, 't' },
    { "ephemeris",                required_argument,  0, 'e' },
    { "clock",                    required_argument,  0, 'c' },
    { "simulated",                no_argument,        0, 's' },
    { "verbose",                  no_argument,     NULL, 'v' },
    { 0, 0, 0, 0 }
  };

  char args[] = "hp:t:e:c:sv";
  char *program = argv[0];

  pars->parfile = NULL;
  pars->timfile = NULL;
  pars->ephem = NULL;
  pars->clock = NULL;

  pars->simulated = 0;

  /* get input arguments */
  while ( 1 ) {
    int option_index = 0;
    int c;

    c = LALgetopt_long( argc, argv, args, long_options, &option_index );
    if ( c == -1 ) { /* end of options */
      break;
    }

    switch ( c ) {
    case 0: /* if option set a flag, nothing else to do */
      if ( long_options[option_index].flag ) {
        break;
      } else
        fprintf( stderr, "Error parsing option %s with argument %s\n",
                 long_options[option_index].name, LALoptarg );
    /* fallthrough */
    case 'h': /* help message */
      fprintf( stderr, USAGE, program );
      exit( 0 );
    case 'v': /* verbose output */
      verbose = 1;
      break;
    case 'p': /* par file */
      pars->parfile = XLALStringDuplicate( LALoptarg );
      break;
    case 't': /* TEMPO2 timing file */
      pars->timfile = XLALStringDuplicate( LALoptarg );
      break;
    case 'e': /* ephemeris (DE405/DE200) */
      pars->ephem = XLALStringDuplicate( LALoptarg );
      break;
    case 'c': /* clock file */
      pars->clock = XLALStringDuplicate( LALoptarg );
      break;
    case 's': /* simulated data */
      pars->simulated = 1;
      break;
    case '?':
      fprintf( stderr, "unknown error while parsing options\n" );
    /* fallthrough */
    default:
      fprintf( stderr, "unknown error while parsing options\n" );
    }
  }

  if ( pars->parfile == NULL ) {
    fprintf( stderr, "Error... no .par file supplied!\n" );
    exit( 1 );
  }

  if ( pars->timfile == NULL ) {
    fprintf( stderr, "Error... no .tim file supplied!\n" );
    exit( 1 );
  }
}
