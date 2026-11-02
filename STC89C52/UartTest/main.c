#include <STC89C5xRC.H>
#include <intrins.h>

void UART_SendByte(unsigned char ch)
{
	SBUF = ch;			// 将数据写入串口发送缓冲寄存器
	while(!TI);			// 等待发送完成标志位TI置位
	TI = 0;				// 清除发送完成标志位
}

void UART_SendString(unsigned char *str)
{
	while(*str != '\0')	// 判断是否到达字符串结束符
	{
		UART_SendByte(*str);	// 发送当前字符
		str++;					// 指针指向下一个字符
	}
}
void UART_SendArray(unsigned char *array , unsigned int num)
{
	unsigned int i = 0;		// 定义循环计数变量
	
	for(i = 0; i < num; i++)	// 按指定长度依次发送数组数据
	{
		UART_SendByte(array[i]);	// 发送数组中的一个字节
	}
}
char putchar(char ch)
{
	/* 调用串口发送函数，将字符通过串口发送出去 */
	UART_SendByte(ch);

	/* 返回已发送的字符，符合 putchar 函数规范 */
    return ch;
}
void UART_Init(void)	// 9600bps@11.0592MHz —— 波特率不宜过高
{
	/* 串口1默认使用定时器1作为波特率发生器 */
	
	PCON &= 0x7F;		// 波特率不倍速（SMOD = 0）
	SCON  = 0x50;		// 串口模式1：8位数据，可变波特率，允许收发
	TMOD &= 0x0F;		// 清除定时器1模式配置位
	TMOD |= 0x20;		// 设置定时器1为方式2（8位自动重装）
	
	TH1 = 0xFD;			// 设置定时器1重装值，用于产生9600bps波特率
	TL1 = TH1;			// 设置定时器1初始计数值
	
	ET1 = 0;			// 禁止定时器1中断，仅作为波特率发生器使用
	TR1 = 1;			// 启动定时器1
}


/*****************************END OF FILE***************************************/

void Delay50ms(void)	//@11.0592MHz
{
	unsigned char data i, j, k;

	_nop_();
	_nop_();
	i = 3;
	j = 26;
	k = 223;
	do
	{
		do
		{
			while (--k);
		} while (--j);
	} while (--i);
}

void Delay20us(void)	//@11.0592MHz
{
	unsigned char data i;

	_nop_();
	_nop_();
	_nop_();
	i = 52;
	while (--i);
}


sbit KEY3 = P3^2;
void main()
{
	UART_Init();
	
	Delay50ms();
	
	while(1)
	{
		if(KEY3==0)
		{
			Delay20us();
			while(!KEY3);
			Delay20us();
			
			UART_SendString("11");
		}
	}
	
	UART_SendString("asdasdasdasd");
}
