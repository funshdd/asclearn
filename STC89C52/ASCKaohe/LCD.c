/**
  ******************************************************************************
  * @file       bsp_lcd.c
  * @author     embedfire
  * @version     V1.0
  * @date        2025
  * @brief      16*2点阵的LCD显示屏驱动函数接口
  ******************************************************************************
  * @attention
  *
  * 实验平台  ：野火 STC89C52RC 开发板 
  * 论坛      ：http://www.firebbs.cn
  * 官网      ：https://embedfire.com/
  * 淘宝      ：https://yehuosm.tmall.com/
  *
  ******************************************************************************
  */
  
#include "LCD.h"

/**
  * @brief  LCD延时函数
  * @param  无
  * @note   用于LCD时序延时控制
  * @retval 无
  */
void LCD_Delay(void)
{
	//Delay20us(1);
	unsigned char data i;

	_nop_();
	i = 6;
	while (--i);
}
void Delay1ms(unsigned int number)	//@11.0592MHz
{
	unsigned char data i, j;

	while(number--)
	{
	_nop_();
	i = 2;
	j = 199;
	do
	{
		while (--j);
	} while (--i);
 }
}

/**
  * @brief  检测LCD忙状态
  * @param  无
  * @note   RS=0 RW=1读取忙标志D7
  * @retval busy 1表示忙，0表示空闲
  */
bit LCD_CheckBusy(void)
{
	bit data busy = 1;
	LCD_RS = 0;          // 指令寄存器选择
	LCD_RW = 1;          // 读模式
	LCD_EN = 1;          // 使能LCD
	LCD_Delay();         // 延时等待数据稳定
	busy = LCD_DATA_PORT & 0x80;  // 读取忙标志位D7
	LCD_EN = 0;          // 关闭使能
	return busy;
}

/**
  * @brief  写LCD指令
  * @param  command 指令字节
  * @note   写入前需等待LCD空闲
  * @retval 无
  */
void LCD_WriteInstruction(unsigned char command)
{
	while(LCD_CheckBusy());   // 等待LCD空闲
	LCD_RS = 0;               // 选择指令寄存器
	LCD_RW = 0;               // 写模式
	LCD_DATA_PORT = command;  // 写入指令
	LCD_EN = 1;               // 使能写入
	LCD_Delay();              // 保持数据
	LCD_EN = 0;               // 锁存数据
	LCD_Delay();              // 延时稳定
}

/**
  * @brief  写LCD数据
  * @param  byte 数据字节
  * @note   写入显示数据到当前地址
  * @retval 无
  */
void LCD_WriteData(unsigned char byte)
{
	while(LCD_CheckBusy());   // 等待LCD空闲
	LCD_RS = 1;               // 选择数据寄存器
	LCD_RW = 0;               // 写模式
	LCD_DATA_PORT = byte;     // 写入数据
	LCD_EN = 1;               // 使能写入
	LCD_Delay();              // 保持数据
	LCD_EN = 0;               // 锁存数据
	LCD_Delay();              // 延时稳定
}

/**
  * @brief  设置LCD显示坐标
  * @param  y 行（1或2）
  * @param  x 列（1~16）
  * @note   根据行列计算DDRAM地址
  * @retval 无
  */
void LCD_SetPos(unsigned char y, unsigned char x)
{
	if(y == 1)
		LCD_WriteInstruction(0x80 | (x-1));
	else
		LCD_WriteInstruction(0x80 | (x-1 + 0x40));
}

/**
  * @brief  LCD初始化函数
  * @param  无
  * @note   初始化为8位模式，显示开，光标关闭
  * @retval 无
  */
void LCD_Init(void)
{
	LCD_WriteInstruction(0x38);  // 8位数据，2行显示，5x7点阵
	Delay1ms(5);
	LCD_WriteInstruction(0x38);  // 重复初始化
	Delay1ms(5);
	LCD_WriteInstruction(0x38);  // 稳定设置

	LCD_WriteInstruction(0x0c);  // 显示开，光标关
	LCD_WriteInstruction(0x06);  // 写入后地址自动加1
	LCD_WriteInstruction(0x01);  // 清屏
	Delay1ms(5);
}

/**
  * @brief  LCD显示单个字符
  * @param  y 行
  * @param  x 列
  * @param  char_data 显示字符
  * @note   在指定位置显示一个字符
  * @retval 无
  */
void LCD_ShowChar(unsigned char y, unsigned char x, char char_data)
{
	LCD_SetPos(y,x);              // 设置显示位置
	LCD_WriteData(char_data);     // 写入字符
}

/**
  * @brief  LCD显示字符串
  * @param  y 行
  * @param  x 列
  * @param  string_data 字符串指针
  * @note   从指定位置连续显示字符串
  * @retval 无
  */
void LCD_ShowString(unsigned char y, unsigned char x, char *string_data)
{
	unsigned char data i = 0;
	LCD_SetPos(y,x);   // 设置起始位置

	for(i = 0; *string_data != '\0'; i++)
	{
		LCD_WriteData(*string_data++);   // 逐字符显示
	}
}

/**
  * @brief  LCD显示整数
  * @param  y 行（1或2）
  * @param  x 列（1~16）
  * @param  number 要显示的整数（支持负数）
  * @note   从指定位置开始显示，自动处理符号和0
  * @retval 无
  */
void LCD_DisplayNumber(unsigned char y, unsigned char x, int number)
{
	unsigned char data buf[8];       // 缓冲区，最大 -32768 共 6 位 + 符号
	unsigned char data i = 0;
	unsigned char data j = 0;
	unsigned char data isNeg = 0;
	unsigned int  data absNum;

	/* 处理负数 */
	if (number < 0)
	{
		isNeg = 1;
		absNum = (unsigned int)(-number);   // 取绝对值
	}
	else
	{
		absNum = (unsigned int)number;
	}

	/* 处理 0 的情况 */
	if (absNum == 0)
	{
		buf[i++] = '0';
	}
	else
	{
		/* 从低位到高位取出每一位，存入缓冲区 */
		while (absNum > 0)
		{
			buf[i++] = (absNum % 10) + '0';
			absNum /= 10;
		}
	}

	/* 设置显示位置 */
	LCD_SetPos(y, x);

	/* 先显示负号 */
	if (isNeg)
	{
		LCD_WriteData('-');
	}

	/* 逆序输出（因为刚才存的是低位在前） */
	for (j = i; j > 0; j--)
	{
		LCD_WriteData(buf[j - 1]);
	}
}

/*****************************END OF FILE***************************************/
