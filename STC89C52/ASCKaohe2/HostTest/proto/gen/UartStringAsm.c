#include "UartStringAsm.h"
#include "Uart.h"
#include "Event.h"
#include "Nixie.h"
#include "Password.h"
#include "States.h"
#include "Feedback.h"

#define C_BAD      0
#define C_DIGIT    1
#define C_OK       2
#define C_DEL      3
#define C_CLEAR    4
#define C_CHANGE   5
#define C_CANCEL   6
#define C_LOCK     7
#define C_STATUS   8
#define C_VERSION  9
#define C_ISP      10

static char xdata line_buf[UART_CMD_MAX + 1];
static char xdata rep_buf[40];
static unsigned char line_len = 0;
static bit line_ovf = 0;
static unsigned char rep_len = 0;

static void rep_ch(char c)
{
	if(rep_len < 38)
	{
		rep_buf[rep_len] = c;
		rep_len++;
	}
}

static void rep_str(const char code *s)
{
	while(*s != '\0')
	{
		rep_ch(*s);
		s++;
	}
}

static void rep_u8(unsigned char v)
{
	if(v >= 100)
	{
		rep_ch((char)('0' + v / 100));
		v = (unsigned char)(v % 100);
		rep_ch((char)('0' + v / 10));
		rep_ch((char)('0' + v % 10));
	}
	else if(v >= 10)
	{
		rep_ch((char)('0' + v / 10));
		rep_ch((char)('0' + v % 10));
	}
	else
	{
		rep_ch((char)('0' + v));
	}
}

static const char code *state_name(void)
{
	switch(State_GetTop())
	{
		case S_UNLOCK: return "UNLOCKED";
		case S_NEW1:   return "NEW1";
		case S_NEW2:   return "NEW2";
		case S_WAIT30: return "WAIT";
		case S_WAIT60: return "PROTECT";
		case S_ISP:    return "ISP";
		default:       return "LOCKED";
	}
}

static const char code *err_name(unsigned char rc)
{
	switch(rc)
	{
		case R_WRONG:    return "WRONG";
		case R_INCOMP:   return "INCOMPLETE";
		case R_FULL:     return "FULL";
		case R_MISMATCH: return "MISMATCH";
		case R_DENIED:   return "DENIED";
		default:         return "BAD_CMD";
	}
}

static void send_isp_reply(void)
{
	rep_len = 0;
	rep_str("OK ISP");
	rep_str("\r\n");
	rep_buf[rep_len] = '\0';
	Send_String((unsigned char *)rep_buf);
}

static void send_reply(unsigned char rc)
{
	rep_len = 0;

	if(rc == R_OK)
	{
		rep_str("OK ");
	}
	else
	{
		rep_str("ERR ");
		rep_str(err_name(rc));
		rep_ch(' ');
	}

	rep_str(state_name());
	rep_ch(' ');
	rep_u8((unsigned char)(input_pos - 2));
	rep_ch(' ');
	rep_u8(err);
	rep_ch(' ');
	rep_u8(State_LimitSec());
	rep_ch(' ');
	rep_u8(State_AutoSec());
	rep_str("\r\n");

	rep_buf[rep_len] = '\0';
	Send_String((unsigned char *)rep_buf);
}

static unsigned char cmd_eq(const char code *lit, unsigned char len)
{
	unsigned char i = 0;

	while(lit[i] != '\0')
	{
		if(i >= len)
		{
			return 0;
		}

		if(line_buf[i] != lit[i])
		{
			return 0;
		}

		i++;
	}

	return (unsigned char)(i == len);
}

static unsigned char parse_cmd(unsigned char len, unsigned char *arg)
{
	if(len == 0)
	{
		return C_BAD;
	}

	if(len == 7 && cmd_eq("DIGIT", 5) && line_buf[5] == ' ')
	{
		if(line_buf[6] >= '0' && line_buf[6] <= '9')
		{
			*arg = (unsigned char)(line_buf[6] - '0');
			return C_DIGIT;
		}
		return C_BAD;
	}

	if(cmd_eq("OK", len))      return C_OK;
	if(cmd_eq("DEL", len))     return C_DEL;
	if(cmd_eq("CLEAR", len))   return C_CLEAR;
	if(cmd_eq("CHANGE", len))  return C_CHANGE;
	if(cmd_eq("CANCEL", len))  return C_CANCEL;
	if(cmd_eq("LOCK", len))    return C_LOCK;
	if(cmd_eq("STATUS", len))  return C_STATUS;
	if(cmd_eq("VERSION", len)) return C_VERSION;
	if(cmd_eq("ISP", len))     return C_ISP;

	return C_BAD;
}

static unsigned char run_event(char e)
{
	state_result = R_OK;
	Event_Exec(e);
	return state_result;
}

static unsigned char handle_line(unsigned char len)
{
	unsigned char cmd;
	unsigned char arg = 0;
	unsigned char rc = R_OK;

	fb_hint = FB_NONE;

	cmd = parse_cmd(len, &arg);

	switch(cmd)
	{
		case C_DIGIT:
			rc = run_event((char)arg);
			send_reply(rc);
			break;
		case C_OK:
			rc = run_event(E_OK);
			send_reply(rc);
			break;
		case C_DEL:
			rc = run_event(E_DELETE);
			send_reply(rc);
			break;
		case C_CLEAR:
			rc = run_event(E_CLEAR);
			send_reply(rc);
			break;
		case C_CHANGE:
			rc = run_event(E_MOD);
			send_reply(rc);
			break;
		case C_CANCEL:
			rc = run_event(E_CANCEL);
			send_reply(rc);
			break;
		case C_LOCK:
			rc = run_event(E_LOCK);
			send_reply(rc);
			break;
		case C_STATUS:
			rc = R_OK;
			send_reply(rc);
			break;
		case C_VERSION:
			rep_len = 0;
			rep_str("OK VERSION 1.0");
			rep_str("\r\n");
			rep_buf[rep_len] = '\0';
			Send_String((unsigned char *)rep_buf);
			break;
		case C_ISP:
			if(State_GetTop() == S_UNLOCK)
			{
				send_isp_reply();
				Isp_Start();
				rc = R_OK;
			}
			else
			{
				rc = R_DENIED;
				send_reply(rc);
			}
			break;
		default:
			rc = R_BAD;
			send_reply(rc);
			break;
	}

	return rc;
}

void Uart_Tick(void)
{
	unsigned char ch;
	unsigned char rc;

	while(Uart_GetByte(&ch))
	{
		if(ch == '\r')
		{
			continue;
		}

		if(ch == '\n')
		{
			if(line_len == 0 && !line_ovf)
			{
				send_reply(R_BAD);
				Feedback_Error();
				return;
			}

			line_buf[line_len] = '\0';

			if(line_ovf)
			{
				rc = R_BAD;
				send_reply(rc);
			}
			else
			{
				rc = handle_line(line_len);
			}

			if(fb_hint == FB_LIMIT)   Feedback_Limit();
			else if(fb_hint == FB_OK) Feedback_Double();
			else if(rc == R_OK)       Feedback_Normal();
			else                      Feedback_Error();

			fb_hint = FB_NONE;

			line_len = 0;
			line_ovf = 0;
			return;
		}

		if(line_ovf)
		{
			continue;
		}

		if(line_len >= UART_CMD_MAX)
		{
			line_ovf = 1;
			continue;
		}

		line_buf[line_len] = (char)ch;
		line_len++;
	}
}
