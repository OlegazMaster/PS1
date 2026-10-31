#include "common.h"
#include "gfx.h"

/* System setup and teardown: CD, events, heap, overlays, video, memory card. */

extern s32 D_80060ABC; /* current level */

void free(void *ptr);
void func_8004C600(s32 mode);
void func_8007170C(void);
s32 func_8005B730(void);
void func_800165F8(DrawContext *dc);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_800154AC);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80015658);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_800156E0);

INCLUDE_RODATA("asm/us/nonmatchings/sys", D_80010230);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80015798);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80016048);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_800160D8);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80016234);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_800163F8);

s32 func_80016544(void) {
    DrawContext *dc;
    s32 i;

    func_800165F8(context.c1_p);
    func_8004C600(0);
    free(context.buffers[0].unk2094);
    for (i = 0; i < context.count; i++) {
        dc = &context.buffers[i];
        free(dc->prims_head);
    }
    if (D_80060ABC == 2 || D_80060ABC == 60) {
        func_8007170C();
    }
    return -0xFF;
}

void func_800165F8(DrawContext *dc) {
    dc->unk2090 = 0;
    dc->unk209C = NULL;
    dc->unk2098 = dc->unk2094;
}

void func_8001660C(DrawContext *dc) {
    dc->unkC = 0;
    dc->prims_tail = dc->prims_head;
    func_800165F8(dc);
}

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80016634);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80016CE4);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80016D8C);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80017204);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_8001742C);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_800176A0);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80017834);

void func_80017AC8(void) {
}

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80017AD0);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80017AE8);

s32 func_80017C88(void) {
    func_8005B730();
    return 0;
}

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80017CA8);
