#ifndef __JSON_HANDLE_H
#define __JSON_HANDLE_H

#include "main.h"

// 定义固件信息结构体
typedef struct {
    char version[32];    // 固件版本号
    char url[256];       // 固件下载URL
    uint32_t size;       // 固件大小
    char md5[64];        // MD5校验值
    uint8_t isValid;     // 是否成功解析
} OTA_FirmwareInfo_t;

OTA_FirmwareInfo_t OTA_ParseFirmwareInfo(const char *jsonStr);






#endif

