#include <REGX52.H>
//  A     1101 1010
// F B	  0010 0101
//  G     
// E C
//  D  H
unsigned char digits[10]= {0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0x80,0x90};

unsigned char digit_count = 0;

void timer_init()
{
	TH0 = 64535/256;
	TL0 = 64535%256;
	
	TF0 = 0;
	TR0 = 1;
	
	ET0 = 1;
	EA = 1;
	
	P2=0x01;
}

void Timer0_Routine() interrupt 1
{
	TH0 = 64535/256;
	TL0 = 64535%256;
	
	P2 = P2<<1;
	
	if(P2==0x10)
	{
		P2=0x01;
	}
}

void main()
{
	timer_init();
	
	P0 = digits[5];
}