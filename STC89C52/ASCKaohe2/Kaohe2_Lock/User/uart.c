/**
  ******************************************************************************
  * @file    uart.c
  * @brief   串口 9600/8N1：发送队列 + 接收环形缓冲
  *
  * 非阻塞设计：
  *   - 接收：中断只把字节塞进环形缓冲，主循环按行取用；
  *   - 发送：业务只把回复压入发送队列，中断逐字节送出；
  *   - 因此“收一条命令 -> 回一条回复”的全过程没有一个 while 等待，
  *     数码管扫描、按键采样、15 秒计时都不会被串口拖住。
  *
  * 缓冲放在片外 RAM（256B），把宝贵的 128B data 留给业务标量。
  ******************************************************************************
  */

#include "config.h"
#include "uart.h"

#define RX_SIZE     64          /* 一条命令最多 18 字节，64 足够抗突发 */
#define TX_SIZE     48          /* 最长回复约 31 字节 */
#define RX_NEXT(x)  (((x) + 1 >= RX_SIZE) ? 0 : (unsigned char)((x) + 1))
#define TX_NEXT(x)  (((x) + 1 >= TX_SIZE) ? 0 : (unsigned char)((x) + 1))

static volatile unsigned char xdata rx_buf[RX_SIZE];
static volatile unsigned char rx_head = 0;
static volatile unsigned char rx_tail = 0;

static volatile unsigned char xdata tx_buf[TX_SIZE];
static volatile unsigned char tx_head = 0;
static volatile unsigned char tx_tail = 0;
static volatile unsigned char tx_busy = 0;

/**
  * @brief  串口初始化：9600 8N1，波特率由 Timer1 模式 2 产生
  */
void Uart_Init(void)
{
    PCON &= 0x7F;           /* SMOD = 0 */
    SCON  = 0x50;           /* 模式 1，8 位 UART，允许接收 */
    TMOD &= 0x0F;
    TMOD |= 0x20;           /* T1：模式 2，8 位自动重装 */
    TH1   = 0xFD;           /* 11.0592MHz、SMOD=0 时为 9600bps */
    TL1   = 0xFD;
    TR1   = 1;

    RI = 0;
    TI = 0;
    ES = 1;
}

/**
  * @brief  从接收环形缓冲取一个字节
  * @retval 1 表示取到，0 表示暂无数据
  */
unsigned char Uart_Read(unsigned char *ch)
{
    if (rx_tail == rx_head) return 0;

    *ch = rx_buf[rx_tail];
    rx_tail = RX_NEXT(rx_tail);
    return 1;
}

/**
  * @brief  把数据压入发送队列并启动发送（立即返回，不等待）
  */
void Uart_Send(const unsigned char *buf, unsigned char len)
{
    unsigned char i;
    unsigned char nh;

    for (i = 0; i < len; i++)
    {
        nh = TX_NEXT(tx_head);
        if (nh == tx_tail) break;           /* 队列满：丢弃（正常不会发生） */
        tx_buf[tx_head] = buf[i];
        tx_head = nh;
    }

    /* 启动发送的瞬间必须与发送中断互斥，否则可能重复写 SBUF */
    ES = 0;
    if (!tx_busy)
    {
        tx_busy = 1;
        SBUF = tx_buf[tx_tail];
        tx_tail = TX_NEXT(tx_tail);
    }
    ES = 1;
}

/**
  * @brief  发送 code 区常量字符串
  */
void Uart_SendStr(const char code *s)
{
    unsigned char nh;

    for (;;)
    {
        unsigned char c = (unsigned char)*s;
        if (c == 0) break;
        s++;

        nh = TX_NEXT(tx_head);
        if (nh == tx_tail) break;
        tx_buf[tx_head] = c;
        tx_head = nh;
    }

    ES = 0;
    if (!tx_busy)
    {
        tx_busy = 1;
        SBUF = tx_buf[tx_tail];
        tx_tail = TX_NEXT(tx_tail);
    }
    ES = 1;
}

/**
  * @brief  发送是否彻底完成（队列空且当前字节已送出）
  */
unsigned char Uart_TxIdle(void)
{
    if (tx_busy) return 0;
    if (tx_head != tx_tail) return 0;
    return 1;
}

/**
  * @brief  串口中断：收字节入环形缓冲，发字节出队列
  */
void Uart_ISR(void) interrupt 4
{
    unsigned char c;
    unsigned char nh;

    if (RI)
    {
        RI = 0;
        c = SBUF;
        nh = RX_NEXT(rx_head);
        if (nh != rx_tail)                  /* 缓冲满则丢弃，不会死等 */
        {
            rx_buf[rx_head] = c;
            rx_head = nh;
        }
    }

    if (TI)
    {
        TI = 0;
        if (tx_tail != tx_head)
        {
            SBUF = tx_buf[tx_tail];
            tx_tail = TX_NEXT(tx_tail);
        }
        else
        {
            tx_busy = 0;
        }
    }
}

/******************************* END OF FILE *********************************/
