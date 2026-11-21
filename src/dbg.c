/* テスト専用：指定アドレスへのアクセスを DR0-3 で監視し、そこを触った命令の位置を集計してログに出す */
#include "bttfjp.h"
#include <tlhelp32.h>

#define MAXH 512
static struct { uint32_t eip, ret, n, ecx, esi, edi, ebp; } g_hit[MAXH];
static volatile LONG g_nhit;
static uint32_t g_dr[4], g_d7;
static CRITICAL_SECTION g_cs;

static LONG CALLBACK veh(EXCEPTION_POINTERS *e)
{
    CONTEXT *c = e->ContextRecord;
    int i;
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP || !(c->Dr6 & 15)) return EXCEPTION_CONTINUE_SEARCH;
    EnterCriticalSection(&g_cs);
    for (i = 0; i < g_nhit; i++) if (g_hit[i].eip == c->Eip) { g_hit[i].n++; break; }
    if (i == g_nhit && i < MAXH) {
        g_hit[i].eip = c->Eip; g_hit[i].n = 1; g_hit[i].ecx = c->Ecx; g_hit[i].esi = c->Esi;
        g_hit[i].edi = c->Edi; g_hit[i].ebp = c->Ebp;
        g_hit[i].ret = *(uint32_t *)c->Esp;
        g_nhit++;
    }
    LeaveCriticalSection(&g_cs);
    c->Dr6 = 0;
    return EXCEPTION_CONTINUE_EXECUTION;
}

static DWORD WINAPI apply_thread(LPVOID p)
{
    DWORD me = GetCurrentThreadId(), pid = GetCurrentProcessId();
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te = { sizeof te };
    (void)p;
    if (s == INVALID_HANDLE_VALUE) return 0;
    for (BOOL ok = Thread32First(s, &te); ok; ok = Thread32Next(s, &te)) {
        HANDLE t;
        CONTEXT c;
        if (te.th32OwnerProcessID != pid || te.th32ThreadID == me) continue;
        t = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
        if (!t) continue;
        SuspendThread(t);
        memset(&c, 0, sizeof c);
        c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (GetThreadContext(t, &c)) {
            c.Dr0 = g_dr[0]; c.Dr1 = g_dr[1]; c.Dr2 = g_dr[2]; c.Dr3 = g_dr[3]; c.Dr7 = g_d7; c.Dr6 = 0;
            SetThreadContext(t, &c);
        }
        ResumeThread(t);
        CloseHandle(t);
    }
    CloseHandle(s);
    return 0;
}

void dbg_init(void)
{
    InitializeCriticalSection(&g_cs);
    AddVectoredExceptionHandler(1, veh);
}

/* rw_len: 下位2bit=種類(1=書込 3=読み書き 0=実行) 上位=長さ(1,2,4)。addr=0 で解除 */
void dbg_watch(uint32_t addr, int slot, int rw_len)
{
    int rw = rw_len & 3, len = (rw_len >> 4) & 7, lc = len == 4 ? 3 : len == 2 ? 1 : 0;
    HANDLE t;
    slot &= 3;
    g_dr[slot] = addr;
    g_d7 &= ~((3u << (slot * 2)) | (15u << (16 + slot * 4)));
    if (addr) g_d7 |= (1u << (slot * 2)) | ((uint32_t)rw << (16 + slot * 4)) | ((uint32_t)lc << (18 + slot * 4));
    t = CreateThread(NULL, 0, apply_thread, NULL, 0, NULL);
    if (t) { WaitForSingleObject(t, 5000); CloseHandle(t); }
    dlog("watch slot %d addr %08x d7 %08x", slot, addr, g_d7);
}

void dbg_dump(void)
{
    int i;
    EnterCriticalSection(&g_cs);
    dlog("---- hits %d", g_nhit);
    for (i = 0; i < g_nhit; i++)
        dlog("HIT %08x n=%u ret=%08x ecx=%08x esi=%08x edi=%08x ebp=%08x", g_hit[i].eip, g_hit[i].n, g_hit[i].ret, g_hit[i].ecx, g_hit[i].esi, g_hit[i].edi, g_hit[i].ebp);
    g_nhit = 0;
    LeaveCriticalSection(&g_cs);
}
