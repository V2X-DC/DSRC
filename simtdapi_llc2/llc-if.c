/**
 * @addtogroup cohda_llc_intern_lib LLC API library
 * @{
 *
 * @file
 * LLC: 'cw-llc' monitoring application (raw interface handling)
 *
 */

//------------------------------------------------------------------------------
// Copyright (c) 2013 Cohda Wireless Pty Ltd
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Included headers
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h> // time()

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

//------------------------------------------------------------------------------
// Functions definitions
//------------------------------------------------------------------------------

/**
 * @brief Open the raw interface (using libpcap)
 * @param pDev
 * @return zero on success, otherwise a negative errno
 */
int LOCAL LLC_InterfaceSetup (struct LLCDev *pDev)
{
  int Res = -ENOSYS;
  char *pInputFileName = NULL;
  #if defined(__QNX__)
  char *pInputInterfaceName = "llc0";
  #else
  char *pInputInterfaceName = "cw-llc";
  #endif
  char ErrorString[PCAP_ERRBUF_SIZE];

  d_fnstart(D_TST, NULL, "()\n");
  d_assert(pDev != NULL);

  // Open the capture file or interface
  if (pInputFileName != NULL)
  {
    pDev->If.pCap = pcap_open_offline(pInputFileName, ErrorString);
    if (pDev->If.pCap == NULL)
    {
      d_error(D_ERR, NULL, "Open failed on '%s', %s\n",
              pInputFileName, ErrorString);
      Res = -EINVAL;
      goto Error;
    }
  }
  // OR: Open live pcap interface
  else if (pInputInterfaceName != NULL)
  {
    // PCAP: Max packet length
    int SnapLength = (1024 * 4); // 4kB
    // PCAP: promiscious mode?
    int Promisc = 1;
    // PCAP: pcap read time-out [ms]
#ifdef BOARD_QNX
    int ReadTimeout = 1;
#else
    int ReadTimeout = 1;
#endif

    pDev->If.pCap = pcap_open_live(pInputInterfaceName,
                                   SnapLength,
                                   Promisc,
                                   ReadTimeout,
                                   ErrorString);
#if defined(__QNX__)
    if (pDev->If.pCap == NULL)
    {
      // try again using a different SOCK prefix to get the correct io-pkt instance
      setenv("SOCK", "/llc", 0);
      pDev->If.pCap = pcap_open_live(pInputInterfaceName,
                                     SnapLength,
                                     Promisc,
                                     ReadTimeout,
                                     ErrorString);
    }
#endif
    if (pDev->If.pCap == NULL)
    {
      d_error(D_ERR, NULL, "Open failed on %s, %s\n",
              pInputInterfaceName, ErrorString);
      Res = -EINVAL;
      goto Error;
    }

    #if defined(__QNX__)
    // Set 'immediate' mode (no delay before returning Rx packets)
    {
      unsigned int Immediate = 1;
      Res =  ioctl(pcap_fileno(pDev->If.pCap), BIOCIMMEDIATE, &Immediate);
      if (Res != 0)
      {
        d_printf(D_WARN, NULL, "Cannot enable immediate mode %d %s\n",
                 Res, strerror(errno));
      }
    }
    #endif // __QNX__

    // Set the socket Rx buffer size
    {
      // ... Requires some inside knowledge of pcap_t (from pcap-int.h)
      int BufLen = 256*1024; // 256 KB Rx buffer
      setsockopt(pcap_fileno(pDev->If.pCap), SOL_SOCKET, SO_RCVBUF,
                 (char *)&BufLen, sizeof(BufLen));
    }

    // Check that the linktype is appropriate - i.e. RAW mode
    {
      int LinkType = pcap_datalink(pDev->If.pCap);
      d_printf(D_INFO, NULL, "PCAP LinkType is: %d\n", LinkType);

      ; // Nothing yet
    }

    // Setup the ioctl() and netdev parameters
    {
      pDev->If.Fd = pcap_fileno(pDev->If.pCap);
      d_printf(D_DBG, NULL, "pDev->If.Fd %d\n", pDev->If.Fd);

      #if defined(__QNX__)
      memset(&(pDev->If.Ifd), 0, sizeof(pDev->If.Ifd));
      snprintf(pDev->If.Ifd.ifd_name, sizeof(pDev->If.Ifd.ifd_name),
               "%s", pInputInterfaceName);
      #else
      memset(&(pDev->If.Ifr), 0, sizeof(pDev->If.Ifr));
      snprintf(pDev->If.Ifr.ifr_name, sizeof(pDev->If.Ifr.ifr_name),
               "%s", pInputInterfaceName);
      #endif
    }

    Res = 0;
  } // else if (pInputInterfaceName)

Error:
  d_fnend(D_TST, NULL, "() = %d\n", Res);
  return Res;
}

/**
 * @brief Close the raw interface (via libpcap)
 * @param pDev
 * @return zero on success, otherwise a negative errno
 */
void LOCAL LLC_InterfaceRelease (struct LLCDev *pDev)
{

  d_fnstart(D_TST, NULL, "()\n");

  if (pDev->If.pCap == NULL)
    goto Error;

  pcap_close(pDev->If.pCap);
  pDev->If.pCap = NULL;

Error:
  d_fnend(D_TST, NULL, "(pDev %p) = void\n", pDev);
  return;
}

/**
 * @brief Send the debug message over the USB interface
 * @param pDev
 * @param pMsg
 * @param Len
 * @return
 */
int LOCAL LLC_InterfaceSend (struct LLCDev *pDev,
                             char *pBuf,
                             int Len)
{
  int Cnt;
  #if defined(__QNX__)
  // QNX BPF over LLC intefaces seems to assume a header to be reserved.(radiotap or something?)
  // Bytes 13,14 is used as a 16bits payload length which overlaps with LLC's IfMsg.Radio
  // Bytes 3,4 is modified(not sure the usage,it is static)
  // Bytes 5,6 seems to be sequence number.
  // Data beyound 12 seems not touched by pcap.
  const int OrigLen = Len;
  #endif

  d_fnstart(D_TST, NULL, "()\n");

  // Send packet to over the raw interface using libpcap
  #if defined(__QNX__)
  // The hack to add 14 more byte at the head to be trashed by pcap
  // Since pcap_inject is a sink only, it should be safe to do following dirty hack.
  // The 14 bytes extra head will be dropped in llc driver.
  pBuf -= 14;
  Len += 14;
  #endif

  Cnt = pcap_inject(pDev->If.pCap, pBuf, Len);
  if (Cnt < 0)
  {
    d_error(D_WARN, NULL, "pcap_inject(%p, %p, %d) = %s\n",
            pDev->If.pCap, pBuf, Len,
            pcap_geterr(pDev->If.pCap));
    Cnt = -1;
  }
  #if defined(__QNX__)
  // make sure we return the number of bytes which the caller expected
  if (Cnt > OrigLen)
    Cnt = OrigLen;
  #endif


  d_fnend(D_TST, NULL, "(pDev %p) = %d\n", pDev, Cnt);
  return Cnt;
}


/**
 * @brief Forward all raw interface messages
 * @param pDev
 * @return zero on success, otherwise a negative errno:
 *         The functions only returns on signal interrupts or failures
 */
int LOCAL LLC_InterfaceRecv (struct LLCDev *pDev,
                             char **ppBuf,
                             int *pLen)
{
  int Res = -ENOSYS;
  struct pcap_pkthdr *pPcapHdr; // The header that pcap gives us
  char *pPcapPkt;      // The actual packet

  d_fnstart(D_TST, NULL, "()\n");
  if ((ppBuf == NULL) || (pLen == NULL))
  {
    Res = -EINVAL;
    goto Error;
  }

  // Grab a packet from the device
  Res = pcap_next_ex(pDev->If.pCap,
                     &pPcapHdr,
                     (const uint8_t **)&pPcapPkt);
  d_printf(D_DBG, NULL, "pcap_next_ex() = %d\n", Res);
  switch (Res)
  {
    case 1: // Read success
      d_printf(D_DBG, NULL, "Received a packet\n");
      *pLen = pPcapHdr->caplen;
      *ppBuf = pPcapPkt;
      Res = 0;
      break;

    case 0: // Read timeout
      d_printf(D_DBG, NULL, "read timeout\n");
      *pLen = 0;
      *ppBuf = NULL;
      Res = -EAGAIN;
      break;

    case -1: // Error
    case -2: // EOF
    default:
      d_printf(D_ERR, NULL, "pcap_next_ex() = %s\n",
               pcap_geterr(pDev->If.pCap));
      Res = -ENODEV;
      break;
  }

Error:
  d_fnend(D_TST, NULL, "() = %d\n", Res);
  return Res;
}

/**
 * @}
 */
