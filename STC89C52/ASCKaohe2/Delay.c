#include "Delay.h"
#include <intrins.h>

void Delay50ms(void)
{
	unsigned char data i, j;

	i = 90;
	j = 163;
	do
	{
		while (--j);
	} while (--i);
}

void Delay10us(void)
{
	unsigned char data i;

	i = 2;
	while (--i);
}
