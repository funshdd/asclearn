#ifndef __PASSWORD_H
#define __PASSWORD_H

#include <STC89C5xRC.H>

#define PASSWORD_LEN  6

extern unsigned char password[PASSWORD_LEN];

extern unsigned char err;
extern unsigned char stage;


void Password_Load(void);

#endif
