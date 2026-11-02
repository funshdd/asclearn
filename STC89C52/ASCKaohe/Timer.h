#ifndef __TIMER_H
#define __TIMER_H

#include <STC89C5xRC.H>

void Timer0_Init(void)		// 1毫秒@11.0592MHz
{
	AUXR &= 0x7F;			// 定时器时钟12T模式
	TMOD &= 0xF0;			// 清除定时器0的模式位
	TMOD |= 0x01;			// 设置定时器0为模式1（16位定时器）
	TL0 = 0x66;				// 设置定时初值低8位
	TH0 = 0xFC;				// 设置定时初值高8位
	
	EA = 1;					// 开启总中断
	ET0 = 1;				// 允许定时器0中断
	
	TF0 = 0;				// 清除定时器0溢出标志
	TR0 = 1;				// 启动定时器0
}

#endif