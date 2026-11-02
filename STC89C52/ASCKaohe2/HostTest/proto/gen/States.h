#ifndef __STATES_H
#define __STATES_H

#define R_OK       0
#define R_WRONG    1
#define R_INCOMP   2
#define R_FULL     3
#define R_MISMATCH 4
#define R_DENIED   5
#define R_BAD      6
#define R_STORAGE  7

extern unsigned char state_result;

void Locked_State_Event(char event);
void Locked_State_Tick(void);

void Unlock_State_Event(char event);
void Unlock_State_Tick(void);

void New1_State_Event(char event);
void New1_State_Tick(void);

void New2_State_Event(char event);
void New2_State_Tick(void);

void Wait30_State_Event(char event);
void Wait30_State_Tick(void);

void Wait60_State_Event(char event);
void Wait60_State_Tick(void);

void Isp_State_Event(char event);
void Isp_State_Tick(void);

void Isp_Start(void);

void Isp_Poll(void);

unsigned char State_LimitSec(void);
unsigned char State_AutoSec(void);

void State_Poll(void);

#endif
