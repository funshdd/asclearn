$ErrorActionPreference = 'Stop'
$root = 'D:\Projects\Keil5\89C51\ASCKaohe2'
$proto = Join-Path $root 'HostTest\proto'
$gen = Join-Path $proto 'gen'

if (Test-Path $gen) { Remove-Item $gen -Recurse -Force }
New-Item -ItemType Directory -Force -Path $gen | Out-Null

# 收集工程源文件与头文件
$srcs = @('Uart.c','UartStringAsm.c','Event.c','States.c','Nixie.c','Password.c','Key.c','Feedback.c')
$hdrs = @('Uart.h','UartStringAsm.h','Event.h','States.h','Nixie.h','Password.h','Key.h','Feedback.h','EEPROM.h')

foreach ($f in ($srcs + $hdrs)) { Copy-Item (Join-Path $root $f) $gen -Force }
Copy-Item (Join-Path $proto 'STC89C5xRC.H') $gen -Force

# 把 sbit 声明换成普通全局变量别名
$map = @{
    'sbit BEEP = P1^5;'   = '#define BEEP   g_b1_5'
    'sbit LED  = P2^6;'   = '#define LED    g_b2_6'
    'sbit KEY_EN = P2^0;' = '#define KEY_EN g_b2_0'
    'sbit NA = P2^2;'     = '#define NA     g_b2_2'
    'sbit NB = P2^3;'     = '#define NB     g_b2_3'
    'sbit NC = P2^4;'     = '#define NC     g_b2_4'
}

Get-ChildItem (Join-Path $gen '*') -Include *.c,*.h -File | ForEach-Object {
    $text = Get-Content $_.FullName -Raw -Encoding UTF8
    foreach ($k in $map.Keys) { $text = $text.Replace($k, $map[$k]) }
    # 去掉 C51 的 interrupt 关键字
    $text = [regex]::Replace($text, '\s+interrupt\s+\d+', '')
    # 去掉 extern ... code 修饰在函数返回值上的残留（本工程未用，保险起见）
    Set-Content -Path $_.FullName -Value $text -Encoding UTF8 -NoNewline
}

$gcc = 'gcc'
$args = @(
    '-std=gnu99', '-w', '-o', (Join-Path $proto 'proto.exe'),
    (Join-Path $proto 'harness.c'),
    (Join-Path $proto 'stubs.c')
)
foreach ($f in $srcs) { $args += (Join-Path $gen $f) }
$args += '-I'; $args += $gen

& $gcc @args
if ($LASTEXITCODE -ne 0) { throw "build failed" }

& (Join-Path $proto 'proto.exe')
