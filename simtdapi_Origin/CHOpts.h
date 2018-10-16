//------------------------------------------------------------------------------
// Copyright (c) 2010 Cohda Wireless Pty Ltd
//------------------------------------------------------------------------------

#ifndef __CHOPTS_H__
#define __CHOPTS_H__

//------------------------------------------------------------------------------
// Included headers
//------------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>
#include "mk2mac-api-types.h"

//------------------------------------------------------------------------------
// Macros & Constants
//------------------------------------------------------------------------------

/// Maximum length of option lists
#define CHOPTS_MAXOPTARGLISTLEN      (10)
#define CHOPTS_MAXFILENAMELENGTH     (512)
#define CHOPTS_MINPACKETLENGTH       (50)
#define CHOPTS_MAXFILTERTABLELEN     (4)
#define CHOPTS_MAXIFNAMELEN          (64)

//------------------------------------------------------------------------------
// Type definitions
//------------------------------------------------------------------------------
/// CHOpt radio selection
typedef enum
{
  CHOPTS_RADIO_A = 0,
  CHOPTS_RADIO_B = 1,
} eCHOptsRadio;
/// @copydoc eCHOptsRadio
typedef uint8_t tCHOptsRadio;

/// CHOpt Error Codes
typedef enum CHOptsErrCode_tag
{
  CHOPTS_ERR_NONE = 0,
  CHOPTS_ERR_INVALIDOPTION = 1,
  CHOPTS_ERR_INVALIDOPTIONARG = 2,
} eCHOptsErrCode;

typedef int tCHOptsErrCode;

/// Channel Configuration (built from Cmd line opts).
/// Stored in form ready for MLME_IF call
typedef struct CHOpts_tag
{

  /// Channel profile describing CCH or SCH
  tMK2ChanProfile ChanProfile;

  /// Channel Type (CCH or SCH)
  int Channel;

  /// Radio channel to use (A or B)
  int Radio;

  /// Interface (Wave Raw or Mgmt)
  char pInterfaceName[CHOPTS_MAXIFNAMELEN];

  /// Ethernet Types for filtering (Tx and Rx)
  uint16_t pFilter[CHOPTS_MAXFILTERTABLELEN];
  uint8_t NFilter;

  /// Command: either set of get parameters from chan profile
  int Command;

  /// MAC address: if (Msg.DA==MACAddr) { Receive(Msg); }
  uint8_t MACAddr[6];

} tCHOpts;

//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------
void CHOpts_PrintUsage();
void CHOpts_Print (tCHOpts * pCHOpts);
int CHOpts_New (int argc,
                char **argv,
                tCHOpts *pCHOpts);





#endif // __CHOPTS_H__
