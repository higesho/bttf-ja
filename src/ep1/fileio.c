/* ファイル API のフック：Pack フォルダに翻訳済みファイルを仮想的に置く（text.c） */
#include "bttfjp.h"

static HANDLE (WINAPI *o_CreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static HANDLE (WINAPI *o_FindFirstFileA)(LPCSTR, LPWIN32_FIND_DATAA);
static BOOL (WINAPI *o_FindNextFileA)(HANDLE, LPWIN32_FIND_DATAA);
static BOOL (WINAPI *o_FindClose)(HANDLE);
static DWORD (WINAPI *o_GetFileAttributesA)(LPCSTR);
static BOOL (WINAPI *o_GetFileAttributesExA)(LPCSTR, GET_FILEEX_INFO_LEVELS, LPVOID);

static HANDLE WINAPI h_CreateFileA(LPCSTR n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD d, DWORD f, HANDLE t)
{
    char buf[MAX_PATH];
    const char *p = text_path(n, buf);
    HANDLE h = o_CreateFileA(p, a, s, sa, d, f, t);
    if (g_test && p != n) dlog("CreateFileA %s -> %s %p", n, p, h);
    return h;
}
static HANDLE WINAPI h_FindFirstFileA(LPCSTR n, LPWIN32_FIND_DATAA fd)
{
    HANDLE h = text_findfirst(n, fd);
    if (!h) h = o_FindFirstFileA(n, fd);
    return h;
}
static BOOL WINAPI h_FindNextFileA(HANDLE h, LPWIN32_FIND_DATAA fd)
{
    if (text_isfind(h)) return text_findnext(h, fd);
    return o_FindNextFileA(h, fd);
}
static BOOL WINAPI h_FindClose(HANDLE h)
{
    if (text_isfind(h)) return text_findclose(h);
    return o_FindClose(h);
}
static DWORD WINAPI h_GetFileAttributesA(LPCSTR n)
{
    char buf[MAX_PATH];
    return o_GetFileAttributesA(text_path(n, buf));
}
static BOOL WINAPI h_GetFileAttributesExA(LPCSTR n, GET_FILEEX_INFO_LEVELS l, LPVOID p)
{
    char buf[MAX_PATH];
    return o_GetFileAttributesExA(text_path(n, buf), l, p);
}

void fileio_init(void)
{
    o_CreateFileA = iat_hook("KERNEL32.dll", "CreateFileA", (void *)h_CreateFileA);
    o_FindFirstFileA = iat_hook("KERNEL32.dll", "FindFirstFileA", (void *)h_FindFirstFileA);
    o_FindNextFileA = iat_hook("KERNEL32.dll", "FindNextFileA", (void *)h_FindNextFileA);
    o_FindClose = iat_hook("KERNEL32.dll", "FindClose", (void *)h_FindClose);
    o_GetFileAttributesA = iat_hook("KERNEL32.dll", "GetFileAttributesA", (void *)h_GetFileAttributesA);
    o_GetFileAttributesExA = iat_hook("KERNEL32.dll", "GetFileAttributesExA", (void *)h_GetFileAttributesExA);
}
