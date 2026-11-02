#ifndef __NIXIE_H
#define __NIXIE_H

#include <STC89C5xRC.H>

sbit NA = P2^2;
sbit NB = P2^3;
sbit NC = P2^4;

#define ALL_ON 10
#define ALL_OFF 11
#define NIXIE_DASH 12

extern unsigned char nixie_buffer[8];
extern unsigned char input_pos;

void Nixie_Display(int pos,int num);

void Nixie_AllOff();

void Nixie_Tick();

void Nixie_Clear();

void Nixie_ShowDefault();

#endif
