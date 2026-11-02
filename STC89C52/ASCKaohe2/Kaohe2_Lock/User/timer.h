/**
  ******************************************************************************
  * @file    timer.h
  * @brief   Timer0 1ms 系统节拍
  ******************************************************************************
  */

#ifndef __TIMER_H
#define __TIMER_H

/* 初始化 Timer0 为 1ms 中断（11.0592MHz 晶振） */
void Timer0_Init(void);

/* 读取系统毫秒计数（关中断读取，避免 16 位读撕裂） */
unsigned int Timer0_Millis(void);

#endif /* __TIMER_H */
