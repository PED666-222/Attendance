#ifndef __USART_H
#define __USART_H
#include "stdio.h"	
#include "stm32f4xx_conf.h"
#include "sys.h" 

//如果想串口中断接收，请不要注释以下宏定义
extern volatile uint8_t  g_usart1_rx_buf[512];
extern volatile uint32_t g_usart1_rx_cnt;
extern volatile uint32_t g_usart1_rx_end;

extern volatile uint8_t  g_usart2_rx_buf[512];
extern volatile uint32_t g_usart2_rx_cnt;
extern volatile uint32_t g_usart2_rx_end;

extern volatile uint8_t  g_usart3_rx_buf[512];
extern volatile uint32_t g_usart3_rx_cnt;
extern volatile uint32_t g_usart3_rx_end;

extern void Usart1_Init(uint32_t baud);
extern void Usart2_Init(uint32_t baud);
extern void Usart3_Init(uint32_t baud);

extern void Usart_Send_Str(USART_TypeDef* USARTx,char *str);
extern void Usart_Send_Bytes(USART_TypeDef* USARTx,uint8_t *buf,uint32_t len);
#endif


