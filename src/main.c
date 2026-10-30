#include "common.h"

/* .sdata */
s32 D_80060878 = 0;
s32 paused = 0;
/* Read by the PSY-Q startup code: stack size and amount of RAM. */
s32 _stacksize = 0x1000;
s32 _ramsize = 0x200000;
s32 pause_status = 0;
/* Placeholder for the "dead\n" literal of CoreLoop() until it is decompiled. */
char D_8006088C[] = "dead\n";

/* .sbss */
s32 pause_stamp;
s32 pause_draw_stamp;

extern s32 use_cd;

void func_800154AC(void);
void CoreLoop(s32 lid);
void func_80015658(void);

/* GCC inserts the call to __main() (global constructors) by itself. */
s32 main(void) {
    use_cd = 1;
    func_800154AC();
    CoreLoop(60);
    func_80015658();
    return 0;
}

INCLUDE_ASM("asm/us/nonmatchings/main", CoreLoop);
