#ifndef __STC89C5XRC_H
#define __STC89C5XRC_H

typedef unsigned char bit;

#define code
#define xdata
#define data
#define idata

extern unsigned char P0, P1, P2, P3;
extern unsigned char SBUF, TI, RI, SCON, PCON, TMOD;
extern unsigned char TH0, TL0, TH1, TL1, TR0, TR1, ET0, ET1;
extern unsigned char EA, ES, ISP_CONTR;

extern unsigned char g_b1_5;
extern unsigned char g_b2_0;
extern unsigned char g_b2_2;
extern unsigned char g_b2_3;
extern unsigned char g_b2_4;
extern unsigned char g_b2_6;
extern unsigned char g_b2_7;

#endif
