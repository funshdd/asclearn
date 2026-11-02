/**
  ******************************************************************************
  * @file    nixie.h
  * @brief   8 位共阴极数码管动态扫描
  ******************************************************************************
  */

#ifndef __NIXIE_H
#define __NIXIE_H

/* 常用段码（共阴极） */
#define SEG_0       0x3F
#define SEG_1       0x06
#define SEG_2       0x5B
#define SEG_3       0x4F
#define SEG_4       0x66
#define SEG_5       0x6D
#define SEG_6       0x7D
#define SEG_7       0x07
#define SEG_8       0x7F
#define SEG_9       0x6F

/* 0~9 段码表（定义在 nixie.c 的 code 区） */
extern const unsigned char code SEG_DIGIT[10];

/* 1ms 中断调用：切换到下一位并送出段码 */
void Nixie_Tick(void);

/* 提交 8 字节显示缓存（左起第 1~8 位），关中断拷贝避免撕裂 */
void Nixie_Commit(const unsigned char *seg8);

#endif /* __NIXIE_H */
