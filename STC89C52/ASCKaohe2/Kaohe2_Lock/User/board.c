/**
  ******************************************************************************
  * @file    board.c
  * @brief   板级初始化与声光反馈（全部非阻塞，由 1ms 节拍推进）
  *
  * 设计要点：
  *   1. 蜂鸣器与矩阵键盘第 3 行复用 P1.5：扫描时由 key.c 暂时拉高行线，
  *      扫描结束后调用 Feedback_RestoreBeep() 还原方波电平。
  *   2. 所有反馈只设置“剩余毫秒数”，真正的翻转在 1ms 中断里做，
  *      任何业务处理都不会被延时阻塞。
  ******************************************************************************
  */

#include "config.h"
#include "board.h"

static volatile unsigned char beep_left = 0;    /* 蜂鸣器剩余毫秒 */
static volatile unsigned char beep_level = 1;   /* 当前方波电平（静音时为 1） */
static volatile unsigned char led_left = 0;     /* 反馈灯剩余毫秒 */

/**
  * @brief  板级初始化：所有引脚置为安全初值
  */
void Board_Init(void)
{
    SEG_PORT = SEG_BLANK;       /* 段码全灭 */
    KEY_PORT = 0xFF;            /* 矩阵键盘行线全部上拉 */
    KEY_EN   = 0;               /* 使能矩阵键盘（同时屏蔽 RTC_RST） */
    BEEP     = 1;               /* 无源蜂鸣器静音 */
    LED_FB   = 1;               /* 反馈灯灭（低电平点亮） */

#if D8_ALWAYS_ON
    LED_D8 = 0;                 /* B 版：D8 常亮 */
#else
    LED_D8 = 1;                 /* A 版：D8 常暗 */
#endif
}

/**
  * @brief  1ms 节拍：推进蜂鸣器方波与反馈灯计时
  * @note   仅做位操作，不改动 P1/P2 其它位，因此不会干扰键盘扫描和位选
  */
void Feedback_Tick1ms(void)
{
    if (beep_left)
    {
#if BEEP_ACTIVE
        beep_level = 1;
#else
        beep_level ^= 1;            /* 1ms 翻转一次 => 500Hz 方波 */
#endif
        BEEP = beep_level;
        if (--beep_left == 0)
        {
            beep_level = 1;         /* 回到静音电平 */
            BEEP = 1;
        }
    }

    if (led_left)
    {
        if (--led_left == 0) LED_FB = 1;   /* 熄灭 */
    }
}

/**
  * @brief  触发蜂鸣器鸣叫
  * @param  ms: 持续毫秒数
  */
void Feedback_Beep(unsigned char ms)
{
    beep_left = ms;
}

/**
  * @brief  点亮反馈灯
  * @param  ms: 点亮毫秒数
  */
void Feedback_Led(unsigned char ms)
{
    LED_FB = 0;
    led_left = ms;
}

/**
  * @brief  按键反馈（短鸣 + 闪灯）
  */
void Feedback_Key(void)
{
    Feedback_Beep(BEEP_MS_KEY);
    Feedback_Led(LED_MS_FB);
}

/**
  * @brief  串口命令反馈（短鸣 + 闪灯）
  */
void Feedback_Command(void)
{
    Feedback_Beep(BEEP_MS_CMD);
    Feedback_Led(LED_MS_FB);
}

/**
  * @brief  恢复蜂鸣器输出电平（键盘扫描后调用）
  */
void Feedback_RestoreBeep(void)
{
    BEEP = beep_level;
}

/******************************* END OF FILE *********************************/
