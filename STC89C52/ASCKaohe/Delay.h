#ifndef __DELAY_H
#define __DELAY_H

#include <STC89C5xRC.H>
#include <intrins.h>

void Delay200ms(void)	//@11.0592MHz
{
	unsigned char data i, j, k;

	_nop_();
	i = 2;
	j = 103;
	k = 147;
	do
	{
		do
		{
			while (--k);
		} while (--j);
	} while (--i);
}

void Delay500ms(void)	//@11.0592MHz
{
	unsigned char data i, j, k;

	_nop_();
	i = 4;
	j = 129;
	k = 119;
	do
	{
		do
		{
			while (--k);
		} while (--j);
	} while (--i);
}

void Delay20ms(void)	//@11.0592MHz
{
	unsigned char data i, j;

	i = 36;
	j = 217;
	do
	{
		while (--j);
	} while (--i);
}

#endif