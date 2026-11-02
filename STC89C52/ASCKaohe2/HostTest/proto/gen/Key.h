#ifndef __KEY_H
#define __KEY_H

#include <STC89C5xRC.H>

/* 矩阵键盘使能脚：P2.0 接 RTC 模块的 RST。
 * 用矩阵键盘时必须拉低，否则 P1.0 / P1.4（列1、行4 的复用脚）
 * 受 RTC 模块影响，扫描读到的行线电平是错的。 */
#define KEY_EN g_b2_0

#define KEY_INDEX_ROW_MAJOR   1

#define KEY_NONE      0
#define KEY_1         1
#define KEY_2         2
#define KEY_3         3
#define KEY_OK        4
#define KEY_4         5
#define KEY_5         6
#define KEY_6         7
#define KEY_DEL       8
#define KEY_7         9
#define KEY_8         10
#define KEY_9         11
#define KEY_CHG       12
#define KEY_CLR       13
#define KEY_0         14
#define KEY_CAN       15
#define KEY_LOCK      16

void Key_Init(void);

void Key_Tick(void);

unsigned char Key_GetEvent(void);

unsigned char Key_ScanRaw(void);

/* 最近一次扫描到的原始键值（0 = 没有键），调试用 */
unsigned char Key_GetRaw(void);

#endif
