/**
 * @addtogroup cohda_llc_intern_app LLC application
 * @{
 *
 * @section cohda_llc_intern_app_dbg LLC 'chconfig' plugin command
 *
 * @verbatim
    MKx channel config
    Usage: llc chconfig
   @endverbatim
 *
 * @file
 * LLC: 'cw-llc' monitoring application ('chconfig' command support)
 *
 */

//------------------------------------------------------------------------------
// Copyright (c) 2015 Cohda Wireless Pty Ltd
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Included headers
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <math.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "linux/cohda/llc/llc.h"
#include "linux/cohda/llc/llc-api.h"
#include "dot4-internal.h"
#include "mk2mac-api-types.h"
#include "test-common.h"
#include "llc-plugin.h"
#include "debug-levels.h"
#include "TxOpts.h"

// this is defined via endian.h except on the 12.04 VM
#ifndef htobe16
uint16_t htobe16(uint16_t host_16bits);
#endif
#ifndef htole16
uint16_t htole16(uint16_t host_16bits);
#endif

//------------------------------------------------------------------------------
// Macros & Constants
//------------------------------------------------------------------------------
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define MALLOC_SIZE_MKxTxPacket 4096

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
// Function Prototypes
//------------------------------------------------------------------------------

static int LLC_TxCmd (struct PluginCmd *pCmd, int Argc, char **ppArgv);
static int LLC_TxMain (int Argc, char **ppArgv);

//------------------------------------------------------------------------------
// Variables
//------------------------------------------------------------------------------

/// Plugin support structure(s)
struct PluginCmd TxCmd =
{
  .pName = "test-tx",
  /// @fixme: copypasta, see TxOpts_PrintUsage and TxOpts_New in TxOpts.c
  .pHelp = (
    "test-tx <Option>\n"
    "-h, --Help|Usage\n"
    "        Print this Usage\n"
    "-f, --LogFile\n"
    "        Log File Name\n"
    "        string\n"
    "-s, --SrcAddr\n"
    "        Source MAC Address\n"
    "        aa:bb:cc:dd:ee:ff\n"
    "-n, --NPackets\n"
    "        Total Number of Packet to Tx\n"
    "        uint32\n"
    "-r, --PacketRate\n"
    "        Packet Rate in Packets Per Second\n"
    "        positive float\n"
    "-y, --DumpPayload\n"
    "        Dump Packet Payload to log file\n"
    "-o, --DumpToStdout\n"
    "        Dump packets to Standard output\n"
    "-g, --PayloadMode\n"
    "        Method used to set Payload bytes\n"
    "        seqnum | increment | random | uint8_t | timed\n"
    "-i, --Interface\n"
    "        Network interface to transmit on\n"
    "        wave-raw | wave-mgmt\n"
    "-t, --TZSP\n"
    "        Forward TZSP encapsulated packets?\n"
    "-z, --TZSP_Port\n"
    "        Listen for TZSP Packets on this Port Number\n"
    "        uint16\n"
    "-c, --ChannelNumber\n"
    "        Attempt to Tx on this Channel Number\n"
    "        uint8\n"
    "-l, --PktLen\n"
    "        Packet Length (Bytes) excluding TxDesc and EtherHeader\n"
    "        List of Start[:Step][:End], uint16 >=50\n"
    "-m, --MCS\n"
    "        Modulation and Coding Scheme\n"
    "        List of MK2MCS_R12BPSK | MK2MCS_R34BPSK | MK2MCS_R12QPSK | MK2MCS_R34QPS\n"
    "K | MK2MCS_R12QAM16 | MK2MCS_R34QAM16 | MK2MCS_R23QAM64 | MK2MCS_R34QAM64 | MK2M\n"
    "CS_DEFAULT | MK2MCS_TRC\n"
    "-w, --TxPwrCtrl\n"
    "        Tx Power Control Mode\n"
    "        MK2TPC_MANUAL | MK2TPC_DEFAULT | MK2TPC_TPC\n"
    "-p, --Power\n"
    "        Tx Power (Half dBm)\n"
    "        Start[:Step][:End]\n"
    "-x, --Expiry\n"
    "        Expiry (us)\n"
    "        uint64\n"
    "-q, --Priority\n"
    "        Priority\n"
    "        MK2_PRIO_0 | MK2_PRIO_1 | MK2_PRIO_2 | MK2_PRIO_3 | MK2_PRIO_4 | MK2_PRIO_5 | MK2_PRIO_6 | MK2_PRIO_7\n"
    "-v, --Service\n"
    "        802.11 Service Class\n"
    "        MK2_QOS_ACK | MK2_QOS_NOACK\n"
    "-a, --TxAnt\n"
    "        Tx Antenna to use\n"
    "        List of MK2_TXANT_DEFAULT | MK2_TXANT_ANTENNA1 | MK2_TXANT_ANTENNA2 | MK2_TXANT_ANTENNA1AND2\n"
    "-e, --EtherType\n"
    "        Ethernet Type\n"
    "        uint16\n"
    "-d, --DstAddr\n"
    "        Destination MAC Address\n"
    "        aa:bb:cc:dd:ee:ff\n"
    "-u, --UDPforwardPort\n"
    "        UDP forwarding port\n"
    "        12345\n"),
  .Func = LLC_TxCmd,
};

/// App/command state
static struct LLCTx _Dev =
{
  .pMKx = NULL,
  .Exit = false,
};
static struct LLCTx *pDev = &(_Dev);


//------------------------------------------------------------------------------
// Functions definitions
//------------------------------------------------------------------------------

/**
 * @brief Called by the llc app when the 'dbg' command is specified
 */
static int LLC_TxCmd (struct PluginCmd *pCmd,
                       int Argc,
                       char **ppArgv)
{
  int Res;

  d_printf(D_DBG, NULL, "Argc=%d, ppArgv[0]=%s\n", Argc, ppArgv[0]);

  Res = LLC_TxMain(Argc, ppArgv);
  if (Res < 0)
    d_printf(D_WARN, NULL, "%d (%s)\n", Res, strerror(-Res));

  return Res;
}

/**
 * @brief Print capture specific usage info
 */
static int LLC_TxUsage (void)
{
  Plugin_CmdPrintUsage(&TxCmd);
  return -EINVAL;
}

/**
 * @brief Signal Handler
 * @param SigNum the signal caught
 *
 * Set the exit flag so that application can exit following next
 * main loop iteration.
 *
 */
static void LLC_TxSignal (int SigNum)
{
  d_printf(D_TST, NULL, "Signal %d!\n", SigNum);

  pDev->TxContinue = false;
  // Break the MKx_Recv() loops
  (void)MKx_Recv(NULL);
}

/**
 * @brief Print an MK2TxDescriptor
 * @param pMK2TxDesc the Descriptor to display
 *
 * prints headings if NULL
 */
static void TxPacket_fprintf (FILE * fp, struct MKxTxPacketData *pPacket)
{

  uint64_t Expiry_Seconds, Expiry_uSeconds;

  if (pPacket == NULL)
  {
    // write headings
    /// Indicate the channel number that frame was received on
    fprintf(fp, "%3s ", "ChN");
    /// Indicate the priority allocated to the received packet (by Tx)
    fprintf(fp, "%3s ", "Pri");
    /// Indicate the 802.11 service class used to transmit the packet
    fprintf(fp, "%3s ", "Srv");
    /// Indicate the data rate that was used
    fprintf(fp, "%2s ", "MC");
    /// Indicate the power to be used (may specify default or TPC)
    fprintf(fp, "%6s ", "P(dBm)");
    /// Indicate the antenna upon which packet should be transmitted
    fprintf(fp, "%2s ", "An");
    /// Indicate the expiry time (0 means never)
    fprintf(fp, "%21s ", "Expiry (s)");
  }
  else
  {
    /// Indicate the channel number that frame was received on
    // fprintf(fp, "%03d ", pPacket->ChannelNumber);
    fprintf(fp, "N/A ");
    /// Indicate the priority allocated to the received packet (by Tx)
    // fprintf(fp, "%03d ", pMK2TxDesc->Priority);
    fprintf(fp, "N/A ");
    /// Indicate the 802.11 service class used to transmit the packet
    // fprintf(fp, "%03d ", pMK2TxDesc->Service);
    fprintf(fp, "N/A ");
    /// Indicate the data rate that was used
    fprintf(fp, "%02d ", pPacket->MCS);
    /// Indicate the power to be used (may specify default or TPC)
    fprintf(fp, "%6.1f ", pPacket->TxPower * 0.5);
    /// Indicate the antenna upon which packet should be transmitted
    fprintf(fp, "%02d ", pPacket->TxAntenna);
    /// Indicate the expiry time (0 means never)
    Expiry_Seconds = pPacket->Expiry / 1000000;
    Expiry_uSeconds = pPacket->Expiry % 1000000;
    fprintf(fp, "%014llu.", Expiry_Seconds);
    fprintf(fp, "%06llu ", Expiry_uSeconds);
  }
}

/**
 * @brief Write the info in packet Buffer to Packet log
 * @param fp File Pointer (open)
 * @param pBuf the Buffer containing the Frame to be logged
 * @param FrameLen the number of relevant Bytes in the Buffer
 * @param DumpPayload Should the entire payload be dumped to file
 * @param DumpHeadings dump headings in addition to data?
 * @return Error Code
 *
 * Buffer includes RxDescriptor, Ethernet header, Payload and FCS
 *
 */
static tTxErrCode Tx_fprintf_waveraw (FILE * fp,
                                      struct MKxTxPacketData *pPacket,
                                      int FrameLen,
                                      bool DumpPayload,
                                      bool DumpHeadings)
{
  tTxErrCode ErrCode = TX_ERR_NONE;
  struct ethhdr *pEthHdr; // pointer into Buf at Eth Hdr
  unsigned char *pPayload; // pointer in Buf at Payload (contains SeqNum)
  int PayloadLen; // Payload Length in Bytes

  if (fp == NULL)
  {
    printf("Fail: Tx_WriteToLog NULL FILE pointer\n");
    ErrCode = TX_ERR_NULLFILEPOINTER;
    return ErrCode;
  }

  if (pPacket == NULL)
  {
    printf("Fail: Tx_WriteToLog NULL Buffer\n");
    ErrCode = TX_ERR_NULLBUFFER;
    return ErrCode;
  }

  //--------------------------------------------------------------------------
  // WAVE-RAW frame: | TxDesc | Eth Header | Protocol & Payload |
  pEthHdr = (struct ethhdr *)pPacket->TxFrame;
  pPayload = (unsigned char *) (pPacket->TxFrame + sizeof(struct ethhdr));

  // now that we have pointer to structs, dump to file line

  // every now an then write a comment with column labels
  if ((DumpHeadings) || (fp == stdout))
  {
    fprintf(fp, "#   SeqNum ");
    TxPacket_fprintf(fp, NULL);
    EthHdr_fprintf(fp, NULL);
    Payload_fprintf(fp, NULL, 0, 0); // last two args ingnored
    fprintf(fp, "\n"); // end this packet line
  }

  // SeqNum is first 4 Bytes of Payload
  fprintf(fp, "%010d ", ntohl(*((uint32_t *) pPayload)));

  // Tx Descriptor has its own dumper
  TxPacket_fprintf(fp, pPacket);
  EthHdr_fprintf(fp, pEthHdr);

  // calc Payload Length
  PayloadLen = FrameLen - sizeof(struct ethhdr);

  Payload_fprintf(fp, pPayload, PayloadLen, DumpPayload);

  fprintf(fp, "\n"); // end this packet line

  return ErrCode;
}

static tTxErrCode Tx_fprintf (FILE * fp,
                              struct MKxTxPacketData *pPacket,
                              int FrameLen,
                              bool DumpPayload,
                              bool DumpHeadings)
{
  tTxErrCode ErrCode = TX_ERR_NONE;

  if (fp == NULL)
    return ErrCode;

  ErrCode = Tx_fprintf_waveraw(fp, pPacket, FrameLen, DumpPayload,
                               DumpHeadings);
  return ErrCode;

}

/**
 * @brief Transmit frame(s) on the opened interface using the CCH or SCH config
 * @param pTxOpts the options used to config the channel for sending
 * @param Pause_us wait this long inline with tx Loop
 * @return the number of bytes sent (-1 is failure)
 *
 * Loops through several variables transmitting several packets in a single call.
 * Loops though
 *   - Packet Length
 *   - Tx Power
 *   - MCS
 *
 * Source Address already loaded into tTx object
 */
static int Tx_Send (tTxOpts * pTxOpts, int Pause_us)
{
  tTxErrCode ErrCode = TX_ERR_NONE;

  int m, r, a; // loop vars
  int ThisFrameLen;
  struct IEEE80211QoSHeader *pMAC;
  struct SNAPHeader *pSNAP;

  unsigned char *pPayload;
  bool DumpHeadings; // in Packet Log file and/or to screen
  long TxPower;
  tRangeSpec *pTxPowerRangeSpec;
  long PayloadLen;
  tRangeSpec *pPayloadLenRangeSpec;
  tTxCHOpts *pTxCHOpts;
  tMKxRadio RadioID;
  tMKxChannel ChannelID;
  int Freq;
  struct MKxTxPacket *pTxPacket = malloc(MALLOC_SIZE_MKxTxPacket);
  struct MKxTxPacketData *pPacket = &pTxPacket->TxPacketData;
  fMKx_TxReq LLC_TxReq = pDev->pMKx->API.Functions.TxReq;
  const tMKxRadioConfigData *pRadio = pDev->pMKx->Config.Radio;

  d_fnstart(D_DEBUG, pDev, "(pDev %p, pTxOpts %p, Pause_us %d)\n", pDev, pTxOpts,
            Pause_us);
  d_assert(pTxOpts != NULL);

  memset(pTxPacket, 0, MALLOC_SIZE_MKxTxPacket);

  // Get existing handles from Tx Object
  pTxCHOpts = &(pTxOpts->TxCHOpts);
  Freq = pTxCHOpts->ChannelNumber * 5 + 5000;

  d_assert(pTxCHOpts != NULL);

  //--------------------------------------------------------------------------
  // WAVE-RAW frame: | TxDesc | Eth Header | Protocol & Payload |
  pMAC = (struct IEEE80211QoSHeader *)pPacket->TxFrame;
  pSNAP = (struct SNAPHeader *) (pPacket->TxFrame + sizeof(*pMAC));
  pPayload = (unsigned char *) (pPacket->TxFrame + sizeof(*pMAC) + sizeof(*pSNAP));

  // Setup the 802.11 MAC QoS header
  pMAC->FrameControl.FrameCtrl = 0;
  pMAC->FrameControl.Fields.Type = MAC_FRAME_TYPE_DATA;
  pMAC->FrameControl.Fields.SubType = MAC_FRAME_SUB_TYPE_QOS_DATA;
  cpu_to_le16s(&(pMAC->FrameControl.FrameCtrl));
  //htons(&(pMAC->FrameControl.FrameCtrl));
  pMAC->DurationId = 0x0000;
  cpu_to_le16s(&(pMAC->DurationId));
  //htons(&(pMAC->DurationId));
  memcpy(pMAC->Address1, pTxCHOpts->DestAddr, ETH_ALEN);
  memcpy(pMAC->Address2, pDev->EthHdr.h_source, ETH_ALEN);
  memset(pMAC->Address3, 0xFF, ETH_ALEN);
  pMAC->SeqControl.SeqCtrl = 0xfffe; // Set by the UpperMAC
  pMAC->QoSControl.QoSCtrl = 0x0000;
  pMAC->QoSControl.Fields.TID = pTxCHOpts->Priority;
  pMAC->QoSControl.Fields.EOSP = 0;
  pMAC->QoSControl.Fields.AckPolicy = pTxCHOpts->Service;
  pMAC->QoSControl.Fields.TXOPorQueue = 0;
  cpu_to_le16s(&pMAC->QoSControl.QoSCtrl);

  // Setup the SNAP header
  pSNAP->DSAP = SNAP_HEADER_DSAP;
  pSNAP->SSAP = SNAP_HEADER_SSAP;
  pSNAP->Control = SNAP_HEADER_CONTROL;
  pSNAP->OUI[0] = 0x00;
  pSNAP->OUI[1] = 0x00;
  pSNAP->OUI[2] = 0x00;
  pSNAP->Type = pTxCHOpts->EtherType;
  cpu_to_be16s(&pSNAP->Type);

  // Get loops specs for RangeSpec variants
  pTxPowerRangeSpec = &(pTxCHOpts->TxPower);

  if (Freq == pRadio[MKX_RADIO_A].ChanConfig[MKX_CHANNEL_0].PHY.ChannelFreq)
  {
    RadioID = MKX_RADIO_A;
    ChannelID = MKX_CHANNEL_0;
  }
  else if (Freq == pRadio[MKX_RADIO_B].ChanConfig[MKX_CHANNEL_0].PHY.ChannelFreq)
  {
    RadioID = MKX_RADIO_B;
    ChannelID = MKX_CHANNEL_0;
  }
  else if (Freq == pRadio[MKX_RADIO_A].ChanConfig[MKX_CHANNEL_1].PHY.ChannelFreq)
  {
    RadioID = MKX_RADIO_A;
    ChannelID = MKX_CHANNEL_1;
  }
  else if (Freq == pRadio[MKX_RADIO_B].ChanConfig[MKX_CHANNEL_1].PHY.ChannelFreq)
  {
    RadioID = MKX_RADIO_B;
    ChannelID = MKX_CHANNEL_1;
  }
  else
  {
    ErrCode = TX_ERR_INVALIDOPTIONARG;
    goto Error;
  }

  // MCS Loop
  for (m = 0; m < pTxCHOpts->NMCS; m++)
  {

    // TxPower Loop through Range
    for (TxPower = pTxPowerRangeSpec->Start; TxPower <= pTxPowerRangeSpec->Stop; TxPower
         += pTxPowerRangeSpec->Step)
    {

      // each range in PackLength list of ranges
      for (r = 0; r < pTxCHOpts->NPacketLengths; r++)
      {
        pPayloadLenRangeSpec = &(pTxCHOpts->PacketLength[r]);

        // Payload Length Loop though Range
        for (PayloadLen = pPayloadLenRangeSpec->Start; PayloadLen
             <= pPayloadLenRangeSpec->Stop; PayloadLen
             += pPayloadLenRangeSpec->Step)
        {

          // Antenna Loop
          for (a = 0; a < pTxCHOpts->NTxAnt; a++)
          {

            // Setup the Mk2Descriptor
            pPacket->RadioID = RadioID;
            pPacket->ChannelID = ChannelID;
            // pPacket->Priority = pTxCHOpts->Priority;
            // pPacket->Service = pTxCHOpts->Service;
            //pPacket->TxPower.PowerSetting = pTxCHOpts->TxPwrCtrl;
            pPacket->TxAntenna = pTxCHOpts->pTxAntenna[a]; // List
            pPacket->Expiry = pTxCHOpts->Expiry;
            pPacket->TxCtrlFlags = 0;
            pPacket->MCS = pTxCHOpts->pMCS[m]; // List
            pPacket->TxPower = (tMK2Power) TxPower; // from long


            size_t overhead = (char*)pPayload - (char*)(pPacket->TxFrame);

            if (pTxCHOpts->UDPforwardSocket >= 0)
            {
                // allow plenty of space for headers etc.
                size_t Size = MALLOC_SIZE_MKxTxPacket - 512;

                if (pTxOpts->UDPforwardPort > 0)
                {
                    fprintf(stderr, "UDPforward payload...\n");
                    PayloadLen = read(pTxCHOpts->UDPforwardSocket, pPayload, Size);
                }
                else
                {
                    fprintf(stderr, "UDPforward 802.11...\n");
                    // This allow messing with the 802.11 header as well
                    PayloadLen = read(pTxCHOpts->UDPforwardSocket, pPacket->TxFrame, Size);
                    overhead = 0;
                }
                fprintf(stderr, "read(%d,%p,%u): %ld\n",
                        pTxCHOpts->UDPforwardSocket, pPayload, Size, PayloadLen);
            }
            else
            {
                // write Payload Bytes according to mode (PayloadValue)
                Payload_gen(pPayload, PayloadLen, pDev->SeqNum,
                            pTxOpts->PayloadMode, pTxOpts->PayloadByte);

            }

            // add any header overhead that we may have incurred
            ThisFrameLen = overhead + PayloadLen;

            pPacket->TxFrameLength = ThisFrameLen;

            // Now send the packet
            ErrCode = LLC_TxReq(pDev->pMKx, pTxPacket, pDev);
            if (ErrCode)
            {
              fprintf(stderr, "LLC_TxReq %s (%d)\n", strerror(ErrCode), ErrCode);
            }
            else
            {

              if (pTxCHOpts->UDPforwardSocket >= 0)
              {
                fprintf(stderr, "pPacket->TxFrameLength = %d\n", pPacket->TxFrameLength);
                int i = 0;
                for (; i < pPacket->TxFrameLength;)
                {
                    fprintf(stderr, "[%03x]", i);
                    int j = min(pPacket->TxFrameLength, i+32);
                    for (; i < j; ++i)
                    {
                        fprintf(stderr, " %02X", (uint8_t)pPacket->TxFrame[i]);
                    }
                    fprintf(stderr, "\n");
                }
              }

              // record this packet in Packet Log file as it has now been Txed
              DumpHeadings = (pDev->SeqNum % 20) == 0; // every now & then dump a heading

              Tx_fprintf(pDev->fp, pPacket, ThisFrameLen,
                         pTxOpts->DumpPayload, DumpHeadings);

              // also to screen?
              if (pTxOpts->DumpToStdout == true)
              {
                Tx_fprintf(stdout, pPacket, ThisFrameLen,
                           pTxOpts->DumpPayload, DumpHeadings);
              }

              (pDev->SeqNum)++; // increment unique ID of packets
            }
            // Wait a little whle before next transmission (attempt to set tx rate)
//            usleep(Pause_us);
          } // Tx Antenna List Loop
        } // Frame Length Loop
      } // Range in PacketLength list
    } // Tx Power Loop
  } // MCS Loop

  free(pTxPacket);

  d_fnend(D_DEBUG, pDev, "(pDev %p) = %d\n", pDev, ErrCode);

Error:
  return ErrCode;
} // end of function

/**
 * @brief Rate controlled send of N packets on CCH and SCH
 * @param pTx pointer to Tx Object owning MLME_IF handle and Config
 * @param pTxOpts the options used to config the channels for sending
 * @return Error Code
 *
 * Will transmit at least pTxOpts->NumberOfPackets.
 * Will typically stop just past this number of packets.
 */

static int Tx_SendAtRate(tTxOpts * pTxOpts)
{
  tTxErrCode ErrCode = TX_ERR_NONE;
  int Pause_us;
  uint32_t RefSeqNum;
  struct timeval t0, t1;
  struct timeval *pt0, *pt1, *ptswap;
  float ThisPeriod_s;
  float ExtraPause_s;
  float ThisRate_pps;
  float TargetRate_pps;
  float OneOnTargetRate;
  /// check and control packet rate this many times per second
  float CheckRate_cps = 1;
  int NPacketsPerCheck; // number of packets between Rate control calcs

  d_assert(pTxOpts->TargetPacketRate_pps > 0);

  // Get dereferenced copy of target rate
  TargetRate_pps = pTxOpts->TargetPacketRate_pps;

  // precompute target period
  OneOnTargetRate = 1.0 / pTxOpts->TargetPacketRate_pps;

  // calculate initial estimate of pause assuming processing takes zero time
  Pause_us = OneOnTargetRate * 1e6;

  // check 2twice a second Half the number of packets per second
  NPacketsPerCheck = (int) (TargetRate_pps / CheckRate_cps);

  // pointer the timeval pointers at the stack memory
  pt0 = &t0;
  pt1 = &t1;
  // initialise the time snapshots and packet number
  gettimeofday(pt0, NULL);
  RefSeqNum = pDev->SeqNum; // Last Update of rate control at this seqnum


  // Continue until signal or transmitted enough packets
  pDev->TxContinue = true;
  while ( (pDev->SeqNum < pTxOpts->NumberOfPackets) && (pDev->TxContinue) )
  {
    int ThisNPackets;

    // Send the CCH or SCH packets with the new spacing
    Tx_Send(pTxOpts, Pause_us);

    // try receive to get MKx_TxCnf callback called
    MKx_Recv(pDev->pMKx);

    // how many packets have been transmitted since the last update?
    ThisNPackets = (pDev->SeqNum - RefSeqNum);

    // if many then update rate control
    if (ThisNPackets >= NPacketsPerCheck)
    {

      // Dump the stats from the MLME
      //Tx_DumpStats (stdout, pTx);

      // Now calc private stats
      // how much time has elapsed ?
      // should we adjust the pause to acheive the desired rate?

      gettimeofday(pt1, NULL);
      ThisPeriod_s = (pt1->tv_sec - pt0->tv_sec)
                     + (pt1->tv_usec - pt0->tv_usec) * 1e-6;

      ThisRate_pps = ThisNPackets / ThisPeriod_s; // only calculate for display

      ExtraPause_s = OneOnTargetRate - 1 / ThisRate_pps; //stable when R * N = T

      fprintf(
        stdout,
        "Tx: Last SeqNum: %10d [/%d]. Packet rate: Current %7.1f, Target %7.1f\n",
        pDev->SeqNum, pTxOpts->NumberOfPackets, ThisRate_pps,
        TargetRate_pps);

      d_printf(D_DEBUG, pDev, "Pause Update: %d / %.1f - %.6f = %.6f\n",
               ThisNPackets, TargetRate_pps, ThisPeriod_s, ExtraPause_s);

      Pause_us += (int) (ExtraPause_s * 5e5); // only feedback half of error

      d_printf(D_DEBUG, pDev, "Pause: %d us\n", Pause_us);

      // can't go any faster
      if (Pause_us < 0)
        Pause_us = 0;

      // flip timing pointers and counter
      RefSeqNum = pDev->SeqNum;
      ptswap = pt1;
      pt1 = pt0;
      pt0 = ptswap;
    } // rate control

  } // number of packets


  return ErrCode;

}

int LLC_OpenUDP(uint16_t Port)
{
  // cut-and-paste coding at it's finest...
  // #include stack/src/plat/util/udp.c

  d_fnstart(D_TST, NULL, "(%d)\n", Port);

  if (!Port)
  {
    d_fnend(D_TST, NULL, "(%d): disabled\n", Port);
    return -1;
  }

  /// Raw socket
  int Fd = socket(PF_INET6, SOCK_DGRAM, 0);
  if (Fd < 0)
  {
    // oops
    d_fnend(D_ERR, NULL, "socket(PF_INET6, SOCK_DGRAM, 0) failed: %d, %s\n",
            errno, strerror(errno));
    return -1;
  }

  // Use a "dual stack" socket that listens on IPv4 and IPv6
  int flag = 0;
  int rc = setsockopt(Fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&flag, sizeof(flag));
  if (rc < 0)
  {
    d_fnend(D_ERR, NULL, "setsockopt(%d, IPPROTO_IPV6, IPV6_V6ONLY, 0) failed: %d %s\n",
            Fd, errno, strerror(errno));
    close(Fd);
    return -1;
  }

  // Set socket flags for address reuse
  flag = 1;
  rc = setsockopt(Fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
  if (rc < 0)
  {
    d_error(D_ERR, NULL, "setsockopt(%d, SOL_SOCKET, SO_REUSEADDR, 1) failed: %d %s\n",
            Fd, errno, strerror(errno));
    close(Fd);
    return -1;
  }

  struct sockaddr_in6 addr = {0,};
  addr.sin6_family = AF_INET6;
  addr.sin6_addr = in6addr_any;
  addr.sin6_port = htons(Port);

  rc = bind(Fd, (struct sockaddr *)&addr, sizeof(addr));
  if(rc)
  {
    // oops
    d_fnend(D_ERR, NULL, "bind(%d, %d) failed: %d, %s\n",
            Fd, Port, errno, strerror(errno));
    close(Fd);
    return -1;
  }
  d_fnend(D_TST, NULL, "(%d): %d\n",
          Port, Fd);
  return Fd;
}

tMKxStatus LLC_TxCnf (struct MKx *pMKx,
                      tMKxTxPacket *pTxPkt,
                      const tMKxTxEvent *pTxEvent,
                      void *pPriv)
{
  int Res = MKXSTATUS_SUCCESS;
  tMKxStatus Result = pTxEvent->Hdr.Ret == MKXSTATUS_SUCCESS ?
    pTxEvent->TxEventData.TxStatus :
    pTxEvent->Hdr.Ret;

  d_printf(Result == MKXSTATUS_SUCCESS ? D_INFO : D_WARN, NULL,
           "TxCnf[%u]: %s [%d]\n", pTxEvent->TxEventData.MACSequenceNumber,
           (Result == MKXSTATUS_FAILURE_INTERNAL_ERROR ? "Internal error" :
            (Result == MKXSTATUS_TX_FAIL_TTL ? "Fail TTL" :
             (Result == MKXSTATUS_TX_FAIL_RETRIES ? "Fail Retries" :
              (Result == MKXSTATUS_TX_FAIL_QUEUEFULL ? "Fail Queue Full" :
               (Result == MKXSTATUS_TX_FAIL_RADIO_NOT_PRESENT ? "Fail Radio Not Present" :
                (Result == MKXSTATUS_TX_FAIL_MALFORMED ? "Fail Malfomed Frame" : "Unknown error")))))),
           Result);
  return Res;
}

/**
 * @brief Initiate the 'dbg' command
 */
int LLC_TxInit (struct LLCTx *pDev)
{
  int Res;
  tTxOpts * pTxOpts = &pDev->TxOpts;

  d_fnstart(D_TST, NULL, "()\n");

  // setup the signal handlers to exit gracefully
  d_printf(D_DBG, NULL, "Signal handlers\n");
  signal(SIGINT,  LLC_TxSignal);
  signal(SIGTERM, LLC_TxSignal);
  signal(SIGQUIT, LLC_TxSignal);
  signal(SIGHUP,  LLC_TxSignal);
  signal(SIGPIPE, LLC_TxSignal);

  Res = MKx_Init(&(pDev->pMKx));
  if (Res < 0)
    goto Error;
  pDev->Fd = MKx_Fd(pDev->pMKx);
  if (pDev->Fd < 0)
    goto Error;

  Res = TxOpts_New(pDev->Args.Argc, pDev->Args.ppArgv, pTxOpts);
  if (Res)
    goto Error;

  TxOpts_Print(pTxOpts);
  pDev->Fd = -1; // File description invalid
  pDev->SeqNum = 0;
  pDev->fp = NULL;

  // PreLoad the Ethernet Header (used in RAW frames)
  memcpy(pDev->EthHdr.h_source, pDev->TxOpts.SrcAddr, ETH_ALEN); // SA

  // Open a handle for the packet log file
  if (pTxOpts->pPacketLogFileName[0] != 0)
  {
    pDev->fp = fopen(pTxOpts->pPacketLogFileName, "w");
    if (pDev->fp == NULL)
    {
      printf("Fail: fopen(%s) errno %d\n", pTxOpts->pPacketLogFileName, errno);
    }
    else
    {
      d_printf(D_INFO, pDev, "Opened %s for logging (Handle %p)\n",
               pTxOpts->pPacketLogFileName, pDev->fp);
    }
  }

  // Optionally open UDP forwarding handle
  pTxOpts->TxCHOpts.UDPforwardSocket = LLC_OpenUDP(abs(pTxOpts->UDPforwardPort));
  d_printf(D_TST, NULL, "LLC_OpenUDP(%u): %d\n",
          pTxOpts->UDPforwardPort, pTxOpts->TxCHOpts.UDPforwardSocket);

  pDev->TxContinue = true;

  pDev->pMKx->API.Callbacks.TxCnf = LLC_TxCnf;
  pDev->pMKx->pPriv = (void *)pDev;

Error:
  if (Res != 0)
    LLC_TxUsage();

  d_fnend(D_TST, NULL, "() = %d\n", Res);
  return Res;
}

/**
 * @brief Cancel any 'dbg' command(s)
 */
int LLC_TxExit (struct LLCTx *pDev)
{
  int Res = -ENOSYS;

  d_fnstart(D_TST, NULL, "()\n");
  d_assert(pDev != NULL);

  // Break the MKx_Recv() loops
  (void)MKx_Recv(NULL);

  if (pDev->pMKx != NULL)
    MKx_Exit(pDev->pMKx);
  Res = 0;

  d_fnend(D_TST, NULL, "() = %d\n", Res);
  return Res;
}

/**
 * @brief Application entry point
 * @param argc Number of command line arguments
 * @param argv Array of command line arguments
 * @return Zero if successful, otherwise a negative errno
 *
 * Main application
 * - Initialise data variables
 * - Parse user option command line variables
 * - Initialise interfaces
 * - Use poll() to wait for some input from interfaces
 * - Perform actions on each input action
 * - De-initialise interfaces when Exit condition met
 *
 */
static int LLC_TxMain (int Argc, char **ppArgv)
{
  int Res;
  tTxOpts *pTxOpts;

  d_fnstart(D_TST, NULL, "()\n");

  pDev->Args.Argc = Argc;
  pDev->Args.ppArgv = ppArgv;

  // Initial actions
  Res = LLC_TxInit(pDev);
  if (Res < 0)
    goto Error;

  pTxOpts = &pDev->TxOpts;
  switch (pTxOpts->Mode)
  {
    case TXOPTS_MODE_CREATE:
      // Packets are transmitted on the CCH or the SCH via the raw socket to the
      // wave-raw interface, using sendmsg socket function call.
      Tx_SendAtRate(pTxOpts);
      break;
    case TXOPTS_MODE_TZSPFWD:
      // Listen for TSZP encapsulated packets in the specified port
      // and forward them to the wave if.
      //Tx_Forward(pTx, pTxOpts);
      break;
    default:
      break;
  }

Error:
  // Final actions
  LLC_TxExit(pDev);

  d_fnend(D_TST, NULL, "() = %d\n", Res);
  return Res;
}

/**
 * @}
 */
