/**
  ******************************************************************************
  * @file    main.c
  * @brief   STC89C52RC 电子密码锁 —— 主程序（任务轮询，无阻塞）
  *
  * 任务划分：
  *   Timer0 1ms 中断 ：数码管换位 / 蜂鸣器方波 / 键盘 10ms 采样
  *   串口 4   中断    ：收字节入环形缓冲 / 发字节出队列
  *   主循环          ：取行 -> 解析 -> 状态机 -> 回复入队，并刷新显示
  *
  * 主循环里没有任何 while 等待，因此“按键、串口、计时、显示”互相不干扰。
  ******************************************************************************
  */

#include "config.h"
#include "board.h"
#include "timer.h"
#include "uart.h"
#include "key.h"
#include "lock.h"

static char xdata line_buf[MAX_CMD_LEN + 1];
static unsigned char line_len;
static bit line_ovf;                /* 本行超长，丢弃至 LF 后回 BAD_CMD */

/**
  * @brief  逐字节送入行解析器
  * @note   仅 LF 作为结束符，CR 被忽略（因此 CRLF 不会触发两遍）。
  *         正文超过 16 字节即判定超长，本行全部丢弃。
  */
static void line_feed(unsigned char ch)
{
    if (ch == '\r') return;                 /* 兼容 CRLF，忽略 CR */

    if (ch == '\n')
    {
        if (line_ovf) Lock_ReplyBad();       /* 超长行：不执行其中任何部分 */
        else          Lock_SerialLine(line_buf, line_len);

        line_len = 0;
        line_ovf = 0;
        return;
    }

    if (line_ovf) return;
    if (line_len >= MAX_CMD_LEN)
    {
        line_ovf = 1;                        /* 第 17 个字节到来 */
        return;
    }
    line_buf[line_len++] = (char)ch;
}

int main(void)
{
    unsigned char ch;
    unsigned char k;
    unsigned int  last_disp;

    Board_Init();
    Uart_Init();
    Timer0_Init();
    Lock_Init();

    EA = 1;

    last_disp = Timer0_Millis();

    while (1)
    {
        /* 1. 时间相关：1 秒倒计时、超时上锁、ISP 软复位倒计时 */
        Lock_Poll();

        /* 2. 串口：取空环形缓冲，按行交给状态机 */
        while (Uart_Read(&ch))
        {
            line_feed(ch);
        }

        /* 3. 键盘：取一次按键事件（已消抖、按住只算一次） */
        k = Key_GetEvent();
        if (k) Lock_KeyEvent(k);

        /* 4. 显示：20ms 刷新一次，避免频繁关中断 */
        if ((unsigned int)(Timer0_Millis() - last_disp) >= 20)
        {
            last_disp = Timer0_Millis();
            Lock_Display();
        }
    }
}

/******************************* END OF FILE *********************************/
