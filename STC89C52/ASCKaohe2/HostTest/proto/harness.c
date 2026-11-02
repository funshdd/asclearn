#include <stdio.h>
#include <string.h>

#include "STC89C5xRC.H"
#include "Nixie.h"

void Uart_Init(void);
void Uart_Tick(void);
void Uart_Interrupt(void);
unsigned char Uart_TxIdle(void);
void Event_Tick(void);
void State_Poll(void);
void State_Push(unsigned char s);
unsigned char State_GetTop(void);
void Password_Load(void);
void Wait30_State_Tick(void);
void Wait60_State_Tick(void);
void Key_Init(void);
void Feedback_Init(void);

extern unsigned char err;
extern unsigned char stage;
extern unsigned char state_top;

static char out[4096];
static int  outn;

static void pump(void)
{
	int guard = 0;

	while (!Uart_TxIdle() && guard++ < 4000)
	{
		out[outn++] = (char)SBUF;
		TI = 1;
		Uart_Interrupt();
	}

	out[outn] = 0;
}

static void feed(const char *bytes, int len)
{
	int i, k;

	for (i = 0; i < len; i++)
	{
		SBUF = (unsigned char)bytes[i];
		RI = 1;
		Uart_Interrupt();
		Uart_Tick();
		pump();
	}

	for (k = 0; k < 16; k++)
	{
		Uart_Tick();
		Event_Tick();
		State_Poll();
		pump();
	}
}

static int match(const char *got, const char *pat)
{
	while (*pat && *got)
	{
		if (*pat == '?')
		{
			if (*got < '0' || *got > '9') return 0;
			while (got[1] >= '0' && got[1] <= '9') got++;
		}
		else if (*pat != *got)
		{
			return 0;
		}
		pat++;
		got++;
	}

	return (*pat == 0 && *got == 0);
}

static void set_state(unsigned char s)
{
	state_top = 0;
	err       = 0;
	Nixie_Clear();

	State_Push(0);

	switch (s)
	{
		case 0: break;
		case 1: State_Push(1); break;
		case 2: State_Push(1); State_Push(2); break;
		case 3: State_Push(1); State_Push(2); State_Push(3); break;
		case 4: State_Push(4); break;
		case 5: State_Push(5); break;
		default: break;
	}
}

static const char *S_Name(unsigned char s)
{
	switch (s)
	{
		case 0: return "LOCKED";
		case 1: return "UNLOCKED";
		case 2: return "NEW1";
		case 3: return "NEW2";
		case 4: return "WAIT";
		case 5: return "PROTECT";
		default: return "?";
	}
}

static int pass = 0, fail = 0;

static void t(unsigned char state, const char *cmd, const char *expect)
{
	outn = 0;
	set_state(state);
	feed(cmd, (int)strlen(cmd));

	while (outn >= 2 && out[outn - 2] == '\r' && out[outn - 1] == '\n')
	{
		outn -= 2;
	}
	out[outn] = 0;

	if (match(out, expect))
	{
		pass++;
	}
	else
	{
		fail++;
		printf("FAIL [%s] %-16s\n      expect: %s\n      actual: %s\n",
		       S_Name(state), cmd, expect, out);
	}
}

int main(void)
{
	unsigned char s;

	Uart_Init();
	Key_Init();
	Feedback_Init();
	Password_Load();

	/* ---- 表格：所有指令 × 六种状态 ---- */
	static const char *cmds[] = { "DIGIT 0\n", "DEL\n", "CLEAR\n", "OK\n", "CHANGE\n",
	                              "CANCEL\n", "LOCK\n", "STATUS\n", "VERSION\n", "ISP\n",
	                              "FOO\n", "\n", NULL };

	/* LOCKED */
	t(0, "DIGIT 0\n",  "OK LOCKED 1 0 0 0");
	t(0, "DEL\n",      "OK LOCKED 0 0 0 0");
	t(0, "CLEAR\n",    "OK LOCKED 0 0 0 0");
	t(0, "OK\n",       "ERR INCOMPLETE LOCKED 0 0 0 0");
	t(0, "CHANGE\n",   "ERR DENIED LOCKED 0 0 0 0");
	t(0, "CANCEL\n",   "OK LOCKED 0 0 0 0");
	t(0, "LOCK\n",     "OK LOCKED 0 0 0 0");
	t(0, "STATUS\n",   "OK LOCKED 0 0 0 0");
	t(0, "VERSION\n",  "OK VERSION 1.0");
	t(0, "ISP\n",      "ERR DENIED LOCKED 0 0 0 0");

	/* UNLOCKED */
	t(1, "DIGIT 0\n",  "ERR DENIED UNLOCKED 0 0 0 ?");
	t(1, "DEL\n",      "ERR DENIED UNLOCKED 0 0 0 ?");
	t(1, "CLEAR\n",    "ERR DENIED UNLOCKED 0 0 0 ?");
	t(1, "OK\n",       "ERR DENIED UNLOCKED 0 0 0 ?");
	t(1, "CHANGE\n",   "OK NEW1 0 0 0 ?");
	t(1, "CANCEL\n",   "OK UNLOCKED 0 0 0 ?");
	t(1, "LOCK\n",     "OK LOCKED 0 0 0 0");
	t(1, "STATUS\n",   "OK UNLOCKED 0 0 0 ?");
	t(1, "ISP\n",      "OK ISP");
	t(1, "FOO\n",      "ERR BAD_CMD UNLOCKED 0 0 0 ?");

	/* NEW1 */
	t(2, "DIGIT 0\n",  "OK NEW1 1 0 0 ?");
	t(2, "DEL\n",      "OK NEW1 0 0 0 ?");
	t(2, "CLEAR\n",    "OK NEW1 0 0 0 ?");
	t(2, "OK\n",       "ERR INCOMPLETE NEW1 0 0 0 ?");
	t(2, "CHANGE\n",   "ERR DENIED NEW1 0 0 0 ?");
	t(2, "CANCEL\n",   "OK UNLOCKED 0 0 0 ?");
	t(2, "LOCK\n",     "OK LOCKED 0 0 0 0");
	t(2, "STATUS\n",   "OK NEW1 0 0 0 ?");
	t(2, "VERSION\n",  "OK VERSION 1.0");

	/* NEW2 */
	t(3, "DIGIT 0\n",  "OK NEW2 1 0 0 ?");
	t(3, "DEL\n",      "OK NEW2 0 0 0 ?");
	t(3, "CLEAR\n",    "OK NEW2 0 0 0 ?");
	t(3, "OK\n",       "ERR INCOMPLETE NEW2 0 0 0 ?");
	t(3, "CHANGE\n",   "ERR DENIED NEW2 0 0 0 ?");
	t(3, "CANCEL\n",   "OK UNLOCKED 0 0 0 ?");
	t(3, "LOCK\n",     "OK LOCKED 0 0 0 0");
	t(3, "STATUS\n",   "OK NEW2 0 0 0 ?");

	/* WAIT */
	t(4, "DIGIT 0\n",  "ERR DENIED WAIT 0 0 ? 0");
	t(4, "DEL\n",      "ERR DENIED WAIT 0 0 ? 0");
	t(4, "CLEAR\n",    "ERR DENIED WAIT 0 0 ? 0");
	t(4, "OK\n",       "ERR DENIED WAIT 0 0 ? 0");
	t(4, "CHANGE\n",   "ERR DENIED WAIT 0 0 ? 0");
	t(4, "CANCEL\n",   "ERR DENIED WAIT 0 0 ? 0");
	t(4, "LOCK\n",     "OK WAIT 0 0 ? 0");
	t(4, "STATUS\n",   "OK WAIT 0 0 ? 0");
	t(4, "ISP\n",      "ERR DENIED WAIT 0 0 ? 0");

	/* PROTECT */
	t(5, "DIGIT 0\n",  "ERR DENIED PROTECT 0 0 ? 0");
	t(5, "OK\n",       "ERR DENIED PROTECT 0 0 ? 0");
	t(5, "LOCK\n",     "OK PROTECT 0 0 ? 0");
	t(5, "STATUS\n",   "OK PROTECT 0 0 ? 0");

	/* ---- 输入规则 ---- */
	t(0, "\n",              "ERR BAD_CMD LOCKED 0 0 0 0");
	t(0, "\r\n",            "ERR BAD_CMD LOCKED 0 0 0 0");
	t(0, "1\n",             "ERR BAD_CMD LOCKED 0 0 0 0");
	t(0, "digit 0\n",       "ERR BAD_CMD LOCKED 0 0 0 0");
	t(0, "DIGIT 10\n",      "ERR BAD_CMD LOCKED 0 0 0 0");
	t(0, "DIGIT\n",         "ERR BAD_CMD LOCKED 0 0 0 0");
	t(0, "DIGIT  0\n",      "ERR BAD_CMD LOCKED 0 0 0 0");
	t(0, "DIGIT 0 \n",      "ERR BAD_CMD LOCKED 0 0 0 0");
	t(0, "DIGIT00000000000\n", "ERR BAD_CMD LOCKED 0 0 0 0");
	t(0, "STATUS\r\n",      "OK LOCKED 0 0 0 0");

	/* 7 位数字，满位后继续输入 */
	t(0, "DIGIT 1\nDIGIT 2\nDIGIT 3\nDIGIT 4\nDIGIT 5\nDIGIT 6\nDIGIT 7\n",
	     "OK LOCKED 1 0 0 0\r\nOK LOCKED 2 0 0 0\r\nOK LOCKED 3 0 0 0\r\nOK LOCKED 4 0 0 0\r\nOK LOCKED 5 0 0 0\r\nOK LOCKED 6 0 0 0\r\nERR FULL LOCKED 6 0 0 0");

	/* 连发三条合法命令 */
	t(0, "STATUS\nSTATUS\nSTATUS\n",
	     "OK LOCKED 0 0 0 0\r\nOK LOCKED 0 0 0 0\r\nOK LOCKED 0 0 0 0");

	/* 全部数字键 */
	for (s = 0; s < 10; s++)
	{
		char buf[16], exp[48];
		sprintf(buf, "DIGIT %d\n", s);
		sprintf(exp, "OK LOCKED 1 0 0 0");
		t(0, buf, exp);
	}

	/* 超长行：300 字节垃圾，只能回一条 BAD_CMD，且后续命令不受影响 */
	{
		static char big[320];
		int i;
		for (i = 0; i < 299; i++) big[i] = 'A';
		big[299] = '\n';
		big[300] = 0;
		t(0, big, "ERR BAD_CMD LOCKED 0 0 0 0");
	}

	/* 超长行后紧跟合法命令 */
	{
		static char big2[64];
		int i;
		for (i = 0; i < 40; i++) big2[i] = 'X';
		big2[40] = '\n'; big2[41] = 0;
		outn = 0;
		set_state(0);
		feed(big2, 41);
		feed("DIGIT 0\n", 8);
		while (outn >= 2 && out[outn - 2] == '\r' && out[outn - 1] == '\n') outn -= 2;
		out[outn] = 0;
		if (match(out, "ERR BAD_CMD LOCKED 0 0 0 0\r\nOK LOCKED 1 0 0 0")) pass++;
		else { fail++; printf("FAIL 长行后接合法命令\n      actual: %s\n", out); }
	}

	/* 限制中重启：栈底必须有 LOCKED，WAIT 到期后要能正常受理命令 */
	{
		int i;
		state_top = 0; err = 3;
		State_Push(0);
		State_Push(4);
		for (i = 0; i < 30010 && State_GetTop() == 4; i++) Wait30_State_Tick();
		Event_Tick(); State_Poll();
		outn = 0;
		feed("DIGIT 0\n", 8);
		while (outn >= 2 && out[outn-2]=='\r' && out[outn-1]=='\n') outn -= 2;
		out[outn] = 0;
		if (match(out, "OK LOCKED 1 3 0 0")) pass++;
		else { fail++; printf("FAIL WAIT重启后到期\n      expect: OK LOCKED 1 3 0 0\n      actual: %s\n", out); }
	}

	/* 保护中重启：到期后错误次数清零 */
	{
		int i;
		state_top = 0; err = 5;
		State_Push(0);
		State_Push(5);
		for (i = 0; i < 60010 && State_GetTop() == 5; i++) Wait60_State_Tick();
		Event_Tick(); State_Poll();
		outn = 0;
		feed("STATUS\n", 7);
		while (outn >= 2 && out[outn-2]=='\r' && out[outn-1]=='\n') outn -= 2;
		out[outn] = 0;
		if (match(out, "OK LOCKED 0 0 0 0")) pass++;
		else { fail++; printf("FAIL PROTECT重启后到期\n      expect: OK LOCKED 0 0 0 0\n      actual: %s\n", out); }
	}

	printf("\n==== PASS %d   FAIL %d ====\n", pass, fail);
	return fail ? 1 : 0;
}
