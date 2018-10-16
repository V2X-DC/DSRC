/**
 * @addtogroup cohda_llc_intern_lib LLC API library
 * @{
 *
 * @file
 * LLC: API functions
 */

//------------------------------------------------------------------------------
// Copyright (c) 2013 Cohda Wireless Pty Ltd
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Included headers
//------------------------------------------------------------------------------
#include <errno.h>
#if defined(__QNX__)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/iofunc.h>
#endif

#include "linux/cohda/llc/llc.h"
#include "linux/cohda/llc/llc-api.h"
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

// LLC state storage
static struct LLCDev __LLCDev;

//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------

/**
 * @copydoc MKx_Init
 */
tMKxStatus MKx_Init (struct MKx **ppMKx)
{
  int Res = LLC_STATUS_ERROR;
  struct LLCDev *pDev = NULL;
  struct MKx *pMKx = NULL;

  d_fnstart(D_API, NULL, "(ppMKx %p)\n", ppMKx);
  d_assert(ppMKx != NULL);

  // MKx_Init() can be called multiple times
  // and only initialised on first invocation
  pDev = LLC_DeviceGet();
  if (pDev == NULL)
  {
    pDev = &__LLCDev;
    Res = LLC_DeviceSetup(pDev);
    if (Res != 0)
      goto Error;

    pMKx = &(pDev->MKx);

    // Get the MKx driver state
    Res = MKx_Get(pMKx);
    if (Res == LLC_STATUS_SUCCESS);
    *ppMKx = pMKx;
  }
  else
  {
    *ppMKx = &(pDev->MKx);
    Res = LLC_STATUS_SUCCESS;
  }

Error:
  d_fnend(D_API, NULL, "(ppMKx %p [%p]) = %d\n", ppMKx, *ppMKx, Res);
  return Res;
}

/**
 * @copydoc MKx_Exit
 */
tMKxStatus MKx_Exit (struct MKx *pMKx)
{
  int Res = LLC_STATUS_ERROR;
  struct LLCDev *pDev = NULL;

  d_fnstart(D_API, NULL, "(pMKx %p)\n", pMKx);
  if (pMKx == NULL)
    goto Error;

  pDev = LLC_Device(pMKx);
  d_printf(D_DBG, NULL, "pDev %p\n", pDev);
  d_assert(pDev != NULL);
  d_assert(pDev == &__LLCDev);

  // Turn off the calllbacks
  pMKx->API.Callbacks.TxCnf     = NULL;
  pMKx->API.Callbacks.RxAlloc   = NULL;
  pMKx->API.Callbacks.RxInd     = NULL;
  pMKx->API.Callbacks.NotifInd  = NULL;
  pMKx->API.Callbacks.DebugInd  = NULL;
  pMKx->API.Callbacks.C2XSecRsp = NULL;

  // Release the LLC
  LLC_DeviceRelease(pDev);

  Res = LLC_STATUS_SUCCESS;

Error:
  d_fnend(D_API, NULL, "(pMKx %p) = %d\n", pMKx, Res);
  return Res;
}


/**
 * @copydoc MKx_Get
 *
 */
tMKxStatus MKx_Get (tMKx *pMKx)
{
  int Res = -ENOSYS;
  struct LLCDev *pDev = NULL;
  struct MKx Bak;

  d_fnstart(D_API, NULL, "(pMKx %p)\n", pMKx);

  // Get a handle of the overarching device
  pDev = LLC_Device(pMKx);
  if (pDev == NULL)
  {
    Res = -ENODEV;
    goto Error;
  }

  // Backup the non-kernel stuff
  Bak.pPriv                   = pMKx->pPriv;
  Bak.API.Callbacks.TxCnf     = pMKx->API.Callbacks.TxCnf;
  Bak.API.Callbacks.RxAlloc   = pMKx->API.Callbacks.RxAlloc;
  Bak.API.Callbacks.RxInd     = pMKx->API.Callbacks.RxInd;
  Bak.API.Callbacks.NotifInd  = pMKx->API.Callbacks.NotifInd;
  Bak.API.Callbacks.DebugInd  = pMKx->API.Callbacks.DebugInd;
  Bak.API.Callbacks.C2XSecRsp = pMKx->API.Callbacks.C2XSecRsp;

  // Invoke the ioctl() on the 'cw-llc' interface
  {
    #if defined(__QNX__)
    struct ifdrv *pIfd = &(pDev->If.Ifd);
    // ioctl() with SIOCGDRVSPEC and associated data doesn't work on QNX -
    // instead we have to use MsgSend() instead
    iov_t pIov[3];
    io_devctl_t Msg;

    Msg.i.type = _IO_DEVCTL;
    Msg.i.combine_len = sizeof(Msg.i);
    Msg.i.dcmd = SIOCGDRVSPEC;
    Msg.i.nbytes = IOCPARM_LEN((unsigned)SIOCGDRVSPEC);
    Msg.i.zero = 0;

    pIfd->ifd_cmd = LLC_IOMSG_MKX;
    pIfd->ifd_len = sizeof(*pMKx);
    pIfd->ifd_data = (caddr_t)pMKx;

    SETIOV(&pIov[0], &Msg, sizeof(Msg));
    SETIOV(&pIov[1], pIfd, sizeof(*pIfd));
    SETIOV(&pIov[2], pIfd->ifd_data, pIfd->ifd_len);

    // on QNX the If.Fd returned from libpcap is for the /dev/bpf device not
    // an llc network interface socket, so need to open a new socket
    int Fd = socket(AF_INET, SOCK_DGRAM, 0);
    Res = MsgSendv_r(Fd, pIov, 3, pIov, 3);
    close(Fd);
    #else
    struct ifreq *pIfr = &(pDev->If.Ifr);
    pIfr->ifr_data = (caddr_t)pMKx;

    Res = ioctl(pDev->If.Fd, SIOCDEVPRIVATE + LLC_IOMSG_MKX, pIfr);
    #endif
    if (Res < 0)
      d_error(D_ERR, NULL, "Unable to perform ioctl on llc device %s: %s\n",
#if defined(__QNX__)
              pIfd->ifd_name,
#else
              pIfr->ifr_name,
#endif
              strerror(errno));
    //d_dump(D_VERBOSE, NULL, pMKx, sizeof(*pMKx));
  }

  // Restore the backup values and API pointers
  pMKx->pPriv                   = Bak.pPriv;
  pMKx->API.Callbacks.TxCnf     = Bak.API.Callbacks.TxCnf;
  pMKx->API.Callbacks.RxAlloc   = Bak.API.Callbacks.RxAlloc;
  pMKx->API.Callbacks.RxInd     = Bak.API.Callbacks.RxInd;
  pMKx->API.Callbacks.NotifInd  = Bak.API.Callbacks.NotifInd;
  pMKx->API.Callbacks.DebugInd  = Bak.API.Callbacks.DebugInd;
  pMKx->API.Callbacks.C2XSecRsp = Bak.API.Callbacks.C2XSecRsp;

  pMKx->API.Functions.Config      = LLC_Config;
  pMKx->API.Functions.TxReq       = LLC_TxReq;
  pMKx->API.Functions.TxFlush     = LLC_TxFlush;
  pMKx->API.Functions.TempCfg     = LLC_TempCfg;
  pMKx->API.Functions.Temp        = LLC_Temp;
  pMKx->API.Functions.PowerDetCfg = LLC_PowerDetCfg;
  pMKx->API.Functions.SetTSF      = LLC_SetTSF;
  pMKx->API.Functions.GetTSF      = LLC_GetTSF;
  pMKx->API.Functions.DebugReq    = LLC_DebugReq;
  pMKx->API.Functions.C2XSecCmd   = LLC_C2XSecReq;

  pMKx->Magic = MKX_API_MAGIC;

Error:
  d_fnend(D_API, NULL, "(pMKx %p) = %d\n", pMKx, Res);
  return Res;
}

/**
 * @copydoc MKx_Fd
 *
 */
int MKx_Fd (tMKx *pMKx)
{
  int Res = -ENOSYS;
  struct LLCDev *pDev = NULL;

  d_fnstart(D_API, NULL, "(pMKx %p)\n", pMKx);

  // Get a handle of the overarching device
  pDev = LLC_Device(pMKx);
  if (pDev == NULL)
  {
    Res = -ENODEV;
    goto Error;
  }

  // The file descriptor for the'cw-llc' netdev
  Res = pDev->If.Fd;

Error:
  d_fnend(D_API, NULL, "(pMKx %p) = %d\n", pMKx, Res);
  return Res;
}


/**
 * @copydoc MKx_Recv
 *
 */
tMKxStatus MKx_Recv (tMKx *pMKx)
{
  int Res = -ENOSYS;
  struct LLCDev *pDev = NULL;

  d_fnstart(D_API, NULL, "(pMKx %p)\n", pMKx);

  // Calling MKx_Recv(NULL) exits any loops below
  if (pMKx == NULL)
  {
    pDev = LLC_DeviceGet();
    if (pDev != NULL)
      pDev->Exit = true;
    Res = -EAGAIN;
    goto Error;
  }

  // Get a handle of the overarching device
  pDev = LLC_Device(pMKx);
  if (pDev == NULL)
  {
    Res = -ENODEV;
    goto Error;
  }
  // Exit with an error early is the 'Exit' fag is set
  if (pDev->Exit == true)
  {
    Res = -EPIPE;
    goto Error;
  }

  // Consume _all_ queued messages
  {
    int Cnt = 0;
    Res = 0;
    while ((pDev->Exit == false) && (Res == 0))
    {
      Res = LLC_MsgRecv(pDev);
      if (Res == 0)
        Cnt++;
    }
    if (Cnt > 0)
      Res = 0;
  }

Error:
  d_fnend(D_API, NULL, "(pMKx %p) = %d\n", pMKx, Res);
  return Res;
}


/**
 * @copydoc fMKx_Config
 *
 */
tMKxStatus LLC_Config (struct MKx *pMKx,
                       tMKxRadio Radio,
                       tMKxRadioConfig *pConfig)
{
  int Res = -ENOSYS;
  struct LLCDev *pDev = NULL;

  d_fnstart(D_API, NULL, "(pMKx %p)\n", pMKx);

  if ((pMKx == NULL) || (pConfig == NULL))
  {
    Res = -EINVAL;
    goto Error;
  }

  // Get a handle of the overarching device
  pDev = LLC_Device(pMKx);
  if (pDev == NULL)
  {
    Res = -ENODEV;
    goto Error;
  }

  Res = LLC_MsgCfgReq(pDev, Radio, pConfig);

Error:
  d_fnend(D_API, NULL, "(pMKx %p) = %d\n", pMKx, Res);
  return Res;
}


/**
 * @copydoc fMKx_TxReq
 *
 */
tMKxStatus LLC_TxReq (struct MKx *pMKx,
                      tMKxTxPacket *pTxPkt,
                      void *pPriv)
{
  int Res = -ENOSYS;
  struct LLCDev *pDev = NULL;

  d_fnstart(D_API, NULL, "(pMKx %p)\n", pMKx);
  if ((pMKx == NULL) || (pTxPkt == NULL))
  {
    Res = -EINVAL;
    goto Error;
  }

  // Get a handle of the overarching device
  pDev = LLC_Device(pMKx);
  if (pDev == NULL)
  {
    Res = -ENODEV;
    goto Error;
  }

  Res = LLC_MsgTxReq(pDev, pTxPkt, pPriv);

Error:
  d_fnend(D_API, NULL, "(pMKx %p) = %d\n", pMKx, Res);
  return Res;
}


/**
 * @copydoc fMKx_TxFlush
 *
 */
tMKxStatus LLC_TxFlush (struct MKx *pMKx,
                        tMKxRadio Radio,
                        tMKxChannel Chan,
                        tMKxTxQueue TxQ)
{
  int Res = -ENOSYS;
  d_fnstart(D_API, NULL, "(pMKx %p)\n", pMKx);

  // TODO

  d_fnend(D_API, NULL, "(pMKx %p) = %d\n", pMKx, Res);
  return Res;
}


/**
 * @copydoc fMKx_TempCfg
 *
 */
tMKxStatus LLC_TempCfg (struct MKx *pMKx,
                        tMKxTempConfig *pCfg)
{
  int Res = -ENOSYS;
  struct LLCDev *pDev = NULL;

  d_fnstart(D_API, NULL, "(pMKx %p)\n", pMKx);

  if ((pMKx == NULL) || (pCfg == NULL))
  {
    Res = -EINVAL;
    goto Error;
  }

  // Get a handle of the overarching device
  pDev = LLC_Device(pMKx);
  if (pDev == NULL)
  {
    Res = -ENODEV;
    goto Error;
  }

  Res = LLC_MsgTempCfgReq(pDev, pCfg);

Error:
  d_fnend(D_API, NULL, "(pMKx %p) = %d\n", pMKx, Res);
  return Res;
}


/**
 * @copydoc fMKx_Temp
 *
 */
tMKxStatus LLC_Temp (struct MKx *pMKx,
                     tMKxTemp *pTemp)
{
  int Res = -ENOSYS;
  struct LLCDev *pDev = NULL;

  d_fnstart(D_API, NULL, "(pMKx %p)\n", pMKx);

  if ((pMKx == NULL) || (pTemp == NULL))
  {
    Res = -EINVAL;
    goto Error;
  }

  // Get a handle of the overarching device
  pDev = LLC_Device(pMKx);
  if (pDev == NULL)
  {
    Res = -ENODEV;
    goto Error;
  }

  Res = LLC_MsgTempReq(pDev, pTemp);

Error:
  d_fnend(D_API, NULL, "(pMKx %p) = %d\n", pMKx, Res);
  return Res;
}

/**
 * @copydoc fMKx_PowerDetCfg
 *
 */
tMKxStatus LLC_PowerDetCfg (struct MKx *pMKx,
                        tMKxPowerDetConfig *pCfg)
{
  int Res = -ENOSYS;
  struct LLCDev *pDev = NULL;

  d_fnstart(D_API, NULL, "(pMKx %p)\n", pMKx);

  if ((pMKx == NULL) || (pCfg == NULL))
  {
    Res = -EINVAL;
    goto Error;
  }

  // Get a handle of the overarching device
  pDev = LLC_Device(pMKx);
  if (pDev == NULL)
  {
    Res = -ENODEV;
    goto Error;
  }

  Res = LLC_MsgPowerDetCfgReq(pDev, pCfg);

Error:
  d_fnend(D_API, NULL, "(pMKx %p) = %d\n", pMKx, Res);
  return Res;
}


/**
 * @copydoc fMKx_GetTSF
 *
 */
tMKxTSF LLC_GetTSF (struct MKx *pMKx)
{
  int Res = -ENOSYS;
  d_fnstart(D_API, NULL, "(pMKx %p)\n", pMKx);

  // TODO

  d_fnend(D_API, NULL, "(pMKx %p) = %d\n", pMKx, Res);
  return Res;
}

/**
 * @copydoc fMKx_SetTSF
 *
 */
tMKxStatus LLC_SetTSF (struct MKx *pMKx,
                       tMKxTSF TSF)
{
  int Res = -ENOSYS;
  struct LLCDev *pDev = NULL;

  d_fnstart(D_API, NULL, "(pMKx %p)\n", pMKx);
  if (pMKx == NULL)
  {
    Res = -EINVAL;
    goto Error;
  }

  // Get a handle of the overarching device
  pDev = LLC_Device(pMKx);
  if (pDev == NULL)
  {
    Res = -ENODEV;
    goto Error;
  }

  Res = LLC_MsgSetTSFReq(pDev, TSF);

Error:
  d_fnend(D_API, NULL, "(pMKx %p) = %d\n", pMKx, Res);
  return Res;
}


/**
 * @copydoc fMKx_TxCnf
 *
 * If the client has set the callback function pointer (non NULL) then call it!
 */
// mask by myself
/*tMKxStatus LLC_TxCnf (struct MKx *pMKx,
                      tMKxTxPacket *pTxPkt,
                      tMKxTxEvent *pTxEvent,
                      void *pPriv)
{
  int Res = LLC_STATUS_ERROR;
  d_fnstart(D_API, NULL, "(pMKx %p pTxPkt %p pTxEvent %p pPriv %p)\n",
            pMKx, pTxPkt, pTxEvent, pPriv);

  if (pMKx->API.Callbacks.TxCnf != NULL)
  {
    Res = pMKx->API.Callbacks.TxCnf(pMKx, pTxPkt, pTxEvent, pPriv);
  }

  d_fnend(D_API, NULL, "(pMKx %p pTxPkt %p pTxEvent %p pPriv %p) = %d\n",
          pMKx, pTxPkt, pTxEvent, pPriv, Res);
  return Res;
}
*/
/**
 * @copydoc fMKx_RxAlloc
 *
 * If the client has set the callback function pointer (non NULL) then call it!
 */
/*
tMKxStatus LLC_RxAlloc (struct MKx *pMKx,
                        int BufLen,
                        uint8_t **ppBuf,
                        void **ppPriv)
{
  int Res = LLC_STATUS_ERROR;
  d_fnstart(D_API, NULL, "(pMKx %p BufLen %d ppBuf %p ppPriv %p)\n",
            pMKx, BufLen, ppBuf, ppPriv);

  if (pMKx->API.Callbacks.RxAlloc != NULL)
  {
    Res = pMKx->API.Callbacks.RxAlloc(pMKx, BufLen, ppBuf, ppPriv);
  }

  d_fnend(D_API, NULL, "(pMKx %p BufLen %d ppBuf %p ppPriv %p) = %d\n",
          pMKx, BufLen, ppBuf, ppPriv, Res);
  return Res;
}*/

/**
 * @copydoc fMKx_RxInd
 *
 * If the client has set the callback function pointer (non NULL) then call it!
 */
/*
tMKxStatus LLC_RxInd (struct MKx *pMKx,
                      tMKxRxPacket *pRxPkt,
                      void *pPriv)
{
  int Res = LLC_STATUS_ERROR;
  d_fnstart(D_API, NULL, "(pMKx %p pRxPkt %p pPriv %p)\n",
            pMKx, pRxPkt, pPriv);

  if (pMKx->API.Callbacks.RxInd != NULL)
  {
    Res = pMKx->API.Callbacks.RxInd(pMKx, pRxPkt, pPriv);
  }

  d_fnend(D_API, NULL, "(pMKx %p pRxPkt %p pPriv %p) = %d\n",
          pMKx, pRxPkt, pPriv, Res);
  return Res;
}

*/
/**
 * @copydoc fMKx_NotifInd
 *
 * If the client has set the callback function pointer (non NULL) then call it!
 */
tMKxStatus LLC_NotifInd (struct MKx *pMKx,
                         tMKxNotif Notif)
{
  int Res = LLC_STATUS_ERROR;
  d_fnstart(D_VERBOSE, NULL, "(pMKx %p Notif 0x%08X)\n", pMKx, Notif);

  if (pMKx->API.Callbacks.NotifInd != NULL)
  {
    Res = pMKx->API.Callbacks.NotifInd(pMKx, Notif);
  }
  else
  {
    // No callback should not mean a failure
    Res = LLC_STATUS_SUCCESS;
  }

  d_fnend(D_VERBOSE, NULL, "(pMKx %p Notif 0x%08X) = %d\n", pMKx, Notif, Res);
  return Res;
}


/**
 * @copydoc fC2XSec_CommandReq
 *
 */
tMKxStatus LLC_C2XSecReq (struct MKx *pMKx,
                          tMKxC2XSec *pMsg)
{
  int Res = -ENOSYS;
  struct LLCDev *pDev = NULL;

  d_fnstart(D_API, NULL, "(pMKx %p)\n", pMKx);
  if ((pMKx == NULL) || (pMsg == NULL))
  {
    Res = -EINVAL;
    goto Error;
  }

  // Get a handle of the overarching device
  pDev = LLC_Device(pMKx);
  if (pDev == NULL)
  {
    Res = -ENODEV;
    goto Error;
  }

  Res = LLC_MsgSecReq(pDev, pMsg);

Error:
  d_fnend(D_API, NULL, "(pMKx %p) = %d\n", pMKx, Res);
  return Res;
}


/**
 * @copydoc fC2XSec_ResponseInd
 *
 * If the client has set the callback function pointer (non NULL) then call it!
 */
tMKxStatus LLC_C2XSecInd (struct MKx *pMKx,
                          tMKxC2XSec *pMsg)
{
  int Res = LLC_STATUS_ERROR;
  d_fnstart(D_API, NULL, "(pMKx %p pMsg %p)\n", pMKx, pMsg);

  if (pMKx->API.Callbacks.C2XSecRsp != NULL)
  {
    Res = pMKx->API.Callbacks.C2XSecRsp(pMKx, pMsg);
  }
  else
  {
    // No callback should not mean a failure
    Res = LLC_STATUS_SUCCESS;
  }

  d_fnend(D_API, NULL, "(pMKx %p pMsg %p) = %d\n", pMKx, pMsg, Res);
  return Res;
}


/**
 * @copydoc fMKx_DebugReq
 *
 */
tMKxStatus LLC_DebugReq (struct MKx *pMKx,
                         struct MKxIFMsg *pMsg)
{
  int Res = -ENOSYS;
  struct LLCDev *pDev = NULL;

  d_fnstart(D_API, NULL, "(pMKx %p)\n", pMKx);

  if ((pMKx == NULL) || (pMsg == NULL))
  {
    Res = -EINVAL;
    goto Error;
  }

  // Get a handle of the overarching device
  pDev = LLC_Device(pMKx);
  if (pDev == NULL)
  {
    Res = -ENODEV;
    goto Error;
  }

  Res = LLC_MsgDbgReq(pDev, pMsg);

Error:
  d_fnend(D_API, NULL, "(pMKx %p) = %d\n", pMKx, Res);
  return Res;
}

/**
 * @copydoc fMKx_DebugInd
 *
 * If the client has set the callback function pointer (non NULL) then call it!
 */
tMKxStatus LLC_DebugInd (struct MKx *pMKx,
                         struct MKxIFMsg *pMsg)
{
  int Res = LLC_STATUS_ERROR;
  d_fnstart(D_API, NULL, "(pMKx %p pMsg %p)\n", pMKx, pMsg);

  if (pMKx->API.Callbacks.DebugInd != NULL)
  {
    Res = pMKx->API.Callbacks.DebugInd(pMKx, pMsg);
  }
  else
  {
    // No callback should not mean a failure
    Res = LLC_STATUS_SUCCESS;
  }

  d_fnend(D_API, NULL, "(pMKx %p pMsg %p) = %d\n", pMKx, pMsg, Res);
  return Res;
}

/**
 * @}
 */

