/* Back to the Future: The Game 日本語化パッチ 共通宣言 */
#pragma once
#include <windows.h>
#include <stdint.h>

/* 試験用の仕組み（ログ、testio.c、dbg.c、環境変数 BTTFJP_*）は試験版（build.bat test、/DBTTFJP_TEST）にだけ入る。
   配布版には入れない */

/* ---- main.c ---- */
extern int g_font_state;                /* font.c：-1 ならフォントのフックが使えない */
extern HMODULE g_self;
void patch_jmp(uint32_t at, void *to);
void patch_bytes(uint32_t at, const void *b, int n);
void *iat_hook(const char *dll, const char *func, void *repl);
#ifdef BTTFJP_TEST
extern int g_test;                      /* テストモード（BTTFJP_TEST=1） */
void dlog(const char *fmt, ...);

/* ---- testio.c ---- */
void testio_init(void);
void testio_present(void *dev);
HRESULT testio_wrap_dinput(void *di);

/* ---- dbg.c（テスト専用：ハードウェアブレークポイントでの読み取り監視） ---- */
void dbg_init(void);
void dbg_watch(uint32_t addr, int slot, int rw_len);
void dbg_dump(void);
#else
#define dlog(...) ((void)0)             /* 配布版はログを出さない */
#endif

/* ---- game.c：エピソードごとのアドレス ---- */
typedef struct {
    const char *exe;
    int ep;
    uint32_t getglyph;      /* Font::GetGlyph */
    uint32_t pageloop;      /* メッシュ作成のページ数の比較 cmp ebp,[ebx+0x40] */
    uint32_t bindcall;      /* ページのテクスチャを渡す call */
    uint32_t bind;          /* その呼び先 */
    uint32_t settext;       /* テキストの SetText（テスト用の記録） */
    uint32_t uncompress;    /* zlib の raw inflate（uncompress と同じ引数） */
    uint32_t entry[7];      /* 文字列を走査する 7 関数 */
    uint32_t bfinit, bfdec; /* エピソード5：Blowfish の初期化（thiscall）と復号（thiscall buf,len）。ほかは 0 */
} GameAddr;
extern const GameAddr *g_addr;
void game_detect(void);
void game_hooks_once(void);              /* main.c */

/* ---- fileio.c ---- */
void fileio_init(void);

/* ---- font.c ---- */
void font_init(void);
void font_device(void *dev);
void font_upload(void);

/* ---- text.c ---- */
uint64_t fnv64(const uint8_t *s, int n);
HANDLE text_findfirst(const char *pat, WIN32_FIND_DATAA *fd);
int text_isfind(HANDLE h);
BOOL text_findnext(HANDLE h, WIN32_FIND_DATAA *fd);
BOOL text_findclose(HANDLE h);
const char *text_path(const char *path, char *buf);
