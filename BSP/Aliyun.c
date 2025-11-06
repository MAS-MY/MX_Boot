#include "main.h"
#include "Aliyun.h"
#include "cJSON.h"
#include "stdlib.h"
#include "string.h"
#include "bsp_ESP8266.h"
#include <stdio.h>
/******************************************************************************
 * 鍑?  鏁帮細 ESP8266_Aliyun_Connect
 * 鍔?  鑳斤細 用来连接阿里云
 ******************************************************************************/
volatile int Model_Data[5] = {0};
#if 1
void ESP8266_Aliyun_Connect(void)
{
    //MQTT名称，MQTT密码
    ESP8266_SendAT(ALIYUN_password, "OK", 2000) ? \
    printf("MQTT Username, MQTT Password configuration successful\r\n\r\n") : \
    printf("MQTT Username, MQTT Password configuration failed\r\n");
    HAL_Delay(3000);

    //MQTTID
    ESP8266_SendAT(ALIYUN_clientID, "OK", 2000) ? \
    printf("MQTT Client ID configuration successful\r\n") : \
    printf("MQTT Client ID configuration failed\r\n");
    HAL_Delay(2000);

    //MQTT域名
    ESP8266_SendAT(ALIYUN_brokerIP, "OK", 2000) ? \
    printf("MQTT Broker Address configuration successful\r\n") : \
    printf("MQTT Broker Address configuration failed\r\n");
    HAL_Delay(2000);
	
	printf("------    Connect Success    ------\r\n");
//    //订阅
//    ESP8266_SendAT(ALIYUN_pubtopic, "OK", 3000) ? \
//    printf("订阅成功1\r\n") : \
//    printf("订阅失败1\r\n");
//    HAL_Delay(2000);
//    ESP8266_SendAT(ALIYUN_subtopic, "OK", 3000) ? \
//    printf("订阅成功2\r\n") : \
//    printf("订阅失败2\r\n");
//    HAL_Delay(2000);
}
#endif

void MQTT_TX_Data(uint16_t temp, uint16_t wet, float Smoke, uint8_t light_value, bool LED)
{
    //发送数据
    char buf[512] = {0};

    sprintf(buf, "AT+MQTTPUB=0,\"/sys/k1mg4tGDRHB/SmartKitchen/thing/event/property/post\",\"\
                 {\\\"params\\\":{\\\"temperature\\\":%d\\,\
								  \\\"Humidity\\\":%d\\,\
								  \\\"GasConcentration\\\":%f\\,\
								  \\\"LightLux\\\":%d\\,\
								  \\\"LightSwitch\\\":%d}}\",1,0\r\n", temp, wet, Smoke, light_value, LED);
    ESP8266_SendAT(buf, "OK", 3000) ? \
    printf("send success\r\n") : \
    printf("send fail\r\n");
}
#if 0
void Parse_Str1(char *buf)
{
    char str1[] = "{\"name\":\"Andy\",\"age\":20}";
    cJSON *str1_json, *str1_name, *str1_age;
    printf("str1:%s\n\n", str1);
    str1_json = cJSON_Parse(str1);   //创建JSON解析对象，返回JSON格式是否正确
    if (!str1_json)
    {
        printf("JSON格式错误:%s\n\n", cJSON_GetErrorPtr()); //输出json格式错误信息
    }
    else
    {
        printf("JSON格式正确:\n%s\n\n", cJSON_Print(str1_json));
        str1_name = cJSON_GetObjectItem(str1_json, "name"); //获取name键对应的值的信息
        if (str1_name->type == cJSON_String)
        {
            printf("姓名:%s\r\n", str1_name->valuestring);
        }
        str1_age = cJSON_GetObjectItem(str1_json, "age");   //获取age键对应的值的信息
        if (str1_age->type == cJSON_Number)
        {
            printf("年龄:%d\r\n", str1_age->valueint);
        }
        cJSON_Delete(str1_json);//释放内存
    }
}

void Rx_YUN_DataProcess(uint16_t temp, uint16_t wet, float Smoke, uint8_t light_value, bool LED){
		if(LED == 0) HAL_GPIO_WritePin(BLUE_LED_GPIO_Port,BLUE_LED_Pin,GPIO_PIN_SET);
		if(LED == 1) HAL_GPIO_WritePin(BLUE_LED_GPIO_Port,BLUE_LED_Pin,GPIO_PIN_RESET);
}

int Rx_YUN_DataHandle(char *buf)
{

    char *raw_str = buf;
    char *params_start = strstr(raw_str, "\"params\":");
    if (params_start)
    {
        params_start += strlen("\"params\":"); // 定位到'{'起始位置
        char *params_end = strstr(params_start, "},\"version\""); // 查找结束标记
        if (params_end)
        {
            size_t params_len = params_end - params_start + 1; // 包含'}'
            char *params_str = (char*)malloc(params_len + 1);
            strncpy(params_str, params_start, params_len);
            params_str[params_len] = '\0';
            printf("Direct extraction:\n%s\n", params_str);

            cJSON *root = cJSON_Parse(params_str);
            if (root == NULL)
            {
                const char *error_ptr = cJSON_GetErrorPtr();
                if (error_ptr)
                {
                    printf("JSON解析错误: %s\n", error_ptr);
                }
                return -1;
            }

            // 2. 提取每个字段的值（假设所有值都是整数）
            int light_switch = -1, light_lux = -1, humidity = -1, temperature = -1, gas_concentration = -1;

            // 提取LightSwitch
            cJSON *item = cJSON_GetObjectItem(root, "LightSwitch");
            if (item->type == cJSON_Number)
            {
                light_switch = item->valueint;
            }
            else
            {
                printf("LightSwitch字段缺失或类型错误\n");
            }

            // 提取LightLux
            item = cJSON_GetObjectItem(root, "LightLux");
            if (item->type == cJSON_Number)
            {
                light_lux = item->valueint;
			}
            else
            {
                printf("LightLux字段缺失或类型错误\n");
            }

            // 提取Humidity
            item = cJSON_GetObjectItem(root, "Humidity");
            if (item->type == cJSON_Number)
            {
                humidity = item->valueint;
            }
            else
            {
                printf("Humidity字段缺失或类型错误\n");
            }

            // 提取temperature（注意键名区分大小写）
            item = cJSON_GetObjectItem(root, "temperature");
            if (item->type == cJSON_Number)
            {
                temperature = item->valueint;
            }
            else
            {
                printf("temperature字段缺失或类型错误\n");
            }

            // 提取GasConcentration
            item = cJSON_GetObjectItem(root, "GasConcentration");
            if (item->type == cJSON_Number)
            {
                gas_concentration = item->valueint;
            }
            else
            {
                printf("GasConcentration字段缺失或类型错误\n");
            }

            // 3. 输出结果
            printf("LightSwitch: %d\n", light_switch);
            printf("LightLux: %d\n", light_lux);
            printf("Humidity: %d\n", humidity);
            printf("Temperature: %d\n", temperature);
            printf("GasConcentration: %d\n", gas_concentration);
			
			Model_Data[0] = light_switch;
			Rx_YUN_DataProcess(temperature,humidity,gas_concentration,light_lux,light_switch);
			// 4. 释放资源
            free(params_str);
            cJSON_Delete(root);
		}
	}
    return 0;
}
#endif










