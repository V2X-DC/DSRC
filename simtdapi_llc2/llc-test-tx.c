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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <math.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "linux/cohda/pktbuf.h"
#include "linux/cohda/llc/llc.h"
#include "linux/cohda/llc/llc-api.h"
#include "dot4-internal.h"
#include "mk2mac-api-types.h"
#include "test-common.h"
#include "llc-plugin.h"
#include "debug-levels.h"
#include "TxOpts.h"
#include "llc-test-tx.h"
#include "um220-good.h"
#include "CarSta.h"
#include "TimerTask.h"
#include "timer_queue.h"

// this is defined via endian.h except on the 12.04 VM
#ifndef htobe16
uint16_t htobe16(uint16_t host_16bits);
#endif
#ifndef htole16
uint16_t htole16(uint16_t host_16bits);
#endif

//------------------------------------------------------------------------------
// Macros & Constants
//------------------------------------------------------------------------------
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define MALLOC_SIZE_MKxTxPacket 2048

#define UNIXSTRTCP_PATH "unixstr"
#define UNIXSTRUDP_PATH "unixdg"


#define LLC_ALLOC_EXTRA_HEADER (56)
#define LLC_ALLOC_EXTRA_FOOTER (16)

const int POLL_INPUT = (POLLIN | POLLPRI);
const int POLL_ERROR = (POLLERR | POLLHUP | POLLNVAL); 


//------------------------------------------------------------------------------
// Type definitions
//------------------------------------------------------------------------------



typedef struct CwMessage
{
    char protocol;
    uint16_t length;
    unsigned char data[2048];
    uint32_t seq;
    uint8_t hop;
}CwMessage;

union float2char
{
	float d;
	unsigned char data[4];
};

union double2char
{
	double d;
	unsigned char data[8];
};

union uint32_t2char
{
	uint32_t d;
	unsigned char data[4];
};

//------------------------------------------------------------------------------
// Function Prototypes
//------------------------------------------------------------------------------

//static int LLC_TxCmd (struct PluginCmd *pCmd, int Argc, char **ppArgv);
static int LLC_TxMain (int Argc, char **ppArgv);

//ȫ�ֱ���
char buff[MALLOC_SIZE_MKxTxPacket];
//����ָ��
static int (*func)(void *, uint16_t);

//------------------------------------------------------------------------------
// Variables
//------------------------------------------------------------------------------

/// App/command state
static struct LLCTx _Dev =
{
  .pMKx = NULL,
  .Exit = false,
};
struct LLCTx *pDev = &(_Dev);//pDev��һ����̬�ṹ�������
tTxOpts *pTxOpts;
pstRMCmsg result = NULL;
static bool UdpEnabled,TcpEnabled;
static struct sockaddr_un UdpCltaddr, TcpCltaddr;
static int tcpfd, udpfd, listenfd, um220fd;
socklen_t tcpaddrlen, udpaddrlen;
struct pollfd Fds[4] = { {-1, },  //MKx Recv
					  {-1, }, //Tcp Socket
					  {-1, },  //Udp Socket
					  {-1, },  //um220 & mpu6050
			};

extern CStatus CarS;


//Declare the debug level
D_LEVEL_DECLARE();
//------------------------------------------------------------------------------
// Functions definitions
//------------------------------------------------------------------------------

/**
 * @brief Called by the llc app when the 'dbg' command is specified
 */

/**
 * @brief Print capture specific usage info
 */
static int LLC_TxUsage (void)
{
 /* Plugin_CmdPrintUsage(&TxCmd);*/
	fprintf(stderr, "%s", "test-tx <Option>\n");
	fprintf(stderr, "%s", "-h, --Help|Usage\n");
	fprintf(stderr, "%s", "        Print this Usage\n");
	fprintf(stderr, "%s", "-f, --LogFile\n");
	fprintf(stderr, "%s", "        Log File Name\n");
	fprintf(stderr, "%s", "        string\n");
	fprintf(stderr, "%s", "-s, --SrcAddr\n");
	fprintf(stderr, "%s", "        Source MAC Address\n");
	fprintf(stderr, "%s", "        aa:bb:cc:dd:ee:ff\n");
	fprintf(stderr, "%s", "-n, --NPackets\n");
	fprintf(stderr, "%s", "        Total Number of Packet to Tx\n");
	fprintf(stderr, "%s", "        uint32\n");
	fprintf(stderr, "%s", "-r, --PacketRate\n");
	fprintf(stderr, "%s", "        Packet Rate in Packets Per Second\n");
	fprintf(stderr, "%s", "        positive float\n");
	fprintf(stderr, "%s", "-y, --DumpPayload\n");
	fprintf(stderr, "%s", "        Dump Packet Payload to log file\n");
	fprintf(stderr, "%s", "-o, --DumpToStdout\n");
	fprintf(stderr, "%s", "        Dump packets to Standard output\n");
	fprintf(stderr, "%s", "-g, --PayloadMode\n");
	fprintf(stderr, "%s", "        Method used to set Payload bytes\n");
	fprintf(stderr, "%s", "        seqnum | increment | random | uint8_t | timed\n");
	fprintf(stderr, "%s", "-i, --Interface\n");
	fprintf(stderr, "%s", "        Network interface to transmit on\n");
	fprintf(stderr, "%s", "        wave-raw | wave-mgmt\n");
	fprintf(stderr, "%s", "-t, --TZSP\n");
	fprintf(stderr, "%s", "        Forward TZSP encapsulated packets?\n");
	fprintf(stderr, "%s", "-z, --TZSP_Port\n");
	fprintf(stderr, "%s", "        Listen for TZSP Packets on this Port Number\n");
	fprintf(stderr, "%s", "        uint16\n");
	fprintf(stderr, "%s", "-c, --ChannelNumber\n");
	fprintf(stderr, "%s", "        Attempt to Tx on this Channel Number\n");
	fprintf(stderr, "%s", "        uint8\n");
	fprintf(stderr, "%s", "-l, --PktLen\n");
	fprintf(stderr, "%s", "        Packet Length (Bytes) excluding TxDesc and EtherHeader\n");
	fprintf(stderr, "%s", "        List of Start[:Step][:End], uint16 >=50\n");
	fprintf(stderr, "%s", "-m, --MCS\n");
	fprintf(stderr, "%s", "        Modulation and Coding Scheme\n");
	fprintf(stderr, "%s", "        List of MK2MCS_R12BPSK | MK2MCS_R34BPSK | MK2MCS_R12QPSK | MK2MCS_R34QPS\n");
	fprintf(stderr, "%s", "K | MK2MCS_R12QAM16 | MK2MCS_R34QAM16 | MK2MCS_R23QAM64 | MK2MCS_R34QAM64 | MK2M\n");
	fprintf(stderr, "%s", "CS_DEFAULT | MK2MCS_TRC\n");
	fprintf(stderr, "%s", "-w, --TxPwrCtrl\n");
	fprintf(stderr, "%s", "        Tx Power Control Mode\n");
	fprintf(stderr, "%s", "        MK2TPC_MANUAL | MK2TPC_DEFAULT | MK2TPC_TPC\n");
	fprintf(stderr, "%s", "-p, --Power\n");
	fprintf(stderr, "%s", "        Tx Power (Half dBm)\n");
	fprintf(stderr, "%s", "        Start[:Step][:End]\n");
	fprintf(stderr, "%s", "-x, --Expiry\n");
	fprintf(stderr, "%s", "        Expiry (us)\n");
	fprintf(stderr, "%s", "        uint64\n");
	fprintf(stderr, "%s", "-q, --Priority\n");
	fprintf(stderr, "%s", "        Priority\n");
	fprintf(stderr, "%s", "        MK2_PRIO_0 | MK2_PRIO_1 | MK2_PRIO_2 | MK2_PRIO_3 | MK2_PRIO_4 | MK2_PRIO_5 | MK2_PRIO_6 | MK2_PRIO_7\n");
	fprintf(stderr, "%s", "-v, --Service\n");
	fprintf(stderr, "%s", "        802.11 Service Class\n");
	fprintf(stderr, "%s", "        MK2_QOS_ACK | MK2_QOS_NOACK\n");
	fprintf(stderr, "%s", "-a, --TxAnt\n");
	fprintf(stderr, "%s", "        Tx Antenna to use\n");
	fprintf(stderr, "%s", "        List of MK2_TXANT_DEFAULT | MK2_TXANT_ANTENNA1 | MK2_TXANT_ANTENNA2 | MK2_TXANT_ANTENNA1AND2\n");
	fprintf(stderr, "%s", "-e, --EtherType\n");
	fprintf(stderr, "%s", "        Ethernet Type\n");
	fprintf(stderr, "%s", "        uint16\n");
	fprintf(stderr, "%s", "-d, --DstAddr\n");
	fprintf(stderr, "%s", "        Destination MAC Address\n");
	fprintf(stderr, "%s", "        aa:bb:cc:dd:ee:ff\n");
	fprintf(stderr, "%s", "-u, --UDPforwardPort\n");
	fprintf(stderr, "%s", "        UDP forwarding port\n");
	fprintf(stderr, "%s", "        12345\n");
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
static void LLC_TxSignal (int SigNum)
{
	switch(SigNum)
	{
			  
		  case SIGPIPE:
			  TcpEnabled = false;
			  Fds[1].fd = listenfd;
			  Fds[1].events = POLL_INPUT;
			  if(tcpfd != -1){
				  close(tcpfd);
				  tcpfd = -1;
			  }
			  printf("SIGPIPE SigNum: %d\n", SigNum);
			  break;
		  case SIGSEGV:printf("SEGMENTATION FAULT!!!! Exiting!!! \n To get a core dump, compile with DEBUG option.\n");
		  case SIGINT:
		  case SIGTERM://����Ӧ�ü�һ������ڴ���������ټ�
		  case SIGQUIT:
		  case SIGHUP:
		  default:
			  printf("receive signal error: %d \n", SigNum);
			  pDev->TxContinue = false;
			  // Break the MKx_Recv() loops
			  (void)MKx_Recv(NULL);
			  break;
	}
	return;

}

char checkSum(char * buf, uint16_t size)
{
	char check;
    int i;
	check = buf[0];
	for(i = 1; i < size; i++)
		check = check ^ buf[i];

	return check; //这个函数会不会返回的时候就被清0 ？
}

//这里是否可以考虑发送的路由，就是根据目的节点是广播节点还是单播节点进行路由选择？？
int packetmessage(CwMessage *message)
{
	memset(buff, 0, sizeof(buff));//将缓存buffer清零
	buff[0] = 0x29;
	buff[1] = 0x29;
	buff[2] = message->protocol;//协议号
	buff[6] = ((message->seq) >> 24) & 0x000000ff;	//序列号高24~31位
	buff[5] = ((message->seq) >> 16) & 0x000000ff;	//序列号高16~23位
	buff[4] = ((message->seq) >> 8) & 0x000000ff;		//序列号高8~15位
	buff[3] = (message->seq) & 0x000000ff;            //序列号0~7位
	buff[7] = message->hop;	//跳数
	buff[9] = (message->length >> 8) & 0x00ff;//取高8~15位
	buff[8] = message->length & 0x00ff;//取低0~7位
	memcpy(&buff[10], message->data, message->length);
	buff[message->length+8] = checkSum(buff, message->length + 8);//除了校验和包尾
	buff[message->length+9] = 0x0d;    //包尾
	return message->length + 10;//整个要发送的包长
//    Tx_Send(message->pTxOpts,  message->length + 12);	
}

void packetandroidmessage(CwMessage *message)
{
	struct sockaddr_in UdpCltaddr;
//	socklen_t addrlen;
//    addrlen = sizeof(UdpCltaddr);
	memset(buff, 0, sizeof(buff));//将缓存buffer清零
	buff[0] = 0x29;
	buff[1] = 0x29;
	buff[2] = message->protocol;//协议号
	buff[6] = ((message->seq) >> 24) & 0x000000ff;	//序列号高24~31位
	buff[5] = ((message->seq) >> 16) & 0x000000ff;	//序列号高16~23位
	buff[4] = ((message->seq) >> 8) & 0x000000ff;	//序列号高8~15位
	buff[3] = (message->seq) & 0x000000ff;          //序列号0~7位
	buff[7] = message->hop;	//跳数
	buff[9] = (message->length >> 8) & 0x00ff;//取高8~15位
	buff[8] = (message->length ) & 0x00ff;//取低0~7位
	memcpy(&buff[10], message->data, message->length);
	buff[message->length+8] = checkSum(buff, message->length + 8);//除了校验和包尾
	buff[message->length+9] = 0x0d;    //包尾

	if(UdpEnabled){
		 sendto(udpfd, buff, message->length + 10, 0, (struct sockaddr*)&UdpCltaddr, udpaddrlen);
	}

	if(TcpEnabled){
		 if(write(tcpfd, buff, message->length + 10) < 0){
			 perror("Socet error");
			 //这里应该有出错处理的，但是在这里不是很方便处理
		 }
	} 
}

/**
 * @brief Transmit frame(s) on the opened interface using the CCH or SCH config
 * @param pTxOpts the options used to config the channel for sending
 * @param Pause_us wait this long inline with tx Loop
 * @return the number of bytes sent (-1 is failure)
 *
 * Loops through several variables transmitting several packets in a single call.
 * Loops though
 *   - Packet Length
 *   - Tx Power
 *   - MCS
 *
 * Source Address already loaded into tTx object
 */
static int Tx_Send (tTxOpts * pTxOpts, int length)
{
  tTxErrCode ErrCode = TX_ERR_NONE;

  int m; // loop vars
  int ThisFrameLen;
  struct IEEE80211QoSHeader *pMAC;
  struct SNAPHeader *pSNAP;

  unsigned char *pPayload;
  long TxPower;
  tTxCHOpts *pTxCHOpts;
  tMKxRadio RadioID;
  tMKxChannel ChannelID;
  int Freq;
  struct MKxTxPacket *pTxPacket = malloc(MALLOC_SIZE_MKxTxPacket);
  struct MKxTxPacketData *pPacket = &pTxPacket->TxPacketData;
  fMKx_TxReq LLC_TxReq = pDev->pMKx->API.Functions.TxReq;
  const tMKxRadioConfigData *pRadio = pDev->pMKx->Config.Radio;

//  d_fnstart(D_DEBUG, pDev, "(pDev %p, pTxOpts %p, Pause_us %d)\n", pDev, pTxOpts,Pause_us);
  d_assert(pTxOpts != NULL);

  memset(pTxPacket, 0, MALLOC_SIZE_MKxTxPacket);

  // Get existing handles from Tx Object
  pTxCHOpts = &(pTxOpts->TxCHOpts);
  Freq = pTxCHOpts->ChannelNumber * 5 + 5000;

  d_assert(pTxCHOpts != NULL);

  //--------------------------------------------------------------------------
  // WAVE-RAW frame: | TxDesc | Eth Header | Protocol & Payload |
  pMAC = (struct IEEE80211QoSHeader *)pPacket->TxFrame;
  pSNAP = (struct SNAPHeader *) (pPacket->TxFrame + sizeof(*pMAC));
  pPayload = (unsigned char *) (pPacket->TxFrame + sizeof(*pMAC) + sizeof(*pSNAP));

  // Setup the 802.11 MAC QoS header
  pMAC->FrameControl.FrameCtrl = 0;
  pMAC->FrameControl.Fields.Type = MAC_FRAME_TYPE_DATA;
  pMAC->FrameControl.Fields.SubType = MAC_FRAME_SUB_TYPE_QOS_DATA;
  cpu_to_le16s(&(pMAC->FrameControl.FrameCtrl));
  //htons(&(pMAC->FrameControl.FrameCtrl));
  pMAC->DurationId = 0x0000;
  cpu_to_le16s(&(pMAC->DurationId));
  //htons(&(pMAC->DurationId));
  memcpy(pMAC->Address1, pTxCHOpts->DestAddr, ETH_ALEN);
  memcpy(pMAC->Address2, pDev->EthHdr.h_source, ETH_ALEN);
  memset(pMAC->Address3, 0xFF, ETH_ALEN);
  pMAC->SeqControl.SeqCtrl = 0xfffe; // Set by the UpperMAC
  pMAC->QoSControl.QoSCtrl = 0x0000;
  pMAC->QoSControl.Fields.TID = pTxCHOpts->Priority;
  pMAC->QoSControl.Fields.EOSP = 0;
  pMAC->QoSControl.Fields.AckPolicy = pTxCHOpts->Service;
  pMAC->QoSControl.Fields.TXOPorQueue = 0;
  cpu_to_le16s(&pMAC->QoSControl.QoSCtrl);

  // Setup the SNAP header
  pSNAP->DSAP = SNAP_HEADER_DSAP;
  pSNAP->SSAP = SNAP_HEADER_SSAP;
  pSNAP->Control = SNAP_HEADER_CONTROL;
  pSNAP->OUI[0] = 0x00;
  pSNAP->OUI[1] = 0x00;
  pSNAP->OUI[2] = 0x00;
  pSNAP->Type = pTxCHOpts->EtherType;
  cpu_to_be16s(&pSNAP->Type);

  // Get loops specs for RangeSpec variants

  if (Freq == pRadio[MKX_RADIO_A].ChanConfig[MKX_CHANNEL_0].PHY.ChannelFreq)
  {
    RadioID = MKX_RADIO_A;
    ChannelID = MKX_CHANNEL_0;
  }
  else if (Freq == pRadio[MKX_RADIO_B].ChanConfig[MKX_CHANNEL_0].PHY.ChannelFreq)
  {
    RadioID = MKX_RADIO_B;
    ChannelID = MKX_CHANNEL_0;
  }
  else if (Freq == pRadio[MKX_RADIO_A].ChanConfig[MKX_CHANNEL_1].PHY.ChannelFreq)
  {
    RadioID = MKX_RADIO_A;
    ChannelID = MKX_CHANNEL_1;
  }
  else if (Freq == pRadio[MKX_RADIO_B].ChanConfig[MKX_CHANNEL_1].PHY.ChannelFreq)
  {
    RadioID = MKX_RADIO_B;
    ChannelID = MKX_CHANNEL_1;
  }
  else
  {
    ErrCode = TX_ERR_INVALIDOPTIONARG;
    goto Error;
  }

  // MCS Loop
  for (m = 0; m < pTxCHOpts->NMCS; m++)
  {
        // Setup the Mk2Descriptor
        pPacket->RadioID = RadioID;
        pPacket->ChannelID = ChannelID;
        // pPacket->Priority = pTxCHOpts->Priority;
        // pPacket->Service = pTxCHOpts->Service;
        // pPacket->TxPower.PowerSetting = pTxCHOpts->TxPwrCtrl;
        pPacket->TxAntenna = pTxCHOpts->pTxAntenna[0]; // List
        pPacket->Expiry = pTxCHOpts->Expiry;
        pPacket->TxCtrlFlags = 0;
        pPacket->MCS = pTxCHOpts->pMCS[m]; // List
		TxPower = 40;
        pPacket->TxPower = (tMK2Power) TxPower; // from long


        size_t overhead = (char*)pPayload - (char*)(pPacket->TxFrame);
//		strcpy((char *)pPayload, "hello world!");
		memcpy((char *)pPayload, buff, length);
//		printf("send message hello world!\n");

        // add any header overhead that we may have incurred
        ThisFrameLen = overhead + length;

        pPacket->TxFrameLength = ThisFrameLen;

        // Now send the packet
        ErrCode = LLC_TxReq(pDev->pMKx, pTxPacket, pDev);
        if (ErrCode)
        {
          fprintf(stderr, "LLC_TxReq %s (%d)\n", strerror(ErrCode), ErrCode);
        }
        else
        {
          (pDev->SeqNum)++; // increment unique ID of packets
        }
        // Wait a little whle before next transmission (attempt to set tx rate)
//        usleep(Pause_us);
  } // MCS Loop

  free(pTxPacket);

  d_fnend(D_DEBUG, pDev, "(pDev %p) = %d\n", pDev, ErrCode);

Error:
  return ErrCode;
} // end of function


/**
 * @brief Rate controlled send of N packets on CCH and SCH
 * @param pTx pointer to Tx Object owning MLME_IF handle and Config
 * @param pTxOpts the options used to config the channels for sending
 * @return Error Code
 *
 * Will transmit at least pTxOpts->NumberOfPackets.
 * Will typically stop just past this number of packets.
 */

int Tx_SendAtRate(tTxOpts * pTxOpts, int length)
{
  tTxErrCode ErrCode = TX_ERR_NONE;
//  int Pause_us;
//  struct timeval t0;
//  struct timeval *pt0;
//  float OneOnTargetRate;
  /// check and control packet rate this many times per second

//  d_assert(pTxOpts->TargetPacketRate_pps > 0);

  // Get dereferenced copy of target rate

  // precompute target period
//  OneOnTargetRate = 1.0 / pTxOpts->TargetPacketRate_pps;

  // calculate initial estimate of pause assuming processing takes zero time
//  Pause_us = OneOnTargetRate * 1e6;

  // check 2twice a second Half the number of packets per second

  // pointer the timeval pointers at the stack memory
//  pt0 = &t0;
  // initialise the time snapshots and packet number
//  gettimeofday(pt0, NULL);


  // Continue until signal or transmitted enough packets
  pDev->TxContinue = true;
  Tx_Send(pTxOpts, length);//500us, a packet

    // try receive to get MKx_TxCnf callback called
//  MKx_Recv(pDev->pMKx);


  return ErrCode;

}

static int wsmp_receive(void *buf, uint16_t len)
{
	int Result = 0;
	char * bufk =	(char *)buf;
	char pPayload[MALLOC_SIZE_MKxTxPacket];
	memcpy(pPayload, bufk, len);
	int i = 0;
//	while(i < len){
//		printf("%x", *(pPayload+i));
//		i++;
//	}
//	printf("\n");
	if((pPayload[0]== 0x29)&&(pPayload[1] == 0x29))
  	{
		uint16_t length = pPayload[9] << 8 | pPayload[8];
		if(checkSum(pPayload, length + 9) != 0){//不算包尾0x0d
			Result = -1;
			printf("checkSum not correct\n");
		}
	}else{
		Result = -1;
	}
    //Result����У��ɹ�
   if(Result == 0){
       union double2char d2c;
       union float2char f2c;
       union uint32_t2char i2c;
       LocalStatu locals;				
	   UniPacket up;
	   int packetlength;
	   CwMessage WsmMessage;
	   memset(&WsmMessage, 0, sizeof(struct CwMessage));
	   if(UdpEnabled){
	   	switch(pPayload[2]){
			//没有转发情况下，其实不需要记录packet的唯一性？
                case 0x10://|0x29��0x29,0x10,seq,hop,length(�ֽ�),plate,check,0x0d|
					WsmMessage.protocol = 0x11;
					WsmMessage.seq = pDev->SeqNum;
					WsmMessage.hop = 1; //������Ϊ�ǽ����Ϣ�����Բ��ö����㲥��ȥ
					WsmMessage.length = 68;//data�ĳ���
					memcpy(WsmMessage.data, &pPayload[10], 9);//Ŀ�공�ƺ�(������Ϣ�ĳ��ƺ�)
					memcpy(&WsmMessage.data[9], CarS.plate, 9);//������ƺ�
					d2c.d = CarS.location.latitude;
					memcpy(&WsmMessage.data[18], d2c.data, 8);
					d2c.d = CarS.location.longitude;
					memcpy(&WsmMessage.data[26], d2c.data, 8);
					f2c.d = CarS.location.speed;
					memcpy(&WsmMessage.data[34], f2c.data, 4);
					f2c.d = CarS.location.bearing;
					memcpy(&WsmMessage.data[38], f2c.data, 4);
					f2c.d = CarS.accel.x;
					memcpy(&WsmMessage.data[42], f2c.data, 4);
					f2c.d = CarS.accel.y;
					memcpy(&WsmMessage.data[46], f2c.data, 4);
					f2c.d = CarS.accel.z;
					memcpy(&WsmMessage.data[50], f2c.data, 4);
					//54~61 海拔高度
					i2c.d = GetDriveStatus();
					memcpy(&WsmMessage.data[62], i2c.data, 4);
					packetlength = packetmessage(&WsmMessage);
					Tx_SendAtRate(pTxOpts, packetlength);
					return 0;
				case 0x11:
					if(memcmp(&pPayload[10], CarS.plate, 9) != 0){//自己发送了请求信息
							return 0;
					}
					break;
				case 0x20://这个可能有多跳，所以需要记录唯一性，不用重复接收
				case 0x21:
					memcpy(up.plate, &pPayload[19], 9);//19是源节点地址
					up.seqno = pPayload[6]<<24 |pPayload[5]<<16 |pPayload[4]<<8 |pPayload[3];
					if(!unipacket_find(up.plate, up.seqno)){
						up.Newtime = time(NULL);
						unipacket_insert(up);
					}else{
						return 0;//已经接收过这个packet,所以不再出理
					}
					if(memcmp(&pPayload[10], CarS.plate, 9) != 0)
						return 0;
					break;
			    case 0x22://广播包不需要判断唯一性
			    	memcpy(locals.status.plate, &pPayload[10], 9);//邻居车牌号
					memcpy(d2c.data, &pPayload[19], 8);//经度
					locals.status.location.latitude = d2c.d;
					memcpy(d2c.data, &pPayload[27], 8);
					locals.status.location.longitude = d2c.d;//纬度
					memcpy(f2c.data, &pPayload[35], 4);
					locals.status.location.speed = f2c.d;//速度
					memcpy(f2c.data, &pPayload[39], 4);
					locals.status.location.bearing = f2c.d;//航向角度
					memcpy(f2c.data, &pPayload[43], 4);
					locals.status.accel.x = f2c.d;
					memcpy(f2c.data, &pPayload[47], 4);
					locals.status.accel.y = f2c.d;
					memcpy(f2c.data, &pPayload[51], 4);
					locals.status.accel.z = f2c.d;
					memcpy(f2c.data, &pPayload[55], 4);
					locals.status.carstatus = f2c.d; //这个是车辆状态
					locals.status.expiretime = 5.0f;
					locals.status.seqno = pPayload[6]<<24|pPayload[5]<<16|pPayload[4]<<8|pPayload[3];
					locals.status.valid = true;
					locals.status.Newtime = time(NULL);
					if(neigh_find(locals.status.plate) != NULL)
						neigh_update(locals);
					else
						neigh_insert(locals);
				case 0x23://报警信息，多跳，所以需要记录plate/seq
				case 0x24:
				case 0x25:
				case 0x26:
				case 0x27:
					memcpy(up.plate, &pPayload[10], 9);//10是源节点地址
					up.seqno = pPayload[6]<<24 |pPayload[5]<<16 |pPayload[4]<<8 |pPayload[3];
					if(!unipacket_find(up.plate, up.seqno)){
						up.Newtime = time(NULL);
						unipacket_insert(up);
					}else{
						return 0;//已经接收过这个packet,所以不再出理
					}
					if((pPayload[7] > 1)&&(pPayload[7]!= 0xFA) ){
						pPayload[7]--;
						WsmMessage.protocol = pPayload[2];
						WsmMessage.seq = up.seqno;
						WsmMessage.length = pPayload[9]<<8 | pPayload[8];
					    memcpy(WsmMessage.data, &pPayload[10], WsmMessage.length - 2);//因为是广播，广播节点车牌号不变，需要重新校验
						packetlength = packetmessage(&WsmMessage);
						Tx_SendAtRate(pTxOpts, packetlength);
					}
					break;
				case 0x28:break;
				default:break;
			}
			sendto(udpfd, pPayload, len, 0, (struct sockaddr*)&UdpCltaddr, udpaddrlen);
	   }

	   if(TcpEnabled){
	       switch(pPayload[2]){
                case 0x10:
					WsmMessage.protocol = 0x11;
					WsmMessage.seq = pDev->SeqNum;;
					WsmMessage.hop = 1; 
					WsmMessage.length = 68;
					memcpy(WsmMessage.data, &pPayload[10], 9);
					memcpy(&WsmMessage.data[9], CarS.plate, 9);
					d2c.d = CarS.location.latitude;
					memcpy(&WsmMessage.data[18], d2c.data, 8);
					d2c.d = CarS.location.longitude;
					memcpy(&WsmMessage.data[26], d2c.data, 8);
					f2c.d = CarS.location.speed;
					memcpy(&WsmMessage.data[34], f2c.data, 4);
					f2c.d = CarS.location.bearing;
					memcpy(&WsmMessage.data[38], f2c.data, 4);
					f2c.d = CarS.accel.x;
					memcpy(&WsmMessage.data[42], f2c.data, 4);
					f2c.d = CarS.accel.y;
					memcpy(&WsmMessage.data[46], f2c.data, 4);
					f2c.d = CarS.accel.z;
					memcpy(&WsmMessage.data[50], f2c.data, 4);
					//54~61 海拔高度
					i2c.d = GetDriveStatus();
					memcpy(&WsmMessage.data[62], i2c.data, 4);
					packetlength = packetmessage(&WsmMessage);
					Tx_SendAtRate(pTxOpts, packetlength);
		   			return 0;
				case 0x11:
					if(memcmp(&pPayload[10], CarS.plate, 9) != 0)
						return 0;
					break;
				case 0x20:
				case 0x21:
					memcpy(up.plate, &pPayload[19], 9);//19是源节点地址
					up.seqno = pPayload[6]<<24 |pPayload[5]<<16 |pPayload[4]<<8 |pPayload[3];
					if(!unipacket_find(up.plate, up.seqno)){
						up.Newtime = time(NULL);
						unipacket_insert(up);
					}else{
						return 0;//已经接收过这个packet,所以不再出理
					}
					if(memcmp(&pPayload[10], CarS.plate, 9) != 0)
						return 0;
					break;

			    case 0x22:
						while(i < len){
							printf("%x", *(pPayload+i));
							i++;
						}
						printf("\n");
					memcpy(locals.status.plate, &pPayload[10], 9);//邻居车牌号
					memcpy(d2c.data, &pPayload[19], 8);//经度
					locals.status.location.latitude = d2c.d;
					memcpy(d2c.data, &pPayload[27], 8);
					locals.status.location.longitude = d2c.d;//纬度
					memcpy(f2c.data, &pPayload[35], 4);
					locals.status.location.speed = f2c.d;//速度
					memcpy(f2c.data, &pPayload[39], 4);
					locals.status.location.bearing = f2c.d;//航向角度
					memcpy(f2c.data, &pPayload[43], 4);
					locals.status.accel.x = f2c.d;
					memcpy(f2c.data, &pPayload[47], 4);
					locals.status.accel.y = f2c.d;
					memcpy(f2c.data, &pPayload[51], 4);
					locals.status.accel.z = f2c.d;
					memcpy(f2c.data, &pPayload[55], 4);
					locals.status.carstatus = f2c.d; //这个是车辆状态
					locals.status.expiretime = 5.0f;
					locals.status.seqno = pPayload[6]<<24|pPayload[5]<<16|pPayload[4]<<8|pPayload[3];
					locals.status.valid = true;
					locals.status.Newtime = time(NULL);
					if(neigh_find(locals.status.plate) != NULL)
						neigh_update(locals);
					else
						neigh_insert(locals);
				case 0x23:
				case 0x24:
				case 0x25:
				case 0x26:
				case 0x27:
					memcpy(up.plate, &pPayload[10], 9);//10是源节点地址
					up.seqno = pPayload[6]<<24 |pPayload[5]<<16 |pPayload[4]<<8 |pPayload[3];
					if(!unipacket_find(up.plate, up.seqno)){
						up.Newtime = time(NULL);
						unipacket_insert(up);
					}else{
						return 0;//已经接收过这个packet,所以不再出理
					}
					if((pPayload[7] > 1)&&(pPayload[7]!= 0xFA) ){
						pPayload[7]--;
						WsmMessage.protocol = pPayload[2];
						WsmMessage.seq = up.seqno;
						WsmMessage.length = pPayload[9]<<8 | pPayload[8];
					    memcpy(WsmMessage.data, &pPayload[10], WsmMessage.length - 2);//因为是广播，广播节点车牌号不变，需要重新校验
						packetlength = packetmessage(&WsmMessage);
						Tx_SendAtRate(pTxOpts, packetlength);
					}
					break;

				case 0x28:break;
				default:break;
	       }
			if(write(tcpfd, pPayload, len) < 0){
				perror("Socet error");
				TcpEnabled = false;
				Fds[1].fd = listenfd;
				Fds[1].events = POLL_INPUT;
				if(tcpfd != -1){
					close(tcpfd);
					tcpfd = -1;
				}
			}else{
				printf("write to android message\n");
			}
	   }
   	}
	return Result;
}

/**
 * @brief Print an MK2TxDescriptor
 * @param pMK2TxDesc the Descriptor to display
 *
 * prints headings if NULL
 */
/*static void TxPacket_fprintf (FILE * fp, struct MKxTxPacketData *pPacket)
{

  uint64_t Expiry_Seconds, Expiry_uSeconds;

  if (pPacket == NULL)
  {
    // write headings
    /// Indicate the channel number that frame was received on
    fprintf(fp, "%3s ", "ChN");
    /// Indicate the priority allocated to the received packet (by Tx)
    fprintf(fp, "%3s ", "Pri");
    /// Indicate the 802.11 service class used to transmit the packet
    fprintf(fp, "%3s ", "Srv");
    /// Indicate the data rate that was used
    fprintf(fp, "%2s ", "MC");
    /// Indicate the power to be used (may specify default or TPC)
    fprintf(fp, "%6s ", "P(dBm)");
    /// Indicate the antenna upon which packet should be transmitted
    fprintf(fp, "%2s ", "An");
    /// Indicate the expiry time (0 means never)
    fprintf(fp, "%21s ", "Expiry (s)");
  }
  else
  {
    /// Indicate the channel number that frame was received on
    // fprintf(fp, "%03d ", pPacket->ChannelNumber);
    fprintf(fp, "N/A ");
    /// Indicate the priority allocated to the received packet (by Tx)
    // fprintf(fp, "%03d ", pMK2TxDesc->Priority);
    fprintf(fp, "N/A ");
    /// Indicate the 802.11 service class used to transmit the packet
    // fprintf(fp, "%03d ", pMK2TxDesc->Service);
    fprintf(fp, "N/A ");
    /// Indicate the data rate that was used
    fprintf(fp, "%02d ", pPacket->MCS);
    /// Indicate the power to be used (may specify default or TPC)
    fprintf(fp, "%6.1f ", pPacket->TxPower * 0.5);
    /// Indicate the antenna upon which packet should be transmitted
    fprintf(fp, "%02d ", pPacket->TxAntenna);
    /// Indicate the expiry time (0 means never)
    Expiry_Seconds = pPacket->Expiry / 1000000;
    Expiry_uSeconds = pPacket->Expiry % 1000000;
    fprintf(fp, "%014llu.", Expiry_Seconds);
    fprintf(fp, "%06llu ", Expiry_uSeconds);
  }
}
*/
/**
 * @brief Write the info in packet Buffer to Packet log
 * @param fp File Pointer (open)
 * @param pBuf the Buffer containing the Frame to be logged
 * @param FrameLen the number of relevant Bytes in the Buffer
 * @param DumpPayload Should the entire payload be dumped to file
 * @param DumpHeadings dump headings in addition to data?
 * @return Error Code
 *
 * Buffer includes RxDescriptor, Ethernet header, Payload and FCS
 *
 */
/*
static tTxErrCode Tx_fprintf_waveraw (FILE * fp,
                                      struct MKxTxPacketData *pPacket,
                                      int FrameLen,
                                      bool DumpPayload,
                                      bool DumpHeadings)
{
  tTxErrCode ErrCode = TX_ERR_NONE;
  struct ethhdr *pEthHdr; // pointer into Buf at Eth Hdr
  unsigned char *pPayload; // pointer in Buf at Payload (contains SeqNum)
  int PayloadLen; // Payload Length in Bytes

  if (fp == NULL)
  {
    printf("Fail: Tx_WriteToLog NULL FILE pointer\n");
    ErrCode = TX_ERR_NULLFILEPOINTER;
    return ErrCode;
  }

  if (pPacket == NULL)
  {
    printf("Fail: Tx_WriteToLog NULL Buffer\n");
    ErrCode = TX_ERR_NULLBUFFER;
    return ErrCode;
  }

  //--------------------------------------------------------------------------
  // WAVE-RAW frame: | TxDesc | Eth Header | Protocol & Payload |
  pEthHdr = (struct ethhdr *)pPacket->TxFrame;
  pPayload = (unsigned char *) (pPacket->TxFrame + sizeof(struct ethhdr));

  // now that we have pointer to structs, dump to file line

  // every now an then write a comment with column labels
  if ((DumpHeadings) || (fp == stdout))
  {
    fprintf(fp, "#   SeqNum ");
    TxPacket_fprintf(fp, NULL);
    EthHdr_fprintf(fp, NULL);
    Payload_fprintf(fp, NULL, 0, 0); // last two args ingnored
    fprintf(fp, "\n"); // end this packet line
  }

  // SeqNum is first 4 Bytes of Payload
  fprintf(fp, "%010d ", ntohl(*((uint32_t *) pPayload)));

  // Tx Descriptor has its own dumper
  TxPacket_fprintf(fp, pPacket);
  EthHdr_fprintf(fp, pEthHdr);

  // calc Payload Length
  PayloadLen = FrameLen - sizeof(struct ethhdr);

  Payload_fprintf(fp, pPayload, PayloadLen, DumpPayload);

  fprintf(fp, "\n"); // end this packet line

  return ErrCode;
}*/
/*
static tTxErrCode Tx_fprintf (FILE * fp,
                              struct MKxTxPacketData *pPacket,
                              int FrameLen,
                              bool DumpPayload,
                              bool DumpHeadings)
{
  tTxErrCode ErrCode = TX_ERR_NONE;

  if (fp == NULL)
    return ErrCode;

  ErrCode = Tx_fprintf_waveraw(fp, pPacket, FrameLen, DumpPayload,
                               DumpHeadings);
  return ErrCode;

}*/


tMKxStatus LLC_RxInd (struct MKx *pMKx,
                      tMKxRxPacket *pRxPkt,
                      void *pPriv)
{
  int Res = MKXSTATUS_SUCCESS;
  bool Matched;
  int PayloadLen; // Number of Bytes returned (and in Payload)
  struct MKxRxPacketData *pPacket;
  struct IEEE80211QoSHeader *pMAC;
  struct SNAPHeader *pSNAP;
  uint8_t *pPayload;
  struct PktBuf *pPkb = (struct PktBuf *)pPriv;
  if (pRxPkt->Hdr.Type != MKXIF_RXPACKET)
    goto Exit;
  pPacket = &pRxPkt->RxPacketData;
  if (pPacket->RxFrameLength < (sizeof(struct IEEE80211QoSHeader) +
                                sizeof(struct SNAPHeader)) + 6)
    // too short
    goto Exit;
  pMAC = (struct IEEE80211QoSHeader *)pPacket->RxFrame;
  pSNAP = (struct SNAPHeader *)(pMAC + 1);
  pPayload = (uint8_t *)(pSNAP + 1);
  PayloadLen = pPacket->RxFrameLength - (sizeof(struct IEEE80211QoSHeader) +
                                         sizeof(struct SNAPHeader));
  d_printf(D_DEBUG, NULL, "pRxPkt %p type = %d\n", pRxPkt, pRxPkt->Hdr.Type);
  d_printf(D_DEBUG, NULL, "Len = %d\n", pRxPkt->Hdr.Len);
  d_printf(D_DEBUG, NULL, "FrameLen = %d\n", PayloadLen);
  d_dump(D_DEBUG, NULL, pRxPkt, pRxPkt->Hdr.Len);
  Matched = true;
  if (Matched)
  {
//    printf("llc receive message: %s: %d\n",pPayload, PayloadLen);
//	printk(KERN_INFO "llc receive message: %s: %d\n", pPayload, PayloadLen);
	func(pPayload, PayloadLen - 4);//FCS 4 bytes
  }

  // Count number of unblocks
Exit:
  PktBuf_Free(pPkb);
  return Res;
}


/** ** @brief Simple RxAlloc() implementation using pktbufs **/
tMKxStatus LLC_RxAlloc (struct MKx *pMKx,
						int BufLen,
						uint8_t **ppBuf,
							void **ppPriv)
{  
	int Res = -ENOMEM;
	int PkbLen = LLC_ALLOC_EXTRA_HEADER + BufLen + LLC_ALLOC_EXTRA_FOOTER;
	struct PktBuf *pPkb = PktBuf_Alloc(PkbLen);
	if (!pPkb)  {
		goto Error;  
	}  // Pre-insert the data into the pPkb
	PktBuf_Reserve(pPkb, LLC_ALLOC_EXTRA_HEADER);
	PktBuf_Put(pPkb, BufLen);
	*ppBuf = (uint8_t *)(pPkb->data);
	*ppPriv = (uint8_t *)pPkb;
	Res = 0;
Error:
	d_printf(D_DEBUG, NULL, "BufLen %d *ppBuf %p *ppPriv %p\n",
		BufLen, *ppBuf, *ppPriv);
	return Res;
}


//����ͨ��UDP socket IPv6����ͨ��
/*int LLC_OpenUDP(uint16_t Port)
{
  // cut-and-paste coding at it's finest...
  // #include stack/src/plat/util/udp.c

  d_fnstart(D_TST, NULL, "(%d)\n", Port);

  if (!Port)
  {
    d_fnend(D_TST, NULL, "(%d): disabled\n", Port);
    return -1;
  }

  /// Raw socket
  int Fd = socket(PF_INET6, SOCK_DGRAM, 0);
  if (Fd < 0)
  {
    // oops
    d_fnend(D_ERR, NULL, "socket(PF_INET6, SOCK_DGRAM, 0) failed: %d, %s\n",
            errno, strerror(errno));
    return -1;
  }

  // Use a "dual stack" socket that listens on IPv4 and IPv6
  int flag = 0;
  int rc = setsockopt(Fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&flag, sizeof(flag));
  if (rc < 0)
  {
    d_fnend(D_ERR, NULL, "setsockopt(%d, IPPROTO_IPV6, IPV6_V6ONLY, 0) failed: %d %s\n",
            Fd, errno, strerror(errno));
    close(Fd);
    return -1;
  }

  // Set socket flags for address reuse
  flag = 1;
  rc = setsockopt(Fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
  if (rc < 0)
  {
    d_error(D_ERR, NULL, "setsockopt(%d, SOL_SOCKET, SO_REUSEADDR, 1) failed: %d %s\n",
            Fd, errno, strerror(errno));
    close(Fd);
    return -1;
  }

  struct sockaddr_in6 addr = {0,};
  addr.sin6_family = AF_INET6;
  addr.sin6_addr = in6addr_any;
  addr.sin6_port = htons(Port);

  rc = bind(Fd, (struct sockaddr *)&addr, sizeof(addr));
  if(rc)
  {
    // oops
    d_fnend(D_ERR, NULL, "bind(%d, %d) failed: %d, %s\n",
            Fd, Port, errno, strerror(errno));
    close(Fd);
    return -1;
  }
  d_fnend(D_TST, NULL, "(%d): %d\n",
          Port, Fd);
  return Fd;
}*/
//����ǰ�ڵ㷢�ͳɹ��󣬸ú����ᱻLLC����֪ͨStack�յ���Ϣ
tMKxStatus LLC_TxCnf (struct MKx *pMKx,
                      tMKxTxPacket *pTxPkt,
                      const tMKxTxEvent *pTxEvent,
                      void *pPriv)
{
  int Res = MKXSTATUS_SUCCESS;
  tMKxStatus Result = pTxEvent->Hdr.Ret == MKXSTATUS_SUCCESS ?
    pTxEvent->TxEventData.TxStatus ://����¼�״̬
    pTxEvent->Hdr.Ret;//���Txevent��������޸�

  //������һЩ����ʧ�ܵķ���ֵ���ɹ�Ϊ
  //MKXSTATUES_SUCCESS ���ͳɹ�
  //MKXSTATUS_FAILURE_INTERNAL_ERROR ���ͽӿڳ���
  //MKXSTATUS_TX_FAIL_TTL	TTLֵ����
  // MKXSTATUS_TX_FAIL_RETRIES	��γ���ʧ��
  // MKXSTATUS_TX_FAIL_QUEUEFULL ������ʧ��
  // MKXSTATUS_TX_FAIL_RADIO_NOT_PRESENT ���ߣ���Ƶ������
  // MKXSTATUS_TX_FAIL_MALFORMED ��ʽ��ʧ��

  d_printf(Result == MKXSTATUS_SUCCESS ? D_INFO : D_WARN, NULL,
           "TxCnf[%u]: %s [%d]\n", pTxEvent->TxEventData.MACSequenceNumber,
           (Result == MKXSTATUS_FAILURE_INTERNAL_ERROR ? "Internal error" :
            (Result == MKXSTATUS_TX_FAIL_TTL ? "Fail TTL" :
             (Result == MKXSTATUS_TX_FAIL_RETRIES ? "Fail Retries" :
              (Result == MKXSTATUS_TX_FAIL_QUEUEFULL ? "Fail Queue Full" :
               (Result == MKXSTATUS_TX_FAIL_RADIO_NOT_PRESENT ? "Fail Radio Not Present" :
                (Result == MKXSTATUS_TX_FAIL_MALFORMED ? "Fail Malfomed Frame" : "Unknown error")))))),
           Result);
  return Res;
}

/**
 * @brief Initiate the 'dbg' command
 */
int LLC_TxInit (struct LLCTx *pDev)
{
  int Res;
  tTxOpts * pTxOpts = &pDev->TxOpts;

  struct sigaction sigact;
  
  memset (&sigact, 0, sizeof(struct sigaction));
  sigact.sa_handler = LLC_TxSignal;
  //PIPE������socket��������·���
  sigaction(SIGPIPE, &sigact, 0);
  
  sigaction(SIGINT,  &sigact, 0);
  sigaction(SIGTERM, &sigact, 0);
  sigaction(SIGQUIT, &sigact, 0);
  sigaction(SIGHUP,  &sigact, 0);
  //�ڴ�����⣬������cw-llc����Ҳ�п������ھӱ������
  sigaction(SIGSEGV, &sigact, 0);


  Res = MKx_Init(&(pDev->pMKx));
  if (Res < 0)
    goto Error;
  pDev->Fd = MKx_Fd(pDev->pMKx);
  if (pDev->Fd < 0)
    goto Error;

  Res = TxOpts_New(pDev->Args.Argc, pDev->Args.ppArgv, pTxOpts);
  if (Res)
    goto Error;

  TxOpts_Print(pTxOpts);
//  pDev->Fd = -1; // File description invalid
  pDev->SeqNum = 0;
  pDev->fp = NULL;

  // PreLoad the Ethernet Header (used in RAW frames)
  memcpy(pDev->EthHdr.h_source, pDev->TxOpts.SrcAddr, ETH_ALEN); // SA

  // Open a handle for the packet log file
  if (pTxOpts->pPacketLogFileName[0] != 0)
  {
    pDev->fp = fopen(pTxOpts->pPacketLogFileName, "w");
    if (pDev->fp == NULL)
    {
      printf("Fail: fopen(%s) errno %d\n", pTxOpts->pPacketLogFileName, errno);
    }
    else
    {
      d_printf(D_INFO, pDev, "Opened %s for logging (Handle %p)\n",
               pTxOpts->pPacketLogFileName, pDev->fp);
    }
  }

  // Optionally open UDP forwarding handle
  //pTxOpts->TxCHOpts.UDPforwardSocket = LLC_OpenUDP(abs(pTxOpts->UDPforwardPort));
  //d_printf(D_TST, NULL, "LLC_OpenUDP(%u): %d\n",
    //      pTxOpts->UDPforwardPort, pTxOpts->TxCHOpts.UDPforwardSocket);

  pDev->TxContinue = true;

  pDev->pMKx->API.Callbacks.TxCnf = LLC_TxCnf;
  /*���ǽ��ղ�����Ҫ���õ�*/
  pDev->pMKx->API.Callbacks.RxInd = LLC_RxInd;  
  pDev->pMKx->API.Callbacks.RxAlloc = LLC_RxAlloc;
  pDev->pMKx->pPriv = (void *)pDev;

Error:
  if (Res != 0)
    LLC_TxUsage();

  d_fnend(D_TST, NULL, "() = %d\n", Res);
  return Res;
}

/**
 * @brief Cancel any 'dbg' command(s)
 */
int LLC_TxExit (struct LLCTx *pDev)
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

static int LLC_TxMain (int Argc, char **ppArgv)
{
  int Res, nb;
  int packetlength;
  struct sockaddr_un servaddr;//和安卓通信用的
  char sndbuf[MALLOC_SIZE_MKxTxPacket];
  char newbuf[MALLOC_SIZE_MKxTxPacket]; 
  char *ret = NULL;
  
  ssize_t n;
  const int on = 1;
  int GpsOn = 0;
  struct timeval *timeout;
  CwMessage WsmMessage;
//  LocalStatu *ls;

  result = (pstRMCmsg)malloc(sizeof(stRMCmsg) * 1);

  d_fnstart(D_TST, NULL, "()\n");

  pDev->Args.Argc = Argc;
  pDev->Args.ppArgv = ppArgv;

  // Initial actions
  Res = LLC_TxInit(pDev);
  if (Res < 0)
    goto Error;

  pTxOpts = &pDev->TxOpts;
  func = wsmp_receive;
  
  Fds[0].fd = pDev->Fd;
  Fds[0].events = POLL_INPUT;

  if( 0 > (listenfd = socket(AF_LOCAL, SOCK_STREAM, 0))){
	perror("Create Tcp Socket Error");
	exit(1);
  }
//  unlink(UNIXSTRTCP_PATH);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sun_family = AF_LOCAL;
  servaddr.sun_path[0] = 0;
  strcpy(servaddr.sun_path + 1 , UNIXSTRTCP_PATH);
  tcpaddrlen = sizeof(servaddr.sun_family) + sizeof(UNIXSTRTCP_PATH);

  if(0 > (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))){
	perror("Can not set socket work on Reuseaddr");
	exit(1);
  }
  if(0 > (bind(listenfd, (struct sockaddr*)&servaddr, tcpaddrlen))){
	perror("bind error");
	exit(1);
  }
  if(0 > listen(listenfd, 5)){
	perror("Can not listen Socket");
	exit(1);
  };

  Fds[1].fd = listenfd;
  Fds[1].events = POLL_INPUT;
  

  if(0 > (udpfd = socket(AF_LOCAL, SOCK_DGRAM, 0))){
	perror("Create Udp Socket Error");
	exit(1);
  }
//  unlink(UNIXSTRUDP_PATH);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sun_family = AF_LOCAL;
  servaddr.sun_path[0] = 0;
  strcpy(servaddr.sun_path + 1, UNIXSTRUDP_PATH);
  udpaddrlen = sizeof(servaddr.sun_family) + sizeof(UNIXSTRUDP_PATH);

  if( 0 > (bind(udpfd, (struct sockaddr *)&servaddr, udpaddrlen))){
		perror("Bind error");
		exit(1);
  }

  Fds[2].fd = udpfd;
  Fds[2].events = POLL_INPUT;
  
  Fds[3].fd = um220fd;
  Fds[3].events = POLL_INPUT;


  UdpEnabled = false;
  TcpEnabled = false;
  
  mpu6050_start();
  broadcast_start();
  neighbor_stop();
  
  while(pDev->TxContinue){
  	
  	memset(&WsmMessage, 0,sizeof(struct CwMessage));
	timeout = timer_age_queue();
  
	if((Res = poll(Fds, 4, timeout->tv_sec * 1000 + timeout->tv_usec / 1000)) < 0){
		printf("Poll error %d '%s'\n", errno, strerror(errno));
		continue;
	}
	
	if(Res > 0){
		if (Fds[0].revents & POLL_ERROR)
		{
			printf("Poll error on Dev cw-llc MKX (revents 0x%02x)\n", Fds[0].revents);
		}    
		if (Fds[0].revents & POLL_INPUT)
		{ 
			Res = MKx_Recv(pDev->pMKx);
		}
		if(Fds[1].revents & POLL_ERROR)
		{
			printf("Poll error on TCP Socket (revents 0x%02x)\n", Fds[1].revents);
			TcpEnabled = false;
			Fds[1].fd = listenfd;
			Fds[1].events = POLL_INPUT;
			if(tcpfd != -1){
				close(tcpfd);
				tcpfd = -1;
			}
		}
		if (Fds[1].revents & POLL_INPUT)
		{
			if(TcpEnabled == false){
				if(-1 == (tcpfd = accept(listenfd,(struct sockaddr *)&TcpCltaddr,&tcpaddrlen))){
					perror("Accept error:");
					TcpEnabled = false;
					goto Error;
				}
				TcpEnabled = true;
				Fds[1].fd = tcpfd;
				Fds[1].events = POLL_INPUT;
			}else{
				memset(sndbuf, 0, MALLOC_SIZE_MKxTxPacket);
				if( 0 > (n = read(tcpfd, sndbuf, MALLOC_SIZE_MKxTxPacket))){//Android -> DSRC
					perror("Tcp read Error:");
					TcpEnabled = false;
					Fds[1].fd = listenfd;
					Fds[1].events = POLL_INPUT;
					if(tcpfd != -1){
						close(tcpfd);
						tcpfd = -1;
					}
				}else{
				  	if((sndbuf[0]== 0x29)&&(sndbuf[1] == 0x29)&&(sndbuf[2] == 0x20)){
						WsmMessage.protocol = 0x20;
						WsmMessage.hop = 5;
						WsmMessage.seq = pDev->SeqNum;
						WsmMessage.length = n - 1;//这个内容包括校验和0x0d
						memcpy(WsmMessage.data, &sndbuf[3], WsmMessage.length - 2);
						packetlength = packetmessage(&WsmMessage);
						Tx_SendAtRate(pTxOpts, packetlength);
					}else if((sndbuf[0] == 0x29)&&(sndbuf[1] == 0x29)&&(sndbuf[2] == 0x10)){
    					WsmMessage.protocol = 0x10;
    					WsmMessage.hop = 1;
    					WsmMessage.seq = pDev->SeqNum;
    					WsmMessage.length = 11;
    					memcpy(WsmMessage.data, CarS.plate, 9);
    					packetlength = packetmessage(&WsmMessage);
						Tx_SendAtRate(pTxOpts, packetlength);
					}else{  
						WsmMessage.protocol = 0x23; 
						WsmMessage.hop = 1;
						WsmMessage.seq = pDev->SeqNum;
						WsmMessage.length = n + 2;//内容+校验+包尾0x0d
						memcpy(WsmMessage.data, sndbuf, WsmMessage.length - 2);
    					packetlength = packetmessage(&WsmMessage);
						Tx_SendAtRate(pTxOpts, packetlength);
					}
				}
			}
		}
		
		if (Fds[2].revents & POLL_ERROR)
		{
			printf("Poll error on UDP Socket (revents 0x%02x)\n", Fds[2].revents);
		}
		if (Fds[2].revents & POLL_INPUT)
		{
			memset(sndbuf, 0, MALLOC_SIZE_MKxTxPacket);
			n = recvfrom(udpfd, sndbuf, MALLOC_SIZE_MKxTxPacket,
				0, (struct sockaddr*)&UdpCltaddr, &udpaddrlen);

			if(n < 0){
				UdpEnabled = false;
			}else if (n == 0){
				perror("client has been closing socket!");
			}else{
				/*DSRC forward*/
				UdpEnabled = true;
				if((sndbuf[0]== 0x29)&&(sndbuf[1] == 0x29)&&(sndbuf[2] == 0x20))
				{
					WsmMessage.protocol = 0x20;
					WsmMessage.hop = 5;
					WsmMessage.seq = pDev->SeqNum;
					WsmMessage.length = n - 3;//android 0x29,0x29,0x20
			    	memcpy(WsmMessage.data, &sndbuf[3], WsmMessage.length);
					packetlength = packetmessage(&WsmMessage);
					Tx_SendAtRate(pTxOpts, packetlength);
				}else if((sndbuf[0] == 0x29)&&(sndbuf[1] == 0x29)&&(sndbuf[2] == 0x10)){
					WsmMessage.protocol = 0x10;
					WsmMessage.hop = 1;
					WsmMessage.seq = pDev->SeqNum;
					WsmMessage.length = 11;
	 				memcpy(WsmMessage.data, CarS.plate, 9);
					packetlength = packetmessage(&WsmMessage);
					Tx_SendAtRate(pTxOpts, packetlength);
				}else{  
					WsmMessage.protocol = 0x23; 
					WsmMessage.hop = 1;
					WsmMessage.seq = pDev->SeqNum;
					WsmMessage.length = n;
					memcpy(WsmMessage.data, sndbuf, WsmMessage.length);
					packetlength = packetmessage(&WsmMessage);
					Tx_SendAtRate(pTxOpts, packetlength);
				}
			}
		}

		if(Fds[3].revents & POLL_ERROR)
		{
			printf("Poll error on ttymxc1 (revents 0x%02x)\n", Fds[3].revents);
		}

		if(Fds[3].revents & POLL_INPUT)
		{
			memset(newbuf, 0, 1024);
			memset(result, 0, sizeof(struct RMCmsg));
			int semcount = 0;
			char *read_buf = newbuf;
			if((nb = read(um220fd, newbuf, 1024)) < 10 ){
				read_buf += nb;
				usleep(10000);
				nb = read(um220fd, read_buf, 1024);
			}
							
			if (nb == -1)  
	        {  
	            perror("read uart error");
				continue;
	        }else{
				printf("GPS Information %s\n",read_buf);
				char* temp = strtok(newbuf, ",");
				while(temp){
					semcount++;
					temp = strtok(NULL, ",");					
				}
			}
			if(((ret = strstr(newbuf, "$GNRMC")) != NULL)&&(semcount >= 12)){
	        	parseData(newbuf, result);
				usleep(10);
			    if(result->status == 0x41)//'A' ASCII Ϊ0x41,��Ч��Ϣ
			    {
					SetValid();
					GpsLocation gps;
					gps.latitude = (result->lat_du + result->lat_fen/60) * 1000;
					gps.clat = result->clat;
					gps.longitude = (result->lon_du + result->lon_fen/60) * 1000;
					gps.clon = result->clon;
					gps.altitude = 0;
					gps.speed = result->spd * 0.514f;
					gps.bearing = result->cog;
					SetGps(&gps);
					if(GpsOn == 0){
						char systime[50];
						memset(systime, 0, sizeof(systime));
						sprintf(systime, "data -s \"%d-%d-%d %d:%d:%lf\"",result->date_yy,result->date_mm,result->date_dd,result->time_hh,result->time_mm, result->time_ss);
						if(system(systime) < 0)
							printf("update system time failed!\n");
						else
						    GpsOn = 1;
					}
					struct tm tmTime;
					time_t timetTime;
					tmTime.tm_year = result->date_yy - 1900;
					tmTime.tm_mon = result->date_mm - 1;
					tmTime.tm_mday = result->date_dd;
				    tmTime.tm_hour = result->time_hh;
					tmTime.tm_min = result->time_mm;
					tmTime.tm_sec = (int)result->time_ss;
					timetTime = mktime(&tmTime);
					SetNewTime(timetTime);
				}else{
					ResetValid();
					GpsOn = 0;
				}
	        }
		}
	}
  }
Error:
  // Final actions
  close(pDev->Fd);
  close(tcpfd); 	// TCP Client
  close(listenfd);	//TCP Service Socket
  close(udpfd);		//UDP socket
  LLC_TxExit(pDev);
  free(result);
  result = NULL;
  mpu6050_stop();
  broadcast_stop();
  neighbor_stop();
  d_fnend(D_TST, NULL, "() = %d\n", Res);
  return Res;
}


//------------------------------------------------------------------------------
// Main Functions definitions
//------------------------------------------------------------------------------

/**
* @brief Called by the llc app when the 'dbg' command is specified
*/

int main(int Argc, char **ppArgv)
{
	int Res;

//	if(system("/system/bin/config.sh") < 0){
//		perror("system error");
//	}
	
	// Get the debug level from the environment
	D_LEVEL_INIT();

	CarStatu_init();

	if((um220fd = um220_init()) < 0)//������ǳ�ʼ������Ȼ����9600�л���115200������
		perror("um220 init error");


	Res = LLC_TxMain(Argc, ppArgv);
	if (Res < 0)
		d_printf(D_WARN, NULL, "%d (%s)\n", Res, strerror(-Res));

	close(um220fd);
	return Res;
}


/**
 * @}
 */


