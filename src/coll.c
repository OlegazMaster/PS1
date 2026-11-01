#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

/* Collision of objects with the surfaces of the zones and with other
 * objects. */

/* .sdata */
s32 D_800609C0 = 0xF424;

/* .sbss */
s32 D_80060AC8;
s32 D_80060ACC;
s32 D_80060AD0;
s32 D_80060AD4;
s32 D_80060AD8;
s32 D_80060ADC;
s32 D_80060AE0;
s32 D_80060AE4;

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002AA94);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002AF70);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002B1CC);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002B394);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002B4E8);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002B838);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002B9A4);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002C230);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002C604);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002CAE4);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002CD8C);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002CEA8);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002D0E8);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002D85C);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002D8C8);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002D8E0);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002D93C);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002D944);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002D990);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002DA74);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002E4F0);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002EC2C);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002EDA8);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002EDF0);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002F0F8);
