/***********************************************************************************************************************************
 ** 【代码编写】  魔女开发板团队
 ** 【淘    宝】  魔女开发板
 ** 【购买链接】  https://demoboard.taobao.com
 ***********************************************************************************************************************************
 ** 【文件名称】  bsp_UART.c
 **
 ** 【文件功能】  各UART的GPIO配置、通信协议配置、中断配置，及功能函数实现
 **
 ** 【适用平台】  STM32F407 + keil5 + 标准库\HAL库
 **
************************************************************************************************************************************/
#include "bsp_UART.h"           // 头文件




/*****************************************************************************
 ** 声明本地变量
****************************************************************************/
typedef struct
{
    uint16_t  usRxNum;            // 新一帧数据，接收到多少个字节数据
    uint8_t  *puRxData;           // 新一帧数据，数据缓存; 存放的是空闲中断后，从临时接收缓存复制过来的完整数据，并非接收过程中的不完整数据;

    uint8_t  *puTxFiFoData;       // 发送缓冲区，环形队列; 为了方便理解阅读，没有封装成队列函数
    uint16_t  usTxFiFoData ;      // 环形缓冲区的队头
    uint16_t  usTxFiFoTail ;      // 环形缓冲区的队尾
} xUSATR_TypeDef;





/******************************************************************************
 * 函  数： delay_ms
 * 功  能： ms 延时函数
 * 备  注： 1、系统时钟168MHz
 *          2、打勾：Options/ c++ / One ELF Section per Function
            3、编译优化级别：Level 3(-O3)
 * 参  数： uint32_t  ms  毫秒值
 * 返回值： 无
 ******************************************************************************/
static volatile uint32_t ulTimesMS;    // 使用volatile声明，防止变量被编译器优化
static void delay_ms(uint16_t ms)
{
    ulTimesMS = ms * 16500;
    while (ulTimesMS)
        ulTimesMS--;                   // 操作外部变量，防止空循环被编译器优化掉
}





//////////////////////////////////////////////////////////////   UART-1   ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if UART1_EN

static xUSATR_TypeDef  xUART1 = { 0 };                      // 定义 UART1 的收发结构体
static uint8_t uaUART1RxData[UART1_RX_BUF_SIZE];            // 定义 UART1 的接收缓存
static uint8_t uaUART1TxFiFoData[UART1_TX_BUF_SIZE];        // 定义 UART1 的发送缓存

/******************************************************************************
 * 函  数： UART1_Init
 * 功  能： 初始化USART1的通信引脚、协议参数、中断优先级
 *          引脚：TX-PA10、RX-PA11
 *          协议：波特率-None-8-1
 *          发送：发送中断
 *          接收：接收+空闲中断
 *
 * 参  数： uint32_t  ulBaudrate  通信波特率
 * 返回值： 无
 ******************************************************************************/
void UART1_Init(uint32_t ulBaudrate)
{
    // 使能相关时钟
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;                       // 使能外设：UART1
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;                        // 使能GPIO：GPIOA
    // 关闭串口
    USART1 -> CR1  =   0;                                       // 关闭串口，配置清零

#ifdef USE_STDPERIPH_DRIVER                                     // 标准库 配置
    // 配置TX引脚
    GPIO_InitTypeDef  GPIO_InitStructure = {0};                 // GPIO 初始化结构体
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;                 // 引脚编号：TX_PA9
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;               // 引脚方向: 复用功能
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;              // 输出模式：推挽
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;               // 上下拉：上拉
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;           // 输出速度：50MHz
    GPIO_Init(GPIOA, &GPIO_InitStructure);                      // 初始化：把上述参数，更新到芯片寄存器
    // 配置RX引脚
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;                // 引脚编号：RX_PA10
    GPIO_Init(GPIOA, &GPIO_InitStructure);                      // 初始化：把上述参数，更新到芯片寄存器
    // 配置引脚的具体复用功能
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);   // 配置PA9复用功能： USART1
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);  // 配置PA10复用功能：USART1
    // 中断配置
    NVIC_InitTypeDef  NVIC_InitStructure = {0};                 // 中断优先级配置结构体
    NVIC_InitStructure .NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure .NVIC_IRQChannelPreemptionPriority = 1;  // 抢占优先级
    NVIC_InitStructure .NVIC_IRQChannelSubPriority = 1;         // 子优先级
    NVIC_InitStructure .NVIC_IRQChannelCmd = ENABLE;            // IRQ通道使能
    NVIC_Init(&NVIC_InitStructure);
#endif

#ifdef USE_HAL_DRIVER                                           // HAL库 配置
    // GPIO引脚工作模式配置
    GPIO_InitTypeDef    GPIO_InitStruct = {0};                  // 声明初始化要用到的结构体
    GPIO_InitStruct.Pin   = GPIO_PIN_9 | GPIO_PIN_10;           // 引脚 TX-PA9、RX-PA10
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;                    // 工作模式
    GPIO_InitStruct.Pull  = GPIO_PULLUP;                        // 上下拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;          // 引脚速率
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;                // 引脚复用功能
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);                     // 初始化引脚工作模式
    // 中断优选级配置
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 1);                    // 配置中断线的优先级
    HAL_NVIC_EnableIRQ(USART1_IRQn);                            // 使能中断线
#endif

    // 计算波特率参数
    float    temp;
    uint16_t mantissa, fraction;
    SystemCoreClockUpdate();                                    // 更新系统运行频率全局值; 函数SystemCoreClock( )，在标准库、HAL库通用
    temp = (float)(SystemCoreClock / 2) / (ulBaudrate * 16);    // 波特率公式计算; USART1挂载在APB2, 时钟为系统时钟的2分频; 全局变量SystemCoreClock，在标准库、HAL库通用;
    mantissa = temp;				                            // 整数部分
    fraction = (temp - mantissa) * 16;                          // 小数部分
    USART1 -> BRR  = mantissa << 4 | fraction;                  // 设置波特率
    // 串口通信参数配置
    USART1 -> CR1 |=   0x01 << 2;                               // 接收使能[2]: 0=失能、1=使能
    USART1 -> CR1 |=   0x01 << 3;                               // 发送使能[3]：0=失能、1=使能
    USART1 -> CR1 &= ~(0x01 << 12);                             // 数据位[12]：0=8位、1=9位
    USART1 -> CR2  =   0;                                       // 数据清0
    USART1 -> CR2 &=  ~(0x03 << 12);                            // 停止位[13:12]：00b=1个停止位、01b=0.5个停止位、10b=2个停止位、11b=1.5个停止位
    USART1 -> CR3  =   0;                                       // 数据清0
    USART1 -> CR3 &= ~(0x01 << 6);                              // DMA接收[6]: 0=禁止、1=使能
    USART1 -> CR3 &= ~(0x01 << 7);                              // DMA发送[7]: 0=禁止、1=使能
    // 串口中断设置
    USART1->CR1 &= ~(0x01 << 7);                                // 关闭发送中断
    USART1->CR1 |= 0x01 << 5;                                   // 使能接收中断: 接收缓冲区非空
    USART1->CR1 |= 0x01 << 4;                                   // 使能空闲中断：超过1字节时间没收到新数据
    USART1->SR   = ~(0x00F0);                                   // 清理中断
    // 打开串口
    USART1 -> CR1 |= 0x01 << 13;                                // 使能UART开始工作
    // 关联缓冲区
    xUART1.puRxData = uaUART1RxData;                            // 关联接收缓冲区的地址
    xUART1.puTxFiFoData = uaUART1TxFiFoData;                    // 关联发送缓冲区的地址
    // 输出提示
    printf("\r\r\r===========  STM32F407VE 外设 初始化报告 ===========\r");                   // 输出到串口助手
    SystemCoreClockUpdate();                                                                  // 更新一下系统运行频率变量
    printf("系统时钟频率             %d MHz\r", SystemCoreClock / 1000000);                   // 输出到串口助手
    printf("UART1 初始化配置         %d-None-8-1; 已完成初始化配置、收发配置\r", ulBaudrate); // 输出到串口助手
}

/******************************************************************************
 * 函  数： USART1_IRQHandler
 * 功  能： USART1的接收中断、空闲中断、发送中断
 * 参  数： 无
 * 返回值： 无
 * 备  注： 本函数，当产生中断事件时，由硬件调用。
 *          如果使用本文件代码，在工程文件的其它地方，要注释同名函数，否则冲突。
******************************************************************************/
// void USART1_IRQHandler(void)
// {
//     static uint16_t cnt = 0;                                         // 接收字节数累计：每一帧数据已接收到的字节数
//     static uint8_t  rxTemp[UART1_RX_BUF_SIZE];                       // 接收数据缓存数组：每新接收１个字节，先顺序存放到这里，当一帧接收完(发生空闲中断), 再转存到全局变量：xUSART.puRxData[xx]中；

	
//     // 发送中断：用于把环形缓冲的数据，逐字节发出

//     if ((USART1->SR & 1 << 7) && (USART1->CR1 & 1 << 7))             // 检查TXE(发送数据寄存器空)、TXEIE(发送缓冲区空中断使能)
//     {
//         USART1->DR = xUART1.puTxFiFoData[xUART1.usTxFiFoTail++];     // 把要发送的字节，放入USART的发送寄存器
//         if (xUART1.usTxFiFoTail == UART1_TX_BUF_SIZE)                // 如果数据指针到了尾部，就重新标记到0
//             xUART1.usTxFiFoTail = 0;
//         if (xUART1.usTxFiFoTail == xUART1.usTxFiFoData)
//             USART1->CR1 &= ~(0x01 << 7);                             // 已发送完成，关闭发送缓冲区空置中断 TXEIE
//         return;
//     }

//     // 接收中断：用于逐个字节接收，存放到临时缓存
//     if (USART1->SR & (0x01 << 5))                                    // 检查RXNE(读数据寄存器非空标志位); RXNE中断清理方法：读DR时自动清理；
//     {
//         if ((cnt >= UART1_RX_BUF_SIZE))//||(xUART1.ReceivedFlag==1   // 判断1: 当前帧已接收到的数据量，已满(缓存区), 为避免溢出，本包后面接收到的数据直接舍弃．
//         {
//             // 判断2: 如果之前接收好的数据包还没处理，就放弃新数据，即，新数据帧不能覆盖旧数据帧，直至旧数据帧被处理．缺点：数据传输过快于处理速度时会掉包；好处：机制清晰，易于调试
//             printf("警告：UART1单帧接收量，已超出接收缓存大小\r!");
//             USART1->DR;                                              // 读取数据寄存器的数据，但不保存．主要作用：读DR时自动清理接收中断标志；
//             return;
//         }
//         rxTemp[cnt++] = USART1->DR ;                                 // 把新收到的字节数据，顺序存放到RXTemp数组中；注意：读取DR时自动清零中断位；
//         return;
//     }

//     // 空闲中断：用于判断一帧数据结束，整理数据到外部备读
//     if (USART1->SR & (0x01 << 4))                                    // 检查IDLE(空闲中断标志位); IDLE中断标志清理方法：序列清零，USART1 ->SR;  USART1 ->DR;
//     {
//         xUART1.usRxNum  = 0;                                         // 把接收到的数据字节数清0
//         memcpy(xUART1.puRxData, rxTemp, UART1_RX_BUF_SIZE);          // 把本帧接收到的数据，存入到结构体的数组成员xUARTx.puRxData中, 等待处理; 注意：复制的是整个数组，包括0值，以方便字符串输出时尾部以0作字符串结束符
//         xUART1.usRxNum  = cnt;                                       // 把接收到的字节数，存入到结构体变量xUARTx.usRxNum中；
//         cnt = 0;                                                     // 接收字节数累计器，清零; 准备下一次的接收
//         memset(rxTemp, 0, UART1_RX_BUF_SIZE);                        // 接收数据缓存数组，清零; 准备下一次的接收
//         USART1 ->SR;
//         USART1 ->DR;                                                 // 清零IDLE中断标志位!! 序列清零，顺序不能错!!
//         return;
//     }
//     return;
// }

/******************************************************************************
 * 函  数： UART1_SendData
 * 功  能： UART通过中断发送数据
 *         【适合场景】本函数可发送各种数据，而不限于字符串，如int,char
 *         【不 适 合】注意h文件中所定义的发缓冲区大小、注意数据压入缓冲区的速度与串口发送速度的冲突
 * 参  数： uint8_t*  puData   需发送数据的地址
 *          uint16_t  usNum    发送的字节数 ，数量受限于h文件中设置的发送缓存区大小宏定义
 * 返回值： 无
 ******************************************************************************/
void UART1_SendData(uint8_t *puData, uint16_t usNum)
{
    for (uint16_t i = 0; i < usNum; i++)                         // 把数据放入环形缓冲区
    {
        xUART1.puTxFiFoData[xUART1.usTxFiFoData++] = puData[i];  // 把字节放到缓冲区最后的位置，然后指针后移
        if (xUART1.usTxFiFoData == UART1_TX_BUF_SIZE)            // 如果指针位置到达缓冲区的最大值，则归0
            xUART1.usTxFiFoData = 0;
    }                                                            // 为了方便阅读理解，这里没有把此部分封装成队列函数，可以自行封装

    if ((USART1->CR1 & 1 << 7) == 0)                             // 检查USART寄存器的发送缓冲区空置中断(TXEIE)是否已打开
        USART1->CR1 |= 1 << 7;                                   // 打开TXEIE中断
}

/******************************************************************************
 * 函  数： UART1_SendString
 * 功  能： 发送字符串
 *          用法请参考printf，及示例中的展示
 *          注意，最大发送字节数为512-1个字符，可在函数中修改上限
 * 参  数： const char *pcString, ...   (如同printf的用法)
 * 返回值： 无
 ******************************************************************************/
void UART1_SendString(const char *pcString, ...)
{
    char mBuffer[512] = {0};;                               // 开辟一个缓存, 并把里面的数据置0
    va_list ap;                                             // 新建一个可变参数列表
    va_start(ap, pcString);                                 // 列表指向第一个可变参数
    vsnprintf(mBuffer, 512, pcString, ap);                  // 把所有参数，按格式，输出到缓存; 参数2用于限制发送的最大字节数，如果达到上限，则只发送上限值-1; 最后1字节自动置'\0'
    va_end(ap);                                             // 清空可变参数列表
    UART1_SendData((uint8_t *)mBuffer, strlen(mBuffer));    // 把字节存放环形缓冲，排队准备发送
}

/******************************************************************************
 * 函    数： UART1_SendAT
 * 功    能： 发送AT命令, 并等待返回信息
 * 参    数： char     *pcString      AT指令字符串
 *            char     *pcAckString   期待的指令返回信息字符串
 *            uint16_t  usTimeOut     发送命令后等待的时间，毫秒
 *
 * 返 回 值： 0-执行失败、1-执行正常
 ******************************************************************************/
uint8_t UART1_SendAT(char *pcAT, char *pcAckString, uint16_t usTimeOutMs)
{
    UART1_ClearRx();                                              // 清0
    UART1_SendString(pcAT);                                       // 发送AT指令字符串
    
    while (usTimeOutMs--)                                         // 判断是否起时(这里只作简单的循环判断次数处理）
    {
        if (UART1_GetRxNum())                                     // 判断是否接收到数据
        {
            UART1_ClearRx();                                      // 清0接收字节数; 注意：接收到的数据内容 ，是没有被清0的
            if (strstr((char *)UART1_GetRxData(), pcAckString))   // 判断返回数据中是否有期待的字符
                return 1;                                         // 返回：0-超时没有返回、1-正常返回期待值
        }
        delay_ms(1);                                              // 延时; 用于超时退出处理，避免死等
    }
    return 0;                                                     // 返回：0-超时、返回异常，1-正常返回期待值
}


/******************************************************************************
 * 函  数： UART1_SendStringForDMA
 * 功  能： UART通过DMA发送数据，省了占用中断的时间
 *         【适合场景】字符串，字节数非常多，
 *         【不 适 合】1:只适合发送字符串，不适合发送可能含0的数值类数据; 2-时间间隔要足够
 * 参  数： char strintTemp  要发送的字符串首地址
 * 返回值： 无
 * 备  注:  本函数为保留函数，留作用户参考。为了方便移植，本文件对外不再使用本函数。
 ******************************************************************************/
#if 0
void UART1_SendStringForDMA(char *stringTemp)
{
    static uint8_t Flag_DmaTxInit = 0;                // 用于标记是否已配置DMA发送
    uint32_t   num = 0;                               // 发送的数量，注意发送的单位不是必须8位的
    char *t = stringTemp ;                            // 用于配合计算发送的数量

    while (*t++ != 0)  num++;                         // 计算要发送的数目，这步比较耗时，测试发现每多6个字节，增加1us，单位：8位

    while (DMA1_Channel4->CNDTR > 0);                 // 重要：如果DMA还在进行上次发送，就等待; 得进完成中断清标志，F4不用这么麻烦，发送完后EN自动清零
    if (Flag_DmaTxInit == 0)                          // 是否已进行过USAART_TX的DMA传输配置
    {
        Flag_DmaTxInit  = 1;                          // 设置标记，下次调用本函数就不再进行配置了
        USART1 ->CR3   |= 1 << 7;                     // 使能DMA发送
        RCC->AHBENR    |= 1 << 0;                     // 开启DMA1时钟  [0]DMA1   [1]DMA2

        DMA1_Channel4->CCR   = 0;                     // 失能， 清0整个寄存器, DMA必须失能才能配置
        DMA1_Channel4->CNDTR = num;                   // 传输数据量
        DMA1_Channel4->CMAR  = (uint32_t)stringTemp;  // 存储器地址
        DMA1_Channel4->CPAR  = (uint32_t)&USART1->DR; // 外设地址

        DMA1_Channel4->CCR |= 1 << 4;                 // 数据传输方向   0:从外设读   1:从存储器读
        DMA1_Channel4->CCR |= 0 << 5;                 // 循环模式       0:不循环     1：循环
        DMA1_Channel4->CCR |= 0 << 6;                 // 外设地址非增量模式
        DMA1_Channel4->CCR |= 1 << 7;                 // 存储器增量模式
        DMA1_Channel4->CCR |= 0 << 8;                 // 外设数据宽度为8位
        DMA1_Channel4->CCR |= 0 << 10;                // 存储器数据宽度8位
        DMA1_Channel4->CCR |= 0 << 12;                // 中等优先级
        DMA1_Channel4->CCR |= 0 << 14;                // 非存储器到存储器模式
    }
    DMA1_Channel4->CCR  &= ~((uint32_t)(1 << 0));     // 失能，DMA必须失能才能配置
    DMA1_Channel4->CNDTR = num;                       // 传输数据量
    DMA1_Channel4->CMAR  = (uint32_t)stringTemp;      // 存储器地址
    DMA1_Channel4->CCR  |= 1 << 0;                    // 开启DMA传输
}
#endif

/******************************************************************************
 * 函  数： UART1_GetRxNum
 * 功  能： 获取最新一帧数据的字节数
 * 参  数： 无
 * 返回值： 0=没有接收到数据，非0=新一帧数据的字节数
 ******************************************************************************/
uint16_t UART1_GetRxNum(void)
{
    return xUART1.usRxNum ;
}

/******************************************************************************
 * 函  数： UART1_GetRxData
 * 功  能： 获取最新一帧数据 (数据的地址）
 * 参  数： 无
 * 返回值： 缓存地址(uint8_t*)
 ******************************************************************************/
uint8_t *UART1_GetRxData(void)
{
    return xUART1.puRxData ;
}

/******************************************************************************
 * 函  数： UART1_ClearRx
 * 功  能： 清理最后一帧数据的缓存
 *          主要是清0字节数，因为它是用来判断接收的标准
 * 参  数： 无
 * 返回值： 无
 ******************************************************************************/
void UART1_ClearRx(void)
{
    xUART1.usRxNum = 0 ;
}
#endif  // endif UART1_EN





//////////////////////////////////////////////////////////////   UART-2   ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if UART2_EN

static xUSATR_TypeDef  xUART2 = { 0 };                      // 定义 UART2 的收发结构体
static uint8_t uaUART2RxData[UART2_RX_BUF_SIZE];            // 定义 UART2 的接收缓存
static uint8_t uaUART2TxFiFoData[UART2_TX_BUF_SIZE];        // 定义 UART2 的发送缓存

/******************************************************************************
 * 函  数： UART2_Init
 * 功  能： 初始化USART2的通信引脚、协议参数、中断优先级
 *          引脚：TX-PA2、RX-PA3
 *          协议：波特率-None-8-1
 *          发送：发送中断
 *          接收：接收+空闲中断
 *
 * 参  数： uint32_t  ulBaudrate  通信波特率
 * 返回值： 无
 ******************************************************************************/
void UART2_Init(uint32_t ulBaudrate)
{
    // 使能相关时钟
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;                       // 使能外设：UART2
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;                        // 使能GPIO：GPIOA
    // 关闭串口
    USART2 -> CR1  =   0;                                       // 关闭串口，配置清零

#ifdef USE_STDPERIPH_DRIVER                                     // 标准库 配置
    // 配置TX_PA2
    GPIO_InitTypeDef  GPIO_InitStructure = {0};                 // GPIO 初始化结构体
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;                 // 引脚编号：TX_PA2
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;               // 引脚方向: 复用功能
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;              // 输出模式：推挽
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;               // 上下拉：上拉
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;           // 输出速度：50MHz
    GPIO_Init(GPIOA, &GPIO_InitStructure);                      // 初始化：把上述参数，更新到芯片寄存器
    // 配置RX_PA3
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_3;                 // 引脚编号：RX_PA3
    GPIO_Init(GPIOA, &GPIO_InitStructure);                      // 初始化：把上述参数，更新到芯片寄存器
    // 配置引脚的具体复用功能
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);   // 配置引脚复用功能：USART2
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);   // 配置引脚复用功能：USART2
    // NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    // 中断配置
    NVIC_InitTypeDef  NVIC_InitStructure = {0};                 // 中断优先级配置结构体
    NVIC_InitStructure .NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure .NVIC_IRQChannelPreemptionPriority = 1;  // 抢占优先级
    NVIC_InitStructure .NVIC_IRQChannelSubPriority = 1;         // 子优先级
    NVIC_InitStructure .NVIC_IRQChannelCmd = ENABLE;            // IRQ通道使能
    NVIC_Init(&NVIC_InitStructure);
#endif

#ifdef USE_HAL_DRIVER                                           // HAL库 配置
    // GPIO引脚工作模式配置
    GPIO_InitTypeDef    GPIO_InitStruct = {0};                  // 声明初始化要用到的结构体
    GPIO_InitStruct.Pin   = GPIO_PIN_2 | GPIO_PIN_3;            // 引脚 TX-PA2、RX-PA3
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;                    // 工作模式
    GPIO_InitStruct.Pull  = GPIO_PULLUP;                        // 上下拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;          // 引脚速率
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;                // 引脚复用功能
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);                     // 初始化引脚工作模式
    // 中断优选级配置
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 1);                    // 配置中断线的优先级
    HAL_NVIC_EnableIRQ(USART2_IRQn);                            // 使能中断线
#endif

    // 计算波特率参数
    float    temp;
    uint16_t mantissa, fraction;
    SystemCoreClockUpdate();                                    // 更新系统运行频率全局值; 函数SystemCoreClock( )，在标准库、HAL库通用
    temp = (float)(SystemCoreClock / 4) / (ulBaudrate * 16);    // 波特率公式计算; USART2挂载在APB1, 时钟为系统时钟的4分频; 全局变量SystemCoreClock，在标准库、HAL库通用;
    mantissa = temp;				                            // 整数部分
    fraction = (temp - mantissa) * 16;                          // 小数部分
    USART2 -> BRR  = mantissa << 4 | fraction;                  // 设置波特率
    // 串口通信参数配置
    USART2 -> CR1 |=   0x01 << 2;                               // 接收使能[2]: 0=失能、1=使能
    USART2 -> CR1 |=   0x01 << 3;                               // 发送使能[3]：0=失能、1=使能
    USART2 -> CR1 &= ~(0x01 << 12);                             // 数据位[12]：0=8位、1=9位
    USART2 -> CR2  =   0;                                       // 数据清0
    USART2 -> CR2 &=  ~(0x03 << 12);                            // 停止位[13:12]：00b=1个停止位、01b=0.5个停止位、10b=2个停止位、11b=1.5个停止位
    USART2 -> CR3  =   0;                                       // 数据清0
    USART2 -> CR3 &= ~(0x01 << 6);                              // DMA接收[6]: 0=禁止、1=使能
    USART2 -> CR3 &= ~(0x01 << 7);                              // DMA发送[7]: 0=禁止、1=使能
    // 串口中断设置
    USART2->CR1 &= ~(0x01 << 7);                                // 关闭发送中断
    USART2->CR1 |= 0x01 << 5;                                   // 使能接收中断: 接收缓冲区非空
    USART2->CR1 |= 0x01 << 4;                                   // 使能空闲中断：超过1字节时间没收到新数据
    USART2->SR   = ~(0x00F0);                                   // 清理中断
    // 开启USART2
    USART2 -> CR1 |= 0x01 << 13;                                // 使能UART开始工作
    // 关联缓冲区
    xUART2.puRxData = uaUART2RxData;                            // 获取接收缓冲区的地址
    xUART2.puTxFiFoData = uaUART2TxFiFoData;                    // 获取发送缓冲区的地址
    // 输出提示
    printf("UART2 初始化配置         %d-None-8-1; 已完成初始化配置、收发配置\r", ulBaudrate);
}

/******************************************************************************
 * 函  数： USART2_IRQHandler
 * 功  能： USART2的接收中断、空闲中断、发送中断
 * 参  数： 无
 * 返回值： 无
 * 备  注： 本函数，当产生中断事件时，由硬件调用。
 *          如果使用本文件代码，在工程文件的其它地方，要注释同名函数，否则冲突。
 ******************************************************************************/
void USART2_IRQHandler(void)
{
    static uint16_t cnt = 0;                                        // 接收字节数累计：每一帧数据已接收到的字节数
    static uint8_t  rxTemp[UART2_RX_BUF_SIZE];                      // 接收数据缓存数组：每新接收１个字节，先顺序存放到这里，当一帧接收完(发生空闲中断), 再转存到全局变量：xUARTx.puRxData[xx]中；

    // 发送中断：用于把环形缓冲的数据，逐字节发出
    if ((USART2->SR & 1 << 7) && (USART2->CR1 & 1 << 7))            // 检查TXE(发送数据寄存器空)、TXEIE(发送缓冲区空中断使能)
    {
        USART2->DR = xUART2.puTxFiFoData[xUART2.usTxFiFoTail++];    // 把要发送的字节，放入USART的发送寄存器
        if (xUART2.usTxFiFoTail == UART2_TX_BUF_SIZE)               // 如果数据指针到了尾部，就重新标记到0
            xUART2.usTxFiFoTail = 0;
        if (xUART2.usTxFiFoTail == xUART2.usTxFiFoData)
            USART2->CR1 &= ~(1 << 7);                               // 已发送完成，关闭发送缓冲区空置中断 TXEIE
        return;
    }

    // 接收中断：用于逐个字节接收，存放到临时缓存
    if (USART2->SR & (1 << 5))                                      // 检查RXNE(读数据寄存器非空标志位); RXNE中断清理方法：读DR时自动清理；
    {
        if ((cnt >= UART2_RX_BUF_SIZE))//||xUART2.ReceivedFlag==1   // 判断1: 当前帧已接收到的数据量，已满(缓存区), 为避免溢出，本包后面接收到的数据直接舍弃．
        {
            // 判断2: 如果之前接收好的数据包还没处理，就放弃新数据，即，新数据帧不能覆盖旧数据帧，直至旧数据帧被处理．缺点：数据传输过快于处理速度时会掉包；好处：机制清晰，易于调试
            printf("警告：UART2单帧接收量，已超出接收缓存大小\r!");
            USART2->DR;                                             // 读取数据寄存器的数据，但不保存．主要作用：读DR时自动清理接收中断标志；
            return;
        }
        rxTemp[cnt++] = USART2->DR ;                                // 把新收到的字节数据，顺序存放到RXTemp数组中；注意：读取DR时自动清零中断位；
        return;
    }

    // 空闲中断：用于判断一帧数据结束，整理数据到外部备读
    if (USART2->SR & (1 << 4))                                      // 检查IDLE(空闲中断标志位); IDLE中断标志清理方法：序列清零，USART1 ->SR;  USART1 ->DR;
    {
        xUART2.usRxNum  = 0;                                        // 把接收到的数据字节数清0
        memcpy(xUART2.puRxData, rxTemp, UART2_RX_BUF_SIZE);         // 把本帧接收到的数据，存入到结构体的数组成员xUARTx.puRxData中, 等待处理; 注意：复制的是整个数组，包括0值，以方便字符串输出时尾部以0作字符串结束符
        xUART2.usRxNum  = cnt;                                      // 把接收到的字节数，存入到结构体变量xUARTx.usRxNum中；
        cnt = 0;                                                    // 接收字节数累计器，清零; 准备下一次的接收
        memset(rxTemp, 0, UART2_RX_BUF_SIZE);                       // 接收数据缓存数组，清零; 准备下一次的接收
        USART2 ->SR;
        USART2 ->DR;                                                // 清零IDLE中断标志位!! 序列清零，顺序不能错!!
        return;
    }

    return;
}

/******************************************************************************
 * 函  数： UART2_SendData
 * 功  能： UART通过中断发送数据
 *         【适合场景】本函数可发送各种数据，而不限于字符串，如int,char
 *         【不 适 合】注意h文件中所定义的发缓冲区大小、注意数据压入缓冲区的速度与串口发送速度的冲突
 * 参  数： uint8_t* puData     需发送数据的地址
 *          uint8_t  usNum      发送的字节数 ，数量受限于h文件中设置的发送缓存区大小宏定义
 * 返回值： 无
 ******************************************************************************/
void UART2_SendData(uint8_t *puData, uint16_t usNum)
{
    for (uint16_t i = 0; i < usNum; i++)                         // 把数据放入环形缓冲区
    {
        xUART2.puTxFiFoData[xUART2.usTxFiFoData++] = puData[i];  // 把字节放到缓冲区最后的位置，然后指针后移
        if (xUART2.usTxFiFoData == UART2_TX_BUF_SIZE)            // 如果指针位置到达缓冲区的最大值，则归0
            xUART2.usTxFiFoData = 0;
    }

    if ((USART2->CR1 & 1 << 7) == 0)                             // 检查USART寄存器的发送缓冲区空置中断(TXEIE)是否已打开
        USART2->CR1 |= 1 << 7;                                   // 打开TXEIE中断
}

/******************************************************************************
 * 函  数： UART2_SendString
 * 功  能： 发送字符串
 *          用法请参考printf，及示例中的展示
 *          注意，最大发送字节数为512-1个字符，可在函数中修改上限
 * 参  数： const char *pcString, ...   (如同printf的用法)
 * 返回值： 无
 ******************************************************************************/
void UART2_SendString(const char *pcString, ...)
{
    char mBuffer[512] = {0};;                               // 开辟一个缓存, 并把里面的数据置0
    va_list ap;                                             // 新建一个可变参数列表
    va_start(ap, pcString);                                 // 列表指向第一个可变参数
    vsnprintf(mBuffer, 512, pcString, ap);                  // 把所有参数，按格式，输出到缓存; 参数2用于限制发送的最大字节数，如果达到上限，则只发送上限值-1; 最后1字节自动置'\0'
    va_end(ap);                                             // 清空可变参数列表
    UART2_SendData((uint8_t *)mBuffer, strlen(mBuffer));    // 把字节存放环形缓冲，排队准备发送
}

/******************************************************************************
 * 函    数： UART2_SendAT
 * 功    能： 发送AT命令, 并等待返回信息
 * 参    数： char     *pcString      AT指令字符串
 *            char     *pcAckString   期待的指令返回信息字符串
 *            uint16_t  usTimeOut     发送命令后等待的时间，毫秒
 *
 * 返 回 值： 0-执行失败、1-执行正常
 ******************************************************************************/
uint8_t UART2_SendAT(char *pcAT, char *pcAckString, uint16_t usTimeOutMs)
{
    UART2_ClearRx();                                              // 清0
    UART2_SendString(pcAT);                                       // 发送AT指令字符串
    
    while (usTimeOutMs--)                                         // 判断是否起时(这里只作简单的循环判断次数处理）
    {
        if (UART2_GetRxNum())                                     // 判断是否接收到数据
        {
            UART2_ClearRx();                                      // 清0接收字节数; 注意：接收到的数据内容 ，是没有被清0的
            if (strstr((char *)UART2_GetRxData(), pcAckString))   // 判断返回数据中是否有期待的字符
                return 1;                                         // 返回：0-超时没有返回、1-正常返回期待值
        }
        delay_ms(1);                                              // 延时; 用于超时退出处理，避免死等
    }
    return 0;                                                     // 返回：0-超时、返回异常，1-正常返回期待值
}

/******************************************************************************
 * 函  数： UART2_GetRxNum
 * 功  能： 获取最新一帧数据的字节数
 * 参  数： 无
 * 返回值： 0=没有接收到数据，非0=新一帧数据的字节数
 ******************************************************************************/
uint16_t UART2_GetRxNum(void)
{
    return xUART2.usRxNum ;
}

/******************************************************************************
 * 函  数： UART2_GetRxData
 * 功  能： 获取最新一帧数据 (数据的地址）
 * 参  数： 无
 * 返回值： 数据的地址(uint8_t*)
 ******************************************************************************/
uint8_t *UART2_GetRxData(void)
{
    return xUART2.puRxData ;
}

/******************************************************************************
 * 函  数： UART2_ClearRx
 * 功  能： 清理最后一帧数据的缓存
 *          主要是清0字节数，因为它是用来判断接收的标准
 * 参  数： 无
 * 返回值： 无
 ******************************************************************************/
void UART2_ClearRx(void)
{
    xUART2.usRxNum = 0 ;
}
#endif  // endif UART2_EN





//////////////////////////////////////////////////////////////   USART-3   //////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if UART3_EN

static xUSATR_TypeDef  xUART3 = { 0 };                      // 定义 UART3 的收发结构体
static uint8_t uaUart3RxData[UART3_RX_BUF_SIZE];            // 定义 UART3 的接收缓存
static uint8_t uaUart3TxFiFoData[UART3_TX_BUF_SIZE];        // 定义 UART3 的发送缓存

/******************************************************************************
 * 函  数： UART3_Init
 * 功  能： 初始化USART3的通信引脚、协议参数、中断优先级
 *          引脚：TX-PB10、RX-PB11
 *          协议：波特率-None-8-1
 *          发送：发送中断
 *          接收：接收+空闲中断
 *
 * 参  数： uint32_t ulBaudrate  通信波特率
 * 返回值： 无
 ******************************************************************************/
void UART3_Init(uint32_t ulBaudrate)
{
    // 使能相关时钟
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;                       // 使能外设：UART3
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;                        // 使能GPIO：GPIOB
    // 关闭串口
    USART3 -> CR1  =   0;                                       // 关闭串口，配置清零

#ifdef USE_STDPERIPH_DRIVER                                     // 标准库 配置
    // 配置TX引脚
    GPIO_InitTypeDef  GPIO_InitStructure = {0};                 // GPIO 初始化结构体
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;                // 引脚编号：TX_PB10
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;               // 引脚方向: 复用功能
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;              // 输出模式：推挽
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;               // 上下拉：上拉
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;           // 输出速度：50MHz
    GPIO_Init(GPIOB, &GPIO_InitStructure);                      // 初始化：把上述参数，更新到芯片寄存器
    // 配置RX
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_11;                // 引脚编号：RX_PB11
    GPIO_Init(GPIOB, &GPIO_InitStructure);                      // 初始化：把上述参数，更新到芯片寄存器
    // 配置引脚的具体复用功能
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_USART3);  // 配置引脚复用功能
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_USART3);  // 配置引脚复用功能
    // NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    // 中断配置
    NVIC_InitTypeDef  NVIC_InitStructure = {0};                 // 中断优先级配置结构体
    NVIC_InitStructure .NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure .NVIC_IRQChannelPreemptionPriority = 1;  // 抢占优先级
    NVIC_InitStructure .NVIC_IRQChannelSubPriority = 1;         // 子优先级
    NVIC_InitStructure .NVIC_IRQChannelCmd = ENABLE;            // IRQ通道使能
    NVIC_Init(&NVIC_InitStructure);
#endif

#ifdef USE_HAL_DRIVER                                           // HAL库 配置
    // GPIO引脚工作模式配置
    GPIO_InitTypeDef    GPIO_InitStruct = {0};                  // 声明初始化要用到的结构体
    GPIO_InitStruct.Pin   = GPIO_PIN_10 | GPIO_PIN_11;          // 引脚 TX-PB10、RX-PB11
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;                    // 工作模式
    GPIO_InitStruct.Pull  = GPIO_PULLUP;                        // 上下拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;          // 引脚速率
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;                // 引脚复用功能
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);                     // 初始化引脚工作模式
    // 中断优选级配置
    HAL_NVIC_SetPriority(USART3_IRQn, 1, 1);                    // 配置中断线的优先级
    HAL_NVIC_EnableIRQ(USART3_IRQn);                            // 使能中断线
#endif

    // 计算波特率参数
    float    temp;
    uint16_t mantissa, fraction;
    SystemCoreClockUpdate();                                    // 更新系统运行频率全局值; 函数SystemCoreClock( )，在标准库、HAL库通用
    temp = (float)(SystemCoreClock / 4) / (ulBaudrate * 16);    // 波特率公式计算; USART3挂载在APB1, 时钟为系统时钟的4分频; 全局变量SystemCoreClock，在标准库、HAL库通用;
    mantissa = temp;				                            // 整数部分
    fraction = (temp - mantissa) * 16;                          // 小数部分
    USART3 -> BRR  = mantissa << 4 | fraction;                  // 设置波特率
    // 串口通信参数配置
    USART3 -> CR1 |=   0x01 << 2;                               // 接收使能[2]: 0=失能、1=使能
    USART3 -> CR1 |=   0x01 << 3;                               // 发送使能[3]：0=失能、1=使能
    USART3 -> CR1 &= ~(0x01 << 12);                             // 数据位[12]：0=8位、1=9位
    USART3 -> CR2  =   0;                                       // 数据清0
    USART3 -> CR2 &=  ~(0x03 << 12);                            // 停止位[13:12]：00b=1个停止位、01b=0.5个停止位、10b=2个停止位、11b=1.5个停止位
    USART3 -> CR3  =   0;                                       // 数据清0
    USART3 -> CR3 &= ~(0x01 << 6);                              // DMA接收[6]: 0=禁止、1=使能
    USART3 -> CR3 &= ~(0x01 << 7);                              // DMA发送[7]: 0=禁止、1=使能
    // 串口中断设置
    USART3->CR1 &= ~(0x01 << 7);                                // 关闭发送中断
    USART3->CR1 |= 0x01 << 5;                                   // 使能接收中断: 接收缓冲区非空
    USART3->CR1 |= 0x01 << 4;                                   // 使能空闲中断：超过1字节时间没收到新数据
    USART3->SR   = ~(0x00F0);                                   // 清理中断
    // 打开串口
    USART3 -> CR1 |= 0x01 << 13;                                // 使能UART开始工作
    // 关联缓冲区
    xUART3.puRxData = uaUart3RxData;                            // 获取接收缓冲区的地址
    xUART3.puTxFiFoData = uaUart3TxFiFoData;                    // 获取发送缓冲区的地址
    // 输出提示
    printf("UART3 Init         %d-None-8-1; Init Success\r", ulBaudrate);
}

/******************************************************************************
 * 函  数： USART3_IRQHandler
 * 功  能： USART3的接收中断、空闲中断、发送中断
 * 参  数： 无
 * 返回值： 无
 * 备  注： 本函数，当产生中断事件时，由硬件调用。
 *          如果使用本文件代码，在工程文件的其它地方，要注释同名函数，否则冲突。
 ******************************************************************************/
void USART3_IRQHandler(void)
{
    static uint16_t cnt = 0;                                        // 接收字节数累计：每一帧数据已接收到的字节数
    static uint8_t  rxTemp[UART3_RX_BUF_SIZE];                      // 接收数据缓存数组：每新接收１个字节，先顺序存放到这里，当一帧接收完(发生空闲中断), 再转存到全局变量：xUARTx.puRxData[xx]中；

    // 发送中断：用于把环形缓冲的数据，逐字节发出
    if ((USART3->SR & 1 << 7) && (USART3->CR1 & 1 << 7))            // 检查TXE(发送数据寄存器空)、TXEIE(发送缓冲区空中断使能)
    {
        USART3->DR = xUART3.puTxFiFoData[xUART3.usTxFiFoTail++];    // 把要发送的字节，放入USART的发送寄存器
        if (xUART3.usTxFiFoTail == UART3_TX_BUF_SIZE)               // 如果数据指针到了尾部，就重新标记到0
            xUART3.usTxFiFoTail = 0;
        if (xUART3.usTxFiFoTail == xUART3.usTxFiFoData)
            USART3->CR1 &= ~(1 << 7);                               // 已发送完成，关闭发送缓冲区空置中断 TXEIE
        return;
    }

    // 接收中断：用于逐个字节接收，存放到临时缓存
    if (USART3->SR & (1 << 5))                                      // 检查RXNE(读数据寄存器非空标志位); RXNE中断清理方法：读DR时自动清理；
    {
        if ((cnt >= UART3_RX_BUF_SIZE))//||xUART3.ReceivedFlag==1   // 判断1: 当前帧已接收到的数据量，已满(缓存区), 为避免溢出，本包后面接收到的数据直接舍弃．
        {
            // 判断2: 如果之前接收好的数据包还没处理，就放弃新数据，即，新数据帧不能覆盖旧数据帧，直至旧数据帧被处理．缺点：数据传输过快于处理速度时会掉包；好处：机制清晰，易于调试
            printf("警告：UART3单帧接收量，已超出接收缓存大小\r!");
            USART3->DR;                                             // 读取数据寄存器的数据，但不保存．主要作用：读DR时自动清理接收中断标志；
            return;
        }
        rxTemp[cnt++] = USART3->DR ;                                // 把新收到的字节数据，顺序存放到RXTemp数组中；注意：读取DR时自动清零中断位
        return;
    }

    // 空闲中断：用于判断一帧数据结束，整理数据到外部备读
    if (USART3->SR & (1 << 4))                                      // 检查IDLE(空闲中断标志位); IDLE中断标志清理方法：序列清零，USART1 ->SR;  USART1 ->DR;
    {
        xUART3.usRxNum  = 0;                                        // 把接收到的数据字节数清0
        memcpy(xUART3.puRxData, rxTemp, UART3_RX_BUF_SIZE);         // 把本帧接收到的数据，存入到结构体的数组成员xUARTx.puRxData中, 等待处理; 注意：复制的是整个数组，包括0值，以方便字符串输出时尾部以0作字符串结束符
        xUART3.usRxNum  = cnt;                                      // 把接收到的字节数，存入到结构体变量xUARTx.usRxNum中；
        cnt = 0;                                                    // 接收字节数累计器，清零; 准备下一次的接收
        memset(rxTemp, 0, UART3_RX_BUF_SIZE);                       // 接收数据缓存数组，清零; 准备下一次的接收
        USART3 ->SR;
        USART3 ->DR;                                                // 清零IDLE中断标志位!! 序列清零，顺序不能错!!
        return;
    }

    return;
}

/******************************************************************************
 * 函  数： UART3_SendData
 * 功  能： UART通过中断发送数据
 *         【适合场景】本函数可发送各种数据，而不限于字符串，如int,char
 *         【不 适 合】注意h文件中所定义的发缓冲区大小、注意数据压入缓冲区的速度与串口发送速度的冲突
 * 参  数： uint8_t* puData   需发送数据的地址
 *          uint8_t  usNum      发送的字节数 ，数量受限于h文件中设置的发送缓存区大小宏定义
 * 返回值： 无
 ******************************************************************************/
void UART3_SendData(uint8_t *puData, uint16_t usNum)
{
    for (uint16_t i = 0; i < usNum; i++)                           // 把数据放入环形缓冲区
    {
        xUART3.puTxFiFoData[xUART3.usTxFiFoData++] = puData[i];    // 把字节放到缓冲区最后的位置，然后指针后移
        if (xUART3.usTxFiFoData == UART3_TX_BUF_SIZE)              // 如果指针位置到达缓冲区的最大值，则归0
            xUART3.usTxFiFoData = 0;
    }

    if ((USART3->CR1 & 1 << 7) == 0)                               // 检查USART寄存器的发送缓冲区空置中断(TXEIE)是否已打开
        USART3->CR1 |= 1 << 7;                                     // 打开TXEIE中断
}

/******************************************************************************
 * 函  数： UART3_SendString
 * 功  能： 发送字符串
 *          用法请参考printf，及示例中的展示
 *          注意，最大发送字节数为512-1个字符，可在函数中修改上限
 * 参  数： const char *pcString, ...   (如同printf的用法)
 * 返回值： 无
 ******************************************************************************/
void UART3_SendString(const char *pcString, ...)
{
    char mBuffer[512] = {0};;                                // 开辟一个缓存, 并把里面的数据置0
    va_list ap;                                              // 新建一个可变参数列表
    va_start(ap, pcString);                                  // 列表指向第一个可变参数
    vsnprintf(mBuffer, 512, pcString, ap);                   // 把所有参数，按格式，输出到缓存; 参数2用于限制发送的最大字节数，如果达到上限，则只发送上限值-1; 最后1字节自动置'\0'
    va_end(ap);                                              // 清空可变参数列表
    UART3_SendData((uint8_t *)mBuffer, strlen(mBuffer));     // 把字节存放环形缓冲，排队准备发送
}

/******************************************************************************
 * 函    数： UART3_SendAT
 * 功    能： 发送AT命令, 并等待返回信息
 * 参    数： char     *pcString      AT指令字符串
 *            char     *pcAckString   期待的指令返回信息字符串
 *            uint16_t  usTimeOut     发送命令后等待的时间，毫秒
 *
 * 返 回 值： 0-执行失败、1-执行正常
 ******************************************************************************/
uint8_t UART3_SendAT(char *pcAT, char *pcAckString, uint16_t usTimeOutMs)
{
    UART3_ClearRx();                                              // 清0
    UART3_SendString(pcAT);                                       // 发送AT指令字符串
    
    while (usTimeOutMs--)                                         // 判断是否起时(这里只作简单的循环判断次数处理）
    {
        if (UART3_GetRxNum())                                     // 判断是否接收到数据
        {
            UART3_ClearRx();                                      // 清0接收字节数; 注意：接收到的数据内容 ，是没有被清0的
            if (strstr((char *)UART3_GetRxData(), pcAckString))   // 判断返回数据中是否有期待的字符
                return 1;                                         // 返回：0-超时没有返回、1-正常返回期待值
        }
        delay_ms(1);                                              // 延时; 用于超时退出处理，避免死等
    }
    return 0;                                                     // 返回：0-超时、返回异常，1-正常返回期待值
}

/******************************************************************************
 * 函  数： UART3_GetRxNum
 * 功  能： 获取最新一帧数据的字节数
 * 参  数： 无
 * 返回值： 0=没有接收到数据，非0=新一帧数据的字节数
 ******************************************************************************/
uint16_t UART3_GetRxNum(void)
{
    return xUART3.usRxNum ;
}

/******************************************************************************
 * 函  数： UART3_GetRxData
 * 功  能： 获取最新一帧数据 (数据的地址）
 * 参  数： 无
 * 返回值： 数据的地址(uint8_t*)
 ******************************************************************************/
uint8_t *UART3_GetRxData(void)
{
    return xUART3.puRxData ;
}

/******************************************************************************
 * 函  数： UART3_ClearRx
 * 功  能： 清理最后一帧数据的缓存
 *          主要是清0字节数，因为它是用来判断接收的标准
 * 参  数： 无
 * 返回值： 无
 ******************************************************************************/
void UART3_ClearRx(void)
{
    xUART3.usRxNum = 0 ;
}
#endif  // endif UART3_EN






