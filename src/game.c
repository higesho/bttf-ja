/* ゲーム本体へのフック（.text 復号後に呼ばれる）とエピソードごとのアドレス表 */
#include "bttfjp.h"
#include <string.h>

/* exe ごとのアドレス（Steam 版。どれも font.c / text.c で中身を確かめてから使う） */
static const GameAddr g_table[] = {
    { "BackToTheFuture101.exe", 1, 0x65b710, 0x6eccbd, 0x6f05c6, 0x75b190, 0x6ec540, 0x4b3eb0,
      { 0x6eaec0, 0x6eb040, 0x6eb120, 0x6eb250, 0x6eb620, 0x6ec700, 0x6ee8f0 } },
    { "BackToTheFuture102.exe", 2, 0x664230, 0x6f90fd, 0x6fca16, 0x7636e0, 0x6f8970, 0x4b4f70,
      { 0x6f72f0, 0x6f7470, 0x6f7550, 0x6f7680, 0x6f7a50, 0x6f8b30, 0x6fad30 } },
    { "BackToTheFuture103.exe", 3, 0x6642f0, 0x6f91dd, 0x6fcaf6, 0x763e50, 0x6f8a50, 0x4b4c10,
      { 0x6f73d0, 0x6f7550, 0x6f7630, 0x6f7760, 0x6f7b30, 0x6f8c10, 0x6fae10 } },
    { "BackToTheFuture104.exe", 4, 0x6643c0, 0x6f94fd, 0x6fce16, 0x763d40, 0x6f8d70, 0x4b4cb0,
      { 0x6f76f0, 0x6f7870, 0x6f7950, 0x6f7a80, 0x6f7e50, 0x6f8f30, 0x6fb130 } },
};
const GameAddr *g_addr;
static GameAddr g_live;   /* 実際に読み込まれた位置に直したもの（Ep3 の exe は ASLR が有効で 0x400000 以外に載る） */

void game_detect(void)
{
    char path[MAX_PATH], *name;
    int i;
    GetModuleFileNameA(NULL, path, MAX_PATH);
    name = strrchr(path, '\\');
    name = name ? name + 1 : path;
    for (i = 0; i < (int)(sizeof g_table / sizeof g_table[0]); i++)
        if (!_stricmp(name, g_table[i].exe)) {
            uint32_t d = (uint32_t)GetModuleHandleA(NULL) - 0x400000;
            int k;
            g_live = g_table[i];
            g_live.getglyph += d; g_live.pageloop += d; g_live.bindcall += d; g_live.bind += d;
            g_live.settext += d; g_live.uncompress += d;
            for (k = 0; k < 7; k++) g_live.entry[k] += d;
            g_addr = &g_live;
            dlog("exe %s: known, base %08x", name, 0x400000 + d);
        }
    if (!g_addr) dlog("exe %s: unknown", name);
}

void game_hooks_init(void)
{
    if (g_addr) dlog("game_hooks_init: glyph %02x %02x", *(uint8_t *)g_addr->getglyph, *(uint8_t *)(g_addr->getglyph + 1));
    font_init();
}
