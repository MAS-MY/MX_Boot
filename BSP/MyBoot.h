#ifndef __MYBOOT_H
#define __MYBOOT_H

typedef unsigned int 	__be32;
typedef	unsigned char	uint8_t;

#include "main.h"

#define IH_MAGIC	0x27051956	/* Image Magic Number		*/
#define IH_NMLEN	32	/* Image Name Length		*/

/* 定义地址常量 */
#define APP_SECTOR6_ADDR     0x08040000    // 扇区6起始地址
#define APP_SECTOR7_ADDR     0x08060000    // 扇区7起始地址
#define STACK_ADDR_MASK      0x20000000    // SRAM地址掩码

typedef struct image_header {
	__be32		ih_magic;			 /* 魔数，用于验证头 */
	__be32		ih_hcrc;			 /* 头部CRC校验 */
	__be32		ih_time;			 /* 镜像创建时间 */
	__be32		ih_size;			 /* 镜像数据大小 */
	__be32		ih_load;			 /* 加载地址 */
	__be32		ih_ep;				 /* 入口点地址 */
	__be32		ih_dcrc;			 /* 数据CRC校验 */
	uint8_t		ih_os;				 /* 操作系统类型 */
	uint8_t		ih_arch;			 /* CPU架构 */
	uint8_t		ih_type;			 /* 镜像类型 */
	uint8_t		ih_comp;			 /* 压缩类型 */
	uint8_t		ih_name[IH_NMLEN];	 /* 镜像名称 */
} image_header_t;

void relocate_and_start_app(unsigned int pos);
uint8_t Boot_CheckAppValid(uint32_t app_addr);
void Boot_JumpToApp(uint32_t app_addr);
uint8_t Boot_CheckSectorEmpty(uint32_t sector_addr);
void Boot_ManageStart(void);
void Boot_EraseSector6(void);



















#endif
