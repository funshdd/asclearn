#include "IIC.h"
#include "Delay.h"

sbit IIC_SCL = P3^4;
sbit IIC_SDA = P3^5;

static void IIC_Delay(void)
{
	Delay10us();
}

void IIC_Start()
{
	IIC_SDA = 1;
	IIC_Delay();
	IIC_SCL = 1;
	IIC_Delay();
	IIC_SDA = 0;
	IIC_Delay();
	IIC_SCL = 0;
	IIC_Delay();
}

void IIC_Stop()
{
	IIC_SDA = 0;
	IIC_Delay();
	IIC_SCL = 1;
	IIC_Delay();
	IIC_SDA = 1;
	IIC_Delay();
}

bit IIC_WaitAck()
{
	bit nack;

	IIC_SDA = 1;
	IIC_Delay();

	IIC_SCL = 1;
	IIC_Delay();

	nack = (IIC_SDA == 1) ? 1 : 0;

	IIC_SCL = 0;
	IIC_Delay();

	return nack;
}

void IIC_SendByte(unsigned char byte)
{
	unsigned char data i = 0;

	for(i=0;i<8;i++)
	{
		IIC_SDA = ((byte & (0x80 >> i)) ? 1 : 0);
		IIC_Delay();

		IIC_SCL = 1;
		IIC_Delay();

		IIC_SCL = 0;
		IIC_Delay();
	}
}

unsigned char IIC_ReadByte()
{
	unsigned char data i = 0;
	unsigned char data read_data = 0;

	IIC_SDA = 1;
	IIC_Delay();

	for(i=0;i<8;i++)
	{
		IIC_SCL = 1;
		IIC_Delay();

		read_data = (unsigned char)(read_data << 1);
		if(IIC_SDA)
		{
			read_data |= 0x01;
		}

		IIC_SCL = 0;
		IIC_Delay();
	}

	return read_data;
}

void IIC_SendAck(unsigned char ack)
{
	IIC_SDA = ack;
	IIC_Delay();

	IIC_SCL = 1;
	IIC_Delay();

	IIC_SCL = 0;
	IIC_Delay();

	IIC_SDA = 1;
	IIC_Delay();
}
