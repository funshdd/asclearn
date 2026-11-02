#ifndef __BUTTON_H
#define __BUTTON_H

#include <STC89C5xRC.H>

sbit KEY1 = P3^0;
sbit KEY2 = P3^1;
sbit KEY3 = P3^2;
sbit KEY4 = P3^3;

sbit MATRIXCMD = P2^0;

unsigned char col[4] = { 0xF7, 0xFB, 0xFD, 0xFE };

const unsigned char xdata key_map[4][4] = {
	{ 1, 5,  9, 13 },   // 第1行按键对应值
	{ 2, 6, 10, 14 },   // 第2行按键对应值
	{ 3, 7, 11, 15 },   // 第3行按键对应值
	{ 4, 8, 12, 16 }    // 第4行按键对应值
};

unsigned char matrix_scan()
{
	unsigned char i = 0;
	unsigned char key_value = 0;
	unsigned char key_temp = 0;
	
	for(i = 0;i < 4;i++)
	{
		P1 = col[i];
		
		key_temp = P1 & 0xF0;
		if(key_temp != 0xF0)
		{
			Delay20ms();
			switch(key_temp)
			{
				case 0x70: key_value = key_map[i][0]; break;
				case 0xB0: key_value = key_map[i][1]; break;
				case 0xD0: key_value = key_map[i][2]; break;
				case 0xE0: key_value = key_map[i][3]; break;
			}
			while(P1 != col[i]);
			Delay20ms();
		}
	}
	return key_value;
}

unsigned char matrix_scan_check()
{
	unsigned char i = 0;
	unsigned char key_pressed = 0;
	unsigned char key_temp = 0;
	
	for(i = 0;i < 4;i++)
	{
		P1 = col[i];
		
		key_temp = P1 & 0xF0;
		if(key_temp != 0xF0)
		{
			key_pressed = 1;
		}
	}
	return key_pressed;
}

void matrix_scan_multi(unsigned char *out)
{
    unsigned char i,r;
    unsigned char pv;

    for(i = 0;i < 4;i++)
    {
        P1 = col[i];
        pv = P1 & 0xF0;

        if(pv == 0xF0) 
		{
			continue;
		}
        for(r = 2;r <4;r++)
        {
            unsigned char check = (0x80 >> r);
            if((pv&check) == 0)
            {
				out[key_map[i][r]-9] = 1;
            }
        }
    }

    P1 = 0xFF;
}

void matrix_init()
{
	MATRIXCMD = 0;
}

void matrix_exit()
{
	MATRIXCMD = 1;
}

#endif