#include "bsp_4G.h"

/**
 * @brief 连接阿里云IoT平台 (Connect to Aliyun IoT Platform)
 * @return 0: 成功 (Success), <0: 失败代码 (Error code)
 */
int Aliyun_4GConnect(void)
{
    char atcmd[128] = {0};
    
    // 1. 测试AT命令响应 (Test AT command response)
    if(!UART3_SendAT("AT\r\n", "OK", 1000)) {
        printf("Error: 4G module not responding\r\n");
        return -1;
    }
    
    // 2. 检查SIM卡状态 (Check SIM card status)
    if(!UART3_SendAT("AT+CPIN?\r\n", "+CPIN: READY", 1000)) {
        printf("Error: SIM card not ready\r\n");
        return -2;
    }
    
    // 3. 查询信号质量 (Query signal quality)
    if(!UART3_SendAT("AT+CSQ\r\n", "+CSQ:", 1000)) {
        printf("Error: Cannot get signal quality\r\n");
        return -3;
    }
    
    // 4. 检查GSM网络注册状态 (Check GSM network registration status)
    if(!UART3_SendAT("AT+CREG?\r\n", "+CREG: 0,1", 2000) && 
       !UART3_SendAT("AT+CREG?\r\n", "+CREG: 0,5", 2000)) {
        printf("Error: Not registered to GSM network\r\n");
        return -4;
    }
    
    // 5. 配置MQTT接收模式 (Configure MQTT receive mode)
    if(!UART3_SendAT("AT+QMTCFG=\"recv/mode\",0,0,1\r\n", "OK", 1000)) {
        printf("Error: Failed to configure MQTT receive mode\r\n");
        return -5;
    }
    
    // 6. 配置阿里云设备三元组 (Configure Aliyun device triple)
    sprintf(atcmd, "AT+QMTCFG=\"aliauth\",0,\"%s\",\"%s\",\"%s\"\r\n", 
            ProductKey, DeviceName, DeviceSecret);
    if(!UART3_SendAT(atcmd, "OK", 2000)) {
        printf("Error: Failed to configure Aliyun device info\r\n");
        return -6;
    }
    
    // 7. 连接阿里云MQTT服务器 (Connect to Aliyun MQTT server)
    if(!UART3_SendAT("AT+QMTOPEN=0,\"iot-as-mqtt.cn-shanghai.aliyuncs.com\",1883\r\n", 
                     "+QMTOPEN: 0,0", 10000)) {
        printf("Error: Failed to open MQTT client\r\n");
        return -7;
    }
    
    // 8. 连接MQTT服务器，需在QMTOPEN成功后10秒内发起 
    // (Connect to MQTT server, must be initiated within 10 seconds after QMTOPEN success)
    if(!UART3_SendAT("AT+QMTCONN=0,0\r\n", "+QMTCONN: 0,0,0", 5000)) {
        printf("Error: Failed to connect to MQTT server\r\n");
        return -8;
    }
    
    printf("Successfully connected to Aliyun IoT platform\r\n");
    return 0;  // 连接成功 (Connection successful)
}

/**
 * @brief  从4G模块获取当前日期(年/月/日) - 优先使用QLTS，再退回CCLK
 * @param  ymd  整型数组, 长度>=3: ymd[0]=year(4位), ymd[1]=month(1-12), ymd[2]=day(1-31)
 * @return 0 成功; <0 失败
 *
 * 模块典型返回格式:
 *  1) AT+QLTS\r\n  -->  +QLTS: "2025/09/13,14:22:05+32"  (年/月/日,时:分:秒+时区四分之一小时)
 *  2) AT+CCLK?      -->  +CCLK: "25/09/13,14:22:05+32"     (年为2位)
 */
int _4G_GetDateYMD(int ymd[3])
{
    if (!ymd) return -10;

    // 尝试 QLTS
    if (UART3_SendAT("AT+QLTS=2\r\n", "+QLTS:", 1500))
    {
        char *resp = (char*)UART3_GetRxData();
        // 查找 +QLTS: "
        char *p = strstr(resp, "+QLTS:");
        if (p)
        {
            char *q = strchr(p, '"');
            if (q)
            {
                // 期望格式: "YYYY/MM/DD,HH:MM:SS+TZ"
                int Y,M,D;
                if (sscanf(q+1, "%d/%d/%d", &Y, &M, &D) == 3)
                {
                    ymd[0] = Y; ymd[1] = M; ymd[2] = D;
                    return 0;
                }
            }
        }
    }

    // 退回 CCLK
    if (UART3_SendAT("AT+CCLK?\r\n", "+CCLK:", 1500))
    {
        char *resp = (char*)UART3_GetRxData();
        char *p = strstr(resp, "+CCLK:");
        if (p)
        {
            char *q = strchr(p, '"');
            if (q)
            {
                // 格式: "YY/MM/DD,HH:MM:SS+TZ"  -> 年需要+2000
                int y2,M,D;
                if (sscanf(q+1, "%d/%d/%d", &y2, &M, &D) == 3)
                {
                    ymd[0] = (y2 < 100 ? y2 + 2000 : y2);
                    ymd[1] = M;
                    ymd[2] = D;
                    return 0;
                }
            }
        }
    }

    return -1; // 获取失败
}


int _4G_get_time_extern(int time[3]){
    ESP8266_Init(USART3,115200);
    Aliyun_4GConnect();
    return _4G_GetDateYMD(time);
}