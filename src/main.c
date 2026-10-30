#include "common.h"

extern s32 D_800608A4;

void func_800154AC(void);
void func_8001166C(s32 arg0);
void func_80015658(void);

/* GCC inserts the call to __main() (global constructors) by itself. */
s32 main(void) {
    D_800608A4 = 1;
    func_800154AC();
    func_8001166C(60);
    func_80015658();
    return 0;
}

INCLUDE_ASM("asm/us/nonmatchings/main", func_8001166C);
