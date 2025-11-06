#ifndef __OTA_COMMMAND_H
#define __OTA_COMMMAND_H

#include "main.h"
#include "command.h"
			// 设备上报固件升级信息
#define  OTA_TOPIC_DEVICE_INFORM    "/ota/device/inform/k1mg4Nbe8hu/BootLoader"
			// 固件升级信息下行
#define  OTA_TOPIC_DEVICE_UPGRADE   "/ota/device/upgrade/k1mg4Nbe8hu/BootLoader"
			// 设备上报固件升级进度
#define	 OTA_TOPIC_DEVICE_PROGRESS  "/ota/device/progress/k1mg4Nbe8hu/BootLoader"
			// 设备主动拉取固件升级信息
#define	 OTA_TOPIC_DEVICE_GET       "/sys/k1mg4Nbe8hu/BootLoader/thing/ota/firmware/get"


extern struct command OTA_cmd; 

 //    {"id":"0000","params":{"version": "0.0.1"}}

#endif
