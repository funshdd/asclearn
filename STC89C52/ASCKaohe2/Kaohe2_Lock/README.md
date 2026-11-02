# STC89C52RC 电子密码锁与不断电升级 —— 完整实现

> 本目录是**独立子工程**，与上级目录的临时工程无关，可单独拷贝提交。
> 开发板：野火 STC89C52RC（11.0592MHz）· 编译：Keil uVision C51 · 协议：9600 8N1

---

## 1. 一分钟上手

| 步骤 | 操作 |
|---|---|
| 1 | 用 `HEX/A_D8_OFF.hex` 烧录（这是评测要求的预烧版本：密码 `012345`、锁定、空输入、错误 0） |
| 2 | 打开 `Tools/lock_console.py` 所在目录，执行 `python Tools/lock_console.py --port COM3` 进交互模式，或直接 `--test` 跑自测 |
| 3 | 矩阵键盘：`S1~S9` 数字、`S14`=0、`S4` 确认、`S8` 删除、`S13` 清空、`S12` 修改、`S15` 取消、`S16` 上锁 |

> 首次上电若 EEPROM 是空的，程序自动使用默认密码 `012345` 并处于锁定态。

---

## 2. 交付清单

| 内容 | 位置 | 说明 |
|---|---|---|
| 源码 | `User/`（9 个模块） | 见第 6 节目录结构 |
| A 版 HEX（D8 常暗） | `HEX/A_D8_OFF.hex` | 预烧录用 |
| B 版 HEX（D8 常亮） | `HEX/B_D8_ON.hex` | 升级用 |
| 电脑升级工具 | `Tools/` | 串口控制台 + 自测 + STC-ISP 升级说明 |
| 编译说明 | 本文第 3 节 | 图形界面与命令行两种方式 |
| Keil 工程 | `Project/Kaohe2_Lock.uvproj` | 含 A / B 两个 Target |

---

## 3. 编译

### 3.1 图形界面

1. 打开 `Project/Kaohe2_Lock.uvproj`；
2. 左下角 Target 下拉框选择 `KAOHE2_A_D8_OFF` 或 `KAOHE2_B_D8_ON`；
3. `Project → Rebuild all target files`；
4. 输出：`Objects/Kaohe2_Lock_A.hex`、`Objects/Kaohe2_Lock_B.hex`（六位文件，直接烧录）。

### 3.2 命令行（一键出两份 HEX）

```bat
cd Project
Tools\build.bat            :: 或用真实路径：..\Tools\build.bat
```

`build.bat` 依次批量编译两个 Target。若 Keil 不在默认位置，先设置环境变量：

```bat
set KEIL_UV4=D:\IDE\Keil5\UV4\UV4.exe
```

### 3.3 两份 HEX 的差异

两个 Target 只有 C51 的宏定义不同：

| Target | 宏 | D8(P2.7) | 现象 |
|---|---|---|---|
| `KAOHE2_A_D8_OFF` | `D8_ALWAYS_ON=0` | 输出 1 | D8 常暗 |
| `KAOHE2_B_D8_ON` | `D8_ALWAYS_ON=1` | 输出 0 | D8 常亮 |

实测两份 HEX 逐行比较**只差 1 个字节**（`SETB P2.7` ↔ `CLR P2.7` 及其校验和），业务代码完全一致。

编译结果：`code = 5100 B`（8KB Flash 用 62%）、`data = 110 B`、`xdata = 239 B`。

---

## 4. 串口协议速查

9600 / 8N1 / 无流控；ASCII；一行一条，LF 结束，接受 CRLF；无回显、无主动日志；正文 ≤ 16 字节；
**整行收齐后才执行**，超长或非法整行丢弃至 LF 后回 `BAD_CMD`。

### 4.1 命令

| 命令 | 含义 |
|---|---|
| `DIGIT 0` ~ `DIGIT 9` | 输入一位数字（必须带空格，单独发 `1` 属非法行） |
| `OK` | 确认 |
| `DEL` | 删除末位 |
| `CLEAR` | 清空输入 |
| `CHANGE` | 进入修改密码 |
| `CANCEL` | 取消修改 |
| `LOCK` | 上锁 |
| `STATUS` | 状态查询 |
| `VERSION` | 版本查询 → `OK VERSION 1.0` |
| `ISP` | 申请不断电升级 → `OK ISP`（无状态字段） |

### 4.2 回复格式

```
OK  <S> <N> <E> <R> <T>\r\n
ERR <CODE> <S> <N> <E> <R> <T>\r\n
```

| 字段 | 含义 |
|---|---|
| `S` | 处理后的状态名：`LOCKED / UNLOCKED / NEW1 / NEW2 / WAIT / PROTECT` |
| `N` | 当前输入位数 |
| `E` | 累计错误次数 |
| `R` | 限制剩余秒数，**仅 WAIT / PROTECT 非零** |
| `T` | 自动上锁剩余秒数，**仅 UNLOCKED / NEW1 / NEW2 非零** |

> 说明：题面示例 `OK UNLOCKED 0 0 0 15` 与 `ERR WRONG WAIT 0 3 30 0` 中的第 3、4 个数字分别是「限制倒计时」和「自动上锁倒计时」，因此回复是 `S N E R T` 五个字段。10 组示例逐条对齐（见第 7 节自测结果）。

错误码：`WRONG / INCOMPLETE / FULL / MISMATCH / DENIED / BAD_CMD / STORAGE`。

### 4.3 指令 × 状态

| 指令 | LOCKED | UNLOCKED | NEW1 | NEW2 | WAIT | PROTECT |
|---|---|---|---|---|---|---|
| DIGIT n | 追加 / 满则 FULL | DENIED | 追加 / FULL | 追加 / FULL | DENIED | DENIED |
| DEL | 删末位 | DENIED | 删末位 | 删末位 | DENIED | DENIED |
| CLEAR | 清空 | DENIED | 清空 | 清空 | DENIED | DENIED |
| OK | 不足 INCOMPLETE / 满则校验 | DENIED | 不足 INCOMPLETE / 满则→NEW2 | 不足 / 一致→保存，否则 MISMATCH | DENIED | DENIED |
| CHANGE | DENIED | →NEW1 | DENIED | DENIED | DENIED | DENIED |
| CANCEL | 原态 | 原态 | →UNLOCKED 丢弃 | →UNLOCKED 丢弃 | DENIED | DENIED |
| LOCK | 清空 | →LOCKED | →LOCKED 丢弃 | →LOCKED 丢弃 | 原态、不重计时 | 原态、不重计时 |
| STATUS | OK | OK | OK | OK | OK | OK |
| VERSION | OK | OK | OK | OK | OK | OK |
| ISP | DENIED | **OK ISP** | DENIED | DENIED | DENIED | DENIED |

---

## 5. 关键规则实现要点

### 5.1 三次 / 五次错误与保护

- 第 1、2、4 次错误：`ERR WRONG`，仍 `LOCKED`，`E` 递增；
- **第 3 次**：进入 `WAIT`，`R=30`；30 秒到期后回 `LOCKED`，**错误次数保留 3**；
- **第 5 次**：进入 `PROTECT`，`R=60`；60 秒到期后回 `LOCKED`，**错误次数清零**；
- WAIT/PROTECT 期间只允许 `STATUS`/`VERSION`，`LOCK` 返回 `OK` 但不解除限制、不重新计时。

### 5.2 15 秒自动上锁

只覆盖 `UNLOCKED / NEW1 / NEW2`。有效业务操作（数字、删除非空、清空非空、确认满位、进入或取消改密）会刷新；
**查询、拒绝、非法、空删除/空清空、不足位确认、已满追加都不刷新**。
到期静默跳回 `LOCKED` 且**不主动回复**；命令处理前先结算超时，所以“到点那一刻收到命令”一定先上锁再返回 `ERR DENIED`。

### 5.3 断电保留

保存密码（6 位数字，天然保留前导零）、错误次数、限制阶段（正常/WAIT/PROTECT）。
限制中重启 → 重新等待**完整** 30 / 60 秒（不保存剩余秒数，避免每秒擦写 EEPROM）。

### 5.4 不断电升级

`UNLOCKED` 下收到 `ISP` → 先回 `OK ISP` → 串口发送完成后再等 **500 ms**（非阻塞）→
`ISP_CONTR = 0x60`（SWBS=1 从 ISP 区启动 + SWRST=1 软复位）→ 芯片进入 ISP 引导区等待下载。
EEPROM 不擦除，所以升级后的新程序仍然是“锁定 + 保留已保存密码”，并且**还能再次升级**。
数码管在等待期间显示状态码 `6`。

---

## 6. 代码结构与“为什么不会堵”

```
User/
  config.h   引脚 / 时序 / 编译开关（D8 版本、蜂鸣器类型、版本号）
  board.c    板级初始化 + 声光反馈（只设剩余毫秒数，翻转在中断里做）
  timer.c    Timer0 1ms 节拍：数码管换位 + 蜂鸣器方波 + 键盘 10ms 采样
  uart.c     串口中断收发 + 环形缓冲 + 发送队列（发送不等待）
  nixie.c    8 位数码管动态扫描（消影 + code 区段码表）
  key.c      4x4 矩阵键盘（10ms 采样、20ms 消抖、边沿触发、长按只算一次）
  iap.c      片内 EEPROM：双扇区 + 32 槽日志，写空槽不擦除，掉电不丢旧值
  lock.c     状态机 + 10 条命令 + 回复拼装 + 显示缓存
  main.c     主循环：Poll → 取行 → 解析 → 键盘事件 → 20ms 刷新显示
```

**没有一处 `while` 等待**（除上电初始化和 ISP 软复位），具体对照：

| 常规写法（会被判“堵塞”） | 本实现 |
|---|---|
| 数码管 `Delay1ms(1)` 逐位刷新 | 1ms 中断换位，125Hz 刷新 |
| `while(!KEY);` 等松手 | 10ms 采样 + 边沿检测，长按只触发一次 |
| `while(!TI);` 阻塞发字节 | 64/48 字节发送队列，中断逐字节送出 |
| `Delay1ms(500)` 等 ISP | 毫秒时间戳比较，主循环继续跑 |
| 蜂鸣器 `Delay20us` 方波 | 1ms 节拍翻转 P1.5（500Hz），不占 CPU |
| 每秒擦写 EEPROM 存倒计时 | 只存阶段与错误次数，按秒刷新在 RAM |

**蜂鸣器与矩阵键盘复用 P1.5 的处理**：扫描期间把行线强制拉高，扫完立刻恢复方波电平
（`Feedback_RestoreBeep()`），因此第 3 行按键（`7 8 9 修改`）始终能正确识别，声音也不会断成噪音。

**内存布局**：`data` 110B / `xdata` 239B / `code` 5100B，全部落在 STC89C52RC 的 128B data + 256B xram + 8KB flash 之内。

---

## 7. 自测

### 7.1 主机侧单元测试（不需要硬件）

`host/` 里面把 `lock.c` 直接 `#include` 进 gcc 编译，用桩函数替换串口、EEPROM、数码管，
逐条回放题面的 10 组示例和边界用例：

```bat
gcc -I host -Wall -Wextra -o host/test_lock.exe host/test_lock.c
host\test_lock.exe
```

当前结果：**72 项全部通过，0 失败**，覆盖题面 10 组示例、15/30/60 秒时序、断电保留、改密回滚、
指令×状态全表、数码管显示内容、ISP 500ms 软复位时机。

### 7.2 真机自测（需要开发板）

```bat
python Tools\lock_console.py --port COM3 --test          :: 约 60 秒，含 15 秒超时验证
python Tools\lock_console.py --port COM3 --test --full   :: 额外验证 30/60 秒限制
```

工具会逐项打印 `PASS/FAIL`，结束时自动把板子恢复成“锁定 + 空输入 + 错误 0 + 默认密码”。

### 7.3 手工验收清单

- [ ] 上电显示 `0 0 - - - - - -`（状态码 0、错误 0、输入全 `-`）
- [ ] 按 `1 2 3` 显示 `0 0 1 2 3 - - -`，蜂鸣器与 D7 有反馈，100ms 内响应
- [ ] 按住某键不放，只触发一次
- [ ] 解锁后显示 `1 0 - - - - 1 5` 并逐秒递减
- [ ] 输错 3 次显示 `4 3 0 0 0 0 3 0`（状态 4、错误 3、剩余 30 秒补零）
- [ ] 限制中按键盘无效果，串口 `STATUS` 仍可查
- [ ] 断电重启后密码/错误次数仍在，WAIT/PROTECT 重新计时
- [ ] 串口 `ISP` 后数码管显示 `6`，STC-ISP 能完成下载，新程序锁定且密码未丢
- [ ] A 版 D8 常暗、B 版 D8 常亮

---

## 8. 引脚分配

| 外设 | 引脚 | 备注 |
|---|---|---|
| 数码管段码 | `P0` | 经 74HC245 |
| 数码管位选 | `P2.2 / P2.3 / P2.4` | 74HC138 的 A/B/C |
| 矩阵键盘 | `P1` 低四位列、高四位行 | 行 1~4 = `P1.7 / P1.6 / P1.5 / P1.4` |

键盘按**行优先**编号（与题面表格一致）：`键号 = 行*4 + 列 + 1`

```
行1(P1.7)  S1=1     S2=2     S3=3     S4=确认
行2(P1.6)  S5=4     S6=5     S7=6     S8=删除
行3(P1.5)  S9=7     S10=8    S11=9    S12=修改
行4(P1.4)  S13=清空  S14=0    S15=取消  S16=上锁
```

> 若实机按键功能整体沿主对角线镜像，说明板子是列优先排布，把
> `config.h` 的 `KEY_INDEX_ROW_MAJOR` 改成 `0` 即可（默认 `1`）。
| 矩阵键盘使能 | `P2.0` | 必须为低（屏蔽 RTC_RST 复用） |
| 蜂鸣器 | `P1.5` | 与行 3 复用，见第 6 节 |
| 反馈灯 D7 | `P2.6` | 低电平点亮 |
| 版本灯 D8 | `P2.7` | A=1 常暗 / B=0 常亮 |
| 串口 | `P3.0 / P3.1` | 9600 8N1 |

---

## 9. 需要按实机确认的两点

1. **蜂鸣器类型**：`config.h` 的 `BEEP_ACTIVE`（默认 0 = 无源，用 500Hz 方波驱动）。
   若板上是**有源**蜂鸣器，把它改成 `1`，改为直接给电平。
2. **`VERSION` 回复**：当前实现为 `OK VERSION 1.0`（版本号在 `config.h` 的 `VERSION_STR`）。
   若评测要求带状态字段，改 `lock.c` 的 `send_reply()` 里那两行即可。
3. **矩阵键盘编号方向**：`config.h` 的 `KEY_INDEX_ROW_MAJOR` 默认 `1`（行优先，键号 = 行*4+列+1，
   与题面 S1~S16 表格一致）。若实机按键沿主对角线镜像，改成 `0` 切回列优先。

---

## 10. 与上级目录临时工程的关系

上级目录的 `main.c` / `Uart.c` / `Nixie.c` 等是早期草稿（`#incldue` 拼写错误、串口 CRLF 双触发、
阻塞发送等），本子工程**没有改动它们任何一个文件**，只在自己的目录内独立完整实现。
