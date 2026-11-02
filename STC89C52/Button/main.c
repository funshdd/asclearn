#include <REGX52.H>

void main()
{
	unsigned char count = 0;
	P2_0 = 0;
	
	while(1)
	{
		if (P1_0==0)
		{
			while(P1_0==0);
			count++;
			P2 = ~count; 
		}
	}
}