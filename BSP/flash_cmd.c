/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 * 版权所有 (c) 2006-2021, RT-Thread 开发团队
 *
 * SPDX-License-Identifier: Apache-2.0
 * 许可证: Apache-2.0
 *
 * Change Logs: 变更日志:
 * Date           Author       Notes
 * 日期           作者         说明
 * 2006-04-30     Bernard      the first version for FinSH (FinSH的第一个版本)
 * 2006-05-08     Bernard      change finsh thread stack to 2048 (将finsh线程栈改为2048)
 * 2006-06-03     Bernard      add support for skyeye (添加对skyeye的支持)
 * 2006-09-24     Bernard      remove the code related with hardware (移除与硬件相关的代码)
 * 2010-01-18     Bernard      fix down then up key bug. (修复向下然后向上键的bug)
 * 2010-03-19     Bernard      fix backspace issue and fix device read in shell. (修复退格问题和shell中的设备读取)
 * 2010-04-01     Bernard      add prompt output when start and remove the empty history (启动时添加提示输出并删除空历史记录)
 * 2011-02-23     Bernard      fix variable section end issue of finsh shell (修复finsh shell的变量段结束问题)
 *                             initialization when use GNU GCC compiler. (使用GNU GCC编译器时的初始化)
 * 2016-11-26     armink       add password authentication (添加密码认证)
 * 2018-07-02     aozima       add custom prompt support. (添加自定义提示支持)
 */

#include "command.h"
#include "usart.h"
#include "string.h"
#include "flash_cmd.h"

/* 声明flash操作函数 (Declare flash operation functions) */
static unsigned int stm32_flash_read(unsigned char *buf, unsigned int offset, unsigned int size);
static unsigned int stm32_flash_write(unsigned char *buf, unsigned int offset, unsigned int size);
static unsigned int stm32_flash_erase(unsigned int offset, unsigned int size);

/* 定义STM32 flash操作结构体 (Define STM32 flash operations structure) */
static struct flash_ops stm32_flash = {
    "stm32_flash",              /* 设备名称 (device name) */
    stm32_flash_read,           /* 读取函数 (read function) */
    stm32_flash_write,          /* 写入函数 (write function) */
    stm32_flash_erase,          /* 擦除函数 (erase function) */
};

/* 获取flash操作结构体 (Get flash operations structure) */
struct flash_ops *get_flash(void)
{
    return &stm32_flash;
}

/* flash命令处理函数 (Flash command handler function) */
static int flash_f(int argc, char **argv)
{
    struct flash_ops *fp = get_flash();
    unsigned char *buf;
    unsigned int offset;
    unsigned int size = 0;
    unsigned int ret = 0;

    /* 检查参数数量 (Check argument count) */
    if (argc < 2)
    {
        printf("Error: too few arguments\r\n");  // 错误：参数太少
        return -1;
    }

    /* 处理读取命令 (Handle read command) */
    if (!strcmp(argv[1], "read"))
    {
        if (argc != 5)
        {
            printf("Error: flash read requires <ram_addr> <flash_addr> <size> parameters\r\n");  // 错误：flash read需要<ram地址><flash地址><大小>参数
            return -1;
        }
        buf    = (unsigned char *)str2hex(argv[2]);  // 转换RAM地址
        offset = str2hex(argv[3]);                  // 转换flash地址
        size   = str2hex(argv[4]);                  // 转换大小
        ret = fp->read(buf, offset, size);          // 执行读取操作
        printf("Read %u bytes from flash: 0x%08X to ram: 0x%08X\r\n", ret, offset, (unsigned int)buf);  // 从flash读取了多少字节到RAM
    }
    /* 处理写入命令 (Handle write command) */
    else if (!strcmp(argv[1], "write"))
    {
        if (argc != 5)
        {
            printf("Error: flash write requires <ram_addr> <flash_addr> <size> parameters\r\n");  // 错误：flash write需要<ram地址><flash地址><大小>参数
            return -1;
        }
        buf    = (unsigned char *)str2hex(argv[2]);  // 转换RAM地址
        offset = str2hex(argv[3]);                  // 转换flash地址
        size   = str2hex(argv[4]);                  // 转换大小
        ret = fp->write(buf, offset, size);         // 执行写入操作
        printf("Wrote %u bytes from ram: 0x%08X to flash: 0x%08X\r\n", ret, (unsigned int)buf, offset);  // 从RAM写入了多少字节到flash
    }
    /* 处理擦除命令 (Handle erase command) */
    else if (!strcmp(argv[1], "erase"))
    {
        if (argc != 4)
        {
            printf("Error: flash erase requires <flash_addr> <size> parameters\r\n");  // 错误：flash erase需要<flash地址><大小>参数
            return -1;
        }
        offset = str2hex(argv[2]);                  // 转换flash地址
        size   = str2hex(argv[3]);                  // 转换大小
        ret = fp->erase(offset, size);             // 执行擦除操作
        printf("Erased %u bytes from flash: 0x%08X\r\n", ret, offset);  // 从flash擦除了多少字节
    }
    /* 处理未知命令 (Handle unknown command) */
    else
    {
        printf("Error: unknown subcommand '%s'\r\n", argv[1]);  // 错误：未知子命令
        printf("Available subcommands: read, write, erase\r\n");  // 可用子命令: read, write, erase
        return -1;
    }

    /* 检查操作结果 (Check operation result) */
    if (ret != size)
    {
        printf("Operation failed: expected to process %u bytes, actually processed %u bytes\r\n", size, ret);  // 操作失败：预期处理字节数与实际处理字节数不符
        return -1;
    }

    return 0;
}

/* 定义flash命令结构体 (Define flash command structure) */
struct command flash_cmd = {
    "flash",                   /* 命令名称 (command name) */
    
    "read/write/erase flash\r\n",  /* 简短帮助信息 (brief help) */
    
    "Usage: \r\n"              /* 用法说明 (usage description) */
    "       flash read <flash_addr> <flash_addr> <size>\r\n"
    "       flash write <flash_addr> <flash_addr> <size>\r\n"
    "       flash erase <flash_addr> <size>\r\n",

    flash_f,                   /* 命令处理函数 (command handler) */
};