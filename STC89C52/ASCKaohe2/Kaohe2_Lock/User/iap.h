/**
  ******************************************************************************
  * @file    iap.h
  * @brief   片内 EEPROM（ISP/IAP）持久化：密码、错误次数、限制阶段
  ******************************************************************************
  */

#ifndef __IAP_H
#define __IAP_H

#define REC_MAGIC0      0x4C        /* 'L' */
#define REC_MAGIC1      0x4B        /* 'K' */
#define REC_VER         0x01
#define REC_SIZE        16

/* 持久化记录：密码 6 位（0~9，天然保留前导零）+ 错误次数 + 限制阶段 */
typedef struct
{
    unsigned char magic0;
    unsigned char magic1;
    unsigned char ver;
    unsigned char pwd[6];
    unsigned char err;
    unsigned char stage;        /* 0 = 正常, 1 = 暂停(WAIT), 2 = 保护(PROTECT) */
    unsigned char seq;
    unsigned char chk;
    unsigned char res[3];
} PersistRec;

/* 上电读取：返回 1 表示读到有效记录 */
unsigned char Iap_Load(PersistRec *out);

/* 保存（只写空槽，写满后轮换到另一个扇区，掉电不会丢旧值） */
unsigned char Iap_Save(PersistRec *rec);

/* 软复位进入芯片 ISP 引导区（不断电升级用），不返回 */
void Iap_SoftResetToIsp(void);

#endif /* __IAP_H */
