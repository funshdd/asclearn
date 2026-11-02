/**
  ******************************************************************************
  * @file    test_lock.c
  * @brief   密码锁业务状态机的 PC 侧单元测试（gcc 编译，直接 #include lock.c）
  *
  * 目的：在没有硬件的情况下，逐条回放题面第 7 节的 10 组示例，
  *       并额外覆盖超时、断电保留、改密、拒绝路径等边界。
  *
  * 编译： gcc -I host -o host/test_lock.exe host/test_lock.c
  ******************************************************************************
  */

#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include "../User/config.h"

#include "../User/uart.h"
#include "../User/board.h"
#include "../User/timer.h"
#include "../User/nixie.h"
#include "../User/key.h"
#include "../User/iap.h"

/* ------------------------- 被替换的硬件接口桩 ------------------------- */

static char          g_tx[64];          /* 最近一条串口回复 */
static unsigned char g_display[8];      /* 最近一次数码管内容 */
static unsigned int  g_ms;              /* 模拟毫秒时钟 */
static int           g_fb_count;        /* 声光反馈次数 */

static PersistRec    g_store;           /* 模拟 EEPROM */
static int           g_store_valid = 0;
static jmp_buf       g_isp_jmp;
static int           g_isp_reset = 0;

const unsigned char SEG_DIGIT[10] =
{
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

void Uart_Send(const unsigned char *buf, unsigned char len)
{
    memcpy(g_tx, buf, len);
    g_tx[len] = '\0';
}

void Uart_SendStr(const char code *s)
{
    strcpy(g_tx, s);
}

unsigned char Uart_TxIdle(void) { return 1; }

unsigned int Timer0_Millis(void) { return g_ms; }

void Nixie_Commit(const unsigned char *seg8)
{
    memcpy(g_display, seg8, 8);
}

void Board_Init(void) { }
void Feedback_Tick1ms(void) { }
void Feedback_Beep(unsigned char ms) { (void)ms; }
void Feedback_Led(unsigned char ms) { (void)ms; }
void Feedback_RestoreBeep(void) { }
void Feedback_Key(void) { g_fb_count++; }
void Feedback_Command(void) { g_fb_count++; }

unsigned char Iap_Load(PersistRec *out)
{
    if (!g_store_valid) return 0;
    *out = g_store;
    return 1;
}

unsigned char Iap_Save(PersistRec *rec)
{
    g_store = *rec;
    g_store_valid = 1;
    return 1;
}

void Iap_SoftResetToIsp(void)
{
    g_isp_reset = 1;
    longjmp(g_isp_jmp, 1);              /* 真机上这里是软复位，测试里跳回来 */
}

#include "../User/lock.c"

/* ----------------------------- 测试框架 ----------------------------- */

static int g_fail = 0;
static int g_pass = 0;

static void cmd(const char *line)
{
    Lock_SerialLine(line, (unsigned char)strlen(line));
}

static void expect(const char *line, const char *want)
{
    cmd(line);
    if (strcmp(g_tx, want) != 0)
    {
        printf("  FAIL  <- %-18s\n        got  : %s\n        want : %s\n",
               line, g_tx, want);
        g_fail++;
    }
    else
    {
        g_pass++;
    }
}

static void expect_str(const char *what, const char *got, const char *want)
{
    if (strcmp(got, want) != 0)
    {
        printf("  FAIL  %s\n        got  : %s\n        want : %s\n",
               what, got, want);
        g_fail++;
    }
    else
    {
        g_pass++;
    }
}

static void expect_int(const char *what, int got, int want)
{
    if (got != want)
    {
        printf("  FAIL  %s : got %d, want %d\n", what, got, want);
        g_fail++;
    }
    else
    {
        g_pass++;
    }
}

/* 复位到指定初始状态（相当于重新上电） */
static void power_on(const unsigned char *pwd, unsigned char err,
                     unsigned char stage)
{
    if (pwd)
    {
        memcpy(g_store.pwd, pwd, 6);
        g_store_valid = 1;
    }
    else
    {
        g_store_valid = 0;
        memset(&g_store, 0, sizeof(g_store));
    }
    g_store.err = err;
    g_store.stage = stage;
    g_ms = 0;
    g_isp_reset = 0;
    Lock_Init();
}

static const unsigned char PWD_DEF[6] = {0, 1, 2, 3, 4, 5};

/* 连续发送若干 DIGIT */
static void digits(const char *s)
{
    char buf[8];
    while (*s)
    {
        buf[0] = 'D'; buf[1] = 'I'; buf[2] = 'G'; buf[3] = 'I'; buf[4] = 'T';
        buf[5] = ' ';
        buf[6] = *s++;
        buf[7] = '\0';
        cmd(buf);
    }
}

static void advance_ms(unsigned int ms)
{
    g_ms = (unsigned int)(g_ms + ms);
    Lock_Poll();
}

/* ------------------------------- 用例 ------------------------------- */

static void t_examples(void)
{
    printf("[1] 题面 10 组示例\n");

    /* 1. 输入一位数字 */
    power_on(PWD_DEF, 0, 0);
    expect("DIGIT 0", "OK LOCKED 1 0 0 0\r\n");

    /* 2. 不足六位就确认 */
    power_on(PWD_DEF, 0, 0);
    digits("012");
    expect("OK", "ERR INCOMPLETE LOCKED 3 0 0 0\r\n");
    expect("STATUS", "OK LOCKED 3 0 0 0\r\n");           /* 输入保留、不计错 */

    /* 3. 六位已满继续输入 */
    power_on(PWD_DEF, 0, 0);
    digits("012345");
    expect("DIGIT 9", "ERR FULL LOCKED 6 0 0 0\r\n");
    expect("STATUS", "OK LOCKED 6 0 0 0\r\n");           /* 不得覆盖 */

    /* 4. 密码正确 */
    power_on(PWD_DEF, 2, 0);
    digits("012345");
    expect("OK", "OK UNLOCKED 0 0 0 15\r\n");

    /* 5. 第三次输错 */
    power_on(PWD_DEF, 2, 0);
    digits("999999");
    expect("OK", "ERR WRONG WAIT 0 3 30 0\r\n");

    /* 6. 第五次输错 */
    power_on(PWD_DEF, 4, 0);
    digits("999999");
    expect("OK", "ERR WRONG PROTECT 0 5 60 0\r\n");

    /* 7. 暂停期间要求上锁 */
    power_on(PWD_DEF, 3, 1);
    advance_ms(7000);                                     /* 30 -> 23 */
    expect("LOCK", "OK WAIT 0 3 23 0\r\n");
    expect("STATUS", "OK WAIT 0 3 23 0\r\n");            /* 不重新计时 */

    /* 8. 第一次新密码输入完成 */
    power_on(PWD_DEF, 0, 0);
    digits("012345");
    cmd("OK");
    cmd("CHANGE");
    digits("654321");
    expect("OK", "OK NEW2 0 0 0 15\r\n");

    /* 9. 两次新密码不一致 */
    digits("654320");
    expect("OK", "ERR MISMATCH UNLOCKED 0 0 0 15\r\n");
    /* 正式密码仍是 012345 */
    expect("LOCK", "OK LOCKED 0 0 0 0\r\n");
    digits("012345");
    expect("OK", "OK UNLOCKED 0 0 0 15\r\n");

    /* 10. 解锁后申请 ISP */
    expect("ISP", "OK ISP\r\n");
}

static void t_timeout(void)
{
    printf("[2] 15s 自动上锁 / 30s 暂停 / 60s 保护\n");

    /* 解锁后 15 秒静默上锁，且不主动回复 */
    power_on(PWD_DEF, 2, 0);
    digits("012345");
    cmd("OK");
    strcpy(g_tx, "<none>");
    advance_ms(14900);
    expect("STATUS", "OK UNLOCKED 0 0 0 1\r\n");         /* 还剩 1 秒（T 字段） */
    strcpy(g_tx, "<none>");
    advance_ms(200);
    expect_str("超时后不主动回复", g_tx, "<none>");

    /* 暂停 30 秒到期：保留错误次数 3 */
    power_on(PWD_DEF, 2, 0);
    digits("999999");
    cmd("OK");                                            /* -> WAIT 30 */
    advance_ms(30000);
    expect("STATUS", "OK LOCKED 0 3 0 0\r\n");           /* 状态码 0，错误次数 3 */

    /* 保护 60 秒到期：错误次数清零 */
    power_on(PWD_DEF, 4, 0);
    digits("999999");
    cmd("OK");                                            /* -> PROTECT 60 */
    advance_ms(60000);
    expect("STATUS", "OK LOCKED 0 0 0 0\r\n");

    /* 有效操作刷新 15 秒，查询不刷新 */
    power_on(PWD_DEF, 0, 0);
    digits("012345");
    cmd("OK");
    advance_ms(10000);
    cmd("CHANGE");                                        /* 有效操作，刷新 */
    advance_ms(10000);
    expect("STATUS", "OK NEW1 0 0 0 5\r\n");             /* 还剩 5 秒 */

    /* 查询不刷新 */
    power_on(PWD_DEF, 0, 0);
    digits("012345");
    cmd("OK");
    advance_ms(10000);
    cmd("STATUS");
    cmd("VERSION");
    advance_ms(5000);
    expect("STATUS", "OK LOCKED 0 0 0 0\r\n");           /* 已被上锁 */
}

static void t_persist(void)
{
    unsigned char np[6] = {6, 5, 4, 3, 2, 1};
    unsigned char err[1] = {0};

    printf("[3] 改密 + 断电保留\n");

    /* 双次一致才保存，保留前导零 */
    power_on(PWD_DEF, 0, 0);
    digits("012345");
    cmd("OK");
    cmd("CHANGE");
    digits("000123");
    expect("OK", "OK NEW2 0 0 0 15\r\n");
    digits("000123");
    expect("OK", "OK UNLOCKED 0 0 0 15\r\n");

    /* 断电重启：新密码仍有效（前导零不丢） */
    g_ms = 0;
    Lock_Init();
    expect("STATUS", "OK LOCKED 0 0 0 0\r\n");
    digits("000123");
    expect("OK", "OK UNLOCKED 0 0 0 15\r\n");

    /* 改密未完成（只有 NEW1）时不提交 */
    cmd("CHANGE");
    digits("111111");
    cmd("OK");                                            /* -> NEW2，未提交 */
    g_ms = 0;
    Lock_Init();                                          /* 重启 */
    expect("STATUS", "OK LOCKED 0 0 0 0\r\n");
    digits("000123");
    expect("OK", "OK UNLOCKED 0 0 0 15\r\n");

    /* 错误次数与阶段断电保留 */
    power_on(np, 2, 0);
    digits("000000");
    expect("OK", "ERR WRONG WAIT 0 3 30 0\r\n");
    g_ms = 0;
    Lock_Init();                                          /* 重启：重新等 30 秒 */
    expect("STATUS", "OK WAIT 0 3 30 0\r\n");

    power_on(np, 4, 0);
    digits("000000");
    expect("OK", "ERR WRONG PROTECT 0 5 60 0\r\n");
    g_ms = 0;
    Lock_Init();
    expect("STATUS", "OK PROTECT 0 5 60 0\r\n");

    (void)err;
}

static void t_matrix(void)
{
    printf("[4] 指令 x 状态 全表抽查\n");

    /* LOCKED：DEL / CLEAR 对空输入成功 */
    power_on(PWD_DEF, 0, 0);
    expect("DEL", "OK LOCKED 0 0 0 0\r\n");
    expect("CLEAR", "OK LOCKED 0 0 0 0\r\n");
    digits("12");
    expect("DEL", "OK LOCKED 1 0 0 0\r\n");
    expect("CLEAR", "OK LOCKED 0 0 0 0\r\n");

    /* LOCKED：CANCEL 原态，输入保留 */
    digits("12");
    expect("CANCEL", "OK LOCKED 2 0 0 0\r\n");

    /* UNLOCKED：只允许 CHANGE / LOCK / STATUS / VERSION / CANCEL / ISP */
    power_on(PWD_DEF, 0, 0);
    digits("012345");
    cmd("OK");
    expect("DIGIT 5", "ERR DENIED UNLOCKED 0 0 0 15\r\n");
    expect("OK", "ERR DENIED UNLOCKED 0 0 0 15\r\n");
    expect("DEL", "ERR DENIED UNLOCKED 0 0 0 15\r\n");
    expect("CLEAR", "ERR DENIED UNLOCKED 0 0 0 15\r\n");
    expect("CHANGE", "OK NEW1 0 0 0 15\r\n");

    /* NEW1：CANCEL 回 UNLOCKED 并丢弃暂存 */
    expect("CANCEL", "OK UNLOCKED 0 0 0 15\r\n");

    /* NEW1：LOCK 丢弃暂存回 LOCKED */
    cmd("CHANGE");
    expect("LOCK", "OK LOCKED 0 0 0 0\r\n");
    expect("ISP", "ERR DENIED LOCKED 0 0 0 0\r\n");

    /* WAIT：只允许查询与 LOCK（原态） */
    power_on(PWD_DEF, 2, 0);
    digits("999999");
    cmd("OK");                                            /* WAIT 30 */
    expect("DIGIT 1", "ERR DENIED WAIT 0 3 30 0\r\n");
    expect("CANCEL", "ERR DENIED WAIT 0 3 30 0\r\n");
    expect("CHANGE", "ERR DENIED WAIT 0 3 30 0\r\n");
    expect("LOCK", "OK WAIT 0 3 30 0\r\n");

    /* 第 4 次错误：仍锁定，不进入限制 */
    power_on(PWD_DEF, 2, 0);
    digits("999999");
    cmd("OK");                                            /* err=3 WAIT */
    advance_ms(30000);                                    /* 到期，err 保留 3 */
    digits("999999");
    expect("OK", "ERR WRONG LOCKED 0 4 0 0\r\n");
    digits("999999");
    expect("OK", "ERR WRONG PROTECT 0 5 60 0\r\n");

    /* 非法命令 / 小写 / 多余参数 */
    power_on(PWD_DEF, 0, 0);
    expect("FOO", "ERR BAD_CMD LOCKED 0 0 0 0\r\n");
    expect("digit 0", "ERR BAD_CMD LOCKED 0 0 0 0\r\n");
    expect("DEL 1", "ERR BAD_CMD LOCKED 0 0 0 0\r\n");
    expect("1", "ERR BAD_CMD LOCKED 0 0 0 0\r\n");
    expect("DIGIT  1", "ERR BAD_CMD LOCKED 0 0 0 0\r\n");
    expect("VERSION", "OK VERSION 1.0\r\n");
}

static void t_display(void)
{
    printf("[5] 数码管显示\n");

    /* 锁定 + 输入 012 + 错误 2 次：0 2 0 1 2 - - - */
    power_on(PWD_DEF, 2, 0);
    digits("012");
    Lock_Display();
    expect_int("状态码位", g_display[0], SEG_DIGIT[0]);
    expect_int("错误次数位", g_display[1], SEG_DIGIT[2]);
    expect_int("输入位 1", g_display[2], SEG_DIGIT[0]);
    expect_int("输入位 2", g_display[3], SEG_DIGIT[1]);
    expect_int("输入位 3", g_display[4], SEG_DIGIT[2]);
    expect_int("空位补 '-'", g_display[5], SEG_DASH);
    expect_int("空位补 '-'", g_display[7], SEG_DASH);

    /* 解锁：1 0 - - - - 1 5 */
    digits("345");
    cmd("OK");
    Lock_Display();
    expect_int("解锁状态码", g_display[0], SEG_DIGIT[1]);
    expect_int("倒计时十位", g_display[6], SEG_DIGIT[1]);
    expect_int("倒计时个位", g_display[7], SEG_DIGIT[5]);

    /* 暂停：4 3 0 0 0 0 3 0 */
    power_on(PWD_DEF, 2, 0);
    digits("999999");
    cmd("OK");
    Lock_Display();
    expect_int("暂停状态码", g_display[0], SEG_DIGIT[4]);
    expect_int("剩余秒数补零", g_display[2], SEG_DIGIT[0]);
    expect_int("剩余秒数十位", g_display[6], SEG_DIGIT[3]);
    expect_int("剩余秒数个位", g_display[7], SEG_DIGIT[0]);
}

static void t_isp(void)
{
    printf("[6] ISP：回复后 500ms 软复位\n");

    power_on(PWD_DEF, 0, 0);
    digits("012345");
    cmd("OK");
    cmd("ISP");

    if (setjmp(g_isp_jmp) == 0)
    {
        advance_ms(400);
        expect_int("400ms 时还不该复位", g_isp_reset, 0);
        advance_ms(200);                                  /* 累计 600ms */
        printf("  FAIL  600ms 后仍未软复位\n");
        g_fail++;
    }
    else
    {
        expect_int("串口发完 500ms 后软复位", g_isp_reset, 1);
    }
}

int main(void)
{
    t_examples();
    t_timeout();
    t_persist();
    t_matrix();
    t_display();
    t_isp();

    printf("\n==== 通过 %d 项，失败 %d 项 ====\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
