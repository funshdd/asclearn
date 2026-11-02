#include "Delay.h"
#include "Nixie.h"
#include "Button.h"
#include "Timer.h"
#include "LCD.h"
#include "Calc.h"
#include "Ledmatrix.h"
#include "Uart.h"

#include <string.h>

int mode = 1;
char run = 0;

//开始动画
char is_start=1;

//数码管
unsigned int nixie_blink_count = 0;
char nixie_on = 1;

//模式1
unsigned int led_count = 0;
unsigned int led_duration = 200;
unsigned char led_current = 0;
unsigned char direction = 0;//0右1左
unsigned char led_rev_mode = 0;//0直接返回到第一个，1则反方向

//模式一附加
unsigned char xdata led_list[8][8]={0};
unsigned char edit_mode = 0,edit_pos=0,is_edited=0;
char itt = 0,sitt=0;
unsigned char clr= 0;

//模式2
unsigned char xdata text[16] = {0};
unsigned char cursor_pos = 0;

//模式3
unsigned int asc_count=0,asc_pos=0;
unsigned char xdata asc[48] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x70,0x90,0x70,0x0F,0x00,0x70,0x89,0x89,0x89,0x06,0x00,0x7E,0x81,0x81,0x81,
0x00,0xF1,0x49,0x49,0x4E,0x00,0x43,0x85,0x89,0xF1,0x00,0x7E,0x81,0x81,0x81,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

//模式4
unsigned char buzz_on=0,led_on=0;
unsigned char pwm_count = 0;
unsigned char ratio = 5;
	
void uart_interrupt(void) interrupt 4
{
	ES = 0;

    if (RI)
    {
        data_read = SBUF;
        RI = 0;
		
		mode = data_read - '0';
		
		run = 1;
		nixie_display(1,mode);
		//enter mode x
		
		if(mode == 1)
		{
			send_string("enter mode 1");
			matrix_init();
		}
		
		if(mode == 2)
		{
			send_string("enter mode 2");
			matrix_init();
		}
		
		if(mode == 3)
		{
			send_string("enter mode 3");
			led_matrix_init();
		}
		
		if(mode == 4)
		{
			send_string("enter mode 4");
			//冲突，led改为数码管显示，蜂鸣器因为冲突的原因无法工作
			matrix_init();
		}	
		
    }
    else
    {
        return;
    }
	ES = 1;
}

void timer_interrupt(void) interrupt 1
{
	TL0 = 0x66;
	TH0 = 0xFC;
	
	if(is_start)
		return;
	
	nixie_blink_count++;//1ms
	if(nixie_blink_count >= 300 && !run)
	{
		nixie_blink_count = 0;
		nixie_on = !nixie_on;
		if(nixie_on)
		{
			nixie_display(1,mode);
		}
		else
		{
			nixie_display(1,ALL_OFF);
		}
	}
	
	if(run && mode == 1 && edit_mode == 1 && matrix_scan_check())
	{
		if(led_list[edit_pos][sitt]==1)
			nixie_display(sitt+1,ALL_ON);
		sitt++;
		if(sitt>=8)
			sitt=0;
	}
	
	if(run && mode == 1 && edit_mode == 0)
	{
		if(is_edited == 1)
		{
			for(itt = 0;itt<8;itt++)
			{
				if(led_list[led_current][itt]==1)
					nixie_display(itt+1,ALL_ON);
			}
		}
		
		led_count++;
		if(led_count>=led_duration)
		{
			led_count = 0;
			if(is_edited)
			{
				led_current++;
				if(led_current==8)
					led_current = 0;
			}
			else
			{
				switch(led_rev_mode)
				{
					case 0:
						if(direction==0)
							led_current++;
						else
							led_current--;
						if(led_current==8)
						{
							led_current = 0;
						}
						if(led_current==-1)
						{
							led_current = 7;
						}
						break;
					case 1:
						if(direction==0)
							led_current++;
						else
							led_current--;
						if(led_current==8)
						{
							led_current = 6;
							direction = !direction;
						}
						if(led_current==-1)
						{
							led_current = 1;
							direction = !direction;
						}
						break;
				}		
				//P2 = ~(0x01<<led_current); 由于我的开发板P2_0为矩阵键盘开启位，有冲突，所以无法使用LED，改为数码管1~8位对应LED1~8
				nixie_display(led_current+1,ALL_ON);
			}
			
		}
	}

	if(run && mode == 2)
	{
	}
	
	if(run&&mode ==3)
	{
		asc_count++;
		if(asc_count>=200)
		{
			asc_pos++;
			asc_count=0;
			if(asc_pos>=48)
			{
				asc_pos=0;
			}
		}
	}
	
	if(run&&mode == 4)
	{
		pwm_count++;
		if(pwm_count>=10)
		{
			pwm_count = 0;
		}
		if(pwm_count>=ratio)
		{
			if(led_on)
			{
				nixie_display(1,8);
			}
		}
		else
		{
			if(led_on)
			{
				nixie_display(1,ALL_OFF);
			}
		}
	}
}

void init_all()
{
	P2=0xFF;
	Timer0_Init();
	//maxtrix_init();
	LCD_Init();
	uart_init();
}

void play_startup()
{
	char p2v=0,cur=0x01,i;
	
	for(i=0;i<8;i++)
	{
		p2v+=cur;
		cur = cur<<1;
		P2=~p2v;
		Delay200ms();
	}
	
	Delay200ms();
	P2 = 0xFF;
	Delay200ms();
	P2 = 0x00;	
	Delay200ms();
	P2 = 0xFF;
	Delay200ms();
	P2 = 0x00;
	Delay200ms();
	P2 = 0xFF;
	
	is_start = 0;
}

int main()
{
	play_startup();
	init_all();
	
	nixie_display(1,mode);
	
	while(1)
	{
		if(KEY1 == 0 && !run)//前
		{
			Delay20ms();while(!KEY1);Delay20ms();
			if(mode!=1)mode--;
			nixie_display(1,mode);
			nixie_on = 1;
		}
		
		if(KEY2 == 0 && !run)//后
		{
			Delay20ms();while(!KEY2);Delay20ms();
			if(mode!=4)mode++;
			nixie_display(1,mode);
			nixie_on = 1;
		}
		
		if(KEY3 == 0 && !run)//确认
		{
			Delay20ms();while(!KEY3);Delay20ms();
			
			run = 1;
			nixie_display(1,mode);
			//enter mode x
			
			if(mode == 1)
			{
				send_string("enter mode 1");
				matrix_init();
			}
			
			if(mode == 2)
			{
				send_string("enter mode 2");
				matrix_init();
			}
			
			if(mode == 3)
			{
				send_string("enter mode 3");
				led_matrix_init();
			}
			
			if(mode == 4)
			{
				send_string("enter mode 4");
				//冲突，led改为数码管显示，蜂鸣器因为冲突的原因无法工作
				matrix_init();
			}	
		}
		
		if(KEY4 == 0)//退出
		{
			Delay20ms();while(!KEY4);Delay20ms();
			
			nixie_display(1,mode);
			nixie_on = 1;
			run = 0;
			
			if(mode==1)
			{
				send_string("exit mode 1");
				P2 = 0xFF;
				led_count = 0;
				led_duration = 200;
				led_current = 0;
				direction = 0;//0右1左
				led_rev_mode = 0;//0回到第一个，1反方向
				matrix_exit();
			}
			
			if(mode==2)
			{
				send_string("exit mode 2");
				matrix_exit();
			}
			
			if(mode == 3)
			{
				send_string("exit mode 3");
			}
			
			if(mode == 4)
			{
				send_string("exit mode 4");
				matrix_exit();
			}
		}
		
		//模式逻辑
		
		if(run&&mode == 1&&edit_mode==0)//MODE1
		{
			unsigned char key_value = matrix_scan();
			if(key_value == 1)
			{
				if(led_duration != 50)
				{
					send_string("freq +");
					led_duration -=50;
				}
			}
			if(key_value == 2)
			{
				if(led_duration != 1000)
				{
					send_string("freq -");
					led_duration +=50;
				}
			}
			if(key_value == 3)
			{
				send_string("direction change");
				direction=!direction;
			}
			if(key_value == 4)
			{
				send_string("led mode change");
				led_rev_mode = !led_rev_mode;
			}
			
			if(key_value == 5)
			{
				send_string("enter led edit mode");
				edit_mode=1;
				for(itt = 0;itt<8;itt++)
				{
					for(clr =0;clr<8;clr++)
					{
						led_list[itt][clr]=0;
					}
					nixie_display(itt+1,ALL_OFF);
				}
			}
				
//			if(edit_mode && key_value<=16&&key_value>=9)
//			{
//				led_list[edit_pos] = key_value-8;
//				nixie_display(led_list[edit_pos],ALL_ON);
//				edit_pos++;
//				if(edit_pos==8)
//				{
//					edit_mode = 0;
//					edit_pos = 0;
//					is_edited =1;
//					led_current = 0;
//				}
//			}
		}
		
		if(run&&mode == 1&&edit_mode==1)
		{
			if(matrix_scan_check())
			{
				while(matrix_scan_check())
				{
					matrix_scan_multi(&led_list[edit_pos]);
				}
				Delay20ms();
				
				for(itt = 0;itt<8;itt++)
				{
					if(led_list[edit_pos][itt]==1)
					{
						nixie_display(itt+1,ALL_OFF);
					}
				}
				
				edit_pos++;
				
				if(edit_pos==8)
				{
					edit_mode = 0;
					edit_pos = 0;
					is_edited =1;
					led_current = 0;
				}
			}
		}
		
		if(run&&mode == 2)
		{
			unsigned char key_value = matrix_scan();
			if(key_value<=10&&key_value>=1)
			{
				text[cursor_pos] = '0'+key_value-1;
				LCD_ShowString(1 , 1 , &text);
				cursor_pos++;
			}
			
			if(key_value<=16&&key_value>=13)
			{
				switch(key_value)
				{
					case 13:text[cursor_pos] = '+';break;
					case 14:text[cursor_pos] = '-';break;
					case 15:text[cursor_pos] = '*';break;
					case 16:text[cursor_pos] = '/';break;
				}
				LCD_ShowString(1 , 1 , &text);
				cursor_pos++;
			}
			
			if(key_value == 11)
			{
				memset(text, '\0', sizeof(text));
				LCD_ShowString(1 , 1 , "                ");
				cursor_pos = 0;
			}
			
			if(key_value == 12)
			{
				//calc
				float res = calc(text);
				memset(text, '\0', sizeof(text));
				LCD_ShowString(1 , 1 , "                ");
				LCD_DisplayNumber(1,1,res);
			}
		}
		
		if(run&&mode == 3)
		{
			unsigned char it=0;
			for(it=0;it<8;it++)
				led_matrix_showcolumn(it,asc[asc_pos+8-it]);
		}
		
		if(run&&mode==4)
		{
			unsigned char key_value = matrix_scan();
			if(key_value==1)
			{
				send_string("pwm ratio -");
				if(ratio!=1)ratio--;
			}

			if(key_value==2)
			{
				send_string("pwm ratio +");
				if(ratio!=10)ratio++;
			}
			
			if(key_value==3)
			{
				led_on = !led_on;
				if(led_on==0)
					nixie_display(1,ALL_OFF);
			}
		}
	}
}