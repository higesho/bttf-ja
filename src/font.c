/* 日本語の文字描画
   - Font::GetGlyph(0x65b710) を置き換え、EUC-JP の 2 バイトを 1 文字として扱う（状態つき）
   - 日本語グリフは自前のアトラス（最大 2 ページ）に GDI で描き、フォントの空きページ（最大 4 ページ中）に差し込む
   - 改行位置は訳文に入れた半角空白。日本語の直後の空白は幅 0（本物の空白は 0xA0）            */
#include "bttfjp.h"
#include <d3d9.h>
#include <stdio.h>

/* アドレスはエピソードごと（game.c の表） */
#define A_GETGLYPH (g_addr->getglyph)
#define A_PAGELOOP (g_addr->pageloop)    /* cmp ebp,[ebx+0x40] → cmp ebp,4（メッシュ作成のページ数） */
#define A_BINDCALL (g_addr->bindcall)    /* ページのテクスチャを渡す call */
#define A_BIND     (g_addr->bind)
#define A_SETTEXT  (g_addr->settext)
static uint32_t g_bind_fn;               /* bind_stub の飛び先 */

typedef struct { int page; float u0, u1, v0, v1, w, h; } Glyph;   /* Font::GlyphInfo 28 バイト */
#define F_HEIGHT(f)  (*(float *)((uint8_t *)(f) + 0x20))
#define F_GLYPHS(f)  (*(Glyph **)((uint8_t *)(f) + 0x38))
#define F_NGLYPH(f)  (*(int *)((uint8_t *)(f) + 0x30))
#define F_NPAGE(f)   (*(int *)((uint8_t *)(f) + 0x40))
#define F_PAGES(f)   (*(uint8_t **)((uint8_t *)(f) + 0x48))
#define T3TEX_SIZE   0x100

/* アトラス：1 ページ 4096x2048、セル 48x64（セルの高さ＝フォントの行の高さ H） */
#define AT_W 4096
#define AT_H 2048
#define CW 48
#define CH 64
#define COLS (AT_W / CW)
#define ROWS (AT_H / CH)
#define PER_PAGE (COLS * ROWS)
#define NPAGES 2
#define EM 46               /* 0.72H */
#define BASE 51             /* 基準線 0.80H */
#define NCODE (94 * 94)

static IDirect3DDevice9 *g_dev;
static IDirect3DTexture9 *g_tex[NPAGES];
static short g_cell[NCODE];         /* EUC コード → セル番号（-1 = 未割り当て） */
static int g_ncell;
static float g_adv[NPAGES * PER_PAGE];   /* セルごとの送り幅（H を 1 とした比） */
static CRITICAL_SECTION g_cs;

/* GPU へ送る待ち行列（Present で転送） */
typedef struct { int cell; uint8_t a[CW * CH]; } Pending;
static Pending *g_pend; static int g_npend, g_cappend;

/* ---------- GDI でグリフを描く ---------- */
static const WCHAR *font_face_name(void)
{
    static WCHAR name[64];
    HRSRC r;
    if (name[0]) return name;
    lstrcpyW(name, L"BIZ UDGothic");
    r = FindResourceA(g_self, MAKEINTRESOURCEA(2), "BTTFJP");
    if (r) {
        DWORD n = 0;
        void *p = LockResource(LoadResource(g_self, r));
        if (p && AddFontMemResourceEx(p, SizeofResource(g_self, r), NULL, &n)) lstrcpyW(name, L"BTTFJP Sans");
    }
    return name;
}

static HDC g_dc;
static HFONT g_font;

static void gdi_init(void)
{
    LOGFONTW lf = { 0 };
    g_dc = CreateCompatibleDC(NULL);
    lf.lfHeight = -EM;
    lf.lfWeight = FW_MEDIUM;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = ANTIALIASED_QUALITY;
    lstrcpyW(lf.lfFaceName, font_face_name());
    g_font = CreateFontIndirectW(&lf);
    SelectObject(g_dc, g_font);
}

static void render_cell(int code, Pending *p)
{
    char euc[2];
    WCHAR wc = 0;
    GLYPHMETRICS gm;
    MAT2 m = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };
    DWORD n;
    uint8_t *buf;
    int x0, y0, x, y, pitch;
    euc[0] = (char)(0xa1 + code / 94);
    euc[1] = (char)(0xa1 + code % 94);
    memset(p->a, 0, sizeof p->a);
    g_adv[p->cell] = 0.75f;
    if (!MultiByteToWideChar(20932, 0, euc, 2, &wc, 1)) return;
    n = GetGlyphOutlineW(g_dc, wc, GGO_GRAY8_BITMAP, &gm, 0, NULL, &m);
    if (n == GDI_ERROR) return;
    /* 全角は送り EM+2px、半角幅の記号は実際の送り幅 */
    {
        int adv = gm.gmCellIncX;
        if (adv > EM * 3 / 4) adv = EM;
        g_adv[p->cell] = (float)(adv + 2) / CH;
        x0 = (adv + 2 - (int)gm.gmBlackBoxX) / 2;
        if (adv == EM) x0 = 1 + gm.gmptGlyphOrigin.x;
    }
    if (n == 0) return;
    buf = (uint8_t *)malloc(n);
    if (!buf) return;
    GetGlyphOutlineW(g_dc, wc, GGO_GRAY8_BITMAP, &gm, n, buf, &m);
    pitch = (gm.gmBlackBoxX + 3) & ~3;
    y0 = BASE - gm.gmptGlyphOrigin.y;
    for (y = 0; y < (int)gm.gmBlackBoxY; y++) {
        int ty = y0 + y;
        if (ty < 0 || ty >= CH) continue;
        for (x = 0; x < (int)gm.gmBlackBoxX; x++) {
            int tx = x0 + x, v;
            if (tx < 0 || tx >= CW) continue;
            v = buf[y * pitch + x] * 255 / 64;
            p->a[ty * CW + tx] = (uint8_t)(v > 255 ? 255 : v);
        }
    }
    free(buf);
}

static int cell_of(int code)
{
    int c;
    if (code < 0 || code >= NCODE) return -1;
    c = g_cell[code];
    if (c >= 0) return c;
    EnterCriticalSection(&g_cs);
    if (g_cell[code] < 0 && g_ncell < NPAGES * PER_PAGE) {
        if (g_npend == g_cappend) {
            g_cappend = g_cappend ? g_cappend * 2 : 64;
            g_pend = (Pending *)realloc(g_pend, g_cappend * sizeof(Pending));
        }
        g_pend[g_npend].cell = g_ncell;
        render_cell(code, &g_pend[g_npend]);
        g_npend++;
        g_cell[code] = (short)g_ncell++;
    }
    c = g_cell[code];
    LeaveCriticalSection(&g_cs);
    return c;
}

/* Present から呼ぶ：描いたグリフを GPU のテクスチャへ */
void font_upload(void)
{
    int i;
    if (!g_npend || !g_dev) return;
    EnterCriticalSection(&g_cs);
    for (i = 0; i < g_npend; i++) {
        Pending *p = &g_pend[i];
        int page = p->cell / PER_PAGE, k = p->cell % PER_PAGE;
        RECT r;
        D3DLOCKED_RECT lr;
        r.left = (k % COLS) * CW; r.top = (k / COLS) * CH; r.right = r.left + CW; r.bottom = r.top + CH;
        if (!g_tex[page]) continue;
        if (SUCCEEDED(IDirect3DTexture9_LockRect(g_tex[page], 0, &lr, &r, 0))) {
            int y, x;
            for (y = 0; y < CH; y++) {
                uint16_t *row = (uint16_t *)((uint8_t *)lr.pBits + y * lr.Pitch);
                for (x = 0; x < CW; x++) row[x] = (uint16_t)((p->a[y * CW + x] << 8) | 0xff);   /* A8L8：L=255 */
            }
            IDirect3DTexture9_UnlockRect(g_tex[page], 0);
        }
    }
    g_npend = 0;
    LeaveCriticalSection(&g_cs);
}

void font_device(void *dev)
{
    int i;
    g_dev = (IDirect3DDevice9 *)dev;
    for (i = 0; i < NPAGES; i++) {
        HRESULT hr = IDirect3DDevice9_CreateTexture(g_dev, AT_W, AT_H, 1, 0, D3DFMT_A8L8, D3DPOOL_MANAGED, &g_tex[i], NULL);
        if (FAILED(hr)) dlog("CreateTexture page %d failed %08x", i, hr);
        else {
            D3DLOCKED_RECT lr;
            if (SUCCEEDED(IDirect3DTexture9_LockRect(g_tex[i], 0, &lr, NULL, 0))) {
                int y;
                for (y = 0; y < AT_H; y++) {
                    uint16_t *row = (uint16_t *)((uint8_t *)lr.pBits + y * lr.Pitch);
                    int x;
                    for (x = 0; x < AT_W; x++) row[x] = 0x00ff;
                }
                IDirect3DTexture9_UnlockRect(g_tex[i], 0);
            }
        }
    }
    /* デバイスを作り直したときは描き直す */
    EnterCriticalSection(&g_cs);
    for (i = 0; i < NCODE; i++) g_cell[i] = -1;
    g_ncell = 0; g_npend = 0;
    LeaveCriticalSection(&g_cs);
    dlog("font_device %p tex %p %p", dev, g_tex[0], g_tex[1]);
}

/* ---------- フォントごとの日本語グリフ表 ---------- */
typedef struct {
    void *font;
    Glyph *orig;
    float h;
    int native;
    Glyph lead, space, cells[NPAGES * PER_PAGE];
    uint8_t *fake[NPAGES];
} FontJP;
static FontJP *g_fc[32];

static FontJP *fc_get(void *font)
{
    int i, free_i = -1;
    FontJP *f;
    for (i = 0; i < 32; i++) {
        f = g_fc[i];
        if (!f) { if (free_i < 0) free_i = i; continue; }
        if (f->font == font) {
            if (f->orig == F_GLYPHS(font) && f->h == F_HEIGHT(font) && f->native == F_NPAGE(font)) return f;
            free_i = i;
            break;
        }
    }
    if (free_i < 0) free_i = 31;
    f = g_fc[free_i];
    if (!f) { f = (FontJP *)calloc(1, sizeof(FontJP)); g_fc[free_i] = f; }
    else { int k; for (k = 0; k < NPAGES; k++) free(f->fake[k]); memset(f, 0, sizeof *f); }
    f->font = font;
    f->orig = F_GLYPHS(font);
    f->h = F_HEIGHT(font);
    f->native = F_NPAGE(font);
    f->lead.page = 99; f->lead.h = f->h;           /* 先行バイト：どのページにも属さない幅 0 の文字 */
    if (F_NGLYPH(font) > 0x20) f->space = f->orig[0x20];
    f->space.w = 0;
    for (i = 0; i < NPAGES * PER_PAGE; i++) {
        Glyph *g = &f->cells[i];
        int k = i % PER_PAGE;
        g->page = f->native + i / PER_PAGE;
        g->u0 = (float)((k % COLS) * CW) / AT_W;
        g->v0 = (float)((k / COLS) * CH) / AT_H;
        g->v1 = g->v0 + (float)CH / AT_H;
        g->h = f->h;
        g->u1 = g->u0;          /* 幅は描いたときに決まる */
        g->w = 0;
    }
    dlog("font %p h=%.1f native=%d", font, f->h, f->native);
    return f;
}

static int g_lead, g_prevjp;

static Glyph *orig_glyph(void *font, unsigned ch)
{
    Glyph *g = F_GLYPHS(font) + ch;
    if (g->u1 - g->u0 == 0.0f) g = F_GLYPHS(font);
    return g;
}

Glyph *__cdecl jp_getglyph(void *font, unsigned ch)
{
    FontJP *f;
    ch &= 0xff;
    if (g_lead) {
        int lead = g_lead;
        g_lead = 0;
        if (ch >= 0xa1 && ch <= 0xfe) {
            int code = (lead - 0xa1) * 94 + (ch - 0xa1), c;
            f = fc_get(font);
            c = cell_of(code);
            g_prevjp = 1;
            if (c < 0) return orig_glyph(font, '?');
            {
                Glyph *g = &f->cells[c];
                if (g->w == 0) {
                    g->w = g_adv[c] * f->h;
                    g->u1 = g->u0 + g_adv[c] * CH / AT_W;
                }
                return g;
            }
        }
    }
    if (ch >= 0xa1 && ch <= 0xfe) {
        g_lead = ch;
        return &fc_get(font)->lead;
    }
    if (ch == 0x20 && g_prevjp) return &fc_get(font)->space;
    g_prevjp = 0;
    if (ch == 0xa0) ch = 0x20;
    return orig_glyph(font, ch);
}

static __declspec(naked) void getglyph_stub(void)
{
    __asm {
        push dword ptr [esp + 4]
        push ecx
        call jp_getglyph
        add esp, 8
        ret 4
    }
}

/* ---------- 描画時のテクスチャ差し替え ---------- */
void *__cdecl jp_bind(void *font, uint8_t *tex)
{
    FontJP *f;
    int page, jp;
    if (!font || !F_PAGES(font)) return tex;
    page = (int)(tex - F_PAGES(font)) / T3TEX_SIZE;
    if (page < F_NPAGE(font)) return tex;
    jp = page - F_NPAGE(font);
    if (jp >= NPAGES || !g_tex[jp]) return NULL;
    f = fc_get(font);
    if (!f->fake[jp]) {
        f->fake[jp] = (uint8_t *)malloc(T3TEX_SIZE);
        memcpy(f->fake[jp], F_PAGES(font), T3TEX_SIZE);
        {   /* 元のページの幅・高さが入っている欄（版によって位置が違う）をアトラスの寸法にする */
            IDirect3DTexture9 *t0 = *(IDirect3DTexture9 **)(F_PAGES(font) + 0x14);
            D3DSURFACE_DESC dd;
            int off;
            if (t0 && SUCCEEDED(t0->lpVtbl->GetLevelDesc(t0, 0, &dd)))
                for (off = 0x18; off + 8 <= T3TEX_SIZE; off += 4)
                    if (*(UINT *)(f->fake[jp] + off) == dd.Width && *(UINT *)(f->fake[jp] + off + 4) == dd.Height) {
                        *(int *)(f->fake[jp] + off) = AT_W; *(int *)(f->fake[jp] + off + 4) = AT_H;
                        if (g_test) dlog("fake tex: size at +0x%x (%ux%u)", off, dd.Width, dd.Height);
                        off += 4;
                    }
        }
    }
    *(void **)(f->fake[jp] + 0x14) = g_tex[jp];
    return f->fake[jp];
}

static __declspec(naked) void bind_stub(void)
{
    __asm {
        push ecx
        push dword ptr [esp + 8]
        push ebx
        call jp_bind
        add esp, 8
        pop ecx
        mov [esp + 4], eax
        jmp dword ptr [g_bind_fn]
    }
}

/* ---------- 文字列を走査する 7 関数の入口で状態を戻す ---------- */
static const uint8_t g_entry_len[7] = { 5, 7, 7, 6, 6, 6, 7 };
static const uint8_t g_entry_sig[7][2] = { {0x83,0xec}, {0x83,0xec}, {0x83,0xec}, {0x55,0x8b}, {0x55,0x8b}, {0x55,0x8b}, {0x6a,0xff} };

static int hook_entries(void)
{
    uint8_t *mem = (uint8_t *)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE), *p = mem;
    int i;
    if (!mem) return 0;
    for (i = 0; i < 7; i++) {
        uint32_t fn = g_addr->entry[i];
        int n = g_entry_len[i];
        uint8_t *stub = p;
        if (memcmp((void *)fn, g_entry_sig[i], 2)) { dlog("entry %08x mismatch", fn); return 0; }
        *p++ = 0xc7; *p++ = 0x05; *(int **)p = &g_lead; p += 4; *(int *)p = 0; p += 4;      /* mov [g_lead],0 */
        *p++ = 0xc7; *p++ = 0x05; *(int **)p = &g_prevjp; p += 4; *(int *)p = 0; p += 4;    /* mov [g_prevjp],0 */
        memcpy(p, (void *)fn, n); p += n;
        *p = 0xe9; *(int32_t *)(p + 1) = (int32_t)((fn + n) - ((uint32_t)p + 5)); p += 5;
        {
            uint8_t b[8];
            int k;
            b[0] = 0xe9; *(int32_t *)(b + 1) = (int32_t)((uint32_t)stub - (fn + 5));
            for (k = 5; k < n; k++) b[k] = 0x90;
            patch_bytes(fn, b, n);
        }
    }
    return 1;
}

/* ---------- テスト用：英語のまま表示された文字列を記録（SetText 0x6ec540） ---------- */
int g_font_state;                       /* 0 未初期化 / 1 動作中 / -1 使えない */
static int g_logall;
static void __cdecl log_settext(const uint8_t *str)
{
    static uint64_t seen[4096];
    static int nseen;
    const uint8_t *s;
    uint32_t n, cap, i, letters = 0;
    uint64_t h;
    if (!str) return;
    n = *(const uint32_t *)(str + 0x10); cap = *(const uint32_t *)(str + 0x14);
    s = cap >= 16 ? *(const uint8_t *const *)str : str;
    if (!s || n == 0 || n > 4000) return;
    if (g_logall) {
        static uint64_t last;
        h = fnv64(s, n);
        if (h != last) dlog("TXT|%lu|%.*s", GetTickCount(), n > 400 ? 400 : n, s);
        last = h;
    }
    for (i = 0; i + 1 < n; i++) if (s[i] >= 0xa1 && s[i + 1] >= 0xa1) return;     /* 日本語あり */
    for (i = 0; i < n; i++) if ((s[i] | 0x20) >= 'a' && (s[i] | 0x20) <= 'z') letters++;
    if (letters < 3) return;
    h = fnv64(s, n);
    for (i = 0; i < (uint32_t)nseen; i++) if (seen[i] == h) return;
    if (nseen < 4096) seen[nseen++] = h;
    dlog("UNTR|%.*s", n > 300 ? 300 : n, s);
}

static void hook_settext_log(void)
{
    static const uint8_t sig[3] = { 0x53, 0x8b, 0xd9 };
    uint8_t *p = (uint8_t *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE), *stub = p;
    if (!p || memcmp((void *)A_SETTEXT, sig, 3)) return;
    *p++ = 0x60;                                              /* pushad */
    *p++ = 0xff; *p++ = 0x74; *p++ = 0x24; *p++ = 0x24;       /* push [esp+0x24]（引数の String*） */
    *p = 0xe8; *(int32_t *)(p + 1) = (int32_t)((uint32_t)log_settext - ((uint32_t)p + 5)); p += 5;
    *p++ = 0x83; *p++ = 0xc4; *p++ = 0x04;                    /* add esp,4 */
    *p++ = 0x61;                                              /* popad */
    memcpy(p, (void *)A_SETTEXT, 10); p += 10;                /* push ebx / mov ebx,ecx / cmp [ebx+0x480],0x10 */
    *p = 0xe9; *(int32_t *)(p + 1) = (int32_t)((A_SETTEXT + 10) - ((uint32_t)p + 5));
    {
        uint8_t b[10] = { 0xe9, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90, 0x90 };
        *(int32_t *)(b + 1) = (int32_t)((uint32_t)stub - (A_SETTEXT + 5));
        patch_bytes(A_SETTEXT, b, 10);
    }
}

void font_init(void)
{
    static const uint8_t sig_gg[10] = { 0x8b, 0x44, 0x24, 0x04, 0x8b, 0x51, 0x38, 0x8d, 0x0c, 0xc5 };
    static const uint8_t sig_loop[3] = { 0x3b, 0x6b, 0x40 }, new_loop[3] = { 0x83, 0xfd, 0x04 };
    int i;
    InitializeCriticalSection(&g_cs);
    if (!g_addr) { dlog("font_init: unknown exe"); g_font_state = -1; return; }
    if (g_test && GetEnvironmentVariableA("BTTFJP_NOFONT", NULL, 0) > 0) { dlog("font_init: skipped (test)"); return; }
    for (i = 0; i < NCODE; i++) g_cell[i] = -1;
    if (memcmp((void *)A_GETGLYPH, sig_gg, 10) || memcmp((void *)A_PAGELOOP, sig_loop, 3) || *(uint8_t *)A_BINDCALL != 0xe8 ||
        (uint32_t)(A_BINDCALL + 5 + *(int32_t *)(A_BINDCALL + 1)) != A_BIND) {
        dlog("font_init: unknown exe");
        g_font_state = -1;                  /* 訳の差し替えもしない（英語のまま） */
        return;
    }
    g_bind_fn = A_BIND;
    gdi_init();
    if (!hook_entries()) { g_font_state = -1; return; }
    patch_jmp(A_GETGLYPH, (void *)getglyph_stub);
    patch_bytes(A_PAGELOOP, new_loop, 3);
    {
        uint8_t b[5];
        b[0] = 0xe8; *(int32_t *)(b + 1) = (int32_t)((uint32_t)bind_stub - (A_BINDCALL + 5));
        patch_bytes(A_BINDCALL, b, 5);
    }
    if (g_test) { g_logall = GetEnvironmentVariableA("BTTFJP_LOGALL", NULL, 0) > 0; hook_settext_log(); }
    g_font_state = 1;
    dlog("font_init ok");
}
