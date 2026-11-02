#include "STC89C5xRC.H"
#include "Password.h"

unsigned char P0, P1, P2, P3;
unsigned char SBUF, TI, RI, SCON, PCON, TMOD;
unsigned char TH0, TL0, TH1, TL1, TR0, TR1, ET0, ET1;
unsigned char EA, ES, ISP_CONTR;

unsigned char g_b1_5, g_b2_0, g_b2_2, g_b2_3, g_b2_4, g_b2_6, g_b2_7;

/* RAM 版 AT24C02：用来验证断电保留逻辑 */
static unsigned char ee_pwd[6] = {0, 1, 2, 3, 4, 5};
static unsigned char ee_err   = 0;
static unsigned char ee_stage = 0;

void Stub_EeReset(void)
{
	unsigned char i;

	for (i = 0; i < 6; i++) ee_pwd[i] = i;
	ee_err   = 0;
	ee_stage = 0;
}

unsigned char Rom_SaveState(unsigned char *pwd, unsigned char e, unsigned char st)
{
	unsigned char i;

	for (i = 0; i < 6; i++) ee_pwd[i] = pwd[i];
	ee_err   = e;
	ee_stage = st;

	return 0;
}

unsigned char Rom_LoadState(unsigned char *pwd, unsigned char *e, unsigned char *st)
{
	unsigned char i;

	for (i = 0; i < 6; i++) pwd[i] = ee_pwd[i];
	*e = ee_err;
	*st = ee_stage;

	return 0;
}

void Delay50ms(void)
{
}

void Delay10us(void)
{
}
