/**
  ******************************************************************************
  * @file    iap.c
  * @brief   片内 EEPROM 持久化（STC89C52RC：2KB，起始 0x2000，512B 一个扇区）
  *
  * 存储策略 —— “日志式双扇区”，保证写入过程掉电时数据要么是完整旧值、
  * 要么是完整新值，绝不出现“半个密码”：
  *
  *   扇区 A = 0x2000~0x21FF，扇区 B = 0x2200~0x23FF，每个扇区 32 个 16 字节槽。
  *   - 每次保存只在“当前扇区”的下一个空槽追加一条记录，不需要擦除；
  *     写之前旧记录原封不动地留着，因此写到一半掉电不会破坏旧值。
  *   - 当前扇区的 32 个槽写满后，才去擦除“另一个扇区”（里面只有更旧的副本），
  *     再把最新记录写进去并切换过去 —— 擦除瞬间丢失的也只是旧副本。
  *   - 每条记录带序号 seq 与校验和 chk；上电扫描两个扇区，取序号最新的有效记录。
  *
  * 时间：读 ~10us/字节，写 ~60us/字节，扇区擦除 ~10ms（擦除期间关总中断）。
  ******************************************************************************
  */

#include "config.h"
#include "iap.h"

/* ---------- ISP/IAP 特殊功能寄存器 ---------- */
sfr ISP_DATA  = 0xE2;
sfr ISP_ADDRH = 0xE3;
sfr ISP_ADDRL = 0xE4;
sfr ISP_CMD   = 0xE5;
sfr ISP_TRIG  = 0xE6;
sfr ISP_CONTR = 0xE7;

#define IAP_CMD_IDLE    0x00
#define IAP_CMD_READ    0x01
#define IAP_CMD_WRITE   0x02
#define IAP_CMD_ERASE   0x03
#define IAP_CONTR_ON    0x81        /* ISPEN = 1, WT1:WT0 = 01 (<20MHz) */

#define SEC_A           0x2000
#define SEC_B           0x2200
#define SLOT_MAX        32          /* 512 / 16 */

static unsigned int  s_base;        /* 当前追加写入的扇区基址 */
static unsigned char s_slot;        /* 当前扇区下一个空槽 */
static unsigned char s_seq;         /* 已保存的最大序号 */

/* 记录缓冲放片外 RAM：避免在函数里定义 16 字节结构体把 DATA 区撑爆 */
static PersistRec xdata rec_a;
static PersistRec xdata rec_b;
static PersistRec xdata rec_tmp;

/* ------------------------------ 底层操作 ------------------------------ */

static unsigned char iap_read(unsigned int addr)
{
    unsigned char v;
    bit ea = EA;

    EA = 0;                             /* 触发序列不可被打断 */
    ISP_CONTR = IAP_CONTR_ON;
    ISP_CMD   = IAP_CMD_READ;
    ISP_ADDRH = (unsigned char)(addr >> 8);
    ISP_ADDRL = (unsigned char)addr;
    ISP_TRIG  = 0x46;
    ISP_TRIG  = 0xB9;
    v = ISP_DATA;
    ISP_CONTR = 0x00;
    ISP_CMD   = IAP_CMD_IDLE;
    ISP_TRIG  = 0x00;
    EA = ea;

    return v;
}

static void iap_write(unsigned int addr, unsigned char dat)
{
    bit ea = EA;

    EA = 0;
    ISP_CONTR = IAP_CONTR_ON;
    ISP_CMD   = IAP_CMD_WRITE;
    ISP_ADDRH = (unsigned char)(addr >> 8);
    ISP_ADDRL = (unsigned char)addr;
    ISP_DATA  = dat;
    ISP_TRIG  = 0x46;
    ISP_TRIG  = 0xB9;
    ISP_CONTR = 0x00;
    ISP_CMD   = IAP_CMD_IDLE;
    ISP_TRIG  = 0x00;
    EA = ea;
}

static void iap_erase(unsigned int addr)
{
    bit ea = EA;

    EA = 0;                             /* 约 10ms，期间数码管由硬件保持 */
    ISP_CONTR = IAP_CONTR_ON;
    ISP_CMD   = IAP_CMD_ERASE;
    ISP_ADDRH = (unsigned char)(addr >> 8);
    ISP_ADDRL = (unsigned char)addr;
    ISP_TRIG  = 0x46;
    ISP_TRIG  = 0xB9;
    ISP_CONTR = 0x00;
    ISP_CMD   = IAP_CMD_IDLE;
    ISP_TRIG  = 0x00;
    EA = ea;
}

/* ------------------------------ 记录读写 ------------------------------ */

static void rec_read(unsigned int addr, PersistRec *r)
{
    unsigned char *p = (unsigned char *)r;
    unsigned char i;

    for (i = 0; i < REC_SIZE; i++) p[i] = iap_read(addr + i);
}

/* 写入并回读校验，返回 0 表示失败 */
static unsigned char rec_write(unsigned int addr, const PersistRec *r)
{
    const unsigned char *p = (const unsigned char *)r;
    unsigned char i;

    for (i = 0; i < REC_SIZE; i++)
    {
        if (p[i] != 0xFF) iap_write(addr + i, p[i]);   /* 0xFF 即空白，可省一次写 */
    }
    for (i = 0; i < REC_SIZE; i++)
    {
        if (iap_read(addr + i) != p[i]) return 0;
    }
    return 1;
}

static unsigned char rec_checksum(const PersistRec *r)
{
    const unsigned char *p = (const unsigned char *)r;
    unsigned char i, s = 0x5A;

    for (i = 0; i < 12; i++) s = (unsigned char)(s + p[i]);
    return s;
}

static unsigned char rec_valid(const PersistRec *r)
{
    unsigned char i;

    if (r->magic0 != REC_MAGIC0 || r->magic1 != REC_MAGIC1) return 0;
    if (r->ver != REC_VER) return 0;
    if (r->chk != rec_checksum(r)) return 0;
    if (r->err > ERR_LIMIT_PROTECT) return 0;
    if (r->stage > 2) return 0;
    for (i = 0; i < 6; i++)
    {
        if (r->pwd[i] > 9) return 0;
    }
    return 1;
}

/* 序号比较：环形，a 比 b 新返回 1 */
static unsigned char seq_newer(unsigned char a, unsigned char b)
{
    unsigned char d = (unsigned char)(a - b);
    return (unsigned char)((d != 0) && (d < 128));
}

/* 扫描一个扇区，返回已用槽数，同时给出该扇区内最新的一条有效记录 */
static unsigned char scan_sector(unsigned int base, PersistRec *best,
                                 unsigned char *have)
{
    unsigned char slot = 0;

    while (slot < SLOT_MAX)
    {
        rec_read(base + (unsigned int)slot * REC_SIZE, &rec_tmp);
        if (rec_tmp.magic0 == 0xFF) break;          /* 空白槽，后面都没写过 */
        if (!rec_valid(&rec_tmp)) break;            /* 半写记录，停止扫描 */
        if (!*have || seq_newer(rec_tmp.seq, best->seq))
        {
            *best = rec_tmp;
            *have = 1;
        }
        slot++;
    }
    return slot;
}

/* ------------------------------ 对外接口 ------------------------------ */

unsigned char Iap_Load(PersistRec *out)
{
    unsigned char have_a = 0, have_b = 0;
    unsigned char cnt_a, cnt_b;

    cnt_a = scan_sector(SEC_A, &rec_a, &have_a);
    cnt_b = scan_sector(SEC_B, &rec_b, &have_b);

    if (!have_a && !have_b)
    {
        s_base = SEC_A;                         /* 首次上电：从 A 扇区槽 0 开始 */
        s_slot = 0;
        s_seq  = 0;
        return 0;
    }

    if (have_a && (!have_b || !seq_newer(rec_b.seq, rec_a.seq)))
    {
        *out   = rec_a;
        s_base = SEC_A;
        s_slot = cnt_a;
    }
    else
    {
        *out   = rec_b;
        s_base = SEC_B;
        s_slot = cnt_b;
    }
    s_seq = out->seq;
    return 1;
}

unsigned char Iap_Save(PersistRec *rec)
{
    unsigned int addr;
    unsigned int other;

    rec->magic0 = REC_MAGIC0;
    rec->magic1 = REC_MAGIC1;
    rec->ver    = REC_VER;
    rec->seq    = (unsigned char)(s_seq + 1);
    rec->res[0] = 0xFF;
    rec->res[1] = 0xFF;
    rec->res[2] = 0xFF;
    rec->chk    = rec_checksum(rec);

    if (s_slot >= SLOT_MAX)
    {
        /* 当前扇区写满：擦掉另一个（更旧的）扇区，把最新记录搬过去 */
        other = (s_base == SEC_A) ? SEC_B : SEC_A;
        iap_erase(other);
        if (!rec_write(other, rec)) return 0;
        s_base = other;
        s_slot = 1;
        s_seq  = rec->seq;
        return 1;
    }

    addr = s_base + (unsigned int)s_slot * REC_SIZE;
    if (!rec_write(addr, rec)) return 0;

    s_slot++;
    s_seq = rec->seq;
    return 1;
}

/**
  * @brief  软复位并从 ISP 引导区启动（不断电升级）
  * @note   ISP_CONTR = 0x60 ：SWBS=1（从 ISP 程序区启动）+ SWRST=1（软件复位）
  */
void Iap_SoftResetToIsp(void)
{
    ISP_CONTR = 0x60;
    while (1) { }
}

/******************************* END OF FILE *********************************/
