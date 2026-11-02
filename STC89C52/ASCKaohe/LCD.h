#ifndef __LCD_H
#define __LCD_H

#include <STC89C5xRC.H>
#include <intrins.h>

/************ LCD控制引脚定义 ************/
sbit LCD_RS = P2^6;     // 寄存器选择信号（0：指令寄存器 1：数据寄存器）
sbit LCD_RW = P2^5;     // 读写控制信号（0：写 1：读）
sbit LCD_EN = P2^7;     // 使能信号

/************ LCD数据端口定义 ************/
#define LCD_DATA_PORT	P0   // LCD数据总线（8位并行）

/************ LCD函数声明 ************/
void LCD_Delay(void); 
bit LCD_CheckBusy(void); 
void LCD_WriteInstruction(unsigned char command); 
void LCD_WriteData(unsigned char byte);  
void LCD_SetPos(unsigned char y, unsigned char x);
void LCD_Init(void);

void LCD_Exit(void);

void LCD_ShowChar(unsigned char y, unsigned char x, char char_data);
void LCD_ShowString(unsigned char y, unsigned char x, char *string_data);
void LCD_DisplayNumber(unsigned char y, unsigned char x, int number);


#endif /*__LCD_H*/