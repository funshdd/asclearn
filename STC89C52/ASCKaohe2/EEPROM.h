#ifndef __EEPROM_H
#define __EEPROM_H

#include <STC89C5xRC.H>

#include "IIC.h"

#define ROM_SLAVER_ADDR (0x50)
#define ROM_SLAVER_ADDR_WRITE ((ROM_SLAVER_ADDR<<1)|0x00)
#define ROM_SLAVER_ADDR_READ ((ROM_SLAVER_ADDR<<1)|0x01)

#define ROM_STATE_ADDR      0x00
#define ROM_STATE_MAGIC1    0x5A
#define ROM_STATE_MAGIC2    0xA5

bit ROM_WriteByte(unsigned char addr, unsigned char byte);
bit ROM_WritePage(unsigned char addr, unsigned char *buffer, unsigned char num);
bit ROM_Read(unsigned char addr, unsigned char *recv_data, unsigned char num);

bit Rom_SaveState(unsigned char *pwd, unsigned char err, unsigned char stage);
bit Rom_LoadState(unsigned char *pwd, unsigned char *err, unsigned char *stage);

#endif
