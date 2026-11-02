#include <REGX52.H>

void TimerInit()
{
	TMOD &= 0xF0;
	TMOD |= 0x01;
	
	TF0 = 0;
	TR0 = 1;
	
	ET0 = 1;
	EA = 1;
	PT0 = 0;
}

void Timer0_Routine() interrupt 1
{
	TL0 = 0x18;
	TH0 = 0xFC;
	
	P1_6 = ~P1_6;
}

void main()
{
	TimerInit();

}