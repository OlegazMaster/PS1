#include "common.h"
#include "gfx.h"
#include "game.h"
#include "pad.h"
#include "ns.h"
#include "gool.h"

extern s32 paused;

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
s32 StopCARD(void);
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
void CardInit(void);
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
    CardInit();
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
s32 CardKill(void);
void func_80048E88(void);
s32 DisableEvent(s32 event);
s32 CloseEvent(s32 event);

s32 SysKill(void) {
    if (use_cd) {
        CdControlB(8, NULL, NULL);
        func_8004B590();
    }
    CardKill();
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

extern Entry *cur_zone;
extern s32 D_8005BF00[][4]; /* gool.c: the direction of each d-pad combination */
extern struct {
    u8 unk0[0x18];
    u32 unk18;
} *D_80061A7C;
s32 func_80048F00(s32 port); /* PadInfoMode, called with only the port */
s32 func_80048FC0(s32 port, s32 term, s32 offs); /* PadInfoAct */
void func_80049234(s32 port, u8 *data); /* PadSetActAlign */
void func_8004926C(s32 port, s32 a, s32 b); /* PadSetMainMode */
void func_800492B4(s32 port, u8 *data, s32 len); /* PadSetAct */
s32 func_8001FD58(s32 angle, s32 a, s32 b);
s32 func_8003D9DC(s32 x); /* square root */
s32 func_8004F794(s32 x, s32 z); /* angle of (x, z) */
#ifdef NON_MATCHING
/* Reads the pads every frame: the buttons (tapped this frame), the analog
 * stick of analog pads (calibrated with game.unk2C0..; its angle, its
 * magnitude and one of 8 directions, also mapped to the d-pad), sets the
 * vibration of the first pad, plays back the pad of a demo, and copies the
 * stick of the pads into game.
 * Equivalent; differs in scheduling (the analog stick, the demo end, the copy to game) and a few registers. */
void func_80015798(void) {
    s32 i;
    Pad *pad;
    s32 port;
    s32 x;
    s32 y;
    s32 d;
    s32 t;
    u32 b;
    u32 m;
    u32 k;
    u32 n;
    s32 *dir;
    u32 end;

    for (i = 0, port = 0; i < pads.count; i++, port += 0x10) {
        pad = &pads.pads[i];
        pad->held_prev2 = pad->held_prev;
        pad->held_prev = pad->held;
        pad->tapped_prev = pad->tapped;
        if (func_80048F00(port) == 6) {
            if (func_80048FC0(port, 1, 0) != 7 && pad->unk48 == 1) {
                func_8004926C(port, 1, 0);
                pad->unk48 = 0;
            }
            func_80049234(port, D_800608EC);
            pad->unk44 = 2;
        } else {
            pad->unk44 = 0;
        }
        if (*(s8 *)&pad->status == 0) {
            switch (*(s8 *)&pad->raw[0]) {
            case 0x12:
                pad->unk48 = 1;
                pad->status = 1;
                goto act;
            case 0x53:
            case 0x73:
                x = pad->raw[5];
                y = pad->raw[6];
                x -= game.unk2C0;
                if (x < 0) {
                    x = (x << 7) / (game.unk2C0 - game.unk2C8);
                } else {
                    x = (x << 7) / (game.unk2D0 - game.unk2C0);
                }
                t = -0x80;
                if (x < 0x7F) {
                    if (x >= -0x80) {
                        t = x;
                    }
                } else {
                    t = 0x7F;
                }
                x = t;
                y -= game.unk2C4;
                if (y < 0) {
                    y = (y * 0x7F) / (game.unk2C4 - game.unk2CC);
                } else {
                    y = (y * 0x7F) / (game.unk2D4 - game.unk2C4);
                }
                t = -0x80;
                if (y < 0x7F) {
                    if (y >= -0x80) {
                        t = y;
                    }
                } else {
                    t = 0x7F;
                }
                y = t;
                if (cur_zone != NULL && (((ZoneHdr *)cur_zone->items[0])->unk2DC & 0x200000)) {
                    pad->stick_angle = func_8001FD58(func_8004F794(x, y) & 0xFFF, 0x200, 0x5000);
                } else {
                    pad->stick_angle = func_8004F794(x, y) & 0xFFF;
                }
                pad->stick_dir = ((0x8FA - pad->stick_angle) & 0xFFF) / 512;
                pad->stick_mag = (func_8003D9DC(x * x + y * y) << 8) >> 7;
                if (pad->stick_mag < 0x66) {
                    pad->stick_mag = 0;
                    pad->stick_dir = 8;
                }
                b = *(u16 *)&pad->raw[1];
                b = ~(((b & 0xFF) << 8) | (b >> 8));
                pad->held = b & 0xFFFF;
                if (*(s8 *)&pad->raw[0] == 0x53) {
                    k = b & 0xF961;
                    if (b & 2) {
                        k |= 4;
                    }
                    if (b & 4) {
                        k |= 0x80;
                    }
                    if (b & 8) {
                        k |= 0x10;
                    }
                    if (b & 0x10) {
                        k |= 8;
                    }
                    if (b & 0x80) {
                        k |= 2;
                    }
                    pad->held = k;
                }
                goto act;
            default:
                x = y = 0;
                pad->stick_mag = 0;
                pad->stick_dir = 8;
                pad->held = ~(((*(u16 *)&pad->raw[1] & 0xFF) << 8) | (*(u16 *)&pad->raw[1] >> 8)) & 0xFFFF;
            act:
                if (pad->unk44 != 0 && i == 0) {
                    if (!(game.unkE0 & 2) && paused == 0 && game.unk204 == 0 && game.game_state == 0x100) {
                        if (game.unk18C != 0) {
                            pad->unk4C = (1 << ((game.unk18C - 1) & 0x1F)) & game.unk164 ? 0xFF : 0;
                        } else {
                            pad->unk4C = 0;
                        }
                        if (game.unk1F4 != 0) {
                            pad->unk4D = game.unk1F0;
                        } else if (game.unk220 != 0) {
                            pad->unk4D = game.unk224;
                        } else {
                            pad->unk4D = 0;
                        }
                    } else {
                        pad->unk4C = 0;
                        pad->unk4D = 0;
                    }
                    if (paused == 0) {
                        if (game.unk18C != 0) {
                            game.unk18C--;
                        }
                        if (game.unk1F4 != 0) {
                            game.unk1F4--;
                        }
                        if (game.unk220 != 0) {
                            game.unk220--;
                        }
                    }
                    func_800492B4(port, &pad->unk4C, 2);
                }
            }
        } else {
            pad->unk48 = 1;
        }
        if (i == 0) {
            switch ((u32)game.unk204) {
            case 3:
                pad->held = 0;
                break;
            case 4:
                if ((u32)(game.unk208 + 1) < 2) {
                    u32 arg = (u32)D_80060A60 >= (u32)(D_80060A88 + *(s32 *)D_80060A88 * 8 + 0xC9C);

                    if (game.unk190 != 0) {
                        GoolSendEvent(NULL, (GoolObject *)game.unk190, 0xE00, 1, &arg);
                        game.unk70 = 0x108;
                        game.unk204 = 3;
                    } else {
                        game.unk204 = 0;
                    }
                }
                break;
            case 2:
                pad->stick_dir = 8;
                pad->stick_mag = 0;
                m = pad->held;
                if (D_80060A60 == D_80060A88 + 0xC9C) {
                    D_80061A7C->unk18 = *(s32 *)(D_80060A88 + 0xC98);
                }
                pad->held = ((u32 *)D_80060A60)[1];
                D_80060A60 += 8;
                if (((m & 0x9F0) && *(s8 *)&pad->status == 0) || (u32)D_80060A60 >= (u32)(D_80060A88 + *(s32 *)D_80060A88 * 8 + 0xC9C)) {
                    if (game.unk190 != 0) {
                        game.unk204 = 4;
                        game.unk70 &= ~0x900A;
                    } else {
                        game.unk204 = 0;
                    }
                }
                break;
            }
        }
        if ((!(game.display_flags & 0x10000) && paused == 0) || (*(s8 *)&pad->status != 0 && game.unk204 == 0)) {
            pad->stick_mag = 0;
            pad->stick_dir = 8;
            pad->held &= 0xFFFF0F13;
        }
        if (pad->held & 0x1000) {
            pad->held &= ~0x4000;
        }
        if (pad->held & 0x8000) {
            pad->held &= ~0x2000;
        }
        n = (pads.pads[i].held & 0xF000) >> 12;
        if (n != 0) {
            dir = D_8005BF00[n];
            pad->stick_dir = dir[0];
            pad->stick_angle = dir[1];
            pad->stick_mag = dir[2];
            x = ((s8 *)dir)[12];
            y = -((s8 *)dir)[13];
        } else {
            switch (pad->stick_dir) {
            case 0:
                pad->held |= 0x1000;
                break;
            case 1:
                pad->held |= 0x3000;
                break;
            case 2:
                pad->held |= 0x2000;
                break;
            case 3:
                pad->held |= 0x6000;
                break;
            case 4:
                pad->held |= 0x4000;
                break;
            case 5:
                pad->held |= 0xC000;
                break;
            case 6:
                pad->held |= 0x8000;
                break;
            case 7:
                pad->held |= 0x9000;
                break;
            }
        }
        switch (i) {
        case 0:
            game.unk250 = pad->stick_mag;
            game.unk2E4 = x;
            game.unk2E8 = -y;
            game.unk254 = pad->stick_angle;
            break;
        case 1:
            game.unk300 = x;
            game.unk304 = -y;
            game.unk25C = pad->held;
            break;
        }
        pad->tapped = pad->held & ~pad->held_prev & 0xFFFF;
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/sys", func_80015798);
#endif

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

void func_800176A0(u32 fade);
void func_80017834(u32 t, u32 t2);
void func_8001742C(s32 fade);
u8 *func_80032710(u8 *item, s32 tag, s32 pos, s32 index, s32 *outcount, s32 mode, u8 *buf, s32 count, s32 def);
void func_80034310(void);
void func_8004CBF4(void *ot); /* DrawOTag */
void func_8006EC9C(void); /* overlay */
void func_8006F9B8(void); /* overlay */
extern s32 D_800690A0[]; /* game.unk208: the screen fade and its step */
extern s32 D_800691FC[]; /* game.unk364 */
#ifdef NON_MATCHING
/* Ends the frame (c1: GpuUpdate): the screen fade, the background of the
 * zone (one color, or two split at the horizon), the screen transition,
 * the sound (unless loading), then swaps and shows the draw buffers, waiting
 * for the vertical sync, and keeps the timers.
 * Equivalent; the original also addresses game from &game.unk364 and
 * computes y - 108 before the projection. */
void func_80016634(s32 loading) {
    s32 type;
    u32 color;
    u32 color2;
    s32 y;
    s32 *p;
    s16 *h;
    u8 buf[8];
    DrawContext *dc;
    s32 *fade;
    Transform *cam;

    if (context.cur->unkC == 0) {
        func_8003AC44(context.cur->ot, 0x800);
        context.cur->unkC = 1;
    }
    if (paused == 0 && (game.display_flags & 0x1000)) {
        game.unk19C++;
    }
    fade = &game.unk208;
    if (fade[0] != 0 && paused != 1) {
        if (fade[0] > 0) {
            if (paused == 0) {
                fade[0] -= fade[1];
                if (fade[0] < 0) {
                    fade[0] = 0;
                }
            }
            func_800176A0(fade[0]);
        } else if (fade[0] >= -2) {
            func_800176A0(0x100);
            if (paused == 0) {
                fade[0] = -1;
            }
        } else {
            if (paused == 0) {
                fade[0] += fade[1];
            }
            func_800176A0(fade[0] + 0x100);
            if (fade[0] == 0 && !(game.display_flags & 0x400000)) {
                fade[0] = -2;
            }
        }
    }
    func_80017834(game.unk184, D_80060900);
    D_80060900 = game.unk184;
    if (!loading && (game.display_flags & 0x2000)) {
        type = 1;
        color = 0;
        if (D_80065C90.unkCC != NULL &&
            (p = (s32 *)func_80032710((u8 *)D_80065C90.unkD8, 0x1FA, D_80065C90.unkE0, 0, NULL, 0, NULL, 0, 0)) != NULL) {
            type = p[0];
            color = p[1];
            if (type == 2) {
                color2 = p[2];
                h = (s16 *)func_80032710((u8 *)D_80065C90.unkD8, 0x1F9, D_80065C90.unkE0, 0, NULL, 3, buf, 0, 0);
                y = h != NULL ? *h : 0;
            }
        }
        if (type == 2) {
            cam = &D_80065C90.unk60;
            y = ((func_8003C2F0(cam->rot[0]) * 170 * *(s32 *)((u8 *)cam + 0x64)) >> 20) - (y - 108);
            if (game.unk184 != 0) {
                y -= (game.unk184 * 120) >> 3;
            }
            if (y < 0) {
                color = color2;
                type = 1;
            } else if (y >= 215) {
                type = 1;
            }
        }
        switch (type) {
        case 0:
            break;
        case 1:
            GpuDrawRect(0, 0, 512, 216, color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF);
            break;
        case 2:
            GpuDrawRect(0, 0, 512, y + 1, color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF);
            GpuDrawRect(0, y + 1, 512, 215 - y, color2 & 0xFF, (color2 >> 8) & 0xFF, (color2 >> 16) & 0xFF);
            break;
        }
    }
    if (D_800691FC[0] >= 0) {
        func_8001742C(D_800691FC[0]++);
    }
    if (!loading) {
        func_80034310();
    }
    context.c2_p->unk14 = ticks_elapsed;
    DrawSync(0);
    if (game.unk204 == 0) {
        game.unk10C = ticks_elapsed - context.c2_p->draw_stamp;
    } else {
        game.unk10C = 17;
    }
    if (ns.draw_skip_counter == 0) {
        dc = context.c2_p;
        context.c1_p = dc;
        context.c2_p = context.cur;
        context.cur = dc;
    }
    if ((u32)(D_800608E8 - context.c1_p->unk1C) < 2) {
        VSync(0);
        if ((u32)(ticks_elapsed - context.c1_p->draw_stamp) < 25) {
            VSync(0);
        }
    }
    if (game.display_flags & 0x200000) {
        if (D_80060904 == 0) {
            EnterCriticalSection();
            StopRCnt(0xF2000002);
            ExitCriticalSection();
        }
        D_80060904 = 1;
        func_8006F9B8();
        ticks_elapsed += 34;
    } else {
        if (D_80060904 != 0) {
            EnterCriticalSection();
            StartRCnt(0xF2000002);
            ExitCriticalSection();
        }
        D_80060904 = 0;
    }
    EnterCriticalSection();
    if (game.unk204 == 2) {
        ticks_elapsed = *(s32 *)D_80060A60;
    }
    context.c2_p->draw_stamp = ticks_elapsed;
    context.c2_p->unk1C = D_800608E8;
    ExitCriticalSection();
    context.c1_p->ticks_per_frame = context.c2_p->draw_stamp - context.c1_p->draw_stamp;
    if ((u32)(context.c1_p->ticks_per_frame - 33) < 3) {
        context.c1_p->ticks_per_frame = 34;
    }
    context.c1_p->disp.screen.x = game.unk1CC;
    PutDispEnv(&context.c1_p->disp);
    PutDrawEnv(&context.c2_p->draw);
    if (ns.draw_skip_counter > 0) {
        ns.draw_skip_counter--;
    }
    if (ns.draw_skip_counter == 0) {
        if (game.display_flags & 0x40000) {
            func_8006EC9C();
        }
        func_8004CBF4(context.c2_p->ot);
    }
    GpuResetPrims(context.cur);
    game.display_flags = game.unk70;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/sys", func_80016634);
#endif

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

#define setRGB0(p, r, g, b) ((p)->r0 = (r), (p)->g0 = (g), (p)->b0 = (b))
#define setXY4(p, _x0, _y0, _x1, _y1, _x2, _y2, _x3, _y3) \
    ((p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x1), (p)->y1 = (_y1), \
     (p)->x2 = (_x2), (p)->y2 = (_y2), (p)->x3 = (_x3), (p)->y3 = (_y3))
#define setUV4(p, _u0, _v0, _u1, _v1, _u2, _v2, _u3, _v3) \
    ((p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u1), (p)->v1 = (_v1), \
     (p)->u2 = (_u2), (p)->v2 = (_v2), (p)->u3 = (_u3), (p)->v3 = (_v3))
s32 func_8003C2F0(s32 angle); /* sin */
s32 func_8003C364(s32 angle); /* cos */
#ifdef NON_MATCHING
/* Draws the screen being drawn turned by angle around (256, 108): four
 * semi-transparent quads textured with the halves of the other buffer.
 * Equivalent; the original reloads the address of context each time and keeps other values in registers. */
void func_80016D8C(s32 angle, Clut16 *unused) {
    POLY_FT4 *p;
    s32 s;
    s32 c;
    s32 sx;
    s32 cx;
    s32 sy;
    s32 cy;

    s = func_8003C2F0(angle);
    c = func_8003C364(angle);
    sx = (s * 0x8600) >> 20;
    cx = (c * 0x8600) >> 20;
    sy = (s * 0x10C00) >> 20;
    cy = (c * 0x10C00) >> 20;

    p = context.cur->prims_tail;
    context.cur->prims_tail = p + 1;
    setRGB0(p, 128, 128, 128);
    p->tpage = GetTPage(2, 0, context.c2_p->draw.clip.x, 0);
    setXY4(p, 256, 108, 256 - cy, sy + 108, 256 - sx, 108 - cx, 256 - sx - cy, sy - (cx - 108));
    setUV4(p, 255, 119, 0, 119, 255, 0, 0, 0);
    SetPolyFT4(p);
    SetSemiTrans(p, 1);
    AddPrim(&context.cur->ot[0x7FF], p);
    p = context.cur->prims_tail;
    context.cur->prims_tail = p + 1;
    setRGB0(p, 128, 128, 128);
    p->tpage = GetTPage(2, 0, context.c2_p->draw.clip.x, 0);
    setXY4(p, 256, 108, 256 - cy, sy + 108, sx + 256, cx + 108, sx + 256 - cy, sy + (cx + 108));
    setUV4(p, 255, 120, 0, 120, 255, 239, 0, 239);
    SetPolyFT4(p);
    SetSemiTrans(p, 1);
    AddPrim(&context.cur->ot[0x7FF], p);
    p = context.cur->prims_tail;
    context.cur->prims_tail = p + 1;
    setRGB0(p, 128, 128, 128);
    p->tpage = GetTPage(2, 0, context.c2_p->draw.clip.x + 256, 0);
    setXY4(p, 256, 108, cy + 256, 108 - sy, 256 - sx, 108 - cx, cy - (sx - 256), 108 - cx - sy);
    setUV4(p, 0, 119, 255, 119, 0, 0, 255, 0);
    SetPolyFT4(p);
    SetSemiTrans(p, 1);
    AddPrim(&context.cur->ot[0x7FF], p);
    p = context.cur->prims_tail;
    context.cur->prims_tail = p + 1;
    setRGB0(p, 128, 128, 128);
    p->tpage = GetTPage(2, 0, context.c2_p->draw.clip.x + 256, 0);
    setXY4(p, 256, 108, cy + 256, 108 - sy, sx + 256, cx + 108, cy + (sx + 256), cx + 108 - sy);
    setUV4(p, 0, 120, 255, 120, 0, 239, 255, 239);
    SetPolyFT4(p);
    SetSemiTrans(p, 1);
    AddPrim(&context.cur->ot[0x7FF], p);
}
#else
INCLUDE_ASM("asm/us/nonmatchings/sys", func_80016D8C);
#endif

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

#ifdef NON_MATCHING
/* Shrinks the picture behind black borders (t steps) and moves the
 * projection plane (GTE H) with it. Equivalent; the original swaps the registers of t2 and bar, and
 * schedules a few instructions differently because of it. */
void func_80017834(u32 t, u32 t2) {
    POLY_F4 *p;
    u32 bar, x, y;
    s32 left, right, top, bottom, mid, w;
    s32 h;

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
    h = func_80025C0C();
    h -= (h / 24) * t;
    if (D_80065C90.unkC4 != h) {
        D_80065C90.unkC4 = h;
        SetGeomScreen(h);
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

s32 VSync(s32 mode);
void InitCARD(s32 shared);
s32 StartCARD(void);
void _bu_init(void);

/* Opens the memory card events (software and hardware: done, error, timeout,
 * new card) and starts the card driver. */
void CardInit(void) {
    EnterCriticalSection();
    D_80060A68 = OpenEvent(0xF4000001, 0x4, 0x2000, NULL);
    D_80060A6C = OpenEvent(0xF4000001, 0x8000, 0x2000, NULL);
    D_80060A70 = OpenEvent(0xF4000001, 0x100, 0x2000, NULL);
    D_80060A74 = OpenEvent(0xF4000001, 0x2000, 0x2000, NULL);
    D_80060A78 = OpenEvent(0xF0000011, 0x4, 0x2000, NULL);
    D_80060A7C = OpenEvent(0xF0000011, 0x8000, 0x2000, NULL);
    D_80060A80 = OpenEvent(0xF0000011, 0x100, 0x2000, NULL);
    D_80060A84 = OpenEvent(0xF0000011, 0x2000, 0x2000, NULL);
    ExitCriticalSection();
    VSync(5);
    InitCARD(1);
    VSync(5);
    StartCARD();
    VSync(5);
    _bu_init();
    VSync(5);
    EnableEvent(D_80060A68);
    EnableEvent(D_80060A6C);
    EnableEvent(D_80060A70);
    EnableEvent(D_80060A74);
    EnableEvent(D_80060A78);
    EnableEvent(D_80060A7C);
    EnableEvent(D_80060A80);
    EnableEvent(D_80060A84);
}

s32 CardKill(void) {
    StopCARD();
    return 0;
}

extern u8 D_8006B0E8[40][4][9]; /* save data: 9 packed bytes per record */
void func_80071590(s32 arg);
s32 func_800717D8(s32 arg);
s32 func_80071880(s32 arg);
s32 func_800718D0(void);
s32 func_80071930(void);
s32 func_80071974(void);
void func_800719D8(void);
void func_80071ABC(u8 *p, s32 arg);
s32 func_80071C8C(s32 arg);
s32 func_80071D38(s32 arg);
s32 func_80071DA8(s32 arg);
s32 func_80072094(s32 arg);
#ifdef NON_MATCHING
/* Messages of the memory card subsystem; 19 and 20 move a save record between
 * the save data and the GOOL stack of obj. Equivalent; the original keeps
 * arg in another register. */

s32 func_80017CA8(GoolObject *obj, u32 msg, s32 arg) {
    u8 *rec;
    u32 v;

    switch (msg) {
    case 0:
        break;
    case 2:
        func_80071590(0x80);
        return 1;
    case 3:
        return func_800717D8(arg);
    case 5:
        return func_800718D0();
    case 10:
        return func_80071930();
    case 12:
        return func_80071974();
    case 13:
        return func_80071C8C(arg);
    case 14:
        return func_80072094(arg);
    case 15:
        if (D_80060908 == 0) {
            break;
        }
        func_80071ABC((u8 *)D_80060908 + (arg * 0x58C + 0x6F4), 0);
        func_800719D8();
        return 0;
    case 16:
        return func_80071D38(arg);
    case 17:
        return func_80071DA8(arg);
    case 18:
        return func_80071880(arg);
    case 19:
        if ((u32)(arg >> 8) < 40) {
            rec = D_8006B0E8[arg >> 8][arg & 0xFF];
            *obj->sp++ = rec[7] | (rec[8] << 8);
            *obj->sp++ = rec[3] | (rec[4] << 8) | (rec[5] << 16) | (rec[6] << 24);
            *obj->sp++ = rec[0] | (rec[1] << 8) | (rec[2] << 16);
        } else {
            *obj->sp++ = 0;
            *obj->sp++ = 0;
            *obj->sp++ = 0;
        }
        return obj->sp[-1];
    case 20:
        if ((u32)(arg >> 8) < 40) {
            rec = D_8006B0E8[arg >> 8][arg & 0xFF];
            v = *--obj->sp;
            rec[1] = v >> 8;
            rec[0] = v;
            rec[2] = v >> 16;
            v = *--obj->sp;
            rec[4] = v >> 8;
            rec[5] = v >> 16;
            rec[3] = v;
            rec[6] = v >> 24;
            v = *--obj->sp;
            rec[7] = v;
            rec[8] = v >> 8;
        } else {
            obj->sp -= 3;
        }
        return 0;
    }
    return 1;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/sys", func_80017CA8);
#endif

/* Advances the primitive memory of the current buffer by size bytes. */

/* Takes size bytes of primitive memory from the current buffer. */

void *func_80017F98(s32 size) {
    u8 *p = context.cur->prims_tail;

    context.cur->prims_tail = p + size;
    return p;
}

/* Takes size bytes from the second list of the current buffer and links them
 * in front of it. */

void *func_80017FB8(s32 size) {
    void **p = context.cur->unk2098;

    context.cur->unk2098 = (u8 *)p + size;
    *p = (void *)context.cur->unk2090;
    context.cur->unk2090 = (s32)p;
    return p;
}

void **GpuGetPrimsTail(void) {
    return &context.cur->prims_tail;
}
