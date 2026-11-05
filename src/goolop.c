#include "common.h"
#include "gool.h"
#include "game.h"
#include "ns.h"
#include "pad.h"

extern GoolObject *crash;
extern Entry *cur_zone;

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
u32 *func_80035538(GoolObject *obj, u32 op, s32 push);
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
/* Like GoolRef for a destination: 0xE1F pushes on the stack. */
static __inline__ u32 *GoolDst(GoolObject *obj, u32 x) {
    if ((x & 0xE00) == 0xE00) {
        if (x == 0xE1F) {
            return obj->sp++;
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

/* For each combination of the d-pad buttons: the direction (8: none), the
 * angle, the magnitude and the stick position it stands for. */
typedef struct {
    s32 dir;
    s32 angle;
    s32 mag;
    s32 stick;
} PadDir;
extern PadDir D_8005BF00[16];
#define PAD_DIR(b) D_8005BF00[((b) & 0xF000) >> 12].dir

#ifdef NON_MATCHING
/* Whether pad idx satisfies the button test op: buttons tapped or held
 * (now or in the previous frame), then a direction (9..12: a d-pad button,
 * else one of the 8 directions) tapped or held; bit 20 negates it.
 * Equivalent; the original keeps the negated result in a0 (v1 here). */
s32 func_800356B8(u32 op, s32 idx) {
    s32 r;
    s32 dir;
    u32 b;
    u32 m;

    r = 1;
    switch ((op >> 12) & 3) {
    case 1:
        b = pads.pads[idx].tapped & 0xFFF;
        r = b & op;
        break;
    case 2:
        b = pads.pads[idx].held & 0xFFF;
        r = b & op;
        break;
    case 4:
        m = op & 0xFFF;
        r = m & (pads.pads[idx].held | pads.pads[idx].held_prev);
        break;
    case 3:
        m = op & 0xFFF;
        r = m & (pads.pads[idx].tapped | pads.pads[idx].tapped_prev);
        break;
    }
    if (!r) {
        return (op & 0x100000) ? !r : r;
    }
    dir = (op >> 16) & 0xF;
    switch ((op >> 14) & 3) {
    case 1:
        switch (dir) {
        case 9:
            r = pads.pads[idx].tapped & 0x1000;
            break;
        case 10:
            r = pads.pads[idx].tapped & 0x2000;
            break;
        case 11:
            r = pads.pads[idx].tapped & 0x4000;
            break;
        case 12:
            r = pads.pads[idx].tapped & 0x8000;
            break;
        default:
            r = dir == PAD_DIR(pads.pads[idx].held) && dir != PAD_DIR(pads.pads[idx].held_prev);
            break;
        }
        break;
    case 2:
        switch (dir) {
        case 9:
            r = pads.pads[idx].held & 0x1000;
            break;
        case 10:
            r = pads.pads[idx].held & 0x2000;
            break;
        case 11:
            r = pads.pads[idx].held & 0x4000;
            break;
        case 12:
            r = pads.pads[idx].held & 0x8000;
            break;
        default:
            r = dir == PAD_DIR(pads.pads[idx].held);
            break;
        }
        break;
    case 3:
    case 4:
        r = 0;
        if (((op >> 16) & 0xF) == PAD_DIR(pads.pads[idx].held) ||
            ((op >> 16) & 0xF) == PAD_DIR(pads.pads[idx].held_prev)) {
            r = 1;
        }
        break;
    }
    return (op & 0x100000) ? !r : r;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800356B8);
#endif

void func_80035ABC(void) {
}

s32 func_80032DF8(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 def);
/* Instruction: pushes the value of property (b >> 8) of the zone item of
 * obj (0 without one). */

void func_80035AC4(GoolObject *obj, u32 op) {
    u32 a = *GoolRef(obj, (op >> 12) & 0xFFF);
    u32 b = *GoolRef(obj, op & 0xFFF);

    *obj->sp++ = obj->unkF0 ? func_80032DF8(obj->unkF0, b >> 8, a, 0, 0, 0, 0) : 0;
}

/* Instruction: sets a word to the value of property (a >> 8) of the zone
 * item of obj (0 without one). */

void func_80035BE0(GoolObject *obj, u32 op) {
    u32 a = *GoolRef(obj, (op >> 12) & 0xFFF);
    u32 *dst = GoolDst(obj, op & 0xFFF);

    if (dst) {
        *dst = obj->unkF0 ? func_80032DF8(obj->unkF0, a >> 8, 0, 0, 0, 2, 0) : 0;
    }
}

extern s32 D_8005C1A4[3]; /* a point (the camera's) */
extern u32 spawns[];
extern s32 ticks_elapsed;
extern s32 next_lid;
extern Entry *D_80065D60[];
typedef struct {
    u32 categories;
    GoolObject *obj;
    GoolObject *nearest_obj;
    s32 dist;
    u32 event;
} GoolNearestQuery;
s32 func_80031A54(s32 *a, s32 *b);
s32 func_80031B50(s32 *a, s32 *b);
s32 func_80031C74(s32 *a, s32 *b);
s32 func_80031CF0(s32 *a, s32 *b);
s32 func_80031D4C(s32 *a, s32 *b);
s32 func_80031DA8(s32 *a, s32 *b);
s32 func_80031DDC(s32 *a, s32 *b);
GoolObject *GoolObjectHasPidFlags(GoolObject *obj, s32 pid_flags);
s32 func_80019ACC(GoolObject *obj, GoolNearestQuery *query);
GoolObject *func_8001ABF8(s32 pid_flags);
void func_80020F68(s32 pid);
void func_80021008(s32 pid);
Entry *func_80028D8C(Entry *zone, s32 *pos);
void func_8002FF3C(GoolObject *obj, SaveState *save, s32 a, s32 mode);
void func_80030388(SaveState *save);
s32 GoolObjectAddChild(GoolObject *child, GoolObject *parent);
void func_80033154(s32 a);
void func_80034C0C(void);
void func_80034C1C(s32 p);
void GoolZoneNeighborObjectsTerminate(void);
void NSCdSeek(s32 idx);
void func_8002FCA0(s32 mode);
u8 *func_80032710(u8 *item, s32 tag, s32 pos, s32 index, s32 *outcount, s32 mode, u8 *buf, s32 count, s32 def);
void func_8001B0BC(GoolObject *obj, s32 *scale);
void func_8001B630(GoolObject *obj, s32 *out);
void func_80072194(void); /* overlay */
void func_800302AC(s32 level, s32 idx, s32 a, s32 b);
s32 *func_8002D0E8(GoolObject *obj, s32 a, s32 idx, s32 unused); /* takes a collision box */
void func_8002D85C(void); /* releases it */
s32 func_8003188C(s32 *point, s32 *box); /* point in box */
s32 func_80017CA8(GoolObject *obj, u32 msg, s32 arg);
#define LINK(obj, op) (*(GoolObject **)((u8 *)(obj) + (((op) >> 10) & 0x1C) + 0x40))
#ifdef NON_MATCHING
/* Miscellaneous instruction: distances and angles between objects or to
 * the camera, normalizing a vector, fields of linked objects, the spawn
 * flags of entities, zones, saves, music, the level, analog sticks, the
 * nearest object, collision boxes.
 * Equivalent; differs in registers of cases 3 and 4 and in the scheduling of the loads of cur_zone (an explicit lui in the original). */
void func_80035D08(GoolObject *obj, u32 op) {
    u32 *x;
    s32 sub;
    s32 *a;
    s32 *pa;
    s32 *pb;
    s32 *b;
    s32 *v;
    s32 len;
    s32 vx;
    s32 vy;
    s32 vz;
    s32 d;
    s32 i;
    GoolHandle *h;
    GoolObject *r;
    u32 *p;
    u32 t;
    Entry *zone;
    s32 type;
    u8 *pad;
    s16 *ids;
    s16 *list;
    s32 id;
    s32 n;
    u32 *flags;
    GoolObject *o;
    GoolObject *l;
    s32 *box;
    s32 buf[2];
    GoolNearestQuery query;
    s32 count;
    u32 arg;

    x = GoolRef(obj, op & 0xFFF);
    sub = (s32)(op << 12) >> 27;
    switch ((op >> 20) & 0xF) {
    case 1:
    case 6:
        pa = (s32 *)x;
        if (((op >> 20) & 0xF) != 6) {
            pa = obj->xform.trans;
        }
        if (sub & 8) {
            pb = D_8005C1A4;
        } else {
            pb = LINK(obj, op)->xform.trans;
        }
        switch (sub & 7) {
        case 0:
        case 4:
            *obj->sp++ = func_80031A54(pa, pb);
            break;
        case 1:
            *obj->sp++ = func_80031C74(pa, pb);
            break;
        case 2:
        case 6:
            *obj->sp++ = func_80031B50(pa, pb);
            break;
        case 3:
            *obj->sp++ = func_80031CF0(pa, pb);
            break;
        case 7:
            *obj->sp++ = func_80031D4C(pa, pb);
            break;
        }
        break;
    case 5:
        len = (s32)*--obj->sp >> 8;
        v = (s32 *)x;
        vx = v[0] >> 8;
        vz = v[2] >> 8;
        if (sub & 1) {
            d = func_8003D9DC(vx * vx + vz * vz);
        } else {
            vy = (s32)x[1] >> 8;
            d = func_8003D9DC(vx * vx + vy * vy + vz * vz);
            if (d != 0) {
                x[1] = ((vy * len) / d) << 8;
            } else {
                x[1] = 0;
            }
        }
        if (len > 0x10) {
            d /= 16;
            if (d == 0) {
                goto zero;
            }
            v[0] = ((vx * len) / 16 / d) << 8;
            v[2] = ((vz * len) / 16 / d) << 8;
        } else if (d == 0) {
        zero:
            v[2] = 0;
            v[0] = 0;
        } else if (len >= 0x1000) {
            v[0] = ((vx * len) / d) << 8;
            v[2] = ((vz * len) / d) << 8;
        } else {
            v[0] = (v[0] * len) / d;
            v[2] = (v[2] * len) / d;
        }
        break;
    case 2:
        if ((u8)(*(u8 *)&obj->unk120 - 6) >= 2) {
            if (!(sub & 1)) {
                pa = LINK(obj, op)->xform.trans;
            } else {
                pa = D_8005C1A4;
            }
            *obj->sp++ = func_80031DA8(pa, (s32 *)x);
        } else {
            *obj->sp++ = func_80031DDC(LINK(obj, op)->xform.trans, (s32 *)x);
        }
        break;
    case 3:
        o = LINK(obj, op);
        p = obj->sp;
        *p++ = ((u32 *)&o->self)[*x >> 8];
        obj->sp = p;
        break;
    case 4:
        o = LINK(obj, op);
        ((u32 *)&o->self)[*x >> 8] = *--obj->sp;
        break;
    case 7:
        if (x == NULL) {
            break;
        }
        if (*x == 0 || !(spawns[*x >> 8] & 1)) {
            *obj->sp++ = 0;
            break;
        }
        i = 3;
        do {
            r = (GoolObject *)GoolObjectSearchTree((GoolObject *)&game.handles[i++], (GoolTraverseFunc)GoolObjectHasPidFlags, *x);
        } while (r == NULL && i < 5);
        *obj->sp++ = (u32)r;
        break;
    case 8:
        if (x == NULL) {
            break;
        }
        if (sub != 0) {
            spawns[*x >> 8] &= ~2;
        } else {
            spawns[*x >> 8] |= 2;
        }
        break;
    case 10:
        if (x == NULL) {
            break;
        }
        switch (sub) {
        case 0:
            spawns[*x >> 8] &= ~4;
            break;
        case 1:
            spawns[*x >> 8] |= 4;
            break;
        case 4:
            func_80021008(*x);
        case 2:
            spawns[*x >> 8] &= ~8;
            break;
        case 5:
            func_80020F68(*x);
        case 3:
            spawns[*x >> 8] |= 8;
            break;
        case 9:
            spawns[*x >> 8] |= 1;
            break;
        case 8:
            spawns[*x >> 8] &= ~1;
            break;
        }
        break;
    case 11:
        switch (sub) {
        case 1:
            p = obj->sp++;
            if (x != NULL) {
                t = !(spawns[*x >> 8] & 2);
            } else {
                t = 0;
            }
            *p = t;
            break;
        case 2:
            p = obj->sp++;
            if (x != NULL) {
                t = spawns[*x >> 8] & 4;
            } else {
                t = 0;
            }
            *p = t;
            break;
        case 3:
            p = obj->sp++;
            if (x != NULL) {
                t = spawns[*x >> 8] & 8;
            } else {
                t = 0;
            }
            *p = t;
            break;
        }
        break;
    case 9:
        if (x == NULL) {
            zone = cur_zone;
        } else {
            zone = func_80028D8C(cur_zone, (s32 *)x);
        }
        if ((s32)zone < -0xFE) {
            LINK(obj, op)->zone = zone;
        }
        break;
    case 12:
        switch ((op >> 15) & 0x1F) {
        case 0:
            func_8002FF3C(obj, &D_80065C90.saves[((u8 *)x)[1]], 0, ((u16 *)x)[1]);
            break;
        case 1:
            func_80030388(&D_80065C90.saves[*x >> 8]);
            break;
        case 2:
            switch ((op >> 12) & 7) {
            case 0:
                GoolObjectAddChild(obj, (GoolObject *)&game.handles[*x >> 8]);
                break;
            case 1:
                GoolObjectAddChild(obj, (GoolObject *)*x);
                break;
            }
            break;
        case 3:
            func_80033154(*x);
            break;
        case 4:
            ((GoolObject *)*x)->zone = D_80065D60[0];
            break;
        case 5:
            func_80034C0C();
            break;
        case 6:
            func_80034C1C(*x);
            break;
        case 7:
            GoolZoneNeighborObjectsTerminate();
            break;
        case 8:
            a = LINK(obj, op)->xform.trans;
            d = func_80031CF0(a, (s32 *)x);
            *obj->sp++ = func_8004F794(a[1] - (s32)x[1], d);
            break;
        case 9:
            next_lid = *x >> 8;
            break;
        case 10:
            NSCdSeek(*x >> 8);
            break;
        case 11:
            switch ((op >> 12) & 7) {
            case 0:
                func_8002FCA0(1);
                break;
            case 1:
                func_8002FCA0(0);
                break;
            }
            break;
        case 12:
            *x = ticks_elapsed;
            break;
        case 13:
            ticks_elapsed = *x;
            break;
        case 14:
            LINK(obj, op)->external = (Entry *)*x;
            break;
        case 15:
            *obj->sp++ = (u32)((GoolObject *)*x)->external;
            break;
        case 16:
            LINK(obj, op)->unk10 = (Entry *)*x;
            break;
        case 17:
            *obj->sp++ = (u32)((GoolObject *)*x)->unk10;
            break;
        case 20:
            type = 0;
            pad = (u8 *)&pads.pads[0];
            if (*(s8 *)&pad[0] == 0) {
                type = *(s8 *)&pad[1];
                if ((type == 0x53 || type == 0x73) && x != NULL) {
                    x[0] = pad[6];
                    x[1] = pad[7];
                }
            }
            *obj->sp++ = type;
            break;
        case 18:
            arg = 1;
            list = (s16 *)func_80032710((u8 *)obj->unkF0, 0x287, 0, 0, &count, 2, NULL, 0, 0);
            if (list == NULL) {
                break;
            }
            for (n = 0; n < count; n++) {
                id = list[n];
                flags = &spawns[id];
                if (*flags & 1) {
                    o = func_8001ABF8(id << 8);
                    if (o != NULL) {
                        GoolSendEvent(obj, o, 0x300, 1, &arg);
                    }
                } else if (!(*flags & 2)) {
                    GoolSendEvent(obj, crash, 0x4800, 0, NULL);
                    *flags |= 2;
                }
            }
            break;
        case 21:
            func_8001B0BC(obj, obj->xform.scale);
            func_8001B630(obj, buf);
            break;
        case 22:
            func_80072194();
            break;
        case 23:
            func_800302AC(ns.ldat->lid, game.unk2A4, 0, game.unk314);
            break;
        }
        break;
    case 13:
        query.categories = (op >> 15) & 0x1F;
        query.obj = LINK(obj, op);
        query.dist = 0x7FFFFFFF;
        query.nearest_obj = NULL;
        query.event = *x;
        GoolObjectSearchTree((GoolObject *)&game.handles[4], (GoolTraverseFunc)func_80019ACC, (s32)&query);
        if ((op >> 15) & 0x17) {
            GoolObjectSearchTree((GoolObject *)&game.handles[3], (GoolTraverseFunc)func_80019ACC, (s32)&query);
        }
        if (query.nearest_obj != NULL) {
            *obj->sp++ = (u32)query.nearest_obj;
        } else {
            *obj->sp++ = 0;
        }
        break;
    case 14:
        l = LINK(obj, op);
        if (((u32)l & 0xFF800003) != 0x80000000 || (box = func_8002D0E8(l, 0, -2, sub)) == NULL) {
            *obj->sp++ = 0;
            break;
        }
        *obj->sp++ = func_8003188C((s32 *)x, box + 2);
        func_8002D85C();
        break;
    case 15:
        obj->unkD4 = func_80017CA8(obj, (op >> 15) & 0x1F, *x);
        break;
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80035D08);
#endif

void *memset(void *dst, s32 c, s32 n);
s32 func_8003C2F0(s32 angle); /* sin */
s32 func_8003C364(s32 angle); /* cos */
void func_80020670(GoolObject *obj, s32 a, s32 *pos);
void func_8001FFF0(s32 *in, s32 *pos, s32 *a, s32 *b, s32 unused, s32 *out);
void func_800200C8(GoolObject *obj, s32 unused, s32 *in, s32 *out);
s32 func_800202CC(s32 *in, s32 *out, s32 unused);
s32 func_800203A4(s32 *in, s32 *out);
/* A frame of vertex animation: its origin, its scale and, after count
 * 0x28-byte items at 0x1C, the vertices (3 bytes each: x, z, y). */
typedef struct {
    s16 x;
    s16 y;
    s32 z;
    s16 sx;
    s16 sy;
    s32 sz;
    u8 unk10[4];
    s32 count;
} VtxFrame;
#define FRAME_VERTEX(f, i) ((u8 *)(f) + ((f)->count * 0x28 + 0x1C) + (i) * 3)
#ifdef NON_MATCHING
/* Instruction: vector operations on vector (op >> 12) & 7 of obj (and
 * vector (op >> 15) & 7 or a linked object): moves it along the path,
 * velocity from the angle, transforms or projects it, a vertex of the
 * model of a linked object.
 * Equivalent; the loads and the stores of the vertex case (3, 6) are scheduled differently. */
void func_80036B88(GoolObject *obj, u32 op) {
    Vec3 t;
    s32 in[3];
    s32 sc[3];
    s32 zero[3];
    s32 unused[4]; /* takes stack space in the original */
    s32 *v;
    u32 *x;
    s32 *dst;
    GoolObject *link;
    GoolObject *src;
    s32 r;
    s32 d;
    s32 *scr;
    VtxFrame *fa;
    VtxFrame *fb;
    s32 *scale;
    s32 *s;
    u8 *vtx;

    v = ((Vec3 *)&obj->xform + ((op >> 12) & 7))->v;
    x = GoolRef(obj, op & 0xFFF);
    switch ((op >> 18) & 7) {
    case 0:
        if (x != NULL && obj->unkF0 != 0) {
            t = *(Vec3 *)v;
            func_80020670(obj, *x, t.v);
            *(Vec3 *)v = t;
            *(s32 *)obj->unkFC = v[1];
        }
        break;
    case 2:
        v[0] = ((func_8003C2F0(obj->misc_b[1] & 0xFFF) >> 4) * (s32)*x) >> 8;
        if ((u8)(*(u8 *)&obj->unk120 - 6) >= 2) {
            v[2] = ((func_8003C364(obj->misc_b[1] & 0xFFF) >> 4) * (s32)*x) >> 8;
        } else {
            v[1] = ((func_8003C364(obj->misc_b[1] & 0xFFF) >> 4) * (s32)*x) >> 8;
        }
        obj->speed = *x;
        break;
    case 4:
    case 5:
        in[2] = *x;
        in[1] = *--obj->sp;
        in[0] = *--obj->sp;
        dst = ((Vec3 *)&obj->xform + ((op >> 15) & 7))->v;
        if (((op >> 18) & 7) == 4) {
            src = *(GoolObject **)((u8 *)obj + ((op >> 19) & 0x1C) + 0x40);
            r = ((in[2] >> 2) * func_8003C364(-src->xform.rot[0])) >> 12;
            dst[1] = (((in[2] >> 2) * func_8003C2F0(-src->xform.rot[0])) >> 10) + src->xform.trans[1];
            dst[0] = ((r * func_8003C2F0(src->xform.rot[1])) >> 10) + src->xform.trans[0];
            dst[2] = ((r * func_8003C364(src->xform.rot[1])) >> 10) + src->xform.trans[2];
        } else {
            func_8001FFF0(in, v, obj->misc_b, NULL, 0, dst);
        }
        break;
    case 3:
    case 6:
        link = *(GoolObject **)((u8 *)obj + ((op >> 19) & 0x1C) + 0x40);
        if (*(u8 **)&link->unkE8 == NULL || **(u8 **)&link->unkE8 != 1) {
            break;
        }
        fa = *(VtxFrame **)((u8 *)link->unk8 + 0x20);
        fb = *(VtxFrame **)((u8 *)link->unk8 + 0x24);
        vtx = FRAME_VERTEX(fa, *x >> 8);
        scale = *(s32 **)(*(u8 **)((u8 *)link->unk8 + 0x28) + 0x10);
        s = scale;
        in[0] = (fa->x + (vtx[0] << 5)) << 8;
        in[1] = (fa->y + (vtx[2] << 5)) << 8;
        in[2] = (fa->z + (vtx[1] << 5)) << 8;
        if (fa != fb) {
            vtx = FRAME_VERTEX(fb, *x >> 8);
            in[0] = in[0] + ((fb->x + (vtx[0] << 5)) << 8);
            in[1] = in[1] + ((fb->y + (vtx[2] << 5)) << 8);
            in[0] >>= 1;
            in[1] >>= 1;
            in[2] = in[2] + ((fb->z + (vtx[1] << 5)) << 8);
            in[2] >>= 1;
        }
        if (((op >> 18) & 7) == 3) {
            memset(zero, 0, sizeof(zero));
            func_8001FFF0(in, link->xform.trans, zero, scale, 0, v);
        } else {
            func_800200C8(link, 0, in, v);
            sc[0] = (s[0] * link->xform.scale[0]) >> 12;
            sc[1] = (s[1] * link->xform.scale[1]) >> 12;
            sc[2] = (s[2] * link->xform.scale[2]) >> 12;
            if (fa != fb) {
                v[0] += ((fa->sx + fb->sx) * sc[0]) >> 5;
                v[1] += ((fa->sy + fb->sy) * sc[1]) >> 5;
                v[2] += ((fa->sz + fb->sz) * sc[2]) >> 5;
            } else {
                v[0] += (fa->sx * sc[0]) >> 4;
                v[1] += (fa->sy * sc[1]) >> 4;
                v[2] += (fa->sz * sc[2]) >> 4;
            }
        }
        break;
    case 1:
        scr = ((Vec3 *)&obj->xform + ((op >> 15) & 7))->v;
        func_800203A4(v, scr);
        if (x[0] != 0 && (d = scr[2] >> 8) != 0) {
            x[0] = (s32)(x[0] * 0x118) / d;
            x[1] = (s32)(x[1] * 0x118) / d;
            x[2] = (s32)(x[2] * 0x118) / d;
        }
        break;
    case 7:
        func_800202CC(v, ((Vec3 *)&obj->xform + ((op >> 15) & 7))->v, 0);
        break;
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80036B88);
#endif

s32 func_800296F8(Vec3 *pos, Vec3 *out);
void *func_80029C1C(void *unused, s32 *pos);
void *func_80029E40(GoolObject *obj, s32 *pos);
void func_8002A22C(GoolObject *obj, s32 *pos, s32 mode, s32 flag);
void func_8002A728(GoolObject *obj);
/* Instruction: positions relative to the camera, the floor, etc. of the
 * position of obj (a vector of obj selected by bits 12-14 for some); the
 * result goes to the vector selected by bits 15-17 and a word. */

void func_80037300(GoolObject *obj, u32 op) {
    Vec3 a;
    Vec3 b;
    Vec3 c;
    Vec3 d;
    Vec3 e;
    Vec3 f;
    Vec3 *v;
    u32 sub;
    s32 mode;
    s32 alt;
    s32 low;
    Entry *zone;
    s32 *pos;

    v = (Vec3 *)&obj->xform + ((op >> 12) & 7);
    func_80035538(obj, op & 0xFFF, 0);
    switch ((op >> 18) & 7) {
    case 0:
        a = *(Vec3 *)&obj->xform;
        b = *v;
        a.v[1] += *(s32 *)((u8 *)obj->unk8 + 0x40) / 2;
        obj->unkD4 = func_800296F8(&a, &b);
        if ((op & 0xFFF) != 0xBE0) {
            *(Vec3 *)GoolDst(obj, op & 0xFFF) = b;
        }
        ((Vec3 *)&obj->xform)[(op >> 15) & 7] = a;
        break;
    case 1:
        c = *(Vec3 *)&obj->xform;
        d = *(Vec3 *)&obj->xform;
        if (obj == crash || obj->parent == crash) {
            func_80029E40(obj, c.v);
        }
        obj->unkD4 = (s32)func_80029C1C(obj, d.v);
        if ((op & 0xFFF) != 0xBE0) {
            *(Vec3 *)GoolDst(obj, op & 0xFFF) = e;
        }
        ((Vec3 *)&obj->xform)[(op >> 15) & 7] = d;
        break;
    case 2:
    case 3:
    case 4:
    case 5:
        sub = (op >> 18) & 7;
        low = sub == 4 || sub == 5;
        alt = sub == 5 || sub == 3;
        e = *(Vec3 *)&obj->xform;
        pos = e.v;
        zone = obj->zone ? obj->zone : cur_zone;
        if (((ZoneHdr *)zone->items[0])->unk2DC & 1) {
            mode = alt ? 6 : 2;
        } else {
            mode = alt ? 5 : 1;
        }
        func_8002A22C(obj, pos, mode, !low);
        if ((op & 0xFFF) != 0xBE0) {
            *(Vec3 *)GoolDst(obj, op & 0xFFF) = f;
        }
        ((Vec3 *)&obj->xform)[(op >> 15) & 7] = e;
        break;
    case 6:
        func_8002A728(obj);
        break;
    }
}

#ifdef NON_MATCHING
/* Instruction: creates op & 0x3F objects (0: count popped) of executable
 * (op >> 12) & 0xFF, subtype (op >> 6) & 0x3F, with (op >> 20) & 0xF
 * arguments from the stack; unkD4 gets the last one.
 * pop++ goes through a temporary in the original (addiu v0 / move s5) */
void func_800376F8(GoolObject *obj, u32 op) {
    s32 count;
    s32 pop;
    s32 i;
    GoolObject *o;

    count = op & 0x3F;
    pop = (op >> 20) & 0xF;
    if (count == 0) {
        pop = pop + 1;
        count = *(obj->sp - pop);
    }
    for (i = 0; i < count; i++) {
        o = GoolObjectCreate(obj, (op >> 12) & 0xFF, (op >> 6) & 0x3F, (op >> 20) & 0xF,
                             &obj->sp[-((op >> 20) & 0xF)], (op >> 24) == 0x46);
        if ((s32)o < -0xFE) {
            o->creator = obj;
            obj->unkD4 = (s32)o;
        } else {
            obj->unkD4 = 0;
        }
    }
    obj->sp -= pop;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/goolop", func_800376F8);
#endif

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

s32 NSCountAvailablePages(void);
s32 NSCountAvailablePages2(u32 **list, s32 len);
/* Instruction: entry operations (open, page in, count free pages). */


void func_80037AE4(GoolObject *obj, u32 op) {
    s32 sub;
    u32 *ref;
    s32 n;
    s32 v;

    sub = *func_80035538(obj, (op >> 12) & 0xFFF, 0);
    ref = func_80035538(obj, op & 0xFFF, 0);
    switch (sub) {
    case 1:
        obj->unkD4 = NSOpen(ref, 0, 1);
        break;
    case 6:
        obj->unkD4 = NSOpen(ref, 1, 1);
        break;
    case 2:
        obj->unkD4 = (s32)func_8001440C(ref, 1);
        break;
    case 3:
        *obj->sp++ = (u32)func_8001440C(ref, 0);
        break;
    case 4:
        *obj->sp++ = NSCountAvailablePages();
        break;
    case 5:
        n = *ref;
        v = NSCountAvailablePages2((u32 **)&obj->sp[-n], n);
        obj->sp -= n;
        *obj->sp++ = v;
        break;
    }
}

/* Instruction: changes the state of obj to op & 0x3FF with (op >> 10) & 0xF
 * popped arguments, unless the state is not allowed (its flags against
 * status_c, with 0x402 while obj is hurt). */

s32 GoolObjectChangeState(GoolObject *obj, u32 state, s32 argc, u32 *argv);

#ifdef NON_MATCHING
/* Equivalent; the original pops the stack in v0. */
s32 func_80037BEC(GoolObject *obj, u32 op, u32 flags) {
    u32 argv[16];
    s32 n;
    s32 i;
    s32 r;
    u32 sc;
    u32 f;
    u32 m;

    n = (op >> 10) & 0xF;
    for (i = n - 1; i >= 0; i--) {
        argv[i] = *--obj->sp;
    }
    sc = obj->status_c;
    f = ((u32 *)obj->global->items[4])[(op & 0x3FF) * 4];
    if ((u32)(obj->invincibility_state - 2) < 3) {
        m = (sc | 0x402) & f;
    } else {
        m = sc & f;
    }
    if (m) {
        return 0;
    }
    r = GoolObjectChangeState(obj, op & 0x3FF, n, argv);
    if (r >= -0xFE || !(flags & 2)) {
        obj->sp--;
        r = 0;
    }
    return r;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/goolop", func_80037BEC);
#endif

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

s32 func_8001E3BC(GoolObject *sender, GoolObject *to, u32 event, s32 type, s32 argc, u32 *argv);
/* Instruction: as func_80037EDC, with func_8001E3BC (type (op >> 21) & 7). */

s32 func_8003808C(GoolObject *obj, u32 op, u32 flags) {
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
        func_8001E3BC(obj, to, *ev, (op >> 21) & 7, n, argv);
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

s32 func_8001E34C(GoolObject *sender, u32 event, s32 type, s32 argc, u32 *argv);
/* Instruction: as func_80037EDC, but sends the event to all the objects of
 * type (op >> 21) & 7 (func_8001E34C). */

s32 func_80038248(GoolObject *obj, u32 op, u32 flags) {
    u32 argv[16];
    u32 *ev;
    s32 n;
    s32 i;

    ev = GoolRef(obj, op & 0xFFF);
    obj->status_a &= ~0x20000;
    if (*GoolReg(obj, (op >> 12) & 0x3F) != 0 && ev != NULL) {
        n = (op >> 18) & 7;
        for (i = 0; i < n; i++) {
            argv[i] = obj->sp[i - n];
        }
        func_8001E34C(obj, *ev, (op >> 21) & 7, n, argv);
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

s32 func_8001D5E8(GoolObject *obj, u32 *flags);
typedef struct {
    s32 state;
    s32 guard;
    s32 argc;
    u32 args[16];
} GoolStateRef;
/* Control flow instruction: on the condition (always, a word or its
 * negation) branches by a signed offset, returns from the frame with a
 * state change (op & 0x3FF, args popped into ref) or returns with state
 * 0xFF. flags (copied to the scratchpad) must have bit 8 set. */

s32 func_800383DC(GoolObject *obj, u32 op, u32 flags, GoolStateRef *ref) {
    s32 type;
    u32 cond;
    s32 c;
    s32 r;
    s32 argc;
    s32 i;

    type = (op >> 22) & 3;
    cond = (op >> 20) & 3;
    *(u32 *)0x1F800048 = flags;
    if (!(flags & 8)) {
        return -0xE;
    }
    switch (cond) {
    case 0:
        c = 1;
        break;
    case 1:
        c = *GoolReg(obj, (op >> 14) & 0x3F);
        break;
    case 2:
        c = !*GoolReg(obj, (op >> 14) & 0x3F);
        break;
    }
    if (c) {
        ref->guard = (op >> 24) != 0x3D;
        flags |= 0x20;
        *(u32 *)0x1F800048 = flags;
        switch (type) {
        case 2:
            r = func_8001D5E8(obj, NULL);
            if (r >= -0xFE) {
                return r;
            }
            ref->state = 0xFF;
            return -0xFF;
        case 1:
            argc = (op >> 10) & 0xF;
            for (i = argc - 1; i >= 0; i--) {
                ref->args[i] = *--obj->sp;
            }
            ref->argc = argc;
            r = func_8001D5E8(obj, NULL);
            if (r >= -0xFE) {
                return r;
            }
            ref->state = op & 0x3FF;
            return -0xFF;
        }
    } else if (type == 0) {
        obj->pc += ((s32)(op << 22) >> 22);
        obj->sp -= (op >> 10) & 0xF;
    }
    return 0;
}

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

s32 func_800450D4(u32 *a, u32 *b, RenderState *rs); /* nd_asm */
extern RenderState D_800676B0;
/* Instruction: pushes the result of func_800450D4 on two words. */

void func_800388C8(GoolObject *obj, u32 op) {
    u32 *b = GoolRef(obj, (op >> 12) & 0xFFF);
    u32 *a = GoolRef(obj, op & 0xFFF);

    *obj->sp++ = func_800450D4(b, a, &D_800676B0);
}

s32 func_8004F794(s32 x, s32 z); /* angle of (x, z) */
s32 func_8003D9DC(s32 x); /* square root */
/* Instruction: the angles (pitch, yaw) of a vector. */

void func_800389C8(GoolObject *obj, u32 op) {
    s32 *out = (s32 *)GoolRef(obj, (op >> 12) & 0xFFF);
    s32 *v = (s32 *)GoolRef(obj, op & 0xFFF);
    s32 x;
    s32 z;

    x = (v[0] < 0 ? -v[0] : v[0]) >> 8;
    z = (v[2] < 0 ? -v[2] : v[2]) >> 8;
    out[1] = func_8004F794(v[0], v[2]);
    out[0] = 0x400 - func_8004F794(v[1], func_8003D9DC(x * x + z * z) << 8);
    out[2] = 0;
}

/* x with its low bits sign-extended. */

s32 func_80038B2C(s32 x, s32 bits) {
    return (x << (32 - bits)) >> (32 - bits);
}
