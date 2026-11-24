#!/bin/sh
# DLL をビルドする（MSVC x86）。BTTF_EP=2..5 ならそのエピソードの build フォルダに作る
#   sh tools/build.sh        配布版（build/dinput8.dll）
#   sh tools/build.sh test   試験版（build/test/dinput8.dll。試験用の仕組み入り、配布しない）
cd "$(dirname "$0")/../src" && powershell -NoProfile -Command "cmd /c ('\"' + (Resolve-Path build.bat).Path + '\" $1')" 2>&1 | grep -E "error|warning|LNK" ; if [ -n "$BTTF_EP" ] && [ "$BTTF_EP" != 1 ]; then B=../ep$BTTF_EP/build; else B=../build; fi; if [ "$1" = test ]; then ls -la $B/test/dinput8.dll; else ls -la $B/dinput8.dll; fi
