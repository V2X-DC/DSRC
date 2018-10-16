//------------------------------------------------------------------------------
// Copyright (c) 2010 Cohda Wireless Pty Ltd
//------------------------------------------------------------------------------

#ifndef __IEEE1609__APP__TEST_MLME_IF__RXOPTS_H__
#define __IEEE1609__APP__TEST_MLME_IF__RXOPTS_H__

//------------------------------------------------------------------------------
// Included headers
//------------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>
#include "mk2mac-api-types.h"

//------------------------------------------------------------------------------
// Macros & Constants
//------------------------------------------------------------------------------

/// Maximum length of option lists
#define RXOPTS_MAXFILENAMELENGTH     (512)
#define RXOPTS_MAXIFNAMELEN          (64)
//------------------------------------------------------------------------------
// Type definitions
//------------------------------------------------------------------------------

/**
 * RxOpt Error Codes
 */
typedef enum RxOptsErrCode_tag
{
  RXOPTS_ERR_NONE = 0,
  RXOPTS_ERR_INVALIDOPTION = 1,
  RXOPTS_ERR_INVALIDOPTIONARG = 2
} eRxOptsErrCode;

typedef int tRxOptsErrCode;

/// Rx Configuration (built from Cmd line opts).
/// Some experiment control at top level.
typedef struct RxOpts_tag
{

  /// packet Log File name
  char pPacketLogFileName[RXOPTS_MAXFILENAMELENGTH];

  /// Report File name
  char pReportFileName[RXOPTS_MAXFILENAMELENGTH];

  /// Log Unmatched packets in addition
  bool LogUnMatched;

  /// Dump entire packet payload to file
  bool DumpPayload;

  /// Should per packet details be dumped to stdout?
  bool DumpToStdout;

  /// Dump Report to screen every now and then (this many unblocks)
  uint16_t ReportPeriod;

  /// Statistics Filter SA. Stats only calculated for packet matching this SA.
  tMK2MACAddr StatSA;
  bool StatSAFilt;

  /// Statistics Filter DA. Stats only calculated for packet matching this SA.
  tMK2MACAddr StatDA;
  bool StatDAFilt;

  /// Interface (Wave Raw or Mgmt)
  char pInterfaceName[RXOPTS_MAXIFNAMELEN];

  /// Channel Number to listen on. Assumes Tx successive SeqNums
  uint8_t ChannelNumber;

  /// Give latency results
  uint8_t LatencyResults;

} tRxOpts;

/// Exported Functions
void RxOpts_printusage ();
void RxOpts_Print (tRxOpts * pRxOpts);
int RxOpts_New (int argc,
                char **argv,
                tRxOpts *pRxOpts);

#endif // __IEEE1609__APP__TEST_MLME_IF__RXOPTS_H__
