#ifndef __FEEDBACK_H
#define __FEEDBACK_H

#include <STC89C5xRC.H>

#define BEEP   g_b1_5
#define LED    g_b2_6

#define LED_ON   0
#define LED_OFF  1

#define FB_NONE   0
#define FB_OK     1
#define FB_LIMIT  2

extern unsigned char fb_hint;

void Feedback_Init(void);

void Feedback_Tick(void);

void Feedback_Normal(void);

void Feedback_Double(void);

void Feedback_Error(void);

void Feedback_Limit(void);

bit Feedback_Busy(void);

#endif
