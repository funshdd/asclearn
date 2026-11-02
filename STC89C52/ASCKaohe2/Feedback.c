#include "Feedback.h"

#define FB_PAT_NORMAL  1
#define FB_PAT_DOUBLE  2
#define FB_PAT_ERROR   3
#define FB_PAT_LIMIT   4

static const unsigned char code FB_ON[5]  = {0,  30,  50, 200, 100};
static const unsigned char code FB_GAP[5] = {0,   0,  50,   0, 100};
static const unsigned char code FB_CNT[5] = {0,   1,   2,   1,   3};
static const unsigned char code FB_PRIO[5] = {0,  1,   2,   2,   3};

unsigned char fb_hint = FB_NONE;

static unsigned char fb_pat  = 0;
static unsigned char fb_left = 0;
static unsigned char fb_ms   = 0;
static bit           fb_phase = 0;

static void fb_start(unsigned char pat)
{
	if(fb_pat)
	{
		if(FB_PRIO[pat] < FB_PRIO[fb_pat])
		{
			return;
		}
	}

	fb_pat   = pat;
	fb_left  = FB_CNT[pat];
	fb_phase = 1;
	fb_ms    = FB_ON[pat];

	BEEP     = 0;
	LED      = LED_ON;
}

void Feedback_Init(void)
{
	BEEP     = 1;
	LED      = LED_OFF;
	fb_pat   = 0;
	fb_left  = 0;
	fb_ms    = 0;
	fb_phase = 0;
	fb_hint  = FB_NONE;
}

void Feedback_Tick(void)
{
	if(fb_pat == 0)
	{
		return;
	}

	if(fb_ms == 0)
	{
		return;
	}

	fb_ms--;

	if(fb_phase)
	{
		BEEP = !BEEP;
	}

	if(fb_ms != 0)
	{
		return;
	}

	if(fb_phase)
	{
		BEEP     = 1;
		LED      = LED_OFF;
		fb_phase = 0;

		if(fb_left > 0)
		{
			fb_left--;
		}

		if(fb_left == 0)
		{
			fb_pat = 0;
			return;
		}

		fb_ms = FB_GAP[fb_pat];
	}
	else
	{
		fb_phase = 1;
		fb_ms    = FB_ON[fb_pat];

		BEEP = 0;
		LED  = LED_ON;
	}
}

void Feedback_Normal(void)
{
	fb_start(FB_PAT_NORMAL);
}

void Feedback_Double(void)
{
	fb_start(FB_PAT_DOUBLE);
}

void Feedback_Error(void)
{
	fb_start(FB_PAT_ERROR);
}

void Feedback_Limit(void)
{
	fb_start(FB_PAT_LIMIT);
}

bit Feedback_Busy(void)
{
	return fb_phase;
}
