#ifndef __EVENT_H
#define __EVENT_H

#include "Nixie.h"

#define EVENT_LIST_SIZE 16
#define STATE_STACK_SIZE 5

//事件码
#define E_DELETE 10
#define E_CLEAR  11
#define E_MOD    12
#define E_LOCK   13
#define E_OK     14
#define E_CANCEL 15

#define SP_STATEPOP 0
#define SP_STATEPUSH 1 //E_STATEPUSH+S_STATE 表示push哪个state

//状态机
#define S_EMPTY 0xFF
#define S_LOCKED 0
#define S_UNLOCK 1
#define S_NEW1   2
#define S_NEW2   3
#define S_WAIT30 4
#define S_WAIT60 5
#define S_ISP    6

extern char event_list[EVENT_LIST_SIZE];
extern unsigned char state_stack[STATE_STACK_SIZE];
extern unsigned char state_process_stack[STATE_STACK_SIZE];
extern unsigned char event_top;
extern unsigned char state_top;
extern unsigned char state_process_top;

bit is_event_empty(void);

void Event_Push(char event);

void Event_Exec(char event);

void Event_Tick(void);

void State_Push(unsigned char state);

void State_Pop(void);

unsigned char State_GetTop(void);

void State_Process_Push(unsigned char state);

void State_Process_Pop(void);

unsigned char State_Process_GetTop(void);

#endif
