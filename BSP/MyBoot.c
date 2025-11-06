#include "Myboot.h"
#include <stdio.h>
#include "main.h"
#include <string.h>

extern UART_HandleTypeDef huart1;


const unsigned char hex_tab[] = {'0','1','2','3','4','5','6','7',
                                 '8','9','A','B','C','D','E','F'}; // 大写字母

void puthex(unsigned int val)
{
    printf("0x");
    int started = 0;  // 标记是否已遇到第一个非零字符

    // 处理 32 位值的 8 个半字节（nibble）
    for (int i = 7; i >= 0; i--) {
        int nibble = (val >> (i * 4)) & 0xF;
        
        // 当遇到第一个非零字符后，后续所有字符都要输出
        if (nibble != 0 || started || i == 0) {
            putchar(hex_tab[nibble]);
            started = 1;
        }
    }
}


unsigned int be32_to_cpu(unsigned int x)
{
	unsigned char *p = (unsigned char *)&x;
	unsigned int le;
	le = (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + (p[3]);
	return le;
}

extern void start_app(unsigned int new_vector);

void delay(int d)
{
	while(d--);
}

void copy_app(int *from, int *to, int len)
{
	// 从哪里到哪里, 多长 ?
	int i;
	for (i = 0; i < len/4+1; i++)
	{
		to[i] = from[i];
	}
}

void relocate_and_start_app(unsigned int pos)
{
	image_header_t *head;
	unsigned int load;
	unsigned int size;
	unsigned int new_pos = pos+sizeof(image_header_t);
	uint32_t SectorError = 0;
	/* 读出头部 */
	head = (image_header_t *)pos;
	
	/* 解析头部 */
	load = be32_to_cpu(head->ih_load);
	size = be32_to_cpu(head->ih_size);
	

	printf("load = ");
	puthex(load);
	printf("\r\n");

	printf("size = ");
	puthex(size);
	printf("\r\n");
	
	printf("new_pos = 0x%x \r\n",new_pos);
	
	

    /* 解锁Flash */
    HAL_FLASH_Unlock();
    
    /* 擦除Sector 7 */
    FLASH_EraseInitTypeDef EraseInitStruct = {0};
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.Sector = FLASH_SECTOR_7;  // 0x08060000在Sector 7
    EraseInitStruct.NbSectors = 1;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    
    if(HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK)
    {
        printf("Erase error\r\n");
        HAL_FLASH_Lock();
        return;
    }
    
    /* 按字写入程序 */
    for(uint32_t i = 0; i < (size + 3) / 4; i++)
    {
        uint32_t data = ((uint32_t *)new_pos)[i];
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, 
                           load + (i * 4), 
                           data) != HAL_OK)
        {
            printf("Program error\r\n");
            HAL_FLASH_Lock();
            return;
        }
    }
    
    /* 锁定Flash */
    HAL_FLASH_Lock();
    
    /* 验证写入的数据 */
    if(memcmp((void *)load, (void *)new_pos, size) != 0)
    {
        printf("Verify error\r\n");
        return;
    }
    
    printf("Copy success!\r\n");

    Boot_EraseSector6();
    /* 检查向量表有效性 */
    uint32_t *vector_table = (uint32_t *)load;
    uint32_t stack_addr = vector_table[0];    // 获取栈顶地址
    uint32_t reset_handler = vector_table[1];  // 获取复位向量

    SCB->VTOR = load;  // 明确设置VTOR为0x08060000
	
    printf("Boot Running at address: 0x%08X\r\n", SCB->VTOR);
    printf("Stack address: 0x%08X\r\n", stack_addr);
    printf("Reset vector: 0x%08X\r\n", reset_handler);    
    
    /* 验证地址的有效性 */
    if((stack_addr & 0x20000000) != 0x20000000) {
        printf("Invalid stack address!\r\n");
        return;
    }
    
    if((reset_handler & 0x08060000) != 0x08060000) {
        printf("Invalid reset vector!\r\n");
        return;
    }
	
	printf("Source address: 0x%08X\r\n", new_pos);
    printf("Target address: 0x%08X\r\n", load);
    printf("Stack address: 0x%08X\r\n", stack_addr);
    printf("Reset vector: 0x%08X\r\n", reset_handler);
    printf("Current VTOR: 0x%08X\r\n", SCB->VTOR);
	

    /* 设置栈指针 */
    __set_MSP(stack_addr);

    /* 定义跳转函数 */
    start_app(load); 
}
// 添加扇区擦除函数
void Boot_EraseSector6(void)
{
    FLASH_EraseInitTypeDef EraseInitStruct = {0};
    uint32_t SectorError = 0;
    uint32_t *sector6 = (uint32_t*)APP_SECTOR6_ADDR; // 0x08040000
    uint32_t preserved_header[16];                  // 保存前64字节

    // 1. 先把前64字（16*4）复制到RAM
    for (int i = 0; i < 16; ++i) {
        preserved_header[i] = sector6[i];
    }

    /* 解锁Flash */
    HAL_FLASH_Unlock();

    /* 配置擦除参数 */
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.Sector = FLASH_SECTOR_6;  // 0x08040000在Sector 6
    EraseInitStruct.NbSectors = 1;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    /* 执行擦除 */
    if(HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK)
    {
        printf("Sector 6 erase error!\r\n");
        HAL_FLASH_Lock();
        return;
    }

    // 2. 将先前缓存的64字节写回（保持前64字节保留）
    for (int i = 0; i < 16; ++i) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                              APP_SECTOR6_ADDR + i*4,
                              preserved_header[i]) != HAL_OK) {
            printf("Rewrite preserved header word %d failed!\r\n", i);
            break;
        }
    }

    printf("Sector 6 erased (header 64B preserved).\r\n");

    /* 锁定Flash */
    HAL_FLASH_Lock();
}
/**
 * @brief  检查应用程序是否有效
 * @param  app_addr: 应用程序地址
 * @retval 0: 无效, 1: 有效
 */
uint8_t Boot_CheckAppValid(uint32_t app_addr)
{
    uint32_t *vector_table = (uint32_t *)app_addr;
    uint32_t stack_addr = vector_table[0];
    
    return ((stack_addr & STACK_ADDR_MASK) == STACK_ADDR_MASK);
}

/**
 * @brief  检查扇区是否为空
 * @param  sector_addr: 扇区地址
 * @retval 0: 非空, 1: 空
 */
uint8_t Boot_CheckSectorEmpty(uint32_t sector_addr)
{
    uint32_t *addr = (uint32_t *)sector_addr;

    for(int i = 16; i < 32; i++)
    {
        if(addr[i] != 0xFFFFFFFF)
        {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief  跳转到应用程序
 * @param  app_addr: 应用程序地址
 */
void Boot_JumpToApp(uint32_t app_addr)
{
    uint32_t *vector_table = (uint32_t *)app_addr;
    uint32_t stack_addr = vector_table[0];
    
    printf("Jumping to address: 0x%08X\r\n", app_addr);
    while(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET);
    
    /* 设置向量表 */
    SCB->VTOR = app_addr;
    __DSB();
    __ISB();
    
    /* 设置栈指针并跳转 */
    __set_MSP(stack_addr);
    void (*app_reset_handler)(void) = (void*)vector_table[1];
    app_reset_handler();
}

/**
 * @brief  启动管理入口函数
 */
void Boot_ManageStart(void)
{
    /* 检查扇区6是否为空 */
    if(Boot_CheckSectorEmpty(APP_SECTOR6_ADDR))
    {
        printf("Sector 6 is empty\r\n");
        /* 检查扇区7是否有有效程序 */
        if(Boot_CheckAppValid(APP_SECTOR7_ADDR))
        {
            printf("Found valid app in Sector 7\r\n");
            Boot_JumpToApp(APP_SECTOR7_ADDR);
        }
    }
    else
    {
        printf("Found app in Sector 6\r\n");
		
        relocate_and_start_app(APP_SECTOR6_ADDR);
    }
}

