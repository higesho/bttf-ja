/* Back to the Future: The Game 日本語化パッチ 共通宣言 */
#pragma once
#include <windows.h>
#include <stdint.h>

/* ---- main.c ---- */
extern int g_test;                      /* テストモード（BTTFJP_TEST=1） */
extern int g_font_state;                /* font.c：-1 ならフォントのフックが使えない */
extern HMODULE g_self;
void dlog(const char *fmt, ...);
void patch_jmp(uint32_t at, void *to);
void patch_bytes(uint32_t at, const void *b, int n);
void *iat_hook(const char *dll, const char *func, void *repl);

/* ---- testio.c ---- */
void testio_init(void);
void testio_present(void *dev);
HRESULT testio_wrap_dinput(void *di);

/* ---- dbg.c（テスト専用：ハードウェアブレークポイントでの読み取り監視） ---- */
void dbg_init(void);
void dbg_watch(uint32_t addr, int slot, int rw_len);
void dbg_dump(void);

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
