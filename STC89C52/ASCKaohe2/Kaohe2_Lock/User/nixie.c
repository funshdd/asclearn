/**
  ******************************************************************************
  * @file    nixie.c
  * @brief   8 位共阴极数码管动态扫描
  *
  * 位选由 74HC138 完成（P2.2/P2.3/P2.4），段码走 P0。扫描放在 1ms 节拍里，
  * 每位点亮 1ms、8 位一循环（125Hz），并且换位前先熄灭段码以消影。
  * 全流程没有阻塞延时，主循环随时可以写显示缓存。
  ******************************************************************************
  */

#include "config.h"
#include "nixie.h"

/* 0~9 段码表放在 code 区，不占用宝贵的 128 字节 data 空间 */
const unsigned char code SEG_DIGIT[10] =
{
    SEG_0, SEG_1, SEG_2, SEG_3, SEG_4, SEG_5, SEG_6, SEG_7, SEG_8, SEG_9
};

static volatile unsigned char nix_buf[8];   /* 8 位显示缓存 */
static unsigned char nix_pos = 0;           /* 当前扫描位 */

/**
  * @brief  1ms 中断调用：换到下一位并输出段码
  * @note   先消影再换位，避免数码管出现重影
  */
void Nixie_Tick(void)
{
    unsigned char v;

    SEG_PORT = SEG_BLANK;                   /* 消影 */

    /* 位序 1~8 对应的 138 编码：位 1 => CBA=111，位 8 => CBA=000 */
    v = (unsigned char)(7 - nix_pos);
    NIX_A = (v & 0x01);
    NIX_B = (v & 0x02);
    NIX_C = (v & 0x04);

    SEG_PORT = nix_buf[nix_pos];            /* 送段码 */

    if (++nix_pos >= 8) nix_pos = 0;
}

/**
  * @brief  提交显示缓存（左起第 1 位到第 8 位）
  * @param  seg8: 8 字节段码
  * @note   关中断拷贝，避免扫描到一半时读到新旧混合的内容
  */
void Nixie_Commit(const unsigned char *seg8)
{
    unsigned char i;
    bit ea = EA;

    EA = 0;
    for (i = 0; i < 8; i++) nix_buf[i] = seg8[i];
    EA = ea;
}

/******************************* END OF FILE *********************************/
