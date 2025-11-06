
#include "file.h"
#include "main.h"
static struct stat g_rcv_st;

/* 全局4字节拼字缓冲, 供 fwrite2 / putc2 / flush_buffer 共享 */
static uint8_t  g_flash_word_buf[4];
static uint32_t g_flash_word_cnt = 0;   /* 当前已缓存的字节数 (0-4) */

/* 写一个已拼好的32位字到Flash (addr需4字节对齐) */
static int flash_write_word(uint32_t addr, uint32_t word)
{
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, word) != HAL_OK)
        return -1;
    return 0;
}

int fileno(FILE *stream)
{
	return 123;
}

int fstat(int fd, struct stat *statbuf)
{
	*statbuf = g_rcv_st;
	return 0;
}

FILE *popen(const char *command, const char *type)
{
	return (FILE*)1;
}


int pclose(FILE *stream)
{
	return 0;
}

unsigned int sleep(unsigned int seconds)
{
	return 0;
}

FILE *fopen2(const char *pathname, const char *mode)
{
	g_rcv_st.datas   = (unsigned char *)0x08040000;
	g_rcv_st.offset  = 0;
	g_rcv_st.st_size = 0;
	return (FILE *)1;
}

size_t fwrite2(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    /* 原语义: 写 nmemb 个元素, 每个 size 字节。当前调用场景通常 size==数据长度, nmemb==1 */
    const uint8_t *pdata = (const uint8_t*)ptr;
    size_t total = size * nmemb;
    uint32_t base_addr = (uint32_t)&g_rcv_st.datas[g_rcv_st.offset];

    for (size_t i = 0; i < total; ++i) {
        g_flash_word_buf[g_flash_word_cnt++] = pdata[i];
        if (g_flash_word_cnt == 4) {
            uint32_t word = (uint32_t)g_flash_word_buf[0] |
                            ((uint32_t)g_flash_word_buf[1] << 8) |
                            ((uint32_t)g_flash_word_buf[2] << 16) |
                            ((uint32_t)g_flash_word_buf[3] << 24);
            if (flash_write_word(base_addr, word) != 0) {
                return 0; /* 失败: 返回0表示未写入完整 */
            }
            base_addr += 4;
            g_flash_word_cnt = 0;
        }
        g_rcv_st.offset++; /* 已接收字节数 */
    }
    g_rcv_st.st_size = g_rcv_st.offset;
    /* 按标准 fwrite 语义, 返回成功写入元素个数 (nmemb). 这里仅在成功时返回 nmemb */
    return nmemb;
}

			  
int putc2(int c, FILE *stream)
{
    uint32_t addr = (uint32_t)&g_rcv_st.datas[g_rcv_st.offset & ~0x3UL];
    g_flash_word_buf[g_flash_word_cnt++] = (uint8_t)c;
    if (g_flash_word_cnt == 4) {
        uint32_t word = (uint32_t)g_flash_word_buf[0] |
                        ((uint32_t)g_flash_word_buf[1] << 8) |
                        ((uint32_t)g_flash_word_buf[2] << 16) |
                        ((uint32_t)g_flash_word_buf[3] << 24);
        if (flash_write_word(addr, word) != 0) {
            return EOF;
        }
        g_flash_word_cnt = 0;
    }
    g_rcv_st.offset++;
    g_rcv_st.st_size = g_rcv_st.offset;
    return c;
}
// 添加这个函数来刷新缓冲区
int flush_buffer(void)
{
    if (g_flash_word_cnt > 0) {
        uint32_t addr = (uint32_t)&g_rcv_st.datas[g_rcv_st.offset - g_flash_word_cnt];
        uint32_t word = 0xFFFFFFFFu; /* 先全1，再覆盖有效字节 */
        /* 低地址放低字节 */
        for (uint32_t i = 0; i < g_flash_word_cnt; ++i) {
            word &= ~(0xFFu << (8*i));
            word |= ((uint32_t)g_flash_word_buf[i]) << (8*i);
        }
        if (flash_write_word(addr, word) != 0)
            return -1;
        g_flash_word_cnt = 0;
    }
    return 0;
}
