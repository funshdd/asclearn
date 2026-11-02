/**
  ******************************************************************************
  * @file    board.h
  * @brief   板级初始化、声光反馈
  ******************************************************************************
  */

#ifndef __BOARD_H
#define __BOARD_H

void Board_Init(void);

/* 1ms 节拍中调用：推进蜂鸣器方波与反馈灯计时 */
void Feedback_Tick1ms(void);

/* 触发一次声光反馈（不阻塞） */
void Feedback_Beep(unsigned char ms);
void Feedback_Led(unsigned char ms);
void Feedback_Key(void);        /* 按键反馈 */
void Feedback_Command(void);    /* 串口命令反馈 */

/* 按键扫描期间保持行线上拉后，恢复蜂鸣器电平 */
void Feedback_RestoreBeep(void);

#endif /* __BOARD_H */
