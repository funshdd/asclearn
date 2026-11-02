#include "Key.h"
#include "Event.h"
#include "Feedback.h"

#define KEY_PORT   P1

static const unsigned char code COL_PAT[4]  = {0xF7, 0xFB, 0xFD, 0xFE};
static const unsigned char code ROW_MASK[4] = {0x80, 0x40, 0x20, 0x10};

static unsigned char          key_last   = KEY_NONE;
static unsigned char          key_stable = 0;
static volatile unsigned char key_event  = KEY_NONE;
static volatile unsigned char key_raw    = KEY_NONE;
static bit                    key_held   = 0;
static unsigned char          tick_div   = 0;

void Key_Init(void)
{
	KEY_EN     = 0;
	KEY_PORT   = 0xFF;
	key_last   = KEY_NONE;
	key_stable = 0;
	key_event  = KEY_NONE;
	key_raw    = KEY_NONE;
	key_held   = 0;
	tick_div   = 0;
}

unsigned char Key_ScanRaw(void)
{
	unsigned char col, rows, row, val = KEY_NONE;

	for (col = 0; col < 4; col++)
	{
		KEY_PORT = COL_PAT[col];
		rows = KEY_PORT & 0xF0;
		if (rows != 0xF0)
		{
			for (row = 0; row < 4; row++)
			{
				if (!(rows & ROW_MASK[row]))
				{
#if KEY_INDEX_ROW_MAJOR
					val = (unsigned char)(row * 4 + col + 1);
#else
					val = (unsigned char)(col * 4 + row + 1);
#endif
				}
			}
		}
	}

	KEY_PORT = 0xFF;

	return val;
}

void Key_Tick(void)
{
	unsigned char raw;

	if (Feedback_Busy())
	{
		KEY_PORT = 0xFF;
		tick_div = 0;
		return;
	}

	if (++tick_div < 10) return;
	tick_div = 0;

	raw = Key_ScanRaw();
	key_raw = raw;

	if (raw != key_last)
	{
		key_last   = raw;
		key_stable = 0;
		return;
	}

	if (key_stable < 3)
	{
		key_stable++;
		if (key_stable == 2)
		{
			if (raw != KEY_NONE)
			{
				if (!key_held)
				{
					key_held  = 1;
					key_event = raw;

					Feedback_Normal();
					//推送event
					switch(raw)
					{
						case KEY_0:
							Event_Push(0);
							break;
						case KEY_1:
							Event_Push(1);
							break;
						case KEY_2:
							Event_Push(2);
							break;
						case KEY_3:
							Event_Push(3);
							break;
						case KEY_4:
							Event_Push(4);
							break;
						case KEY_5:	
							Event_Push(5);
							break;
						case KEY_6:
							Event_Push(6);
							break;
						case KEY_7:
							Event_Push(7);
							break;
						case KEY_8:
							Event_Push(8);
							break;
						case KEY_9:	
							Event_Push(9);
							break;
						case KEY_DEL:
							Event_Push(E_DELETE);
							break;
						case KEY_CLR:
							Event_Push(E_CLEAR);
							break;
						case KEY_OK:
							Event_Push(E_OK);
							break;
						case KEY_CAN:
							Event_Push(E_CANCEL);
							break;
						case KEY_LOCK:
							Event_Push(E_LOCK);
							break;
						case KEY_CHG:
							Event_Push(E_MOD);
							break;
						default:
							break;
					}
				}
			}
			else
			{
				key_held = 0;
			}
		}
	}
}

unsigned char Key_GetEvent(void)
{
	unsigned char e;
	bit ea = EA;

	EA = 0;
	e = key_event;
	key_event = KEY_NONE;
	EA = ea;

	return e;
}

unsigned char Key_GetRaw(void)
{
	return key_raw;
}
