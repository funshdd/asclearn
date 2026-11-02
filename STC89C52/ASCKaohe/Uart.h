#ifndef __UART_H
#define __UART_H

#include <STC89C5xRC.H>

unsigned char data_read;

void send_byte(unsigned char ch)
{
	SBUF = ch;
	while(!TI);
	TI = 0;
}

void send_string(unsigned char *str)
{
	while(*str != '\0')
	{
		send_byte(*str);
		str++;
	}
} 

void uart_init()
{
	PCON &= 0x7F;
	SCON = 0x50;
	
	TMOD &= 0x0F;
	TMOD |= 0x20;
	
	TH1 = 0xFD;
	TL1 = TH1;
	
	ET1 = 0;
	TR1 = 1;
	
	ES = 1;
	EA = 1;
}

#endif