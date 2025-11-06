#include "test.h"
#define URL "https://ota-cn-shanghai.iot-thing.aliyuncs.com/ota/48f11ace404950715a420a6110c57bb3/cmajb01i100003jafac0d90l1.bin?auth_key=1747395766-a6cd4efc99984ce083680eab890d639b-0-fb5c744d9d1a6208cba0dbb5ddaa2b2b\r\n"
#if 1
int Test_Command(void){

     if(UART3_GetRxNum() > 0) {
         // 处理接收到的数据
         uint8_t *data = UART3_GetRxData();
         uint32_t dataLen = UART3_GetRxNum();
         printf("Received %lu bytes\r\n", dataLen);
         printf("%s\r\n", data);
         UART3_ClearRx();
     }
     return 0;  
}
#endif