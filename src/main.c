#include "common.h"
#include "gfx.h"

/* Names follow the Crash 1 port "c1" where the code is clearly the same.
 * Structures only list the fields used so far, at their offsets. */

typedef struct {
    u8 unk0[0x4];
    s8 unk4;
    u8 unk5[0x23];
    u32 tapped; /* 0x28: buttons pressed this frame */
} Pad;

typedef struct {
    u8 unk0[0x44];
    s32 unk44;
} Object;

typedef struct {
    u8 unk0[0x40];
    u8 handles[0x34]; /* 0x40 */
    s32 unk74;
    u8 unk78[0xC];
    u32 display_flags; /* 0x84 */
    s32 unk88;
    u8 unk8C[0x4];
    s32 pause_obj; /* 0x90: a pointer or an error code */
    u8 unk94[0x10];
    s32 game_state; /* 0xA4 */
    u8 unkA8[0xE0];
    s32 unk188;
    u8 unk18C[0x10];
    s32 unk19C;
    u8 unk1A0[0x64];
    s32 unk204;
    s32 unk208;
    u8 unk20C[0x20];
    Object *unk22C;
    u8 unk230[0x3C];
    s32 unk26C;
    u8 unk270[0xD4];
    s32 unk344;
    Object *unk348;
    u8 unk34C[0x10];
    s32 unk35C;
    u8 unk360[0x4];
    s32 unk364;
} GameState;

typedef struct {
    u8 unk0[0x8];
    s32 lid; /* 0x8: level id */
} LevelData;

typedef struct {
    u8 unk0[0x14];
    LevelData *ldat; /* 0x14 */
    u8 unk18[0xC];
    s32 draw_skip_counter; /* 0x24 */
} NsState;


typedef struct ObjectLink {
    struct ObjectLink *next;
    struct GoolObject *obj;
} ObjectLink;

typedef struct GoolObject {
    u8 unk0[0xAC];
    u32 unkAC;
    u8 unkB0[0x38];
    s32 unkE8;
    u32 unkEC;
} GoolObject;


typedef struct {
    u8 unk0[0x28];
    void *unk28;
    u8 unk2C[0x5C];
    s32 unk88;
    u8 unk8C[0xC];
    s32 unk98;
    u8 unk9C[0x2DC];
    u32 unk378;
} RenderState;

typedef struct {
    u8 unk0[0x10];
    s32 *unk10;
} UnkCC;

typedef struct {
    u8 unk0[0x20];
    u16 unk20[9];
    u8 unk32[0x9A];
    UnkCC *unkCC;
    u8 unkD0[0x74];
    u32 unk144;
} Unk80065C90;

typedef struct {
    u8 unk0[0x10];
    s32 *unk10;
} Unk80065D5C;

typedef struct {
    u16 unk0;
    u8 unk2[0x2];
    s32 unk4;
    s32 unk8;
    s32 *unkC;
    s32 *unk10;
    s32 unk14;
    s32 *unk18;
    s32 unk1C;
    s32 unk20;
    s32 unk24;
    u8 unk28[0x8];
} Unk30;

typedef struct {
    u8 unk0[0x30];
    s32 lid; /* 0x30 */
} SaveState;

/* .sdata */
s32 D_80060878 = 0;
s32 paused = 0;
/* Read by the PSY-Q startup code: stack size and amount of RAM. */
s32 _stacksize = 0x1000;
s32 _ramsize = 0x200000;
s32 pause_status = 0;
#ifndef NON_MATCHING
/* The "dead\n" literal of CoreLoop(), which is still assembly in matching builds. */
char D_8006088C[] = "dead\n";
#endif

/* .sbss */
s32 pause_stamp;
s32 pause_draw_stamp;

extern s32 use_cd;
extern s32 done;
extern s32 next_lid;
extern s32 bonus_return;
extern s32 ticks_elapsed; /* lives inside Naughty Dog's assembly */
extern void *D_80060A90;
extern u32 D_80060A64;
extern s32 D_80060A94;
extern s32 D_80060A98;
extern s32 D_80060A9C;
extern s32 D_80060AA4;
extern u8 D_80041550[];
extern Pad pads;
extern NsState ns;
extern GameState game;
extern RenderState D_800676B0;
extern Unk80065C90 D_80065C90;
extern u16 D_80065CB0[];
extern Unk80065D5C *D_80065D5C;
extern SaveState savestate;

void func_800154AC(void);
void CoreLoop(s32 lid);
void func_80015658(void);
void func_80011FB8(void);
void func_80011FC0(void);
void NSUpdate(s32);
void NSInit(NsState *, s32);
void NSKill(NsState *);
void func_80016634(s32);
void func_80018A54(void);
s32 func_8001A42C(void *, s32, s32, s32, s32, s32);
void func_8001C824(GoolObject *, s32, u32, void *, s32);
void func_8001CD80(Object *, s32, s32, s32);
void func_8001D16C(s32);
void func_8001DB38(s32, s32, s32, s32, s32 *);
void func_8001E34C(s32, s32, s32, s32, s32);
void func_80028A78(void);
void func_8002FA1C(void);
void func_8002FBD0(void);
void func_80030388(SaveState *);
void func_800308B8(u32 *);
void func_8003AC44(void *, s32);
void func_8003F3AC(s32, DrawContext *, RenderState *);
void func_80040ED8(s32, DrawContext *, RenderState *, s32);
void func_80043050(s32, DrawContext *, RenderState *);
void func_80043310(s32, DrawContext *, RenderState *);
void func_800435DC(DrawContext *, RenderState *);
void func_80043FFC(s32, DrawContext *, RenderState *);
void func_800452A0(s32, DrawContext *, RenderState *);
void func_8004F674(u16 *);
void func_8005D370(s32, DrawContext *, RenderState *);
int printf(const char *fmt, ...);

/* GCC inserts the call to __main() (global constructors) by itself. */
s32 main(void) {
    use_cd = 1;
    func_800154AC();
    CoreLoop(60);
    func_80015658();
    return 0;
}

#ifdef NON_MATCHING
/* Equivalent to the original and close to matching (a few register choices
 * and the order of some loads still differ). */
void CoreLoop(s32 lid) {
    u8 unused[0xA8];
    s32 saved[8];
    s32 arg;
    s32 new_lid;
    s32 bonus_return2;
    s32 draw;
    s32 i;

    func_8002FBD0();
    NSInit(&ns, lid);
    func_8002FA1C();
    do {
        if (((pads.tapped & 0x800) || (paused == 1 && game.pause_obj == 0) ||
             (paused == 0 && pads.unk4 != 0)) &&
            ((game.unk204 == 0 && (game.unk208 == 0 || (game.display_flags & 0x800000)) &&
              game.unk188 != 0) ||
             paused != 0)) {
            if (paused = !paused) {
                if (game.pause_obj == 0) {
                    game.pause_obj = func_8001A42C(game.handles, 4, 4, 0, 0, 1);
                    if (game.pause_obj >= -0xFE) {
                        paused = 0;
                        game.pause_obj = 0;
                        pause_status = 0;
                    } else {
                        pause_status = 1;
                        pause_stamp = ticks_elapsed;
                        pause_draw_stamp = context.c2_p->draw_stamp;
                    }
                }
                func_80018A54();
            } else if (game.pause_obj != 0) {
                arg = 1;
                func_8001DB38(0, game.pause_obj, 0xC00, 1, &arg);
                paused = 1;
            } else {
                pause_status = -1;
                ticks_elapsed = pause_stamp;
                context.c2_p->draw_stamp = pause_draw_stamp;
            }
        } else {
            pause_status = 0;
        }
        func_80011FB8();
        if (D_80060A90 != NULL && D_80060A64 != 0x6396347F) {
            func_800308B8(&D_80060A64);
        }
        bonus_return2 = 0;
        if (next_lid == -1 && ns.ldat->lid != 2 &&
            (game.game_state == 0x200 || game.game_state == 0x300 || game.game_state == 0x400)) {
            printf("dead\n");
            next_lid = 0x3B;
            game.unk26C = 0;
        }
        if (next_lid != -1) {
            new_lid = next_lid;
            func_8001E34C(0, 0x2900, 0, 0, 0);
            if (next_lid == -2) {
                new_lid = savestate.lid;
                bonus_return2 = 1;
                bonus_return = 1;
            } else {
                bonus_return = 0;
            }
            ns.draw_skip_counter = 2;
            NSKill(&ns);
            paused = 0;
            D_80060A94 = 0;
            if (new_lid == 2 || new_lid == 0x3B || new_lid == 0x3C || new_lid == 0x3A) {
                game.unk88 = game.game_state;
                game.unk344 = game.unk74;
            }
            if (new_lid != 0x3A) {
                game.unk35C = -1;
            }
            NSInit(&ns, new_lid);
            func_8002FA1C();
            if (bonus_return2) {
                next_lid = -1;
                func_80030388(&savestate);
            }
            bonus_return = 0;
        }
        NSUpdate(-1);
        if (!(game.display_flags & 0x40000000)) {
            if (!paused) {
                func_80028A78();
            }
            func_80018A54();
        } else {
            if (!(game.display_flags & 1)) {
                func_8003AC44(context.c1_p->ot, 0x800);
                context.c1_p->unkC = 1;
            }
            func_8001D16C(!paused);
        }
        draw = (game.display_flags & 1) && *D_80065C90.unkCC->unk10 != 0 && D_80065C90.unk144 != 0;
        if (draw) {
            u32 *dst;
            s32 *src;
            s32 count;
            Unk30 *entry;

            func_8004F674(D_80065CB0);
            for (i = 0; i < 9; i++) {
                D_80065CB0[i] &= ~3;
            }
            src = D_80065D5C->unk10;
            count = *src;
            dst = (u32 *)0x1F800280;
            entry = (Unk30 *)(src + 2);
            while (count != 0) {
                count--;
                *dst++ = ((entry->unk4 << 16) & 0xFFFE0000) | entry->unk0;
                *dst++ = entry->unk8;
                *dst++ = (s32)(entry->unk10 + entry->unkC[5]);
                *dst++ = entry->unk14;
                *dst++ = (s32)(entry->unk18 + entry->unkC[4]);
                *dst++ = entry->unk1C;
                *dst++ = entry->unk20;
                *dst++ = entry->unk24;
                entry++;
            }
            if (D_800676B0.unk28 != D_80041550) {
                func_8003AC44(context.c1_p->ot, 0x800);
            }
            func_80040ED8(D_80060AA4, context.c1_p, &D_800676B0, game.unk19C);
            context.c1_p->unkC = 1;
        } else {
            func_8004F674(D_80065CB0);
            if (!(game.display_flags & 0x40000000)) {
                func_8003AC44(context.c1_p->ot, 0x800);
                context.c1_p->unkC = 1;
            }
        }
        if (game.display_flags & 0x20000001) {
            if (D_800676B0.unk88 != 0 && (D_800676B0.unk378 & 0x10)) {
                func_80043050(D_800676B0.unk88, context.c1_p, &D_800676B0);
            }
            if (D_800676B0.unk98 != 0 && (D_800676B0.unk378 & 0x400000)) {
                func_80043310(D_800676B0.unk98, context.c1_p, &D_800676B0);
            }
            if (D_800676B0.unk378 & 0x1000000) {
                func_80043FFC(game.unk19C, context.c1_p, &D_800676B0);
                if (ns.ldat->lid != 0xE && ns.ldat->lid != 0x1C) {
                    func_800452A0(game.unk19C, context.c1_p, &D_800676B0);
                    if ((*(u32 *)((u8 *)D_80060A90 + 0x100) >> 28) == 0xA) {
                        func_8005D370(game.unk19C, context.c1_p, &D_800676B0);
                    }
                }
            }
            if (D_800676B0.unk378 & 0x10000000) {
                func_800435DC(context.c1_p, &D_800676B0);
            }
        }
        if (!(game.display_flags & 0x40000000)) {
            func_8001D16C(!paused);
        }
        func_80011FC0();
        for (i = 0; i < 8; i++) {
            saved[i] = ((s32 *)0x1F800040)[i];
        }
        func_8003F3AC(context.c1_p->unk2090, context.c1_p, &D_800676B0);
        if (game.unk364 == -1) {
            if (game.unk22C != NULL) {
                func_8001CD80(game.unk22C, game.unk22C->unk44, D_80060A98, 0);
            }
            if (game.unk348 != NULL) {
                func_8001CD80(game.unk348, game.unk348->unk44, D_80060A9C, 1);
            }
        }
        D_80060A9C = 0;
        D_80060A98 = 0;
        if (paused) {
            func_80018A54();
        }
        for (i = 0; i < 8; i++) {
            ((s32 *)0x1F800040)[i] = saved[i];
        }
        {
            ObjectLink *link;

            for (link = context.c1_p->unk209C; link != NULL; link = link->next) {
                GoolObject *obj = link->obj;
                s32 dir = 1;

                if (obj->unkAC & 0x200000) {
                    dir = -1;
                }
                if (obj->unkAC & 0x400) {
                    dir = 0;
                }
                func_8001C824(obj, obj->unkE8, obj->unkEC >> 8, context.c1_p->ot, dir);
            }
        }
        func_80016634(0);
    } while (!done);
    game.display_flags = 0;
    func_80016634(0);
    func_80016634(0);
    NSKill(&ns);
}
#else
INCLUDE_ASM("asm/us/nonmatchings/main", CoreLoop);
#endif
