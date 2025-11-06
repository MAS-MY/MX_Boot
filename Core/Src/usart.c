/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* USER CODE BEGIN 0 */
#include <stdio.h>
#include "ring_buffer.h"
ring_buffer rx_buf;

int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 1000);//
    return ch;
}

int fgetc(FILE *f)
{
    int ch;
    HAL_UART_Receive(&huart1, (uint8_t *)&ch, 1, 1000);
    return ch;
}
// �޸�getchar2����,���ȴӻ��λ�������ȡ����
int getchar2(void)
{
	unsigned char c;
	
	while (0 != ring_buffer_read(&c, &rx_buf));
	
	return c;
}

int My_putchar(char c)
{
    USART_TypeDef *usart1 = USART1;
    while(!(usart1->SR & USART_SR_TXE));  // �ȴ����ͻ�������
    usart1->DR = (uint8_t)c;
    return c;
}
void My_putstr(const char *str)
{
	while (*str)
	{
		My_putchar(*str);
		str++;
	}
}
void putdatas(const char *datas, int len)
{
	int i;
	for (i = 0; i < len; i++)
	{
		My_putchar(datas[i]);
	}
}

// ��ʼ��UART���ջ��λ��������ж�
void UART1_RingBuffer_Init(void)
{
    // ��ʼ�����λ�����
    ring_buffer_init(&rx_buf);
    
    // ʹ��USART1�����ж�
    USART1->CR1 |= USART_CR1_RXNEIE;
    
    // ����NVIC
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}


/* USER CODE END 0 */

UART_HandleTypeDef huart1;

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
    UART1_RingBuffer_Init();  // ��ʼ�����λ������ͽ����ж�

  /* USER CODE END USART1_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */
    /* USART1 clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN USART1_MspInit 1 */

  /* USER CODE END USART1_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspDeInit 0 */

  /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9|GPIO_PIN_10);

  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
// UART�����жϴ�����
void USART1_IRQHandler(void)
{
    USART_TypeDef *usart1 = USART1;
    
    // ��������������
    if (usart1->SR & USART_SR_ORE)
    {
        // ��DR�Ĵ�����������־
        volatile uint32_t temp = usart1->DR;
        (void)temp;  // �������������
    }
    // �����������
    if (usart1->SR & USART_SR_RXNE)
    {
        unsigned char data = (unsigned char)(usart1->DR & 0xFF);
        ring_buffer_write(data, &rx_buf);
    }
    
    // ���ʹ��HAL��,������Ҫ��������ĺ���
     HAL_UART_IRQHandler(&huart1);
}
/* USER CODE END 1 */
