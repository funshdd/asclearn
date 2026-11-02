/**
  ******************************************************************************
  * @file    uart.h
  * @brief   串口 9600/8N1 —— 中断收发 + 环形缓冲（完全非阻塞）
  ******************************************************************************
  */

#ifndef __UART_H
#define __UART_H

void Uart_Init(void);

/* 取一个接收字节，返回 1 表示取到 */
unsigned char Uart_Read(unsigned char *ch);

/* 发送一串字节（入队即返回，实际发送由中断完成） */
void Uart_Send(const unsigned char *buf, unsigned char len);
void Uart_SendStr(const char code *s);

/* 发送队列是否已空 */
unsigned char Uart_TxIdle(void);

#endif /* __UART_H */
