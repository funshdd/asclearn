#include "Event.h"
#include "States.h"

char event_list[EVENT_LIST_SIZE];
unsigned char state_stack[STATE_STACK_SIZE];
unsigned char state_process_stack[STATE_STACK_SIZE];
unsigned char event_top = 0;
unsigned char state_top = 0;
unsigned char state_process_top = 0;

bit is_event_empty(void)
{
	return (event_top == 0) ? 1 : 0;
}

void Event_Push(char event)
{
	if (event_top >= EVENT_LIST_SIZE)
	{
		return;
	}

	event_list[event_top] = event;
	event_top++;
}

void Event_Exec(char e)
{
	switch (State_GetTop())
	{
		case S_LOCKED:
			Locked_State_Event(e);
			break;
		case S_UNLOCK:
			Unlock_State_Event(e);
			break;
		case S_NEW1:
			New1_State_Event(e);
			break;
		case S_NEW2:
			New2_State_Event(e);
			break;
		case S_WAIT30:
			Wait30_State_Event(e);
			break;
		case S_WAIT60:
			Wait60_State_Event(e);
			break;
		case S_ISP:
			Isp_State_Event(e);
			break;
		default:
			break;
	}
}

void Event_Tick(void)
{
	unsigned char i;
	char e;

	if (event_top == 0)
	{
		return;
	}

	for (i = 0; i < event_top; i++)
	{
		e = event_list[i];
		Event_Exec(e);
	}

	event_top = 0;
}

void State_Push(unsigned char state)
{
	bit ea = EA;

	EA = 0;

	if (state_top < STATE_STACK_SIZE)
	{
		state_stack[state_top] = state;
		state_top++;
	}

	EA = ea;
}

void State_Pop(void)
{
	bit ea = EA;

	EA = 0;

	if (state_top > 0)
	{
		state_top--;
	}

	EA = ea;
}

unsigned char State_GetTop(void)
{
	if (state_top == 0)
	{
		return S_EMPTY;
	}

	return state_stack[state_top - 1];
}

void State_Process_Push(unsigned char state)
{
	if (state_process_top >= STATE_STACK_SIZE)
	{
		return;
	}

	state_process_stack[state_process_top] = state;
	state_process_top++;
}

void State_Process_Pop(void)
{
	if (state_process_top == 0)
	{
		return;
	}

	state_process_top--;
}

unsigned char State_Process_GetTop(void)
{
	if (state_process_top == 0)
	{
		return S_EMPTY;
	}

	return state_process_stack[state_process_top - 1];
}