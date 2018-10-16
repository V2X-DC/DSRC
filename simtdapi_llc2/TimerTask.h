//------------------------------------------------------------------------------
// Copyright (c) 2017 SCUT Sensor network laboratory
//------------------------------------------------------------------------------
#ifndef __TimerTask_H__
#define __TimerTask_H__

#include <stdint.h>
#include "mpu6050.h"

#define IS_LOCATED 0x00000001
#define NOT_LOCATED 0xFFFFFFFE
#define IS_NORTH_LAT 0x00000002
#define NOT_NORTH_LAT 0xFFFFFFFD
#define IS_EAST_LON 0x00000004
#define NOT_EAST_LON 0xFFFFFFFB
#define IS_BRAKE 0x00000008
#define NOT_BRAKE 0xFFFFFFF7
#define IS_OVER_SPEED 0x00000010
#define NOT_OVER_SPEED 0xFFFFFFEF
#define IS_FATIGUE_DRIVE 0x00000020
#define NOT_FATIGUE_DRIVE 0xFFFFFFDF
#define IS_TURN_LEFT 0x00000040
#define NOT_TURN_LEFT 0xFFFFFFBF
#define IS_TURN_RIGHT 0x00000080
#define NOT_TURN_RIGHT 0xFFFFFF7F
#define IS_COLLISION 0x00000100
#define NOT_COLLISION 0xFFFFFEFF
#define IS_TURN_OVER 0x00000200
#define NOT_TURN_OVER 0xFFFFFDFF

#define SET_BRAKE_RANK_0 0x00000000
#define SET_BRAKE_RANK_1 0x00000400
#define SET_BRAKE_RANK_2 0x00000800
#define SET_BRAKE_RANK_3 0x00000C00
#define SET_BRAKE_RANK_4 0x00001000
#define CLEAR_BRAKE_RANK 0xFFFFE3FF

#define SET_TURN_RANK_0 0x00000000
#define SET_TURN_RANK_1 0x00002000
#define SET_TURN_RANK_2 0x00004000
#define SET_TURN_RANK_3 0x00006000
#define SET_TURN_RANK_4 0x00008000
#define CLEAR_TURN_RANK 0xFFFF1FFF

#define SET_TURN_OVER_RANK_0 0x00000000
#define SET_TURN_OVER_RANK_1 0x00010000
#define SET_TURN_OVER_RANK_2 0x00020000
#define SET_TURN_OVER_RANK_3 0x00030000
#define SET_TURN_OVER_RANK_4 0x00040000

#define CLEAR_TURN_OVER_RANK 0xFFF8FFFF

#define SET_COLLISION_RANK_0 0x00000000
#define SET_COLLISION_RANK_1 0x00080000
#define SET_COLLISION_RANK_2 0x00100000
#define SET_COLLISION_RANK_3 0x00180000
#define CLEAR_COLLISION_RANK 0xFFE7FFFF

#define SET_SPEEDUP_RANK_0 0x00000000
#define SET_SPEEDUP_RANK_1 0x00200000
#define SET_SPEEDUP_RANK_2 0x00400000
#define SET_SPEEDUP_RANK_3 0x00600000
#define SET_SPEEDUP_RANK_4 0x00800000
#define CLEAR_SPEEDUP_RANK 0xFF1FFFFF



#define CLEAR_ALL 0x00000000


void mpu6050_start(void);
void broadcast_start(void);
void neighbortable_start(void);


void mpu6050_stop(void);
void broadcast_stop(void);
void neighbor_stop(void);


//mpu6050_start之后调用
struct CarStatus *GetCS(void);
Sensor *GetSensor(void);
uint32_t GetDriveStatus(void);


#endif 
