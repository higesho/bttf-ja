/* Back to the Future: The Game Ep.1 日本語化パッチ：入口、パッチ補助、テスト用 D3D9 フック */
#include "bttfjp.h"
#include <stdio.h>
#include <stdarg.h>
#include <d3d9.h>

void proxy_init(void);
HRESULT WINAPI real_DirectInput8Create(HINSTANCE h, DWORD v, REFIID r, LPVOID *o, LPUNKNOWN u);
extern LONG g_curx, g_cury;

int g_test;
HMODULE g_self;
static FILE *g_log;
static CRITICAL_SECTION g_logcs;

void dlog(const char *fmt, ...)
{
    va_list ap;
    if (!g_log) return;
    EnterCriticalSection(&g_logcs);
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
    LeaveCriticalSection(&g_logcs);
}

void patch_bytes(uint32_t at, const void *b, int n)
{
    DWORD old;
    VirtualProtect((void *)at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy((void *)at, b, n);
    VirtualProtect((void *)at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void *)at, n);
}

void patch_jmp(uint32_t at, void *to)
{
    uint8_t b[5];
    b[0] = 0xe9;
    *(int32_t *)(b + 1) = (int32_t)((uint32_t)to - (at + 5));
    patch_bytes(at, b, 5);
}

void *iat_hook(const char *dll, const char *func, void *repl)
{
    uint8_t *base = (uint8_t *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress);
    for (; imp->Name; imp++) {
        IMAGE_THUNK_DATA *names, *addrs;
        if (_stricmp((char *)(base + imp->Name), dll)) continue;
        names = (IMAGE_THUNK_DATA *)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        addrs = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; names++, addrs++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            if (names->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            ibn = (IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData);
            if (!strcmp((char *)ibn->Name, func)) {
                void *old = (void *)addrs->u1.Function;
                DWORD prot;
                VirtualProtect(&addrs->u1.Function, 4, PAGE_READWRITE, &prot);
                addrs->u1.Function = (ULONG_PTR)repl;
                VirtualProtect(&addrs->u1.Function, 4, prot, &prot);
                return old;
            }
        }
    }
    dlog("iat_hook: %s!%s not found", dll, func);
    return NULL;
}

/* ---------- テストモード：窓モード強制・バックバッファ撮影 ---------- */
static HRESULT (STDMETHODCALLTYPE *o_CreateDevice)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *, IDirect3DDevice9 **);
static HRESULT (STDMETHODCALLTYPE *o_Reset)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);
static HRESULT (STDMETHODCALLTYPE *o_Present)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);
HWND g_hwnd;

static void force_windowed(D3DPRESENT_PARAMETERS *pp)
{
    if (!g_test) return;
    pp->Windowed = TRUE;
    pp->FullScreen_RefreshRateInHz = 0;
}

static HRESULT STDMETHODCALLTYPE h_Present(IDirect3DDevice9 *d, const RECT *a, const RECT *b, HWND w, const RGNDATA *r)
{
    font_upload();
    if (g_test) testio_present(d);
    return o_Present(d, a, b, w, r);
}

static HRESULT STDMETHODCALLTYPE h_Reset(IDirect3DDevice9 *d, D3DPRESENT_PARAMETERS *pp)
{
    HRESULT hr;
    force_windowed(pp);
    hr = o_Reset(d, pp);
    dlog("Reset %dx%d win=%d -> %08x coop %08x", pp->BackBufferWidth, pp->BackBufferHeight, pp->Windowed, hr,
         IDirect3DDevice9_TestCooperativeLevel(d));
    return hr;
}

static void vt_patch(void **vt, int idx, void *repl, void **orig)
{
    DWORD prot;
    if (vt[idx] == repl) return;
    VirtualProtect(&vt[idx], 4, PAGE_EXECUTE_READWRITE, &prot);
    *orig = vt[idx];
    vt[idx] = repl;
    VirtualProtect(&vt[idx], 4, prot, &prot);
}

static void **g_devvt;
static DWORD WINAPI watch_thread(LPVOID p)
{
    (void)p;
    for (;;) {
        Sleep(1000);
        if (g_devvt && g_devvt[17] != (void *)h_Present) {
            dlog("Present slot changed to %p, re-patching", g_devvt[17]);
            vt_patch(g_devvt, 17, (void *)h_Present, (void **)&o_Present);
        }
    }
}

/* テスト時：前面にいなくても止まらないよう、非アクティブ化の通知を握りつぶす */
static WNDPROC g_oldproc;
static LRESULT CALLBACK test_wndproc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_ACTIVATEAPP: wp = TRUE; break;
    case WM_ACTIVATE: wp = WA_ACTIVE; break;
    case WM_NCACTIVATE: wp = TRUE; break;
    case WM_KILLFOCUS: return 0;
    }
    return CallWindowProcA(g_oldproc, h, m, wp, lp);
}
static HWND WINAPI h_GetForegroundWindow(void) { return g_hwnd ? g_hwnd : GetForegroundWindow(); }
static HWND WINAPI h_GetActiveWindow(void) { return g_hwnd ? g_hwnd : GetActiveWindow(); }
static HWND WINAPI h_GetFocus(void) { return g_hwnd ? g_hwnd : GetFocus(); }

static HRESULT STDMETHODCALLTYPE h_CreateDevice(IDirect3D9 *d3d, UINT ad, D3DDEVTYPE t, HWND w, DWORD fl, D3DPRESENT_PARAMETERS *pp, IDirect3DDevice9 **out)
{
    HRESULT hr;
    force_windowed(pp);
    g_hwnd = w;
    if (g_test && !g_oldproc) g_oldproc = (WNDPROC)SetWindowLongPtrA(w, GWLP_WNDPROC, (LONG_PTR)test_wndproc);
    dlog("CreateDevice %dx%d fmt %d win=%d hwnd=%p", pp->BackBufferWidth, pp->BackBufferHeight, pp->BackBufferFormat, pp->Windowed, w);
    hr = o_CreateDevice(d3d, ad, t, w, fl, pp, out);
    if (SUCCEEDED(hr)) {
        void **vt = *(void ***)*out;
        vt_patch(vt, 16, (void *)h_Reset, (void **)&o_Reset);
        vt_patch(vt, 17, (void *)h_Present, (void **)&o_Present);
        font_device(*out);
        if (!g_devvt) { g_devvt = vt; CreateThread(NULL, 0, watch_thread, NULL, 0, NULL); }
    } else dlog("CreateDevice failed %08x", hr);
    return hr;
}

void game_hooks_init(void);
static int g_game_hooked;
/* ゲームのフックを一度だけ入れる（.text の復号後、ゲーム自身の呼び出しの中から呼ぶこと） */
void game_hooks_once(void)
{
    if (!g_game_hooked) { g_game_hooked = 1; game_hooks_init(); }
}
static IDirect3D9 *(WINAPI *o_Direct3DCreate9)(UINT);
static IDirect3D9 *WINAPI h_Direct3DCreate9(UINT v)
{
    IDirect3D9 *d = o_Direct3DCreate9(v);
    if (!g_game_hooked) { g_game_hooked = 1; game_hooks_init(); }   /* この時点で .text は復号済み */
    if (d) vt_patch(*(void ***)d, 16, (void *)h_CreateDevice, (void **)&o_CreateDevice);
    return d;
}

/* テスト時：カーソル位置はテスト側から与える（本物のマウスは無視） */
static BOOL WINAPI h_GetCursorPos(POINT *p)
{
    if (g_curx >= 0 && g_hwnd) {
        p->x = g_curx; p->y = g_cury;
        ClientToScreen(g_hwnd, p);
        return TRUE;
    }
    return GetCursorPos(p);
}
static BOOL WINAPI h_ClipCursor(const RECT *r) { (void)r; return TRUE; }
static BOOL WINAPI h_SetCursorPos(int x, int y) { (void)x; (void)y; return TRUE; }

HRESULT WINAPI p_DirectInput8Create(HINSTANCE h, DWORD v, REFIID r, LPVOID *o, LPUNKNOWN u)
{
    HRESULT hr = real_DirectInput8Create(h, v, r, o, u);
    if (!g_game_hooked) { g_game_hooked = 1; game_hooks_init(); }
    if (g_test && SUCCEEDED(hr)) testio_wrap_dinput(*o);
    return hr;
}

static void init(HMODULE self)
{
    char buf[MAX_PATH];
    (void)self;
    InitializeCriticalSection(&g_logcs);
    if (GetEnvironmentVariableA("BTTFJP_TEST", buf, sizeof buf) && buf[0] == '1') g_test = 1;
    if (GetEnvironmentVariableA("BTTFJP_LOG", buf, sizeof buf)) g_log = fopen(buf, "w");
    dlog("bttfjp start test=%d", g_test);
    game_detect();
    o_Direct3DCreate9 = iat_hook("d3d9.dll", "Direct3DCreate9", (void *)h_Direct3DCreate9);
    fileio_init();
    if (g_test) {
        iat_hook("USER32.dll", "GetCursorPos", (void *)h_GetCursorPos);
        iat_hook("USER32.dll", "ClipCursor", (void *)h_ClipCursor);
        iat_hook("USER32.dll", "SetCursorPos", (void *)h_SetCursorPos);
        iat_hook("USER32.dll", "GetForegroundWindow", (void *)h_GetForegroundWindow);
        iat_hook("USER32.dll", "GetActiveWindow", (void *)h_GetActiveWindow);
        iat_hook("USER32.dll", "GetFocus", (void *)h_GetFocus);
        dbg_init();
        testio_init();
    }
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        g_self = h;
        proxy_init();
        init(h);
    }
    return TRUE;
}
