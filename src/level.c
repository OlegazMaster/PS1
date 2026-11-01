#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

/* Starting a level: the checkpoint and spawn state, level data. */

/* .sdata */
s32 bonus_return = 0;
s32 D_8006092C = 1;

/* .sbss */
s32 D_80060AA0;
s32 D_80060AA4;
s32 D_80060AA8[2];
s32 D_80060AB0;
s32 D_80060AB4;
s32 D_80060AB8;
s32 D_80060ABC;

INCLUDE_ASM("asm/us/nonmatchings/level", func_80021084);

INCLUDE_ASM("asm/us/nonmatchings/level", func_80021330);

INCLUDE_ASM("asm/us/nonmatchings/level", func_80021474);

INCLUDE_ASM("asm/us/nonmatchings/level", func_800214B8);

INCLUDE_ASM("asm/us/nonmatchings/level", func_80021530);

INCLUDE_ASM("asm/us/nonmatchings/level", func_8002157C);

INCLUDE_ASM("asm/us/nonmatchings/level", func_800215E8);
