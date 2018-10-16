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

//------------------------------------------------------------------------------
// Included headers
//------------------------------------------------------------------------------
#include <stddef.h>
#include "linux/cohda/llc/llc-api.h"
#include "llc-lib.h"

#include "debug-levels.h"

//------------------------------------------------------------------------------
// Macros & Constants
//------------------------------------------------------------------------------

#define container_of(ptr, type, member) ({ \
    const typeof( ((type *)0)->member ) *__mptr = (ptr); \
    (type *)( (char *)__mptr - offsetof(type,member) );})

//------------------------------------------------------------------------------
// Type definitions
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Variables
//------------------------------------------------------------------------------

/// Local copy of lib pointer
struct LLCDev *_pDev = NULL;

//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------

/**
 * @brief Setup the LLC device structure
 * @param pDev LLC device pointer
 * @return Zero (LLC_STATUS_SUCCESS) or a negative errno
 */
int LOCAL LLC_DeviceSetup (struct LLCDev *pDev)
{
  int Res;

  d_fnstart(D_DBG, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);

  memset(pDev->Msg.TxReqs, 0, sizeof(pDev->Msg.TxReqs));

  // Enable MKx_Recv()
  pDev->Exit = false;

  // Open the 'cw-llc' network interface
  Res = LLC_InterfaceSetup(pDev);
  if (Res != 0)
    goto ErrorInterface;
  // Setup the message specific functionality
  Res = LLC_MsgSetup(pDev);
  if (Res != 0)
    goto ErrorMsg;

  pDev->Magic = LLC_DEV_MAGIC;
  _pDev = pDev;
  Res = 0;
  goto Success;

  // cppcheck-suppress unreachableCode
  LLC_MsgRelease(pDev);
ErrorMsg:
  LLC_InterfaceRelease(pDev);
ErrorInterface:

Success:
  d_fnend(D_DBG, NULL, "(pDev %p) = %d\n", pDev, Res);
  return Res;
}

/**
 * @brief Release (cleanup) the LLC device structure
 * @param pDev LLC device pointer
 * @return Zero (LLC_STATUS_SUCCESS) or a negative errno
 */
void LOCAL LLC_DeviceRelease (struct LLCDev *pDev)
{
  int Res = LLC_STATUS_ERROR;

  d_fnstart(D_DBG, NULL, "(pDev %p)\n", pDev);
  d_assert(pDev != NULL);

  pDev->Exit = true;

  if (pDev == _pDev)
  {
    LLC_MsgRelease(pDev);
    // Close the 'cw-llc' network interface
    LLC_InterfaceRelease(pDev);

    _pDev = NULL;
  }

  d_fnend(D_DBG, NULL, "(pDev %p) = void '%d'\n", pDev, Res);
  return;
}

/**
 * @brief Get the LLC device structure
 * @return LLC device pointer
 */
struct LLCDev LOCAL *LLC_DeviceGet (void)
{
  return _pDev;
}

/**
 * @brief Morph the API pointer to the internal device strucuture
 * @param pMKx MKx handle
 * @return A handle to the internal device
 */
struct LLCDev LOCAL *LLC_Device (struct MKx *pMKx)
{
  struct LLCDev *pDev;

  d_assert(pMKx != NULL);

  pDev = container_of(pMKx, struct LLCDev, MKx);

  if (pDev->Magic != LLC_DEV_MAGIC)
    pDev = NULL;

  d_printf(D_DBG, NULL, "pDev %p _pDev %p\n", pDev, _pDev);
  d_assert(pDev == _pDev);
  return pDev;
}


/**
 * @}
 */
