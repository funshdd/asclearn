#include "Tick.h"
#include "Nixie.h"
#include "Key.h"
#include "Event.h"
#include "States.h"
#include "Uart.h"
#include "Feedback.h"

volatile unsigned int tick_count = 0;

void Timer2_Init(void)
{
	T2CON  = 0x00;
	RCAP2H = 0xFC;
	RCAP2L = 0x66;
	TH2    = 0xFC;
	TL2    = 0x66;

	PT2 = 1;
	ET2 = 1;
	TR2 = 1;
}

void Timer0_Init(void)
{
	TMOD &= 0xF0;
	TMOD |= 0x01;
	TH0 = 0xFC;
	TL0 = 0x66;

	PT0 = 0;
	ET0 = 1;
	TR0 = 1;
}

unsigned int Tick_Millis(void)
{
	unsigned int v;
	bit ea = EA;

	EA = 0;
	v = tick_count;
	EA = ea;

	return v;
}

void Timer2_ISR(void) interrupt 5
{
	TF2 = 0;

	tick_count++;

	switch (State_GetTop())
	{
		case S_LOCKED:
			Locked_State_Tick();
			break;
		case S_UNLOCK:
			Unlock_State_Tick();
			break;
		case S_NEW1:
			New1_State_Tick();
			break;
		case S_NEW2:
			New2_State_Tick();
			break;
		case S_WAIT30:
			Wait30_State_Tick();
			break;
		case S_WAIT60:
			Wait60_State_Tick();
			break;
		case S_ISP:
			Isp_State_Tick();
			break;

		default:
			break;
	}
}

void Timer0_ISR(void) interrupt 1
{
	TH0 = 0xFC;
	TL0 = 0x66;

	Feedback_Tick();
	Key_Tick();
	Nixie_Tick();
}
