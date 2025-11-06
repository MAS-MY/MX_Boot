#include "OTA_Command.h"
#include <stdio.h>
#include <string.h>
#include "usart.h"
#include "main.h"
#include "cJSON.h"
#include "JSON_Handle.h"
// Define current firmware version
#define CURRENT_VERSION "1.0.0"
extern uint8_t OTA_CheckMQTTConnection(void);
extern ring_buffer rx_buf;
/**
 * @brief 上报设备固件版本信息 (Report device firmware version information)
 * 立即上报当前固件版本 (Report current firmware version immediately)
 */
void OTA_DeviceInform(char *version)
{
    char payload[128];
    char end_char = 0x1A; // Ctrl+Z 结束符

    // 构建JSON格式的版本信息 (Construct version information in JSON format)
    sprintf(payload, "{\"id\":\"1234\",\"params\":{\"version\":\"%s\"}}",
            version);
    
    // 发布版本信息到对应主题 (Publish version information to corresponding topic)
    if(!UART3_SendAT("AT+QMTPUB=0,0,0,0,\"" OTA_TOPIC_DEVICE_INFORM "\"\r\n", ">", 1000)) {
        printf("Error: Failed to start publishing version\r\n");
        return;
    }
    // 发送消息内容 (Send message content)
    UART3_SendString(payload);
    
    // 发送结束符 (Send end character)
    UART3_SendData((uint8_t*)&end_char, 1);

    printf("Successfully reported firmware version: %s\r\n", version);
}

/**
 * @brief 上报OTA升级进度
 * @param step 升级步骤: "upgrade"(开始), "downloading"(下载中), "done"(完成)
 * @param progress 进度百分比(0-100)
 * @param desc 描述信息(可选)
 * @return 0: 成功, <0: 失败
 */
int OTA_ReportProgress(const char* step, int progress, const char* desc)
{
    char payload[256] = {0};
    char end_char = 0x1A; // Ctrl+Z结束符
    
    // 构建上报进度的JSON消息
    if(desc && strlen(desc) > 0) {
        sprintf(payload, "{\"id\":\"1234\",\"code\":200,\"data\":{\"step\":\"%s\",\"desc\":\"%s\",\"progress\":%d}}", 
                step, desc, progress);
    } else {
        sprintf(payload, "{\"id\":\"1234\",\"code\":200,\"data\":{\"step\":\"%s\",\"progress\":%d}}", 
                step, progress);
    }
    
    printf("Reporting progress: step=%s, progress=%d%%\r\n", step, progress);
    
    // 发布进度消息到OTA进度主题
    if(!UART3_SendAT("AT+QMTPUB=0,0,0,0,\"" OTA_TOPIC_DEVICE_PROGRESS "\"\r\n", ">", 1000)) {
        printf("Error: Failed to publish progress\r\n");
        return -1;
    }
    
    // 发送消息内容
    UART3_SendString(payload);
    UART3_SendData((uint8_t*)&end_char, 1);
    
    // 等待发送完成
    HAL_Delay(500);
    
    return 0;
}

/**
 * @brief 从HTTP URL下载固件并保存到Flash
 * @param firmwareInfo 固件信息结构体
 * @return 0:成功 <0:失败
 */
int OTA_DownloadFirmware(OTA_FirmwareInfo_t *firmwareInfo)
{
    char cmd[128] = {0};
    uint32_t url_len = strlen(firmwareInfo->url);
    int result = -1;
    char end_char = 0x1A; // Ctrl+Z 结束符
    
    printf("\r\n====== Downloading Firmware ======\r\n");
    printf("Target version: %s\r\n", firmwareInfo->version);
    printf("Size: %lu bytes\r\n", firmwareInfo->size);
    printf("Download address: 0x08040000\r\n");
    printf("================================\r\n\r\n");
    
    // 1. 设置HTTP URL
    sprintf(cmd, "AT+QHTTPURL=%d,80\r\n", url_len);
    if(!UART3_SendAT(cmd, "CONNECT", 2000)) {
        printf("Error: Failed to set HTTP URL\r\n");
        return -1;
    }
    
    // 2. 发送URL内容
    printf("Sending URL...\r\n");
    UART3_SendString(firmwareInfo->url);
    UART3_SendString("\r\n");
    HAL_Delay(500); // 等待URL发送完成
    
    // 3. 发送HTTP GET请求
    printf("Starting HTTP download...\r\n");
    sprintf(cmd, "+QHTTPGET: 0,200,%lu\r\n", firmwareInfo->size);
    if(!UART3_SendAT("AT+QHTTPGET=80\r\n", cmd, 10000)) {
        printf("Error: Failed to download firmware\r\n");
        return -2;
    }
       
    printf("Reading firmware data to file...\r\n");
    UART3_ClearRx();
    extern_Backup_App_With_Header(); // 备份当前应用
    // 直接下载到模块的文件系统
    UART3_SendString("AT+QHTTPREADFILE=\"UFS:firmware.bin\",80\r\n");
    HAL_Delay(1000);  // 等待足够长时间完成下载
    // 检查下载结果
    printf("Download result: %s\r\n", (char*)UART3_GetRxData());
    // 从文件系统读取固件
    UART3_ClearRx();
    UART3_SendString("AT+QFDWL=\"UFS:firmware.bin\"\r\n");
    HAL_Delay(1000);
    // 等待数据接收完成，确保所有固件数据已到达
    
    // 提取接收到的二进制数据
    uint8_t *data = UART3_GetRxData();
    uint32_t dataLen = UART3_GetRxNum() - 27;
    memset(data + dataLen, 0xFF, 27);

    printf("Received %lu bytes\r\n", dataLen);

    uint32_t firmware_size = dataLen;
	
    for(int i = 0;i < dataLen; i++) {
        printf("%02X ", data[i]);
        if((i+1) % 32 == 0) printf("\r\n");
    }
        
    printf("Received %lu bytes of data\r\n", dataLen);
    // 将固件数据写入Flash的0x08040000地址
    printf("Writing firmware to Flash address 0x08040000...\r\n");
    // 1. 解锁Flash
    HAL_FLASH_Unlock(); 
    // 3. 写入固件数据（按字写入，4字节为一组）
    uint32_t address = 0x08040000;
    uint32_t *data32 = (uint32_t*)data;
    uint32_t word_count = (dataLen + 3) / 4;  // 向上取整，确保所有字节都被写入
    
    for(uint32_t i = 0; i < word_count; i++) {
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, data32[i]) != HAL_OK) {
            printf("Error: Flash write failed at address 0x%08lX!\r\n", address);
            HAL_FLASH_Lock();
            return -4;
        }
        address += 4;  // 移动到下一个字位置
    }
    
    // 4. 锁定Flash
    HAL_FLASH_Lock();    
    printf("Firmware successfully written to Flash!\r\n");
    return 0;
}
/**
 * @brief 主动拉取固件更新
 * 向云平台请求最新固件信息，如有可用更新则下载
 * @return 0:成功 <0:失败
 */
int OTA_DeviceFirmwareGet(void)
{
    char request_payload[128];
    char receivedData[512] = {0};
    uint32_t timeout = 0;
    
    printf("Requesting firmware update information...\r\n");
    
    // 1. 订阅固件响应主题
    if(!UART3_SendAT("AT+QMTSUB=0,1,\"" OTA_TOPIC_DEVICE_UPGRADE "\",0\r\n", "OK", 2000)) {
        printf("Failed to subscribe to OTA upgrade topic!\r\n");
        return -1;
    }
    printf("%s\r\n", (char *)UART3_GetRxData());

    printf("Request sent. Waiting for response...\r\n");
    
    // 清理接收缓冲区
    UART3_ClearRx();
    
    // 4. 等待固件信息响应
    while(1) {
            if(UART3_GetRxNum() > 0) {
                uint16_t dataLen = UART3_GetRxNum();
                printf("\n\r Intern receive %s\r\n",(char *)UART3_GetRxData());

                // 安全地复制数据
                if(dataLen < sizeof(receivedData)) {
                    memcpy(receivedData, UART3_GetRxData(), dataLen);
                    receivedData[dataLen] = '\0';
                } else {
                    memcpy(receivedData, UART3_GetRxData(), sizeof(receivedData)-1);
                    receivedData[sizeof(receivedData)-1] = '\0';
                    printf("Warning: Data truncated (%d bytes)\r\n", dataLen);
                }

                    // 解析JSON数据
                    char *json_start = strstr(receivedData, "{");
                    if(json_start != NULL) {
                        // 解析固件信息
                        OTA_FirmwareInfo_t firmwareInfo = OTA_ParseFirmwareInfo(json_start);
                        
                        if(firmwareInfo.isValid) {  
                            // 判断是否需要升级
                            if(strcmp(firmwareInfo.version, CURRENT_VERSION) > 0) {
                                printf("New firmware available. Starting upgrade...\r\n");
                                
                                // 开始升级，上报状态
                                OTA_ReportProgress("upgrade", 0, "start upgrade");
                                
                                // 下载固件
                                if(OTA_DownloadFirmware(&firmwareInfo) == 0) {
                                    printf("Firmware download successful\r\n");
                                    OTA_ReportProgress("done", 100, "success");
                                    
                                    // 更新版本信息
                                    OTA_DeviceInform(firmwareInfo.version);
                                } else {
                                    printf("Firmware download failed\r\n");
                                    OTA_ReportProgress("failed", 0, "download error");
                                }
                            } else {
                                printf("Current firmware is up-to-date\r\n");
                            }
                            
                            // 取消订阅
                            UART3_SendAT("AT+QMTUNS=0,1,\"" OTA_TOPIC_DEVICE_UPGRADE "\"\r\n", "OK", 2000);
                            return 0;
                        } else {
                            printf("Error: Invalid firmware information\r\n");
                        }
                    }
                
                // 清理缓冲区
                UART3_ClearRx();
            }
        // 检查用户中断
        unsigned char c;
        if(ring_buffer_read(&c, &rx_buf) == 0) {
            printf("\r\nUser interrupted\r\n");
            UART3_SendAT("AT+QMTUNS=0,1,\"" OTA_TOPIC_DEVICE_UPGRADE "\"\r\n", "OK", 2000);
            return -3;
        }
        
        // 超时检测
        timeout++;
        if(timeout > 300) { // 约30秒超时
            printf("Timeout waiting for firmware information\r\n");
            UART3_SendAT("AT+QMTUNS=0,1,\"" OTA_TOPIC_DEVICE_UPGRADE "\"\r\n", "OK", 2000);
            return -4;
        }
        
        HAL_Delay(100);
    }
}

/**
 * @brief OTA menu selection interface
 * Provide interactive menu for users to select different OTA functions
 */
void OTA_MenuSelect(void)
{
    uint8_t choice = 0;
    
    while(1)
    {
        printf("\r\n================ OTA Menu ================\r\n");
        printf("1. Report Firmware Version\r\n");
        printf("2. Check for Firmware Updates\r\n");
        printf("3. Restart EC800M Module\r\n");
        printf("0. Exit OTA Menu\r\n");

        printf("==========================================\r\n");
        printf("Select function (0-3): ");

        // Wait for user input
        while(ring_buffer_read(&choice, &rx_buf) != 0);
        printf("%c\r\n", choice);
        
        // Execute function based on selection
        switch(choice - '0')
        {
            case 1:
                OTA_DeviceInform(CURRENT_VERSION);
                break;
            case 2:
                OTA_DeviceFirmwareGet();
                break;
            case 3:
                // 重启EC800M模块
                printf("Restarting EC800M module...\r\n");
                UART3_SendAT("AT+CFUN=1,1\r\n", "OK", 2000);
                printf("Restart command sent. Module will reboot shortly.\r\n");
                HAL_Delay(5000); // 等待模块重启
            case 0:
                printf("Exiting OTA menu\r\n");
                return;
            default:
                printf("Invalid choice, please try again\r\n");
                break;
        }
        
        HAL_Delay(500);
    }
}

/**
 * @brief OTA command main function
 */
static int ota_f(void)
{
    ESP8266_Init(USART3,115200);
    Aliyun_4GConnect();

    // Display menu and handle user selection
    OTA_MenuSelect();
    
    return 0;
}

struct command OTA_cmd = {
    "ota",
    "trigger OTA firmware update via wireless\r\n",
    "Usage:\r\n"
    "    ota - Enter OTA menu to select functions\r\n",
    ota_f,
};
