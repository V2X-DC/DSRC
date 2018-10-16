#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "CarSta.h"
/*LocalStatu and Neighbor status*/

LIST(Car_list);//这个是头
LIST(Packet_list);
myCStatus CarS;
int CarSize;

#define PACKETEXPIRETIME 100.0f

//LocalStatu *GetLocalStatu(void)
//{
//	return &CarS;
//}
//
//UniPacket *GetUniPacket(void)
//{
//	return &uniPacket;
//}

//void UniPacket_init(void)
//{
//	UniPacket *uniPacket = (UniPacket *)malloc(sizeof(uniPacket));
//	memset(uniPacket, 0, sizeof(UniPacket));
//	INIT_LIST_HEAD(uniPacket->list);
//
//	list_add(&(uniPacket), &Packet_list);
//}

void CarStatu_init(void)
{
//	LocalStatu *CarS = (LocalStatu *)malloc(sizeof(LocalStatu));
	memset(&CarS, 0, sizeof(myCStatus));
//	INIT_LIST_HEAD(CarS->list);

	FILE *rfp;
	if((rfp = fopen("/data/plate.txt", "r")) == NULL){
		memcpy(CarS.plate, "yuA-00000", 9);
	}else{
		char readline[10];
		char *fgetsflag;
		if((fgetsflag = fgets(readline, 9, rfp)) == NULL){
			memcpy(CarS.plate, "yuA-00000", 9);
		}else{
            memcpy(CarS.plate, readline, 9);
		}
		fclose(rfp);
	}
	CarS.valid = false;// GPS Is valid ?
	CarSize = 0;
}

LocalStatu *neigh_insert(LocalStatu ls)
{
	list_t *pos, *tmp;
	LocalStatu *lstmp;

	/*Check if we already have an entry for dest_plate*/
	list_foreach_safe(pos, tmp, &Car_list){
		lstmp = (LocalStatu *)pos;
		if(memcmp(lstmp->status.plate, ls.status.plate, 9) == 0){
			fprintf(stderr, "%s already exist in neighbor table!", ls.status.plate);
			return NULL; //neigh already exit in table
		}
	}//如果已经存在这个，则返回本身有的

	if((lstmp =(LocalStatu *)malloc(sizeof(LocalStatu))) == NULL){
		fprintf(stderr, "Malloc failed!\n");
		return NULL;
	}

	memset(lstmp, 0, sizeof(LocalStatu));
	memcpy(lstmp, &ls, sizeof(LocalStatu));
	INIT_LIST_HEAD(&(lstmp->list));
	list_add(&(lstmp->list), &Car_list);//插入邻居列表
	CarSize ++;
	return lstmp;
}




LocalStatu *neigh_find(char *plate)
{
	list_t *pos;
	list_t *tmp;
	LocalStatu *lstmp;
	list_foreach_safe(pos, tmp, &Car_list){
	     lstmp = (LocalStatu *)pos;
		if(memcmp(plate, lstmp->status.plate, 9) == 0){
			return lstmp;
		}
	}
	return NULL;
}


//更新就更新，插入就插入
LocalStatu *neigh_update(LocalStatu ls)
{
	if(ls.status.valid)
	{
		LocalStatu *lstmp = (LocalStatu *)neigh_find(ls.status.plate);
		if(lstmp == NULL){
			return NULL;
		}else{
			memcpy(&(lstmp->status), &(ls.status), sizeof(CStatus));//list不用更新
			return lstmp;
		}
	}
    return NULL;
}

void neigh_delete(LocalStatu *ls)
{
	if(!ls){
		return;
	}
	LocalStatu *lstmp = (LocalStatu *)neigh_find(ls->status.plate);
	if(lstmp == NULL){
		return;
	}

	list_detach(&lstmp->list);
	free(lstmp);
	CarSize--;
	return;
}


//注意这个参数列表是从Packet_list获取到的UniPacket才能正确删除掉，因为这样子的list才是在链表中，其他脱离链表的作用
static void unipacket_delete(UniPacket *up)
{
	if(!up){
		return;
	}

	list_detach(&up->list);
	free(up);
	return;
}

//这个应该插入的时候记得更新
UniPacket *unipacket_insert(UniPacket up)
{
	list_t *pos, *tmp;
	UniPacket *lstmp;
	time_t t;
	t = time(NULL);

	/*Check if we already have an entry for dest_plate*/
	list_foreach_safe(pos, tmp, &Packet_list){
		lstmp = (UniPacket *)pos;
		if(memcmp(lstmp->plate, up.plate, 9) == 0){
			if((difftime(lstmp->Newtime, t) > PACKETEXPIRETIME)||(lstmp->seqno < up.seqno)){//本地无效，重新插入
				unipacket_delete(lstmp);
				break;
			}else{
				return NULL;
			}
		}
	}//如果已经存在这个，则返回本身有的

	if((lstmp =(UniPacket *)malloc(sizeof(UniPacket))) == NULL){
		fprintf(stderr, "Malloc failed!\n");
		exit(-1);
	}

	memset(lstmp, 0, sizeof(UniPacket));
	memcpy(lstmp, &up, sizeof(UniPacket));
	INIT_LIST_HEAD(&(lstmp->list));
	list_add(&(lstmp->list), &Packet_list);//插入邻居列表
	return lstmp;
}



//用于确认是不是接收过
bool unipacket_find(char *plate, uint32_t seq)
{
	list_t *pos;
	list_t *tmp;
	UniPacket *lstmp;
	time_t t = time(NULL);
	
	list_foreach_safe(pos, tmp, &Packet_list){
	    lstmp = (UniPacket *)pos;
		if(memcmp(plate, lstmp->plate, 9) == 0){
			if(difftime(lstmp->Newtime, t) > PACKETEXPIRETIME){//过期了
				return false;
			}else if(lstmp->seqno >= seq)
				return true;
			else{
				return false;
			}
		}
	}
	return false;
}



void SetPlate(char *s)
{
    memcpy(CarS.plate, s, 9);
}

void SetAccel(struct Accel *s)
{
	memcpy(&CarS.accel, s, sizeof(struct Accel));
	return;
}

uint32_t GetSeq(void)
{
	return CarS.seqno;
}

void SetCarSeq(uint32_t seq)
{
	CarS.seqno = seq;
}

void ResetCarSeq(void)
{
	CarS.seqno = 0;
}

void UpdataTime(void)
{
	time(&CarS.Newtime);//单位是s，要更高精度的话，可以通过获取timeval
}

void SetNewTime(time_t t)
{
	CarS.Newtime = t;
}

void SetExpiretime(double t)
{
	CarS.expiretime = t;
}

bool IsValid(void)
{
	return CarS.valid;
}

void SetValid(void)
{
	CarS.valid = true;
}

void ResetValid(void)
{
	CarS.valid = false;
}
 
GpsLocation *GetGps(void)
{
	return &CarS.location;
}

void SetGps(GpsLocation *Gpsl)
{
    memset(&CarS.location, 0, sizeof(GpsLocation));
	memcpy(&CarS.location, Gpsl, sizeof(GpsLocation));
}

void SetCarStatus(struct CarStatus *cs)
{
	memset(&CarS.carstatus, 0, sizeof(struct CarStatus));
	memcpy(&CarS.carstatus, cs, sizeof(struct CarStatus));
}

struct CarStatus *GetCarStatus(void)
{
	return &CarS.carstatus;
}

struct Accel *GetAccel(void)
{
	return &CarS.accel;
}
