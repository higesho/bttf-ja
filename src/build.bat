@echo off
rem DLL をビルドする。BTTF_EP（2〜5）があれば ..\epN\build の訳とフォントを使い、そこに作る
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
set OUT=..\build
if not "%BTTF_EP%"=="" if not "%BTTF_EP%"=="1" set OUT=..\ep%BTTF_EP%\build
if not exist %OUT% mkdir %OUT%
pushd %OUT%
rc /nologo /fo bttfjp.res "%~dp0bttfjp.rc" || exit /b 1
popd
cl /nologo /utf-8 /O2 /MT /W3 /LD /D_CRT_SECURE_NO_WARNINGS /Fo%OUT%\ proxy.c main.c testio.c dbg.c game.c fileio.c font.c text.c %OUT%\bttfjp.res /Fe%OUT%\dinput8.dll /link /DEF:dinput8.def kernel32.lib user32.lib gdi32.lib shlwapi.lib || exit /b 1
