#include "common.h"
#include "gfx.h"
#include "game.h"
#include "pad.h"
#include "ns.h"

/* System setup and teardown: CD, events, heap, overlays, video, memory card. */

extern s32 D_80060ABC; /* current level */

/* .sdata, in the order of the original */
s32 D_800608E0 = 0;
s32 done = 0;
s32 D_800608E8 = 0;
u8 D_800608EC[6] = { 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF }; /* pad actuator alignment */
s32 D_800608F4 = 0;

/* .sbss */
s32 D_80060A60;
s32 D_80060A64;
s32 D_80060A68;
s32 D_80060A6C;
s32 D_80060A70;
s32 D_80060A74;
s32 D_80060A78;
s32 D_80060A7C;
s32 D_80060A80;
s32 D_80060A84;
s32 D_80060A88;
s32 D_80060A8C;

typedef struct {
    u16 c[16];
} Clut16;

void free(void *ptr);

void func_8007170C(void);
s32 func_8005B730(void);
void func_800165F8(DrawContext *dc);
void GpuResetPrims(DrawContext *dc);
void GpuInit(void);

extern s32 use_cd;
extern u8 scratchpad[]; /* the 1 KB of fast RAM at 0x1F800000 */
extern u32 D_80060B90[];
extern PageStruct D_80060D70[16];
void func_8004BFBC(s32);
void func_8004F37C(void);
s32 CdInit(void);
void func_8004592C(s32);
void func_80032144(void);
void func_800322D0(void);
void func_80017AE8(void);
void PadInit(s32 count);
void VSyncCallback(void (*func)(void));
void func_80017AD0(void);
void func_8003AE70(void);
void EnterCriticalSection(void);
void ExitCriticalSection(void);
void StopRCnt(u32 spec);
void StartRCnt(u32 spec);
void SetRCnt(u32 spec, u16 target, s32 mode);
s32 OpenEvent(u32 desc, s32 spec, s32 mode, void (*func)(void));
s32 EnableEvent(s32 event);
s32 func_8001549C(void); /* returns u8 in ns.c; declared as int here, as in the original */

#ifdef NON_MATCHING
/* Starts the system: scratchpad, CD, video, sound, pads, the vsync callback
 * and the root counter event. Equivalent; the original strength-reduces the
 * scratchpad clearing loop to a pointer and loads the EID constant before
 * the counters of the two fill loops. */
s32 SysInit(void) {
    u8 *p;
    s32 i;

    p = (u8 *)0x1F800000;
    for (i = 0x3FF; i >= 0; i--) {
        p[i] = 0;
    }
    func_8004BFBC(0);
    func_8004F37C();
    if (use_cd) {
        ResetCallback();
        CdInit();
        func_8004592C(1);
    }
    GpuInit();
    func_80032144();
    ns.draw_skip_counter = 0;
    for (i = 3; i >= 0; i--) {
        D_80060B90[i] = 0x6396347F;
    }
    for (i = 15; i >= 0; i--) {
        D_80060D70[i].eid = 0x6396347F;
    }
    func_80017AE8();
    PadInit(2);
    VSyncCallback(func_80017AD0);
    EnterCriticalSection();
    StopRCnt(0xF2000002);
    D_80060A8C = OpenEvent(0xF2000002, 2, 0x1000, func_8003AE70);
    EnableEvent(D_80060A8C);
    SetRCnt(0xF2000002, 0x1010, 0x1000);
    StartRCnt(0xF2000002);
    ExitCriticalSection();
    if (use_cd) {
        func_800322D0();
    }
    game.unk36C = func_8001549C();
    if (game.unk36C < 3) {
        game.unk21C = 0x1000;
    } else {
        game.unk21C = 0x1016;
    }
    game.unk364 = -1;
    return -0xFF;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/sys", SysInit);
#endif

s32 CdControlB(s32 com, u8 *param, u8 *result);
void func_8004B590(void);
s32 func_80017C88(void);
void func_80048E88(void);
s32 DisableEvent(s32 event);
s32 CloseEvent(s32 event);

s32 SysKill(void) {
    if (use_cd) {
        CdControlB(8, NULL, NULL);
        func_8004B590();
    }
    func_80017C88();
    func_80048E88();
    VSyncCallback(NULL);
    EnterCriticalSection();
    StopRCnt(0xF2000002);
    DisableEvent(D_80060A8C);
    CloseEvent(D_80060A8C);
    ExitCriticalSection();
    return -0xFF;
}

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

/* The CLUT of the screen fade (see func_8001742C). */
const Clut16 D_80010230 = { { 0xFFFF, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
                              0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000 } };

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

extern s32 _ramsize;
extern s32 _stacksize;
extern u8 D_8006EA78[]; /* end of .bss: where the overlay is loaded */
extern u8 D_8006EA80[];
extern u8 D_80072230[]; /* end of the overlay */
void D_8006F64C(void); /* overlay functions */
void D_8006F780(void);
void func_800716A0(void);
void *NSFileRead(CdFile *file, void *buf, s32 mode, s32 arg3);
u32 NSChecksum(u32 *buf, s32 size);
void InitHeap(void *start, u32 size);
void FlushCache(void);
/* Loads the overlay for the title and the warp room (levels 2 and 60), then
 * puts the heap after the overlay or after .bss. */
#ifdef NON_MATCHING
/* Equivalent; the original computes the heap size as
 * _ramsize - (_stacksize + 0x80000000) without folding the constant. */

s32 func_80016234(s32 lid) {
    s32 sectors;
    u32 *last;
    u32 checksum;
    u8 *heap;

    if (lid == 2 || lid == 60) {
    retry:
        if (use_cd) {
            NSFileRead((CdFile *)&D_8006C4D8[61], D_8006EA78, 1, 0);
            sectors = (((CdFile *)&D_8006C4D8[61])->size + 0x7FF) / 0x800;
        }
        last = (u32 *)(D_8006EA78 + (sectors << 11)) - 1;
        checksum = *last;
        *last = 0;
        if (NSChecksum((u32 *)D_8006EA78, sectors << 11) != checksum) {
            printf("fwc %d\n", D_800608F4++);
            goto retry;
        }
        heap = D_80072230;
        if (heap < (u8 *)D_800608E0) {
            heap = (u8 *)D_800608E0;
        }
        EnterCriticalSection();
        FlushCache();
        InitHeap(heap, (u8 *)0x80000000 + _ramsize - _stacksize - heap);
        ExitCriticalSection();
        subsys[21].init = D_8006F64C;
        subsys[21].kill = D_8006F780;
        func_800716A0();
    } else {
        heap = D_8006EA80;
        subsys[21].init = NULL;
        subsys[21].kill = NULL;
        if (heap < (u8 *)D_800608E0) {
            heap = (u8 *)D_800608E0;
        }
        EnterCriticalSection();
        InitHeap(heap, (u8 *)0x80000000 + _ramsize - _stacksize - heap);
        ExitCriticalSection();
    }
    return -0xFF;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/sys", func_80016234);
#endif

#ifndef NON_MATCHING
/* The "fwc %d\n" literal of func_80016234, which is still assembly. */
char D_800608F8[] = "fwc %d\n";
#endif

/* .sdata continues after the literal of func_80016234 */
s32 D_80060900 = 0;
s32 D_80060904 = 0;
s32 D_80060908 = 0;
/* Not used by the game code (the overlays may use them). */
s32 D_8006090C = 1;
s32 D_80060910 = 0;
s32 D_80060914 = 0;
s32 D_80060918 = 0x808080;
s32 D_8006091C = 0x8000;

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
 * or the whole screen. Equivalent; the original addresses game.unk364 before
 * the base of game. */
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
        p = GpuAllocPrim(POLY_FT4);
        GpuDrawFadeQuad(p, type, 256, 60, 384, 120, 128, 120, 256, 180);
    }
    if (type >= 31) {
        p = GpuAllocPrim(POLY_FT4);
        GpuDrawFadeQuad(p, type, 0, 0, 512, 0, 0, 240, 512, 240);
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/sys", func_80017204);
#endif

void func_80016D8C(s32 count, Clut16 *clut);
void func_80017204(s32 type);

void func_8001742C(s32 fade) {
    RECT move;
    RECT rect;
    Clut16 clut;
    DR_STP *stp;
    POLY_FT4 *p;
    DR_MOVE *mv;

    clut = D_80010230;
    if (fade > 0) {
        func_80016D8C(16, &clut);
        func_80017204(fade);
    } else {
        rect.y = 256;
        rect.w = 16;
        rect.x = 0;
        rect.h = 1;
        LoadImage(&rect, (u32 *)&clut);
        DrawSync(0);
        stp = GpuAllocPrim(DR_STP);
        setlen(stp, 2);
        stp->code[0] = 0xE6000000;
        stp->code[1] = 0;
        AddPrim(&context.cur->ot[0x7FF], stp);
        p = GpuAllocPrim(POLY_FT4);
        p->r0 = 0;
        p->g0 = 0;
        p->b0 = 0;
        p->tpage = GetTPage(2, 1, 0, 256);
        p->x0 = 0;
        p->y0 = 0;
        p->x1 = 512;
        p->y1 = 0;
        p->x2 = 0;
        p->y2 = 240;
        p->x3 = 512;
        p->y3 = 240;
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
        stp = GpuAllocPrim(DR_STP);
        setlen(stp, 2);
        stp->code[0] = 0xE6000001;
        stp->code[1] = 0;
        AddPrim(&context.cur->ot[0x7FF], stp);
    }
    mv = GpuAllocPrim(DR_MOVE);
    move.x = context.c2_p->draw.clip.x;
    move.y = 0;
    move.w = 512;
    move.h = 240;
    SetDrawMove(mv, &move, context.cur->draw.clip.x, 0);
    AddPrim(&context.cur->ot[0x7FF], mv);
}

extern u8 D_8005C000[];

/* Darkens the screen; fade (0..255) indexes a table of gray levels. */
void func_800176A0(u32 fade) {
    DR_MODE *mode;
    POLY_F4 *p;
    void **ot;
    u8 *t;
    u32 lo;
    s32 abr;

    mode = GpuAllocPrim(DR_MODE);
    p = GpuAllocPrim(POLY_F4);
    ot = &context.cur->ot[0x7FF];
    if (fade != 0) {
        while (((u32)*ot & 0xFFFFFF) != 0xFFFFFF) {
            ot = (void **)(((u32)ot & 0xFF000000) | ((u32)*ot & 0xFFFFFF));
        }
        t = &D_8005C000[fade >> 4];
        lo = t[0];
        fade = (((t[1] - lo) * (fade & 0xF)) >> 4) + lo;
        p->x0 = 0;
        p->x2 = 0;
        p->y0 = -12;
        p->y1 = -12;
        p->x1 = 512;
        p->y2 = 228;
        p->x3 = 512;
        p->y3 = 228;
        p->r0 = fade;
        p->g0 = fade;
        p->b0 = fade;
        SetPolyF4(p);
        SetSemiTrans(p, 1);
        AddPrim(ot, p);
        abr = 2;
        if (game.display_flags & 0x800000) {
            abr = 1;
        }
        SetDrawMode(mode, 1, 0, GetTPage(0, abr, 0, 0), NULL);
        AddPrim(ot, mode);
    }
}

s32 func_80025C0C(void);
void func_8004F724(s32 vol);

#ifdef NON_MATCHING
/* Shrinks the picture behind black borders (t steps) and lowers the volume
 * with it. Equivalent; the original swaps the registers of t2 and bar, and
 * schedules a few instructions differently because of it. */
void func_80017834(u32 t, u32 t2) {
    POLY_F4 *p;
    u32 bar, x, y;
    s32 left, right, top, bottom, mid, w;
    s32 vol;

    bar = t * 25;
    x = (bar * 4) >> 3;
    y = (t * 60) >> 3;
    p = GpuAllocPrim(POLY_F4);
    if (context.c2_p->idx == 1) {
        context.c2_p->draw.ofs[0] = 512 - x;
    } else {
        context.c2_p->draw.ofs[0] = -x;
    }
    context.c2_p->draw.ofs[1] = 12 - y;
    if (t != 0) {
        if (t2 == 0) {
            y = 0;
            x = 0;
        } else {
            x = (t2 * 100) >> 3;
            y = (t2 * 60) >> 3;
        }
        right = x + 512;
        left = right - bar;
        top = y - 12;
        bottom = y + 228;
        p->x0 = left;
        p->y0 = top;
        p->x1 = right;
        p->y1 = top;
        p->x2 = left;
        p->y2 = bottom;
        p->x3 = right;
        p->y3 = bottom;
        p->r0 = 0;
        p->g0 = 0;
        p->b0 = 0;
        SetPolyF4(p);
        AddPrim(&context.cur->ot[0x7FF], p);
        p = GpuAllocPrim(POLY_F4);
        mid = bottom - t * 15;
        p->x0 = x;
        p->y0 = mid;
        p->x1 = left;
        p->y1 = mid;
        p->x2 = x;
        p->y2 = bottom;
        p->x3 = left;
        p->y3 = bottom;
        p->r0 = 0;
        p->g0 = 0;
        p->b0 = 0;
        SetPolyF4(p);
        AddPrim(&context.cur->ot[0x7FF], p);
        w = x + ((t * 9) >> 3);
        p = GpuAllocPrim(POLY_F4);
        p->x0 = x;
        p->y0 = top;
        p->x1 = w;
        p->y1 = top;
        p->x2 = x;
        p->y2 = bottom;
        p->x3 = w;
        p->y3 = bottom;
        p->r0 = 0;
        p->g0 = 0;
        p->b0 = 0;
        SetPolyF4(p);
        AddPrim(&context.cur->ot[0x7FF], p);
    }
    vol = func_80025C0C();
    vol -= (vol / 24) * t;
    if (D_80065C90.unkC4 != vol) {
        D_80065C90.unkC4 = vol;
        func_8004F724(vol);
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/sys", func_80017834);
#endif

void func_80017AC8(void) {
}

/* VSync callback: counts vertical blanks. */

void func_80017AD0(void) {
    D_800608E8++;
}

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80017AE8);

s32 func_80017C88(void) {
    func_8005B730();
    return 0;
}

INCLUDE_ASM("asm/us/nonmatchings/sys", func_80017CA8);
