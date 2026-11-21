/* 翻訳済みの .landb を作って、Pack フォルダにあるように見せる
   - 元データは利用者の Pack\0_dlog.ttarch（非圧縮）から読む。配布物に原作のデータは入れない
   - 訳は資源 BTTFJP/1：英文（Windows-1252）の FNV-1a 64 → EUC-JP
   - 出来たファイルは %TEMP%\BTTFJP\ に置き、Pack\<名前> への検索・オープンをそこへ回す */
#include "bttfjp.h"
#include <stdio.h>
#include <shlwapi.h>

static char g_pack[MAX_PATH];       /* <ゲーム>\Pack */
static char g_tmp[MAX_PATH];        /* %TEMP%\BTTFJP */
static int g_ready;
#define MAXF 64
static char g_names[MAXF][64];
static int g_nnames;

/* ---------- 訳の表 ---------- */
static const uint8_t *g_tr;
static uint32_t g_ntr;

uint64_t fnv64(const uint8_t *s, int n)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    int i;
    for (i = 0; i < n; i++) { h ^= s[i]; h *= 0x100000001b3ULL; }
    return h;
}

static const char *tr_find(uint64_t key)
{
    uint32_t lo = 0, hi = g_ntr;
    const uint8_t *rec = g_tr + 8;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        uint64_t k = *(const uint64_t *)(rec + mid * 12);
        if (k == key) return (const char *)(g_tr + *(const uint32_t *)(rec + mid * 12 + 8));
        if (k < key) lo = mid + 1; else hi = mid;
    }
    return NULL;
}

static void tr_load(void)
{
    HRSRC r = FindResourceA(g_self, MAKEINTRESOURCEA(1), "BTTFJP");
    if (!r) { dlog("no translation resource"); return; }
    g_tr = (const uint8_t *)LockResource(LoadResource(g_self, r));
    if (!g_tr || memcmp(g_tr, "BTJ1", 4)) { g_tr = NULL; return; }
    g_ntr = *(const uint32_t *)(g_tr + 4);
    dlog("translations: %u", g_ntr);
}

/* ---------- .landb の組み直し ---------- */
typedef struct { uint8_t *p; size_t n, cap; } Buf;
static void put(Buf *b, const void *d, size_t n)
{
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 4096; b->p = (uint8_t *)realloc(b->p, b->cap); }
    memcpy(b->p + b->n, d, n); b->n += n;
}
static void put32(Buf *b, uint32_t v) { put(b, &v, 4); }
static uint32_t rd32(const uint8_t *p) { return *(const uint32_t *)p; }

/* 英文 → 訳（なければ NULL）。ファイル名と ID の個別指定を先に見る */
static const char *lookup(const char *file, uint32_t id, const uint8_t *s, int n)
{
    char key[160];
    const char *t;
    if (file) {                                  /* 本文だけ ID 指定を見る（話し手の名前には使わない） */
        sprintf(key, "#%s#%u", file, id);
        t = tr_find(fnv64((const uint8_t *)key, (int)strlen(key)));
        if (t) return t;
    }
    if (n == 0) return NULL;
    return tr_find(fnv64(s, n));
}

static int rebuild_landb(const char *name, const uint8_t *d, size_t len, Buf *out, int *ntr)
{
    uint32_t nc = rd32(d + 4), p = 8 + 12 * nc, msize, cnt, i, end;
    Buf body = { 0 };
    *ntr = 0;
    if (memcmp(d, "ERTM", 4) || p + 24 > len) return 0;
    put(out, d, p + 16);                  /* ヘッダ＋前の 2 ブロック */
    p += 16;
    msize = rd32(d + p); cnt = rd32(d + p + 4);
    end = p + msize;
    p += 8;
    for (i = 0; i < cnt; i++) {
        uint32_t key = rd32(d + p), id = rd32(d + p + 4), b0, k, sb, s0, bl[2], ln[2], so[2];
        const char *tp, *tt;
        Buf ss = { 0 };
        put(&body, d + p, 12);
        p += 12;
        b0 = p;
        for (k = 0; k < 3; k++) p += rd32(d + p);
        p += 4;                                       /* z2 */
        put(&body, d + b0, p - b0);
        sb = rd32(d + p); s0 = p; p += 4;
        for (k = 0; k < 2; k++) { bl[k] = rd32(d + p); ln[k] = rd32(d + p + 4); so[k] = p + 8; p += bl[k]; }
        tp = lookup(NULL, id, d + so[0], ln[0]);     /* 話し手の名前 */
        tt = lookup(name, id, d + so[1], ln[1]);
        (void)key;
        if (tp) { uint32_t n = (uint32_t)strlen(tp); put32(&ss, n + 8); put32(&ss, n); put(&ss, tp, n); }
        else put(&ss, d + so[0] - 8, bl[0]);
        if (tt) { uint32_t n = (uint32_t)strlen(tt); put32(&ss, n + 8); put32(&ss, n); put(&ss, tt, n); (*ntr)++; }
        else put(&ss, d + so[1] - 8, bl[1]);
        put(&ss, d + p, s0 + sb - p);                 /* 残り（フラグのブロック） */
        p = s0 + sb;
        put32(&body, (uint32_t)ss.n + 4);
        put(&body, ss.p, ss.n);
        free(ss.p);
        put(&body, d + p, 4);                         /* flags */
        p += 4;
        if (p > end) { free(body.p); return 0; }
    }
    if (p != end) { free(body.p); return 0; }
    put32(out, (uint32_t)body.n + 8);
    put32(out, cnt);
    put(out, body.p, body.n);
    put(out, d + end, len - end);
    free(body.p);
    return 1;
}

/* ---------- 0_dlog.ttarch を読む（v8・非圧縮） ---------- */
static void build_all(void)
{
    char path[MAX_PATH];
    HANDLE h;
    DWORD got, sz;
    uint8_t *a = NULL, *info;
    uint32_t mode, infosz, p, nd, nf, i, dataoff;
    sprintf(path, "%s\\0_dlog.ttarch", g_pack);
    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { dlog("open %s failed", path); return; }
    sz = GetFileSize(h, NULL);
    a = (uint8_t *)malloc(sz);
    if (!a || !ReadFile(h, a, sz, &got, NULL) || got != sz) { CloseHandle(h); free(a); return; }
    CloseHandle(h);
    mode = rd32(a + 12);
    if (rd32(a) != 8 || rd32(a + 4) != 0 || mode != 1 || rd32(a + 16) != 0) { dlog("0_dlog.ttarch: unsupported"); free(a); return; }
    infosz = *(uint32_t *)(a + 0x2d);
    info = a + 0x31;
    dataoff = 0x31 + infosz;
    p = 0;
    nd = rd32(info); p = 4;
    for (i = 0; i < nd; i++) p += 4 + rd32(info + p);
    nf = rd32(info + p); p += 4;
    CreateDirectoryA(g_tmp, NULL);
    for (i = 0; i < nf; i++) {
        uint32_t nl = rd32(info + p), off, size;
        char name[MAX_PATH];
        if (nl >= sizeof name) break;
        memcpy(name, info + p + 4, nl); name[nl] = 0;
        p += 4 + nl;
        off = rd32(info + p + 4); size = rd32(info + p + 8);
        p += 12;
        if (!StrStrIA(name, "_english.landb") || g_nnames >= MAXF || dataoff + off + size > sz) continue;
        {
            Buf out = { 0 };
            int ntr = 0;
            if (rebuild_landb(name, a + dataoff + off, size, &out, &ntr)) {
                char dst[MAX_PATH];
                HANDLE w;
                sprintf(dst, "%s\\%s", g_tmp, name);
                w = CreateFileA(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
                if (w != INVALID_HANDLE_VALUE) {
                    WriteFile(w, out.p, (DWORD)out.n, &got, NULL);
                    CloseHandle(w);
                    lstrcpynA(g_names[g_nnames++], name, 64);
                }
                if (g_test) dlog("landb %s: %d translated", name, ntr);
            } else dlog("landb %s: parse error", name);
            free(out.p);
        }
    }
    free(a);
}

static void ensure_ready(void)
{
    char *s;
    if (g_ready) return;
    g_ready = 1;
    GetModuleFileNameA(NULL, g_pack, MAX_PATH);
    s = strrchr(g_pack, '\\');
    if (s) strcpy(s, "\\Pack");
    GetTempPathA(MAX_PATH, g_tmp);
    strcat(g_tmp, "BTTFJP");
    if (g_test && GetEnvironmentVariableA("BTTFJP_NOTEXT", NULL, 0) > 0) return;   /* テスト用：英語のまま */
    tr_load();
    if (g_tr) build_all();
}

/* Pack\<名前> が自前のファイルなら一時フォルダのパスを返す。名前にワイルドカードがあれば 2 を返す */
static int redirect(const char *path, char *out)
{
    size_t n;
    const char *name;
    int i;
    if (!path || g_font_state < 0) return 0;     /* 字が出せないなら英語のまま */
    ensure_ready();
    if (!g_nnames) return 0;
    n = strlen(g_pack);
    if (_strnicmp(path, g_pack, n) || (path[n] != '\\' && path[n] != '/')) return 0;
    name = path + n + 1;
    if (strchr(name, '\\') || strchr(name, '/')) return 0;
    if (strchr(name, '*') || strchr(name, '?')) { sprintf(out, "%s\\%s", g_tmp, name); return 2; }
    for (i = 0; i < g_nnames; i++)
        if (!_stricmp(name, g_names[i])) { sprintf(out, "%s\\%s", g_tmp, name); return 1; }
    return 0;
}

/* ---------- 検索ハンドル：本物の結果のあとに自前のファイルを続ける ---------- */
typedef struct { uint32_t magic; HANDLE real, ours; int phase; char tpat[MAX_PATH]; } MFind;
#define MF_MAGIC 0x4d464a42

static int dotname(const char *n) { return !strcmp(n, ".") || !strcmp(n, ".."); }

static BOOL ours_next(MFind *m, WIN32_FIND_DATAA *fd, int first)
{
    for (;;) {
        BOOL ok;
        if (first) { m->ours = FindFirstFileA(m->tpat, fd); ok = m->ours != INVALID_HANDLE_VALUE; first = 0; if (!ok) m->ours = NULL; }
        else ok = m->ours && FindNextFileA(m->ours, fd);
        if (!ok) return FALSE;
        if (!dotname(fd->cFileName)) return TRUE;
    }
}

/* 戻り値 NULL = 対象外（呼び出し側で本物を呼ぶ） */
HANDLE text_findfirst(const char *pat, WIN32_FIND_DATAA *fd)
{
    char t[MAX_PATH];
    int r = redirect(pat, t);
    MFind *m;
    if (r == 0) return NULL;
    if (r == 1) return FindFirstFileA(t, fd);
    m = (MFind *)calloc(1, sizeof *m);
    m->magic = MF_MAGIC;
    lstrcpyA(m->tpat, t);
    m->real = FindFirstFileA(pat, fd);
    if (m->real != INVALID_HANDLE_VALUE) return (HANDLE)m;
    m->real = NULL;
    m->phase = 1;
    if (ours_next(m, fd, 1)) return (HANDLE)m;
    free(m);
    SetLastError(ERROR_FILE_NOT_FOUND);
    return INVALID_HANDLE_VALUE;
}

int text_isfind(HANDLE h)
{
    return h && h != INVALID_HANDLE_VALUE && !IsBadReadPtr(h, 4) && ((MFind *)h)->magic == MF_MAGIC;
}

BOOL text_findnext(HANDLE h, WIN32_FIND_DATAA *fd)
{
    MFind *m = (MFind *)h;
    if (m->phase == 0) {
        if (FindNextFileA(m->real, fd)) return TRUE;
        m->phase = 1;
        if (ours_next(m, fd, 1)) return TRUE;
    } else if (ours_next(m, fd, 0)) return TRUE;
    SetLastError(ERROR_NO_MORE_FILES);
    return FALSE;
}

BOOL text_findclose(HANDLE h)
{
    MFind *m = (MFind *)h;
    if (m->real) FindClose(m->real);
    if (m->ours) FindClose(m->ours);
    m->magic = 0;
    free(m);
    return TRUE;
}

/* CreateFileA / GetFileAttributes 用：自前のファイルならパスを差し替える */
const char *text_path(const char *path, char *buf)
{
    return redirect(path, buf) == 1 ? buf : path;
}
