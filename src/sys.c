#include "common.h"
#include "gfx.h"
#include "game.h"
#include "pad.h"

/* System setup and teardown: CD, events, heap, overlays, video, memory card. */

extern s32 D_80060ABC; /* current level */

void free(void *ptr);

void func_8007170C(void);
s32 func_8005B730(void);
void func_800165F8(DrawContext *dc);
void GpuResetPrims(DrawContext *dc);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_800154AC);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80015658);

void PadInitDirect(u8 *buf1, u8 *buf2);
void PadStartCom(void);

void PadInit(s32 count) {
    s32 i;

    game.unk164 = 0;
    game.unk18C = 0;
    game.unk1F0 = 0;
    game.unk1F4 = 0;
    game.unk224 = 0;
    game.unk220 = 0;
    pads.count = count;
    for (i = 0; i < count; i++) {
        pads.pads[i].tapped = pads.pads[i].held = pads.pads[i].held_prev = pads.pads[i].held_prev2 = 0;
        if (i == 0) {
            PadInitDirect((u8 *)&pads.pads[0], (u8 *)&pads.pads[1]);
            PadStartCom();
        }
        pads.pads[i].unk4C = 0;
        pads.pads[i].unk4D = 0;
        pads.pads[i].unk48 = 1;
    }
}

INCLUDE_RODATA("asm/us/nonmatchings/sys", D_80010230);

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80015798);

void GpuClearVram(void) {
    DRAWENV env;
    BLK_FILL bf;

    SetDefDrawEnv(&env, 0, 0, 1024, 512);
    env.dfe = 1;
    PutDrawEnv(&env);
    TermPrim(&bf);
    setBlockFill(&bf);
    bf.w = 1023;
    bf.h = 511;
    bf.r0 = 0;
    bf.g0 = 0;
    bf.b0 = 0;
    bf.x0 = 0;
    bf.y0 = 0;
    DrawPrim(&bf);
}

extern s32 D_80069034[];
extern s32 ticks_elapsed;

void GpuInit(void) {
    DrawContext *dc;
    s32 i;

    D_80069034[0] = 0;
    ResetGraph(0);
    SetGraphDebug(0);
    GpuClearVram();
    context.count = 2;
    for (i = 0; i < context.count; i++) {
        dc = &context.buffers[i];
        dc->idx = i;
        dc->draw_stamp = 0;
        SetDefDispEnv(&dc->disp, i << 9, 0, 512, 240);
        game.unk1CC = 0;
        dc->disp.screen.x = 0;
        SetDefDrawEnv(&dc->draw, i << 9, 12, 512, 216);
        dc->draw.ofs[1] = 12;
        dc->draw.ofs[0] = i << 9;
        dc->draw.dtd = 1;
        dc->draw.dfe = 0;
        if (i == 0) {
            context.cur = dc;
            context.c1_p = dc;
        }
        if (i == 1) {
            context.c2_p = dc;
        }
        dc->ticks_per_frame = 34;
        dc->draw_stamp = ticks_elapsed;
    }
    PutDispEnv(&context.c1_p->disp);
    PutDrawEnv(&context.c2_p->draw);
    SetDispMask(1);
}

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80016234);

void *malloc(s32 size);
void exit(s32 status);
void func_8003AC44(void *ot, s32 count);

s32 GpuSetupPrims(void) {
    DrawContext *dc;
    void *mem;
    s32 size;
    s32 i;

    func_8003AC44(context.cur->ot, 0x800);
    func_8003AC44(context.c2_p->ot, 0x800);
    mem = malloc(0x1A40);
    for (i = 0; i < context.count; i++) {
        dc = &context.buffers[i];
        dc->unk2094 = mem;
        if (D_80060ABC == 40) {
            game.unk70 &= ~2;
        }
        switch (D_80060ABC) {
        case 3:
        case 5:
        case 6:
        case 19:
        case 23:
        case 31:
        case 42:
            size = 0x1E400;
            break;
        case 2:
            size = 0x18000;
            break;
        default:
            size = 0x16400;
            break;
        }
        if ((dc->prims_head = malloc(size)) == NULL) {
            exit(-0xF);
        }
        GpuResetPrims(dc);
    }
    return -0xFF;
}

s32 GpuKill(void) {
    DrawContext *dc;
    s32 i;

    func_800165F8(context.cur);
    DrawSync(0);
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

void GpuResetPrims(DrawContext *dc) {
    dc->unkC = 0;
    dc->prims_tail = dc->prims_head;
    func_800165F8(dc);
}

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80016634);

void GpuDrawRect(s32 x, s32 y, s32 w, s32 h, s32 r, s32 g, s32 b) {
    BLK_FILL *bf;

    bf = context.cur->prims_tail;
    context.cur->prims_tail = bf + 1;
    setBlockFill(bf);
    bf->w = w;
    bf->h = h;
    bf->r0 = r;
    bf->g0 = g;
    bf->b0 = b;
    bf->x0 = context.cur->draw.clip.x + x;
    bf->y0 = context.cur->draw.clip.y + y;
    AddPrim(context.cur->ot, bf);
}

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80016D8C);

#ifdef NON_MATCHING
/* Fades the screen with a dark semi-transparent quad: a diamond in the middle,
 * or the whole screen. Equivalent; the original copies the primitive pointer
 * from another register and addresses game.unk364 before the rest of game. */
static inline void GpuDrawFadeQuad(POLY_FT4 *p, s32 type, s32 x0, s32 y0, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3) {
    p->r0 = 8;
    p->g0 = 8;
    p->b0 = 8;
    p->tpage = GetTPage(2, 2, 0, 256);
    p->x0 = x0;
    p->y0 = y0;
    p->x1 = x1;
    p->y1 = y1;
    p->x2 = x2;
    p->y2 = y2;
    p->x3 = x3;
    p->y3 = y3;
    p->u0 = 0;
    p->v0 = 0;
    p->u1 = 0;
    p->v1 = 0;
    p->u2 = 0;
    p->v2 = 0;
    p->u3 = 0;
    p->v3 = 0;
    SetPolyFT4(p);
    SetSemiTrans(p, 1);
    AddPrim(&context.cur->ot[0x7FF], p);
    if (type >= 63) {
        game.unk364 = -1;
        game.unk208 = 0x120;
        game.unk70 &= ~0x800000;
    }
}

void func_80017204(s32 type) {
    POLY_FT4 *p;

    if (type >= 7 && type < 30) {
        p = context.cur->prims_tail;
        context.cur->prims_tail = p + 1;
        GpuDrawFadeQuad(p, type, 256, 60, 384, 120, 128, 120, 256, 180);
    }
    if (type >= 31) {
        p = context.cur->prims_tail;
        context.cur->prims_tail = p + 1;
        GpuDrawFadeQuad(p, type, 0, 0, 512, 0, 0, 240, 512, 240);
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/sys", func_80017204);
#endif

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
