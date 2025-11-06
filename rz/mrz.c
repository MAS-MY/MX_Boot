#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zmodem.h"
#include "command.h"
#include "main.h"

int
rz(int argc, char *argv[])
{
    bool bps_flag = false;
    uint64_t bps = 0u;
    uint32_t SectorError = 0;

    Boot_EraseSector6();
    HAL_FLASH_Unlock();
    
    size_t bytes = zmodem_receive(NULL, /* use current directory */
                                NULL, /* receive everything */
                                NULL,
                                NULL,
                                bps_flag ? bps : 0,
                                RZSZ_FLAGS_NONE);
    
    // 添加这行确保缓冲区数据写入Flash
    flush_buffer();
    
    HAL_FLASH_Lock();
    printf("Received %zu bytes to Flash.\r\n", bytes);
    return 0;
}

struct command rz_cmd = {
	"rz",
		
	"receive file through z modem\r\n",
	
	"Usage: \r\n"
	"       rz <flash address>\r\n",

	rz,
};




