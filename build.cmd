@echo off
rem LegacyWheel - compila tudo com Zig (https://ziglang.org), sem Visual Studio.
rem Uso: build.cmd        -> gera dist\ (LegacyWheel.exe, x86\, x64\) e roda os testes
setlocal
cd /d "%~dp0"
where zig >nul 2>nul || (echo Zig nao encontrado no PATH. Instale com: winget install zig.zig & exit /b 1)

if not exist dist\x86 mkdir dist\x86
if not exist dist\x64 mkdir dist\x64
if not exist build mkdir build

set CFLAGS=-O2 -Wall
set GUILIBS=-ldxguid -lcomctl32 -lcomdlg32 -lole32 -lshell32 -lgdi32 -luser32 -ladvapi32

echo [1/4] adaptador 32 bits
zig cc -target x86-windows-gnu %CFLAGS% -shared -o dist\x86\dinput.dll  src\pedalmerge.c src\dinput.def  -ldxguid || exit /b 1
zig cc -target x86-windows-gnu %CFLAGS% -shared -o dist\x86\dinput8.dll src\pedalmerge.c src\dinput8.def -ldxguid || exit /b 1

echo [2/4] adaptador 64 bits
zig cc -target x86_64-windows-gnu %CFLAGS% -shared -o dist\x64\dinput.dll  src\pedalmerge.c src\dinput64.def   -ldxguid || exit /b 1
zig cc -target x86_64-windows-gnu %CFLAGS% -shared -o dist\x64\dinput8.dll src\pedalmerge.c src\dinput8_64.def -ldxguid || exit /b 1

echo [3/4] programa de ajustes
zig cc -target x86-windows-gnu %CFLAGS% -municode -Wl,--subsystem,windows -o dist\LegacyWheel.exe src\legacywheel.c src\legacywheel.rc %GUILIBS% || exit /b 1

echo [4/4] testes
zig cc -target x86-windows-gnu -O2 -o build\ffbtest.exe tests\ffbtest.c -ldxguid || exit /b 1
build\ffbtest.exe || exit /b 1

del /q dist\*.pdb dist\*.lib dist\x86\*.pdb dist\x86\*.lib dist\x64\*.pdb dist\x64\*.lib 2>nul
copy /y README.md dist\ >nul
copy /y README.pt-BR.md dist\ >nul
copy /y LICENSE dist\LICENSE.txt >nul
powershell -NoProfile -Command "Compress-Archive -Path dist\* -DestinationPath build\LegacyWheel-1.0.0.zip -Force" || exit /b 1
echo.
echo Pronto: dist\ e build\LegacyWheel-1.0.0.zip
