/**
  ******************************************************************************
  * @file    test_event.c
  * @brief   事件队列 + 显示缓存的主机侧单元测试
  *
  * 编译运行：
  *     gcc -I HostTest/stub -Wall -o HostTest/test_event.exe HostTest/test_event.c
  *     HostTest\test_event.exe
  ******************************************************************************
  */

#include <stdio.h>
#include <string.h>

#include "../Nixie.h"
#include "../Event.h"

/* 端口桩 */
unsigned char P0_mem;

#include "../Nixie.c"
#include "../Event.c"

static int g_pass = 0, g_fail = 0;

static void expect_int(const char *what, int got, int want)
{
	if (got != want)
	{
		printf("  FAIL  %-28s got=%d want=%d\n", what, got, want);
		g_fail++;
	}
	else
	{
		g_pass++;
	}
}

static void reset_all(void)
{
	unsigned char i;
	for (i = 0; i < 8; i++) nixie_buffer[i] = ALL_OFF;
	input_pos = 2;
	event_top = 0;
}

int main(void)
{
	unsigned char i;

	printf("[1] 单个数字事件只生效一次\n");
	reset_all();
	Event_Push(1);
	Event_Tick();
	expect_int("第 3 位显示 1", nixie_buffer[2], 1);
	expect_int("输入位置推进到 3", input_pos, 3);
	expect_int("队列已清空", event_top, 0);

	Event_Tick();
	Event_Tick();
	expect_int("重复 Tick 不再写入", nixie_buffer[3], ALL_OFF);
	expect_int("输入位置不再推进", input_pos, 3);

	printf("[2] 连续输入 6 位后停止\n");
	reset_all();
	for (i = 0; i < 6; i++)
	{
		Event_Push((char)i);
		Event_Tick();
	}
	expect_int("六位填满后 input_pos", input_pos, 8);
	expect_int("第 8 位是 5", nixie_buffer[7], 5);

	Event_Push(9);
	Event_Tick();
	expect_int("第 7 个数字被丢弃", input_pos, 8);
	expect_int("第 8 位未被覆盖", nixie_buffer[7], 5);

	printf("[3] 删除与清空\n");
	reset_all();
	Event_Push(7);
	Event_Tick();
	Event_Push(E_DELETE);
	Event_Tick();
	expect_int("删除后输入位置回退", input_pos, 2);
	expect_int("删除位显示 '-'", nixie_buffer[2], NIXIE_DASH);

	Event_Push(E_DELETE);
	Event_Tick();
	expect_int("空删除不越过状态位", input_pos, 2);

	Event_Push(3);
	Event_Tick();
	Event_Push(E_CLEAR);
	Event_Tick();
	expect_int("清空后输入位置归零", input_pos, 2);
	expect_int("清空后全灭", nixie_buffer[0], ALL_OFF);
	expect_int("清空后全灭(末位)", nixie_buffer[7], ALL_OFF);

	printf("[4] 队列边界\n");
	reset_all();
	for (i = 0; i < 20; i++) Event_Push(1);
	expect_int("队列长度被限制", event_top, EVENT_LIST_SIZE);
	Event_Tick();
	expect_int("消费后清空", event_top, 0);
	expect_int("16 次按键填满输入区", input_pos, 8);
	expect_int("位置 3 也写入", nixie_buffer[3], 1);

	printf("[5] 空队列\n");
	reset_all();
	Event_Tick();
	expect_int("空队列安全", input_pos, 2);

	printf("\n==== 通过 %d 项，失败 %d 项 ====\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
