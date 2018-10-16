/************************************************************/
//ÎÄ¼şÃû£ºmpu6050.c
//¹¦ÄÜ:²âÊÔlinuxÏÂiic¶ÁĞ´mpu6050³ÌĞò
//Ê¹ÓÃËµÃ÷: (1)
//          (2)
//          (3)
//          (4)
//×÷Õß:huangea
//ÈÕÆÚ:2016-10-03
/************************************************************/
//°üº¬Í·ÎÄ¼ş
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include "mpu6050.h"
//****************************************  
// ¶¨ÒåMPU6050ÄÚ²¿µØÖ·  
//****************************************  
#define	SMPLRT_DIV	0x19	  //ÍÓÂİÒÇ²ÉÑùÂÊ£¬µäĞÍÖµ£º0x07(125Hz)
#define	CONFIG	0x1A    	  //µÍÍ¨ÂË²¨ÆµÂÊ£¬µäĞÍÖµ£º0x06(5Hz)  
#define	GYRO_CONFIG	0x1B      //ÍÓÂİÒÇ×Ô¼ì¼°²âÁ¿·¶Î§£¬µäĞÍÖµ£º0x18(²»×Ô¼ì£¬2000deg/s)
#define	ACCEL_CONFIG	0x1C  //¼ÓËÙ¼Æ×Ô¼ì¡¢²âÁ¿·¶Î§¼°¸ßÍ¨ÂË²¨ÆµÂÊ£¬µäĞÍÖµ£º0x01(²»×Ô¼ì£¬2G£¬5Hz) 
#define	ACCEL_XOUT_H	0x3B
#define	ACCEL_XOUT_L	0x3C  //xÖá¼ÓËÙ¶È
#define	ACCEL_YOUT_H	0x3D
#define	ACCEL_YOUT_L	0x3E  //yÖá¼ÓËÙ¶È 
#define	ACCEL_ZOUT_H	0x3F
#define	ACCEL_ZOUT_L	0x40  //zÖá¼ÓËÙ¶È
#define	TEMP_OUT_H	0x41
#define	TEMP_OUT_L	0x42
#define	GYRO_XOUT_H	0x43
#define	GYRO_XOUT_L	0x44      //xÖá½ÇËÙ¶È  
#define	GYRO_YOUT_H	0x45
#define	GYRO_YOUT_L	0x46      //yÖá½ÇËÙ¶È
#define	GYRO_ZOUT_H	0x47
#define	GYRO_ZOUT_L	0x48      //zÖá½ÇËÙ¶È
#define	PWR_MGMT_1	0x6B      //µçÔ´¹ÜÀí£¬µäĞÍÖµ£º0x00(Õı³£ÆôÓÃ) 
#define	WHO_AM_I	0x75	  //IICµØÖ·¼Ä´æÆ÷(Ä¬ÈÏÊıÖµ0x68£¬Ö»¶Á)
#define	SlaveAddress	0xD0	
#define Address 0x68                  //MPU6050µØÖ·
#define I2C_RETRIES   0x0701
#define I2C_TIMEOUT   0x0702
#define I2C_SLAVE     0x0703       //IIC´ÓÆ÷¼şµÄµØÖ·ÉèÖÃ
#define I2C_BUS_MODE   0x0780

typedef unsigned char uint8;
static int fd = -1;//º¯ÊıÉùÃ÷
//float q_bias[3];
float gyro_x, gyro_y, gyro_z;
float accel_x, accel_y, accel_z;
static uint8 i2c_write(int fd, uint8 reg, uint8 val);
static uint8 i2c_read(int fd, uint8 reg, uint8 *val);
static short GetData(unsigned char REG_Address);
#define GYRO 9.78833f
#define PI 3.14159265358979f

//MPU6050³õÊ¼»¯
uint8 MPU6050_Init(void)
{
	fd = open("/dev/i2c-0", O_RDWR);    // open file and enable read and  write	
	if(fd< 0)
	{		
		perror("Can't open /dev/MPU6050 \n"); // open i2c dev file fail		
		exit(1);	
	}	
	printf("open /dev/i2c-0 success !\n");   // open i2c dev file succes	
	if(ioctl(fd, I2C_SLAVE, Address)<0) {    //set i2c address 		
		printf("fail to set i2c device slave address!\n");
		close(fd);
		return -1;
	}
	printf("set slave address to 0x%x success!\n", Address);
	i2c_write(fd,PWR_MGMT_1,0X00);
	i2c_write(fd,SMPLRT_DIV,0X07);
	i2c_write(fd,CONFIG,0X06);
	i2c_write(fd,GYRO_CONFIG, 0x18);
	i2c_write(fd,ACCEL_CONFIG,0X00);
	
	return(1);
}

//MPU6050 wirte byte
static uint8 i2c_write(int fd, uint8 reg, uint8 val)
{	
	int retries;
	uint8 data[2];
	data[0] = reg;
	data[1] = val;
	for(retries=5; retries; retries--) {
		if(write(fd, data, 2)==2)
			return 0;
		usleep(1000*10);
	}
	return -1;
}

//MPU6050 read byte
static uint8 i2c_read(int fd, uint8 reg, uint8 *val)
{
	int retries;
	for(retries=5; retries; retries--)
		if(write(fd, &reg, 1)==1)
			if(read(fd, val, 1)==1)
				return 0;
	return -1;
}

//get data
static short GetData(unsigned char REG_Address)
{	
	unsigned char H,L;
	i2c_read(fd, REG_Address, &H);
	i2c_read(fd, REG_Address + 1, &L);
	return (((short)H)<<8)+L;
}

static void I2C_Receive14Bytes(uint8 *anbt_i2c_data_buffer)
{
	i2c_read(fd, 0x3B, &anbt_i2c_data_buffer[0]);
	i2c_read(fd, 0x3C, &anbt_i2c_data_buffer[1]);
	i2c_read(fd, 0x3D, &anbt_i2c_data_buffer[2]);
	i2c_read(fd, 0x3E, &anbt_i2c_data_buffer[3]);
	i2c_read(fd, 0x3F, &anbt_i2c_data_buffer[4]);
	i2c_read(fd, 0x40, &anbt_i2c_data_buffer[5]);
	i2c_read(fd, 0x41, &anbt_i2c_data_buffer[6]);
	i2c_read(fd, 0x42, &anbt_i2c_data_buffer[7]);
	i2c_read(fd, 0x43, &anbt_i2c_data_buffer[8]);
	i2c_read(fd, 0x44, &anbt_i2c_data_buffer[9]);
	i2c_read(fd, 0x45, &anbt_i2c_data_buffer[10]);
	i2c_read(fd, 0x46, &anbt_i2c_data_buffer[11]);
	i2c_read(fd, 0x47, &anbt_i2c_data_buffer[12]);
	i2c_read(fd, 0x48, &anbt_i2c_data_buffer[13]);
}

void Cal_MPU6050_Data(int *cal_data)   
{
	unsigned char i,j;
	unsigned char mpu6050_cal_data_buffer[14];
	signed short int mpu6050_cal_data[100][7];
	int mpu6050_cal_sum[7];
	//
	for(i=0;i<7;i++) mpu6050_cal_sum[i]=0;
	
	//
	for(j=0;j<100;j++)
	{
		I2C_Receive14Bytes(mpu6050_cal_data_buffer);	//åœ†ç‚¹åšå£«:è¯»å‡ºå¯„å­˜å™¨å€¼		
		for(i=0;i<7;i++) mpu6050_cal_data[j][i]=(((signed short int)mpu6050_cal_data_buffer[i*2]) << 8) | mpu6050_cal_data_buffer[i*2+1];
	}
	//
	for(i=0;i<7;i++)
	{
		for(j=0;j<100;j++) mpu6050_cal_sum[i]=mpu6050_cal_sum[i]+(int)mpu6050_cal_data[j][i];
	}
	//3åº”è¯¥æ˜¯æ¸©åº¦å€¼ï¼Œæ‰€ä»¥ä¸ç®¡äº†ï¼Œè¿™é‡Œå°±æ˜¯å¤šæ¬¡è¯»å–æ•°æ®æ±‚å¹³å‡å€¼
	//è¿™ä¸ªåº”è¯¥ç®—æ˜¯ç»Ÿè®¡é›¶æ¼‚
	cal_data[0]=mpu6050_cal_sum[4]/100;
	cal_data[1]=mpu6050_cal_sum[5]/100;
	cal_data[2]=mpu6050_cal_sum[6]/100;
	cal_data[3]=mpu6050_cal_sum[0]/100;
	cal_data[4]=mpu6050_cal_sum[1]/100;
	cal_data[5]=mpu6050_cal_sum[2]/100;
}


void Load_Calibration_Parameter(float *q_bias)
{
	int cal_par[12];
	signed short int Q_BIAS[6];
	
	Cal_MPU6050_Data(cal_par);	   //è‡ªæˆ‘æ ¡å‡†è¿™ä¸ªè¿˜æ˜¯å¾ˆéœ€è¦çš„
	//
	Q_BIAS[0] = (signed short int)cal_par[0];		//é™€èºä»ªæ ¡éªŒå‚æ•°
	Q_BIAS[1] = (signed short int)cal_par[1];     	//é™€èºä»ªæ ¡éªŒå‚æ•°
	Q_BIAS[2] = (signed short int)cal_par[2];     	//é™€èºä»ªæ ¡éªŒå‚æ•°
	
	Q_BIAS[3] = (signed short int)cal_par[3];		//åŠ é€Ÿåº¦æ ¡éªŒå‚æ•°
	Q_BIAS[4] = (signed short int)cal_par[4];     	//åŠ é€Ÿåº¦æ ¡éªŒå‚æ•°
	Q_BIAS[5] = (signed short int)cal_par[5];     	//åŠ é€Ÿåº¦æ ¡éªŒå‚æ•°

	q_bias[0] = 2000 * Q_BIAS[0]/32768;	// è§’åŠ é€Ÿåº¦åç½®å€¼
	q_bias[1] = 2000 * Q_BIAS[1]/32768;
	q_bias[2] = 2000 * Q_BIAS[2]/32768;
}


void GetSensorData(Sensor *sensordata)
{
	if(sensordata != NULL){
		sensordata->accel_x = GetData(ACCEL_XOUT_H);
		sensordata->accel_y = GetData(ACCEL_YOUT_H);
		sensordata->accel_z = GetData(ACCEL_ZOUT_H);
		sensordata->gyro_x = GetData(GYRO_XOUT_H);
		sensordata->gyro_y = GetData(GYRO_YOUT_H);
		sensordata->gyro_z = GetData(GYRO_ZOUT_H);
	}
}

uint8 MPU6050_exit(void)
{
	close(fd);
	return(1);
}


