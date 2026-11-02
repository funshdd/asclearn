#include "Uart.h"

#define UART_RX_SIZE 128
#define UART_TX_SIZE 64

static volatile unsigned char xdata rx_buf[UART_RX_SIZE];
static volatile unsigned char rx_head = 0;
static volatile unsigned char rx_tail = 0;

static volatile unsigned char xdata tx_buf[UART_TX_SIZE];
static volatile unsigned char tx_head = 0;
static volatile unsigned char tx_tail = 0;
static volatile bit tx_busy = 0;

void Send_Byte(unsigned char ch)
{
	unsigned char nh;
	unsigned char c;

	nh = (unsigned char)((tx_head + 1) & (UART_TX_SIZE - 1));

	while(nh == tx_tail)
	{
		nh = (unsigned char)((tx_head + 1) & (UART_TX_SIZE - 1));
	}

	tx_buf[tx_head] = ch;
	tx_head = nh;

	if(!tx_busy)
	{
		tx_busy = 1;
		c = tx_buf[tx_tail];
		tx_tail = (unsigned char)((tx_tail + 1) & (UART_TX_SIZE - 1));
		SBUF = c;
	}
}

void Send_String(unsigned char *str)
{
	while(*str != '\0')
	{
		Send_Byte(*str);
		str++;
	}
}

unsigned char Uart_GetByte(unsigned char *ch)
{
	if(rx_tail == rx_head)
	{
		return 0;
	}

	*ch = rx_buf[rx_tail];
	rx_tail = (unsigned char)((rx_tail + 1) & (UART_RX_SIZE - 1));

	return 1;
}

unsigned char Uart_TxIdle(void)
{
	if(tx_busy)
	{
		return 0;
	}

	if(tx_head != tx_tail)
	{
		return 0;
	}

	return 1;
}

void Uart_Init()
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

void Uart_Interrupt() interrupt 4
{
	unsigned char c;
	unsigned char nh;

	if(RI)
	{
		RI = 0;
		c = SBUF;
		nh = (unsigned char)((rx_head + 1) & (UART_RX_SIZE - 1));
		if(nh != rx_tail)
		{
			rx_buf[rx_head] = c;
			rx_head = nh;
		}
	}

	if(TI)
	{
		TI = 0;

		if(tx_tail != tx_head)
		{
			SBUF = tx_buf[tx_tail];
			tx_tail = (unsigned char)((tx_tail + 1) & (UART_TX_SIZE - 1));
		}
		else
		{
			tx_busy = 0;
		}
	}
}
