//
// From https://heasarc.gsfc.nasa.gov/docs/software/fitsio/cexamples.html:
//
// FITS Tools: Handy FITS Utilities that illustrate how to use CFITSIO
// -------------------------------------------------------------------
//
// These are working programs written in ANSI C that illustrate how one can
// easily read, write, and modify FITS files using the CFITSIO library. Most of
// these programs are very short, containing only a few 10s of lines of
// executable code or less, yet they perform quite useful operations on FITS
// files. Copy the programs to your local machine, then compile, and link them
// with the CFITSIO library. A short description of how to use each program can
// be displayed by executing the program without any command line arguments.
//
// You may freely modify, reuse, and redistribute these programs as you wish. It
// is often easier to use one of these programs as a template when writing a new
// program, rather than coding the new program completely from scratch.
//

/**
 * \file
 * \ingroup lalpulsar_bin_FITSTools
 */

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#if defined(HAVE_LIBCFITSIO)
// disable -Wstrict-prototypes flag for this header file as this causes
// a build failure for cfitsio-3.440+
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <fitsio.h>
#pragma GCC diagnostic pop
#else
#error CFITSIO library is not available
#endif

static int ndigits( int x )
{
  return floor( log10( abs( x ) ) ) + 1;
}

int main( int argc, char *argv[] )
{
  fitsfile *fptr = 0;      /* FITS file pointer, defined in fitsio.h */
  char *val = 0, value[1000], nullstr[] = "NAN";
  char keyword[FLEN_KEYWORD], colname[1000][FLEN_VALUE];
  int status = 0;   /*  CFITSIO status value MUST be initialized to zero!  */
  int hdunum = 0, hdutype = 0, ncols = 0, ii = 0, anynul = 0, typecode[1000], dispwidth[1000], nd = 0;
  long jj = 0, nrows = 0, nvecelem[1000], kk = 0, repeat, width = 0;

  int printhelp = ( argc == 2 && ( strcmp( argv[1], "-h" ) == 0 || strcmp( argv[1], "--help" ) == 0 ) );

  char *argfile;
  int printhdr;
  if ( !printhelp && argc == 3 && strcmp( argv[1], "-n" ) == 0 ) {
    printhdr = 0;
    argfile = argv[2];
  } else if ( !printhelp && argc == 2 ) {
    printhdr = 1;
    argfile = argv[1];
  } else {
    fprintf( stderr, "Usage:  %s filename[ext][col filter][row filter] \n", argv[0] );
    fprintf( stderr, "\n" );
    fprintf( stderr, "List the contents of a FITS table \n" );
    fprintf( stderr, "\n" );
    fprintf( stderr, "Examples: \n" );
    fprintf( stderr, "  %s tab.fits[GTI]           - list the GTI extension\n", argv[0] );
    fprintf( stderr, "  %s tab.fits[1][#row < 101] - list first 100 rows\n", argv[0] );
    fprintf( stderr, "  %s tab.fits[1][col X;Y]    - list X and Y cols only\n", argv[0] );
    fprintf( stderr, "  %s tab.fits[1][col -PI]    - list all but the PI col\n", argv[0] );
    fprintf( stderr, "  %s tab.fits[1][col -PI][#row < 101]  - combined case\n", argv[0] );
    fprintf( stderr, "  %s -n ...                  - list without table header\n", argv[0] );
    fprintf( stderr, "\n" );
    fprintf( stderr, "Display formats can be modified with the TDISPn keywords.\n" );
    return ( 0 );
  }

#if defined(PAGER) && defined(HAVE_POPEN) && defined(HAVE_PCLOSE)
  FILE *fout = popen( PAGER, "w" );
  if ( fout == NULL ) {
    fprintf( stderr, "Could not execute '%s'\n", PAGER );
    return ( 1 );
  }
#else
  FILE *fout = stdout;
#endif

  if ( !fits_open_file( &fptr, argfile, READONLY, &status ) ) {
    if ( fits_get_hdu_num( fptr, &hdunum ) == 1 )
      /* This is the primary array;  try to move to the */
      /* first extension and see if it is a table */
    {
      fits_movabs_hdu( fptr, 2, &hdutype, &status );
    } else {
      fits_get_hdu_type( fptr, &hdutype, &status ); /* Get the HDU type */
    }

    if ( hdutype == IMAGE_HDU ) {
      fprintf( stderr, "Error: this program only displays tables, not images\n" );
    } else {
      fits_get_num_rows( fptr, &nrows, &status );
      fits_get_num_cols( fptr, &ncols, &status );

      for ( ii = 1; ii <= ncols; ii++ ) {
        fits_make_keyn( "TTYPE", ii, keyword, &status );
        fits_read_key( fptr, TSTRING, keyword, colname[ii], NULL, &status );
        fits_get_col_display_width( fptr, ii, &dispwidth[ii], &status );
        fits_get_coltype( fptr, ii, &typecode[ii], &repeat, &width, &status );
        if ( typecode[ii] != TSTRING && repeat > 1 ) {
          nvecelem[ii] = repeat;
          dispwidth[ii] += ndigits( nvecelem[ii] ) + 2;
        } else {
          nvecelem[ii] = 1;
        }
        if ( dispwidth[ii] < ( int )strlen( colname[ii] ) ) {
          dispwidth[ii] = ( int )strlen( colname[ii] );
        }
      }

      /* print column names as column headers */
      if ( printhdr ) {
        fprintf( fout, "##\n## " );
        for ( ii = 1; ii <= ncols; ii++ ) {
          if ( nvecelem[ii] > 1 ) {
            for ( kk = 1; kk <= nvecelem[ii]; kk++ ) {
              nd = ndigits( kk );
              fprintf( fout, "%*s[%*li] ", dispwidth[ii] - nd - 2, colname[ii], nd, kk );
            }
          } else {
            fprintf( fout, "%*s ", dispwidth[ii], colname[ii] );
          }
        }
        fprintf( fout, "\n" ); /* terminate header line */
      }

      /* print each column, row by row (there are faster ways to do this) */
      val = value;
      for ( jj = 1; jj <= nrows && !status; jj++ ) {
        fprintf( fout, "   " );
        for ( ii = 1; ii <= ncols && !status; ii++ ) {

          /* read value as a string, regardless of intrinsic datatype */
          for ( kk = 1; kk <= nvecelem[ii] && !status; kk++ ) {
            if ( fits_read_col_str( fptr, ii, jj, kk, 1, nullstr, &val, &anynul, &status ) ) {
              break;  /* jump out of loop on error */
            }
            fprintf( fout, "%*s ", dispwidth[ii], value );
          }

        }
        fprintf( fout, "\n" );
      }
    }
    fits_close_file( fptr, &status );
  }

#if defined(PAGER) && defined(HAVE_POPEN) && defined(HAVE_PCLOSE)
  pclose( fout );
#endif

  if ( status ) {
    fits_report_error( stderr, status ); /* print any error message */
  }
  return ( status );
}
