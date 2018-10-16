#include <stdio.h>  
#include <termios.h>  
#include <strings.h>  
#include <string.h>  
#include <stdlib.h>  
#include <fcntl.h>  
#include <unistd.h>  
#include "um220-good.h"

#define DEV_NODE "/dev/ttymxc1"  
#define MAX_PACKET_SIZE 1024  
static int um220_fd = -1;				 //文件描述符


//static int set_com_config(int fd, int baud_rate, int data_bits, char parity, int stop_bits)
//{
//	struct termios new_cfg,old_cfg;
//	int speed;
//	/*保存并测试现有串口参数设置，在这里如果串口号等出错，会有相关的出错信息*/
//	if (tcgetattr(fd, &new_cfg) != 0)
//	{
//		perror("tcgetattr");
//		return -1;
//	}
//	
//	/*设置字符大小*/
//	cfmakeraw(&new_cfg);/*配置为原始模式*/
//	
//	new_cfg.c_cflag &= ~CSIZE;//用数据位掩码清空数据位设置
//	switch(baud_rate)
//	{
//		case 2400:
//		{
//			speed = B2400;
//		}
//		break;
//		case 4800:
//		{
//			speed = B4800;
//		}
//		break;
//		case 9600:
//		{
//			speed = B9600;
//		}
//		break;
//		case 19200:
//		{
//			speed = B19200;
//		}
//		break;
//		case 38400:
//		{
//			speed = B38400;
//		}
//		break;
//		case 115200:
//		{
//			speed = B115200;
//		}
//		break;
//		default:{
//			speed = B9600;
//		}
//		break;
//	}
//	cfsetispeed(&new_cfg, speed);
//	cfsetospeed(&new_cfg, speed);
//	/*设置停止位*/
//	switch(data_bits)
//	{
//		case 7:
//			new_cfg.c_cflag |= CS7;
//		break;
//		default:
//		case 8:
//			new_cfg.c_cflag |= CS8;
//		break;
//	}
//	/*设置奇偶校验位*/
//	switch (parity)
//	{
//		default:
//		case 'n':
//		case 'N':
//		{
//			new_cfg.c_cflag &= ~PARENB;
//			new_cfg.c_iflag &= ~INPCK;
//		}
//		break;
//		case 'o':
//		case 'O':
//		{
//			new_cfg.c_cflag |= (PARODD | PARENB);
//			new_cfg.c_iflag |= INPCK;
//		}
//		break;
//		case 'e':
//		case 'E':
//		{
//			new_cfg.c_cflag |= PARENB;
//			new_cfg.c_cflag &= ~PARODD;
//			new_cfg.c_iflag |= INPCK;
//		}
//		break;
//		case 's':/*as no parity*/
//		{
//			new_cfg.c_cflag &= ~PARENB;
//			new_cfg.c_cflag &= ~CSTOPB;
//		}
//		break;
//	}
//	/*设置停止位*/
//	switch(stop_bits)
//	{
//		default:
//		case 1:
//			new_cfg.c_cflag &= ~CSTOPB;
//		break;
//		case 2:
//			new_cfg.c_cflag |= CSTOPB;
//		break;
//	}
//	/*设置等待时间和最小接收字符,原来我设置了最小1个字符返回*/
//	new_cfg.c_cc[VTIME] = 10;
//	new_cfg.c_cc[VMIN] = 0xff;
//	
//	/*处理未接收字符*/
//	tcflush(fd, TCIFLUSH);
//	/*激活新配置*/
//	if((tcsetattr(fd, TCSANOW, &new_cfg)) != 0){
//		perror("tcsetattr");
//		return -1;
//	}
//	return 0;
//}

static int open_port()
{
	int fd;
	fd = open(DEV_NODE, O_RDWR|O_NOCTTY|O_NDELAY);//读写模式|不会成为该进程的控制终端|不关心DCD,非阻塞
	if(fd < 0)
	{
		perror("open serial port");
		return (-1);
	}
	/**/
	//是否要设置成阻塞状态和是否测试是否为终端设备
	/**/
	return fd;
}

static void setTermios(struct termios * pNewtio, int uBaudRate)  
{  
    bzero(pNewtio, sizeof(struct termios)); /* clear struct for new port settings */  
    //8N1  
    pNewtio->c_cflag = uBaudRate | CS8 | CREAD | CLOCAL;  
    pNewtio->c_iflag = IGNPAR;  
    pNewtio->c_oflag = 0;  
    pNewtio->c_lflag = 0; //non ICANON  
}  
  
void um220_uart_init(int ttyFd,struct termios *oldtio,struct termios *newtio, int baudrate)  
{  
    tcgetattr(ttyFd, oldtio); /* save current serial port settings */    
 	if(baudrate == 115200)
		setTermios(newtio, B115200);
	else
		setTermios(newtio, B9600);  
    tcflush(ttyFd, TCIFLUSH);  
    tcsetattr(ttyFd, TCSANOW, newtio);  
}  

static void convert(pstTmpMsg msg, pstRMCmsg result)
{
    char tmpbuf[32];
    memset(tmpbuf, 0, sizeof(tmpbuf));
    strncpy(tmpbuf, msg->time, 2);
    result->time_hh = atoi(tmpbuf);
    strncpy(tmpbuf, (msg->time) + 2, 2);
    result->time_mm = atoi(tmpbuf);
    memset(tmpbuf, 0, sizeof(tmpbuf));
    strcpy(tmpbuf, (msg->time) + 4);
    result->time_ss = atof(tmpbuf);

    result->status = msg->status;

    memset(tmpbuf, 0, sizeof(tmpbuf));
    strncpy(tmpbuf, msg->lat, 2);
    result->lat_du = atoi(tmpbuf);
    strcpy(tmpbuf, (msg->lat) + 2);
    result->lat_fen = atof(tmpbuf);

    result->clat = msg->clat;

    memset(tmpbuf, 0, sizeof(tmpbuf));
    strncpy(tmpbuf, msg->lon, 2);
    result->lon_du = atoi(tmpbuf);
    strcpy(tmpbuf, (msg->lon) + 2);
    result->lon_fen = atof(tmpbuf);

    result->clon = msg->clon;
    
    result->spd = atof(msg->spd);
    result->cog = atof(msg->cog);

    memset(tmpbuf, 0, sizeof(tmpbuf));
    strncpy(tmpbuf, msg->date, 2);
    result->date_yy = atoi(tmpbuf);

    strncpy(tmpbuf, (msg->date) + 2, 2);
    result->date_mm = atoi(tmpbuf);

    strncpy(tmpbuf, (msg->date) + 4, 2);
    result->date_dd = atoi(tmpbuf);

    result->mv = '\0';
    result->mvE = 'E';
    result->mode = msg->mode;
}


/*  指针参量,1秒钟分配和执行一次会不会有问题
* 	这里使用了GPS与北斗系统混合定位
*   $GNRMC,123400.000,A,4002.217821,N,11618.105743,E,0.026,181.631,180411,,E,A*2C
*   UTC时间格式:hhmmss.sss --> 12 23 00.000
*   位置有效标识，V - 无效， A - 有效
*   纬度,格式为ddmm.mmmmmm -->> 40 度 02.217821-分
*   北纬或南纬指示   N-北纬，S南纬
*   经度,格式为dddmm.mmmmmm，ddd-度，mm.mmmmmm-分
*   E-东经，W-西经
*   spd double 地面速率，单位为节
*   cog double 地面航向，单位为度，从北向起顺时针计算
*   date  UTC日期，格式为ddmmyy 日，月，年
*   mv DOUBLE 磁偏角，固定填空
*   mvE 磁偏角方向，固定填E
*   mode 定位模式 N-未定位， A单点定位
*   cs  校验和
*/
void parseData(char *buf, pstRMCmsg result)  
{  
    char* ptr = NULL;
    char* ptr_helper = NULL;
    pstTmpMsg pMsg = NULL;
    char buffer[256];

    if (buf == NULL || result == NULL)  
    {  
        printf("error: Can't get buf or result!\n");  
        return ;  
    }  
    sscanf(buf, "$GNRMC,%s", buffer);
    
    pMsg = (pstTmpMsg)malloc(sizeof(stTmpMsg) * 1);
    memset(pMsg, 0, sizeof(stTmpMsg));

    ptr = strchr(buffer, ',');
    if(ptr == NULL){
        perror("UTC time error");
		return ;
	}
	
    strncpy(pMsg->time, buffer, ptr - buffer);

    ptr_helper = ptr + 1;
    ptr = strchr(ptr_helper, ',');
    if(ptr == NULL){
        perror("status error");
		return ;
	}
    pMsg->status = *ptr_helper;

    ptr_helper = ptr + 1;
    ptr = strchr(ptr_helper, ',');
    if(ptr == NULL){
        perror("latitude error");
		return;
	}
    strncpy(pMsg->lat, ptr_helper, ptr - ptr_helper);

    ptr_helper = ptr + 1;
    ptr = strchr(ptr_helper, ',');
    if(ptr == NULL){
        perror("clat error");
		return ;
	}
    pMsg->clat = *ptr_helper;

    ptr_helper = ptr + 1;
    ptr = strchr(ptr_helper, ',');
    if(ptr == NULL){
        perror("lontitude error");
		return ;
	}
    strncpy(pMsg->lon, ptr_helper, ptr - ptr_helper);

    ptr_helper = ptr + 1;
    ptr = strchr(ptr_helper, ',');
    if(ptr == NULL){
        perror("clon error");
		return ;
	}
    pMsg->clon = *ptr_helper;
    
    ptr_helper = ptr + 1;
    ptr = strchr(ptr_helper, ',');
    if(ptr == NULL){
        perror("spd error");
		return;
	}
    strncpy(pMsg->spd, ptr_helper, ptr - ptr_helper);

    ptr_helper = ptr + 1;
    ptr = strchr(ptr_helper, ',');
    if(ptr == NULL){
        perror("cog error");
		return;
	}
    strncpy(pMsg->cog, ptr_helper, ptr - ptr_helper);

    ptr_helper = ptr + 1;
    ptr = strchr(ptr_helper, ',');
    if(ptr == NULL){
        perror("date error");
		return;
	}
    strncpy(pMsg->date, ptr_helper, ptr - ptr_helper);

    ptr_helper = ptr + 4;
    pMsg->mode = *ptr_helper;

    convert(pMsg, result);

    free(pMsg);
    pMsg = NULL;
	
}


/* 	这个用于返回文件描述符就好了 	*/
/* 	这个文件描述符用于读取GPS数据	*/
int um220_init()
{ 
    char msg[256];
    struct termios oldtio, newtio;
	
	/*Open Um220 Module*/  
    if((um220_fd = open_port()) < 0){
		perror("uart open error");
		return -1;
    }

	/*Init Uart for Um220*/ 
//	if(set_com_config(um220_fd, 9600, 8, 'N', 1) < 0){ //配置串口
//			perror("set_com_config");
//			return -1;
//	}
    um220_uart_init(um220_fd, &oldtio, &newtio, 115200);

    memset(msg, 0, sizeof(msg));  
    //BD + GPS
    strcpy(msg,"$CFGLOAD,h0F\r\n");  
    while(write(um220_fd,msg,sizeof(msg)) < 0);
    	printf("load config succeed!\n");

    return um220_fd; 	
}


