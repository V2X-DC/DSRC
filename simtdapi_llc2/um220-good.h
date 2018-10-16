//------------------------------------------------------------------------------
// Copyright (c) 2017 SCUT Sensor network laboratory
//------------------------------------------------------------------------------
#ifndef __UM220_GOOD_H__
#define __UM220_GOOD_H__

typedef struct RMCmsg
{
    //UTC时间
    int   time_hh;     //小时
    int   time_mm;     //分钟
    double time_ss;     //秒
    //位置有效标识,V无效,A有效
    char  status; 
    //纬度
    int   lat_du;      //度
    double lat_fen;     //分
    char  clat;        //纬度指示,N北纬,S南纬
    //经度
    int   lon_du;      //度
    double lon_fen;     //分
    char  clon;        //经度指示,E东经,W西经
    //速率
    float spd;         //节
    float cog;         //度
    //UTC日期
    int   date_dd;     //日
    int   date_mm;     //月
    int   date_yy;     //年
    //磁偏角
    char  mv;           //空
    //磁偏角方向           
    char  mvE;          //固定为E
    //定位模式
    char  mode;         //N未定点,A单点定位
}stRMCmsg, *pstRMCmsg;

typedef struct tmpMsg
{
    char time[11];//UTC时间
    char status;//有效与否
    char lat[12];//纬度
    char clat;//北纬还是南纬
    char lon[13];//经度
    char clon;//东经西经
    char spd[10];//地面速率，节
    char cog[10];//地面航向，单位为度，从北向起顺时针计算
    char date[7];//UTC日期，日月年
    char mode;//定位模式
}stTmpMsg, *pstTmpMsg;


/*  指针参量,1秒钟分配和执行一次会不会有问题*/
void parseData(char *buf, pstRMCmsg result);

/* 	这个用于返回文件描述符就好了 	*/
/* 	这个文件描述符用于读取GPS数据	*/
int um220_init();

#endif 