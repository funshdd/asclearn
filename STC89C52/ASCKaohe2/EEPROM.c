#include "EEPROM.h"
#include "Delay.h"

#define ROM_WRITE_WAIT_US   600
#define ROM_STATE_SIZE      16

static void ROM_WriteWait(void)
{
	unsigned int i;

	for (i = 0; i < ROM_WRITE_WAIT_US; i++)
	{
		Delay10us();
	}
}

bit ROM_WriteByte(unsigned char addr, unsigned char byte)
{
	IIC_Start();
	IIC_SendByte(ROM_SLAVER_ADDR_WRITE);
	if(IIC_WaitAck())
	{
		IIC_Stop();
		return 1;
	}

	IIC_SendByte(addr);
	if(IIC_WaitAck())
	{
		IIC_Stop();
		return 1;
	}

	IIC_SendByte(byte);
	IIC_Stop();

	return 0;
}

bit ROM_WritePage(unsigned char addr, unsigned char *buffer, unsigned char num)
{
	IIC_Start();
	IIC_SendByte(ROM_SLAVER_ADDR_WRITE);
	if(IIC_WaitAck())
	{
		IIC_Stop();
		return 1;
	}

	IIC_SendByte(addr);
	if(IIC_WaitAck())
	{
		IIC_Stop();
		return 1;
	}

	while(num != 0)
	{
		IIC_SendByte(*buffer);
		if(IIC_WaitAck())
		{
			IIC_Stop();
			return 1;
		}
		buffer++;
		num--;
	}

	IIC_Stop();
	return 0;
}

bit ROM_Read(unsigned char addr, unsigned char *recv_data, unsigned char num)
{
	IIC_Start();
	IIC_SendByte(ROM_SLAVER_ADDR_WRITE);
	if(IIC_WaitAck())
	{
		IIC_Stop();
		return 1;
	}

	IIC_SendByte(addr);
	if(IIC_WaitAck())
	{
		IIC_Stop();
		return 1;
	}

	IIC_Start();
	IIC_SendByte(ROM_SLAVER_ADDR_READ);
	if(IIC_WaitAck())
	{
		IIC_Stop();
		return 1;
	}

	while(num > 1)
	{
		*recv_data = IIC_ReadByte();
		IIC_SendAck(IIC_ACK);
		recv_data++;
		num--;
	}
	*recv_data = IIC_ReadByte();
	IIC_SendAck(IIC_NACK);
	IIC_Stop();

	return 0;
}

bit Rom_SaveState(unsigned char *pwd, unsigned char err, unsigned char stage)
{
	unsigned char data buf[ROM_STATE_SIZE];
	unsigned char i;
	unsigned char sum = 0;

	for (i = 0; i < ROM_STATE_SIZE; i++) buf[i] = 0xFF;

	buf[0] = ROM_STATE_MAGIC1;
	for (i = 0; i < 6; i++) buf[1 + i] = pwd[i];
	buf[7] = (unsigned char)((stage << 4) | (err & 0x0F));

	sum = 0;
	for (i = 0; i < 8; i++) sum = (unsigned char)(sum + buf[i]);

	buf[8] = ROM_STATE_MAGIC2;
	buf[9] = sum;

	if (ROM_WritePage(ROM_STATE_ADDR, buf, 8)) return 1;
	ROM_WriteWait();
	if (ROM_WritePage(ROM_STATE_ADDR + 8, &buf[8], 8)) return 1;
	ROM_WriteWait();

	return 0;
}

bit Rom_LoadState(unsigned char *pwd, unsigned char *err, unsigned char *stage)
{
	unsigned char data buf[ROM_STATE_SIZE];
	unsigned char i;
	unsigned char sum = 0;
	unsigned char e, s;

	if (ROM_Read(ROM_STATE_ADDR, buf, ROM_STATE_SIZE)) return 1;
	if (buf[0] != ROM_STATE_MAGIC1) return 1;
	if (buf[8] != ROM_STATE_MAGIC2) return 1;

	for (i = 0; i < 8; i++) sum = (unsigned char)(sum + buf[i]);
	if (buf[9] != sum) return 1;

	for (i = 0; i < 6; i++)
	{
		if (buf[1 + i] > 9) return 1;
	}
	e = (unsigned char)(buf[7] & 0x0F);
	s = (unsigned char)(buf[7] >> 4);
	if (e > 5 || s > 2) return 1;

	for (i = 0; i < 6; i++) pwd[i] = buf[1 + i];
	*err   = e;
	*stage = s;

	return 0;
}
