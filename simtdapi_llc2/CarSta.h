#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include <sys/time.h>
#include "list.h"

/** Represents a location. */
typedef struct {
    /** Represents latitude in degrees. */
    double          latitude;
	char            clat;//南纬北纬
    /** Represents longitude in degrees. */
    double          longitude;
	char 			clon;//西经东经
    /** Represents altitude in meters above the WGS 84 reference
     * ellipsoid. */
    double          altitude;
    /** Represents speed in meters per second. */
    float           speed;
    /** Represents heading in degrees. */
    float           bearing;
} GpsLocation;


struct CarStatus{
	int brake_rand;	//刹车
	int speedup_rand; //加速
	int turn_rand;	//转弯 //bit 5-6 : 0 no turn, 1 left, 2 right; bit 0-3 : rand[0, 1, 2, 3, 4]
	int Rollover_rand; //侧翻
};

struct Accel{
	float x;
	float y;
	float z;
};

typedef struct {
	uint32_t seqno;
	time_t Newtime;
	double expiretime; //s
	bool valid;
	char plate[9];
	GpsLocation location;
	uint32_t carstatus;
	struct Accel accel;
}CStatus;

typedef struct {
	uint32_t seqno;
	time_t Newtime;
	double expiretime; //s
	bool valid;
	char plate[9];
	GpsLocation location;
	struct CarStatus carstatus;
	struct Accel accel;
}myCStatus;



typedef struct {
	CStatus status;
	list_t list;
}LocalStatu;

typedef struct {
	uint32_t seqno;
	char plate[9];
	time_t Newtime;
	list_t list;
}UniPacket;//用于标识接收过的packet,在多跳传播中防止多次接收同样的packet

UniPacket *unipacket_insert(UniPacket up);
bool unipacket_find(char *plate, uint32_t seq);


LocalStatu *neigh_update(LocalStatu ls);

void neigh_delete(LocalStatu *ls);
LocalStatu *neigh_find(char *plate);
LocalStatu *neigh_insert(LocalStatu ls);
//LocalStatu *GetLocalStatu(void);

void SetPlate(char *s);

void SetAccel(struct Accel *s);

void CarStatu_init(void);

uint32_t GetSeq(void);

void SetCarSeq(uint32_t seq);

void ResetCarSeq(void);

void UpdataTime(void);

void SetNewTime(time_t t);

void SetExpiretime(double t);

bool IsValid(void);

void SetValid(void);

void ResetValid(void);

GpsLocation *GetGps(void);

void SetGps(GpsLocation *l);

void SetCarStatus(struct CarStatus *cs);

struct CarStatus *GetCarStatus(void);

struct Accel *GetAccel(void);
