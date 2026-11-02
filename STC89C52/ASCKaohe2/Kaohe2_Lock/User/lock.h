/**
  ******************************************************************************
  * @file    lock.h
  * @brief   密码锁业务：状态机 + 串口协议 + 显示 + 键盘
  ******************************************************************************
  */

#ifndef __LOCK_H
#define __LOCK_H

/* 上电初始化（从 EEPROM 恢复密码、错误次数、限制阶段） */
void Lock_Init(void);

/* 主循环轮询：1 秒计时 + 超时跳转 + ISP 软复位倒计时 */
void Lock_Poll(void);

/* 刷新数码管显示缓存（建议 10~20ms 调用一次） */
void Lock_Display(void);

/* 处理一条完整的串口命令行（不含换行符），内部完成回复发送 */
void Lock_SerialLine(const char *line, unsigned char len);

/* 非法 / 超长行：回 BAD_CMD */
void Lock_ReplyBad(void);

/* 处理一次矩阵键盘事件（键值见 key.h） */
void Lock_KeyEvent(unsigned char k);

#endif /* __LOCK_H */
