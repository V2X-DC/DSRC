/**
 * @addtogroup mk2mac_test MK2 MLME_IF test application(s)
 * @{
 *
 * @file
 * test-rx: Receive testing
 */

//------------------------------------------------------------------------------
// Copyright (c) 2010 Cohda Wireless Pty Ltd
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Included headers
//------------------------------------------------------------------------------
#include <string.h>
#include <netinet/in.h> // ntohs
//#include "mk2mac-api.h"
#include "linux/cohda/llc/llc-api.h"
#include "RxStats.h"
#include "mkxtest.h"
#include "test-common.h"

#define D_LOCAL D_WARN
#include "debug-levels.h"

//------------------------------------------------------------------------------
// Macros & Constants
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------

/**
 * @brief Create a new RxStats object
 * @param ppRxStats pointer to new handle of RxStats object
 * @return the Error Code
 */
tRxStatsErrCode RxStats_new (tRxStats ** ppRxStats)
{
  tRxStatsErrCode ErrCode = RXSTATS_ERR_NONE;
  tRxStats * pRxStats;

  d_fnstart(D_DEBUG, ppRxStats, "(ppRxStats %p)\n", ppRxStats);

  pRxStats = (tRxStats *) malloc(sizeof(tRxStats));
  if (pRxStats == NULL)
  {
    printf("Fail: pRxStats malloc()\n");
    ErrCode = RXSTATS_ERR_MALLOC;
    goto Error;
  }
  d_printf(D_DEBUG, ppRxStats, "Allocated pRxStats=%p\n", pRxStats);
  *ppRxStats = pRxStats; // set output pointer


  //-------------------------------------------------------------
  // Initiliase the Counters
  pRxStats->LastSeqNum = -1;
  pRxStats->NPayloadErrors = 0;
  pRxStats->NNotMatched = 0;
  pRxStats->NMatched = 0;

  // Initialise latency stats
  pRxStats->MaxUsec = 0;
  pRxStats->MinUsec = 0;
  pRxStats->AvgUsec = 0;
  pRxStats->TotalLatency = 0;
  memset(pRxStats->Hist, 0, RXSTATS_HIST_SIZE * sizeof(int));

  // Create a Buffer to generated desired Payloads
  pRxStats->pBuf = (unsigned char *) malloc(TEST_MAX_FRAMESIZE);
  if (pRxStats->pBuf == NULL)
  {
    printf("Fail: pBuf malloc()\n");
    ErrCode = RXSTATS_ERR_MALLOC;
    goto Error;
  }

  //-------------------------------------------------------------
  // allocate space for the histograms

  // Matched Histogram by Channel Number
  pRxStats->pNMatchedbyChannelNumber = (long *) \
                                       malloc(sizeof(long) * RXSTATS_MAXCHANNELS);
  if (pRxStats->pNMatchedbyChannelNumber == NULL)
  {
    printf("Fail: pNMatchedbyChannelNumber malloc()\n");
    ErrCode = RXSTATS_ERR_MALLOC;
    goto Error;
  }
  memset(pRxStats->pNMatchedbyChannelNumber, 0,
         RXSTATS_MAXCHANNELS * sizeof(long));

  // total number of entries required
  pRxStats->HistogramLength = RXSTATS_MAXMCS * RXSTATS_MAXLENGTHS;

  /// Matched Histogram by MCS and Packet Length
  pRxStats->pNMatchedHist = (long *) \
                            malloc(sizeof(long) * pRxStats->HistogramLength);
  if (pRxStats->pNMatchedHist == NULL)
  {
    printf("Fail: pNMatchedHist malloc()\n");
    ErrCode = RXSTATS_ERR_MALLOC;
    goto Error;
  }
  memset(pRxStats->pNMatchedHist, 0, pRxStats->HistogramLength * sizeof(long));

  /// Payload errors Histogram by MCS and Packet Length
  pRxStats->pNPayloadErrorsHist = (long *) \
                                  malloc(sizeof(long) * pRxStats->HistogramLength);
  if (pRxStats->pNPayloadErrorsHist == NULL)
  {
    printf("Fail: pNPayloadErrorsHist malloc()\n");
    ErrCode = RXSTATS_ERR_MALLOC;
    goto Error;
  }
  memset(pRxStats->pNPayloadErrorsHist, 0,
         pRxStats->HistogramLength * sizeof(long));

Error:
  d_fnend(D_DEBUG, ppRxStats, "(ppRxStats %p) = %d\n", ppRxStats, ErrCode);
  return ErrCode;
}

/**
 * @brief Destroy a RxStats object
 * @param pRxStats pointer to RxStats object
 * @return the Error Code
 */
tRxStatsErrCode RxStats_free (tRxStats * pRxStats)
{
  tRxStatsErrCode ErrCode = RXSTATS_ERR_NONE;

  d_fnstart(D_DEBUG, pRxStats, "(pRxStats %p)\n", pRxStats);

  if (pRxStats != NULL)
  {

    /// Buffer for storing Payload hypothesis to check against Rx
    if (pRxStats->pBuf != NULL)
    {
      free(pRxStats->pBuf);
    }

    /// Matched Histogram by Channel
    if (pRxStats->pNMatchedbyChannelNumber != NULL)
    {
      free(pRxStats->pNMatchedbyChannelNumber);
    }

    /// Matched Histogram by MCS and Packet Length
    if (pRxStats->pNMatchedHist != NULL)
    {
      free(pRxStats->pNMatchedHist);
    }

    /// Payload errors Histogram by MCS and Packet Length
    if (pRxStats->pNPayloadErrorsHist != NULL)
    {
      free(pRxStats->pNPayloadErrorsHist);
    }

    // Now free the top level object
    free(pRxStats);
  }

  d_fnend(D_DEBUG, pRxStats, "(pRxStats %p) = %d\n", pRxStats, ErrCode);

  return ErrCode;

}

/**
 * @brief Add a new packet to the Stats database
 * @param pRxDesc the RxDesc obtained from the packet
 * @param Cnt the number of relevant Bytes in the Buffer
 * @return the Error Code
 *
 * No differentiation based on Channel Number
 */
tRxStatsErrCode RxStats_addrxdesc (tRxStats * pRxStats,
                                   struct MKxRxPacketData *pPacket,
                                   int PayloadByteErrors,
                                   int PayloadLen)
{
  tRxStatsErrCode ErrCode = RXSTATS_ERR_NONE;
  long Index, MaxIndex; // indexing of flat histograms

  // total number of entries required
  MaxIndex = RXSTATS_MAXMCS * RXSTATS_MAXLENGTHS;

  d_assert(PayloadLen < RXSTATS_MAXLENGTHS);

  Index = pPacket->MCS * RXSTATS_MAXLENGTHS + PayloadLen;

  // Matched Histogram
  pRxStats->pNMatchedHist[Index]++;

  // Payload Errors Histogram
  pRxStats->pNPayloadErrorsHist[Index] += (PayloadByteErrors > 0);

  (void)MaxIndex;  // value is currently unused

  return ErrCode;

}

/**
 * @brief Analyse the Frame in the Rx buffer and update running stats
 * @param pRx the Rx object owning running stats
 * @param pRxDesc the RxDesc obtained from the packet
 * @param pRxOpts parameters of assessment criteria
 * @param pBuf the Buffer containing the Frame to be analysed
 * @param Cnt the number of relevant Bytes in the Buffer
 * @return the Error Code
 */
tRxStatsErrCode RxStats_Assess (tRxStats * pRxStats,
                                uint8_t ChannelNumber,
                                struct MKxRxPacketData *pPacket,
                                unsigned char *pPayload,
                                int PayloadLen,
                                int * PayloadByteErrors)
{
  tRxStatsErrCode ErrCode = RXSTATS_ERR_NONE;
  uint32_t ThisSeqNum; // The received SeqNum of the Payload
  tPayloadMode PayloadMode; // What kind of Payload to use

  d_assert(PayloadLen >= 6);

  // Packet Sequence number is first 4 Bytes
  ThisSeqNum = ntohl(*((uint32_t *) pPayload));
  d_printf(D_DEBUG, pRxStats, "SeqNum: 0x%08x\n", ThisSeqNum);

  // PayloadValue is 5th Byte (after SeqNum)
  // Assume not in error. If it is then error may be missed.
  PayloadMode = pPayload[4];
  d_printf(D_DEBUG, pRxStats, "PayloadMode: %d\n", PayloadMode);

  // check that SeqNum fits in int
  //d_assert(ThisSeqNum < 0x7FFFFFFF); // ~25 Days at 1000 frames/sec
  pRxStats->LastSeqNum = ThisSeqNum;

  // Histogram of received packets over variables (MCS, PacketLength & TxPower)
  pRxStats->pNMatchedbyChannelNumber[ChannelNumber]++;

  // Generate a local copy of what should have been received
  Payload_gen(pRxStats->pBuf, PayloadLen - 4, ThisSeqNum, PayloadMode,
              pPayload[5]);

  // Payload Errors (ignore 4 byte FCS at end)
  *PayloadByteErrors = 0;

  // do packet comparison in 2 parts, packet region before timestamp and
  // packet region after timestamp (to omit the timestamp)
  {
    int FirstLen = 0;
    int SecondLen = 0;
    int SecondStart = 0;
    int i; // Used to loop through Payload bytes

    if (PayloadMode == PAYLOADMODE_TIME)
    {
      // how much to test in the first loop
      if ( (PayloadLen - 4) < 8)
        FirstLen = PayloadLen - 4;
      else
        FirstLen = 8;

      // how much to test in the second loop
      SecondLen = PayloadLen - 4;

      // if length of packet doesnt go beyond the timestamp dont compare contents beyond it
      if ((PayloadLen - 4) < 17)
        SecondLen = 0;

      SecondStart = 16;
    }
    else
    {
      // normal comparison (no timestamp)
      FirstLen = PayloadLen - 4;
    }

    for (i = 0; i < FirstLen; i++)
    {
      //if (pPayload[i] != pRxStats->pBuf[i]) printf("%d != %d\n", pPayload[i], pRxStats->pBuf[i]);
      *PayloadByteErrors += (pPayload[i] != pRxStats->pBuf[i]);
    }
    for (i = SecondStart; i < SecondLen; i++)
    {
      //if (pPayload[i] != pRxStats->pBuf[i]) printf("%d != %d\n", pPayload[i], pRxStats->pBuf[i]);
      *PayloadByteErrors += (pPayload[i] != pRxStats->pBuf[i]);
    }
  }

  // If there were any Byte errors then there was a Packet payload error
  if ((*PayloadByteErrors) > 0)
    pRxStats->NPayloadErrors++;

  // Add this Rx Descriptor to the Histogram
  RxStats_addrxdesc(pRxStats, pPacket, *PayloadByteErrors, PayloadLen);

  return ErrCode;

}

/**
 * @brief Report on the Stats so far
 * @param fp File pointer to place output upon
 * @param pRx the Rx object owning running stats
 * @return the Error Code
 */
tRxStatsErrCode RxStats_Report (FILE * fp, tRxStats * pRxStats)
{
  tRxStatsErrCode ErrCode = RXSTATS_ERR_NONE;
  int c; // loop through channel numbers
  int NTxFrames, NMissedFrames, NPayloadErrors, NErrors;
  // error statistics for the last interval
  int NTxFramesLast, NMissedFramesLast, NPayloadErrorsLast, NErrorsLast;
  long MCS;
  long PayloadLen;
  long i;
  long TotalMatched, TotalPayloadErrors;
  bool HeadingPrinted = false;

  if (fp == NULL)
    goto Exit;

  // Display analysis
  NPayloadErrors = pRxStats->NPayloadErrors;
  NTxFrames = pRxStats->LastSeqNum + 1; // Estimate: start at SeqNum 0
  NMissedFrames = NTxFrames - pRxStats->NMatched;

  NErrors = NMissedFrames + NPayloadErrors;

  // How many new errors in this interval?
  NPayloadErrorsLast           = NPayloadErrors - pRxStats->NPayloadErrorsPrev;
  pRxStats->NPayloadErrorsPrev = NPayloadErrors;

  // same for number of frames received in the last interval
  NTxFramesLast           = NTxFrames - pRxStats->NTxFramesPrev;
  pRxStats->NTxFramesPrev = NTxFrames;
  // if no new frames have arrived in the last interval set the
  // frames counter to -1 to avoid a /0 below
  if (!NTxFramesLast)
  {
    NTxFramesLast = -1;
  }

  // ... and finally the number of missed frames
  NMissedFramesLast           = NMissedFrames - pRxStats->NMissedFramesPrev;
  pRxStats->NMissedFramesPrev = NMissedFrames;

  // total number of errors in the last interval
  NErrorsLast = NMissedFramesLast + NPayloadErrorsLast;

  fprintf(fp, "Rx: Report [Last SeqNum: %d (0x%08x)]\n", pRxStats->LastSeqNum,
          pRxStats->LastSeqNum);
  fprintf(
    fp,
    "Rx:   Approx PER: %.1f%% [Missed: %d, Payload Error: %d / Tx: %d] (%.1f%% [%d, %d / %d)\n",
    100.0 * (double) (NErrors) / ((double) (NTxFrames)), NMissedFrames,
    NPayloadErrors, NTxFrames,
    100.0 * (double) (NErrorsLast) / ((double) (NTxFramesLast)),
    NMissedFramesLast,    NPayloadErrorsLast, NTxFramesLast);

  // latency stats are updated if latency enabled
  if ( (pRxStats->MinUsec != 0) || (pRxStats->MaxUsec != 0) || (pRxStats->AvgUsec != 0) )
  {
    fprintf(fp, "Latency (usec): Min:%d, Max:%d, Avg:%d\n", pRxStats->MinUsec,
            pRxStats->MaxUsec, pRxStats->AvgUsec);
    fprintf(fp, "Latency (usec): %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
            pRxStats->Hist[0], pRxStats->Hist[1], pRxStats->Hist[2],
            pRxStats->Hist[3], pRxStats->Hist[4], pRxStats->Hist[5],
            pRxStats->Hist[6], pRxStats->Hist[7], pRxStats->Hist[8],
            pRxStats->Hist[9], pRxStats->Hist[10], pRxStats->Hist[11],
            pRxStats->Hist[12], pRxStats->Hist[13], pRxStats->Hist[14],
            pRxStats->Hist[15], pRxStats->Hist[16], pRxStats->Hist[17],
            pRxStats->Hist[18], pRxStats->Hist[19]);
  }
  fprintf(fp, "Rx:   Un/Matched Frames: %ld/%ld\n", pRxStats->NNotMatched,
          pRxStats->NMatched);

  //-----------------------------------------------------------------
  // Histogram of received packets over variables (Channel Number, MCS, PacketLength & TxPower)

  // Rx packets per channel
  for (c = 0; c < RXSTATS_MAXCHANNELS; c++)
  {
    if (pRxStats->pNMatchedbyChannelNumber[c] > 0)
      fprintf(fp, "Rx:   Channel %3d: %08ld Matched Packets.\n", c,
              pRxStats->pNMatchedbyChannelNumber[c]);
  }

  // search histograms for no zero entries.
  // If found map index to MCS and length then display.
  TotalMatched = 0;
  TotalPayloadErrors = 0;
  // Matched Histogram
  for (i = 0; i < pRxStats->HistogramLength; i++)
  {

    if (pRxStats->pNMatchedHist[i] > 0)
    {

      if (!HeadingPrinted)
      {
        fprintf(fp, "# %3s %4s %10s %10s\n", "MCS", "Len", "Matched",
                "Payload Err");
        HeadingPrinted = true;
      }

      // Index = pRxDesc->MCS * RXSTATS_MAXLENGTHS + PayloadLen;
      MCS = i / RXSTATS_MAXLENGTHS;
      PayloadLen = i - MCS * RXSTATS_MAXLENGTHS;

      fprintf(fp, "  %3ld %4ld %10ld %10ld\n", MCS, PayloadLen,
              pRxStats->pNMatchedHist[i], pRxStats->pNPayloadErrorsHist[i]);

      TotalMatched += pRxStats->pNMatchedHist[i];
      TotalPayloadErrors += pRxStats->pNPayloadErrorsHist[i];

    } // Any matched for this record
  } // Record number

  // show totals
  fprintf(fp, "  %8s %10ld %10ld\n", "Total", TotalMatched, TotalPayloadErrors);

  fflush(fp);

Exit:
  return ErrCode;

}

/**
 * @}
 */
