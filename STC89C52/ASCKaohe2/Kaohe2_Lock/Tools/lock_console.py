#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
电子密码锁 · 上位机验收工具（纯标准库，不依赖 pyserial）

用法：
    python lock_console.py --port COM3            交互式发送命令，看回复
    python lock_console.py --port COM3 --test      跑一遍自动回归用例（约 10 秒）
    python lock_console.py --port COM3 --test --full   额外验证 30 秒暂停 / 60 秒保护

说明：
    1. 直接调用 Windows 的 kernel32 完成串口收发，考试机上只要能跑 Python 就能用。
    2. 发送 '\n' 结尾、9600/8N1，与题面协议一致。
    3. 自动测试会把密码改回去（默认 012345），结束时回到“锁定 + 无输入”状态。
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import sys
import time

# --------------------------------------------------------------------------
# 极简 Windows 串口封装（ctypes，无第三方依赖）
# --------------------------------------------------------------------------

GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
kernel32.CreateFileW.restype = wt.HANDLE
kernel32.CreateFileW.argtypes = [wt.LPCWSTR, wt.DWORD, wt.DWORD, wt.LPVOID,
                                 wt.DWORD, wt.DWORD, wt.HANDLE]
kernel32.CloseHandle.argtypes = [wt.HANDLE]
kernel32.ReadFile.argtypes = [wt.HANDLE, wt.LPVOID, wt.DWORD,
                              ctypes.POINTER(wt.DWORD), wt.LPVOID]
kernel32.WriteFile.argtypes = [wt.HANDLE, wt.LPVOID, wt.DWORD,
                               ctypes.POINTER(wt.DWORD), wt.LPVOID]
kernel32.SetCommState.argtypes = [wt.HANDLE, wt.LPVOID]
kernel32.GetCommState.argtypes = [wt.HANDLE, wt.LPVOID]
kernel32.SetCommTimeouts.argtypes = [wt.HANDLE, wt.LPVOID]
kernel32.PurgeComm.argtypes = [wt.HANDLE, wt.DWORD]


class DCB(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("DCBlength", wt.DWORD),
        ("BaudRate", wt.DWORD),
        ("flags", wt.DWORD),          # fBinary/fDtrControl/fRtsControl 等位域
        ("wReserved", wt.WORD),
        ("XonLim", wt.WORD),
        ("XoffLim", wt.WORD),
        ("ByteSize", wt.BYTE),
        ("Parity", wt.BYTE),
        ("StopBits", wt.BYTE),
        ("XonChar", ctypes.c_char),
        ("XoffChar", ctypes.c_char),
        ("ErrorChar", ctypes.c_char),
        ("EofChar", ctypes.c_char),
        ("EvtChar", ctypes.c_char),
        ("wReserved1", wt.WORD),
    ]


class COMMTIMEOUTS(ctypes.Structure):
    _fields_ = [
        ("ReadIntervalTimeout", wt.DWORD),
        ("ReadTotalTimeoutMultiplier", wt.DWORD),
        ("ReadTotalTimeoutConstant", wt.DWORD),
        ("WriteTotalTimeoutMultiplier", wt.DWORD),
        ("WriteTotalTimeoutConstant", wt.DWORD),
    ]


class SerialPort:
    """9600 / 8N1 / 无流控"""

    def __init__(self, port, baud=9600):
        path = "\\\\.\\%s" % port
        self.h = kernel32.CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
                                      0, None, OPEN_EXISTING, 0, None)
        if not self.h or self.h == wt.HANDLE(-1).value:
            raise OSError("打开 %s 失败（错误码 %d），请检查串口号是否被占用"
                          % (port, ctypes.get_last_error()))

        dcb = DCB()
        dcb.DCBlength = ctypes.sizeof(DCB)
        dcb.BaudRate = baud
        # fBinary=1, fDtrControl=01, fTXContinueOnXoff=1, fRtsControl=01
        dcb.flags = 0x0001 | (1 << 4) | (1 << 7) | (1 << 12)
        dcb.ByteSize = 8
        dcb.Parity = 0
        dcb.StopBits = 0
        if not kernel32.SetCommState(self.h, ctypes.byref(dcb)):
            raise OSError("配置串口参数失败（错误码 %d）" % ctypes.get_last_error())

        # 读：缓冲区里有数据就立刻返回；写：最多等 2 秒
        to = COMMTIMEOUTS(0xFFFFFFFF, 0, 0, 0, 2000)
        kernel32.SetCommTimeouts(self.h, ctypes.byref(to))
        kernel32.PurgeComm(self.h, 0x000F)

    def write(self, data):
        n = wt.DWORD(0)
        buf = ctypes.create_string_buffer(data, len(data))
        if not kernel32.WriteFile(self.h, buf, len(data), ctypes.byref(n), None):
            raise OSError("串口写失败（错误码 %d）" % ctypes.get_last_error())
        return n.value

    def read_available(self):
        out = b""
        chunk = ctypes.create_string_buffer(256)
        while True:
            n = wt.DWORD(0)
            ok = kernel32.ReadFile(self.h, chunk, 256, ctypes.byref(n), None)
            if not ok or n.value == 0:
                break
            out += chunk.raw[:n.value]
        return out

    def read_line(self, timeout=1.5):
        """读到 LF 为止（表现为收到一条完整回复）"""
        buf = b""
        deadline = time.time() + timeout
        while time.time() < deadline:
            buf += self.read_available()
            if b"\n" in buf:
                break
            time.sleep(0.01)
        return buf

    def close(self):
        if self.h:
            kernel32.CloseHandle(self.h)
            self.h = None


# --------------------------------------------------------------------------
# 协议测试
# --------------------------------------------------------------------------

class Runner:
    def __init__(self, sp, echo=False):
        self.sp = sp
        self.echo = echo
        self.ok = 0
        self.fail = 0
        self.notes = []

    # 发一条命令，返回去掉 CRLF 的回复
    def cmd(self, line, timeout=1.5):
        self.sp.read_available()                    # 丢掉残留
        self.sp.write((line + "\n").encode("ascii"))
        raw = self.sp.read_line(timeout)
        text = raw.decode("ascii", "replace").replace("\r", "").strip("\n")
        if self.echo:
            print("   > %-18s %s" % (line, text))
        return text

    @staticmethod
    def _tokens(s):
        return s.split()

    def check(self, name, line, want, tol=1, timeout=1.5):
        """比较回复：允许最后两个数字字段有 ±tol 秒的计时误差"""
        got = self.cmd(line, timeout)
        g, w = self._tokens(got), self._tokens(want)
        good = len(g) == len(w)
        if good:
            for i, (a, b) in enumerate(zip(g, w)):
                if a == b:
                    continue
                near_end = i >= len(w) - 2
                if near_end and a.isdigit() and b.isdigit() and abs(int(a) - int(b)) <= tol:
                    continue
                good = False
                break
        if good:
            self.ok += 1
            print("  PASS  %-26s %s" % (name, got))
        else:
            self.fail += 1
            print("  FAIL  %-26s\n        got  : %s\n        want : %s"
                  % (name, got or "<无回复>", want))
        return got

    def note(self, text):
        self.notes.append(text)


def run_tests(sp, full=False):
    r = Runner(sp, echo=False)

    st = r.cmd("STATUS")
    print("当前状态：%s" % (st or "<无回复>"))
    if not st.startswith("OK LOCKED 0 0"):
        print("\n[提示] 建议先把开发板断电重上电（或按复位），"
              "让状态回到“锁定 + 空输入 + 错误 0”再测。\n")

    print("\n-- P01 基础输入与解锁 --")
    r.check("输入一位数字", "DIGIT 9", "OK LOCKED 1 0 0 0")
    r.check("不足六位确认", "OK", "ERR INCOMPLETE LOCKED 1 0 0 0")
    r.check("清空", "CLEAR", "OK LOCKED 0 0 0 0")
    r.check("删除空输入", "DEL", "OK LOCKED 0 0 0 0")
    for d in "01234":
        r.cmd("DIGIT " + d)
    r.check("第五位后仍锁定", "STATUS", "OK LOCKED 5 0 0 0")
    r.cmd("DIGIT 5")
    r.check("满六位后追加", "DIGIT 7", "ERR FULL LOCKED 6 0 0 0")
    r.check("密码正确解锁", "OK", "OK UNLOCKED 0 0 0 15")

    print("\n-- P02 组合操作一致性 --")
    r.check("解锁后禁止输入", "DIGIT 1", "ERR DENIED UNLOCKED 0 0 0 15")
    r.check("解锁后禁止确认", "OK", "ERR DENIED UNLOCKED 0 0 0 15")
    r.check("解锁后状态查询", "STATUS", "OK UNLOCKED 0 0 0 15")
    r.check("进入改密", "CHANGE", "OK NEW1 0 0 0 15")
    r.check("取消改密", "CANCEL", "OK UNLOCKED 0 0 0 15")
    r.check("上锁", "LOCK", "OK LOCKED 0 0 0 0")
    r.check("锁定态禁止改密", "CHANGE", "ERR DENIED LOCKED 0 0 0 0")
    r.check("锁定态禁止 ISP", "ISP", "ERR DENIED LOCKED 0 0 0 0")

    print("\n-- P03 协议健壮性 --")
    r.check("非法命令", "FOO", "ERR BAD_CMD LOCKED 0 0 0 0")
    r.check("小写命令", "digit 0", "ERR BAD_CMD LOCKED 0 0 0 0")
    r.check("裸数字", "1", "ERR BAD_CMD LOCKED 0 0 0 0")
    r.check("多余参数", "DEL 1", "ERR BAD_CMD LOCKED 0 0 0 0")
    r.check("空行", "", "ERR BAD_CMD LOCKED 0 0 0 0")
    r.check("版本查询", "VERSION", "OK VERSION 1.0")
    over = "DIGIT " + "0" * 11                       # 17 字节超长行
    r.check("超长行丢弃", over, "ERR BAD_CMD LOCKED 0 0 0 0")
    r.check("超长行后仍可用", "STATUS", "OK LOCKED 0 0 0 0")

    print("\n-- P04 双次改密与校验 --")
    for d in "012345":
        r.cmd("DIGIT " + d)
    r.cmd("OK")
    r.cmd("CHANGE")
    for d in "654321":
        r.cmd("DIGIT " + d)
    r.check("第一次新密码", "OK", "OK NEW2 0 0 0 15")
    for d in "654320":
        r.cmd("DIGIT " + d)
    r.check("两次不一致", "OK", "ERR MISMATCH UNLOCKED 0 0 0 15")
    r.cmd("LOCK")
    for d in "012345":
        r.cmd("DIGIT " + d)
    r.check("原密码仍有效", "OK", "OK UNLOCKED 0 0 0 15")
    r.cmd("CHANGE")
    for d in "000123":
        r.cmd("DIGIT " + d)
    r.cmd("OK")
    for d in "000123":
        r.cmd("DIGIT " + d)
    r.check("改密成功", "OK", "OK UNLOCKED 0 0 0 15")
    r.cmd("LOCK")
    for d in "000123":
        r.cmd("DIGIT " + d)
    r.check("前导零新密码生效", "OK", "OK UNLOCKED 0 0 0 15")
    r.cmd("CHANGE")                                  # 改回默认密码
    for d in "012345":
        r.cmd("DIGIT " + d)
    r.cmd("OK")
    for d in "012345":
        r.cmd("DIGIT " + d)
    r.check("恢复默认密码", "OK", "OK UNLOCKED 0 0 0 15")
    r.cmd("LOCK")

    print("\n-- P05 超时自动上锁 --")
    for d in "012345":
        r.cmd("DIGIT " + d)
    r.cmd("OK")
    print("  等待 16 秒，观察是否自动上锁（此间不要发命令）...")
    time.sleep(16)
    r.check("15 秒后自动上锁", "STATUS", "OK LOCKED 0 0 0 0")

    print("\n-- P06 错误计数与保护 --")
    st = r.cmd("STATUS")
    if not st.startswith("OK LOCKED 0"):
        print("  [跳过] 错误次数非 0，重新上电后单独验证本节")
    else:
        for i in range(1, 3):
            for d in "999999":
                r.cmd("DIGIT " + d)
            r.check("第 %d 次错误" % i, "OK", "ERR WRONG LOCKED 0 %d 0 0" % i)
        for d in "999999":
            r.cmd("DIGIT " + d)
        r.check("第 3 次错误暂停", "OK", "ERR WRONG WAIT 0 3 30 0")
        r.check("暂停中拒绝输入", "DIGIT 1", "ERR DENIED WAIT 0 3 30 0")
        r.check("暂停中上锁不解除", "LOCK", "OK WAIT 0 3 30 0")
        if full:
            print("  等待 31 秒，验证暂停到期后错误次数保留 3 ...")
            time.sleep(31)
            r.check("暂停到期保留 3 次", "STATUS", "OK LOCKED 0 3 0 0")
            for d in "999999":
                r.cmd("DIGIT " + d)
            r.check("第 4 次错误仍锁定", "OK", "ERR WRONG LOCKED 0 4 0 0")
            for d in "999999":
                r.cmd("DIGIT " + d)
            r.check("第 5 次错误进入保护", "OK", "ERR WRONG PROTECT 0 5 60 0")
            print("  等待 61 秒，验证保护到期后错误次数清零 ...")
            time.sleep(61)
            r.check("保护到期清零", "STATUS", "OK LOCKED 0 0 0 0")
        else:
            print("  [--full] 未指定，跳过 30 秒 / 60 秒的等待验证")

    print("\n-- 收尾：恢复默认状态（锁定 / 空输入 / 错误 0 / 密码 012345） --")
    st = r.cmd("STATUS")
    waited = 0
    while not st.startswith("OK LOCKED") and waited < 70:
        time.sleep(1)
        waited += 1
        if waited % 10 == 0:
            print("  等待限制解除... %d 秒" % waited)
        st = r.cmd("STATUS")
    if st.startswith("OK LOCKED"):
        # 正确解锁会把错误次数清零
        for d in "012345":
            r.cmd("DIGIT " + d)
        r.cmd("OK")
        r.cmd("LOCK")
    print("  最终状态：%s" % r.cmd("STATUS"))

    print("\n==== 通过 %d 项，失败 %d 项 ====" % (r.ok, r.fail))
    return 0 if r.fail == 0 else 1


def run_console(sp):
    print("交互模式：回车发送命令。可用命令：")
    print("  DIGIT 0~9 / OK / DEL / CLEAR / CHANGE / CANCEL / LOCK / STATUS / VERSION / ISP")
    print("  空行 = 直接回车（测非法行），输入 quit 退出\n")
    while True:
        try:
            line = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            break
        if line.lower() in ("quit", "exit"):
            break
        sp.read_available()
        sp.write((line + "\n").encode("ascii", "ignore"))
        raw = sp.read_line(2.0)
        text = raw.decode("ascii", "replace").replace("\r", "").strip("\n")
        print("< %s" % (text if text else "<无回复>"))
        if line.strip().upper() == "ISP":
            print("  [提示] 已进入升级等待，接下来请在 STC-ISP 中点击“下载/编程”完成升级。")


def main():
    ap = argparse.ArgumentParser(description="电子密码锁上位机验收工具")
    ap.add_argument("--port", required=True, help="串口号，如 COM3")
    ap.add_argument("--test", action="store_true", help="运行自动回归用例")
    ap.add_argument("--full", action="store_true", help="连 30s/60s 限制一起验证")
    args = ap.parse_args()

    try:
        sp = SerialPort(args.port)
    except OSError as e:
        print("打开串口失败：%s" % e)
        return 2

    try:
        if args.test:
            return run_tests(sp, args.full)
        run_console(sp)
        return 0
    finally:
        sp.close()


if __name__ == "__main__":
    sys.exit(main())
