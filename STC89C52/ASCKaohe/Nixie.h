#ifndef __NIXIE_H
#define __NIXIE_H

#include <STC89C5xRC.H>

sbit NA = P2^2;
sbit NB = P2^3;
sbit NC = P2^4;

#define ALL_ON 10
#define ALL_OFF 11

unsigned char xdata segment_code[18] = 
{
	0x3F,	//0
	0x06,	//1
	0x5B,	//2
	0x4F,	//3
	0x66,	//4
	0x6D,	//5
	0x7D,	//6
	0x07,	//7
	0x7F,	//8
	0x6F,	//9
	0xFF,	//全亮
	0x00	//全灭
};

void nixie_display(int pos,int num)
{
	switch(pos)
	{
		case 1:NC=1;NB=1;NA=1;break;
		case 2:NC=1;NB=1;NA=0;break;
		case 3:NC=1;NB=0;NA=1;break;
		case 4:NC=1;NB=0;NA=0;break;
		case 5:NC=0;NB=1;NA=1;break;
		case 6:NC=0;NB=1;NA=0;break;
		case 7:NC=0;NB=0;NA=1;break;
		case 8:NC=0;NB=0;NA=0;break;
		default:return;
	}
	
	P0 = segment_code[num];
}

#endif