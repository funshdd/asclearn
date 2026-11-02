#ifndef __LEDMAT_H
#define __LEDMAT_H
#include <STC89C5xRC.H>

sbit SER	= P3^4;  //SER
sbit RCLK_	= P3^5;  //RCLK
sbit SRCLK	= P3^6;  //SRCLK

#define LED_MATRIX_ALL_ON	0xff
#define LED_MATRIX_ALL_OFF	0x00

void _74HC595_WriteByte(unsigned char byte)
{
	unsigned char data i = 0;
	EA = 0;

	for(i = 0;i < 8;i++)
	{
		SER = (byte & 0x80) ? 1  : 0;  // 取出当前最高位输出到串行数据引脚DS
		byte <<= 1;  // 数据左移，准备下一位
		
		SRCLK = 1;  // 移位时钟拉高，锁存当前DS电平
		SRCLK = 0;  // 移位时钟拉低，完成1bit移位
	}
	RCLK_ = 1;  // 存储锁存时钟拉高，将移位寄存器数据打入输出锁存器
	RCLK_ = 0;  // 存储锁存时钟拉低，锁存完成

	EA = 1;
}

void led_matrix_init(void)
{
    P1 = 0xFF;
    _74HC595_WriteByte(LED_MATRIX_ALL_OFF);
}

void led_matrix_showcolumn(unsigned char col, unsigned char row_data)
{
    P0 = 0xFF;
    _74HC595_WriteByte(row_data);
    P0 = ~(0x01 << col);
}

#endif