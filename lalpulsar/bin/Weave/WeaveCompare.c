//
// Copyright (C) 2016, 2017 Karl Wette
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with with program; see the file COPYING. If not, write to the
// Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
// MA 02110-1301 USA
//

///
/// \file
/// \ingroup lalpulsar_bin_Weave
///

#include "Weave.h"
#include "SetupData.h"
#include "OutputResults.h"

#include <lal/LogPrintf.h>
#include <lal/UserInput.h>

int main( int argc, char *argv[] )
{

  // Set help information
  lalUserVarHelpBrief = "compare result files produced by lalpulsar_Weave";

  ////////// Parse user input //////////

  // Initialise user input variables
  struct uvar_type {
    BOOLEAN sort_by_semi_phys;
    CHAR *setup_file, *result_file_1, *result_file_2;
    REAL8 unmatched_item_tol, param_tol_mism, result_tol_L1, result_tol_L2, result_tol_angle, result_tol_at_max;
    UINT4 round_param_to_dp, round_param_to_sf, toplist_limit;
  } uvar_struct = {
    .unmatched_item_tol = 0,
    .param_tol_mism = 1e-3,
    .result_tol_L1 = 5.5e-2,
    .result_tol_L2 = 4.5e-2,
    .result_tol_angle = 0.04,
    .result_tol_at_max = 5e-2,
  };
  struct uvar_type *const uvar = &uvar_struct;

  // Register user input variables:
  //
  // - General
  //
  XLALRegisterUvarMember(
    setup_file, STRING, 'S', REQUIRED,
    "Setup file generated by lalpulsar_WeaveSetup; the segment list, parameter-space metrics, and other required data. "
  );
  XLALRegisterUvarMember(
    result_file_1, STRING, '1', REQUIRED,
    "First result file produced by lalpulsar_Weave for comparison. "
  );
  XLALRegisterUvarMember(
    result_file_2, STRING, '2', REQUIRED,
    "Second result file produced by lalpulsar_Weave for comparison. "
  );
  //
  // - Comparison options
  //
  lalUserVarHelpOptionSubsection = "Comparison options";
  XLALRegisterUvarMember(
    sort_by_semi_phys, BOOLEAN, 'p', OPTIONAL,
    "Sort toplist items by semicoherent physical coordinates, instead of serial number. "
  );
  XLALRegisterUvarMember(
    round_param_to_dp, UINT4, 'f', OPTIONAL,
    "Round parameter-space points to the given number of decimal places (must be >0, or zero to disable). "
  );
  XLALRegisterUvarMember(
    round_param_to_sf, UINT4, 'e', OPTIONAL,
    "Round parameter-space points to the given number of significant figures (must be >0, or zero to disable). "
  );
  //
  // - Tolerances
  //
  lalUserVarHelpOptionSubsection = "Tolerances";
  XLALRegisterUvarMember(
    unmatched_item_tol, REAL8, 'u', OPTIONAL,
    "When comparing toplists, allow this fraction of items to be umatched to an item in the other toplist (must in in range [0, 1]). "
  );
  XLALRegisterUvarMember(
    param_tol_mism, REAL8, 'm', OPTIONAL,
    "Allowed tolerance on mismatch between parameter-space points (must be >0, or zero to disable comparison). "
  );
  XLALRegisterUvarMember(
    result_tol_L1, REAL8, 'r', OPTIONAL,
    "Allowed tolerance on relative error between mismatch between result vectors using L1 (sum of absolutes) norm. "
    "Must be within range [0,2]. "
  );
  XLALRegisterUvarMember(
    result_tol_L2, REAL8, 's', OPTIONAL,
    "Allowed tolerance on relative error between mismatch between result vectors using L2 (root of sum of squares) norm. "
    "Must be within range [0,2]. "
  );
  XLALRegisterUvarMember(
    result_tol_angle, REAL8, 'a', OPTIONAL,
    "Allowed tolerance on angle between result vectors, in radians. "
    "Must be within range [0,PI]. "
  );
  XLALRegisterUvarMember(
    result_tol_at_max, REAL8, 'x', OPTIONAL,
    "Allowed tolerance on relative errors at maximum component of each result vector. "
    "Must be within range [0,2]. "
  );
  XLALRegisterUvarMember(
    toplist_limit, UINT4, 'n', OPTIONAL,
    "Maximum number of candidates to compare in an output toplist; if 0, all candidates are compared. "
  );

  // Parse user input
  XLAL_CHECK_FAIL( xlalErrno == 0, XLAL_EFUNC, "A call to XLALRegisterUvarMember() failed" );
  BOOLEAN should_exit = 0;
  XLAL_CHECK_FAIL( XLALUserVarReadAllInput( &should_exit, argc, argv, lalPulsarVCSInfoList ) == XLAL_SUCCESS, XLAL_EFUNC );

  // Check user input:
  //
  // - General
  //

  //
  // - Comparison options
  //

  //
  // - Tolerances
  //
  XLALUserVarCheck( &should_exit,
                    0 <= uvar->unmatched_item_tol && uvar->unmatched_item_tol <= 1,
                    UVAR_STR( unmatched_item_tol ) " must be within range [0,1]" );
  XLALUserVarCheck( &should_exit,
                    0 <= uvar->param_tol_mism,
                    UVAR_STR( param_tol_mism ) " must be >=0" );
  XLALUserVarCheck( &should_exit,
                    0 <= uvar->result_tol_L1 && uvar->result_tol_L1 <= 2,
                    UVAR_STR( result_tol_L1 ) " must be within range [0,2]" );
  XLALUserVarCheck( &should_exit,
                    0 <= uvar->result_tol_L2 && uvar->result_tol_L2 <= 2,
                    UVAR_STR( result_tol_L2 ) " must be within range [0,2]" );
  XLALUserVarCheck( &should_exit,
                    0 <= uvar->result_tol_angle && uvar->result_tol_angle <= LAL_PI,
                    UVAR_STR( result_tol_angle ) " must be within range [0,PI]" );
  XLALUserVarCheck( &should_exit,
                    0 <= uvar->result_tol_at_max && uvar->result_tol_at_max <= 2,
                    UVAR_STR( result_tol_at_max ) " must be within range [0,2]" );

  // Exit if required
  if ( should_exit ) {
    return EXIT_FAILURE;
  }
  LogPrintf( LOG_NORMAL, "Parsed user input successfully\n" );

  ////////// Load setup data //////////

  // Initialise setup data
  WeaveSetupData XLAL_INIT_DECL( setup );

  {
    // Open setup file
    LogPrintf( LOG_NORMAL, "Opening setup file '%s' for reading ...\n", uvar->setup_file );
    FITSFile *file = XLALFITSFileOpenRead( uvar->setup_file );
    XLAL_CHECK_FAIL( file != NULL, XLAL_EFUNC );

    // Read setup data
    XLAL_CHECK_FAIL( XLALWeaveSetupDataRead( file, &setup ) == XLAL_SUCCESS, XLAL_EFUNC );

    // Close result file
    XLALFITSFileClose( file );
    LogPrintf( LOG_NORMAL, "Closed setup file '%s'\n", uvar->setup_file );
  }

  ////////// Load output results //////////

  // Initialise output results
  WeaveOutputResults *out_1 = NULL, *out_2 = NULL;

  {
    // Open result file #1
    LogPrintf( LOG_NORMAL, "Opening result file '%s' for reading ...\n", uvar->result_file_1 );
    FITSFile *file = XLALFITSFileOpenRead( uvar->result_file_1 );
    XLAL_CHECK_FAIL( file != NULL, XLAL_EFUNC );

    // Read output results
    XLAL_CHECK_FAIL( XLALWeaveOutputResultsReadAppend( file, &out_1, 0 ) == XLAL_SUCCESS, XLAL_EFUNC );

    // Close result file
    XLALFITSFileClose( file );
    LogPrintf( LOG_NORMAL, "Closed result file '%s'\n", uvar->result_file_1 );
  }

  {
    // Open result file #2
    LogPrintf( LOG_NORMAL, "Opening result file '%s' for reading ...\n", uvar->result_file_2 );
    FITSFile *file = XLALFITSFileOpenRead( uvar->result_file_2 );
    XLAL_CHECK_FAIL( file != NULL, XLAL_EFUNC );

    // Read output results
    XLAL_CHECK_FAIL( XLALWeaveOutputResultsReadAppend( file, &out_2, 0 ) == XLAL_SUCCESS, XLAL_EFUNC );

    // Close result file
    XLALFITSFileClose( file );
    LogPrintf( LOG_NORMAL, "Closed result file '%s'\n", uvar->result_file_2 );
  }

  ////////// Compare output results //////////

  // Initalise struct with comparison tolerances
  const VectorComparison result_tol = {
    .relErr_L1 = uvar->result_tol_L1,
    .relErr_L2 = uvar->result_tol_L2,
    .angleV = uvar->result_tol_angle,
    .relErr_atMaxAbsx = uvar->result_tol_at_max,
    .relErr_atMaxAbsy = uvar->result_tol_at_max,
  };

  // Compare output results
  BOOLEAN equal = 0;
  LogPrintf( LOG_NORMAL, "Comparing result files '%s' and '%s ...\n", uvar->result_file_1, uvar->result_file_2 );
  XLAL_CHECK_FAIL( XLALWeaveOutputResultsCompare( &equal,
                   &setup, uvar->sort_by_semi_phys,
                   uvar->round_param_to_dp, uvar->round_param_to_sf, uvar->unmatched_item_tol, uvar->param_tol_mism, &result_tol, uvar->toplist_limit,
                   out_1, out_2
                                                ) == XLAL_SUCCESS, XLAL_EFUNC );
  LogPrintf( LOG_NORMAL, "Result files compare %s\n", equal ? "EQUAL" : "NOT EQUAL" );

  ////////// Cleanup memory and exit //////////

  // Cleanup memory from output results
  XLALWeaveOutputResultsDestroy( out_1 );
  XLALWeaveOutputResultsDestroy( out_2 );

  // Cleanup memory from setup data
  XLALWeaveSetupDataClear( &setup );

  // Cleanup memory from user input
  XLALDestroyUserVars();

  // Check for memory leaks
  LALCheckMemoryLeaks();

  LogPrintf( LOG_NORMAL, "Finished successfully!\n" );

  // Indicate whether result files compared equal
  return equal ? 0 : 1;

XLAL_FAIL:

  // Indicate an error preventing result files from being compared
  return 2;

}

// Local Variables:
// c-file-style: "linux"
// c-basic-offset: 2
// End:
