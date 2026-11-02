#include "Nixie.h"
#include "Uart.h"
#include "Delay.h"
#include "Tick.h"
#include "Key.h"
#include "Event.h"
#include "Password.h"
#include "States.h"
#include "UartStringAsm.h"
#include "Feedback.h"

#ifndef D8_ALWAYS_ON
#define D8_ALWAYS_ON 0
#endif

#define D8_ON   0
#define D8_OFF  1

sbit D8 = P2^7;

void main(void)
{
#if D8_ALWAYS_ON
	D8 = D8_ON;
#else
	D8 = D8_OFF;
#endif

	Key_Init();
	Feedback_Init();
	Timer2_Init();
	Timer0_Init();
	Uart_Init();
	EA = 1;

	Delay50ms();

	Password_Load();
	Nixie_ShowDefault();

	State_Push(S_LOCKED);

	if (stage == 1)
	{
		State_Push(S_WAIT30);
	}
	else if (stage == 2)
	{
		State_Push(S_WAIT60);
	}

	while(1)
	{
		Uart_Tick();
		Event_Tick();
		State_Poll();
		Isp_Poll();
	}
}
