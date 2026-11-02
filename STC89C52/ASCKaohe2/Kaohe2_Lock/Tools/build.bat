@echo off
rem ============================================================================
rem  One-click build: produce A / B HEX files
rem  Usage: Tools\build.bat   (or double click)
rem  If Keil is installed elsewhere, set KEIL_UV4 first, e.g.
rem      set KEIL_UV4=C:\Keil_v5\UV4\UV4.exe
rem ============================================================================
setlocal

set "PROJECT=%~dp0..\Project\Kaohe2_Lock.uvproj"
if "%KEIL_UV4%"=="" set "KEIL_UV4=D:\IDE\Keil5\UV4\UV4.exe"

if not exist "%KEIL_UV4%" goto no_uv4
if not exist "%PROJECT%" goto no_proj

for %%T in (KAOHE2_A_D8_OFF KAOHE2_B_D8_ON) do (
    echo.
    echo === Building target: %%T ===
    "%KEIL_UV4%" -b "%PROJECT%" -t %%T -j0 -o "build_%%T.log"
    if errorlevel 2 goto failed
)

if not exist "%~dp0..\HEX" mkdir "%~dp0..\HEX"
copy /y "%~dp0..\Objects\Kaohe2_Lock_A.hex" "%~dp0..\HEX\A_D8_OFF.hex" >nul
copy /y "%~dp0..\Objects\Kaohe2_Lock_B.hex" "%~dp0..\HEX\B_D8_ON.hex" >nul

echo.
echo Build OK:
echo    HEX\A_D8_OFF.hex   (D8 always off)
echo    HEX\B_D8_ON.hex    (D8 always on)
endlocal
exit /b 0

:no_uv4
echo [ERROR] UV4.exe not found: %KEIL_UV4%
echo         Set env var KEIL_UV4 to your uVision UV4.exe path.
endlocal
exit /b 1

:no_proj
echo [ERROR] Project not found: %PROJECT%
endlocal
exit /b 1

:failed
echo [ERROR] Build failed, see Project\build_*.log
endlocal
exit /b 2
