//------------------------------------------------------------------------------
// Copyright (c) 2010 Cohda Wireless Pty Ltd
//------------------------------------------------------------------------------

#ifndef __IEEE1609__APP__TEST_MLME_IF__RXSTATS_H__
#define __IEEE1609__APP__TEST_MLME_IF__RXSTATS_H__

//------------------------------------------------------------------------------
// Included headers
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "linux/cohda/llc/llc-api.h"
//#include "dot4-internal.h"
//#include "mk2mac-api.h" // Descriptors
//------------------------------------------------------------------------------
// Macros & Constants
//------------------------------------------------------------------------------
#define RXSTATS_MAXMCS (16)
#define RXSTATS_MAXLENGTHS (4096)
#define RXSTATS_MAXCHANNELS (256)
#define RXSTATS_HIST_SIZE (20)
//------------------------------------------------------------------------------
// Type definitions
//------------------------------------------------------------------------------
/**
 * RxStats Error Codes
 */
typedef enum RxStatsErrCode_tag
{
  RXSTATS_ERR_NONE = 0,
  RXSTATS_ERR_MALLOC = 1
} eRxStatsErrCode;

typedef int tRxStatsErrCode;

/// Receivers open-loop analysis of received frames. Doesn't know Tx frames?
typedef struct RxStats
{
  /// The most recently received SeqNum (could be delivered out of order)
  int LastSeqNum;
  /// Not Macthed
  long NNotMatched;
  /// Macthed
  long NMatched;
  /// Matched but had payload error
  long NPayloadErrors;
  /// number of payload errors at the end of the previous interval
  long NPayloadErrorsPrev;
  /// estimated number of transmitted frames at the end of the previous interval
  long NTxFramesPrev;
  /// number of missed frames at the end of the previous interval
  long NMissedFramesPrev;
  /// How many packets received by Channel Number
  long * pNMatchedbyChannelNumber;
  /// Matched Histogram by MCS and Packet Length
  long * pNMatchedHist;
  /// Payload errors Histogram by MCS and Packet Length
  long * pNPayloadErrorsHist;
  /// Flat Histogram length (private)
  long HistogramLength;
  /// Rx Buffer (rather than re-alloc each time)
  unsigned char * pBuf;
  /// Max latency results
  int MaxUsec;
  /// Min latency results
  int MinUsec;
  /// Avg latency results
  int AvgUsec;
  /// Total latency
  long TotalLatency;
  /// Histogram of latencies
  int Hist[RXSTATS_HIST_SIZE];

} tRxStats;

//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------

tRxStatsErrCode RxStats_new (tRxStats ** ppRxStats);
tRxStatsErrCode RxStats_free (tRxStats * pRxStats);
tRxStatsErrCode RxStats_Report (FILE * fp,
                                tRxStats * pRxStats);
tRxStatsErrCode RxStats_Assess (tRxStats * pRxStats,
                                uint8_t ChannelNumber,
                                struct MKxRxPacketData *pPacket,
                                unsigned char *pPayload,
                                int PayloadLen,
                                int *PayloadByteErrors);

#endif // __IEEE1609__APP__TEST_MLME_IF__RXSTATS_H__
