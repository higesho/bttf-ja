#!/bin/sh
# DLL をビルドする（MSVC x86）。BTTF_EP=2..5 ならそのエピソードの build フォルダに作る
cd "$(dirname "$0")/../src" && powershell -NoProfile -Command "cmd /c ('\"' + (Resolve-Path build.bat).Path + '\"')" 2>&1 | grep -E "error|warning|LNK" ; if [ -n "$BTTF_EP" ] && [ "$BTTF_EP" != 1 ]; then ls -la ../ep$BTTF_EP/build/dinput8.dll; else ls -la ../build/dinput8.dll; fi
