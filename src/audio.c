#include "common.h"
#include "gool.h"
#include "game.h"
#include "ns.h"

/* Sound effects and music (voices of the SPU, volumes, the music sequence). */

/* .rodata: the debug messages of func_8003328C and func_80033864 */
const char D_8001052C[] = "x:%d y:%d z:%d dist2:%d\ndir:%d cos:%d vol:%d svol:%d   %d/%d\n";
const char D_8001056C[] = "vol %d*%d = %d\n";

/* .sdata */
s32 D_80060A00 = 30;
s32 D_80060A04 = 0;
s32 D_80060A08 = 0;
s32 D_80060A0C = 0;
s32 D_80060A10 = 0;
s32 D_80060A14 = -1;
s32 D_80060A18 = -1;
s32 D_80060A1C = 0;
s32 D_80060A20 = 0;
s32 D_80060A24 = 0x1F800040;
s32 D_80060A28 = 0;

/* .sbss */
s32 D_80060AE8;
s32 D_80060AEC;
s16 D_80060AF0[4];
s32 D_80060AF8;
s16 D_80060AFC[2];
s32 D_80060B00;
s32 D_80060B04;
s32 D_80060B08[2];
s32 D_80060B10[2];
s32 D_80060B18;
s16 D_80060B1C[2];

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032F28);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032F30);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032F38);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032F40);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032F48);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032F50);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032F58);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032F60);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032F68);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032F70);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032F78);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032F80);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032F88);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032F90);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032F98);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032FA0);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032FA8);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032FB0);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032FB8);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032FC0);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80032FC8);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80033000);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80033050);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_8003314C);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80033154);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_8003315C);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80033238);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80033280);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_8003328C);

INCLUDE_ASM("asm/us/nonmatchings/audio", AudioVoiceFree);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_8003359C);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80033760);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80033864);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80033C54);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_800342B8);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80034310);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80034B00);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80034B8C);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80034C0C);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80034C1C);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80034D14);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_8003505C);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80035078);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_800351EC);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80035224);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_8003529C);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_800352FC);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80035410);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80035538);
