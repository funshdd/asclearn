#include "States.h"
#include "Event.h"
#include "Password.h"
#include "EEPROM.h"
#include "Feedback.h"

#define INPUT_START 2
#define INPUT_END   8

static unsigned int unlock_counter = 15000;
static unsigned int wait30_counter = 30000;
static unsigned int wait60_counter = 60000;
static unsigned char new_password[PASSWORD_LEN] = {0, 0, 0, 0, 0, 0};
static bit save_pending = 0;
static unsigned int isp_counter = 0;

unsigned char state_result = R_OK;

static void save_state(void)
{
	unsigned char st;

	if (State_GetTop() == S_WAIT30)      st = 1;
	else if (State_GetTop() == S_WAIT60) st = 2;
	else                                 st = 0;

	if(Rom_SaveState(password, err, st) != 0)
	{
		Rom_SaveState(password, err, st);
	}
}

static void show_head(unsigned char sc)
{
	nixie_buffer[0] = sc;
	nixie_buffer[1] = err;
}

static void show_wait_sec(unsigned int ms)
{
	unsigned int sec = (ms + 999) / 1000;

	nixie_buffer[2] = NIXIE_DASH;
	nixie_buffer[3] = NIXIE_DASH;
	nixie_buffer[4] = NIXIE_DASH;
	nixie_buffer[5] = NIXIE_DASH;
	nixie_buffer[6] = (unsigned char)(sec / 10);
	nixie_buffer[7] = (unsigned char)(sec % 10);
}

void Locked_State_Event(char event)
{
	unsigned char i;
	bit ok;

    if(event<=9&&event>=0)
    {
        if (input_pos < INPUT_END)
        {
            nixie_buffer[input_pos] = (unsigned char)event;
            input_pos++;
        }
        else
        {
            state_result = R_FULL;
        }
        return;
    }

    switch (event)
    {
        case E_DELETE:
            if (input_pos > INPUT_START)
            {
                input_pos--;
                nixie_buffer[input_pos] = NIXIE_DASH;
                unlock_counter = 15000;
            }
            break;
        case E_CLEAR:
            if (input_pos > INPUT_START)
            {
                unlock_counter = 15000;
            }
			Nixie_Clear();
            break;
        case E_MOD:
            state_result = R_DENIED;
            break;
        case E_LOCK:
			Nixie_Clear();
            break;
        case E_OK:
            if (input_pos < INPUT_END)
            {
                state_result = R_INCOMP;
                break;
            }

            ok = 1;
            for (i = 0; i < PASSWORD_LEN; i++)
            {
                if (nixie_buffer[INPUT_START + i] != password[i])
                {
                    ok = 0;
                    break;
                }
            }

            if (ok)
            {
                err = 0;
                unlock_counter = 15000;
                Nixie_Clear();
                State_Push(S_UNLOCK);
                save_state();
                fb_hint = FB_OK;
            }
            else
            {
                state_result = R_WRONG;

                if (err < 5)
                {
                    err++;
                }

                if (err == 3)
                {
                    wait30_counter = 30000;
                    State_Push(S_WAIT30);
                    fb_hint = FB_LIMIT;
                }
                else if (err == 5)
                {
                    wait60_counter = 60000;
                    State_Push(S_WAIT60);
                    fb_hint = FB_LIMIT;
                }

                Nixie_Clear();
                save_state();
            }
            break;
        case E_CANCEL:

            break;
        default:

            break; 
    }
}

void Locked_State_Tick(void)
{
    show_head(S_LOCKED);
}

void Unlock_State_Event(char event)
{    
    state_result = R_DENIED;

    if(event<=9&&event>=0)
    {
        
        return;
    }

    switch (event)
    {
        case E_DELETE:

            break;
        case E_CLEAR:

            break;
        case E_MOD:
            state_result = R_OK;
            unlock_counter = 15000;
            Nixie_Clear();
            State_Push(S_NEW1);
            break;
        case E_LOCK:
            state_result = R_OK;
            unlock_counter = 15000;
            Nixie_Clear();
            State_Pop();
            break;
        case E_OK:

            break;
        case E_CANCEL:
            state_result = R_OK;
            break;
        default:

            break; 
    }    
}

void Unlock_State_Tick(void)
{
    unsigned int sec;

    show_head(S_UNLOCK);

    if (unlock_counter > 0)
    {
        unlock_counter--;

        sec = (unlock_counter + 999) / 1000;
        nixie_buffer[6] = (unsigned char)(sec / 10);
        nixie_buffer[7] = (unsigned char)(sec % 10);
    }
    else
    {
        Nixie_Clear();
        State_Pop();
    }
}

void New1_State_Event(char event)
{    
    unsigned char i;
    if(event<=9&&event>=0)
    {
        if (input_pos < INPUT_END)
        {
            nixie_buffer[input_pos] = (unsigned char)event;
            input_pos++;
            unlock_counter = 15000;
        }
        else
        {
            state_result = R_FULL;
        }
        return;
    }

    switch (event)
    {
        case E_DELETE:
            if (input_pos > INPUT_START)
            {
                input_pos--;
                nixie_buffer[input_pos] = NIXIE_DASH;
                unlock_counter = 15000;
            }
            break;
        case E_CLEAR:
            if (input_pos > INPUT_START)
            {
                unlock_counter = 15000;
            }
			Nixie_Clear();
            break;
        case E_MOD:
            state_result = R_DENIED;
            break;
        case E_LOCK://LOCK -> UNLOCK -> NEW1    
            Nixie_Clear();
            State_Pop();//POP NEW1
            State_Pop();//POP UNLOCK
            break;
        case E_OK:
            if (input_pos < INPUT_END)
            {
                state_result = R_INCOMP;
                break;
            }
            for (i = 0; i < PASSWORD_LEN; i++)
            {
                new_password[i] = nixie_buffer[INPUT_START + i];
            }
            unlock_counter = 15000;
            Nixie_Clear();
            State_Push(S_NEW2);//PUSH NEW2
            break;
        case E_CANCEL:
            Nixie_Clear();
            State_Pop();//POP NEW1
            unlock_counter = 15000;
            break;
        default:

            break; 
    }
}

void New1_State_Tick(void)
{
    show_head(S_NEW1);

    if (unlock_counter > 0)
    {
        unlock_counter--;
    }
    else
    {
        Nixie_Clear();
        State_Pop();//POP NEW1
        State_Pop();//POP UNLOCK
    }
}

void New2_State_Event(char event)
{    
    unsigned char i;
    if(event<=9&&event>=0)
    {
        if (input_pos < INPUT_END)  
        {
            nixie_buffer[input_pos] = (unsigned char)event;
            input_pos++;
            unlock_counter = 15000;
        }
        else
        {
            state_result = R_FULL;
        }
        return;
    }

    switch (event)
    {
        case E_DELETE:
            if (input_pos > INPUT_START)
            {
                input_pos--;
                nixie_buffer[input_pos] = NIXIE_DASH;
                unlock_counter = 15000;
            }
            break;
        case E_CLEAR:
            if (input_pos > INPUT_START)
            {
                unlock_counter = 15000;
            }
            Nixie_Clear();
            break;
        case E_MOD:
            state_result = R_DENIED;
            break;
        case E_LOCK://LOCK -> UNLOCK -> NEW1 -> NEW2
            Nixie_Clear();
            State_Pop();//POP NEW2
            State_Pop();//POP NEW1
            State_Pop();//POP UNLOCK
            
            break;
        case E_OK:
            if (input_pos < INPUT_END)
            {
                state_result = R_INCOMP;
                break;
            }
            for (i = 0; i < PASSWORD_LEN; i++)
            {
                if (nixie_buffer[INPUT_START + i] != new_password[i])
                {
                    state_result = R_MISMATCH;
                    Nixie_Clear();
                    State_Pop();//POP NEW2
                    State_Pop();//POP NEW1
                    unlock_counter = 15000;
                    return;
                }
            }
            if (Rom_SaveState(new_password, err, 0) == 0)
            {
                for (i = 0; i < PASSWORD_LEN; i++)
                {
                    password[i] = new_password[i];
                }
                fb_hint = FB_OK;
            }
            else
            {
                state_result = R_STORAGE;
            }

            Nixie_Clear();
            State_Pop();//POP NEW2
            State_Pop();//POP NEW1
            unlock_counter = 15000;
            break;
        case E_CANCEL:
            Nixie_Clear();
            State_Pop();//POP NEW2
            State_Pop();//POP NEW1
            unlock_counter = 15000;
            break;
        default:

            break; 
    }
}

void New2_State_Tick(void)
{
    show_head(S_NEW2);

    if (unlock_counter > 0)
    {
        unlock_counter--;
    }
    else
    {
        Nixie_Clear();
        State_Pop();//POP NEW2
        State_Pop();//POP NEW1
        State_Pop();//POP UNLOCK
    }
}

void Wait30_State_Event(char event)
{    
    state_result = R_DENIED;

    if(event<=9&&event>=0)
    {
        
        return;
    }

    switch (event)
    {
        case E_DELETE:

            break;
        case E_CLEAR:

            break;
        case E_MOD:

            break;
        case E_LOCK:
            state_result = R_OK;
            break;
        case E_OK:

            break;
        case E_CANCEL:

            break;
        default:

            break; 
    }
}

void Wait30_State_Tick(void)
{
    show_head(S_WAIT30);

    if(wait30_counter > 0)
    {
        wait30_counter--;
        show_wait_sec(wait30_counter);
    }
    else
    {//LOCK -> WAIT30
        Nixie_Clear();
        State_Pop();//POP WAIT30
        save_pending = 1;
    }
}

void Wait60_State_Event(char event)
{    
    state_result = R_DENIED;

    if(event<=9&&event>=0)
    {
        
        return;
    }

    switch (event)
    {
        case E_DELETE:

            break;
        case E_CLEAR:

            break;
        case E_MOD:

            break;
        case E_LOCK:
            state_result = R_OK;
            break;
        case E_OK:

            break;
        case E_CANCEL:

            break;
        default:

            break; 
    }
}

void Wait60_State_Tick(void)
{
    show_head(S_WAIT60);

    if(wait60_counter > 0)
    {
        wait60_counter--;
        show_wait_sec(wait60_counter);
    }
    else
    {//LOCK -> WAIT60
        Nixie_Clear();
        err = 0;
        State_Pop();//POP WAIT60
        save_pending = 1;
    }
}

/*
    if(event<=9&&event>=0)
    {
        
        return;
    }

    switch (event)
    {
        case E_DELETE:

            break;
        case E_CLEAR:

            break;
        case E_MOD:

            break;
        case E_LOCK:

            break;
        case E_OK:

            break;
        case E_CANCEL:

            break;
        default:

            break; 
    }
*/

unsigned char State_LimitSec(void)
{
	unsigned int v = 0;
	unsigned char st;
	bit ea;

	ea = EA;
	EA = 0;

	st = State_GetTop();

	if(st == S_WAIT30)      v = wait30_counter;
	else if(st == S_WAIT60) v = wait60_counter;

	EA = ea;

	return (unsigned char)((v + 999) / 1000);
}

unsigned char State_AutoSec(void)
{
	unsigned int v = 0;
	unsigned char st;
	bit ea;

	ea = EA;
	EA = 0;

	st = State_GetTop();

	if(st == S_UNLOCK || st == S_NEW1 || st == S_NEW2) v = unlock_counter;

	EA = ea;

	return (unsigned char)((v + 999) / 1000);
}

void State_Poll(void)
{
	if(save_pending)
	{
		save_pending = 0;
		save_state();
	}
}

void Isp_Start(void)
{
	isp_counter = 500;
	Nixie_Clear();
	State_Push(S_ISP);
}

void Isp_State_Event(char event)
{
	state_result = R_DENIED;
}

void Isp_State_Tick(void)
{
	show_head(6);

	if(isp_counter > 0)
	{
		isp_counter--;
	}
}

void Isp_Poll(void)
{
	if(State_GetTop() != S_ISP)
	{
		return;
	}

	if(isp_counter > 0)
	{
		return;
	}

	if(!Uart_TxIdle())
	{
		return;
	}

	ISP_CONTR = 0x60;

	while(1);
}
