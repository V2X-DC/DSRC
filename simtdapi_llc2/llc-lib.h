/**
 * @addtogroup cohda_llc_intern_lib LLC API library
 * @{
 *
 * @file
 * LLC: API library management related functionality
 *
 */

//------------------------------------------------------------------------------
// Copyright (c) 2013 Cohda Wireless Pty Ltd
//------------------------------------------------------------------------------

#ifndef __COHDA__APP__LLC__LIB__LLC_LIB_H__
#define __COHDA__APP__LLC__LIB__LLC_LIB_H__
#ifndef __KERNEL__

//------------------------------------------------------------------------------
// Included headers
//------------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <pcap.h>

#include "linux/cohda/llc/llc-api.h"

//------------------------------------------------------------------------------
// Macros & Constants
//------------------------------------------------------------------------------

/// Makes the functions hidden within a .so library
#define LOCAL __attribute__((visibility("hidden")))

// Catchall error codes
#define LLC_STATUS_SUCCESS (0)
#define LLC_STATUS_ERROR (-1)

/// 'Magic' value inside a #tLLCDev
#define LLC_DEV_MAGIC          (MKX_API_MAGIC)

/// Maximum possible seqnum to allocate for outgoing TxReq's
#define LLC_SEQNUM_MAX 4095

//------------------------------------------------------------------------------
// Type definitions
//------------------------------------------------------------------------------

typedef struct LLCTxReq
{
  /// Copy of the original TxReq message
  struct MKxTxPacket *pTxPacket;
  /// Storage for the private data passed to MKx_TxReq()
  void *pPriv;
} tLLCTxReq;

/// LLC device structure
typedef struct LLCDev
{
  /// Value is LLC_DEV_MAGIC if this is a valid structure
  uint16_t Magic;
  /// 16 bits reserved for sundry status information
  uint16_t Unused;
  /// The MKx API object
  struct MKx MKx;

  /// Interface handling parameters
  struct
  {
    /// libpcap handle for the raw socket
    pcap_t *pCap;
    /// The 'llc' interface file number (for internal ioctls)
    int Fd;
    /// Structure for issuing ioctl requests
    struct ifreq Ifr;
    #if defined(__QNX__)
    /// Structure for issuing ioctl requests
    struct ifdrv Ifd;
    #endif
  } If;

  /// Msg specific
  struct
  {
    /// RxAlloc() buffer length
    int RxLen;
    /// RxAlloc() buffer pointer
    uint8_t *pRxBuf;
    /// RxAlloc() private pointer
    void *pRxPriv;

    /// Storage of outgoing TxReq's (to associate with corresponding TxCnf)
    tLLCTxReq TxReqs[LLC_SEQNUM_MAX + 1];
    /// Counter for next sequence number into TxReqs
    uint16_t Cnt;
  } Msg;

  /// Exit flag
  bool Exit;

} tLLCDev;

//------------------------------------------------------------------------------
// Function declarations
//------------------------------------------------------------------------------

int LOCAL LLC_DeviceSetup (struct LLCDev *pDev);
void LOCAL LLC_DeviceRelease (struct LLCDev *pDev);
struct LOCAL LLCDev *LLC_DeviceGet (void);
struct LOCAL LLCDev *LLC_Device (struct MKx *pMKx);

int LOCAL LLC_InterfaceSetup (struct LLCDev *pDev);
void LOCAL LLC_InterfaceRelease (struct LLCDev *pDev);
int LOCAL LLC_InterfaceSend (struct LLCDev *pDev,
                             char *pBuf,
                             int Len);
int LOCAL LLC_InterfaceRecv (struct LLCDev *pDev,
                             char **ppBuf,
                             int *pLen);


tMKxStatus LOCAL LLC_Config (struct MKx *pMKx,
                             tMKxRadio Radio,
                             tMKxRadioConfig *pConfig);
tMKxStatus LOCAL LLC_TxReq (struct MKx *pMKx,
                            tMKxTxPacket *pTxPkt,
                            void *pPriv);
tMKxStatus LOCAL LLC_TxFlush (struct MKx *pMKx,
                              tMKxRadio Radio,
                              tMKxChannel Chan,
                              tMKxTxQueue TxQ);
tMKxStatus LOCAL LLC_TempCfg (struct MKx *pMKx,
                              tMKxTempConfig *pCfg);
tMKxStatus LOCAL LLC_Temp (struct MKx *pMKx,
                           tMKxTemp *pTemp);
tMKxStatus LOCAL LLC_PowerDetCfg (struct MKx *pMKx,
                                  tMKxPowerDetConfig *pCfg);
tMKxTSF LOCAL LLC_GetTSF (struct MKx *pMKx);
tMKxStatus LOCAL LLC_SetTSF (struct MKx *pMKx,
                             tMKxTSF TSF);
tMKxStatus LOCAL LLC_TxCnf (struct MKx *pMKx,
                            tMKxTxPacket *pTxPkt,
                            tMKxTxEvent *pTxEvent,
                            void *pPriv);
tMKxStatus LOCAL LLC_RxAlloc (struct MKx *pMKx,
                              int BufLen,
                              uint8_t **ppBuf,
                              void **ppPriv);
tMKxStatus LOCAL LLC_RxInd (struct MKx *pMKx,
                            tMKxRxPacket *pRxPkt,
                            void *pPriv);
tMKxStatus LOCAL LLC_NotifInd (struct MKx *pMKx,
                               tMKxNotif Notif);
tMKxStatus LOCAL LLC_DebugReq (struct MKx *pMKx,
                               struct MKxIFMsg *pMsg);
tMKxStatus LOCAL LLC_DebugInd (struct MKx *pMKx,
                               struct MKxIFMsg *pMsg);
tMKxStatus LLC_C2XSecReq (struct MKx *pMKx,
                          tMKxC2XSec *pMsg);
tMKxStatus LLC_C2XSecInd (struct MKx *pMKx,
                          tMKxC2XSec *pMsg);
int LOCAL LLC_MsgSetup (struct LLCDev *pDev);
int LOCAL LLC_MsgRelease (struct LLCDev *pDev);
int LOCAL LLC_MsgRecv (struct LLCDev *pDev);

int LOCAL LLC_MsgCfgReq (struct LLCDev *pDev,
                         tMKxRadio Radio,
                         tMKxRadioConfig *pConfig);
int LOCAL LLC_MsgTxReq (struct LLCDev *pDev,
                        tMKxTxPacket *pTxPkt,
                        void *pPriv);
int LOCAL LLC_MsgTempCfgReq (struct LLCDev *pDev,
                             tMKxTempConfig *pCfg);
int LOCAL LLC_MsgPowerDetCfgReq (struct LLCDev *pDev,
                                 tMKxPowerDetConfig *pCfg);
int LOCAL LLC_MsgTempReq (struct LLCDev *pDev,
                          tMKxTemp *pTemp);
int LOCAL LLC_MsgDbgReq (struct LLCDev *pDev,
                         struct MKxIFMsg *pMsg);
int LOCAL LLC_MsgSecReq (struct LLCDev *pDev,
                         tMKxC2XSec *pMsg);
int LOCAL LLC_MsgSetTSFReq (struct LLCDev *pDev,
                            tMKxTSF TSF);

#else
#error This file should not be included from kernel space.
#endif // #ifndef __KERNEL__
#endif // #ifndef __COHDA__APP__LLC__LIB__LLC_LIB_H__
/**
 * @}
 */
