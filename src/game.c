/* ゲーム本体へのフック（.text 復号後に呼ばれる）とエピソードごとのアドレス表 */
#include "bttfjp.h"
#include <string.h>

/* exe ごとのアドレス（Steam 版。どれも font.c / text.c で中身を確かめてから使う） */
static const GameAddr g_table[] = {
    { "BackToTheFuture101.exe", 1, 0x65b710, 0x6eccbd, 0x6f05c6, 0x75b190, 0x6ec540, 0x4b3eb0,
      { 0x6eaec0, 0x6eb040, 0x6eb120, 0x6eb250, 0x6eb620, 0x6ec700, 0x6ee8f0 } },
    { "BackToTheFuture102.exe", 2, 0x664230, 0x6f90fd, 0x6fca16, 0x7636e0, 0x6f8970, 0x4b4f70,
      { 0x6f72f0, 0x6f7470, 0x6f7550, 0x6f7680, 0x6f7a50, 0x6f8b30, 0x6fad30 } },
};
const GameAddr *g_addr;

void game_detect(void)
{
    char path[MAX_PATH], *name;
    int i;
    GetModuleFileNameA(NULL, path, MAX_PATH);
    name = strrchr(path, '\\');
    name = name ? name + 1 : path;
    for (i = 0; i < (int)(sizeof g_table / sizeof g_table[0]); i++)
        if (!_stricmp(name, g_table[i].exe)) g_addr = &g_table[i];
    dlog("exe %s: %s", name, g_addr ? "known" : "unknown");
}

void game_hooks_init(void)
{
    dlog("game_hooks_init: text %02x %02x", *(uint8_t *)0x401000, *(uint8_t *)0x401001);
    font_init();
}
