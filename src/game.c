#include "common.h"
#include "gool.h"
#include "game.h"
#include "ns.h"

/* The state of the game: a new game, the objects of a level, the spawn
 * points. */

/* .sdata */
char D_800609CC[] = "pb??B";

INCLUDE_ASM("asm/us/nonmatchings/game", func_8002FA1C);

void *memset(void *dst, s32 c, s32 n);
extern u8 D_8006B0E8[40][4][9]; /* save data (sys.c) */
extern u16 D_8006AEE8[]; /* removed objects of the level (interp.c) */
extern u32 D_8006C2D8[64][2]; /* per level: 16 4-bit counters */
void func_8002FCA0(s32 mode);
/* Starts a new game. */

void func_8002FBD0(void) {
    game.unkB0 = 99;
    game.unkAC = -1;
    game.unkA8 = 7;
    game.unk170 = 0;
    game.unk214 = 0;
    game.unk1DC = 42;
    game.unk1E0 = 0;
    game.unk2D8 = 0;
    game.unk354 = 0;
    game.unk35C = -1;
    game.unk2BC = 0;
    memset(D_8006B0E8, 0, 0x2D0);
    game.game_state = 0;
    func_8002FCA0(1);
}

void func_8002FC50(void) {
    game.unkE8 = 0x100;
    game.unkEC = 0xC0;
    game.unkDC = 0x400;
    game.unk2C0 = 0x80;
    game.unk2C4 = 0x80;
    game.unkE0 = 0;
    game.unkE4 = 0;
    game.unkB4 = 0;
    game.unk2C8 = 0;
    game.unk2CC = 0;
    game.unk2D0 = 0xFF;
    game.unk2D4 = 0xFF;
}

#ifdef NON_MATCHING
/* Resets the state of a level (mode != 0: also of the player; mode 1: and
 * the removed objects).
 * Equivalent; the original keeps the constants in other registers. */
void func_8002FCA0(s32 mode) {
    if (mode != 0) {
        func_8002FC50();
        game.unkB0 = 99;
        game.unk1C4 = 99;
        game.unk26C = 0;
        game.unk210 = 0;
        game.unk74 = 0;
        game.unkF0 = 0;
        game.unk230 = 0;
        game.unkC4 = 0;
        game.unkC8 = 0;
        game.unkCC = 0;
        game.unkD0 = 0;
        game.unk118 = 0;
        game.unk11C = 0;
        game.unk294 = 0;
        game.unk298 = 0;
        game.unk31C = 0;
        game.unk320 = 0;
        game.unk324 = 0;
        game.unk328 = 0;
        game.unk2B8 = 0;
        game.unk2E0 = 0;
        game.unk140 = 0;
        game.unk144 = 0;
        game.unk120 = 0;
        game.unk124 = 0;
        game.unk32C = 0;
        game.unk330 = 0;
        game.unk334 = 0;
        game.unk338 = 0;
        game.unk174 = -1;
        game.unk1C8 = 0;
        game.unkC0 = game.unkDC;
        game.unk88 = 0;
        game.unk15C = 0;
        game.unk160 = 0;
        game.unk178 = 0;
        game.unk17C = 0;
        game.unk2A4 = -1;
        game.unk2A8 = 0;
        if (mode != 2) {
            memset(D_8006C2D8, 0, sizeof(D_8006C2D8));
        }
        game.unk244 = 0;
        game.unk248 = 0;
        game.unk24C = 0;
        game.unkBC = 0;
    }
    memset(D_8006AEE8, 0, 0x200);
}
#else
INCLUDE_ASM("asm/us/nonmatchings/game", func_8002FCA0);
#endif

extern RenderState D_800676B0;
extern s32 D_80060920;
void func_8002AA94(GoolObject *obj, s32 *delta, void *q);
/* Resets the state of the player for a new life in the level (with full,
 * also the counters of the level). */

extern s32 D_80068E40[];

void func_8002FDC4(s32 full) {
    s32 i;

    game.unk184 = 0;
    game.unk158 = 0;
    if (game.unk204 != 2) {
        game.unk190 = 0;
    }
    game.unk188 = 1;
    game.unk258 = 0;
    game.unk280 = 0;
    game.unk314 = 0;
    if (full) {
        game.unk174 = -1;
        game.unk28C = 0;
        game.unk290 = 0;
        game.unk74 = 0;
        game.unkF0 = 0;
        game.unkA0 = 0;
        game.unk230 = 0;
        game.unk210 = 0;
        game.unk2A8 = (D_8006C2D8[ns.ldat->lid][0] & 0xF) << 8;
        if (game.unk2A8 < 0) {
            game.unk2A8 = 0;
        }
        game.unk2A4 = 0;
        game.unk308 = 0;
        game.unk1E4 = 0;
        game.unk218 = 0;
        game.unk22C = NULL;
        game.unk348 = NULL;
    }
    func_8002AA94(NULL, NULL, NULL);
    D_80068E40[0] = 0;
    *(s32 *)((u8 *)&game + 0x20C) = 0x20; /* game.unk20C, written so it matches */
    if (game.game_state != 0x600) {
        game.unk208 = 0x120;
    }
    D_80060920 = 0;
    if ((game.display_flags & 0x20000001) && (D_800676B0.unk378 & 0x1000000)
        && ns.ldat->lid != 0xE && ns.ldat->lid != 0x1C) {
        for (i = 63; i >= 0; i--) {
            D_800676B0.unk140[i] = 0;
        }
        D_800676B0.unk138 = 0;
    }
}

extern GoolObject *crash;
extern u32 spawns[0x300];
extern s32 D_80068FF0;
s32 func_80032DF8(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g);
void func_80014650(u32 *bits);
/* Saves the state of the level in save at a checkpoint touched by obj
 * (mode 0); mode 2 only adds the spawn flags cleared since save[1]. */

void func_8002FF3C(GoolObject *obj, SaveState *save, s32 a, s32 mode) {
    s32 copy;
    s32 idx;
    u32 i;

    if (((ZoneHdr *)((Entry *)D_80065C90.unkCC)->items[0])->unk2DC & 0x2000) {
        return;
    }
    copy = save == &D_80065C90.saves[1];
    if (mode == 0) {
        GoolSendEvent(obj, crash, 0x4400, 0, NULL);
        if (a) {
            game.unkF0 = 0;
            game.unk158 = 0;
        } else {
            game.unkF0 += 0x100;
        }
        save->unk34 = a;
        save->xform = crash->xform;
        if ((u8)obj->unk120 - 6 >= 2u) {
            *(Vec3 *)save->xform.trans = *(Vec3 *)obj->xform.trans;
        }
        if (game.unk174 >= 0) {
            if (game.unk174 != 0) {
                *(Vec3 *)save->xform.trans = *(Vec3 *)&game.unk1F8;
            }
        }
        save->xform.rot[0] = 0;
        save->xform.rot[1] = 0;
        save->xform.rot[2] = 0;
        save->zone = ((Entry *)D_80065C90.unkCC)->eid;
        save->progress = D_80065C90.unkE0;
        save->lid = ns.ldat->lid;
        save->unk28 = func_80032DF8(D_80065C90.unkD4, 0x173, 0, 0, 0, 2, 0);
        if (game.unk174 >= -1 && obj->unkF0 != 0 && !copy) {
            idx = func_80032DF8(obj->unkF0, 0x288, 0, 0, 0, 2, 0);
            save->unk38 = idx;
            if (idx != 0) {
                game.unk2A8 = ((D_8006C2D8[save->lid][idx / 8] >> (idx % 8 * 4)) & 0xF) << 8;
                if (game.unk2A8 < 0) {
                    game.unk2A8 = 0;
                }
                game.unk2A4 = save->unk38;
            }
        } else {
            save->unk38 = -1;
        }
        func_80014650(save->pages);
    }
    if (mode == 2) {
        save->unkC48 += game.unk158;
        for (i = 0; i < 0x300; i++) {
            save->spawns[i] |= ~save[1].spawns[i] & spawns[i];
        }
    } else {
        save->unkC48 = game.unk158;
        save->unk3C = game.unk308;
    }
    if (mode != 2) {
        for (i = 0; i < 0x300; i++) {
            save->spawns[i] = spawns[i];
        }
    }
}

/* Counter idx (0..15) of level: with !a and !b it is raised by one (up to 15);
 * game.unk2A4 and game.unk2A8 (its value << 8) then tell it. */

void func_800302AC(s32 level, s32 idx, s32 a, s32 b) {
    if (idx >= 0 && a == 0) {
        if (((D_8006C2D8[level][idx >> 3] >> ((idx - (idx >> 3) * 8) * 4)) & 0xF) < 15 && b == 0) {
            D_8006C2D8[level][idx >> 3] += 1 << ((idx - (idx >> 3) * 8) * 4);
        }
        game.unk2A8 = ((D_8006C2D8[level][idx / 8] >> (idx % 8 * 4)) & 0xF) << 8;
        if (game.unk2A8 < 0) {
            game.unk2A8 = 0;
        }
        game.unk2A4 = idx;
    } else {
        game.unk2A8 = 0;
        game.unk2A4 = 0;
    }
}

INCLUDE_ASM("asm/us/nonmatchings/game", func_80030388);

INCLUDE_ASM("asm/us/nonmatchings/game", func_80030780);

INCLUDE_ASM("asm/us/nonmatchings/game", func_80030794);

INCLUDE_ASM("asm/us/nonmatchings/game", func_8003079C);

INCLUDE_ASM("asm/us/nonmatchings/game", func_800307EC);

INCLUDE_ASM("asm/us/nonmatchings/game", func_800307F4);

INCLUDE_ASM("asm/us/nonmatchings/game", func_800308B8);

INCLUDE_ASM("asm/us/nonmatchings/game", func_80030940);
