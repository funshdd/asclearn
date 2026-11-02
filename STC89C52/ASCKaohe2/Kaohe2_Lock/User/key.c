/**
  ******************************************************************************
  * @file    key.c
  * @brief   4x4 矩阵键盘扫描 + 消抖 + 边沿检测
  *
  * 硬件特点（野火 STC89C52RC 板）：
  *   P1.0~P1.3 为列（低电平扫描输出），P1.4~P1.7 为行（输入，内部上拉）。
  *   行 1 = P1.7，行 2 = P1.6，行 3 = P1.5，行 4 = P1.4；
  *   其中 P1.5 同时是蜂鸣器，因此扫描期间必须把行线强制拉高，
  *   读完立刻调用 Feedback_RestoreBeep() 还原蜂鸣器的方波电平。
  *
  * 非阻塞：整轮扫描约 30us，全部在 10ms 节拍里完成，没有 while 等松手。
  ******************************************************************************
  */

#include "config.h"
#include "key.h"
#include "board.h"

static const unsigned char code COL_PAT[4]  = {0xF7, 0xFB, 0xFD, 0xFE};
static const unsigned char code ROW_MASK[4] = {0x80, 0x40, 0x20, 0x10};

static unsigned char key_last  = 0;     /* 上一次原始键值 */
static unsigned char key_cnt   = 0;     /* 稳定计数 */
static bit           key_held  = 0;     /* 已上报，等待松手 */
static volatile unsigned char key_event = 0;
static unsigned char tick_div  = 0;

/**
  * @brief  扫描一轮矩阵键盘
  * @retval 0 = 无键按下，1~16 = 键号（行优先编号，与题面表格一致）
  *
  * 键号 = 行 * 4 + 列 + 1，即：
  *   行1(P1.7)：S1 =1   S2 =2   S3 =3   S4 =确认
  *   行2(P1.6)：S5 =4   S6 =5   S7 =6   S8 =删除
  *   行3(P1.5)：S9 =7   S10=8   S11=9   S12=修改
  *   行4(P1.4)：S13=清空 S14=0   S15=取消 S16=上锁
  */
static unsigned char key_scan_raw(void)
{
    unsigned char col, rows, i, val = 0;

    BEEP = 1;                           /* 行 3 与蜂鸣器复用：先强制上拉 */
    for (col = 0; col < 4; col++)
    {
        KEY_PORT = COL_PAT[col];        /* 低四位：选中一列为低，其余为高 */
        rows = KEY_PORT & 0xF0;         /* 读高四位行线 */
        if (rows != 0xF0)
        {
            for (i = 0; i < 4; i++)
            {
                if (!(rows & ROW_MASK[i]))
                {
#if KEY_INDEX_ROW_MAJOR
                    val = (unsigned char)(i * 4 + col + 1);     /* 行优先 */
#else
                    val = (unsigned char)(col * 4 + i + 1);     /* 列优先（C51 参考例程的写法） */
#endif
                }
            }
        }
    }
    KEY_PORT = 0xFF;                    /* 全部行线恢复上拉 */
    Feedback_RestoreBeep();             /* 还原蜂鸣器电平 */
    return val;
}

/**
  * @brief  10ms 节拍：采样 + 消抖 + 边沿触发
  * @note   连续两次采样一致（20ms）才认可；松手后 key_held 清零，
  *         因此长按只会产生一次事件。
  */
void Key_Tick10ms(void)
{
    unsigned char raw;

    if (++tick_div < 10) return;
    tick_div = 0;

    raw = key_scan_raw();

    if (raw != key_last)
    {
        key_last = raw;
        key_cnt  = 0;
        return;
    }

    if (key_cnt < 3)
    {
        key_cnt++;
        if (key_cnt == 2)
        {
            if (raw != 0)
            {
                if (!key_held)
                {
                    key_held  = 1;
                    key_event = raw;
                }
            }
            else
            {
                key_held = 0;
            }
        }
    }
}

/**
  * @brief  取一次按键事件（取走后清除）
  */
unsigned char Key_GetEvent(void)
{
    unsigned char e;
    bit ea = EA;

    EA = 0;
    e = key_event;
    key_event = 0;
    EA = ea;
    return e;
}

/******************************* END OF FILE *********************************/
