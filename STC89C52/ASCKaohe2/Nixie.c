#include "Nixie.h"

static unsigned char scan_pos = 0;
unsigned char nixie_buffer[8] = {ALL_OFF, ALL_OFF, ALL_OFF, ALL_OFF,
                                 ALL_OFF, ALL_OFF, ALL_OFF, ALL_OFF};
unsigned char input_pos = 2;

const unsigned char code segment_code[13] =
{
	0x3F,
	0x06,
	0x5B,
	0x4F,
	0x66,
	0x6D,
	0x7D,
	0x07,
	0x7F,
	0x6F,
	0xFF,
	0x00,
	0x40
};

void Nixie_Display(int pos,int num)
{
	switch(pos)
	{
		case 0:NC=1;NB=1;NA=1;break;
		case 1:NC=1;NB=1;NA=0;break;
		case 2:NC=1;NB=0;NA=1;break;
		case 3:NC=1;NB=0;NA=0;break;
		case 4:NC=0;NB=1;NA=1;break;
		case 5:NC=0;NB=1;NA=0;break;
		case 6:NC=0;NB=0;NA=1;break;
		case 7:NC=0;NB=0;NA=0;break;
		default:return;
	}

	P0 = segment_code[num];
}

void Nixie_AllOff()
{
	P0 = 0x00;
}

void Nixie_Tick()
{
	Nixie_AllOff();
	Nixie_Display(scan_pos,nixie_buffer[scan_pos]);
	if (++scan_pos >= 8) scan_pos = 0;
}

void Nixie_Clear()
{
	unsigned char i;
	for(i=2;i<8;i++)
	{
		nixie_buffer[i] = NIXIE_DASH;
	}
	input_pos = 2;
}

void Nixie_ShowDefault()
{
	unsigned char i;

	nixie_buffer[0] = 0;
	nixie_buffer[1] = 0;
	for(i=2;i<8;i++)
	{
		nixie_buffer[i] = NIXIE_DASH;
	}
	input_pos = 2;
}
