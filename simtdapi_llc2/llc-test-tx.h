/**
 * @addtogroup cohda_llc_intern_app LLC application
 * @{
 *
 * @section cohda_llc_intern_app_dbg LLC 'test-tx' plugin command
 *
 * @verbatim
    MKx channel config
    Usage: llc test-tx
   @endverbatim
 *
 * @file
 * LLC: 'cw-llc' monitoring application ('test-tx' command support)
 *
 */


//------------------------------------------------------------------------------
// Copyright (c) 2015 Cohda Wireless Pty Ltd
//------------------------------------------------------------------------------

#ifndef __LLC_TESTTX_H__
#define __LLC_TESTTX_H__

//------------------------------------------------------------------------------
// Included headers
//------------------------------------------------------------------------------
#include "llc-plugin.h"

//------------------------------------------------------------------------------
// Macros & Constants
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Type definitions
//------------------------------------------------------------------------------
/// Application/command state structure
typedef struct LLCTx
{
  /// Command line agruments
  struct
  {
    int Argc;
    char **ppArgv;
  } Args;
  uint32_t SeqNum;
  tTxOpts TxOpts;
  /// MKx handle
  struct MKx *pMKx;
  /// MKx file descriptor
  int Fd;
  /// Exit flag
  bool Exit;
  FILE *fp;
  /// Ethernet header (for preloading invariants)
  struct ethhdr EthHdr;
  bool TxContinue;
} tLLCTx;
//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------

//extern struct PluginCmd TxCmd;

#endif // __LLC_TESTTX_H__
/**
 * @}
 */
