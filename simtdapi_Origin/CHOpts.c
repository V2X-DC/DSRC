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
#if defined(__QNX__)
// QNX puts getopt in unistd (and doesn't have getopt_long)
#include <unistd.h>
#else
#include <getopt.h>
#include <linux/if_ether.h>
#endif
#include <arpa/inet.h>

#include "mkxtest.h"
#include "test-common.h"
#include "CHOpts.h"

// identifier this module for debugging
#define D_SUBMODULE TXAPP_Module
#include "debug-levels.h"

void CHOpts_PrintUsage()
{
  int i;
  static tUsageOption UsageOptionList[] =
  {
    {'h',"Help|Usage",   "Print this Usage",""},
    {'s',"Set",          "Set & Start specified Channel on the specified interface. Will write defaults if not specified", "no arg"},
    {'g',"Get",          "Get specified Channel on the specified interface",      "no arg"},
    {'x',"Stop",         "Stop the specified Channel on the specified interface", "no arg"},
    {'e',"Filter",       "List of EtherTypes to Pass",                            "uint16_t"},
    {'i',"Interface",    "Network interface to transmit on",                      "wave-raw | wave-mgmt"},
    {'w',"Channel",      "Logical Channel to transmit on",                        "CCH | SCH"},
    {'c',"ChannelNumber", "Channel Number",                                       "uint8"},
    {'m',"DefaultMCS",   "Default Modulation and Coding Scheme","MK2MCS_R12BPSK | MK2MCS_R34BPSK | MK2MCS_R12QPSK | MK2MCS_R34QPSK | MK2MCS_R12QAM16 | MK2MCS_R34QAM16 | MK2MCS_R23QAM64 | MK2MCS_R34QAM64 | MK2MCS_DEFAULT | MK2MCS_TRC"},
    {'p',"DefaultTxPower", "Default Tx Power level","uint16 (0.5 dBm steps)"},
    {'z',"DefaultTRC",   "Default to Transmit Rate Control?",                      "bool"},
    {'t',"DefaultTPC",   "Default to Transmit Power Control",                      "bool"},
    {'b',"BW",           "Bandwidth",                                              "MK2BW_10MHz | MK2BW_20MHz"},
    {'d',"DualTx",       "Tx Diversity Mode","MK2TXC_NONE | MK2TXC_TX | MK2TXC_RX | MK2TXC_TXRX"},
    {'a',"RxAnt",        "Which Antenna(s) to use for Receiving?",    "0,1,2,3"},
    {'u',"UtilPeriod",   "Channel Utilisation Period (ms)",           "uint8"},
    {'r',"Radio",        "Physical Radio to use",                     "a | b"},
    {'f',"MACAddr",      "MAC Address","aa:bb:cc:dd:ee:ff"},
    {0,0,0,0}
  };

  printf("usage: chconfig <Option>\n");
  i = 0;
  while (UsageOptionList[i].pName != NULL)
  {
    printf("-%c, ", UsageOptionList[i].Letter);
    printf("--%-s\n", UsageOptionList[i].pName);
    printf("        %s\n", UsageOptionList[i].pDescription);
    // Display any type info
    if (strlen(UsageOptionList[i].pValues) > 0)
      printf("        %s\n", UsageOptionList[i].pValues);
    i++;
  }

}

/**
 * @brief Print a CH Options Object. Makes use of inherited type printers.
 * @param Tx Options Object
 */
void CHOpts_Print (tCHOpts * pCHOpts)
{
  printf("Interface:         %s\n", pCHOpts->pInterfaceName);
  printf("Channel: ");
  switch (pCHOpts->Channel)
  {
    case MK2CHAN_CCH:
      printf("CCH\n");
      break;
    case MK2CHAN_SCH:
      printf("SCH\n");
      break;
    default:
      printf("Unknown\n");
      break;
  }
  printf("Radio: ");
  switch (pCHOpts->Radio)
  {
    case CHOPTS_RADIO_A:
      printf("A\n");
      break;
    case CHOPTS_RADIO_B:
      printf("B\n");
      break;
    default:
      printf("Unknown\n");
      break;
  }

  // Channel Profile
  MK2ChanProfile_Print(&(pCHOpts->ChanProfile), "");

  printf("Filter:            ");
  if (pCHOpts->NFilter > 0)
  {
    int i;
    for (i = 0; i < pCHOpts->NFilter; i++)
      printf("0x%04x%s", ntohs(pCHOpts->pFilter[i]), (i
             < (pCHOpts->NFilter - 1)) ? "," : "\n");
  }
  else
    printf("None\n");

  printf("MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
         pCHOpts->MACAddr[0], pCHOpts->MACAddr[1], pCHOpts->MACAddr[2],
         pCHOpts->MACAddr[3], pCHOpts->MACAddr[4], pCHOpts->MACAddr[5]);

}

/**
 * @brief Process the CHOpts command line arguments
 * @param argc the number of command line arguments
 * @param argv pointer to the command line options
 * @param pCHOpts pointer to the tCHOpts object to be filled
 *
 * pCHOpts pre-allocated
 *
 */

int CHOpts_New (int argc,
                char **argv,
                tCHOpts *pCHOpts)
{
  unsigned char mac[6] = { 0x04, 0xe5, 0x48, 0x00, 0x10, 0x00 };
  tMK2ChanProfile * pChanProfile; // and similarly for Chan Profile
  char short_options[100];
  tCHOptsErrCode ErrCode = CHOPTS_ERR_NONE;
  int c = 0; //
  char * pch; // used in splitting list args using strtok


  // Long Option specification
  // {Long Name, has arg?, Flag Pointer, Val to set if found}
#ifndef __QNX__
  int option_index = 0;
  static struct option long_options[] =
  {
    { "Help", no_argument, 0, 'h' }, // Usage
    { "help", no_argument, 0, 'h' }, // Usage
    { "Usage", no_argument, 0, 'h' }, // Usage
    { "Set", no_argument, 0, 's' }, // Set (overwrites with defs)
    { "Get", no_argument, 0, 'g' }, // Get
    { "Delete", no_argument, 0, 'x' }, // Delete (Stop the channel)
    { "Stop", no_argument, 0, 'x' }, // Delete (Stop the channel)
    { "Filter", required_argument, 0, 'e' }, // List of EtherTypes to pass
    { "Interface", required_argument, 0, 'i' }, // wave-raw or wave-mgmt
    { "Channel", required_argument, 0, 'w' }, // CCH or SCH
    { "ChannelNumber", required_argument, 0, 'c' }, // Channel Number
    { "DefaultMCS", required_argument, 0, 'm' },
    { "DefaultTxPower", required_argument, 0, 'p' },
    { "DefaultTRC", required_argument, 0, 'z' },
    { "DefaultTPC", required_argument, 0, 't' },
    { "BW", required_argument, 0, 'b' },
    { "DualTx", required_argument, 0, 'd' },
    { "RxAnt", required_argument, 0, 'a' }, // Rx on which antennas?
    { "UtilPeriod", required_argument, 0, 'u' },
    { "Radio", required_argument, 0, 'r' },
    { "MACAddr", required_argument, 0, 'f' },
    { 0, 0, 0, 0 }
  };
#endif

  // Default settings
  pCHOpts->NFilter = 0; // no filtering, all EtherTypes pass
  strcpy(pCHOpts->pInterfaceName, "wave-raw");
  pCHOpts->Channel = MK2CHAN_CCH;
  memcpy(pCHOpts->MACAddr, mac, ETH_ALEN);
  // IEEE Std 802 - Local Experimental Ethertype (the other is 88b6)
  pCHOpts->NFilter = 1;
  pCHOpts->pFilter[0] = (uint16_t) htons(0x88b5);

  //-----------------------------------------------------------
  // Channel Profile

  // Get the handle to the Chan Profile under this TxCHOpt
  pChanProfile = &(pCHOpts->ChanProfile);

  // Profile Defaults
  pChanProfile->ChannelNumber = 172;
  pChanProfile->DefaultMCS = MK2MCS_R12QPSK;
  pChanProfile->DefaultTxPower = (int16_t) 40; // 1/2 dBm steps
  pChanProfile->DefaultTRC = false;
  pChanProfile->DefaultTPC = false;
  pChanProfile->Bandwidth = MK2BW_10MHz;
  pChanProfile->DualTxControl = MK2TXC_NONE;
  pChanProfile->ChannelUtilisationPeriod = 49;
  pChanProfile->TxAntenna = MK2_TXANT_ANTENNA1AND2;
  pChanProfile->RxAntenna = MK2_RXANT_ANTENNA1AND2;
  memcpy(pChanProfile->MACAddr, pCHOpts->MACAddr, ETH_ALEN);

#if defined(__QNX__)
  strcpy(short_options, "hsgxe:i:w:c:m:p:z:t:b:d:a:u:r:f:");
#else
  // Build the short options from the Long
  copyopts(long_options, (char *) (&short_options));
#endif

  while (1)
  {
    ErrCode = CHOPTS_ERR_NONE; // set the error for this arg to none
#if defined(__QNX__)
    c = getopt(argc, argv, short_options);
#else
    c = getopt_long_only(argc, argv,
                         short_options, long_options,
                         &option_index);
    //printf("c:%c, option_index: %d, option arg: %s\n", c, option_index, optarg);
#endif
    if (c == -1)
      return 0;

    switch (c)
    {

      case 'h': // usage
        return -1;
        break;

      case 's': // Set
        pCHOpts->Command = MK2_CMD_SET;
        break;

      case 'g': // Get
        pCHOpts->Command = MK2_CMD_GET;
        break;

      case 'x': // /Stop/End/Delete
        pCHOpts->Command = MK2_CMD_DELETE;
        break;

      case 'e': // List of Filter EtherTypes
        pch = strtok(optarg, ",");
        pCHOpts->NFilter = 0;
        while ((pch != NULL) & (pCHOpts->NFilter < CHOPTS_MAXFILTERTABLELEN))
        {
          pCHOpts->pFilter[pCHOpts->NFilter]
          = (uint16_t) htons(strtoul(pch, NULL, 0));
          pCHOpts->NFilter++;
          pch = strtok(NULL, ","); // step to next token
        }
        break;
      case 'i': // Interface (raw of mgmt, could add data)
        if (strcmp("wave-raw", optarg) == 0)
          strcpy(pCHOpts->pInterfaceName, optarg);
        else if (strcmp("wave-mgmt", optarg) == 0)
          strcpy(pCHOpts->pInterfaceName, optarg);
        else
        {
          ErrCode = CHOPTS_ERR_INVALIDOPTIONARG;
          goto error;
        }
        break;
      case 'w': // Channel (CCH or SCH)
        if (strcmp("CCH", optarg) == 0)
        {
          pCHOpts->Channel = MK2CHAN_CCH;
        }
        else if (strcmp("SCH", optarg) == 0)
        {
          pCHOpts->Channel = MK2CHAN_SCH;
        }
        else
        {
          ErrCode = CHOPTS_ERR_INVALIDOPTIONARG;
          goto error;
        }
        break;

      case 'c': // Channel Number
        pChanProfile->ChannelNumber = strtoul(optarg, NULL, 0);
        break;
      case 'm': // Default MCS (List)
        if (strcmp("MK2MCS_R12BPSK", optarg) == 0)
          pChanProfile->DefaultMCS = MK2MCS_R12BPSK;
        else if (strcmp("MK2MCS_R34BPSK", optarg) == 0)
          pChanProfile->DefaultMCS = MK2MCS_R34BPSK;
        else if (strcmp("MK2MCS_R12QPSK", optarg) == 0)
          pChanProfile->DefaultMCS = MK2MCS_R12QPSK;
        else if (strcmp("MK2MCS_R34QPSK", optarg) == 0)
          pChanProfile->DefaultMCS = MK2MCS_R34QPSK;
        else if (strcmp("MK2MCS_R12QAM16", optarg) == 0)
          pChanProfile->DefaultMCS = MK2MCS_R12QAM16;
        else if (strcmp("MK2MCS_R34QAM16", optarg) == 0)
          pChanProfile->DefaultMCS = MK2MCS_R34QAM16;
        else if (strcmp("MK2MCS_R23QAM64", optarg) == 0)
          pChanProfile->DefaultMCS = MK2MCS_R23QAM64;
        else if (strcmp("MK2MCS_R34QAM64", optarg) == 0)
          pChanProfile->DefaultMCS = MK2MCS_R34QAM64;
        else if (strcmp("MK2MCS_TRC", optarg) == 0)
          pChanProfile->DefaultMCS = MK2MCS_TRC;
        else
        {
          ErrCode = CHOPTS_ERR_INVALIDOPTIONARG;
          goto error;
        }

        break;

      case 'p': // Default Tx Power
        pChanProfile->DefaultTxPower = (int16_t) strtol(optarg, NULL, 0);
        break;

      case 'z': // Default to Transmit Rate Control ?
        pChanProfile->DefaultTRC = (bool) strtoul(optarg, NULL, 0);
        break;

      case 't': // Default to Transmit Power Control ?
        pChanProfile->DefaultTPC = (bool) strtoul(optarg, NULL, 0);
        break;

      case 'b': // CCH Bandwidth (also parse 10 or 20)
        if ((strcmp("MK2BW_10MHz", optarg) == 0) ||
            (strtoul(optarg, NULL, 0) == 10))
          pChanProfile->Bandwidth = MK2BW_10MHz;
        else if ((strcmp("MK2BW_20MHz", optarg) == 0) ||
                 (strtoul(optarg, NULL, 0) == 20))
          pChanProfile->Bandwidth = MK2BW_20MHz;
        else
        {
          ErrCode = CHOPTS_ERR_INVALIDOPTIONARG;
          goto error;
        }
        break;

      case 'd': // Dual Tx Control
        if (strcmp("MK2TXC_NONE", optarg) == 0)
          pChanProfile->DualTxControl = MK2TXC_NONE;
        else if (strcmp("MK2TXC_TX", optarg) == 0)
          pChanProfile->DualTxControl = MK2TXC_TX;
        else if (strcmp("MK2TXC_RX", optarg) == 0)
          pChanProfile->DualTxControl = MK2TXC_RX;
        else if (strcmp("MK2TXC_TXRX", optarg) == 0)
          pChanProfile->DualTxControl = MK2TXC_TXRX;
        else
        {
          ErrCode = CHOPTS_ERR_INVALIDOPTIONARG;
          goto error;
        }
        break;

      case 'a': // Which Antennas to use for Rx
        pChanProfile->RxAntenna = (tMK2RxAntenna) strtoul(optarg, NULL, 0);
        break;

      case 'u': // Utilisation Period
        pChanProfile->ChannelUtilisationPeriod = strtoul(optarg, NULL, 0);
        break;

      case 'r':
        if ((strcmp("A", optarg) == 0) | (strcmp("a", optarg) == 0))
          pCHOpts->Radio = CHOPTS_RADIO_A;
        else if ((strcmp("B", optarg) == 0) | (strcmp("b", optarg) == 0))
          pCHOpts->Radio = CHOPTS_RADIO_B;
        else
        {
          ErrCode = CHOPTS_ERR_INVALIDOPTIONARG;
          goto error;
        }
        break;

      case 'f': // MAC Address
        if (!(GetMacFromString(pCHOpts->MACAddr, optarg)))
        {
          ErrCode = CHOPTS_ERR_INVALIDOPTIONARG;
          goto error;
        }
        memcpy(pChanProfile->MACAddr, pCHOpts->MACAddr, ETH_ALEN);
        break;

      default:
        ErrCode = CHOPTS_ERR_INVALIDOPTION;
        break;
    } // switch on argument

    if (ErrCode != CHOPTS_ERR_NONE)
      goto error;

  } // while getopt is not finished


error:
  if (ErrCode == CHOPTS_ERR_INVALIDOPTIONARG)
    fprintf(stderr, "invalid argument %s of option %s\n", optarg, argv[optind - 2]);
  if (ErrCode == CHOPTS_ERR_INVALIDOPTION)
    fprintf(stderr, "invalid option %s\n", argv[optind - 2]);
  return -1;

}

/**
 * @}
 */
