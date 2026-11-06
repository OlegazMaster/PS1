/* The few functions of the game that are still assembly in the PlayStation
 * build (asm/us/2768.s), written in C for the PC port. */
#include "common.h"
#include "game.h"

extern s32 D_80060878;
extern RenderState D_800676B0;
extern u8 nd_entry_80041648[];
extern u8 nd_entry_800418CC[];
extern u8 nd_entry_80041768[];
extern u8 nd_entry_80041A3C[];

/* the default procedures of the renderer, unless already set */
void func_80011F68(void)
{
    if (D_80060878 == 0) {
        *(void **)((u8 *)&D_800676B0 + 0x18) = nd_entry_80041648;
        *(void **)((u8 *)&D_800676B0 + 0x1C) = nd_entry_800418CC;
        *(void **)((u8 *)&D_800676B0 + 0x20) = nd_entry_80041768;
        *(void **)((u8 *)&D_800676B0 + 0x24) = nd_entry_80041A3C;
    }
}

void func_80011FB8(void)
{
}

void func_80011FC0(void)
{
}
