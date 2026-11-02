/**
  ******************************************************************************
  * @file    lock.c
  * @brief   STC89C52RC 电子密码锁 —— 业务状态机与串口协议实现
  *
  * 状态机（与题目状态机图一致）：
  *
  *   LOCKED --正确--> UNLOCKED --CHANGE--> NEW1 --满位--> NEW2 --一致--> 保存回 UNLOCKED
  *      ^                |                   |              |
  *      |                +----- LOCK --------+--------------+
  *      |                +----- CANCEL(仅 NEW1/NEW2) -------+
  *      +-- 第3次错误 -> WAIT(30s，到期保留 3 次)
  *      +-- 第5次错误 -> PROTECT(60s，到期清零)
  *
  * 串口回复格式：
  *   OK  <S> <N> <E> <R>\r\n
  *   ERR <CODE> <S> <N> <E> <R>\r\n
  *   特殊：ISP -> "OK ISP"；VERSION -> "OK VERSION <ver>"
  *
  * 所有操作都是“改状态 -> 生成回复 -> 入队发送”，没有一处阻塞等待。
  ******************************************************************************
  */

#include "config.h"
#include "lock.h"
#include "iap.h"
#include "uart.h"
#include "board.h"
#include "timer.h"
#include "nixie.h"
#include "key.h"

/* ------------------------------ 状态定义 ------------------------------ */
#define ST_LOCKED       0
#define ST_UNLOCKED     1
#define ST_NEW1         2
#define ST_NEW2         3
#define ST_WAIT         4
#define ST_PROTECT      5
#define ST_ISP          6

/* ------------------------------ 错误码 ------------------------------ */
#define RP_NONE         0
#define RP_WRONG        1
#define RP_INCOMP       2
#define RP_FULL         3
#define RP_MISMATCH     4
#define RP_DENIED       5
#define RP_BAD          6
#define RP_STORAGE      7

/* ------------------------------ 命令字 ------------------------------ */
#define CMD_BAD         0
#define CMD_DIGIT       1
#define CMD_CNF         2
#define CMD_DEL         3
#define CMD_CLEAR       4
#define CMD_CHANGE      5
#define CMD_CANCEL      6
#define CMD_LOCK        7
#define CMD_STATUS      8
#define CMD_VERSION     9
#define CMD_ISP         10

/* ------------------------------ 运行上下文 ------------------------------ */
static struct
{
    unsigned char pwd[6];       /* 正式密码（0~9，保留前导零） */
    unsigned char input[6];     /* 当前输入 */
    unsigned char len;          /* 当前输入位数 */
    unsigned char tmp[6];       /* NEW1 暂存的新密码 */
    unsigned char err;          /* 错误次数 */
    unsigned char state;        /* 当前状态 */
    unsigned char countdown;    /* 倒计时秒数 */
} ctx;

/* 回复上下文 */
static unsigned char rep_ok;
static unsigned char rep_code;
static unsigned char rep_special;   /* 1 = OK ISP, 2 = OK VERSION */

static char xdata rep_buf[40];      /* 回复拼装缓冲放片外 RAM，节省 data 空间 */
static unsigned char rep_len;

static PersistRec xdata pr;             /* persist() 的记录缓冲 */
static unsigned char xdata old_pwd[6];  /* 改密保存失败时回滚 */

static unsigned int sec_ms;         /* 1 秒计时基准 */
static unsigned int isp_ms;         /* ISP 等待起点 */

/* ------------------------------ 小工具 ------------------------------ */

static void clear_input(void)
{
    unsigned char i;
    for (i = 0; i < 6; i++) ctx.input[i] = 0;
    ctx.len = 0;
}

static void clear_tmp(void)
{
    unsigned char i;
    for (i = 0; i < 6; i++) ctx.tmp[i] = 0;
}

static unsigned char mem_eq6(const unsigned char *a, const unsigned char *b)
{
    unsigned char i;
    for (i = 0; i < 6; i++)
    {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

/* 进入新状态并按状态设定倒计时 */
static void enter_state(unsigned char st)
{
    ctx.state = st;
    switch (st)
    {
    case ST_UNLOCKED:
    case ST_NEW1:
    case ST_NEW2:
        ctx.countdown = TMO_AUTO_LOCK;
        break;
    case ST_WAIT:
        ctx.countdown = TMO_WAIT;
        break;
    case ST_PROTECT:
        ctx.countdown = TMO_PROTECT;
        break;
    default:
        ctx.countdown = 0;
        break;
    }

    /* 倒计时按“进入该状态的时刻”重新对齐 1 秒基准，
       保证 15/30/60 秒是整段而不是被上一秒的余数截短 */
    if (ctx.countdown) sec_ms = Timer0_Millis();
}

/* 有效业务操作刷新 15 秒自动上锁计时 */
static void refresh_tmo(void)
{
    if (ctx.state == ST_UNLOCKED || ctx.state == ST_NEW1 || ctx.state == ST_NEW2)
    {
        ctx.countdown = TMO_AUTO_LOCK;
    }
}

/* 写 EEPROM：密码 + 错误次数 + 限制阶段 */
static unsigned char persist(void)
{
    unsigned char i;

    for (i = 0; i < 6; i++) pr.pwd[i] = ctx.pwd[i];
    pr.err = ctx.err;

    if (ctx.state == ST_WAIT)         pr.stage = 1;
    else if (ctx.state == ST_PROTECT) pr.stage = 2;
    else                              pr.stage = 0;

    return Iap_Save(&pr);
}

/* 回复中的两个倒计时字段：
 *   R = 限制剩余秒数，仅 WAIT / PROTECT 非零
 *   T = 自动上锁剩余秒数，仅 UNLOCKED / NEW1 / NEW2 非零
 */
static unsigned char limit_sec(void)
{
    if (ctx.state == ST_WAIT || ctx.state == ST_PROTECT) return ctx.countdown;
    return 0;
}

static unsigned char auto_sec(void)
{
    if (ctx.state == ST_UNLOCKED || ctx.state == ST_NEW1 || ctx.state == ST_NEW2)
    {
        return ctx.countdown;
    }
    return 0;
}

/* ------------------------------ 回复拼装 ------------------------------ */

static void rep_ch(char c)
{
    if (rep_len < 38) rep_buf[rep_len++] = c;
}

static void rep_str(const char code *s)
{
    while (*s) rep_ch(*s++);
}

static void rep_u8(unsigned char v)
{
    if (v >= 100)
    {
        rep_ch((char)('0' + v / 100));
        v = (unsigned char)(v % 100);
        rep_ch((char)('0' + v / 10));
        rep_ch((char)('0' + v % 10));
    }
    else if (v >= 10)
    {
        rep_ch((char)('0' + v / 10));
        rep_ch((char)('0' + v % 10));
    }
    else
    {
        rep_ch((char)('0' + v));
    }
}

static const char code *state_name(void)
{
    switch (ctx.state)
    {
    case ST_UNLOCKED: return "UNLOCKED";
    case ST_NEW1:     return "NEW1";
    case ST_NEW2:     return "NEW2";
    case ST_WAIT:     return "WAIT";
    case ST_PROTECT:  return "PROTECT";
    default:          return "LOCKED";
    }
}

static const char code *code_name(void)
{
    switch (rep_code)
    {
    case RP_WRONG:    return "WRONG";
    case RP_INCOMP:   return "INCOMPLETE";
    case RP_FULL:     return "FULL";
    case RP_MISMATCH: return "MISMATCH";
    case RP_DENIED:   return "DENIED";
    case RP_STORAGE:  return "STORAGE";
    default:          return "BAD_CMD";
    }
}

static void send_reply(void)
{
    if (rep_special == 1)
    {
        Uart_SendStr("OK ISP\r\n");
        return;
    }
    if (rep_special == 2)
    {
        Uart_SendStr("OK VERSION " VERSION_STR "\r\n");
        return;
    }

    rep_len = 0;
    if (rep_ok) rep_str("OK ");
    else
    {
        rep_str("ERR ");
        rep_str(code_name());
        rep_ch(' ');
    }
    rep_str(state_name());
    rep_ch(' '); rep_u8(ctx.len);
    rep_ch(' '); rep_u8(ctx.err);
    rep_ch(' '); rep_u8(limit_sec());
    rep_ch(' '); rep_u8(auto_sec());
    rep_str("\r\n");

    Uart_Send((const unsigned char *)rep_buf, rep_len);
}

static void set_ok(void)
{
    rep_ok = 1;
    rep_code = RP_NONE;
}

static void set_err(unsigned char ec)
{
    rep_ok = 0;
    rep_code = ec;
}

/* ------------------------------ 命令执行 ------------------------------ */

static void do_digit(unsigned char d)
{
    switch (ctx.state)
    {
    case ST_LOCKED:
    case ST_NEW1:
    case ST_NEW2:
        if (ctx.len >= MAX_DIGITS)
        {
            set_err(RP_FULL);           /* 已满不得覆盖或追加 */
            return;
        }
        ctx.input[ctx.len++] = d;
        refresh_tmo();
        set_ok();
        break;
    default:
        set_err(RP_DENIED);
        break;
    }
}

static void do_del(void)
{
    switch (ctx.state)
    {
    case ST_LOCKED:
    case ST_NEW1:
    case ST_NEW2:
        if (ctx.len)                    /* 空删除：成功但不刷新计时 */
        {
            ctx.len--;
            ctx.input[ctx.len] = 0;
            refresh_tmo();
        }
        set_ok();
        break;
    default:
        set_err(RP_DENIED);
        break;
    }
}

static void do_clear(void)
{
    switch (ctx.state)
    {
    case ST_LOCKED:
    case ST_NEW1:
    case ST_NEW2:
        if (ctx.len) refresh_tmo();     /* 空清空：成功但不刷新计时 */
        clear_input();
        set_ok();
        break;
    default:
        set_err(RP_DENIED);
        break;
    }
}

static void do_confirm(void)
{
    unsigned char i;

    switch (ctx.state)
    {
    case ST_LOCKED:
    {
        if (ctx.len < MAX_DIGITS)
        {
            set_err(RP_INCOMP);         /* 不足六位：不计错、保留输入 */
            return;
        }
        if (mem_eq6(ctx.pwd, ctx.input))
        {
            clear_input();
            ctx.err = 0;
            enter_state(ST_UNLOCKED);
            persist();
            set_ok();
        }
        else
        {
            clear_input();
            if (ctx.err < 255) ctx.err++;

            if (ctx.err >= ERR_LIMIT_PROTECT)      enter_state(ST_PROTECT);   /* 第 5 次 */
            else if (ctx.err == ERR_LIMIT_WAIT)    enter_state(ST_WAIT);      /* 第 3 次 */
            else                                   enter_state(ST_LOCKED);    /* 第 1/2/4 次 */

            persist();
            set_err(RP_WRONG);
        }
        break;
    }

    case ST_NEW1:
    {
        if (ctx.len < MAX_DIGITS)
        {
            set_err(RP_INCOMP);
            return;
        }
        for (i = 0; i < 6; i++) ctx.tmp[i] = ctx.input[i];
        clear_input();
        enter_state(ST_NEW2);
        set_ok();
        break;
    }

    case ST_NEW2:
    {
        if (ctx.len < MAX_DIGITS)
        {
            set_err(RP_INCOMP);
            return;
        }
        if (mem_eq6(ctx.tmp, ctx.input))
        {
            unsigned char ok;

            for (i = 0; i < 6; i++) { old_pwd[i] = ctx.pwd[i]; ctx.pwd[i] = ctx.input[i]; }
            clear_input();
            clear_tmp();
            enter_state(ST_UNLOCKED);

            ok = persist();
            if (!ok)
            {
                for (i = 0; i < 6; i++) ctx.pwd[i] = old_pwd[i];   /* 存不下就回滚，不报成功 */
                set_err(RP_STORAGE);
            }
            else
            {
                set_ok();
            }
        }
        else
        {
            clear_input();
            clear_tmp();
            enter_state(ST_UNLOCKED);
            set_err(RP_MISMATCH);
        }
        break;
    }

    default:
        set_err(RP_DENIED);
        break;
    }
}

static void do_change(void)
{
    if (ctx.state == ST_UNLOCKED)
    {
        clear_input();
        clear_tmp();
        enter_state(ST_NEW1);
        set_ok();
    }
    else
    {
        set_err(RP_DENIED);
    }
}

static void do_cancel(void)
{
    switch (ctx.state)
    {
    case ST_NEW1:
    case ST_NEW2:
        clear_input();
        clear_tmp();
        enter_state(ST_UNLOCKED);
        set_ok();
        break;
    case ST_LOCKED:
    case ST_UNLOCKED:
        set_ok();                       /* 原态，输入保留 */
        break;
    default:
        set_err(RP_DENIED);
        break;
    }
}

static void do_lock(void)
{
    switch (ctx.state)
    {
    case ST_WAIT:
    case ST_PROTECT:
        set_ok();                       /* 原态：不解除限制、不重新计时 */
        break;
    case ST_LOCKED:
        clear_input();
        set_ok();
        break;
    default:
        clear_input();
        clear_tmp();
        enter_state(ST_LOCKED);
        persist();
        set_ok();
        break;
    }
}

static void do_isp(void)
{
    if (ctx.state == ST_UNLOCKED)
    {
        rep_special = 1;                /* 特殊回复：OK ISP（无状态字段） */
        enter_state(ST_ISP);
        isp_ms = Timer0_Millis();
    }
    else
    {
        set_err(RP_DENIED);
    }
}

/* ------------------------------ 命令解析 ------------------------------ */

static unsigned char cmd_eq(const char *s, unsigned char len,
                            const char code *lit)
{
    unsigned char i = 0;

    while (lit[i])
    {
        if (i >= len || s[i] != lit[i]) return 0;
        i++;
    }
    return (unsigned char)(i == len);
}

static unsigned char parse_cmd(const char *s, unsigned char len,
                               unsigned char *arg)
{
    if (len == 0) return CMD_BAD;

    if (len >= 5 && cmd_eq(s, 5, "DIGIT"))
    {
        if (len != 7) return CMD_BAD;
        if (s[5] != ' ') return CMD_BAD;
        if (s[6] < '0' || s[6] > '9') return CMD_BAD;
        *arg = (unsigned char)(s[6] - '0');
        return CMD_DIGIT;
    }

    if (cmd_eq(s, len, "OK"))      return CMD_CNF;
    if (cmd_eq(s, len, "DEL"))     return CMD_DEL;
    if (cmd_eq(s, len, "CLEAR"))   return CMD_CLEAR;
    if (cmd_eq(s, len, "CHANGE"))  return CMD_CHANGE;
    if (cmd_eq(s, len, "CANCEL"))  return CMD_CANCEL;
    if (cmd_eq(s, len, "LOCK"))    return CMD_LOCK;
    if (cmd_eq(s, len, "STATUS"))  return CMD_STATUS;
    if (cmd_eq(s, len, "VERSION")) return CMD_VERSION;
    if (cmd_eq(s, len, "ISP"))     return CMD_ISP;

    return CMD_BAD;
}

/* ------------------------------ 时间处理 ------------------------------ */

static void lock_tick_1s(void)
{
    if (ctx.countdown == 0) return;

    ctx.countdown--;

    if (ctx.countdown != 0) return;

    /* 到期：静默跳转，不主动回复 */
    switch (ctx.state)
    {
    case ST_UNLOCKED:
    case ST_NEW1:
    case ST_NEW2:
        clear_input();
        clear_tmp();
        enter_state(ST_LOCKED);
        persist();
        break;

    case ST_WAIT:                       /* 到期保留错误次数（3 次） */
        enter_state(ST_LOCKED);
        persist();
        break;

    case ST_PROTECT:                    /* 到期清零 */
        ctx.err = 0;
        enter_state(ST_LOCKED);
        persist();
        break;

    default:
        break;
    }
}

void Lock_Poll(void)
{
    unsigned int now = Timer0_Millis();

    while ((unsigned int)(now - sec_ms) >= 1000)
    {
        sec_ms += 1000;
        lock_tick_1s();
    }

    if (ctx.state == ST_ISP)
    {
        /* 回复发完后再等 500ms，然后软复位进入芯片 ISP（不清 EEPROM） */
        if (Uart_TxIdle() && (unsigned int)(now - isp_ms) >= 500)
        {
            Iap_SoftResetToIsp();
            while (1) { }
        }
    }
}

/* ------------------------------ 显示 ------------------------------ */

void Lock_Display(void)
{
    unsigned char b[8];
    unsigned char i;
    unsigned char e = ctx.err;
    unsigned char v;

    if (e > ERR_LIMIT_PROTECT) e = ERR_LIMIT_PROTECT;

    if (ctx.state == ST_WAIT || ctx.state == ST_PROTECT)
    {
        /* 第 1 位状态码，第 2 位错误次数，后六位剩余秒数（补零） */
        b[0] = SEG_DIGIT[ctx.state];
        b[1] = SEG_DIGIT[e];
        v = ctx.countdown;
        for (i = 7; i >= 2; i--)
        {
            b[i] = SEG_DIGIT[v % 10];
            v = (unsigned char)(v / 10);
        }
    }
    else if (ctx.state == ST_UNLOCKED)
    {
        /* 解锁：后六位显示 ----NN 形式的自动上锁倒计时 */
        b[0] = SEG_DIGIT[1];
        b[1] = SEG_DIGIT[e];
        b[2] = SEG_DASH;
        b[3] = SEG_DASH;
        b[4] = SEG_DASH;
        b[5] = SEG_DASH;
        b[6] = SEG_DIGIT[(ctx.countdown / 10) % 10];
        b[7] = SEG_DIGIT[ctx.countdown % 10];
    }
    else
    {
        b[0] = SEG_DIGIT[ctx.state];
        b[1] = SEG_DIGIT[e];
        for (i = 0; i < 6; i++)
        {
            b[2 + i] = (i < ctx.len) ? SEG_DIGIT[ctx.input[i]] : SEG_DASH;
        }
    }

    Nixie_Commit(b);
}

/* ------------------------------ 对外接口 ------------------------------ */

void Lock_Init(void)
{
    PersistRec r;
    unsigned char i;
    const unsigned char code def_pwd[6] = {0, 1, 2, 3, 4, 5};

    clear_input();
    clear_tmp();

    if (Iap_Load(&r))
    {
        for (i = 0; i < 6; i++) ctx.pwd[i] = r.pwd[i];
        ctx.err = r.err;

        if (r.stage == 1)       enter_state(ST_WAIT);       /* 重启后重新等待完整时长 */
        else if (r.stage == 2)  enter_state(ST_PROTECT);
        else                    enter_state(ST_LOCKED);
    }
    else
    {
        for (i = 0; i < 6; i++) ctx.pwd[i] = def_pwd[i];
        ctx.err = 0;
        enter_state(ST_LOCKED);
    }

    sec_ms = Timer0_Millis();
    Lock_Display();
}

void Lock_SerialLine(const char *line, unsigned char len)
{
    unsigned char cmd;
    unsigned char arg = 0;

    Lock_Poll();                        /* 先结算超时：到点先上锁，再处理命令 */

    rep_ok = 1;
    rep_code = RP_NONE;
    rep_special = 0;

    cmd = parse_cmd(line, len, &arg);

    switch (cmd)
    {
    case CMD_DIGIT:   do_digit(arg); break;
    case CMD_CNF:     do_confirm();  break;
    case CMD_DEL:     do_del();      break;
    case CMD_CLEAR:   do_clear();    break;
    case CMD_CHANGE:  do_change();   break;
    case CMD_CANCEL:  do_cancel();   break;
    case CMD_LOCK:    do_lock();     break;
    case CMD_STATUS:  set_ok();      break;
    case CMD_VERSION: rep_special = 2; break;
    case CMD_ISP:     do_isp();      break;
    default:          set_err(RP_BAD); break;
    }

    send_reply();
    Feedback_Command();
}

void Lock_ReplyBad(void)
{
    Lock_Poll();

    rep_ok = 0;
    rep_code = RP_BAD;
    rep_special = 0;

    send_reply();
    Feedback_Command();
}

void Lock_KeyEvent(unsigned char k)
{
    Feedback_Key();

    switch (k)
    {
    case KEY_1:    do_digit(1); break;
    case KEY_2:    do_digit(2); break;
    case KEY_3:    do_digit(3); break;
    case KEY_OK:   do_confirm(); break;
    case KEY_4:    do_digit(4); break;
    case KEY_5:    do_digit(5); break;
    case KEY_6:    do_digit(6); break;
    case KEY_DEL:  do_del();    break;
    case KEY_7:    do_digit(7); break;
    case KEY_8:    do_digit(8); break;
    case KEY_9:    do_digit(9); break;
    case KEY_CHG:  do_change(); break;
    case KEY_CLR:  do_clear();  break;
    case KEY_0:    do_digit(0); break;
    case KEY_CAN:  do_cancel(); break;
    case KEY_LOCK: do_lock();   break;
    default: break;
    }
}

/******************************* END OF FILE *********************************/
