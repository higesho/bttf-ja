#!/bin/sh
# DLL をビルドする（MSVC x86）
cd "$(dirname "$0")/../src" && powershell -NoProfile -Command "cmd /c ('\"' + (Resolve-Path build.bat).Path + '\"')" 2>&1 | grep -E "error|warning|LNK" ; ls -la ../build/dinput8.dll
