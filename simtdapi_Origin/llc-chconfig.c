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
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <math.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include "linux/cohda/llc/llc.h"
#include "linux/cohda/llc/llc-api.h"
#include "mk2mac-api-types.h"
#include "test-common.h"
#include "llc-plugin.h"
#include "debug-levels.h"
#include "CHOpts.h"

//------------------------------------------------------------------------------
// Macros & Constants
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Type definitions
//------------------------------------------------------------------------------

/// Application/command state structure
typedef struct LLCChconfig
{
  /// Command line agruments
  struct
  {
    int Argc;
    char **ppArgv;
    /// Channel number
    uint8_t ChannelNumber;
    /// Tx power
    int16_t TxPower;
  } Args;
  tCHOpts CHOpts;
  /// MKx handle
  struct MKx *pMKx;
  /// MKx file descriptor
  int Fd;
  /// Storage for the tMKxTxPacketData
  struct MKxTxPacket TxPacket;
  /// Exit flag
  bool Exit;
} tLLCChconfig;

//------------------------------------------------------------------------------
// Function Prototypes
//------------------------------------------------------------------------------

static int LLC_ChconfigCmd (struct PluginCmd *pCmd, int Argc, char **ppArgv);
static int LLC_ChconfigMain (int Argc, char **ppArgv);

//------------------------------------------------------------------------------
// Variables
//------------------------------------------------------------------------------

/// Plugin support structure(s)
struct PluginCmd ChconfigCmd =
{
  .pName = "chconfig",
  .pHelp = (
    "usage: llc chconfig <Option>\n"
    "-h, --Help|Usage\n"
    "        Print this Usage\n"
    "-s, --Set\n"
    "        Set & Start specified Channel on the specified interface. Will write def\n"
    "aults if not specified\n"
    "        no arg\n"
    "-g, --Get\n"
    "        Get specified Channel on the specified interface\n"
    "        no arg\n"
    "-x, --Stop\n"
    "        Stop the specified Channel on the specified interface\n"
    "        no arg\n"
    "-e, --Filter\n"
    "        List of EtherTypes to Pass\n"
    "        uint16_t\n"
    "-i, --Interface\n"
    "        Network interface to transmit on\n"
    "        wave-raw | wave-mgmt\n"
    "-w, --Channel\n"
    "        Logical Channel to transmit on\n"
    "        CCH | SCH\n"
    "-c, --ChannelNumber\n"
    "        Channel Number\n"
    "        uint8\n"
    "-m, --DefaultMCS\n"
    "        Default Modulation and Coding Scheme\n"
    "        MK2MCS_R12BPSK | MK2MCS_R34BPSK | MK2MCS_R12QPSK | MK2MCS_R34QPSK | MK2M\n"
    "CS_R12QAM16 | MK2MCS_R34QAM16 | MK2MCS_R23QAM64 | MK2MCS_R34QAM64 | MK2MCS_DEFAU\n"
    "LT | MK2MCS_TRC\n"
    "-p, --DefaultTxPower\n"
    "        Default Tx Power level\n"
    "        uint16 (0.5 dBm steps)\n"
    "-z, --DefaultTRC\n"
    "        Default to Transmit Rate Control?\n"
    "        bool\n"
    "-t, --DefaultTPC\n"
    "        Default to Transmit Power Control\n"
    "        bool\n"
    "-b, --BW\n"
    "        Bandwidth\n"
    "        MK2BW_10MHz | MK2BW_20MHz\n"
    "-d, --DualTx\n"
    "        Tx Diversity Mode\n"
    "        MK2TXC_NONE | MK2TXC_TX | MK2TXC_RX | MK2TXC_TXRX\n"
    "-a, --RxAnt\n"
    "        Which Antenna(s) to use for Receiving?\n"
    "        0,1,2,3\n"
    "-u, --UtilPeriod\n"
    "        Channel Utilisation Period (ms)\n"
    "        uint8\n"
    "-r, --Radio\n"
    "        Physical Radio to use\n"
    "        a | b\n"
    "-f, --MACAddr\n"
    "        MAC Address\n"
    "        aa:bb:cc:dd:ee:ff\n"),
  .Func = LLC_ChconfigCmd,
};

/// App/command state
static struct LLCChconfig _Dev =
{
  .pMKx = NULL,
  .Exit = false,
};
static struct LLCChconfig *pDev = &(_Dev);


//------------------------------------------------------------------------------
// Functions definitions
//------------------------------------------------------------------------------

/**
 * @brief Called by the llc app when the 'dbg' command is specified
 */
static int LLC_ChconfigCmd (struct PluginCmd *pCmd,
                       int Argc,
                       char **ppArgv)
{
  int Res;

  d_printf(D_DBG, NULL, "Argc=%d, ppArgv[0]=%s\n", Argc, ppArgv[0]);

  Res = LLC_ChconfigMain(Argc, ppArgv);
  if (Res < 0)
    d_printf(D_WARN, NULL, "%d (%s)\n", Res, strerror(-Res));

  return Res;
}

/**
 * @brief Print capture specific usage info
 */
static int LLC_ChconfigUsage (void)
{
  Plugin_CmdPrintUsage(&ChconfigCmd);
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
static void LLC_ChconfigSignal (int SigNum)
{
  d_printf(D_TST, NULL, "Signal %d!\n", SigNum);
  pDev->Exit = true;

  // Break the MKx_Recv() loops
  (void)MKx_Recv(NULL);
}

static void LLC_SetAddressMatch (struct MKxAddressMatching *pMatch,
                                 uint8_t *pMAC)
{
  d_printf(D_DEBUG, NULL, "MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           pMAC[0], pMAC[1], pMAC[2], pMAC[3], pMAC[4], pMAC[5]);

  pMatch->Addr = pMAC[5];
  pMatch->Addr <<= 8;
  pMatch->Addr |= pMAC[4];
  pMatch->Addr <<= 8;
  pMatch->Addr |= pMAC[3];
  pMatch->Addr <<= 8;
  pMatch->Addr |= pMAC[2];
  pMatch->Addr <<= 8;
  pMatch->Addr |= pMAC[1];
  pMatch->Addr <<= 8;
  pMatch->Addr |= pMAC[0];

  pMatch->Mask = 0xFFFFFFFFFFFFULL;
  if (pMatch->Addr & 0x1)
    pMatch->MatchCtrl = 0;
  else
    pMatch->MatchCtrl = MKX_ADDRMATCH_RESPONSE_ENABLE;
}

static int LLC_ConstructChannelConfig(const bool enable,
                                      tCHOpts *pCHOpts,
                                      tMKxRadioConfigData *pRadio)
{
  int Res = -EINVAL;
  struct MKxChanConfig *pCfg;
  tMK2ChanProfile *pChanProfile = &(pCHOpts->ChanProfile);
  tMKxChannelFreq Freq[MKX_CHANNEL_COUNT];

  if (pCHOpts->Channel == MK2CHAN_CCH)
  {
    pCfg = &pRadio->ChanConfig[MKX_CHANNEL_0];
  }
  else if (pCHOpts->Channel == MK2CHAN_SCH)
  {
    pCfg = &pRadio->ChanConfig[MKX_CHANNEL_1];
  }
  else
  {
    goto Error;
  }

  if (enable)
  {
    pCfg->PHY.ChannelFreq = 5000 + (pChanProfile->ChannelNumber * 5);
    pCfg->PHY.Bandwidth = pChanProfile->Bandwidth;
    pCfg->PHY.TxAntenna = pChanProfile->TxAntenna;
    pCfg->PHY.RxAntenna = pChanProfile->RxAntenna;
    pCfg->PHY.DefaultMCS = pChanProfile->DefaultMCS;
    pCfg->PHY.DefaultTxPower = pChanProfile->DefaultTxPower;
    pCfg->MAC.DualTxControl = pChanProfile->DualTxControl;

    // Accept broadcast frames, but do not respond
    pCfg->MAC.AMSTable[0].MatchCtrl = 0;
    pCfg->MAC.AMSTable[0].Addr = 0xFFFFFFFFFFFFULL;
    pCfg->MAC.AMSTable[0].Mask = 0xFFFFFFFFFFFFULL;

    // Accept anonymous frames, but do not respond
    pCfg->MAC.AMSTable[1].MatchCtrl = 0;
    pCfg->MAC.AMSTable[1].Addr = 0x000000000000ULL;
    pCfg->MAC.AMSTable[1].Mask = 0xFFFFFFFFFFFFULL;

    // Accept frames addressed to our wave-mgmt address(dummy)
    LLC_SetAddressMatch(pCfg->MAC.AMSTable + 2, pChanProfile->MACAddr);
    // Accept frames addressed to our wave-raw address(dummy)
    LLC_SetAddressMatch(pCfg->MAC.AMSTable + 3, pChanProfile->MACAddr);
    // Accept frames addressed to our wave-data address(dummy)
    LLC_SetAddressMatch(pCfg->MAC.AMSTable + 4, pChanProfile->MACAddr);
    // Accept frames addressed to llc chconfig defined address
    LLC_SetAddressMatch(pCfg->MAC.AMSTable + 5, pChanProfile->MACAddr);
    // Accept frames addressed to our MIB address(dummy)
    LLC_SetAddressMatch(pCfg->MAC.AMSTable + 6, pChanProfile->MACAddr);

    // Accept IPv6 multicast frames addressed to 33:33:xx:xx:xx:xx and respond
    pCfg->MAC.AMSTable[7].MatchCtrl = MKX_ADDRMATCH_LAST_ENTRY;
    pCfg->MAC.AMSTable[7].Addr = 0x000000003333ULL;
    pCfg->MAC.AMSTable[7].Mask = 0x00000000FFFFULL;
  }
  else
  {
    pCfg->PHY.ChannelFreq = 0;
  }

  Freq[MKX_CHANNEL_0] = pRadio->ChanConfig[MKX_CHANNEL_0].PHY.ChannelFreq;
  Freq[MKX_CHANNEL_1] = pRadio->ChanConfig[MKX_CHANNEL_1].PHY.ChannelFreq;
  pRadio->Mode = MKX_MODE_OFF;
  if ((Freq[MKX_CHANNEL_0] > 5000) && (Freq[MKX_CHANNEL_0] < 6000))
  {
    pRadio->Mode |= MKX_MODE_CHANNEL_0;
  }
  if ((Freq[MKX_CHANNEL_1] > 5000) && (Freq[MKX_CHANNEL_1] < 6000))
  {
    pRadio->Mode |= MKX_MODE_CHANNEL_1;
  }

  Res = 0;
Error:
  return Res;
}

static int LLC_Stop(tCHOpts *pCHOpts)
{
  int Res = -ENOSYS;
  struct MKx *pMKx = pDev->pMKx;
  fMKx_Config Config = pMKx->API.Functions.Config;
  tMKxRadioConfig RadioCfg;
  tMKxRadioConfigData *pRadio = &(RadioCfg.RadioConfigData);
  tMKxRadio Radio;
  d_fnstart(D_DEBUG, NULL, "()\n");

  if (pCHOpts->Radio == CHOPTS_RADIO_A)
  {
    Radio = MKX_RADIO_A;
  }
  else if (pCHOpts->Radio == CHOPTS_RADIO_B)
  {
    Radio = MKX_RADIO_B;
  }
  else
  {
    Res = -EINVAL;
    goto Error;
  }

  memcpy(pRadio, &(pMKx->Config.Radio[Radio]), sizeof(*pRadio));
  Res = LLC_ConstructChannelConfig(false, pCHOpts, pRadio);
  if (Res)
  {
    d_printf(D_WARN, NULL, "LLC_ConstructChannelConfig failed %d\n", Res);
    goto Error;
  }
  Res = Config(pMKx, Radio, &RadioCfg);
  if (Res)
  {
    d_printf(D_WARN, NULL, "LLC_Config failed %d\n", Res);
    goto Error;
  }

Error:
  d_fnend(D_DEBUG, NULL, "() = %d\n", Res);
  return Res;
}

static int LLC_Chconfig(tCHOpts *pCHOpts)
{
  int Res = -ENOSYS;
  struct MKx *pMKx = pDev->pMKx;
  fMKx_Config Config = pMKx->API.Functions.Config;
  tMKxRadioConfig RadioCfg;
  tMKxRadioConfigData *pRadio = &(RadioCfg.RadioConfigData);
  tMKxRadio Radio;
  d_fnstart(D_DEBUG, NULL, "()\n");

  if (pCHOpts->Radio == CHOPTS_RADIO_A)
  {
    Radio = MKX_RADIO_A;
  }
  else if (pCHOpts->Radio == CHOPTS_RADIO_B)
  {
    Radio = MKX_RADIO_B;
  }
  else
  {
    Res = -EINVAL;
    goto Error;
  }

  memcpy(pRadio, &(pMKx->Config.Radio[Radio]), sizeof(*pRadio));
  Res = LLC_ConstructChannelConfig(true, pCHOpts, pRadio);
  if (Res)
  {
    d_printf(D_WARN, NULL, "LLC_ConstructChannelConfig failed %d\n", Res);
    goto Error;
  }
  Res = Config(pMKx, Radio, &RadioCfg);
  if (Res)
  {
    d_printf(D_WARN, NULL, "LLC_Config failed %d\n", Res);
    goto Error;
  }

Error:
  d_fnend(D_DEBUG, NULL, "() = %d\n", Res);
  return Res;
}

static int LLC_PrintConfig(tCHOpts *pCHOpts)
{
  int Res = -ENOSYS;
  const struct MKxChanConfig *pCfg;
  const tMKxRadioConfigData *pRadio;
  struct MKx *pMKx = pDev->pMKx;
  d_fnstart(D_DEBUG, NULL, "()\n");

  if (pCHOpts->Radio == CHOPTS_RADIO_A)
  {
    pRadio = pMKx->Config.Radio;
  }
  else if (pCHOpts->Radio == CHOPTS_RADIO_B)
  {
    pRadio = pMKx->Config.Radio + 1;
  }
  else
  {
    Res = -EINVAL;
    goto Error;
  }
  if (pCHOpts->Channel == MK2CHAN_CCH)
  {
    pCfg = &pRadio->ChanConfig[MKX_CHANNEL_0];
  }
  else if (pCHOpts->Channel == MK2CHAN_SCH)
  {
    pCfg = &pRadio->ChanConfig[MKX_CHANNEL_1];
  }
  else
  {
    goto Error;
  }

  MKxChanConfig_Print(pCfg, "  ");
  Res = 0;

Error:
  d_fnend(D_DEBUG, NULL, "() = %d\n", Res);
  return Res;
}

/**
 * @brief Initiate the 'dbg' command
 */
int LLC_ChconfigInit (struct LLCChconfig *pDev)
{
  int Res;

  d_fnstart(D_TST, NULL, "()\n");

  // setup the signal handlers to exit gracefully
  d_printf(D_DBG, NULL, "Signal handlers\n");
  pDev->Exit = false;
  signal(SIGINT,  LLC_ChconfigSignal);
  signal(SIGTERM, LLC_ChconfigSignal);
  signal(SIGQUIT, LLC_ChconfigSignal);
  signal(SIGHUP,  LLC_ChconfigSignal);
  signal(SIGPIPE, LLC_ChconfigSignal);

  Res = MKx_Init(&(pDev->pMKx));
  if (Res < 0)
    goto Error;
  pDev->Fd = MKx_Fd(pDev->pMKx);
  if (pDev->Fd < 0)
    goto Error;

  Res = CHOpts_New(pDev->Args.Argc, pDev->Args.ppArgv, &pDev->CHOpts);
  if (Res)
    goto Error;

  pDev->pMKx->pPriv = (void *)pDev;

Error:
  if (Res != 0)
    LLC_ChconfigUsage();

  d_fnend(D_TST, NULL, "() = %d\n", Res);
  return Res;
}

/**
 * @brief Cancel any 'dbg' command(s)
 */
int LLC_ChconfigExit (struct LLCChconfig *pDev)
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
static int LLC_ChconfigMain (int Argc, char **ppArgv)
{
  int Res;
  tCHOpts *pCHOpts;

  d_fnstart(D_TST, NULL, "()\n");

  pDev->Args.Argc = Argc;
  pDev->Args.ppArgv = ppArgv;

  // Initial actions
  Res = LLC_ChconfigInit(pDev);
  if (Res < 0)
    goto Error;

  pCHOpts = &pDev->CHOpts;
  switch (pCHOpts->Command)
  {
    case MK2_CMD_SET:
      CHOpts_Print(pCHOpts);
      Res = LLC_Chconfig(pCHOpts);
      break;
    case MK2_CMD_GET:
      Res = LLC_PrintConfig(pCHOpts);
      break;
    case MK2_CMD_DELETE:
      Res = LLC_Stop(pCHOpts);
      break;
    default:
      d_printf(D_ERR, NULL, "Unknown Command\n");
      break;
  }

Error:
  // Final actions
  LLC_ChconfigExit(pDev);

  d_fnend(D_TST, NULL, "() = %d\n", Res);
  return Res;
}

/**
 * @}
 */
