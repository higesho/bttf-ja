/* ゲーム本体へのフック（.text 復号後に呼ばれる） */
#include "bttfjp.h"

void game_hooks_init(void)
{
    dlog("game_hooks_init: text %02x %02x", *(uint8_t *)0x401000, *(uint8_t *)0x401001);
    font_init();
}
