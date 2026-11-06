#include "common.h"
#include "gool.h"
#include "game.h"
#include "ns.h"
#include "gfx.h"
#include "gte_inline.h"

/* The GOOL bytecode interpreter (func_80038E28 and its helpers) in C.
 *
 * On the PlayStation this code is hand-written assembly (the gool_asm
 * segment, 0x80038B40-0x80039E3C) that keeps the interpreter state in
 * registers: s0 the object, s1 the flags, s2 the out argument, s3/s4 the
 * data of the state's and the object's executables, s5 the pc, s6 the sp,
 * s7 the fp, and fp the scratchpad (0x1F800000). This file is that code
 * written out instruction by instruction; it is not linked into the
 * PlayStation build (the linker script lists only splat's segments), it is
 * meant for the PC port.
 *
 * An instruction is a word: the opcode in the top byte, then usually two
 * 12-bit operands, L (bits 12-23) and R (bits 0-11). L is always read
 * before R, which matters when both pop the stack.
 *
 * Differences from the assembly, all in cases that would crash or read
 * garbage there:
 *  - division by zero gives what the R3000 gives (it does not trap);
 *  - writing through a destination whose linked object is missing goes to
 *    a dummy word (the original writes near address 0), and reading a
 *    halfword field of a missing linked object gives 0;
 *  - opcodes above 0x51 (past the end of the table) do nothing;
 *  - opcode 0x49, which calls native code at the pc, cannot run here: it
 *    returns from the subroutine instead. */

#ifndef GOOL_SCRATCHPAD
#define GOOL_SCRATCHPAD ((u8 *)0x1F800000)
#endif
#define SCRATCH(off) (*(u32 *)(GOOL_SCRATCHPAD + (off)))

/* Word n of the registers of obj, from &obj->self (0x40). */
#define OBJ_REG(o, n) ((u32 *)((u8 *)(o) + 0x40) + (n))

extern s16 D_8005C91C[]; /* sine table: a quarter wave, 0x401 entries */
extern u32 D_80060A2C;   /* random seeds (goolop.c) */
extern u32 D_80060A30;

void func_80035ABC(void);
void func_80035AC4(GoolObject *obj, u32 op);
void func_80035BE0(GoolObject *obj, u32 op);
void func_80035D08(GoolObject *obj, u32 op);
void func_80036B88(GoolObject *obj, u32 op);
void func_80037300(GoolObject *obj, u32 op);
void func_800376F8(GoolObject *obj, u32 op);
void func_8003780C(GoolObject *obj, u32 op);
void func_800378F8(GoolObject *obj, u32 op);
void func_80037A00(GoolObject *obj, u32 op);
void func_80037AE4(GoolObject *obj, u32 op);
void func_800385C0(GoolObject *obj, u32 op);
void func_8003864C(GoolObject *obj, u32 op);
void func_800386BC(GoolObject *obj, u32 op);
void func_800387A8(GoolObject *obj, u32 op);
void func_800388C8(GoolObject *obj, u32 op);
void func_800389C8(GoolObject *obj, u32 op);
s32 func_80037BEC(GoolObject *obj, u32 op, u32 flags);
s32 func_80037CD8(GoolObject *obj, u32 op, u32 flags);
s32 func_80037DAC(GoolObject *obj, u32 op, u32 flags);
s32 func_80037EDC(GoolObject *obj, u32 op, u32 flags);
s32 func_8003808C(GoolObject *obj, u32 op, u32 flags);
s32 func_80038248(GoolObject *obj, u32 op, u32 flags);
s32 func_800383DC(GoolObject *obj, u32 op, u32 flags, void *ref);
s32 func_800356B8(u32 op, s32 idx);
s32 func_80020184(s32 *in, s32 *out);
s32 func_8004F794(s32 x, s32 z); /* angle of (x, z) */

/* The registers of the interpreter. */
typedef struct {
    GoolObject *obj; /* s0 */
    u32 *pc;         /* s5 */
    u32 *sp;         /* s6 */
    u32 *fp;         /* s7 */
    u32 *sdata;      /* s3: data of the state's executable (obj->external) */
    u32 *gdata;      /* s4: data of obj->unk10 */
} GoolVm;

static u32 gool_sink; /* see the notes above */

/* Division as the R3000 does it, by zero included. */
static s32 GoolDiv(s32 a, s32 b) {
    if (b == 0) {
        return a < 0 ? 1 : -1;
    }
    if (b == -1 && a == (s32)0x80000000) {
        return a;
    }
    return a / b;
}

static s32 GoolMod(s32 a, s32 b) {
    if (b == 0) {
        return a;
    }
    if (b == -1) {
        return 0;
    }
    return a % b;
}

static u32 GoolModU(u32 a, u32 b) {
    return b != 0 ? a % b : a;
}

/* Sine of a 12-bit angle, 0x1000 = 1 (func_800397CC). */
static s32 GoolSin(u32 a) {
    s32 i = a & 0x3FF;
    s32 v = (a & 0x400) ? D_8005C91C[0x400 - i] : D_8005C91C[i];

    return (a & 0x800) ? -v : v;
}

/* The value of the 12-bit operand x (func_80038C94):
 *   0nnn nnnn nnnn  word n & 0x3FF of the data of obj->unk10 (bit 10 clear)
 *                   or of the state's executable (bit 10 set)
 *   100n nnnn nnnn  the constant n << 8 (signed)
 *   1010 nnnn nnnn  the constant n << 4 (signed)
 *   1011 0nnn nnnn  word n (signed) of the stack frame
 *   1011 1xxb xxxx  b
 *   110l llnn nnnn  register n of the object in register l (0 if none)
 *   111n nnnn nnnn  register n of obj; 0xE1F pops the stack */
static u32 GoolGet(GoolVm *vm, u32 x) {
    GoolObject *link;

    x &= 0xFFF;
    if (!(x & 0x800)) {
        return ((x & 0x400) ? vm->sdata : vm->gdata)[x & 0x3FF];
    }
    if (!(x & 0x400)) {
        if (!(x & 0x200)) {
            return (s32)(x << 23) >> 15;
        }
        if (!(x & 0x100)) {
            return (s32)(x << 24) >> 20;
        }
        if (!(x & 0x80)) {
            return vm->fp[(s32)(x << 25) >> 25];
        }
        return (x >> 4) & 1;
    }
    if (!(x & 0x200)) {
        link = *(GoolObject **)OBJ_REG(vm->obj, (x >> 6) & 7);
        if (link == NULL) {
            return 0;
        }
        return *OBJ_REG(link, x & 0x3F);
    }
    if (x == 0xE1F) {
        return *--vm->sp;
    }
    return *OBJ_REG(vm->obj, x & 0x1FF);
}

/* The address of the operand x (func_80038B40); a constant is stored in the
 * scratchpad word at slot first, and 1011 1xxb xxxx gives the "address" b. */
static u32 *GoolRef(GoolVm *vm, u32 x, s32 slot) {
    GoolObject *link;

    x &= 0xFFF;
    if (!(x & 0x800)) {
        return &((x & 0x400) ? vm->sdata : vm->gdata)[x & 0x3FF];
    }
    if (!(x & 0x400)) {
        if (!(x & 0x200)) {
            SCRATCH(slot) = (s32)(x << 23) >> 15;
            return &SCRATCH(slot);
        }
        if (!(x & 0x100)) {
            SCRATCH(slot) = (s32)(x << 24) >> 20;
            return &SCRATCH(slot);
        }
        if (!(x & 0x80)) {
            return &vm->fp[(s32)(x << 25) >> 25];
        }
        return (u32 *)((x >> 4) & 1);
    }
    if (!(x & 0x200)) {
        link = *(GoolObject **)OBJ_REG(vm->obj, (x >> 6) & 7);
        if (link == NULL) {
            return NULL;
        }
        return OBJ_REG(link, x & 0x3F);
    }
    if (x == 0xE1F) {
        return --vm->sp;
    }
    return OBJ_REG(vm->obj, x & 0x1FF);
}

/* The destination operand: bits 0-10 of op (func_80038C24); 0x21F pushes. */
static u32 *GoolDst(GoolVm *vm, u32 op) {
    GoolObject *link;

    if (!(op & 0x400)) {
        return &vm->fp[(s32)(op << 25) >> 25];
    }
    if (!(op & 0x200)) {
        link = *(GoolObject **)OBJ_REG(vm->obj, (op >> 6) & 7);
        if (link == NULL) {
            return &gool_sink;
        }
        return OBJ_REG(link, op & 0x3F);
    }
    if ((op & 0x3FF) == 0x21F) {
        return vm->sp++;
    }
    return OBJ_REG(vm->obj, op & 0x1FF);
}

/* A register of obj, or the stack (0x1F) popped (func_80038D88) or pushed
 * (func_80038DB0). */
static u32 *GoolRegPop(GoolVm *vm, u32 n) {
    if (n == 0x1F) {
        return --vm->sp;
    }
    return OBJ_REG(vm->obj, n);
}

static u32 *GoolRegPush(GoolVm *vm, u32 n) {
    if (n == 0x1F) {
        return vm->sp++;
    }
    return OBJ_REG(vm->obj, n);
}

/* The target and step of the seek instructions: L, step 0x100, or both
 * popped (step first) when L is 0xBF0. */
static s32 GoolSeekArgs(GoolVm *vm, u32 op, s32 *step) {
    s32 target;

    if (((op >> 12) & 0xFFF) == 0xBF0) {
        *step = *--vm->sp;
        target = *--vm->sp;
    } else {
        *step = 0x100;
        target = GoolGet(vm, op >> 12);
    }
    return target;
}

/* Moves R toward the target by the step (opcode 0x22, func_80039494). */
static s32 GoolSeek(GoolVm *vm, u32 op) {
    s32 step;
    s32 target;
    s32 cur;
    s32 d;

    if (((op >> 12) & 0xFFF) == 0xBF0) {
        step = *--vm->sp;
        target = *--vm->sp;
        if (step < 0) {
            step = -step;
        }
    } else {
        step = 0x100;
        target = GoolGet(vm, op >> 12);
    }
    cur = GoolGet(vm, op);
    d = target - cur;
    if (d >= 0) {
        return d - step > 0 ? cur + step : target;
    }
    return d + step >= 0 ? target : cur - step;
}

/* Turns the 12-bit angle R toward the target by step per frame time
 * (opcode 0x25, func_8003952C). */
static s32 GoolSeekAngle(GoolVm *vm, u32 op) {
    s32 step;
    s32 target = GoolSeekArgs(vm, op, &step);
    s32 cur = GoolGet(vm, op);
    s32 d = (s32)(((u32)target << 20) - ((u32)cur << 20)) >> 20;
    s32 ad = d < 0 ? -d : d;
    s32 s = (step * (s32)SCRATCH(0x54)) >> 10;

    if (ad - s <= 0) {
        return target;
    }
    return (d >= 0 ? cur + s : cur - s) & 0xFFF;
}

/* Changes the speed in the operand R toward the target by the step and
 * returns its size (opcode 0x13, func_800395E0). */
static s32 GoolSeekSpeed(GoolVm *vm, u32 op) {
    s32 step;
    s32 target = GoolSeekArgs(vm, op, &step);
    u32 *ref = GoolRef(vm, op, 0x18);
    s32 v = *ref;

    if (target >= 0) {
        if (v - target + step < 0) {
            v += step;
        } else {
            v = 2 * step - target;
        }
        *ref = v;
        return v < 0 ? -v : v;
    }
    if (v - target - step > 0) {
        v -= step;
    } else {
        v = -target - 2 * step;
    }
    *ref = v;
    return v > 0 ? -v : v;
}

/* Calls a subroutine: pushes the frame (flags, return pc, and the offsets
 * from &obj->self of the old fp and of the argc arguments, which the return
 * pops), then sets fp to it. */
static void GoolPushFrame(GoolVm *vm, u32 *flags, s32 argc) {
    u32 *sp = vm->sp;
    u8 *base = (u8 *)vm->obj + 0x40;

    sp[0] = *flags;
    sp[1] = (u32)vm->pc;
    sp[2] = ((u32)((u8 *)(sp - argc) - base)) | ((u32)((u8 *)vm->fp - base) << 16);
    vm->fp = sp;
    vm->sp = sp + 3;
    *flags &= ~9;
}

/* A GOOL line (opcode 0x51): projects the points L and R relative to the
 * camera and adds a Gouraud line (colors a and c of obj) to the ordering
 * table at their depth, after a draw mode primitive. */
static void GoolDrawLine(GoolVm *vm, u32 op) {
    GoolObject *obj = vm->obj;
    s32 *a = (s32 *)GoolRef(vm, op >> 12, 0x1C);
    s32 *b = (s32 *)GoolRef(vm, op, 0x18);
    MATRIX *m = &D_80065C90.unk20;
    u32 *mw = (u32 *)m;
    DrawContext *dc;
    u32 *prim;
    u32 *mode;
    u32 **slot;
    u32 c0;
    u32 c1;
    s32 sz1;
    s32 sz2;
    s32 i;

    gte_ctc2(mw[0], 0);
    gte_ctc2(mw[1], 1);
    gte_ctc2(mw[2], 2);
    gte_ctc2(mw[3], 3);
    gte_ctc2(mw[4], 4);
    gte_ctc2(0, 5);
    gte_ctc2(0, 6);
    gte_ctc2(0, 7);
    gte_mtc2((((b[0] >> 8) - m->t[0]) & 0xFFFF) | (((b[1] >> 8) - m->t[1]) << 16), 0);
    gte_mtc2((b[2] >> 8) - m->t[2], 1);
    gte_mtc2((((a[0] >> 8) - m->t[0]) & 0xFFFF) | (((a[1] >> 8) - m->t[1]) << 16), 2);
    gte_mtc2((a[2] >> 8) - m->t[2], 3);
    c0 = (u8)obj->colors.color_a.r | ((u8)obj->colors.color_a.g << 8) | ((u8)obj->colors.color_a.b << 16);
    c1 = (u8)obj->colors.color_c.r | ((u8)obj->colors.color_c.g << 8) | ((u8)obj->colors.color_c.b << 16);
    gte_rtpt();
    dc = context.cur;
    prim = (u32 *)dc->prims_tail;
    gte_mfc2(sz1, 17);
    gte_mfc2(sz2, 18);
    if (sz1 < 0 || sz2 < 0) {
        return;
    }
    i = obj->unk118 + 0x770 - ((u32)sz1 >> 6);
    if (i < 0) {
        i = 0;
    }
    if (i > 0x7FF) {
        i = 0x7FF;
    }
    slot = (u32 **)&dc->ot[i];
    prim[1] = c0 | 0x52000000;
    gte_stsxy(12, prim[2]);
    prim[3] = c1;
    gte_stsxy(13, prim[4]);
    prim[0] = (u32)*slot | 0x04000000;
    mode = prim + 5;
    mode[1] = 0xE1000A20;
    mode[2] = 0;
    mode[0] = 0x02000000 | ((u32)prim & 0xFFFFFF);
    *slot = (u32 *)((u32)mode & 0xFFFFFF);
    dc->prims_tail = mode + 3;
}

/* Runs the code of obj from its pc until an instruction stops it
 * (GoolObjectInterpret). flags: 1 return from the outermost frame, 8 stop
 * at a return (0x20: setting *out to 0xFF), 0x10 push 0 then. Returns
 * -0xFF, or the result of the instruction that stopped. */
s32 func_80038E28(GoolObject *obj, u32 flags, u32 *out) {
    GoolVm vm;
    u32 op;
    s32 l;
    s32 r;
    u32 *p;
    u32 *q;
    u32 w;
    s32 v;
    s32 sh;
    GoolObject *link;

    vm.obj = obj;
    vm.pc = obj->pc;
    vm.sp = obj->sp;
    vm.fp = obj->fp;
    vm.sdata = (u32 *)obj->external->items[2];
    vm.gdata = (u32 *)obj->unk10->items[2];

#define L_ GoolGet(&vm, op >> 12)
#define R_ GoolGet(&vm, op)
#define PUSH(x) (*vm.sp++ = (u32)(x))
#define BINOP(expr)                                                            \
    l = L_;                                                                    \
    r = R_;                                                                    \
    PUSH(expr);                                                                \
    break
#define UNOP(expr)                                                             \
    l = L_;                                                                    \
    p = GoolDst(&vm, op);                                                      \
    *p = (u32)(expr);                                                          \
    break
#define SAVE() (obj->sp = vm.sp, obj->fp = vm.fp)
#define LOAD() (vm.pc = obj->pc, vm.sp = obj->sp, vm.fp = obj->fp)
/* an instruction written in C (func_80038DD8) */
#define CALL_C(f)                                                              \
    SAVE();                                                                    \
    f(obj, op);                                                                \
    LOAD();                                                                    \
    break
/* one that may stop the interpreter */
#define CALL_F(f)                                                              \
    SAVE();                                                                    \
    v = f(obj, op, flags);                                                     \
    LOAD();                                                                    \
    if (v != 0) {                                                              \
        goto stop;                                                             \
    }                                                                          \
    break
#define BRANCH()                                                               \
    vm.pc += (s32)(op << 22) >> 22;                                            \
    vm.sp -= (op >> 10) & 0xF
#define POP_ARGS() (vm.sp -= (op >> 10) & 0xF)

    for (;;) {
        op = *vm.pc++;
        obj->pc = vm.pc;
        switch (op >> 24) {
        case 0x00: BINOP(l + r);
        case 0x01: BINOP(r - l);
        case 0x02: BINOP(l * r);
        case 0x03: BINOP(GoolDiv(r, l));
        case 0x04: BINOP(l == r);
        case 0x05: BINOP(l != 0 ? r : 0);
        case 0x06: BINOP((u32)l | (u32)r);
        case 0x07: BINOP((u32)l & (u32)r);
        case 0x08: BINOP((u32)l | (u32)r);
        case 0x09: BINOP(r < l);
        case 0x0A: BINOP(!(l < r));
        case 0x0B: BINOP(l < r);
        case 0x0C: BINOP(!(r < l));
        case 0x0D: BINOP(GoolMod(r, l));
        case 0x0E: BINOP((u32)l ^ (u32)r);
        case 0x0F: BINOP(((u32)l & (u32)r) == (u32)l);
        case 0x10: { /* random number in [R, L) */
            u32 s0 = D_80060A2C;
            u32 s1 = D_80060A30;
            u32 hi = s1 << 24;
            u32 n1 = s1 >> 8;
            u32 t = s0 + n1;
            u32 n0 = (s0 >> 8) | hi;

            l = L_;
            r = R_;
            t += n0 >> 8;
            n1 |= t << 24;
            D_80060A30 = n1;
            D_80060A2C = n0;
            PUSH(GoolModU(n1, l - r) + r);
            break;
        }
        case 0x11: UNOP(l);
        case 0x12: UNOP(l == 0);
        case 0x13:
            PUSH(GoolSeekSpeed(&vm, op));
            break;
        case 0x14:
            p = GoolRef(&vm, op >> 12, 0x1C);
            q = GoolDst(&vm, op);
            *q = (u32)p;
            break;
        case 0x15: /* shift R left by L (right when L < 0) */
            l = L_;
            r = R_;
            PUSH(l >= 0 ? (u32)r << (l & 31) : (u32)(r >> (-l & 31)));
            break;
        case 0x16:
            p = GoolRef(&vm, op >> 12, 0x1C);
            r = R_;
            w = p != NULL ? *p : 0;
            PUSH(r);
            if (p != NULL) {
                PUSH(w);
            }
            break;
        case 0x17: UNOP(~(u32)l);
        case 0x18: /* address of code: op & 0x3FFF in the state's (0x4000) or
                    * the object's executable, 0x3FFF none */
            p = GoolRegPush(&vm, (op >> 18) & 0x3F);
            w = op & 0x3FFF;
            if (w == 0x3FFF) {
                *p = 0;
            } else {
                *p = (u32)((u32 *)((op & 0x4000) ? obj->external : obj->global)->items[1] + w);
            }
            break;
        case 0x19: UNOP(l < 0 ? -l : l);
        case 0x1A:
            PUSH(func_800356B8(op & 0x1FFFFF, 0));
            break;
        case 0x1B: BINOP(((l * (s32)SCRATCH(0x54)) >> 10) + r);
        case 0x1C:
            SAVE();
            func_80035D08(obj, op);
            vm.sdata = (u32 *)obj->external->items[2];
            vm.gdata = (u32 *)obj->unk10->items[2];
            vm.sp = obj->sp;
            break;
        case 0x1D: /* a wave over the period L at R */
            l = L_;
            r = R_;
            sh = (0xFFFF < l) ? 2 : 0;
            v = GoolSin(GoolDiv(r << 11, l) - 0x400);
            v = (v + 0x1000) >> sh;
            v = (v * l) >> (13 - sh);
            PUSH(v);
            break;
        case 0x1E: BINOP(GoolMod(l + (s32)SCRATCH(0x50), r));
        case 0x1F:
            r = R_;
            PUSH(((u32 *)SCRATCH(0x58))[r >> 8]);
            break;
        case 0x20:
            l = L_;
            r = R_;
            ((u32 *)SCRATCH(0x58))[r >> 8] = l;
            break;
        case 0x21: BINOP((s32)(((u32)l << 20) - ((u32)r << 20)) >> 20);
        case 0x22:
            PUSH(GoolSeek(&vm, op));
            break;
        case 0x23: /* a halfword (from colors) of the object in a register */
            link = *(GoolObject **)OBJ_REG(obj, (op >> 12) & 7);
            PUSH(link != NULL ? *(s16 *)((u8 *)link + 0x20 + ((op >> 15) & 0x3F) * 2) : 0);
            break;
        case 0x24:
            r = R_;
            link = *(GoolObject **)OBJ_REG(obj, (op >> 12) & 7);
            if (link != NULL) {
                *(s16 *)((u8 *)link + 0x20 + ((op >> 15) & 0x3F) * 2) = r;
            }
            break;
        case 0x25:
            PUSH(GoolSeekAngle(&vm, op));
            break;
        case 0x26:
            p = GoolRef(&vm, op >> 12, 0x1C);
            q = GoolRef(&vm, op, 0x18);
            PUSH(q);
            if (p != NULL) {
                PUSH(p);
            }
            break;
        case 0x27: UNOP((u32 *)obj->global->items[5] + ((u32)l >> 8));
        case 0x28: CALL_C(func_80035AC4);
        case 0x29: CALL_C(func_80035BE0);
        case 0x2A: BINOP(*((u32 *)r + ((u32)l >> 8)));
        case 0x2B: UNOP(GoolSin(l));
        case 0x2C: UNOP(GoolSin(l + 0x400));
        case 0x2D:
            l = L_;
            p = GoolRef(&vm, op, 0x18);
            PUSH(func_8004F794(*p, l));
            break;
        case 0x2E:
            p = GoolRef(&vm, op >> 12, 0x1C);
            q = GoolDst(&vm, op);
            SCRATCH(0x20) = p[0];
            SCRATCH(0x24) = p[1];
            SCRATCH(0x28) = p[2];
            func_80020184((s32 *)p, (s32 *)q);
            break;
        case 0x2F:
            break;
        case 0x30:
            SAVE();
            func_80035ABC();
            LOAD();
            break;
        case 0x49: /* native code at the pc: not supported, see above */
        case 0x31: /* return from the subroutine */
            w = vm.fp[2];
            v = -0x19;
            if ((w >> 16) != 0) {
                u32 *fp = vm.fp;

                vm.sp = (u32 *)((u8 *)obj + 0x40 + (w & 0xFFFF));
                vm.pc = (u32 *)fp[1];
                vm.fp = (u32 *)((u8 *)obj + 0x40 + (w >> 16));
                w = (flags & 0xFFFF0000) | (fp[0] & 0xFFFF);
                v = -0xFF;
            }
            if (flags & 8) {
                if (flags & 0x20) {
                    *out = 0xFF;
                    v = -0xFF;
                } else {
                    v = -0x1A;
                }
                goto stop;
            }
            if ((flags & 1) || v > -0xFF) {
                if ((flags & 0x10) && v < -0xFE) {
                    PUSH(0);
                }
                goto stop;
            }
            if ((w & 0xFFFF) != 0xFFFF) {
                flags = w;
            }
            break;
        case 0x32:
            BRANCH();
            break;
        case 0x33:
            if (*GoolRegPop(&vm, (op >> 14) & 0x3F) != 0) {
                BRANCH();
            }
            break;
        case 0x34:
            if (*GoolRegPop(&vm, (op >> 14) & 0x3F) == 0) {
                BRANCH();
            }
            break;
        case 0x35: CALL_F(func_80037BEC);
        case 0x36:
            if (*GoolRegPop(&vm, (op >> 14) & 0x3F) == 0) {
                POP_ARGS();
                break;
            }
            CALL_F(func_80037BEC);
        case 0x37:
            if (*GoolRegPop(&vm, (op >> 14) & 0x3F) != 0) {
                POP_ARGS();
                break;
            }
            CALL_F(func_80037BEC);
        case 0x38: CALL_F(func_80037CD8);
        case 0x39: CALL_F(func_80037DAC);
        case 0x3A: CALL_C(func_80036B88);
        case 0x3B: /* call op & 0x3FFF in the state's (0x4000) or the
                    * object's executable with (op >> 20) & 0xF arguments */
            w = op & 0x3FFF;
            GoolPushFrame(&vm, &flags, (op >> 20) & 0xF);
            if (w == 0x3FFF) {
                vm.pc = NULL;
            } else {
                vm.pc = (u32 *)((op & 0x4000) ? obj->external : obj->global)->items[1] + w;
            }
            break;
        case 0x3C: CALL_F(func_80037EDC);
        case 0x3D:
        case 0x3E:
            SAVE();
            v = func_800383DC(obj, op, flags, out);
            flags = SCRATCH(0x48);
            LOAD();
            if (v != 0) {
                goto stop;
            }
            break;
        case 0x3F: CALL_C(func_800376F8);
        case 0x40: CALL_C(func_80037AE4);
        case 0x41: CALL_C(func_8003780C);
        case 0x42: CALL_C(func_800378F8);
        case 0x43: CALL_C(func_80037300);
        case 0x44: CALL_F(func_80038248);
        case 0x45: CALL_F(func_8003808C);
        case 0x46: CALL_C(func_800376F8);
        case 0x47: /* call the code R points to, with L >> 8 arguments */
            l = L_;
            p = GoolRef(&vm, op, 0x18);
            if (p == NULL || *p == 0) {
                vm.sp -= l; /* as in the original: L words, not L >> 8 */
                break;
            }
            w = *p;
            GoolPushFrame(&vm, &flags, l >> 8);
            vm.pc = (u32 *)w;
            break;
        case 0x48: CALL_C(func_80037A00);
        case 0x4A: CALL_C(func_800385C0);
        case 0x4B: CALL_C(func_8003864C);
        case 0x4C: CALL_C(func_800386BC);
        case 0x4D: CALL_C(func_800387A8);
        case 0x4E:
            l = L_;
            r = R_;
            p = (u32 *)r + ((u32)l >> 8);
            *p = *--vm.sp;
            break;
        case 0x4F: CALL_C(func_800388C8);
        case 0x50: CALL_C(func_800389C8);
        case 0x51:
            GoolDrawLine(&vm, op);
            break;
        default:
            break;
        }
    }

stop:
    obj->pc = vm.pc;
    obj->sp = vm.sp;
    obj->fp = vm.fp;
    return v;

#undef L_
#undef R_
#undef PUSH
#undef BINOP
#undef UNOP
#undef SAVE
#undef LOAD
#undef CALL_C
#undef CALL_F
#undef BRANCH
#undef POP_ARGS
}
