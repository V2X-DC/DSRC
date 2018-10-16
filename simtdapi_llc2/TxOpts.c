/**
 * @addtogroup mk2mac_test MK2 MLME_IF test application(s)
 * @{
 *
 * @file
 * test-tx: Channel configuration & Transmit testing
 */

//------------------------------------------------------------------------------
// Copyright (c) 2010 Cohda Wireless Pty Ltd
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Included headers
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#ifndef __QNX__
#include <getopt.h>
#include <linux/if_ether.h>
#endif

#include "mkxtest.h"
#include "TxOpts.h"

// identifier this module for debugging
#define D_SUBMODULE TXAPP_Module
#include "debug-levels.h"

typedef enum
{
  RANGESPEC_ERR_NONE = 0,
  RANGESPEC_ERR_DECREASING = 1
} eRangeSpecErrCode;
typedef uint8_t tRangeSpecErrCode;

/**
 * @brief Print a RangeSpec Object
 * @param pRangeSpec pointer to RangeSpec object to be printed
 *
 * displays as on of the following forms
 * A, A:B, A:B:C
 */
tRangeSpecErrCode RangeSpec_printf (tRangeSpec * pRangeSpec)
{
  tRangeSpecErrCode ErrCode = RANGESPEC_ERR_NONE;
  // Always a first value
  printf("%ld", pRangeSpec->Start); // start

  // Now decide if its A:B:C or A:B based on difference between Start and End
  if (pRangeSpec->Start != pRangeSpec->Stop)
  {
    // A:B or A:B:C
    if (pRangeSpec->Step == 1)
    {
      // A:B
      printf(":%ld\n", pRangeSpec->Stop); // Stop
    }
    else
    {
      // A:B:C
      printf(":%ld:%ld\n", pRangeSpec->Step, pRangeSpec->Stop); // Step:Stop
    }
  }
  else
  {
    printf("\n");
  } // any other constraints
  return ErrCode;
}

/**
 * @brief parse a string for a integer range specification
 * @param pStr null terminated string to parse for start:step:stop
 * @param pRangeSpec Start, Step and Stop fields (all long)
 * @return none
 *
 * String of form Start[:Step][:Stop]
 * e.g. 3
 *      5:9
 *      10:-2:1
 *
 */
tRangeSpecErrCode RangeSpec_Scan (char * pStr, tRangeSpec * pRangeSpec)
{
  tRangeSpecErrCode ErrCode = RANGESPEC_ERR_NONE;
  char * pch; // used in splitting list args using strtok
  char * saveptr = NULL;

  pch = strtok_r(pStr, ":", &saveptr);

  // Start
  pRangeSpec->Start = strtol(pch, NULL, 0);
  // to next token which is either Step or Stop
  pch = strtok_r(NULL, ":", &saveptr);
  if (pch != NULL)
  {
    // is 2nd empty?
    // Step or Stop
    long StepOrStop = strtol(pch, NULL, 0);
    // to next token
    // If none then previous was Stop
    pch = strtok_r(NULL, ":", &saveptr);
    if (pch != NULL)
    {
      // is 3rd empty?

      // A:B:C
      // StepOrStop is Step

      // ensure step is positivie
      if (StepOrStop <= 0)
        StepOrStop = 1;

      pRangeSpec->Step = StepOrStop;
      pRangeSpec->Stop = strtol(pch, NULL, 0);

    }
    else
    {

      // A:B => A:1:B
      pRangeSpec->Step = 1;
      pRangeSpec->Stop = StepOrStop;

    } // 3rd
  }
  else
  {
    // pch was NULL

    // A => A:1:A
    pRangeSpec->Step = 1;
    pRangeSpec->Stop = pRangeSpec->Start;

  } // 2nd

  // ensure Stop is greater than Start
  if (pRangeSpec->Stop < pRangeSpec->Start)
  {
    ErrCode = RANGESPEC_ERR_DECREASING;
    d_printf(D_ERR, NULL, "Range must be increasing");
  }

  d_printf(D_DEBUG, NULL, "%ld:%ld:%ld\n", pRangeSpec->Start, pRangeSpec->Step,
           pRangeSpec->Stop);
  return ErrCode;
}

/**
 * @brief Print a Tx Channel Options Object
 * @param pTxCHOpts pointer to Tx Channel Options Object
 * @param IndentStr each line pre-fixed with this string
 */
void TxCHOpts_Print (tTxCHOpts * pTxCHOpts,
                     char *IndentStr)
{
  int i; // index into lists

  //------------------------------------------------
  // Tx Descriptor lists
  /// Indicate the channel number that should be used (e.g. 172)
  /// Indicate the priority to used
  printf("%s", IndentStr);
  printf("Priority: %d\n", pTxCHOpts->Priority);
  /// Indicate the 802.11 Service Class to use
  printf("%s", IndentStr);
  printf("Service: %d\n", pTxCHOpts->Service);
  /// Indicate the MCS to be used (may specify default or TRC)
  for (i = 0; i < pTxCHOpts->NMCS; i++)
  {
    printf("%s", IndentStr);
    printf("MCS[%d]: %d\n", i, pTxCHOpts->pMCS[i]);
  }
  printf("%s", IndentStr);
  printf("NMCS: %d\n", pTxCHOpts->NMCS);
  /// What Tx Power mode is to be used ( manual, default or TPC)
  printf("%s", IndentStr);
  printf("TxPwrCtrl: %d\n", pTxCHOpts->TxPwrCtrl);
  /// Indicate the power to be used in Manual mode
  printf("%s", IndentStr);
  printf("TxPower: ");
  RangeSpec_printf(&(pTxCHOpts->TxPower));
  /// Indicate the antenna upon which packet should be transmitted
  printf("%s", IndentStr);
  for (i = 0; i < pTxCHOpts->NTxAnt; i++)
  {
    printf("%s", IndentStr);
    printf("TxAntenna[%d]: %d\n", i, pTxCHOpts->pTxAntenna[i]);
  }
  printf("%s", IndentStr);
  printf("NTxAnt: %d\n", pTxCHOpts->NTxAnt);
  /// Indicate the expiry time (0 means never)
  printf("%s", IndentStr);
  printf("Expiry: %llu\n", pTxCHOpts->Expiry);
  /// Packet length in Bytes
  for (i = 0; i < pTxCHOpts->NPacketLengths; i++)
  {
    printf("%s", IndentStr);
    printf("PayloadLength[%d]: ", i);
    RangeSpec_printf(&(pTxCHOpts->PacketLength[i]));
  }
  /// DA to use in test packets to WAVE-RAW
  printf("%s", IndentStr);
  printf("Destination MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
         pTxCHOpts->DestAddr[0], pTxCHOpts->DestAddr[1],
         pTxCHOpts->DestAddr[2], pTxCHOpts->DestAddr[3],
         pTxCHOpts->DestAddr[4], pTxCHOpts->DestAddr[5]);
  /// Ethernet Type to use in Ethernet Header input to WAVE_RAW
  printf("%s", IndentStr);
  printf("EtherType: 0x%04x\n", pTxCHOpts->EtherType);
  // Channel Numbers
  printf("ChannelNumber:     %d\n", pTxCHOpts->ChannelNumber);

}

void TxOpts_PrintUsage ()
{
  int i;
  static tUsageOption UsageOptionList[] =
  {
    /// @fixme: copypasta, see TxCmd llc-test-tx.c  and TxOpts_New below
    { 'h', "Help|Usage",     "Print this Usage", "" },
    { 'f', "LogFile",        "Log File Name", "string" },
    { 's', "SrcAddr",        "Source MAC Address", "aa:bb:cc:dd:ee:ff" },
    { 'n', "NPackets",       "Total Number of Packet to Tx", "uint32" },
    { 'r', "PacketRate",     "Packet Rate in Packets Per Second", "positive float" },
    { 'y', "DumpPayload",    "Dump Packet Payload to log file", "" },
    { 'o', "DumpToStdout",   "Dump packets to Standard output", "" },
    { 'g', "PayloadMode",    "Method used to set Payload bytes",
      "seqnum | increment | random | uint8_t | time"},
    { 'i', "Interface",      "Network interface to transmit on","wave-raw | wave-mgmt"},
    { 't', "TZSP",           "Forward TZSP encapsulated packets?", "" },
    { 'z', "TZSP_Port",      "Listen for TZSP Packets on this Port Number", "uint16"},
    { 'c', "ChannelNumber",  "Attempt to Tx on this Channel Number", "uint8"},
    { 'l', "PktLen",         "Packet Length (Bytes) excluding TxDesc and EtherHeader",
      "List of Start[:Step][:End], uint16 >=50"},
    { 'm', "MCS",            "Modulation and Coding Scheme",
      "List of MK2MCS_R12BPSK | MK2MCS_R34BPSK | MK2MCS_R12QPSK | MK2MCS_R34QPSK | MK2MCS_R12QAM16 | MK2MCS_R34QAM16 | MK2MCS_R23QAM64 | MK2MCS_R34QAM64 | MK2MCS_DEFAULT | MK2MCS_TRC"},
    { 'w', "TxPwrCtrl",      "Tx Power Control Mode","MK2TPC_MANUAL | MK2TPC_DEFAULT | MK2TPC_TPC"},
    { 'p', "Power",          "Tx Power (Half dBm)", "Start[:Step][:End]" },
    { 'x', "Expiry",         "Expiry (us)", "uint64" },
    { 'q', "Priority",       "Priority",
      "MK2_PRIO_0 | MK2_PRIO_1 | MK2_PRIO_2 | MK2_PRIO_3 | MK2_PRIO_4 | MK2_PRIO_5 | MK2_PRIO_6 | MK2_PRIO_7 | MK2_PRIO_NON_QOS"},
    { 'v', "Service", "802.11 Service Class","MK2_QOS_ACK | MK2_QOS_NOACK"},
    { 'a', "TxAnt",          "Tx Antenna to use",
      "List of MK2_TXANT_DEFAULT | MK2_TXANT_ANTENNA1 | MK2_TXANT_ANTENNA2 | MK2_TXANT_ANTENNA1AND2"},
    { 'e', "EtherType",      "Ethernet Type", "uint16" },
    { 'd', "DstAddr",        "Destination MAC Address", "aa:bb:cc:dd:ee:ff" },
    { 'u', "UDPforwardPort", "UDP forwarding port", "12345" },
    { 0, 0, 0, 0 }
  };

  printf("test-tx <Option>\n");
  i = 0;
  while (UsageOptionList[i].pName != 0)
  {
    printf("-%c, ", UsageOptionList[i].Letter);
    printf("--%-s\n", UsageOptionList[i].pName);
    printf("        %s\n", UsageOptionList[i].pDescription);
    // Display any type info
    if (strlen(UsageOptionList[i].pValues) > 0)
    {
      printf("        %s\n", UsageOptionList[i].pValues);
    }
    i++;
  }

}

/**
 * @brief Print a Tx Options Object. Makes use of inherited type printers.
 * @param Tx Options Object
 */
void TxOpts_Print (tTxOpts * pTxOpts)
{

  printf("Mode:             ");
  switch (pTxOpts->Mode)
  {
    case TXOPTS_MODE_CREATE:
      printf("CREATE\n");
      printf("Number Of Packets:  %d\n", pTxOpts->NumberOfPackets);
      printf("Target Packet Rate: %.2f\n", pTxOpts->TargetPacketRate_pps);
      TxCHOpts_Print(&(pTxOpts->TxCHOpts), "");
      printf("PayloadMode:       ");
      switch (pTxOpts->PayloadMode)
      {
        case PAYLOADMODE_SEQNUM:
          printf("seqnum\n");
          break;
        case PAYLOADMODE_RANDOM:
          printf("random\n");
          break;
        case PAYLOADMODE_INCREMENT:
          printf("increment\n");
          break;
        case PAYLOADMODE_TIME:
          printf("time\n");
          break;
        case PAYLOADMODE_BYTE:
          printf("Byte 0x%02x\n", pTxOpts->PayloadByte);
          break;
        default:
          printf("Error\n");
          break;
      } // Payload Type switch
      break;
    case TXOPTS_MODE_TZSPFWD:
      printf("TZSP Forward to Port %d\n", pTxOpts->TZSPPort);
      break;
    default:
      printf("Unknown\n");
      break;
  }

  printf("Source MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
         pTxOpts->SrcAddr[0], pTxOpts->SrcAddr[1], pTxOpts->SrcAddr[2],
         pTxOpts->SrcAddr[3], pTxOpts->SrcAddr[4], pTxOpts->SrcAddr[5]);
  printf("Packet Log File:   %s\n", pTxOpts->pPacketLogFileName);
  printf("DumpPayload:       %d\n", pTxOpts->DumpPayload);
  printf("DumpToStdout:      %d\n", pTxOpts->DumpToStdout);

  printf("Interface:         %s\n", pTxOpts->pInterfaceName);

}

/**
 * @brief Process the Tx command line arguments
 * @param argc the number of command line arguments
 * @param argv pointer to the command line options
 * @param pTxOpts pointer to the tTxOpts object to be filled
 *
 * Example call
 * ./test-tx \
 * --DumpPayload \
 * --DumpToStdout \
 * --SrcAddr 01:04:07:0a:0d:11 \
 * --NPackets 1234 \
 * --PacketRate 10.5 \
 * --Interface wave-raw \
 * --PktLen 100:10:300 \
 * --MCS 0,1,2,3,4,5,6 \
 * --TxPwrCtrl MK2TPC_MANUAL \
 * --Power 20:10:50 \
 * --Expiry 0 \
 * --Priority MK2_PRIO_4 \
 * --Service MK2_QOS_NOACK \
 * --TxAnt MK2_TXANT_ANTENNA1 \
 * --EtherType 0x88d5 \
 * --DstAddr 04:05:06:07:08:09 \
 * --UDPforwardPort 12345 \
 *
 * or
 * ./test-tx \
 *   --TZSP \
 *   --TZSP_Port 37008
 *
 */
int TxOpts_New (int argc, char **argv, tTxOpts *pTxOpts)
{
  unsigned char src_mac[6] = { 0x04, 0xe5, 0x48, 0x00, 0x10, 0x00 };
  unsigned char dst_mac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  tTxCHOpts * pTxCHOpts; // used to hlve arg handling by parsing C/S CH
  char short_options[100];
  char * saveptr = NULL; // copy token to here for procssing
  tTxOptsErrCode ErrCode = TXOPTS_ERR_NONE;
  tRangeSpecErrCode RangeSpecErrCode = RANGESPEC_ERR_NONE;
  int c = 0; //
  char * pch; // used in splitting list args using strtok
  int IntVal = -1; // used to store integer version of input value


#ifndef __QNX__
  int option_index = 0;
  // Long Option specification
  // {Long Name, has arg?, Flag Pointer, Val to set if found}
  static struct option long_options[] =
  {
    { "SrcAddr",        required_argument, 0, 's' }, //
    { "NPackets",       required_argument, 0, 'n' }, //
    { "PacketRate",     required_argument, 0, 'r' }, // target packet rate
    { "LogFile",        required_argument, 0, 'f' }, // Log file name
    { "Help",           no_argument,       0, 'h' }, // Usage
    { "Usage",          no_argument,       0, 'h' }, // Usage
    { "DumpPayload",    no_argument,       0, 'y' }, // Dump Packet payload
    { "DumpToStdout",   no_argument,       0, 'o' }, // Dump to stdout?
    { "PayloadMode",    required_argument, 0, 'g' }, // What kind of Payload?
    { "Interface",      required_argument, 0, 'i' }, // wave-raw or wave-mgmt
    { "TZSP",           no_argument,       0, 't' }, // create or forward packets
    { "TZSP_Port",      required_argument, 0, 'z' }, // port to forward to
    { "ChannelNumber",  required_argument, 0, 'c' }, // Tx Chan Num
    { "PktLen",         required_argument, 0, 'l' }, // PktLen Range (List)
    { "MCS",            required_argument, 0, 'm' }, // List
    { "TxPwrCtrl",      required_argument, 0, 'w' },
    { "Power",          required_argument, 0, 'p' }, // Tx Power Range (List)
    { "Expiry",         required_argument, 0, 'x' },
    { "Priority",       required_argument, 0, 'q' },
    { "Service",        required_argument, 0, 'v' },
    { "TxAnt",          required_argument, 0, 'a' }, // List
    { "EtherType",      required_argument, 0, 'e' },
    { "DstAddr",        required_argument, 0, 'd' },
    { "UDPforwardPort", required_argument, 0, 'u' },
    { 0, 0, 0, 0 }
  };
#endif

  // Default settings

  //-----------------------------------------------------------
  // Non CH specific

  // Mode
  pTxOpts->Mode = TXOPTS_MODE_CREATE;
  pTxOpts->TZSPPort = TXOPTS_TZSPPORT;

  // Common Source address irrespective of channel
  memcpy(pTxOpts->SrcAddr, src_mac, ETH_ALEN);
  // Total number of packets to Tx then exit
  pTxOpts->NumberOfPackets = 100;
  strcpy(pTxOpts->pInterfaceName, "wave-raw");
  //strcpy(pTxOpts->pPacketLogFileName, "TxLog.txt");
  pTxOpts->pPacketLogFileName[0] = 0; // none
  pTxOpts->TargetPacketRate_pps = 10;
  pTxOpts->DumpPayload = false; // set true if option present
  // Dump entire packet payload to file
  pTxOpts->DumpToStdout = false; // true if option present
  pTxOpts->PayloadMode = PAYLOADMODE_BYTE;
  // disable UDP forward by default
  pTxOpts->UDPforwardPort = 0;

  // CCH Tx Descriptor Lists
  pTxOpts->TxCHOpts.ChannelNumber = 178;//TEST_DEFAULT_CHANNELNUMBER;
  pTxOpts->TxCHOpts.PacketLength[0].Start = 100;
  pTxOpts->TxCHOpts.PacketLength[0].Step = 1; // no range
  pTxOpts->TxCHOpts.PacketLength[0].Stop = 100;
  pTxOpts->TxCHOpts.NPacketLengths = 1;
  pTxOpts->TxCHOpts.pMCS[0] = MK2MCS_R12BPSK;
  pTxOpts->TxCHOpts.NMCS = 1;
  pTxOpts->TxCHOpts.TxPwrCtrl = MK2TPC_DEFAULT; // use system default power by default (ie via chconfig)
  pTxOpts->TxCHOpts.TxPower.Start = 40;
  pTxOpts->TxCHOpts.TxPower.Step = 1; // no range
  pTxOpts->TxCHOpts.TxPower.Stop = 40;

  // CCH Tx Descriptor Vals
  pTxOpts->TxCHOpts.Expiry = 0; //0x0123456789ABCDEFULL;
  pTxOpts->TxCHOpts.Priority = MK2_PRIO_4;
  pTxOpts->TxCHOpts.Service = MK2_QOS_NOACK;
  pTxOpts->TxCHOpts.pTxAntenna[0] = MK2_TXANT_DEFAULT;
  pTxOpts->TxCHOpts.NTxAnt = 1;
  // IEEE Std 802 - Local Experimental Ethertype 2
  pTxOpts->TxCHOpts.EtherType = 0x88b5;
  memcpy(pTxOpts->TxCHOpts.DestAddr, dst_mac, ETH_ALEN);
  pTxOpts->TxCHOpts.UDPforwardSocket = -1;

#if defined(__QNX__)
  strcpy(short_options, "s:n:r:f:hyog:i:tz:c:l:m:w:p:x:q:v:a:e:d:");
#else
  // Build the short options from the Long
  copyopts(long_options, (char *) (&short_options));
#endif

  while (1)
  {
    ErrCode = TXOPTS_ERR_NONE; // set the error for this arg to none
#if defined(__QNX__)
    c = getopt(argc, argv, short_options);
#else
    c
      = getopt_long_only(argc, argv, short_options, long_options,
                         &option_index);
#endif
    if (c == -1)
      return 0;

    // get pointer to CCH or SCH TxCHOpt
    pTxCHOpts = &(pTxOpts->TxCHOpts);

    switch (c)
    {
      case 's': // Ethernet Header Source address

        if (!(GetMacFromString(pTxOpts->SrcAddr, optarg)))
        {
          ErrCode = TXOPTS_ERR_INVALIDOPTIONARG;
          goto error;
        }

        break;
      case 'n': // Number of Packets
        pTxOpts->NumberOfPackets = strtoul(optarg, NULL, 0);
        break;

      case 'r': // Target packet rate

        sscanf(optarg, "%40f", &(pTxOpts->TargetPacketRate_pps));
        // following doesn't work: maybe not null-terminated (by strtok)?
        //pTxOpts->TargetPacketRate_pps = strtof(optarg, NULL);

        break;

      case 'f': // Packet Log File Name
        strcpy(pTxOpts->pPacketLogFileName, optarg);
        break;

      case 'h': // usage
        return -1;
        break;

      case 'y': // dump payload? Yes if present
        pTxOpts->DumpPayload = true;
        break;

      case 'o': // Dump entire packet payload to file? Yes if present
        pTxOpts->DumpToStdout = true;
        break;

      case 'g': // Payload fill mode
        if (strncmp("random", optarg, 6) == 0)
          pTxOpts->PayloadMode = PAYLOADMODE_RANDOM;
        else if (strncmp("increm", optarg, 6) == 0)
          pTxOpts->PayloadMode = PAYLOADMODE_INCREMENT;
        else if (strncmp("time", optarg, 6) == 0)
          pTxOpts->PayloadMode = PAYLOADMODE_TIME;
        else if (strncmp("seqnum", optarg, 6) == 0) // copy the seqnum
          pTxOpts->PayloadMode = PAYLOADMODE_SEQNUM;
        else
        {
          // use this value as the payload (1 Byte)
          pTxOpts->PayloadMode = PAYLOADMODE_BYTE;
          pTxOpts->PayloadByte = (uint8_t) strtoul(optarg, NULL, 0);
        }
        break;

      case 'i': // Interface (raw of mgmt, could add data)
        if (strcmp("wave-raw", optarg) == 0)
          strcpy(pTxOpts->pInterfaceName, optarg);
        else if (strcmp("wave-mgmt", optarg) == 0)
          strcpy(pTxOpts->pInterfaceName, optarg);
        else
        {
          ErrCode = TXOPTS_ERR_INVALIDOPTIONARG;
          goto error;
        }
        break;

      case 't': // Forward tzsp encapsulated packets?
        pTxOpts->Mode = TXOPTS_MODE_TZSPFWD;
        break;
      case 'z': // listen for tzsp encapsulated packets on this port
        pTxOpts->TZSPPort = strtoul(optarg, NULL, 0);
        break;

      case 'c': // Channel Number to Tx on
        pTxCHOpts->ChannelNumber = strtoul(optarg, NULL, 0);
        break;

      case 'l': // Packet Length (Range)
        pch = strtok_r(optarg, ",", &saveptr); // break into ranges
        pTxCHOpts->NPacketLengths = 0;
        // parse each range
        while ((pch != NULL) & (pTxCHOpts->NPacketLengths
                                < TXOPTS_MAXOPTARGLISTLEN))
        {
          // parse range an increment number found in list
          RangeSpecErrCode
          = RangeSpec_Scan(
              pch,
              &(pTxCHOpts->PacketLength[pTxCHOpts->NPacketLengths++]));
          if (RangeSpecErrCode != RANGESPEC_ERR_NONE)
            return -1;
          // step to next token
          pch = strtok_r(NULL, ",", &saveptr);
        } // each range in list
        break;
      case 'm': // MCS (List)
        pch = strtok(optarg, ",");
        pTxCHOpts->NMCS = 0;
        while ((pch != NULL) & (pTxCHOpts->NMCS < TXOPTS_MAXOPTARGLISTLEN))
        {
          // try and parse MCS values as just integers first
          char *pEnd = NULL;
          unsigned long int MCS;
          // save original value of NMCS so we can know if we successfully
          // parsed one at the end
          int NMCS = pTxCHOpts->NMCS;

          MCS = strtoul(pch, &pEnd, 10);

          // if we parsed as an integer which matches rate use this
          // otherwise look for mcs in the string parameter
#define TRY_PARSE_MCS(mcs, rate)                                        \
          do {                                                          \
            if ((pEnd != pch && MCS == rate) ||                         \
                strstr(pch, #mcs) != NULL)                              \
              pTxCHOpts->pMCS[pTxCHOpts->NMCS++] = MK2MCS_##mcs;        \
          } while (0)

          TRY_PARSE_MCS(R12BPSK, 3);
          TRY_PARSE_MCS(R34BPSK, 4);
          TRY_PARSE_MCS(R12QPSK, 6);
          TRY_PARSE_MCS(R34QPSK, 9);
          TRY_PARSE_MCS(R12QAM16, 12);
          TRY_PARSE_MCS(R34QAM16, 18);
          TRY_PARSE_MCS(R23QAM64, 24);
          TRY_PARSE_MCS(R34QAM64, 27);
          TRY_PARSE_MCS(DEFAULT, 0);
          TRY_PARSE_MCS(TRC, 1);

#undef TRY_PARSE_MCS

          if (pTxCHOpts->NMCS == NMCS)
          {
            ErrCode = TXOPTS_ERR_INVALIDOPTIONARG;
            goto error;
          }

          // step to next token
          pch = strtok(NULL, ",");

        }

        break;
      case 'w': // Tx Power Control
        if (strcmp("MK2TPC_MANUAL", optarg) == 0)
          pTxCHOpts->TxPwrCtrl = MK2TPC_MANUAL;
        else if (strcmp("MK2TPC_DEFAULT", optarg) == 0)
          pTxCHOpts->TxPwrCtrl = MK2TPC_DEFAULT;
        else if (strcmp("MK2TPC_TPC", optarg) == 0)
          pTxCHOpts->TxPwrCtrl = MK2TPC_TPC;
        else
        {
          ErrCode = TXOPTS_ERR_INVALIDOPTIONARG;
          goto error;
        }

        break;
      case 'p': // Tx Power (Range)
        RangeSpecErrCode = RangeSpec_Scan(optarg, &(pTxCHOpts->TxPower));
        if (RangeSpecErrCode != RANGESPEC_ERR_NONE)
          return -1;
        // if -p specified, we force manual power control mode
        pTxCHOpts->TxPwrCtrl = MK2TPC_MANUAL;
        break;
      case 'x': // Expiry
        pTxCHOpts->Expiry = strtoull(optarg, NULL, 0);
        break;
      case 'q': // Priority
        if (strcmp("MK2_PRIO_0", optarg) == 0)
          pTxCHOpts->Priority = MK2_PRIO_0;
        else if (strcmp("MK2_PRIO_1", optarg) == 0)
          pTxCHOpts->Priority = MK2_PRIO_1;
        else if (strcmp("MK2_PRIO_2", optarg) == 0)
          pTxCHOpts->Priority = MK2_PRIO_2;
        else if (strcmp("MK2_PRIO_3", optarg) == 0)
          pTxCHOpts->Priority = MK2_PRIO_3;
        else if (strcmp("MK2_PRIO_4", optarg) == 0)
          pTxCHOpts->Priority = MK2_PRIO_4;
        else if (strcmp("MK2_PRIO_5", optarg) == 0)
          pTxCHOpts->Priority = MK2_PRIO_5;
        else if (strcmp("MK2_PRIO_6", optarg) == 0)
          pTxCHOpts->Priority = MK2_PRIO_6;
        else if (strcmp("MK2_PRIO_7", optarg) == 0)
          pTxCHOpts->Priority = MK2_PRIO_7;
        else if (strcmp("MK2_PRIO_NON_QOS", optarg) == 0)
          pTxCHOpts->Priority = MK2_PRIO_NON_QOS;
        else
        {
          ErrCode = TXOPTS_ERR_INVALIDOPTIONARG;
          goto error;
        }

        break;
      case 'v':// Service

        if (strcmp("MK2_QOS_ACK", optarg) == 0)
          pTxCHOpts->Service = MK2_QOS_ACK;
        else if (strcmp("MK2_QOS_NOACK", optarg) == 0)
          pTxCHOpts->Service = MK2_QOS_NOACK;
        else
        {
          ErrCode = TXOPTS_ERR_INVALIDOPTIONARG;
          goto error;
        }
        break;

      case 'a': // TxAnt
        pch = strtok(optarg, ",");
        pTxCHOpts->NTxAnt = 0;

        while ((pch != NULL) & (pTxCHOpts->NTxAnt < TXOPTS_MAXOPTARGLISTLEN))
        {

          // Handle case where integer is supplied, Non int to -1
          IntVal = -1; // Invalid
          if (strlen(pch) == 1)
            IntVal = strtoul(pch, NULL, 0);

          if ((strcmp("MK2_TXANT_DEFAULT", pch) == 0) || (IntVal == 0))
            pTxCHOpts->pTxAntenna[pTxCHOpts->NTxAnt++] = MK2_TXANT_DEFAULT;
          else if ((strcmp("MK2_TXANT_ANTENNA1", pch) == 0) || (IntVal == 1))
            pTxCHOpts->pTxAntenna[pTxCHOpts->NTxAnt++] = MK2_TXANT_ANTENNA1;
          else if ((strcmp("MK2_TXANT_ANTENNA2", pch) == 0) || (IntVal == 2))
            pTxCHOpts->pTxAntenna[pTxCHOpts->NTxAnt++] = MK2_TXANT_ANTENNA2;
          else if ((strcmp("MK2_TXANT_ANTENNA1AND2", pch) == 0)
                   || (IntVal == 3))
            pTxCHOpts->pTxAntenna[pTxCHOpts->NTxAnt++] = MK2_TXANT_ANTENNA1AND2;
          else
          {
            ErrCode = TXOPTS_ERR_INVALIDOPTIONARG;
            goto error;
          }
          // step to next token
          pch = strtok(NULL, ",");
        }
        break;
      case 'e': // Ether Type

        pTxCHOpts->EtherType = strtoul(optarg, NULL, 0);
        break;

      case 'd': // Destination Address
        if (!(GetMacFromString(pTxCHOpts->DestAddr, optarg)))
        {
          ErrCode = TXOPTS_ERR_INVALIDOPTIONARG;
          goto error;
        }

        break;

      case 'u': // UDP forwarding
      {
        pTxOpts->UDPforwardPort = strtoul(optarg, NULL, 0);
      } break;

      default:
        ErrCode = TXOPTS_ERR_INVALIDOPTION;
        break;
    } // switch on argument

    if (ErrCode != TXOPTS_ERR_NONE)
      goto error;

  } // while getopt is not finished


error:
  if (ErrCode == TXOPTS_ERR_INVALIDOPTIONARG)
    fprintf(stderr, "invalid argument %s of option %s\n", optarg, argv[optind - 2]);
  if (ErrCode == TXOPTS_ERR_INVALIDOPTION)
    fprintf(stderr, "invalid option %s\n", argv[optind - 2]);
  if (ErrCode == TXOPTS_ERR_PACKETLENGTHTOOSHORT)
    fprintf(stderr, "Packet Length too short.\n");
  return -1;

}

/**
 * @}
 */
