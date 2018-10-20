/**
 * @addtogroup cohda_llc_intern_lib LLC API library
 * @{
 *
 * @file
 * LLC: Transport agnostic LLC <-> SAF5100 messaging functionality
 *
 */

//------------------------------------------------------------------------------
// Copyright (c) 2013 Cohda Wireless Pty Ltd
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Included headers
//------------------------------------------------------------------------------
#include <errno.h>
#include "llc-lib.h"

#include "debug-levels.h"
//------------------------------------------------------------------------------
// Macros & Constants
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Type definitions
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Variables
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------

static int LLC_MsgTxCnf (struct LLCDev *pDev,
                         struct MKxIFMsg *pMsg);
static int LLC_MsgRxPkt (struct LLCDev *pDev,
                         struct MKxIFMsg *pMsg);
static int LLC_MsgCfgInd (struct LLCDev *pDev,
                          struct MKxIFMsg *pMsg);
static int LLC_MsgTempCfgInd (struct LLCDev *pDev,
                              struct MKxIFMsg *pMsg);
static int LLC_MsgTempInd (struct LLCDev *pDev,
                           struct MKxIFMsg *pMsg);
static int LLC_MsgPowerDetCfgInd (struct LLCDev *pDev,
                                  struct MKxIFMsg *pMsg);
static int LLC_MsgDbgInd (struct LLCDev *pDev,
                          struct MKxIFMsg *pMsg);
static int LLC_MsgStatsInd (struct LLCDev *pDev,
                            struct MKxIFMsg *pMsg);
static int LLC_MsgSecInd (struct LLCDev *pDev,
                          struct MKxIFMsg *pMsg);


/**
 * @brief Setup the LLC messaging handling
 * @param pDev LLC device pointer
 * @return Zero (LLC_STATUS_SUCCESS) or a negative errno
 */
int LOCAL LLC_MsgSetup (struct LLCDev *pDev)
{
  int Res;

  d_fnstart(D_VERBOSE, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);

  pDev->Msg.RxLen = -1;
  pDev->Msg.pRxBuf = NULL;
  pDev->Msg.pRxPriv = NULL;

  Res = 0;

  d_fnend(D_VERBOSE, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}


/**
 * @brief Release (cleanup) the LLC messaging
 * @param pDev LLC device pointer
 * @return Zero (LLC_STATUS_SUCCESS) or a negative errno
 */
int LOCAL LLC_MsgRelease (struct LLCDev *pDev)
{
  int Res;

  d_fnstart(D_VERBOSE, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);

  ; // TODO
  Res = 0;

  d_fnend(D_VERBOSE, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}


/**
 * @brief Handle a message from the MKx
 */
int LOCAL LLC_MsgRecv (struct LLCDev *pDev)
{
  int Res = LLC_STATUS_ERROR;
  int Len = -1;
  struct MKxIFMsg *pMsg = NULL;

  d_fnstart(D_VERBOSE, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);

  pDev->Msg.RxLen = 0;
  pDev->Msg.pRxBuf = NULL;
  pDev->Msg.pRxPriv = NULL;

  // Get the buffer from the interface
  {
    char *pBuf = NULL;

    Res = LLC_InterfaceRecv(pDev, &pBuf, &Len);
    if ((Res < 0) || (pBuf == NULL) || (Len < 0))
      goto Error;
    if ((Len <= 0) || (pBuf == NULL))
      goto Error;

    pDev->Msg.RxLen = Len;
    pDev->Msg.pRxBuf = (uint8_t *)pBuf;
    pDev->Msg.pRxPriv = NULL; // TODO
  }

  pMsg = (struct MKxIFMsg *)(pDev->Msg.pRxBuf);
  // Sanity check the lengths then trim off the header
  if ((Len < sizeof(struct MKxIFMsg)) || (Len < pMsg->Len))
  {
    d_error(D_WARN, NULL, "Truncated message: pBuf %d pMsg %d\n",
            Len, pMsg->Len);
    //d_dump(D_WARN, NULL, pMsg, pMsg->Len);
    goto Error;
  }
  Len = pMsg->Len - sizeof(struct MKxIFMsg);

  d_printf(D_DBG, NULL, "Msg: Type:%d Len:%d Seq:%d Ret:%d\n",
           pMsg->Type, pMsg->Len, pMsg->Seq, pMsg->Ret);
 
  // Ignore driver usb send echo 
  if (pMsg->Ret == (int16_t)MKXSTATUS_RESERVED)
  {
    Res = MKXSTATUS_SUCCESS;
    goto Error;
  }
    
  switch (pMsg->Type)
  {
    case MKXIF_TXEVENT: // A transmit confirm (message data is tMKxTxEvent)
      Res = LLC_MsgTxCnf(pDev, pMsg);
      break;

    case MKXIF_RXPACKET: // A received packet (message data is tMKxRxPacket)
      Res = LLC_MsgRxPkt(pDev, pMsg);
      pDev->Msg.pRxBuf = NULL; // Force an RxAlloc()
      break;

    case MKXIF_RADIOASTATS: // Radio A stats (message data is tMKxRadioStats)
    case MKXIF_RADIOBSTATS: // Radio B stats (message data is tMKxRadioStats)
      Res = LLC_MsgStatsInd(pDev, pMsg);
      break;

    case MKXIF_RADIOACFG: // Radio A config (message data is tMKxRadioConfig)
    case MKXIF_RADIOBCFG: // Radio B config (message data is tMKxRadioConfig)
      Res = LLC_MsgCfgInd(pDev, pMsg);
      break;

    case MKXIF_TEMPCFG: // Temperature config (message data is tMKxTempConfig)
      Res = LLC_MsgTempCfgInd(pDev, pMsg);
      break;

    case MKXIF_TEMP: // Temperature measurement (message data is tMKxTemp)
      Res = LLC_MsgTempInd(pDev, pMsg);
      break;

    case MKXIF_POWERDETCFG: // Power Detector config (message data is tMKxPowerDetConfig)
      Res = LLC_MsgPowerDetCfgInd(pDev, pMsg);
      break;

    case MKXIF_DEBUG: // Generic debug container (message data is tMKxDebug)
      Res = LLC_MsgDbgInd(pDev, pMsg);
      break;

    case MKXIF_C2XSEC: // A C2X security response (message data is tMKxC2XSec)
      Res = LLC_MsgSecInd(pDev, pMsg);
      break;

    case MKXIF_TXPACKET: // Transmit packet data
    case MKXIF_SET_TSF: // New UTC Time (obtained from NMEA data)
    case MKXIF_FLUSHQ: // Flush a single queue or all queueus
      d_printf(D_NOTICE, NULL, "Unexpected recv message type %d\n", pMsg->Type);
      break;
    default:
      d_printf(D_ERR, NULL, "Unknown recv message type %d\n", pMsg->Type);
      break;
  }

Error:
  d_fnend(D_VERBOSE, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}


/**
 * @brief Send on a message verbatim to the 'cw-llc' netdev
 */
int LOCAL LLC_MsgSend (struct LLCDev *pDev, struct MKxIFMsg *pMsg)
{
  int Res;

  d_fnstart(D_VERBOSE, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);
  d_assert(pMsg != NULL);

  Res = LLC_InterfaceSend(pDev, (char *)pMsg, pMsg->Len);
  if (Res == pMsg->Len)
    Res = 0; // Success
  else
    Res = -EIO;

  d_fnend(D_VERBOSE, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}


/**
 * @brief Create a TxReq message and pass it to the underlying transport
 *
 */
int LOCAL LLC_MsgTxReq (struct LLCDev *pDev,
                        tMKxTxPacket *pTxPkt,
                        void *pPriv)
{
  int Res;
  struct MKxIFMsg *pMsg = (struct MKxIFMsg *)(pTxPkt);
  tLLCTxReq *pTxReq;

  d_fnstart(D_TST, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);

  // store the pTxPkt pointer and pPriv pointer
  pTxReq = &pDev->Msg.TxReqs[pDev->Msg.Cnt];
  if (pTxReq->pTxPacket != NULL)
  {
    d_printf(D_WARN, NULL, "Slot %u already in use - will try next on next TxReq\n",
             pDev->Msg.Cnt);

    // no free slots left (we could look for an empty slot?)
    Res = -ENOMEM;
    // try the next slot on next tx
    pDev->Msg.Cnt = (pDev->Msg.Cnt + 1) % (LLC_SEQNUM_MAX + 1);
    goto Error;
  }

  // Populate the 'tx' bits that we know
  pMsg->Type = MKXIF_TXPACKET;
  pMsg->Len = sizeof(struct MKxTxPacket) + \
              pTxPkt->TxPacketData.TxFrameLength;
  pMsg->Seq = pDev->Msg.Cnt;
  pMsg->Ret = MKXSTATUS_RESERVED; 

  pDev->Msg.Cnt = (pDev->Msg.Cnt + 1) % (LLC_SEQNUM_MAX + 1);

  pTxReq->pTxPacket = pTxPkt;
  pTxReq->pPriv = pPriv;

  d_printf(D_DEBUG, NULL, "-> TxReq(%u) {%p, %p}\n", pMsg->Seq,
           pTxReq->pTxPacket, pTxReq->pPriv);

  // Pass it to the underlying transport layer (blocks until sent)
  Res = LLC_MsgSend(pDev, pMsg);

Error:
  d_fnend(D_TST, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}


/**
 * @brief Parse a TxCnf message and pass it to the API
 *
 */
static int LLC_MsgTxCnf (struct LLCDev *pDev,
                         struct MKxIFMsg *pMsg)
{
  int Res = LLC_STATUS_ERROR;
  int Len;
  struct MKxTxEvent *pEvt = (struct MKxTxEvent *)pMsg;
  tLLCTxReq *pTxReq;
  tMKxStatus Result = LLC_STATUS_ERROR;

  d_fnstart(D_VERBOSE, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);

  Len = pMsg->Len - sizeof(struct MKxIFMsg);
  if (Len != sizeof(struct MKxTxEventData))
  {
    d_printf(D_WARN, NULL, "Len %d != %d)\n",
             Len, sizeof(struct MKxTxEventData));
    goto Error;
  }

  // Get the general result from the message header
  Result = pEvt->Hdr.Ret;
  // If that's okay, get the actual transmit result from the message payload
  if (Result == MKXSTATUS_SUCCESS)
    Result = pEvt->TxEventData.TxStatus;

  // Try lookup the original packet
  if (pEvt->Hdr.Seq <= LLC_SEQNUM_MAX)
  {
    pTxReq = &pDev->Msg.TxReqs[pEvt->Hdr.Seq];

    d_printf(D_DEBUG, NULL, "<- TxCnf(%u) {%p, %p}\n", pMsg->Seq,
             pTxReq->pTxPacket, pTxReq->pPriv);

    // Call the TxCnf API function
    Res = LLC_TxCnf(&(pDev->MKx), pTxReq->pTxPacket,  pEvt, pTxReq->pPriv);

    pTxReq->pTxPacket = NULL;
    pTxReq->pPriv = NULL;
  }
  else
  {
    // if a TxCnf callback is registered then be noisier otherwise be quieter
    // since probably nobody cares...
    d_printf(pDev->MKx.API.Callbacks.TxCnf != NULL ? D_WARN : D_INFO, NULL,
             "Received a TxCnf with invalid Seq: %u\n", pEvt->Hdr.Seq);
  }

Error:
  d_fnend(D_VERBOSE, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}


/**
 * @brief Parse a RxPkt message and pass it to the API
 *
 */
static int LLC_MsgRxPkt (struct LLCDev *pDev,
                         struct MKxIFMsg *pMsg)
{
  int Res = LLC_STATUS_ERROR;
  int Len;
  uint8_t *pBuf;
  struct PktBuf *pPkb;

  d_fnstart(D_VERBOSE, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);

  // Sanity check the packet lengths etc.
  Len = pMsg->Len - sizeof(struct MKxIFMsg);
  if (Len < sizeof(struct MKxRxPacketData))
  {
    d_printf(D_WARN, NULL, "Len %d < %d)\n",
             Len, sizeof(struct MKxRxPacketData));
    goto Error;
  }
  ; // TODO more

  // don't bother trying to call LLC_RxAlloc() if there is no RxAlloc Callback
  // since it will fail (and clearly if there is no RxAlloc Callback then no-one
  // cares about trying to receive the packet anyway)
  if (pDev->MKx.API.Callbacks.RxAlloc == NULL)
    goto Error;

  Res = LLC_RxAlloc(&(pDev->MKx), pMsg->Len, &pBuf, (void **)&pPkb);
  if (Res)
  {
    d_error(D_ERR, NULL, "LLC_RxAlloc failed.\n");
    goto Error;
  }
  pDev->Msg.pRxPriv = pPkb;
  memcpy(pBuf, pMsg, pMsg->Len);
  Res = LLC_RxInd(&(pDev->MKx), (struct MKxRxPacket *)pBuf, pDev->Msg.pRxPriv);

Error:
  d_fnend(D_VERBOSE, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}

/**
 * @brief Create a CfgReq message and pass it to the underlying transport
 *
 */
int LOCAL LLC_MsgCfgReq (struct LLCDev *pDev,
                         tMKxRadio Radio,
                         tMKxRadioConfig *pConfig)
{
  int Res = LLC_STATUS_ERROR;

  d_fnstart(D_TST, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);

  // Populate the 'out' bits that we know
  {
    struct MKxIFMsg *pMsg = (struct MKxIFMsg *)(pConfig);

    switch (Radio)
    {
      case MKX_RADIO_A:
        pMsg->Type = MKXIF_RADIOACFG;
        break;
      case MKX_RADIO_B:
        pMsg->Type = MKXIF_RADIOBCFG;
        break;
      default:
        Res = MKXSTATUS_FAILURE_INVALID_PARAM;
        goto Error;
        break;
    }
    pMsg->Len = sizeof(struct MKxRadioConfig);
    //pMsg->Seq = 0xC0DA;
    pMsg->Ret = MKXSTATUS_RESERVED;

    // Pass it to the underlying transport layer (blocks until sent)
    Res = LLC_MsgSend(pDev, pMsg);
  }

  // Optimistically apply the requested config to the MKx structure
  // (eventually overwritten by the contents of the reply message)
  {
    memcpy((void *)&(pDev->MKx.Config.Radio[Radio]),
           &(pConfig->RadioConfigData),
           sizeof(struct MKxRadioConfigData));
  }

Error:
  d_fnend(D_TST, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}


/**
 * @brief Parse a CfgInd message and pass it to the API
 *
 */
static int LLC_MsgCfgInd (struct LLCDev *pDev,
                          struct MKxIFMsg *pMsg)
{
  int Res = LLC_STATUS_ERROR;
  int Len;
  tMKxNotif Notif = MKX_NOTIF_MASK_RADIOA;
  tMKxRadio Radio = MKX_RADIO_A;

  d_fnstart(D_VERBOSE, NULL, "(pDev %p)\n", pDev);

  Len = pMsg->Len - sizeof(struct MKxIFMsg);
  if (Len != sizeof(struct MKxRadioConfigData))
  {
    d_printf(D_WARN, NULL, "Len %d != %d)\n",
             Len, sizeof(struct MKxRadioConfigData));
    goto Error;
  }

  if (pMsg->Type == MKXIF_RADIOBCFG)
  {
    Notif = MKX_NOTIF_MASK_RADIOB;
    Radio = MKX_RADIO_B;
  }

  {
    // Collect the config into the MKx structure
    tMKxRadioConfig *pRadioConfig = (tMKxRadioConfig *)pMsg;
    memcpy((void *)&(pDev->MKx.Config.Radio[Radio]),
           &(pRadioConfig->RadioConfigData),
           sizeof(struct MKxRadioConfigData));
  }
  // Set the notification
  if (pMsg->Ret != 0)
  {
    Notif |= MKX_NOTIF_ERROR;
  }
  Res = LLC_NotifInd(&(pDev->MKx), Notif);

Error:
  d_fnend(D_VERBOSE, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}


/**
 * @brief Create a TempCfgReq message and pass it to the underlying transport
 *
 */
int LOCAL LLC_MsgTempCfgReq (struct LLCDev *pDev,
                             tMKxTempConfig *pCfg)
{
  int Res = LLC_STATUS_ERROR;

  d_fnstart(D_TST, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);

  // Populate the 'out' bits that we know
  {
    struct MKxIFMsg *pMsg = (struct MKxIFMsg *)(pCfg);

    pMsg->Type = MKXIF_TEMPCFG;
    pMsg->Len = sizeof(struct MKxTempConfig);
    pMsg->Seq = 0xC0DA;
    pMsg->Ret = MKXSTATUS_RESERVED;

    // Pass it to the underlying transport layer (blocks until sent)
    Res = LLC_MsgSend(pDev, pMsg);
  }

  // Optimistically apply the requested config to the MKx structure
  // (eventually overwritten by the contents of the reply message)
  if (pCfg->TempConfigData.SensorSource != 0x80)
  {
    memcpy((void *)&(pDev->MKx.Config.Temp),
           &(pCfg->TempConfigData),
           sizeof(struct MKxTempConfigData));
  }

  d_fnend(D_TST, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}


/**
 * @brief Parse a TempCfgInd message and pass it to the API
 *
 */
static int LLC_MsgTempCfgInd (struct LLCDev *pDev,
                              struct MKxIFMsg *pMsg)
{
  int Res = LLC_STATUS_ERROR;
  int Len;
  tMKxNotif Notif = MKX_NOTIF_TEMPCFG;

  d_fnstart(D_VERBOSE, NULL, "(pDev %p)\n", pDev);

  Len = pMsg->Len - sizeof(struct MKxIFMsg);
  if (Len != sizeof(struct MKxTempConfigData))
  {
    d_printf(D_WARN, NULL, "Len %d != %d)\n",
             Len, sizeof(struct MKxTempConfigData));
    goto Error;
  }

  if (pMsg->Ret == 0)
  {
    // Collect the config into the MKx structure
    tMKxTempConfig *pCfg = (struct MKxTempConfig *)pMsg;
    memcpy((void *)&(pDev->MKx.Config.Temp),
           &(pCfg->TempConfigData),
           sizeof(struct MKxTempConfigData));
  }
  else
  {
    Notif |= MKX_NOTIF_ERROR;
  }
  Res = LLC_NotifInd(&(pDev->MKx), Notif);

Error:
  d_fnend(D_VERBOSE, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}


/**
 * @brief Create a TempReq message and pass it to the underlying transport
 *
 */
int LOCAL LLC_MsgTempReq (struct LLCDev *pDev,
                             tMKxTemp *pTemp)
{
  int Res = LLC_STATUS_ERROR;

  d_fnstart(D_TST, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);

  // Populate the 'out' bits that we know
  {
    struct MKxIFMsg *pMsg = (struct MKxIFMsg *)(pTemp);

    pMsg->Type = MKXIF_TEMP;
    pMsg->Len = sizeof(struct MKxTempConfig);
    pMsg->Seq = 0xC0DA;
    pMsg->Ret = MKXSTATUS_RESERVED; 

    // Pass it to the underlying transport layer (blocks until sent)
    Res = LLC_MsgSend(pDev, pMsg);
  }

  // Optimistically apply the requested config to the MKx structure
  // (eventually overwritten by the contents of the reply message)
  if (pTemp->TempData.TempPAAnt1 != (int8_t)0x80)
  {
    memcpy((void *)&(pDev->MKx.State.Temp),
           &(pTemp->TempData),
           sizeof(struct MKxTempData));
  }

  d_fnend(D_TST, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}


/**
 * @brief Parse a TempInd message and pass it to the API
 *
 */
static int LLC_MsgTempInd (struct LLCDev *pDev,
                           struct MKxIFMsg *pMsg)
{
  int Res = LLC_STATUS_ERROR;
  int Len;
  tMKxNotif Notif = MKX_NOTIF_TEMP;

  d_fnstart(D_VERBOSE, NULL, "(pDev %p)\n", pDev);

  Len = pMsg->Len - sizeof(struct MKxIFMsg);
  if (Len != sizeof(struct MKxTempData))
  {
    d_printf(D_WARN, NULL, "Len %d != %d)\n",
             Len, sizeof(struct MKxTempData));
    goto Error;
  }

  if (pMsg->Ret == 0)
  {
    // Collect the measurement(s) into the MKx structure
    tMKxTemp *pTemp = (tMKxTemp *)pMsg;
    memcpy((void *)&(pDev->MKx.State.Temp),
           &(pTemp->TempData),
           sizeof(struct MKxTempData));
  }
  else
    Notif |= MKX_NOTIF_ERROR;

  Res = LLC_NotifInd(&(pDev->MKx), Notif);

Error:
  d_fnend(D_VERBOSE, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}



/**
 * @brief Create a PowerDetReq message and pass it to the underlying transport
 *
 */
int LOCAL LLC_MsgPowerDetCfgReq (struct LLCDev *pDev,
                                 tMKxPowerDetConfig *pCfg)
{
  int Res = LLC_STATUS_ERROR;

  d_fnstart(D_TST, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);

  // Populate the 'out' bits that we know
  {
    struct MKxIFMsg *pMsg = (struct MKxIFMsg *)(pCfg);

    pMsg->Type = MKXIF_POWERDETCFG;
    pMsg->Len = sizeof(struct MKxPowerDetConfig);
    pMsg->Seq = 0xC0DA;
    pMsg->Ret = MKXSTATUS_RESERVED;

    // Pass it to the underlying transport layer (blocks until sent)
    Res = LLC_MsgSend(pDev, pMsg);
  }

  if (pCfg->PowerDetConfigData.PowerCalMode != 0x80)
  {
    memcpy((void *)&(pDev->MKx.Config.PowerDet),
           &(pCfg->PowerDetConfigData),
           sizeof(struct MKxPowerDetCalData));
  }

  d_fnend(D_TST, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}


/**
 * @brief Parse a PowerDetInd message and pass it to the API
 *
 */
static int LLC_MsgPowerDetCfgInd (struct LLCDev *pDev,
                                  struct MKxIFMsg *pMsg)
{
  int Res = LLC_STATUS_ERROR;
  int Len;
  tMKxNotif Notif = MKX_NOTIF_POWERDETCFG;

  d_fnstart(D_VERBOSE, NULL, "(pDev %p)\n", pDev);

  Len = pMsg->Len - sizeof(struct MKxIFMsg);
  if (Len != sizeof(struct MKxPowerDetConfigData))
  {
    d_printf(D_WARN, NULL, "Len %d != %d)\n",
             Len, sizeof(struct MKxPowerDetConfigData));
    goto Error;
  }

  if (pMsg->Ret == 0)
  {
    // Collect the config into the MKx structure
    tMKxPowerDetConfig *pCfg = (struct MKxPowerDetConfig *)pMsg;

    memcpy((void *)&(pDev->MKx.Config.PowerDet),
           &(pCfg->PowerDetConfigData),
           sizeof(struct MKxPowerDetConfigData));
  }
  else
  {
    Notif |= MKX_NOTIF_ERROR;
  }
  Res = LLC_NotifInd(&(pDev->MKx), Notif);

Error:
  d_fnend(D_VERBOSE, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}







/**
 * @brief Create a DbgReq message and pass it to the underlying transport
 *
 */
int LOCAL LLC_MsgDbgReq (struct LLCDev *pDev,
                         struct MKxIFMsg *pMsg)
{
  int Res = LLC_STATUS_ERROR;

  d_fnstart(D_TST, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);

  // Init the header and populate the context bits that we know
  {
    switch (pMsg->Type)
    {
      case MKXIF_DEBUG:
        break;

      default:
        pMsg->Type = MKXIF_DEBUG;
        break;
    }
    pMsg->Seq = 0xC0DA;
    pMsg->Ret = MKXSTATUS_RESERVED;

    d_printf(D_DBG, NULL, "Msg: Type:%d Len:%d Seq:%d Ret:%d\n",
             pMsg->Type, pMsg->Len, pMsg->Seq, pMsg->Ret);

    // Pass it to the underlying transport layer (blocks until sent)
    Res = LLC_MsgSend(pDev, pMsg);
  }

  d_fnend(D_TST, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}

/**
 * @brief Parse a DbgInd message and pass it to the API
 *
 */
static int LLC_MsgDbgInd (struct LLCDev *pDev,
                          struct MKxIFMsg *pMsg)
{
  int Res = LLC_STATUS_ERROR;
  int Len;

  d_fnstart(D_VERBOSE, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);
  d_assert(pMsg != NULL);

  // Sanity check the packet lengths etc.
  Len = pMsg->Len - sizeof(struct MKxIFMsg);
  if (Len < 0)
  {
    d_printf(D_WARN, NULL, "Len %d\n", Len);
    goto Error;
  }

  // Call the DbgInd API function
  Res = LLC_DebugInd(&(pDev->MKx), pMsg);

Error:
  d_fnend(D_VERBOSE, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}


/**
 * @brief Create a C2X security command message and pass it to the underlying transport
 *
 */
int LOCAL LLC_MsgSecReq (struct LLCDev *pDev,
                         tMKxC2XSec *pMsg)
{
  int Res = LLC_STATUS_ERROR;

  d_fnstart(D_TST, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);

  // Init the header and populate the context bits that we know
  {
    pMsg->Hdr.Type = MKXIF_C2XSEC;
    pMsg->Hdr.Seq = 0xC0DA;
    pMsg->Hdr.Ret = MKXSTATUS_RESERVED;

    d_printf(D_DBG, NULL, "Msg: Type:%d Len:%d Seq:%d Ret:%d\n",
             pMsg->Hdr.Type, pMsg->Hdr.Len, pMsg->Hdr.Seq, pMsg->Hdr.Ret);

    // Pass it to the underlying transport layer (blocks until sent)
    Res = LLC_MsgSend(pDev, (struct MKxIFMsg *)pMsg);
  }

  d_fnend(D_TST, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}

/**
 * @brief Parse a C2X security response message and pass it to the API
 *
 */
static int LLC_MsgSecInd (struct LLCDev *pDev,
                          struct MKxIFMsg *pMsg)
{
  int Res = LLC_STATUS_ERROR;
  int Len;

  d_fnstart(D_VERBOSE, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);
  d_assert(pMsg != NULL);

  // Sanity check the packet lengths etc.
  Len = pMsg->Len - sizeof(struct MKxIFMsg);
  if (Len <= 0)
  {
    d_printf(D_WARN, NULL, "Len %d\n", Len);
    goto Error;
  }

  // Call the C2XSecInd API function
  Res = LLC_C2XSecInd(&(pDev->MKx), (tMKxC2XSec *)pMsg);

Error:
  d_fnend(D_VERBOSE, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}


/**
 * @brief Parse a DbgInd message and pass it to the API
 *
 */
static int LLC_MsgStatsInd (struct LLCDev *pDev,
                            struct MKxIFMsg *pMsg)
{
  int Res = LLC_STATUS_ERROR;
  int Len;
  tMKxNotif Notif = MKX_NOTIF_NONE;
  tMKxRadio Radio = MKX_RADIO_A;

  d_fnstart(D_VERBOSE, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);
  d_assert(pMsg != NULL);

  // Sanity check the packet lengths etc.
  Len = pMsg->Len - sizeof(struct MKxIFMsg);
  if (Len < sizeof(tMKxRadioStatsData))
  {
    d_printf(D_WARN, NULL, "Len %d\n", Len);
    goto Error;
  }

  // Collect the stats into the MKx structure
  if (pMsg->Type == MKXIF_RADIOASTATS)
    Radio = MKX_RADIO_A;
  if (pMsg->Type == MKXIF_RADIOBSTATS)
    Radio = MKX_RADIO_B;
  memcpy((void *)&(pDev->MKx.State.Stats[Radio].RadioStatsData),
         (struct tMKxRadioStatsData *)(pMsg->Data),
         sizeof(tMKxRadioStatsData));

  // Send the 'stats' notification
  Notif = MKX_NOTIF_MASK_STATS;
  if (Radio == MKX_RADIO_A)
    Notif |= MKX_NOTIF_MASK_RADIOA;
  if (Radio == MKX_RADIO_B)
    Notif |= MKX_NOTIF_MASK_RADIOB;
  if (pMsg->Seq == 0)
    Notif |= MKX_NOTIF_MASK_CHANNEL0;
  else
    Notif |= MKX_NOTIF_MASK_CHANNEL1;
  if (pMsg->Ret != 0)
    Notif |= MKX_NOTIF_ERROR;
  Res = LLC_NotifInd(&(pDev->MKx), Notif);

  // Send the 'active' notification
  Notif = MKX_NOTIF_MASK_ACTIVE;
  if (Radio == MKX_RADIO_A)
    Notif |= MKX_NOTIF_MASK_RADIOA;
  if (Radio == MKX_RADIO_B)
    Notif |= MKX_NOTIF_MASK_RADIOB;
  if (pMsg->Seq == 0)
  { // X0 just ended
    if (pDev->MKx.Config.Radio[Radio].Mode == MKX_MODE_OFF)
      Notif = MKX_NOTIF_NONE; // Nothing restarted
    else if (pDev->MKx.Config.Radio[Radio].Mode == MKX_MODE_CHANNEL_0)
      Notif |= MKX_NOTIF_MASK_CHANNEL0; // X0 just restarted
    else if (pDev->MKx.Config.Radio[Radio].Mode == MKX_MODE_SWITCHED)
      Notif |= MKX_NOTIF_MASK_CHANNEL1; // X1 just restarted
    else
      d_printf(D_WARN, NULL, "Stats %c0 event, but Mode is %d\n",
               Radio ? 'A':'B', pDev->MKx.Config.Radio[Radio].Mode);
  }
  else
  { // X1 just ended
    if (pDev->MKx.Config.Radio[Radio].Mode == MKX_MODE_OFF)
      Notif = MKX_NOTIF_NONE; // Nothing restarted
    else if (pDev->MKx.Config.Radio[Radio].Mode == MKX_MODE_CHANNEL_1)
      Notif |= MKX_NOTIF_MASK_CHANNEL1; // X1 just restarted
    else if (pDev->MKx.Config.Radio[Radio].Mode == MKX_MODE_SWITCHED)
      Notif |= MKX_NOTIF_MASK_CHANNEL0; // X0 just restarted
    else
      d_printf(D_WARN, NULL, "Stats %c1 event, but Mode is %d\n",
               Radio ? 'A':'B', pDev->MKx.Config.Radio[Radio].Mode);
  }
  if (Notif != MKX_NOTIF_NONE) // Don't notify for nothing
    Res = LLC_NotifInd(&(pDev->MKx), Notif);

Error:
  d_fnend(D_VERBOSE, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}

/**
 * @brief Create a SET_TSF message and pass it to the underlying transport
 *
 */
int LOCAL LLC_MsgSetTSFReq (struct LLCDev *pDev,
                            tMKxTSF TSF)
{
  int Res = LLC_STATUS_ERROR;

  d_fnstart(D_TST, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);

  {
    uint8_t Msg[sizeof(tMKxIFMsg) + sizeof(tMKxTSF)];
    memset(Msg, 0, sizeof(Msg));

    struct MKxIFMsg *pMsg = (struct MKxIFMsg *)(Msg);

    pMsg->Type = MKXIF_SET_TSF;
    pMsg->Len = sizeof(struct MKxIFMsg) + sizeof(tMKxTSF);
    pMsg->Seq = 0xC0DA;
    pMsg->Ret = MKXSTATUS_RESERVED;
    memcpy(pMsg->Data, &TSF, sizeof(tMKxTSF));

    // Pass it to the underlying transport layer (blocks until sent)
    Res = LLC_MsgSend(pDev, pMsg);
  }

  d_fnend(D_TST, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}

/**
 * @}
 */

