#include "common.h"
#include "gool.h"
#include "game.h"
#include "ns.h"

/* The GOOL instructions (operands, the instructions that read the pad,
 * play sounds, send events, change states...). The bytecode interpreter
 * itself (func_80038E28 and its helpers) is hand-written assembly: gool_asm
 * in splat.yaml. */

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

u32 *func_80035410(GoolObject *obj, u32 op);
/* The word operand x of obj refers to: 0xE1F pops the stack, 0xE00.. a
 * field of obj, else func_80035410. */
static __inline__ u32 *GoolRef(GoolObject *obj, u32 x) {
    if ((x & 0xE00) == 0xE00) {
        if (x == 0xE1F) {
            return --obj->sp;
        }
        return (u32 *)&obj->self + (x & 0x1FF);
    }
    return func_80035410(obj, x);
}
/* The word register r of obj refers to: 0x1F pops the stack. */
static __inline__ u32 *GoolReg(GoolObject *obj, u32 r) {
    if (r == 0x1F) {
        return --obj->sp;
    }
    return (u32 *)&obj->self + r;
}

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

void func_80035ABC(void) {
}

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80035AC4);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80035BE0);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80035D08);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80036B88);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80037300);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800376F8);

s32 func_80033864(GoolObject *obj, u32 ref, s32 vol);
/* Instruction: plays a sound (entry and volume). */

void func_8003780C(GoolObject *obj, u32 op) {
    u32 *b = GoolRef(obj, (op >> 12) & 0xFFF);
    u32 *a = GoolRef(obj, op & 0xFFF);

    obj->voice_id = func_80033864(obj, (u32)a, *(s16 *)b);
}

s32 func_80033C54(s32 handle, u32 flags, u32 *arg, GoolObject *obj); /* audio.c */
/* Instruction: a command for the voice of obj (unkD4 gets the result). */

void func_800378F8(GoolObject *obj, u32 op) {
    u32 *a = GoolRef(obj, op & 0xFFF);
    u32 flags = (op >> 20) & 0xF;
    u32 r = (op >> 12) & 0x3F;
    s32 v;

    if (r == 0) {
        v = 0;
    } else {
        v = *(r == 0x1F ? --obj->sp : (u32 *)&obj->self + r);
    }
    if (flags == 0xF) {
        flags = 0x80000000;
    }
    if (op & 0x40000) {
        flags |= 0x40000000;
    }
    if (op & 0x80000) {
        flags |= 0x20000000;
    }
    obj->unkD4 = func_80033C54(v, flags, a, obj);
}

void func_80027D6C(s32 type, u32 *args); /* camera.c: a camera event */
/* Instruction: sends a camera event. */

void func_80037A00(GoolObject *obj, u32 op) {
    u32 *b = GoolRef(obj, (op >> 12) & 0xFFF);
    u32 *a = GoolRef(obj, op & 0xFFF);

    func_80027D6C(*a, b);
}

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80037AE4);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80037BEC);

/* Instruction: sets the animation of obj (op & 0x7F: frame, (op >> 7) &
 * 0x1FF: offset in its code), then as func_80037DAC. */

s32 func_80037CD8(GoolObject *obj, u32 op, u32 flags) {
    u32 *code = (u32 *)obj->global->items[5];
    u32 *sp = obj->sp;

    obj->unkEC = (op & 0x7F) << 8;
    obj->unkE8 = (s32)(code + ((op >> 7) & 0x1FF));
    *sp++ = (((op >> 16) & 0x3F) << 24) | game.frames;
    obj->sp = sp;
    switch ((op >> 22) & 3) {
    case 0:
        if (obj->xform.scale[0] < 0) {
            obj->xform.scale[0] = -obj->xform.scale[0];
        }
        break;
    case 1:
        if (obj->xform.scale[0] > 0) {
            obj->xform.scale[0] = -obj->xform.scale[0];
        }
        break;
    case 2:
        obj->xform.scale[0] = -obj->xform.scale[0];
        break;
    }
    if (flags & 4) {
        return -0xFF;
    }
    return 0;
}

/* Instruction: sets the frame of the animation of obj (from an operand),
 * waits (op >> 16) & 0x3F frames and flips obj (op >> 22: 0 to the right,
 * 1 to the left, 2 the other way). */

s32 func_80037DAC(GoolObject *obj, u32 op, u32 flags) {
    obj->unkEC = *GoolRef(obj, op & 0xFFF);
    *obj->sp++ = (((op >> 16) & 0x3F) << 24) | game.frames;
    switch ((op >> 22) & 3) {
    case 0:
        if (obj->xform.scale[0] < 0) {
            obj->xform.scale[0] = -obj->xform.scale[0];
        }
        break;
    case 1:
        if (obj->xform.scale[0] > 0) {
            obj->xform.scale[0] = -obj->xform.scale[0];
        }
        break;
    case 2:
        obj->xform.scale[0] = -obj->xform.scale[0];
        break;
    }
    if (flags & 4) {
        return -0xFF;
    }
    return 0;
}

/* Instruction: if a condition (register (op >> 12) & 0x3F) holds, sends the
 * event *operand to the object linked at (op >> 21) & 7 with the top
 * (op >> 18) & 7 words of the stack as arguments. */

s32 func_80037EDC(GoolObject *obj, u32 op, u32 flags) {
    u32 argv[16];
    GoolObject *to;
    u32 *ev;
    s32 n;
    s32 i;

    to = *(GoolObject **)((u8 *)obj + ((op >> 19) & 0x1C) + 0x40);
    ev = GoolRef(obj, op & 0xFFF);
    obj->status_a &= ~0x20000;
    if (*GoolReg(obj, (op >> 12) & 0x3F) != 0 && to != NULL && ev != NULL) {
        n = (op >> 18) & 7;
        for (i = 0; i < n; i++) {
            argv[i] = obj->sp[i - n];
        }
        GoolSendEvent(obj, to, *ev, n, argv);
    } else {
        obj->unkD4 = 0;
    }
    if (!(obj->status_a & 0x20000)) {
        obj->sp -= (op >> 18) & 7;
    } else {
        if (flags & 2) {
            return -0xFF;
        }
        obj->sp--;
    }
    return 0;
}

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_8003808C);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80038248);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800383DC);

s32 func_8006F280(u32 *p); /* overlay */
/* Instruction: pushes the result of an overlay function on a word. */

void func_800385C0(GoolObject *obj, u32 op) {
    *obj->sp++ = func_8006F280(GoolRef(obj, op & 0xFFF));
}

void func_8006F514(u32 v); /* overlay */
/* Instruction: passes a word to an overlay function. */

void func_8003864C(GoolObject *obj, u32 op) {
    func_8006F514(*GoolRef(obj, op & 0xFFF));
}

s32 func_8006ECF4(u32 a, u32 b); /* overlay */
/* Instruction: unkD4 = an overlay function of two words. */

void func_800386BC(GoolObject *obj, u32 op) {
    u32 *b = GoolRef(obj, (op >> 12) & 0xFFF);
    u32 *a = GoolRef(obj, op & 0xFFF);

    obj->unkD4 = func_8006ECF4(*a, *b);
}

void func_8006F21C(u32 a, u32 *b, u32 c, u32 d); /* overlay */
/* Instruction: an overlay function of two words and two popped values (one
 * an offset in the code of obj). */

void func_800387A8(GoolObject *obj, u32 op) {
    u32 *b = GoolRef(obj, (op >> 12) & 0xFFF);
    u32 *a = GoolRef(obj, op & 0xFFF);
    u32 *code = (u32 *)obj->global->items[5] + (*--obj->sp >> 8);
    u32 v = *--obj->sp;

    func_8006F21C(v, code, *a, *b);
}

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800388C8);

INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800389C8);

/* x with its low bits sign-extended. */

s32 func_80038B2C(s32 x, s32 bits) {
    return (x << (32 - bits)) >> (32 - bits);
}
