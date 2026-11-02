#ifndef __TICK_H
#define __TICK_H

#include <STC89C5xRC.H>

extern volatile unsigned int tick_count;

unsigned int Tick_Millis(void);

void Timer2_Init(void);

void Timer0_Init(void);

#endif
