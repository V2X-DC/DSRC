//------------------------------------------------------------------------------
// Copyright (c) 2017 SCUT Sensor network laboratory
//------------------------------------------------------------------------------
#ifndef __MPU6050_H__
#define __MPU6050_H__
#include <sys/types.h>

typedef struct sensor
{
	short accel_x;
	short accel_y;
	short accel_z;
	short gyro_x;
	short gyro_y;
	short gyro_z;
}Sensor;

void Load_Calibration_Parameter(float *);
unsigned char MPU6050_Init(void);
unsigned char MPU6050_exit(void);
//get data
void GetSensorData(Sensor *);

#endif 
