//#include <REGX52.H>
#include <89c52.h>

// 100hz T=10ms

unsigned int ratio = 50;

sbit P2_0 = P2^0;
sbit P1_0 = P1^0;
sbit P1_1 = P1^1;

void Timer0Init(void)
{
	AUXR &= 0x7F;		
	TMOD &= 0xF0;
	TMOD |= 0x01;
	TL0 = 0x9C+1;
	TH0 = 0xFF;	
	TF0 = 0;	
	TR0 = 1;
	
	ET0 = 1; //interr piority
	EA = 1;
	PT0 = 0;
}


void TimerInit()
{
	//TCON = 0x01;//timer mode control
	
	TMOD = TMOD&0x0F;
	TMOD = TMOD|0x01;
	
	TF0 = 0;//timer overflow
	TR0 = 1;//timer ready
		
	//0.1ms
	TH0 = 0xFF;//timer counter high
	TL0 = 0x9C+1;//timer counter low
	
	ET0 = 1; //interr piority
	EA = 1;
	PT0 = 0;
}

unsigned int timer_count = 0;
int isH = 0;

//0.1ms
void Timer0_Routine() interrupt 1
{
	TH0 = 0xFF;//timer counter high
	TL0 = 0x9C+1;//timer counter low
	
	timer_count++;
	
	if(timer_count > ratio)
	{
		P2_0 = 0;
	}
	else
	{
		P2_0 = 1;
	}
	
	if(timer_count>=100)
	{
		timer_count  = 0;
	}
}

void main()
{
	Timer0Init();
	
	while(1)
	{
		if(P1_0==0)
		{
			while(P1_0==0);
			if(ratio>=10)
				ratio-=5;
		}
		if(P1_1==0)
		{
			while(P1_1==0);
			if(ratio<=90)
				ratio+=5;
		}
	}
}