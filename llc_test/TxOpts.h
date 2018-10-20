//------------------------------------------------------------------------------
// Copyright (c) 2010 Cohda Wireless Pty Ltd
//------------------------------------------------------------------------------

#ifndef __IEEE1609__APP__TEST_MLME_IF__TXOPTS_H__
#define __IEEE1609__APP__TEST_MLME_IF__TXOPTS_H__

//------------------------------------------------------------------------------
// Included headers
//------------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>
#include "mk2mac-api-types.h"
#include "test-common.h"

//------------------------------------------------------------------------------
// Macros & Constants
//------------------------------------------------------------------------------

/// Maximum length of option lists
#define TXOPTS_MAXOPTARGLISTLEN      (10)
#define TXOPTS_MAXFILENAMELENGTH     (512)
#define TXOPTS_MINPACKETLENGTH       (50)
#define TXOPTS_MAXFILTERTABLELEN     (4)
#define TXOPTS_MAXIFNAMELEN          (64)

#define TXOPTS_TZSPPORT 37008 // default port to listen on
//------------------------------------------------------------------------------
// Type definitions
//------------------------------------------------------------------------------

/**
 * TxOpt Error Codes
 */
typedef enum TxOptsErrCode_tag
{
  TXOPTS_ERR_NONE = 0,
  TXOPTS_ERR_INVALIDOPTION = 1,
  TXOPTS_ERR_INVALIDOPTIONARG = 2,
  TXOPTS_ERR_PACKETLENGTHTOOSHORT = 3
} eTxOptsErrCode;
typedef int tTxOptsErrCode;

/// Modes
typedef enum TxMode_tag
{
  TXOPTS_MODE_CREATE = 0,
  TXOPTS_MODE_TZSPFWD = 1
} eTxMode;
typedef int tTxMode;

/// Specify an integer list
typedef struct RangeSpec_tag
{
  long Start;
  long Step;
  long Stop;
} tRangeSpec;

/// Options for CCH or SCH. Some fields may be lists that will be looped thru
typedef struct TxCHOpts_tag
{

  //------------------------------------------------
  // Tx Descriptor lists
  /// Indicate the channel number that should be used (e.g. 172)
  tMK2ChannelNumber ChannelNumber;
  /// Indicate the priority to used
  tMK2Priority Priority;
  /// Indicate the 802.11 Service Class to use
  tMK2Service Service;
  /// Indicate the MCS to be used (may specify default or TRC)
  tMK2MCS pMCS[TXOPTS_MAXOPTARGLISTLEN];
  uint8_t NMCS;
  /// What Tx Power mode is to be used ( manual, default or TPC)
  tMK2TxPwrCtl TxPwrCtrl;
  /// Indicate the power to be used in Manual mode
  tRangeSpec TxPower; // Start, Step, End
  /// Indicate the antenna upon which packet should be transmitted
  tMK2TxAntenna pTxAntenna[TXOPTS_MAXOPTARGLISTLEN];
  uint8_t NTxAnt;

  /// Indicate the expiry time (0 means never)
  tMK2Time Expiry;

  /// Packet length in Bytes, list of ranges
  tRangeSpec PacketLength[TXOPTS_MAXOPTARGLISTLEN]; // Start, Step, End
  uint8_t NPacketLengths; // number of packet length ranges

  /// DA to use in test packets to WAVE-RAW
  tMK2MACAddr DestAddr;
  /// Ethernet Type to use in Ethernet Header input to WAVE_RAW
  uint16_t EtherType;

  /// UDP socket to listen on and forward packets from
  int UDPforwardSocket;
  
  /// TCP socket to listen on and forward packets from
  int TCPforwardSocket;

} tTxCHOpts;

/// Tx Configuration (built from Cmd line opts).
/// Stored in form ready for MLME_IF call
/// Some experiment control at top level.
typedef struct TxOpts_tag
{

  /// Forward of Create
  tTxMode Mode;

  /// TZSP Port (used to listen in forwarding mode)
  uint16_t TZSPPort;

  /// Channel Options
  tTxCHOpts TxCHOpts;

  /// SA to use in test packets (if supplied, driver default used otherwise)
  tMK2MACAddr SrcAddr;

  /// Interface (Wave Raw or Mgmt)
  char pInterfaceName[TXOPTS_MAXIFNAMELEN];

  /// Number of Packets to be transmitted in total
  uint32_t NumberOfPackets;

  /// packet Log File name
  char pPacketLogFileName[TXOPTS_MAXFILENAMELENGTH];

  /// Target number of packets per second
  float TargetPacketRate_pps;

  /// Dump entire packet payload to file
  bool DumpPayload;

  /// Should per packet details be dumped to stdout?
  bool DumpToStdout;

  /// How should the payload be generated?
  tPayloadMode PayloadMode;
  uint8_t PayloadByte; // if needed based on Mode

  /// UDP port to listen on and forward packets from
  int32_t UDPforwardPort;

} tTxOpts;

void TxOpts_PrintUsage ();
void TxOpts_Print (tTxOpts * pTxOpts);
int TxOpts_New (int argc, char **argv, tTxOpts *pTxOpts);

#endif // __IEEE1609__APP__TEST_MLME_IF__TXOPTS_H__
