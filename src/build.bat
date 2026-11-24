@echo off
rem DLL をビルドする。BTTF_EP（2〜5）があれば ..\epN\build の訳とフォントを使い、そこに作る
rem   build.bat       配布版 → ..\build\dinput8.dll（Ep2 以降は ..\epN\build\dinput8.dll）
rem   build.bat test  試験版（/DBTTFJP_TEST、testio.c と dbg.c を足す）→ 同じ build の test\dinput8.dll。配布しない
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
set EP=%BTTF_EP%
if "%EP%"=="" set EP=1
set D=..\build
if not "%EP%"=="1" set D=..\ep%EP%\build
set O=%D%
set R=bttfjp.res
set DEF=
set EXTRA=
if "%1"=="test" (
  set O=%D%\test
  set R=test\bttfjp.res
  set DEF=/DBTTFJP_TEST
  set EXTRA=testio.c dbg.c
)
if not exist %O% mkdir %O%
pushd %D%
rc /nologo /dEP=%EP% /fo %R% "%~dp0bttfjp.rc" || exit /b 1
popd
cl /nologo /utf-8 /O2 /MT /W3 /LD /D_CRT_SECURE_NO_WARNINGS %DEF% /Fo%O%\ proxy.c main.c game.c fileio.c font.c text.c %EXTRA% %O%\bttfjp.res /Fe%O%\dinput8.dll /link /DEF:dinput8.def kernel32.lib user32.lib gdi32.lib shlwapi.lib || exit /b 1
