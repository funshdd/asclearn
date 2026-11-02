#include "Password.h"
#include "EEPROM.h"

unsigned char password[PASSWORD_LEN] = {0, 0, 0, 0, 0, 0};

unsigned char err = 0;
unsigned char stage = 0;

void Password_Load(void)
{
	unsigned char i;

	if (Rom_LoadState(password, &err, &stage) != 0)
	{
		for (i = 0; i < PASSWORD_LEN; i++)
		{
			password[i] = i;
		}
	}
}
