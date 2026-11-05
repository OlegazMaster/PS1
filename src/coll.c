#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

/* Collision of objects with the surfaces of the zones and with other
 * objects. */

/* .sdata */
s32 D_800609C0 = 0xF424;

/* .sbss */
s32 D_80060AC8;
s32 D_80060ACC;
s32 D_80060AD0;
s32 D_80060AD4;
u32 *D_80060AD8; /* scratchpad 0x1F800180 */
u32 *D_80060ADC; /* scratchpad 0x1F800100: 32 rows of 32 bits */
u32 *D_80060AE0; /* scratchpad 0x1F800380 */
s32 D_80060AE4;

/* The surfaces near an object (see camera.c's SurfaceQuery) with a second
 * search box. */
typedef struct {
    u16 recs[0x200][4];
    s32 unk1000;
    s32 count; /* 0x1004 */
    s32 box[6]; /* 0x1008 */
    s32 box2[6]; /* 0x1020 */
    s32 floor; /* 0x1038: floor height found (0 if none) */
    s32 ceil_a; /* 0x103C: ceiling found (0 if none) */
    s32 ceil_b; /* 0x1040 */
    s32 floor_a; /* 0x1044 */
    s32 floor_b; /* 0x1048 */
    s32 floor_obj; /* 0x104C: top of an object under it */
    s32 steps; /* 0x1050: collision steps of the move */
} CollQuery;

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002AA94);

void func_8002CEA8(GoolObject *obj, CollQuery *q, s32 *pos, s32 *step, s32 *out);
/* Moves obj at pos by delta in steps of at most 25600 across and 153600 up
 * or down, colliding at each step. */

void func_8002AF70(GoolObject *obj, CollQuery *q, s32 *pos, s32 *delta) {
    s32 step[3];
    s32 next[3];
    s32 done[3];
    s32 n;
    s32 nx;
    s32 nz;
    s32 f;
    s32 ny;
    s32 k;
    s32 i;
    s32 any;
    s32 v;

    q->steps = 0;
    nx = (delta[0] < 0 ? -delta[0] : delta[0]) / 25600;
    nz = (delta[2] < 0 ? -delta[2] : delta[2]) / 25600;
    ny = (delta[1] < 0 ? -delta[1] : delta[1]) / 153600;
    n = nx;
    if (n < ny) {
        n = ny;
    }
    if (n < nz) {
        n = nz;
    }
    n++;
    for (i = 2; i >= 0; i--) {
        done[i] = 0;
    }
    for (k = 1; k <= n; k++) {
        any = 0;
        f = (k << 8) / n;
        for (i = 0; i < 3; i++) {
            v = (delta[i] * f) >> 8;
            step[i] = v - done[i];
            any |= step[i];
            done[i] = v;
        }
        if (any) {
            func_8002CEA8(obj, q, pos, step, next);
            for (i = 0; i < 3; i++) {
                pos[i] = next[i];
            }
        }
    }
}

/* What touching a surface of type flags does to obj: bits 4..9 give the
 * event it gets (0x700 ..., 0x900 deadly, ...), bits 1..3 its kind. Returns
 * whether the surface is solid. */

s32 func_8002B1CC(GoolObject *obj, u32 flags) {
    s32 kind;

    if (flags != 0) {
        kind = ((flags & 0xE) >> 1) + 1;
    } else {
        kind = 0;
    }
    switch ((flags & 0x3F0) >> 4) {
    case 1:
        obj->event = 0x700;
        obj->status_a |= 0x400;
        break;
    case 2:
        obj->event = 0xC00;
        obj->status_a |= 0x400;
        break;
    case 5:
        obj->event = 0x1E00;
        obj->status_a |= 0x400;
        break;
    case 4:
        obj->event = 0x1F00;
        obj->status_a |= 0x400;
        break;
    case 3:
        obj->event = 0x2100;
        obj->status_a |= 0x400;
        break;
    case 12:
        obj->event = 0x2300;
        obj->status_a |= 0x400;
        break;
    case 11:
        obj->event = 0x900;
        obj->status_a |= 0x400;
        return 1;
    case 6:
        obj->event = 0xD00;
        obj->status_a |= 0x400;
        break;
    case 13:
        obj->event = 0x3300;
        obj->status_a |= 0x400;
        break;
    case 18:
        obj->event = 0x2100;
        obj->status_a |= 0x400;
        break;
    case 14:
        if ((obj->state_flags >> 28) == 0xE) {
            obj->status_a |= 0x400;
        } else {
            obj->status_a |= 0x2000400;
        }
        obj->event = 0x3500;
        break;
    case 15:
        obj->status_a |= 0x4000000;
        break;
    case 7:
    case 8:
    case 9:
    case 10:
        obj->event = 0x1200;
        obj->status_a |= 0x2000;
        break;
    case 27:
        obj->event = 0x3000;
        obj->status_a |= 0x400;
        break;
    case 28:
        obj->event = 0x3100;
        obj->status_a |= 0x400;
        break;
    case 29:
    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
        obj->event = ((flags & 0x3F0) >> 4) + 0x40000;
        obj->status_a |= 0x400;
        break;
    }
    if (kind == 4) {
        obj->event = 0x900;
        obj->status_a |= 0x400;
        return 1;
    }
    if (kind == 6) {
        obj->event = 0x4000;
        obj->status_a |= 0x400;
        return 0;
    }
    if (kind == 5 || kind == 3) {
        return 1;
    }
    return 0;
}

s32 func_8002B1CC();
s32 func_8003BD18(GoolObject *obj, CollQuery *q, s32 *box, s32 *box2, s32 top, s32 a, s32 best, s32 (*cb)());
/* Searches the surfaces under obj moving from prev to pos: a box of
 * 0x9600 / div around pos, from 0x4B00 below to the object's height above. */

void func_8002B394(GoolObject *obj, s32 *prev, s32 *pos, CollQuery *q, s32 div, s32 a) {
    s32 top;
    s32 d;
    s32 lo;
    s32 hi;
    s32 *b;

    top = pos[1] + D_800609C0;
    d = prev[1] - pos[1];
    b = q->box2;
    if (d > 0) {
        top += d;
    }
    lo = -0x9600 / div;
    hi = 0x9600 / div;
    q->box2[0] = pos[0] + lo + 0xC80;
    b[1] = pos[1] - 0x4B00;
    b[2] = pos[2] + lo + 0xC80;
    b[3] = pos[0] + hi - 0xC80;
    b[4] = pos[1] + *(s32 *)((u8 *)obj->unk8 + 0x40);
    b[5] = pos[2] + hi - 0xC80;
    func_8003BD18(obj, q, q->box, b, top, a, -999999999, func_8002B1CC);
}

s32 func_8002B9A4(s32 *rec, s32 *box, s32 a, s32 b, s32 best, s32 top, s32 *c);
/* The top of the highest object (flag 0x20000) obj moving from prev to pos
 * can stand on, + 1 (-999999999 if none); collides with the last object
 * touched. */

s32 func_8002B4E8(GoolObject *obj, s32 *prev, s32 *pos, CollQuery *q) {
    s32 res;
    s32 top;
    s32 best;
    s32 d;
    s32 h;
    s32 i;
    u32 sf;
    s32 *b;
    s32 *rec;
    s32 *last;
    GoolObject *o;
    s32 t;
    u32 m;

    if (game.unk5C == 0) {
        return -999999999;
    }
    res = -999999999;
    best = -999999999;
    top = pos[1] + 0xF424;
    d = prev[1] - pos[1];
    if (d > 0) {
        top += d;
    }
    q->box2[0] = pos[0] - 0x4B00;
    b = q->box2;
    b[1] = pos[1];
    b[2] = pos[2] - 0x4B00;
    b[3] = pos[0] + 0x4B00;
    b[4] = pos[1] + *(s32 *)((u8 *)obj->unk8 + 0x40);
    b[5] = pos[2] + 0x4B00;
    q->steps += 0x10;
    last = NULL;
    for (i = 0; i < game.unk5C; i++) {
        rec = game.unk390[i];
        o = (GoolObject *)rec[1];
        t = -999999999;
        if ((o->status_b & 0x400000) != 1) {
            t = top;
        }
        h = func_8002B9A4(rec, b, 1, 1, best, t, (o->status_b & 0x200) ? o->misc_c : NULL);
        if (h != -999999999) {
            if ((o->status_b & 0x20000)
                && ((((!(obj->state_flags & 0x10) && obj->invincibility_state != 5)
                      || ((GoolObjectType(o) != 0x300 || (o->status_c & 0x412)) && !(o->state_flags & 0x20)))
                     && !(obj->state_flags & 0x4000))
                    || (o->state_flags & 0x4000))) {
                sf = obj->state_flags;
                m = sf & 0x280000;
                if (m != 0x80000 || (o->status_c & m)
                    || ((sf & 8) ? (o->state_flags & 0x4000000) : (o->state_flags & 0x80))) {
                    res = h + 1;
                    o->status_a |= 0x4000;
                    best = h;
                    *(GoolObject **)((u8 *)obj->unk8 + 0x48) = o;
                }
            }
            last = rec;
            q->steps += 0x1000;
        }
    }
    if (last != NULL) {
        func_8001B328(obj, b, (GoolObject *)last[1], (last[0] & 7) == 0 ? last + 2 : NULL);
    }
    return res;
}

s32 func_8003191C(s32 *a, s32 *b); /* whether two boxes overlap */
/* Collides the box of obj at pos with the objects of the collision records. */

s32 func_8003191C(s32 *a, s32 *b); /* whether two boxes overlap */
s32 func_8001B328(GoolObject *obj, s32 *box, GoolObject *other, s32 *other_box);

s32 func_8003191C(s32 *a, s32 *b); /* whether two boxes overlap */

s32 func_8003191C(s32 *a, s32 *b); /* whether two boxes overlap */

s32 func_8003191C(s32 *a, s32 *b); /* whether two boxes overlap */

s32 func_8003191C(s32 *a, s32 *b); /* whether two boxes overlap */

s32 func_8002B838(GoolObject *obj, s32 *pos, CollQuery *q) {
    s32 *box;
    s32 *rec;
    s32 *rbox;
    GoolObject *other;
    s32 i;

    box = q->box2;
    if (game.unk5C == 0) {
        return 0;
    }
    q->box2[0] = pos[0] + *(s32 *)((u8 *)obj->unk8 + 0x30);
    box[1] = pos[1];
    box[2] = pos[2] + *(s32 *)((u8 *)obj->unk8 + 0x38);
    box[3] = pos[0] + *(s32 *)((u8 *)obj->unk8 + 0x3C);
    box[4] = pos[1] + *(s32 *)((u8 *)obj->unk8 + 0x40);
    box[5] = pos[2] + *(s32 *)((u8 *)obj->unk8 + 0x44);
    for (i = 0; i < game.unk5C; i++) {
        rec = game.unk390[i];
        rbox = rec + 2;
        other = (GoolObject *)rec[1];
        if (func_8003191C(box, rbox)) {
            func_8001B328(obj, box, other, (rec[0] & 7) == 0 ? rbox : NULL);
        }
    }
    return 0;
}

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002B9A4);

s32 func_800319B8(s32 *a, s32 *b); /* whether box a lies inside box b */
s32 func_8002A7A8(s32 *pos, GoolObject *obj, CollQuery *q);
s32 func_8002B4E8(GoolObject *obj, s32 *prev, s32 *pos, CollQuery *q);
#ifdef NON_MATCHING
/* The floor under obj moving from prev to pos: collects the surfaces near it
 * (again when it left the searched box), finds the highest floor of the zone
 * surfaces and of the objects, puts obj on it and sets its landing flags.
 * Returns the floor height + 1 (or -999999999 if none).
 * Equivalent; the original allocates fa and fb to the other registers. */
s32 func_8002C230(GoolObject *obj, s32 *prev, s32 *pos, CollQuery *q, s32 *top) {
    s32 found[10];
    s32 *b;
    s32 fa;
    s32 fb;
    s32 fo;
    s32 lift;
    u32 flags;
    s32 y;
    s32 h;
    s32 v;
    GoolObject *other;

    q->box2[0] = pos[0] - 0x9600;
    b = q->box2;
    b[1] = pos[1];
    b[2] = pos[2] - 0x9600;
    b[3] = pos[0] + 0x9600;
    lift = 0;
    b[4] = pos[1] + *(s32 *)((u8 *)obj->unk8 + 0x40);
    b[5] = pos[2] + 0x9600;
    flags = 0x40001;
    if (q->unk1000 == 0 || !func_800319B8(b, q->box)) {
        func_8002A7A8(pos, obj, q);
    }
    func_8002B394(obj, prev, pos, q, 8, (s32)found);
    fb = found[1];
    fa = found[2];
    if ((obj->state_flags >> 28) == 2) {
        fb = -999999999;
        fa = -999999999;
    }
    if (fa == -999999999) {
        q->floor_a = 0;
    } else {
        q->floor_a = fa;
    }
    if (fb == -999999999) {
        q->floor_b = 0;
    } else {
        q->floor_b = fb;
    }
    fo = func_8002B4E8(obj, prev, pos, q);
    if (fo == -999999999) {
        q->floor_obj = 0;
    } else if (fa == -999999999 || fo >= fa) {
        fa = fo;
        q->floor_obj = fa;
        other = obj->collider;
        flags &= ~0x40000;
        if (other != NULL) {
            if (other->status_b & 0x400000) {
                lift = 0x19000;
            }
            if (obj->update_stamp - obj->unk110 >= 4) {
                flags = 0x4001;
            }
        }
        flags |= 0x200000;
    }
    if ((fa == -999999999 && fb == fa) || obj->misc_a[1] > 0) {
        if (obj->misc_a[1] > 0) {
            obj->status_a &= 0xFFDBBFFE;
        }
        q->floor = 0;
        y = -999999999;
    } else {
        v = fa;
        if (fa == -999999999) {
            v = fb;
            flags = 1;
        }
        q->floor = v;
        y = v + 1;
        if (fa == -999999999) {
            h = obj->xform.trans[1];
        } else {
            h = obj->xform.trans[1] + D_800609C0 + lift;
        }
        *top = h;
        if (h < y) {
            y = prev[1];
            b[1] = y;
        } else {
            if (y >= pos[1]) {
                if (flags & 0x200000) {
                    other = *(GoolObject **)((u8 *)obj->unk8 + 0x48);
                    if (other != NULL) {
                        other->status_a |= 0x4000;
                    }
                }
            } else {
                y = pos[1];
            }
            b[1] = y;
        }
        b[4] = y + *(s32 *)((u8 *)obj->unk8 + 0x40);
        pos[1] = y;
        if (q->unk1000 == 0 || !func_800319B8(b, q->box)) {
            func_8002A7A8(pos, obj, q);
        }
        v = obj->misc_a[1];
        if (v < 0 && (flags & 1)) {
            obj->unk114 = v;
            obj->misc_a[1] = 0;
        }
        obj->status_a |= flags;
        obj->unk110 = game.frames;
    }
    return y;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002C230);
#endif

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002C604);

s32 func_8002C604(GoolObject *obj, s32 *to, CollQuery *q, s32 *ceil, s32 flag);
s32 func_8001FD08(s32 cur, s32 target, s32 step);
#define ALL_FLAG(b, i) ((b)[i] & 0x100000)
/* Keeps obj moving from pos to to under the ceiling: stops it below the
 * ceiling found by func_8002C604 (in state 2 it slides there and gets
 * event 0x3300). */






#ifdef NON_MATCHING
/* Equivalent; the original reloads to[1] and orders two loads differently. */
void func_8002CAE4(GoolObject *obj, s32 *pos, s32 *to, CollQuery *q) {
    s32 ceil;
    u32 arg;
    u32 flag;
    s32 c;
    s32 *b;
    s32 *pc;
    s32 y;
    s32 h;
    s32 lo;
    s32 lim;
    s32 v;
    s32 top;

    flag = 0;
    pc = &ceil;
    if (obj->status_a & 0x100000) {
        b = (s32 *)obj->unk8;
        if (ALL_FLAG(b, 19) && ALL_FLAG(b, 20) && ALL_FLAG(b, 21) && ALL_FLAG(b, 22) && ALL_FLAG(b, 23) && ALL_FLAG(b, 24) && ALL_FLAG(b, 25)) {
            flag = flag < (u32)ALL_FLAG(b, 26);
        }
    }
    c = func_8002C604(obj, to, q, pc, flag);
    if (c == -999999999) {
        q->ceil_a = 0;
        q->ceil_b = 0;
    } else {
        q->ceil_a = c;
        q->ceil_b = ceil;
    }
    if (c != -999999999) {
        y = to[1];
        h = *(s32 *)((u8 *)obj->unk8 + 0x40);
        top = y + h;
        lo = ceil;
        if (c < lo) {
            lo = c;
        }
        if (lo - 1 < top && !(obj->status_a & 1)) {
            lim = c - h - 1;
            if ((obj->state_flags >> 28) == 2) {
                v = y;
                if (lim < v) {
                    v = lim;
                }
                to[1] = func_8001FD08(pos[1], v, 0x4E20);
            } else {
                v = pos[1];
                if (v >= y) {
                    v = y;
                }
                if (v < lim) {
                    v = lim;
                }
                to[1] = v;
                if (ceil < to[1] + *(s32 *)((u8 *)obj->unk8 + 0x40)) {
                    to[1] = ceil - *(s32 *)((u8 *)obj->unk8 + 0x40) - 1;
                }
            }
            obj->status_a |= 0x80;
            if (obj->misc_a[1] > 0) {
                obj->misc_a[1] = 0;
            }
            return;
        }
    }
    if ((obj->state_flags >> 28) == 2) {
        if (c == -999999999) {
            to[1] = pos[1];
            return;
        }
        arg = 0;
        to[1] = func_8001FD08(pos[1], c - *(s32 *)((u8 *)obj->unk8 + 0x40) - 1, 0x4E20);
        GoolSendEvent(NULL, obj, 0x3300, 1, &arg);
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002CAE4);
#endif

Entry *func_80028D8C(Entry *zone, s32 *pos);
/* Finds the zone obj at pos is in; if there is none and pos is below the
 * floor of its zone, either sends it event 0x900 (zones whose floor kills)
 * or puts it on the floor. */

s32 func_8002CD8C(GoolObject *obj, s32 *pos) {
    Entry *zone;
    s32 *box;
    s32 land;
    u32 arg;
    s32 v;

    zone = func_80028D8C((Entry *)D_80065C90.unkCC, pos);
    if ((s32)zone >= -0xFE) {
        box = (s32 *)obj->zone->items[1];
        if (pos[1] < (box[1] << 8) - *(s32 *)((u8 *)obj->unk8 + 0x34)) {
            land = 1;
            if ((((ZoneHdr *)obj->zone->items[0])->unk2DC & 2) && obj->invincibility_state != 2) {
                arg = 0x6400;
                GoolSendEvent(NULL, obj, 0x900, 1, &arg);
                land = 0;
            }
            if (land) {
                pos[1] = (box[1] << 8) - *(s32 *)((u8 *)obj->unk8 + 0x34);
                v = obj->misc_a[1];
                obj->misc_a[1] = 0;
                obj->status_a |= 1;
                obj->unk114 = v;
                obj->unk110 = game.frames;
            }
        }
    } else {
        obj->zone = zone;
    }
    return -0xFF;
}

s32 func_8002C230(GoolObject *obj, s32 *prev, s32 *pos, CollQuery *q, s32 *top);
s32 func_8002F0F8(s32 *pos, s32 gx, s32 gz, s32 *cx, s32 *cz, GoolObject *obj, s32 a);
void func_8002CAE4(GoolObject *obj, s32 *pos, s32 *to, CollQuery *q);
/* One collision step of obj from pos by step: collides with the surfaces,
 * keeps it on a free cell of the grid around it and puts the result in out. */

void func_8002EC2C(s32 *pos, s32 *prev, GoolObject *obj, CollQuery *q);

#ifdef NON_MATCHING
/* Equivalent; the original schedules the loads of cx and cz differently. */
void func_8002CEA8(GoolObject *obj, CollQuery *q, s32 *pos, s32 *step, s32 *out) {
    s32 to[3];
    s32 d[3];
    s32 snap[3];
    s32 hit;
    s32 cx;
    s32 cz;
    s32 gx;
    s32 gz;
    s32 v;
    s32 i;

    for (i = 0; i < 3; i++) {
        d[i] = step[i];
    }
    for (i = 0; i < 3; i++) {
        to[i] = pos[i] + d[i];
    }
    func_8002C230(obj, pos, to, q, &hit);
    func_8002EC2C(pos, to, obj, q);
    gx = ((to[0] - pos[0]) << 2) / 0x2000 + 16;
    gz = ((to[2] - pos[2]) << 2) / 0x2000 + 16;
    if (func_8002F0F8(pos, gx, gz, &cx, &cz, obj, 0)) {
        v = (((cx - 16) << 13) >> 2) + pos[0];
        snap[0] = v;
        to[0] = v;
        v = (((cz - 16) << 13) >> 2) + pos[2];
        snap[2] = v;
    } else {
        to[0] = pos[0];
        v = pos[2];
    }
    to[2] = v;
    q->steps++;
    if ((gx != 16 || gz != gx) && (gx != cx || gz != cz)) {
        obj->status_a |= 0x100;
    }
    func_8002CAE4(obj, pos, to, q);
    func_8002CD8C(obj, to);
    for (i = 0; i < 3; i++) {
        out[i] = to[i];
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002CEA8);
#endif

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002D0E8);

/* Drops the last collision record. */

void func_8002D85C(void) {
    u8 *end;

    if (game.unk5C != 0) {
        game.unk5C--;
        end = game.unk58;
        switch (*(u32 *)game.unk390[game.unk5C] & 7) {
        case 0:
            end -= 0x20;
            break;
        case 1:
            end -= 0x44;
            break;
        }
        game.unk58 = end;
    }
}

/* Clears the list of collision records. */

void func_8002D8C8(void) {
    game.unk58 = (u8 *)&game + 0x4B0;
    game.unk5C = 0;
}

extern u32 D_8005C884[];
/* Puts the collision tables in the scratchpad. */

s32 func_8002D8E0(void) {
    s32 i;

    D_80060ADC = (u32 *)0x1F800100;
    D_80060AD8 = (u32 *)0x1F800180;
    D_80060AE0 = (u32 *)0x1F800380;
    for (i = 0; i < 32; i++) {
        ((u32 *)0x1F800380)[i] = D_8005C884[i];
    }
    return -0xFF;
}

s32 func_8002D93C(void) {
    return -0xFF;
}

void func_8003BBF0(s32 x, s32 z);

void func_8002D944(s32 x, s32 z) {
    if ((x < 0 ? -x : x) < 32 && (z < 0 ? -z : z) < 32) {
        func_8003BBF0(x, z);
    }
}

/* Sets (set) or clears the cells x0 + 16 .. x1 + 16, y0 + 16 .. y1 + 16 of the
 * 32x32 grid D_80060ADC (bit 31 of a row is column 0). */

void func_8002D990(s32 x0, s32 y0, s32 x1, s32 y1, s32 set) {
    u32 *row;
    u32 mask = -1;

    x1 += 16;
    if (x1 < 32) {
        if (x1 <= 0) {
            return;
        }
        mask = -1 << (32 - x1);
    }
    x0 += 15;
    if (x0 >= 32) {
        return;
    }
    if (x0 >= 0) {
        mask &= ~((s32)0x80000000 >> x0);
    }
    if (mask == 0) {
        return;
    }
    y0 += 16;
    if (y0 < 0) {
        y0 = 0;
    }
    y1 += 16;
    if (y1 > 32) {
        y1 = 32;
    }
    if (y0 >= 32) {
        return;
    }
    row = &D_80060ADC[y0];
    if (set) {
        for (; y0 < y1; y0++) {
            *row++ |= mask;
        }
    } else {
        mask = ~mask;
        for (; y0 < y1; y0++) {
            *row++ &= mask;
        }
    }
}

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002DA74);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002E4F0);

extern u32 D_80067A28[];
void func_8003B874(CollQuery *q, s32 *box, s32 mode, s32 y0, s32 y1, s32 top, s32 x, s32 z);
void func_8002F678(s32 *pos, s32 a, s32 b);
void func_8002E4F0(s32 *pos, s32 *prev, GoolObject *obj, CollQuery *q, s32 a);
/* Prepares the collision of obj moving from prev to pos: clears the grids,
 * collects the surfaces around it and resolves the move. */

void func_8002EC2C(s32 *pos, s32 *prev, GoolObject *obj, CollQuery *q) {
    s32 i;
    s32 mode;
    s32 y;
    s32 *box;
    s32 unused[12]; /* takes stack space in the original */

    if (!(((ZoneHdr *)((Entry *)D_80065C90.unkCC)->items[0])->unk2DC & 0x100000)) {
        for (i = 31; i >= 0; i--) {
            D_80060ADC[i] = 0;
        }
        for (i = 127; i >= 0; i--) {
            D_80060AD8[i] = 0;
        }
        box = q->box;
        mode = obj->status_c;
        if ((u32)(obj->invincibility_state - 2) < 3) {
            mode = 2;
        } else {
            mode &= 2;
        }
        y = obj->xform.trans[1];
        func_8003B874(q, box, mode, y + D_800609C0, y + 0x3200, pos[1] + *(s32 *)((u8 *)obj->unk8 + 0x40), pos[0], pos[2]);
        if ((game.display_flags & 0x4000000) && (D_80067A28[0] & 0x20000)) {
            if (!(D_80067A28[0] & 0x40000) || (obj->state_flags & 0x2000000)) {
                func_8002F678(pos, 0, 0x40);
            }
        }
    }
    func_8002E4F0(pos, prev, obj, q, 1);
}

/* Whether cell (x, y) of the 32x32 grid D_80060ADC is free. */

s32 func_8002EDA8(u32 x, u32 y) {
    if (x < 32 && y < 32 && !(D_80060ADC[y] & (0x80000000 >> x))) {
        return 1;
    }
    return 0;
}

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002EDF0);

extern s8 D_8005C074[]; /* (dx, dz) offsets of the cells around a cell, nearest first */
extern CollQuery D_80067E40;
s32 func_8002EDF0(s32 a, s32 b, s32 *pos, GoolObject *obj);
#ifdef NON_MATCHING
/* Finds the free cell of the 32x32 grid nearest to (gx, gz) and puts it in
 * (cx, cz); restarts from the centre, then pushes obj out, when there is
 * none. Returns tries + 2, or 0 if it gave up.
 * Equivalent; the original keeps tries in a register and gz on the stack. */
s32 func_8002F0F8(s32 *pos, s32 gx, s32 gz, s32 *cx, s32 *cz, GoolObject *obj, s32 tries) {
    s8 *off;
    s32 i;
    s32 a;
    s32 b;
    s32 x;
    s32 z;
    s32 z2;
    u32 *row;

loop:
    off = D_8005C074;
    if (func_8002EDA8(gx, gz) || (((ZoneHdr *)((Entry *)D_80065C90.unkCC)->items[0])->unk2DC & 0x100000)) {
        *cx = gx;
        *cz = gz;
        return tries + 2;
    }
    for (i = 0; i < 0x98; i++) {
        a = off[0];
        b = off[1];
        off += 2;
        x = gx + a;
        z = gz + b;
        if (func_8002EDA8(x, z)) {
            goto found;
        }
        z2 = gz - b;
        if (func_8002EDA8(x, z2)) {
            goto found2;
        }
        x = gx - a;
        if (func_8002EDA8(x, z)) {
            goto found;
        }
        if (func_8002EDA8(x, z2)) {
            goto found2;
        }
        if (a != b) {
            x = gx + b;
            z = gz + a;
            if (func_8002EDA8(x, z)) {
                goto found;
            }
            z2 = gz - a;
            if (func_8002EDA8(x, z2)) {
                goto found2;
            }
            x = gx - b;
            if (func_8002EDA8(x, z)) {
                goto found;
            }
            if (func_8002EDA8(x, z2)) {
                goto found2;
            }
        }
    }
    if ((gx != 16 || gz != gx) && (obj->collider == NULL || (obj->collider->id >> 16) != 0x22)) {
        row = D_80060ADC;
        for (i = 0; i < 32; i++) {
            if (*row != -1) {
                gx = 16;
                gz = gx;
                tries++;
                goto loop;
            }
            row++;
        }
    }
    if (tries < 0x100 && (obj->collider == NULL || (obj->collider->id >> 16) != 0x22)) {
        obj->status_a |= 0x8000;
        if (func_8002EDF0(0, 0, pos, obj)) {
            func_8002EDF0(1, 1, pos, obj);
            func_8002E4F0(pos, pos, obj, &D_80067E40, 0);
        }
        tries = 0x100;
        goto loop;
    }
    obj->status_a |= 0x100000;
    *cx = 16;
    *cz = 16;
    return 0;
found:
    *cx = x;
    *cz = z;
    return tries + 2;
found2:
    *cx = x;
    *cz = z2;
    return tries + 2;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002F0F8);
#endif
