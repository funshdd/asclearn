#ifndef __UART_H
#define __UART_H
#include <STC89C5xRC.H>
#include <stdio.h>

void Send_Byte(unsigned char ch);

void Send_String(unsigned char *str);

unsigned char Uart_GetByte(unsigned char *ch);

unsigned char Uart_TxIdle(void);

void Uart_Init();

#endif
