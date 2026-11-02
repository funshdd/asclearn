/**
  ******************************************************************************
  * @file    timer.c
  * @brief   Timer0 1ms 系统节拍（全系统唯一的时间基准）
  *
  * 中断里只做三件事：
  *   1. 数码管换位（8 位动态扫描，1ms/位 => 125Hz 刷新，无闪烁）
  *   2. 蜂鸣器方波 / 反馈灯计时
  *   3. 矩阵键盘 10ms 采样（内部消抖，非阻塞）
  * 任何一项都不含 while 等待，因此不会拖慢串口与业务处理。
  ******************************************************************************
  */

#include "config.h"
#include "timer.h"
#include "nixie.h"
#include "key.h"
#include "board.h"

/* 11.0592MHz / 12T：1ms = 921.6 个机器周期 => 65536-922 = 0xFC66 */
#define T0_RELOAD_H     0xFC
#define T0_RELOAD_L     0x66

static volatile unsigned int ms_tick = 0;

void Timer0_Init(void)
{
    TMOD &= 0xF0;           /* 保留 T1 的波特率模式设置 */
    TMOD |= 0x01;           /* T0：模式 1，16 位定时 */
    TH0 = T0_RELOAD_H;
    TL0 = T0_RELOAD_L;
    ET0 = 1;
    TR0 = 1;
}

unsigned int Timer0_Millis(void)
{
    unsigned int v;
    bit ea = EA;

    EA = 0;
    v = ms_tick;
    EA = ea;
    return v;
}

void Timer0_ISR(void) interrupt 1
{
    TH0 = T0_RELOAD_H;
    TL0 = T0_RELOAD_L;

    ms_tick++;

    Nixie_Tick();           /* 数码管换位 */
    Feedback_Tick1ms();     /* 蜂鸣器 / 反馈灯 */
    Key_Tick10ms();         /* 分频为 10ms 的键盘采样 */
}

/******************************* END OF FILE *********************************/
