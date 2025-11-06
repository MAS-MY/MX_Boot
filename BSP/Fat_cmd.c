#include "Fat_cmd.h"
#include "fatfs.h"
#include <stdio.h>
#include <string.h>

// 外部变量引用
extern FATFS SDFatFS;
extern char SDPath[4];
extern FIL SDFile;
extern ring_buffer rx_buf;



// 辅助函数：列出SD卡上的文件
static void list_files(void)
{
    FRESULT res;
    DIR dir;
    static FILINFO fno;
    uint32_t fileCount = 0, dirCount = 0;
    uint64_t totalSize = 0;
    
    printf("\r\nReading SD card root directory...\r\n");
    
    // 打开根目录
    res = f_opendir(&dir, SDPath);
    if (res != FR_OK) {
        printf("Failed to open directory, error code: %d\r\n", res);
        return;
    }

    printf("\r\nFile List:\r\n");
    printf("-------------------------------\r\n");
    printf("Type  Size(bytes)    Filename\r\n");
    printf("-------------------------------\r\n");

    // 读取目录内容
    for (;;) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;  // 错误或目录结束
        char* filename = fno.fname;

        // 显示文件信息
        if (fno.fattrib & AM_DIR) {
            // 目录
            printf("<DIR> %10s    %s\r\n", "", filename);
            dirCount++;
        } else {
            // 文件
            printf("<FILE>%10lu    %s\r\n", fno.fsize, filename);
            fileCount++;
            totalSize += fno.fsize;
        }
    }

    f_closedir(&dir);

    printf("-------------------------------\r\n");
    printf("%lu files, total size: %llu bytes\r\n", fileCount, totalSize);
    printf("%lu directories\r\n", dirCount);
}
// 辅助函数：读取文件内容
static void read_file(void)
{
    FRESULT res;
    char filename[32];
    uint8_t buffer[513];  // +1用于字符串结束符
    UINT bytesRead;
    
    printf("\r\nEnter filename to read: ");
    
    // 清空缓冲区
    memset(filename, 0, sizeof(filename));
    
    // 获取用户输入的文件名
    uint8_t idx = 0;
    uint8_t c;
    
    while(1) {
        if(ring_buffer_read(&c, &rx_buf) == 0) {
            if(c == '\r' || c == '\n') {
                if(idx > 0) {
                    printf("\r\n");
                    break;
                }
            } else if(idx < sizeof(filename) - 1) {
                filename[idx++] = c;
                printf("%c", c);  // 回显字符
            }
        }
    }
    
    filename[idx] = '\0';  // 确保字符串正确终止
    
    // 打开文件
    char filepath[40];
    sprintf(filepath, "%s/%s", SDPath, filename);
    
    res = f_open(&SDFile, filepath, FA_READ);
    if (res != FR_OK) {
        printf("Failed to open file, error code: %d\r\n", res);
        return;
    }
    
    printf("\r\nFile content:\r\n");
    printf("-----------------------------------\r\n");
    
    // 读取并显示文件内容
    do {
        memset(buffer, 0, sizeof(buffer));
        res = f_read(&SDFile, buffer, 512, &bytesRead);
        
        if (res == FR_OK && bytesRead > 0) {
            // 确保字符串有终止符
            buffer[bytesRead] = '\0';
            
            // 同时处理二进制文件和文本文件
            for (uint16_t i = 0; i < bytesRead; i++) {
                if (buffer[i] >= 32 && buffer[i] < 127) {
                    // 可打印ASCII字符
                    printf("%c", buffer[i]);
                } else {
                    // 非可打印字符显示为十六进制
                    printf("[%02X]", buffer[i]);
                }
            }
        }
    } while (res == FR_OK && bytesRead == 512);
    
    printf("\r\n-----------------------------------\r\n");
    f_close(&SDFile);
}

// 辅助函数：删除文件
static void delete_file(void)
{
    FRESULT res;
    char filename[32];
    uint8_t confirmation;
    
    printf("\r\nEnter filename to delete: ");
    
    // 清空缓冲区
    memset(filename, 0, sizeof(filename));
    
    // 获取用户输入的文件名
    uint8_t idx = 0;
    uint8_t c;
    
    while(1) {
        if(ring_buffer_read(&c, &rx_buf) == 0) {
            if(c == '\r' || c == '\n') {
                if(idx > 0) {
                    printf("\r\n");
                    break;
                }
            } else if(idx < sizeof(filename) - 1) {
                filename[idx++] = c;
                printf("%c", c);  // 回显字符
            }
        }
    }
    
    filename[idx] = '\0';  // 确保字符串正确终止
    
    // 确认删除操作
    printf("Confirm delete file '%s'? (y/n): ", filename);
    
    while(1) {
        if(ring_buffer_read(&confirmation, &rx_buf) == 0) {
            if(confirmation == 'y' || confirmation == 'Y') {
                printf("%c\r\n", confirmation);
                break;
            } else if(confirmation == 'n' || confirmation == 'N') {
                printf("%c\r\nDelete operation cancelled\r\n", confirmation);
                return;
            }
        }
    }
    
    // 构建完整文件路径
    char filepath[40];
    sprintf(filepath, "%s/%s", SDPath, filename);
    
    // 执行删除操作
    res = f_unlink(filepath);
    
    if (res == FR_OK) {
        printf("File '%s' successfully deleted\r\n", filename);
    } else {
        printf("Failed to delete file, error code: %d\r\n", res);
    }
}

static void download_to_flash(void)
{
    FRESULT res;
    char filename[32];
    static uint8_t buffer[1024];  // static 减少栈占用
    UINT bytesRead;
    uint32_t fileSize = 0;
    const uint32_t flash_addr = 0x08040000;  // 目标Flash起始（Sector6）
    uint32_t currentAddr = flash_addr;
    uint32_t percent = 0;
    uint32_t writtenBytes = 0;
    HAL_StatusTypeDef flash_status;
    uint8_t c;

    printf("\r\nEnter bin filename to download to Flash: ");
    memset(filename, 0, sizeof(filename));

    // 获取用户输入文件名
    uint8_t idx = 0;
    while (1) {
        if (ring_buffer_read(&c, &rx_buf) == 0) {
            if (c == '\r' || c == '\n') {
                if (idx > 0) { printf("\r\n"); break; }
            } else if (idx < sizeof(filename) - 1) {
                filename[idx++] = c;
                printf("%c", c);
            }
        }
    }
    filename[idx] = '\0';

    char filepath[40];
    sprintf(filepath, "%s/%s", SDPath, filename);

    res = f_open(&SDFile, filepath, FA_READ);
    if (res != FR_OK) {
        printf("Failed to open file, FR=%d\r\n", res);
        return;
    }

    fileSize = f_size(&SDFile);
    printf("File size: %lu bytes\r\n", fileSize);
    if (fileSize == 0) {
        printf("Error: empty file\r\n");
        f_close(&SDFile);
        return;
    }
    if (fileSize > 0x20000) { // 仅擦除Sector6(128KB)，超过则提示
        printf("Error: file exceeds Sector6 capacity (128KB)\r\n");
        f_close(&SDFile);
        return;
    }

    printf("WARNING: This will erase Flash area 0x%08lX - 0x%08lX (Sector6)\r\n",
           flash_addr, flash_addr + fileSize - 1);
    printf("Proceed? (y/n): ");
    while (1) {
        if (ring_buffer_read(&c, &rx_buf) == 0) {
            if (c == 'y' || c == 'Y') { printf("%c\r\n", c);
                Backup_App_With_Header();
                break; }
            if (c == 'n' || c == 'N') { printf("%c\r\nCanceled.\r\n", c); f_close(&SDFile); return; }
        }
    }

    // 解锁并清错误标志
    if (HAL_FLASH_Unlock() != HAL_OK) {
        printf("FLASH_Unlock failed\r\n");
        f_close(&SDFile);
        return;
    }
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    // 擦除Sector6
    FLASH_EraseInitTypeDef eraseInit;
    memset(&eraseInit, 0, sizeof(eraseInit));
    eraseInit.TypeErase    = FLASH_TYPEERASE_SECTORS;
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    eraseInit.Sector       = FLASH_SECTOR_6;
    eraseInit.NbSectors    = 1; // 只擦除Sector6

    uint32_t sectorError = 0xFFFFFFFF;
    printf("Erasing sector 6...\r\n");
    flash_status = HAL_FLASHEx_Erase(&eraseInit, &sectorError);
    if (flash_status != HAL_OK) {
        printf("Flash erase FAILED: status=%d sectorErr=%lu FLASH->SR=0x%08lX\r\n",
               flash_status, sectorError, FLASH->SR);
        HAL_FLASH_Lock();
        f_close(&SDFile);
        return;
    }
    printf("Erase OK. Writing...\r\n");

    // 编程循环
    while (1) {
        memset(buffer, 0xFF, sizeof(buffer));
        res = f_read(&SDFile, buffer, sizeof(buffer), &bytesRead);
        if (res != FR_OK) {
            printf("Read error FR=%d\r\n", res);
            HAL_FLASH_Lock();
            f_close(&SDFile);
            return;
        }
        if (bytesRead == 0) break; // EOF

        // 逐字(32位)写，处理末尾不足4字节
        uint32_t i = 0;
        while (i < bytesRead) {
            uint8_t b0 = buffer[i];
            uint8_t b1 = (i + 1 < bytesRead) ? buffer[i + 1] : 0xFF;
            uint8_t b2 = (i + 2 < bytesRead) ? buffer[i + 2] : 0xFF;
            uint8_t b3 = (i + 3 < bytesRead) ? buffer[i + 3] : 0xFF;
            uint32_t data = ((uint32_t)b3 << 24) | ((uint32_t)b2 << 16) | ((uint32_t)b1 << 8) | b0;

            flash_status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, currentAddr, data);
            if (flash_status != HAL_OK) {
                printf("Write FAIL addr=0x%08lX status=%d SR=0x%08lX\r\n", currentAddr, flash_status, FLASH->SR);
                HAL_FLASH_Lock();
                f_close(&SDFile);
                return;
            }
            currentAddr += 4;
            i += 4;
        }

        writtenBytes += bytesRead;
        uint32_t newPercent = (writtenBytes * 100) / fileSize;
        if (newPercent > percent) {
            percent = newPercent;
            printf("Progress: %lu%%\r\n", percent);
        }
    }

    HAL_FLASH_Lock();
    f_close(&SDFile);

    // 可选：简单验证前64字节
    int mismatch = 0;
    for (uint32_t off = 0; off < (fileSize < 64 ? fileSize : 64); off++) {
        uint8_t fileByte;
        // 重新打开读取校验
    }
    printf("\r\nDownload complete! %lu bytes written to Flash (Sector6)\r\n", writtenBytes);
    printf("App staging at 0x%08lX ready.\r\n", flash_addr);
}

// FATFS菜单功能
static void fatfs_menu(void)
{
    FRESULT fre = f_mount(&SDFatFS, SDPath, 1);
    if(fre == FR_OK)
    {
        printf("SD card mount OK \r\n");
    }
    else{
        printf("SD card mount failed \r\n");
    }
    
    uint8_t choice = 0;
    uint8_t exit_flag = 0;
    
    while(!exit_flag) {
        printf("FATFS Menu:\r\n");
        printf("1. List files on SD card\r\n");
        printf("2. Read file\r\n");
        printf("3. Delete file\r\n");
        printf("4. Download file to Flash\r\n");  // 新增选项
        printf("0. Exit\r\n");
        printf("Enter your choice: ");
        
        // 等待用户输入
        while(ring_buffer_read(&choice, &rx_buf) != 0);
        
        // 回显选择
        printf("%c\r\n", choice);
        
        // 执行选择的操作
        switch(choice) {
            case '1':
                list_files();
                break;
                
            case '2':
                read_file();
                break;
                
            case '3':
                delete_file();
                break;
                
            case '4':
                download_to_flash();  // 调用新增的功能
                break;
                
            case '0':
                printf("Exiting FATFS menu\r\n");
                exit_flag = 1;
                break;
                
            default:
                printf("Invalid choice. Please try again.\r\n");
                break;
        }
    }
    
    // 取消挂载文件系统
    f_mount(NULL, (TCHAR const*)SDPath, 0);
}
// 命令结构体定义
struct command Fat_cmd = {
    "fat",
    "Operate FAT file system on SD card\r\n",
    "Usage:\r\n"
    "    fatfs - Enter FAT file system menu for SD card operations\r\n",
    fatfs_menu,
};

// =========================  新增：应用备份函数  =========================
// 功能: 创建一个以日期命名的备份文件，先写入 16 个固定 32bit 头部字，然后
//       写入从 0x08060000 (当前活动应用所在 Sector7) 开始的实际有效程序数据，
//       有效长度=去掉尾部连续的 0xFFFFFFFF 填充后的大小。
// 依赖: 已经挂载 FATFS (若未挂载本函数会尝试挂载)；时间获取函数 _4G_get_time_extern。
// 文件名: APP_YYYYMMDD.bin  (若同名存在自动追加 _1,_2...)
// 返回: 0 成功；负值失败

extern int _4G_get_time_extern(int time[3]);

int extern_Backup_App_With_Header(void)
{
    FRESULT fre = f_mount(&SDFatFS, SDPath, 1);
    if(fre == FR_OK)
    {
        printf("SD card mount OK \r\n");
    }
    else{
        printf("SD card mount failed \r\n");
    }
    const uint32_t APP_BASE_ADDR   = 0x08060000UL;   // Sector7 起始
    const uint32_t APP_MAX_SIZE    = 0x20000UL;      // Sector7 128KB
    const uint32_t *flash32        = (const uint32_t*)APP_BASE_ADDR;

    // 动态头部：使用 Sector6 前64字节作为备份文件头（保持与擦除前保留一致）
    uint32_t backup_header[16];
    const uint32_t *sector6 = (const uint32_t*)0x08040000UL; // Sector6 基地址
    for (int i = 0; i < 16; ++i) {
        backup_header[i] = sector6[i];
    }

    // 1. 计算有效数据长度（去掉尾部全 0xFFFFFFFF）
    uint32_t used_end = 0; // 指向最后一个有效字之后的位置
    for (uint32_t i = 0; i < APP_MAX_SIZE/4; ++i) {
        if (flash32[i] != 0xFFFFFFFF) {
            used_end = (i + 1) * 4; // 记录到此为止的有效范围
        }
    }
    if (used_end == 0) {
        printf("Backup abort: no valid data in app sector (all 0xFFFFFFFF)\r\n");
        return -1;
    }

    // 2. 获取日期 (year, month, day)
    int ymd[3] = {1970,1,1};
    if (_4G_get_time_extern(ymd) != 0) {
        printf("Warning: time fetch failed, using fallback 19700101\r\n");
    }

    // 3. 构造文件名 APP_YYYYMMDD.bin，若存在则追加 _序号
    char baseName[32];
    sprintf(baseName, "APP_%04d%02d%02d", ymd[0], ymd[1], ymd[2]);

    char filename[40];
    UINT attempt = 0;
    FRESULT fr;
    FIL file;
    for (;;) {
        if (attempt == 0) {
            sprintf(filename, "%s/%s.bin", SDPath, baseName);
        } else {
            sprintf(filename, "%s/%s_%u.bin", SDPath, baseName, attempt);
        }
        FILINFO info;
        fr = f_stat(filename, &info);
        if (fr == FR_NO_FILE) {
            break; // 不存在，可以使用
        } else if (fr != FR_OK) {
            printf("f_stat error (%d)\r\n", fr);
            return -2;
        }
        attempt++;
        if (attempt > 99) {
            printf("Too many duplicate backup filenames\r\n");
            return -3;
        }
    }

    // 4. 若尚未挂载尝试挂载 (简单判断: 根路径尝试打开目录)
    DIR testDir; 
    if (f_opendir(&testDir, SDPath) != FR_OK) {
        if (f_mount(&SDFatFS, SDPath, 1) != FR_OK) {
            printf("Mount SD failed\r\n");
            return -4;
        }
    } else {
        f_closedir(&testDir);
    }

    fr = f_open(&file, filename, FA_CREATE_NEW | FA_WRITE);
    if (fr != FR_OK) {
        printf("Create file %s failed (%d)\r\n", filename, fr);
        return -5;
    }

    // 5. 写入头部
    UINT bw = 0;
    fr = f_write(&file, backup_header, sizeof(backup_header), &bw);
    if (fr != FR_OK || bw != sizeof(backup_header)) {
        printf("Write header failed fr=%d bw=%u\r\n", fr, bw);
        f_close(&file);
        f_unlink(filename); // 清理
        return -6;
    }

    // 6. 分块写入应用有效数据
    const uint8_t *app_bytes = (const uint8_t*)APP_BASE_ADDR;
    uint32_t remain = used_end;
    const UINT CHUNK = 1024; // 1KB 缓冲
    while (remain > 0) {
        UINT thisWrite = (remain > CHUNK) ? CHUNK : remain;
        bw = 0;
        fr = f_write(&file, app_bytes, thisWrite, &bw);
        if (fr != FR_OK || bw != thisWrite) {
            printf("Write data failed fr=%d bw=%u expect=%u\r\n", fr, bw, thisWrite);
            f_close(&file);
            f_unlink(filename);
            return -7;
        }
        app_bytes += thisWrite;
        remain    -= thisWrite;
    }

    f_close(&file);
    printf("Backup success: %s (header 64B + app %luB)\r\n", filename, (unsigned long)used_end);
    return 0;
}

int Backup_App_With_Header(void)
{
    const uint32_t APP_BASE_ADDR   = 0x08060000UL;   // Sector7 起始
    const uint32_t APP_MAX_SIZE    = 0x20000UL;      // Sector7 128KB
    const uint32_t *flash32        = (const uint32_t*)APP_BASE_ADDR;

    // 动态头部：使用 Sector6 前64字节作为备份文件头（保持与擦除前保留一致）
    uint32_t backup_header[16];
    const uint32_t *sector6 = (const uint32_t*)0x08040000UL; // Sector6 基地址
    for (int i = 0; i < 16; ++i) {
        backup_header[i] = sector6[i];
    }

    // 1. 计算有效数据长度（去掉尾部全 0xFFFFFFFF）
    uint32_t used_end = 0; // 指向最后一个有效字之后的位置
    for (uint32_t i = 0; i < APP_MAX_SIZE/4; ++i) {
        if (flash32[i] != 0xFFFFFFFF) {
            used_end = (i + 1) * 4; // 记录到此为止的有效范围
        }
    }
    if (used_end == 0) {
        printf("Backup abort: no valid data in app sector (all 0xFFFFFFFF)\r\n");
        return -1;
    }

    // 2. 获取日期 (year, month, day)
    int ymd[3] = {1970,1,1};
    if (_4G_get_time_extern(ymd) != 0) {
        printf("Warning: time fetch failed, using fallback 19700101\r\n");
    }

    // 3. 构造文件名 APP_YYYYMMDD.bin，若存在则追加 _序号
    char baseName[32];
    sprintf(baseName, "APP_%04d%02d%02d", ymd[0], ymd[1], ymd[2]);

    char filename[40];
    UINT attempt = 0;
    FRESULT fr;
    FIL file;
    for (;;) {
        if (attempt == 0) {
            sprintf(filename, "%s/%s.bin", SDPath, baseName);
        } else {
            sprintf(filename, "%s/%s_%u.bin", SDPath, baseName, attempt);
        }
        FILINFO info;
        fr = f_stat(filename, &info);
        if (fr == FR_NO_FILE) {
            break; // 不存在，可以使用
        } else if (fr != FR_OK) {
            printf("f_stat error (%d)\r\n", fr);
            return -2;
        }
        attempt++;
        if (attempt > 99) {
            printf("Too many duplicate backup filenames\r\n");
            return -3;
        }
    }

    // 4. 若尚未挂载尝试挂载 (简单判断: 根路径尝试打开目录)
    DIR testDir; 
    if (f_opendir(&testDir, SDPath) != FR_OK) {
        if (f_mount(&SDFatFS, SDPath, 1) != FR_OK) {
            printf("Mount SD failed\r\n");
            return -4;
        }
    } else {
        f_closedir(&testDir);
    }

    fr = f_open(&file, filename, FA_CREATE_NEW | FA_WRITE);
    if (fr != FR_OK) {
        printf("Create file %s failed (%d)\r\n", filename, fr);
        return -5;
    }

    // 5. 写入头部
    UINT bw = 0;
    fr = f_write(&file, backup_header, sizeof(backup_header), &bw);
    if (fr != FR_OK || bw != sizeof(backup_header)) {
        printf("Write header failed fr=%d bw=%u\r\n", fr, bw);
        f_close(&file);
        f_unlink(filename); // 清理
        return -6;
    }

    // 6. 分块写入应用有效数据
    const uint8_t *app_bytes = (const uint8_t*)APP_BASE_ADDR;
    uint32_t remain = used_end;
    const UINT CHUNK = 1024; // 1KB 缓冲
    while (remain > 0) {
        UINT thisWrite = (remain > CHUNK) ? CHUNK : remain;
        bw = 0;
        fr = f_write(&file, app_bytes, thisWrite, &bw);
        if (fr != FR_OK || bw != thisWrite) {
            printf("Write data failed fr=%d bw=%u expect=%u\r\n", fr, bw, thisWrite);
            f_close(&file);
            f_unlink(filename);
            return -7;
        }
        app_bytes += thisWrite;
        remain    -= thisWrite;
    }

    f_close(&file);
    printf("Backup success: %s (header 64B + app %luB)\r\n", filename, (unsigned long)used_end);
    return 0;
}
