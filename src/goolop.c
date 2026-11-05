#include "common.h"
#include "gool.h"
#include "game.h"
#include "ns.h"

/* The GOOL instructions and the bytecode interpreter (operands, the
 * instructions that read the pad, play sounds, send events..., the loop). */

/* .rodata: messages of the debug version, no longer printed */
const char D_80010660[] = "----stack pop\n";
const char D_80010670[] = "----stack push\n";
const char D_80010680[] = "-----ireg ref\n";
const char D_80010690[] = "-----null ref\n";
const char D_800106A0[] = "-----sp-double ref\n";
const char D_800106B4[] = "-----stack ref\n";
const char D_800106C4[] = "-----var ref\n";
const char D_800106D4[] = "-----frac ref\n";
const char D_800106E4[] = "-----int ref\n";
const char D_800106F4[] = "-----pool ref\n";
const char D_80010704[] = "-----reg ref\n";
/* (an empty string follows at 0x8001073C) */
const char D_80010714[0x29] = "******************** state return %d\n";

/* .sdata */
s32 D_80060A20 = 0; /* which of the two scratchpad words holds the last constant */
s32 D_80060A24 = 0x1F800040; /* the two words */
s32 D_80060A28 = 0;
s32 D_80060A2C = 12345; /* random seeds (set by game.c) */
s32 D_80060A30 = 765634;
s32 D_80060A34 = 0;

#ifdef NON_MATCHING
/* The value that GOOL operand op of obj refers to: a field of a linked
 * object, a constant (put in one of two words of the scratchpad), a word of
 * the stack frame, or a word of the code (global or of the state).
 * Equivalent; the original shares the NULL return and keeps the code word in v0. */
u32 *func_80035410(GoolObject *obj, u32 op) {
    u32 *buf = (u32 *)0x1F800040;
    GoolObject *link;
    s32 unused[4]; /* takes stack space in the original */

    if (op & 0x800) {
        if (op & 0x400) {
            link = *(GoolObject **)((u8 *)obj + ((op >> 4) & 0x1C) + 0x40);
            if (link == NULL) {
                return NULL;
            }
            return (u32 *)&link->self + (op & 0x3F);
        }
        if (op & 0x200) {
            if (op & 0x100) {
                if (op & 0x80) {
                    if (op == 0xBE0) {
                        return NULL;
                    }
                    if (op == 0xBF0) {
                        return (u32 *)1;
                    }
                } else {
                    return obj->fp + ((s32)(op << 25) >> 25);
                }
            } else {
                D_80060A20 = D_80060A20 == 0;
                buf[D_80060A20] = (s32)(op << 24) >> 20;
                return &buf[D_80060A20];
            }
        } else {
            D_80060A20 = D_80060A20 == 0;
            buf[D_80060A20] = (s32)(op << 23) >> 15;
            return &buf[D_80060A20];
        }
    } else if (op & 0x400) {
        return &((u32 *)obj->external->items[2])[op & 0x3FF];
    } else {
        return &((u32 *)obj->unk10->items[2])[op & 0x3FF];
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80035410);
#endif

#ifdef NON_MATCHING
/* The word that GOOL operand op of obj refers to, as func_80035410, with the
 * fields of obj itself and its stack (0x1F: pops, or with push pushes).
 * Equivalent; the original shares the NULL return and allocates the registers differently. */
u32 *func_80035538(GoolObject *obj, u32 op, s32 push) {
    GoolObject *link;
    u32 *p;
    u32 idx;
    s32 unused[4]; /* takes stack space in the original */

    if (op & 0x800) {
        if (op & 0x400) {
            if (op & 0x200) {
                idx = op & 0x1FF;
                if (idx == 0x1F) {
                    if (!push) {
                        p = --obj->sp;
                    } else {
                        p = obj->sp++;
                    }
                } else {
                    p = (u32 *)&obj->self + idx;
                }
                return p;
            }
            link = *(GoolObject **)((u8 *)obj + ((op >> 4) & 0x1C) + 0x40);
            if (link == NULL) {
                return NULL;
            }
            return (u32 *)&link->self + (op & 0x3F);
        }
        if (op & 0x200) {
            if (op & 0x100) {
                if (op & 0x80) {
                    if (op == 0xBE0) {
                        return NULL;
                    }
                    if (op == 0xBF0) {
                        return (u32 *)1;
                    }
                } else {
                    return obj->fp + ((s32)(op << 25) >> 25);
                }
            } else {
                D_80060A28 = D_80060A28 == 0;
                ((u32 *)D_80060A24)[D_80060A28] = (s32)(op << 24) >> 20;
                return &((u32 *)D_80060A24)[D_80060A28];
            }
        } else {
            D_80060A28 = D_80060A28 == 0;
            ((u32 *)D_80060A24)[D_80060A28] = (s32)(op << 23) >> 15;
            return &((u32 *)D_80060A24)[D_80060A28];
        }
    } else if (op & 0x400) {
        return &((u32 *)obj->external->items[2])[op & 0x3FF];
    } else {
        return &((u32 *)obj->unk10->items[2])[op & 0x3FF];
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80035538);
#endif

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800356B8);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80035ABC);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80035AC4);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80035BE0);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80035D08);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80036B88);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80037300);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800376F8);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_8003780C);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800378F8);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80037A00);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80037AE4);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80037BEC);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80037CD8);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80037DAC);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80037EDC);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_8003808C);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80038248);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800383DC);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800385C0);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_8003864C);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800386BC);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800387A8);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800388C8);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800389C8);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80038B2C);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80038B40);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80038C24);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80038C94);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80038D88);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80038DB0);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80038DD8);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80038E28);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80039494);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_8003952C);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800395E0);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800397CC);
