#ifndef __IIC_H
#define __IIC_H

#include <STC89C5xRC.H>

#define IIC_ACK 0
#define IIC_NACK 1

void IIC_Start();

void IIC_Stop();

bit IIC_WaitAck();

void IIC_SendByte(unsigned char byte);

unsigned char IIC_ReadByte();

void IIC_SendAck(unsigned char ack);

#endif
