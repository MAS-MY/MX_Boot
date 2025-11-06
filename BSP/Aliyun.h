#ifndef   __ALIYUN_H
#define   __ALIYUN_H

				 /*              本程序用来实现ESP8266的WIFI连接                */
                /*              开发板用的是魔女开发板F407VET6                 */   
 
#define ALIYUN_productKey      "k1mg4Nbe8hu"													//密码标识
#define ALIYUN_deviceName      "BootLoader"														//设备名
#define ALIYUN_deviceSecret    "3a151aee081e026d074978009496b952"    							//设备密码
#define ALIYUN_password    		"AT+MQTTUSERCFG=0,1,\"NULL\",\"BootLoader&k1mg4Nbe8hu\",\"7198851b3c5adccd4a8586c39e7aabab1637356efdac798419fc36df52591c7d\",0,0,\"\"\r\n"
#define ALIYUN_clientID   		"AT+MQTTCLIENTID=0,\"k1mg4Nbe8hu.BootLoader|securemode=2\\,signmethod=hmacsha256\\,timestamp=1746159079369|\"\r\n"
#define ALIYUN_brokerIP    		"AT+MQTTCONN=0,\"iot-06z00a7mniazx4o.mqtt.iothub.aliyuncs.com\",1883,1\r\n"		//域名									
#define ALIYUN_pubtopic    		"AT+MQTTSUB=0,\"/sys/k1mg4tGDRHB/SmartKitchen/thing/event/property/post\",1\r\n"			        //推送主题
#define ALIYUN_subtopic    		"AT+MQTTSUB=0,\"/sys/k1mg4tGDRHB/SmartKitchen/thing/service/property/set\",1\r\n"			        //订阅主题   

#include "main.h"
#include "stdbool.h"


void ESP8266_Aliyun_Connect(void);
void MQTT_TX_Data(uint16_t temp,uint16_t wet, float Smoke, uint8_t light_value,bool LED);
int Rx_YUN_DataHandle(char *buf);
void Rx_YUN_DataProcess(uint16_t temp, uint16_t wet, float Smoke, uint8_t light_value, bool LED);

#endif

