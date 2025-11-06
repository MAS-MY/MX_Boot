#include "JSON_Handle.h"
#include "cJSON.h"

/**
 * @brief 解析OTA固件升级JSON数据
 * @param jsonStr 收到的JSON字符串
 * @return OTA固件信息结构体
 */
OTA_FirmwareInfo_t OTA_ParseFirmwareInfo(const char *jsonStr)
{
    OTA_FirmwareInfo_t info = {0};
    cJSON *root = NULL;
    cJSON *params = NULL;
    
    // 默认设置为无效
    info.isValid = 0;
    
    // 检查输入参数
    if(jsonStr == NULL) {
        printf("Error: JSON string is NULL\r\n");
        return info;
    }
    
    // 跳过可能的AT命令前缀，找到JSON开始的位置
    const char *json_start = strstr(jsonStr, "{");
    if(json_start == NULL) {
        printf("Error: No JSON data found\r\n");
        return info;
    }
    
    // 解析JSON数据
    root = cJSON_Parse(json_start);
    if(root == NULL) {
        printf("Error: Failed to parse JSON\r\n");
        return info;
    }
    cJSON *data_container = cJSON_GetObjectItem(root, "data");
    
    // 提取固件信息
    cJSON *version = cJSON_GetObjectItem(data_container, "version");
    cJSON *url = cJSON_GetObjectItem(data_container, "url");
    cJSON *size = cJSON_GetObjectItem(data_container, "size");
    cJSON *md5 = cJSON_GetObjectItem(data_container, "md5");
    
    // 检查是否成功获取所有字段
    if(version && url && size && md5) {
        // 复制数据到结构体
        strncpy(info.version, version->valuestring, sizeof(info.version) - 1);
        strncpy(info.url, url->valuestring, sizeof(info.url) - 1);
        info.size = (uint32_t)size->valueint;
        strncpy(info.md5, md5->valuestring, sizeof(info.md5) - 1);
        info.isValid = 1;
        
        printf("Firmware info parsed successfully\r\n");
    } else {
        printf("Error: Missing required fields in firmware info\r\n");
    }
    // 显示固件信息
    printf("\r\n====== Firmware Information ======\r\n");
    printf("Version: %s\r\n", info.version);
    printf("URL: %s\r\n", info.url);
    printf("Size: %lu bytes\r\n", info.size);
    printf("MD5: %s\r\n", info.md5);
    printf("================================\r\n\r\n");
        
    // 释放JSON资源
    cJSON_Delete(root);
    return info;
}