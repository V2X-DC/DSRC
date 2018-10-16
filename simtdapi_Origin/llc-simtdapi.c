/**
 * @addtogroup cohda_llc_intern_app LLC application
 * @{
 *
 * @section cohda_llc_intern_app_dbg LLC 'simtdapi' plugin
 *
 * @verbatim
    One plugin for chconfig, test-tx and test-rx
   @endverbatim
 *
 * @file
 * LLC: 'cw-llc' monitoring application
 *
 */
//------------------------------------------------------------------------------
// Copyright (c) 2015 Cohda Wireless Pty Ltd
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Included headers
//------------------------------------------------------------------------------
#include "llc-chconfig.h"
#include "llc-test-rx.h"
#include "llc-test-tx.h"
#include "llc-plugin.h"
#include "debug-levels.h"

//------------------------------------------------------------------------------
// Macros & Constants
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Type definitions
//------------------------------------------------------------------------------

static int LLC_SimTDAPIInit (void);
static void LLC_SimTDAPIExit (void);
PLUGIN("simtdapi", LLC_SimTDAPIInit, LLC_SimTDAPIExit);

/**
 * @brief Called by the llc app when it loads the .so
 */
static int LLC_SimTDAPIInit (void)
{
  int Ret = Plugin_CmdRegister(&ChconfigCmd);
  Ret |= Plugin_CmdRegister(&RxCmd);
  Ret |= Plugin_CmdRegister(&TxCmd);
  return Ret;
}

/**
 * @brief Called by the llc app when it exits
 */
static void LLC_SimTDAPIExit (void)
{
  Plugin_CmdDeregister(&TxCmd);
  Plugin_CmdDeregister(&RxCmd);
  Plugin_CmdDeregister(&ChconfigCmd);
}
