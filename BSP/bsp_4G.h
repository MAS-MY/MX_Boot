#ifndef __BSP_4G_H
#define __BSP_4G_H

#include "main.h"

#define ProductKey 		"k1mg4Nbe8hu"
#define DeviceName 		"BootLoader"
#define DeviceSecret 	"3a151aee081e026d074978009496b952"

int Aliyun_4GConnect(void);
int _4G_get_time_extern(int time[3]);
int _4G_GetDateYMD(int ymd[3]);











#endif
