#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

/* The GOOL interpreter: state changes, events, opcodes (gool.c in c1). */

extern u8 D_8005C488[];

/* .sdata */
u8 *D_80060924 = D_8005C488;

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001D16C);

INCLUDE_ASM("asm/us/nonmatchings/interp", GoolObjectChangeState);

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001D588);

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001D5E8);

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001D674);

INCLUDE_ASM("asm/us/nonmatchings/interp", GoolSendEvent);

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001DDF4);

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001E34C);

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001E3BC);

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001E42C);

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001E4BC);

INCLUDE_ASM("asm/us/nonmatchings/interp", func_8001E78C);
