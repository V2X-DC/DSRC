//------------------------------------------------------------------------------
// Copyright (c) 2010 Cohda Wireless Pty Ltd
//------------------------------------------------------------------------------

#ifndef __IEEE1609__APP__TEST_MLME_IF__TESTCOMMON_H__
#define __IEEE1609__APP__TEST_MLME_IF__TESTCOMMON_H__

#ifndef ETH_ALEN
#define ETH_ALEN     6
#endif

#if defined(__QNX__)
#include <unistd.h>
#include <arpa/inet.h>

struct ethhdr {
  unsigned char      h_dest[ETH_ALEN];   /* destination eth addr */
  unsigned char      h_source[ETH_ALEN]; /* source ether addr    */
  uint16_t/*__be16*/ h_proto;            /* packet type ID field */
} __attribute__((packed));
#else
#include <linux/if_ether.h>
#include <getopt.h> // getopts
#endif
#include <stdint.h>
#include "mkxtest.h"
#include "mk2mac-api-types.h"

#if defined(__QNX__)
#define cpu_to_le16s(x) do { (*(x)) = ENDIAN_LE16(*(x));} while (0)
#define cpu_to_be16s(x) do { (*(x)) = ENDIAN_BE16(*(x));} while (0)
#else
#define cpu_to_le16s(x) do { (*(x)) = htole16(*(x));} while (0)
#define cpu_to_be16s(x) do { (*(x)) = htobe16(*(x));} while (0)
#endif

/// Command line options def
typedef struct UsageOption_tag
{
  char Letter;
  char *pName;
  char *pDescription;
  char *pValues;
} tUsageOption;

/// Payload Modes (Random, Incrementing, Sequence Number or uint8_t)
typedef enum PayloadMode_tag
{
  PAYLOADMODE_BYTE = 0,
  PAYLOADMODE_SEQNUM = 1,
  PAYLOADMODE_RANDOM = 2,
  PAYLOADMODE_INCREMENT = 3,
  PAYLOADMODE_TIME = 4
} ePayloadMode;
typedef uint8_t tPayloadMode;

struct MKxChanConfig;

//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------
int GetMacFromString (uint8_t * MacAddr, const char * MacAddrStr);

void EthHdr_fprintf (FILE *fp, struct ethhdr * pEthHdr);
void Dot11Hdr_fprintf (FILE *fp, struct IEEE80211MACHdr * pDot11Hdr);
void Payload_fprintf (FILE * fp,
                      unsigned char *pPayload,
                      int PayloadLen,
                      bool DumpPayload);
void Payload_gen (unsigned char *pPayload,
                  int PayloadLen,
                  uint32_t SeqNum,
                  tPayloadMode PayloadMode,
                  uint8_t PayloadByte);
int GetMacFromString (uint8_t * MacAddr,
                      const char * MacAddrStr);
void MACAddr_fprintf (FILE *fp, uint8_t * pAddr);
void MK2ChanProfile_Print (tMK2ChanProfile * pChanProfile, char *IndentStr);
void MKxChanConfig_Print (const struct MKxChanConfig *pCfg, char *IndentStr);
#if !defined(__QNX__)
void copyopts (struct option * pLongOpts, char * pShortOpts);
#endif

#endif // __IEEE1609__APP__TEST_MLME_IF__TESTCOMMON_H__
