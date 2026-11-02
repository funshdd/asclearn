/**
  ******************************************************************************
  * @file    key.h
  * @brief   4x4 矩阵键盘：10ms 采样 + 边沿触发（按住只算一次）
  ******************************************************************************
  */

#ifndef __KEY_H
#define __KEY_H

/* 键值定义（S1~S16 与题目表格一致） */
#define KEY_1       1
#define KEY_2       2
#define KEY_3       3
#define KEY_OK      4
#define KEY_4       5
#define KEY_5       6
#define KEY_6       7
#define KEY_DEL     8
#define KEY_7       9
#define KEY_8       10
#define KEY_9       11
#define KEY_CHG     12
#define KEY_CLR     13
#define KEY_0       14
#define KEY_CAN     15
#define KEY_LOCK    16

/* 10ms 节拍调用（由 Timer0 中断分频） */
void Key_Tick10ms(void);

/* 取一次按键事件，0 表示无事件（取走后自动清除） */
unsigned char Key_GetEvent(void);

#endif /* __KEY_H */
