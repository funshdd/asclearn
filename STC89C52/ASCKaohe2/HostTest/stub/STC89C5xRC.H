/* PC 侧测试用的 <STC89C5xRC.H> 替身，不参与单片机编译 */
#ifndef __STC89C5XRC_H
#define __STC89C5XRC_H

#define code
#define xdata
#define data
#define idata
#define pdata

#define bit     unsigned char
#define sbit    unsigned char
#define sfr     unsigned char
#define sfr16   unsigned int

/* 段码端口需要可写，用变量代替 */
extern unsigned char P0_mem;
#define P0      P0_mem
#define P1      0
#define P2      0
#define P3      0

#define EA      0
#define ES      0
#define RI      0
#define TI      0

/* 关中断用到的 EA 必须是可赋值的变量，单独给一个 */
#undef  EA
unsigned char EA = 0;

/* ISP_CONTR 做成可挂钩的寄存器：测试里一旦写入就 longjmp 出来 */
unsigned char *isp_reg(void);
#define ISP_CONTR  (*isp_reg())

#endif
