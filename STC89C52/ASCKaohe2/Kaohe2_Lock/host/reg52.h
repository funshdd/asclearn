/**
  ******************************************************************************
  * @file    reg52.h   （仅用于 PC 侧单元测试的替身头文件，不参与单片机编译）
  * @brief   把 Keil C51 的存储类型/位类型关键字抹平，让 lock.c 能在 PC 上编译
  ******************************************************************************
  */

#ifndef __HOST_REG52_H
#define __HOST_REG52_H

#define code
#define xdata
#define idata
#define data
#define pdata
#define bit     unsigned char
#define sbit    unsigned char
#define sfr     unsigned char
#define sfr16   unsigned int

/* 端口在主机测试里只用于 sbit 初始化，取值无所谓 */
#define P0      0
#define P1      0
#define P2      0
#define P3      0

#define EA      0
#define ES      0
#define RI      0
#define TI      0

#endif
