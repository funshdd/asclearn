#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
void Stub_EeReset(void);
void Key_Init(void);
void Feedback_Init(void);

void Locked_State_Tick(void);
void Unlock_State_Tick(void);
void New1_State_Tick(void);
void New2_State_Tick(void);
void Wait30_State_Tick(void);
void Wait60_State_Tick(void);

extern unsigned char err;
extern unsigned char stage;
extern unsigned char state_top;
extern unsigned char password[];

static char out[2048];
static int  outn;

/* ---------------- 固件侧 ---------------- */

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

static void fw_feed(const char *bytes, int len)
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

	for (k = 0; k < 8; k++)
	{
		Uart_Tick();
		Event_Tick();
		State_Poll();
		pump();
	}
}

static void fw_tick1(void)
{
	switch (State_GetTop())
	{
		case 0: Locked_State_Tick(); break;
		case 1: Unlock_State_Tick(); break;
		case 2: New1_State_Tick();   break;
		case 3: New2_State_Tick();   break;
		case 4: Wait30_State_Tick(); break;
		case 5: Wait60_State_Tick(); break;
		default: break;
	}
	Event_Tick();
	State_Poll();
}

static void fw_advance(unsigned int ms)
{
	unsigned int i;
	for (i = 0; i < ms; i++) fw_tick1();
}

static void fw_reboot(void)
{
	state_top = 0;
	Password_Load();
	Nixie_ShowDefault();
	State_Push(0);
	if (stage == 1)      State_Push(4);
	else if (stage == 2) State_Push(5);
}

/* ---------------- 参考模型 ---------------- */

typedef struct
{
	unsigned char st;
	unsigned char err;
	unsigned char n;
	unsigned char cur[6];
	unsigned char tmp[6];
	unsigned int  wait_ms;
	unsigned int  unlock_ms;
} Model;

static Model M;
static char  rep[256];

static unsigned int ceil_ms(unsigned int ms)
{
	return (ms + 999) / 1000;
}

static const char *m_name(void)
{
	switch (M.st)
	{
		case 1: return "UNLOCKED";
		case 2: return "NEW1";
		case 3: return "NEW2";
		case 4: return "WAIT";
		case 5: return "PROTECT";
		default: return "LOCKED";
	}
}

static void m_reply(const char *ecode, unsigned int is_err)
{
	unsigned int r = 0, t = 0;

	if (M.st == 4 || M.st == 5) r = ceil_ms(M.wait_ms);
	if (M.st == 1 || M.st == 2 || M.st == 3) t = ceil_ms(M.unlock_ms);

	if (!is_err)
	{
		sprintf(rep, "OK %s %d %d %u %u", m_name(), M.n, M.err, r, t);
	}
	else
	{
		sprintf(rep, "ERR %s %s %d %d %u %u", ecode, m_name(), M.n, M.err, r, t);
	}
}

static void m_tick1(void)
{
	if (M.st == 1 || M.st == 2 || M.st == 3)
	{
		if (M.unlock_ms > 0) M.unlock_ms--;
		if (M.unlock_ms == 0) { M.st = 0; M.n = 0; }
	}
	else if (M.st == 4 || M.st == 5)
	{
		if (M.wait_ms > 0) M.wait_ms--;
		if (M.wait_ms == 0)
		{
			if (M.st == 5) M.err = 0;
			M.st = 0;
			M.n  = 0;
		}
	}
}

static void m_advance(unsigned int ms)
{
	unsigned int i;
	for (i = 0; i < ms; i++) m_tick1();
}

static void m_reboot(void)
{
	if (M.st == 4)      { M.st = 4; M.wait_ms = 30000; }
	else if (M.st == 5) { M.st = 5; M.wait_ms = 60000; }
	else                { M.st = 0; }

	M.n = 0;
}

static unsigned char m_can_input(void)
{
	return (M.st == 0 || M.st == 2 || M.st == 3) ? 1 : 0;
}

static void m_refresh(void)
{
	if (M.st == 2 || M.st == 3) M.unlock_ms = 15000;
}

static void m_cmd(const char *cmd)
{
	unsigned char i, same;

	if (strcmp(cmd, "STATUS") == 0)  { m_reply("", 0); return; }
	if (strcmp(cmd, "VERSION") == 0) { strcpy(rep, "OK VERSION 1.0"); return; }

	if (strcmp(cmd, "FOO") == 0) { m_reply("BAD_CMD", 1); return; }

	if (M.st == 4 || M.st == 5)
	{
		if (strcmp(cmd, "LOCK") == 0) { m_reply("", 0); return; }
		m_reply("DENIED", 1);
		return;
	}

	if (cmd[0] == 'D' && cmd[1] == 'I')
	{
		if (!m_can_input()) { m_reply("DENIED", 1); return; }

		if (M.n >= 6) { m_reply("FULL", 1); return; }

		M.cur[M.n] = (unsigned char)(cmd[6] - '0');
		M.n++;
		m_refresh();
		m_reply("", 0);
		return;
	}

	if (strcmp(cmd, "DEL") == 0)
	{
		if (!m_can_input()) { m_reply("DENIED", 1); return; }
		if (M.n > 0) { M.n--; m_refresh(); }
		m_reply("", 0);
		return;
	}

	if (strcmp(cmd, "CLEAR") == 0)
	{
		if (!m_can_input()) { m_reply("DENIED", 1); return; }
		if (M.n > 0) { M.n = 0; m_refresh(); }
		m_reply("", 0);
		return;
	}

	if (strcmp(cmd, "OK") == 0)
	{
		if (!m_can_input()) { m_reply("DENIED", 1); return; }
		if (M.n < 6) { m_reply("INCOMPLETE", 1); return; }

		if (M.st == 0)
		{
			same = 1;
			for (i = 0; i < 6; i++)
			{
				if (M.cur[i] != password[i]) { same = 0; break; }
			}

			M.n = 0;

			if (same)
			{
				M.err = 0;
				M.st  = 1;
				M.unlock_ms = 15000;
				m_reply("", 0);
			}
			else
			{
				if (M.err < 5) M.err++;
				if (M.err == 3)      { M.st = 4; M.wait_ms = 30000; }
				else if (M.err == 5) { M.st = 5; M.wait_ms = 60000; }
				m_reply("WRONG", 1);
			}
			return;
		}

		if (M.st == 2)
		{
			for (i = 0; i < 6; i++) M.tmp[i] = M.cur[i];
			M.n = 0;
			M.st = 3;
			M.unlock_ms = 15000;
			m_reply("", 0);
			return;
		}

		same = 1;
		for (i = 0; i < 6; i++)
		{
			if (M.cur[i] != M.tmp[i]) { same = 0; break; }
		}

		M.n = 0;

		if (!same)
		{
			M.st = 1;
			M.unlock_ms = 15000;
			m_reply("MISMATCH", 1);
			return;
		}

		for (i = 0; i < 6; i++) password[i] = M.cur[i];
		M.st = 1;
		M.unlock_ms = 15000;
		m_reply("", 0);
		return;
	}

	if (strcmp(cmd, "CHANGE") == 0)
	{
		if (M.st != 1) { m_reply("DENIED", 1); return; }
		M.st = 2;
		M.n  = 0;
		M.unlock_ms = 15000;
		m_reply("", 0);
		return;
	}

	if (strcmp(cmd, "CANCEL") == 0)
	{
		if (M.st == 2 || M.st == 3)
		{
			M.st = 1;
			M.n  = 0;
			M.unlock_ms = 15000;
			m_reply("", 0);
			return;
		}
		m_reply("", 0);
		return;
	}

	if (strcmp(cmd, "LOCK") == 0)
	{
		M.st = 0;
		M.n  = 0;
		m_reply("", 0);
		return;
	}

	m_reply("BAD_CMD", 1);
}

/* ---------------- 随机对拍 ---------------- */

static const char *CMDS[] =
{
	"DIGIT 0\n", "DIGIT 1\n", "DIGIT 2\n", "DIGIT 3\n", "DIGIT 4\n",
	"DIGIT 5\n", "DIGIT 6\n", "DIGIT 7\n", "DIGIT 8\n", "DIGIT 9\n",
	"DEL\n", "CLEAR\n", "OK\n", "CHANGE\n", "CANCEL\n", "LOCK\n",
	"STATUS\n", "VERSION\n", "FOO\n"
};

static const char *SHORT[] =
{
	"DIGIT 0", "DIGIT 1", "DIGIT 2", "DIGIT 3", "DIGIT 4",
	"DIGIT 5", "DIGIT 6", "DIGIT 7", "DIGIT 8", "DIGIT 9",
	"DEL", "CLEAR", "OK", "CHANGE", "CANCEL", "LOCK",
	"STATUS", "VERSION", "FOO"
};

int main(int argc, char **argv)
{
	int round, i;
	int fail = 0;
	long seed = (argc > 1) ? atol(argv[1]) : 12345;

	srand((unsigned)seed);

	Uart_Init();
	Key_Init();
	Feedback_Init();
	Password_Load();

	for (round = 0; round < 300 && fail < 10; round++)
	{
		state_top = 0;
		State_Push(0);
		Stub_EeReset();
		err = 0;
		Nixie_Clear();
		for (i = 0; i < 6; i++) password[i] = (unsigned char)i;

		memset(&M, 0, sizeof(M));

		for (i = 0; i < 60 && fail < 10; i++)
		{
			unsigned int adv;
			int k = rand() % 19;

			if ((rand() % 25) == 0)
			{
				fw_reboot();
				m_reboot();

				outn = 0;
				fw_feed("STATUS\n", 7);
				while (outn >= 2 && out[outn - 2] == '\r' && out[outn - 1] == '\n') outn -= 2;
				out[outn] = 0;
				m_cmd("STATUS");

				if (strcmp(out, rep) != 0)
				{
					fail++;
					printf("--- 轮 %d 步 %d 重启后 ---\n  模型 %s\n  固件 %s\n", round, i, rep, out);
				}
				continue;
			}

			adv = (unsigned int)(rand() % 300);
			if ((rand() % 35) == 0) adv = 16000;
			if ((rand() % 80) == 0) adv = 31000;

			fw_advance(adv);
			m_advance(adv);

			outn = 0;
			fw_feed(CMDS[k], (int)strlen(CMDS[k]));
			while (outn >= 2 && out[outn - 2] == '\r' && out[outn - 1] == '\n') outn -= 2;
			out[outn] = 0;

			m_cmd(SHORT[k]);

			if (err != M.err)
			{
				fail++;
				printf("--- 轮 %d 步 %d err 不一致 ---\n  命令 %s 推进 %u ms\n  模型 err=%d st=%d\n  固件 err=%d st=%d\n",
				       round, i, SHORT[k], adv, M.err, M.st, err, State_GetTop());
				continue;
			}

			if (strcmp(out, rep) != 0)
			{
				fail++;
				printf("--- 轮 %d 步 %d ---\n", round, i);
				printf("  命令 %s   推进 %u ms\n", SHORT[k], adv);
				printf("  模型 %s\n", rep);
				printf("  固件 %s\n", out);
			}
		}
	}

	printf("\n==== 不一致 %d 次（seed=%ld）====\n", fail, seed);
	return fail ? 1 : 0;
}
