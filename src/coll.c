#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

Entry *func_80028D8C(Entry *zone, s32 *pos);

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

extern GoolObject *crash;
void func_8003F520(s32 *out, s32 *in, void *model, s32 *pos, s32 n, s32 shift); /* turns n points by a model's matrix */
extern s32 D_8006C2C8[3]; /* the last move that did not move the object */
s32 func_8004F794(s32 x, s32 z); /* angle of (x, z) */
s32 func_80031CF0(s32 *a, s32 *b); /* distance between two points */
s32 func_8003C2F0(s32 angle); /* sin */
s32 func_8003C364(s32 angle); /* cos */
s32 func_8001E34C(GoolObject *sender, u32 event, s32 type, s32 argc, u32 *argv);
void func_80027D6C(s32 type, s32 *args);
void func_8002AF70(GoolObject *obj, CollQuery *q, s32 *pos, s32 *delta);
#define ABS_DIFF(a, b) ((a) - (b) >= 0 ? (a) - (b) : (b) - (a))
#ifdef NON_MATCHING
/* Moves obj by delta, colliding with the zone and the objects (q), and sends
 * it the event of the surface it touched. A move that changes nothing is
 * remembered and not tried again.
 * Equivalent; the original allocates three registers differently. */
void func_8002AA94(GoolObject *obj, s32 *delta, CollQuery *q) {
    s32 pos[3];
    s32 d[3];
    s32 msg;
    u32 arg;
    Transform *t;
    s32 a;
    s32 b;
    s32 f;
    s32 speed;
    u32 ev;
    s32 type;

    if (obj == NULL) {
        D_80060AC8 = 0;
        return;
    }
    *(s32 *)((u8 *)obj->unk8 + 0x48) = 0;
    if (ns.ldat->lid == 10 || ns.ldat->lid == 17) {
        if (obj->state_flags & 0x2000000) {
            D_800609C0 = 0x27AC4;
        } else {
            D_800609C0 = 0xF424;
        }
    } else {
        D_800609C0 = 0xF424;
    }
    t = &obj->xform;
    pos[0] = t->trans[0];
    pos[1] = t->trans[1];
    pos[2] = t->trans[2];
    d[0] = delta[0];
    d[1] = delta[1];
    d[2] = delta[2];
    if (D_80060AC8 != 0 && ABS_DIFF(D_8006C2C8[0], delta[0]) < 10 && ABS_DIFF(D_8006C2C8[1], delta[1]) < 10
        && ABS_DIFF(D_8006C2C8[2], delta[2]) < 10) {
        d[0] = d[2] = 0;
    }
    func_8002AF70(obj, q, pos, d);
    if (D_80060AC8 == 0 && (delta[0] != 0 || delta[2] != 0) && ABS_DIFF(t->trans[0], pos[0]) < 2
        && ABS_DIFF(t->trans[1], pos[1]) < 2 && ABS_DIFF(t->trans[2], pos[2]) < 2) {
        D_8006C2C8[0] = delta[0];
        D_8006C2C8[1] = delta[1];
        D_80060AC8 = 1;
        D_8006C2C8[2] = delta[2];
    } else {
        D_80060AC8 = 0;
    }
    if ((d[0] != 0 || d[2] != 0) && (obj->status_a & 0x100)) {
        a = func_8004F794(delta[0], delta[2]) & 0xFFF;
        b = func_8004F794(pos[0] - t->trans[0], pos[2] - t->trans[2]) & 0xFFF;
        f = func_80031CF0(t->trans, pos) * 15;
        a = ((a - b) << 20) >> 20;
        if (a < 0) {
            a = -a;
        }
        speed = f * 2;
        if (a > 0x3F4) {
            speed = 0;
        }
        obj->misc_a[0] = ((func_8003C2F0(b) >> 4) * speed) >> 8;
        obj->misc_a[2] = ((func_8003C364(b) >> 4) * speed) >> 8;
    }
    t->trans[0] = pos[0];
    t->trans[1] = pos[1];
    t->trans[2] = pos[2];
    if (obj->status_a & 0x400) {
        ev = obj->event;
        if (ev >= 0x40000) {
            if (ev > 0x40024) {
                msg = (ev - 0x40024) << 8;
                type = 0x100;
            } else {
                msg = 0x100 << (ev - 0x4001D);
                type = 0;
            }
            func_80027D6C(type, &msg);
            return;
        }
        arg = 0x6400;
        switch (obj->event) {
        case 0x900:
            if (!(obj->status_a & 1)) {
                GoolSendEvent(NULL, obj, obj->event, 1, &arg);
            }
            break;
        case 0x3000:
        case 0x3100:
            func_8001E34C(NULL, obj->event, 0, 0, NULL);
            break;
        case 0x1F00:
        default:
            GoolSendEvent(NULL, obj, obj->event, 1, &arg);
            break;
        }
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002AA94);
#endif

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
s32 func_8001B328(GoolObject *obj, s32 *box, GoolObject *other, s32 *other_box);
/* Collides the box of obj at pos with the objects of the collision records. */

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

extern u32 D_8005C824[6][4]; /* the corners of the 6 faces of a box */
s32 func_8003116C(s32 *pt, s32 *quad); /* whether pt is inside the quad (x, z pairs) */
s32 func_80030F88(s32 x, s32 z, s32 *quad); /* height of the quad (x, y, z) at (x, z) */
#ifdef NON_MATCHING
/* The height of the top (up) or the bottom (!up) of the collision record
 * rec where box lies over or under it, between best and top (each
 * -999999999 for no limit); with vert, it must also be inside the height of
 * box. For a turned box (kind 1), c gets the lowest corner of the face
 * found. -999999999 if none.
 * Equivalent; the original allocates the registers differently. */
s32 func_8002B9A4(s32 *rec, s32 *box, s32 up, s32 vert, s32 best, s32 top, s32 *c) {
    s32 pts[8][3];
    s32 *q[4];
    s32 quad[4][3];
    s32 flat[4][2];
    s32 pt[2];
    s32 i;
    s32 k;
    s32 j;
    u32 *face;
    s32 m;
    s32 cross;
    s32 cx;
    s32 cy;
    s32 cz;
    s32 h;
    s32 y;
    s32 *a;
    s32 *b;
    s32 *d;

    switch (rec[0] & 7) {
    case 0:
        m = 0;
        if (*(u32 *)((u8 *)crash->unk8 + 0x4C) & 1) {
            m = ((GoolObject *)rec[1])->hotspot_size;
        }
        if (vert && (box[1] > rec[6] || box[4] < rec[3])) {
            break;
        }
        if (up) {
            if (best != -999999999 && rec[6] < best) {
                break;
            }
            if (top != -999999999 && top < rec[6]) {
                break;
            }
        } else {
            if (best != -999999999 && best < rec[3]) {
                break;
            }
            if (top != -999999999 && rec[3] < top) {
                break;
            }
        }
        if (box[3] - m < rec[2] || box[0] + m > rec[5] || box[5] - m < rec[4] || box[2] + m > rec[7]) {
            break;
        }
        if (up) {
            return rec[6];
        }
        return rec[3];
    case 1:
        if (vert && (box[1] > rec[15] || box[4] < rec[12])) {
            break;
        }
        if (up) {
            if (best != -999999999 && rec[15] < best) {
                break;
            }
            if (top != -999999999 && top < rec[12]) {
                break;
            }
        } else {
            if (best != -999999999 && best < rec[12]) {
                break;
            }
            if (top != -999999999 && rec[15] < top) {
                break;
            }
        }
        if (box[3] < rec[11] || box[0] > rec[14] || box[5] < rec[13] || box[2] > rec[16]) {
            break;
        }
        pts[0][0] = pts[1][0] = pts[4][0] = pts[5][0] = rec[2];
        pts[2][0] = pts[3][0] = pts[6][0] = pts[7][0] = rec[5];
        pts[0][1] = pts[3][1] = pts[4][1] = pts[7][1] = rec[3];
        pts[1][1] = pts[2][1] = pts[5][1] = pts[6][1] = rec[6];
        pts[0][2] = pts[1][2] = pts[2][2] = pts[3][2] = rec[4];
        pts[4][2] = pts[5][2] = pts[6][2] = pts[7][2] = rec[7];
        func_8003F520(pts[0], pts[0], ((GoolObject *)rec[1])->unk8, rec + 8, 8, 7);
        face = D_8005C824[0];
        for (i = 0; i < 6; i++) {
            q[0] = pts[*face++];
            q[1] = pts[*face++];
            q[2] = pts[*face++];
            q[3] = pts[*face++];
            cross = ((q[1][2] - q[0][2]) >> 7) * ((q[2][0] - q[0][0]) >> 7);
            cross -= ((q[1][0] - q[0][0]) >> 7) * ((q[2][2] - q[0][2]) >> 7);
            if (up ? cross >= 0 : cross <= 0) {
                continue;
            }
            cx = (q[0][0] + q[1][0] + q[2][0] + q[3][0]) >> 2;
            cz = (q[0][2] + q[1][2] + q[2][2] + q[3][2]) >> 2;
            for (k = 0; k < 4; k++) {
                a = q[k];
                flat[k][0] = (a[0] - cx) >> 7;
                flat[k][1] = (a[2] - cz) >> 7;
            }
            pt[0] = (((box[0] + box[3]) >> 1) - cx) >> 7;
            pt[1] = (((box[2] + box[5]) >> 1) - cz) >> 7;
            if (!func_8003116C(pt, flat[0])) {
                continue;
            }
            cy = (q[0][1] + q[1][1] + q[2][1] + q[3][1]) >> 2;
            for (k = 0; k < 4; k++) {
                quad[k][0] = flat[k][0];
                quad[k][2] = flat[k][1];
                quad[k][1] = (q[k][1] - cy) >> 7;
            }
            h = func_80030F88(pt[0], pt[1], quad[0]);
            if (h == -999999999) {
                continue;
            }
            h = (h << 7) + cy;
            if (vert && (box[4] < h || h < box[1])) {
                break;
            }
            if (up) {
                if (best != -999999999 && h < best) {
                    break;
                }
                if (top != -999999999 && top < h) {
                    break;
                }
                if (c != NULL) {
                    j = 0;
                    y = q[0][1];
                    for (i = 1; i < 4; i++) {
                        if (q[i][1] < y) {
                            y = q[i][1];
                            j = i;
                        }
                    }
                    c[1] = y - cy;
                    a = q[j];
                    b = q[(j + 1) & 3];
                    d = q[(j - 1) & 3];
                    if (b[1] == y) {
                        if (d[1] == y) {
                            c[0] = 0;
                            c[2] = 0;
                        } else {
                            c[0] = ((a[0] + b[0]) >> 1) - cx;
                            c[2] = ((a[2] + b[2]) >> 1) - cz;
                        }
                    } else if (d[1] == y) {
                        c[0] = ((a[0] + d[0]) >> 1) - cx;
                        c[2] = ((a[2] + d[2]) >> 1) - cz;
                    } else {
                        c[0] = a[0] - cx;
                        c[2] = a[2] - cz;
                    }
                }
            } else {
                if (best != -999999999 && best < h) {
                    break;
                }
                if (top != -999999999 && h < top) {
                    break;
                }
            }
            return h;
        }
        break;
    }
    return -999999999;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002B9A4);
#endif

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

s32 func_8003C04C(GoolObject *obj, CollQuery *q, s32 *box, s32 *box2, s32 *ceil2, s32 best, s32 a);
#define OBJ_H(obj) (*(s32 *)((u8 *)(obj)->unk8 + 0x40))
#ifdef NON_MATCHING
/* The ceiling over obj at pos: the lowest object over it (flag 0x8000000,
 * which then gets event 0x1700) or surface, or the top of the zone; sets
 * flag 0x80 of obj when there is one. Returns -999999999 if there is none
 * low enough.
 * Equivalent; the original allocates the registers and stack slots differently. */
s32 func_8002C604(GoolObject *obj, s32 *pos, CollQuery *q, s32 *ceil2, s32 flag) {
    s32 p[3];
    u32 arg;
    s32 best;
    s32 *b;
    s32 half;
    s32 quarter;
    s32 y;
    s32 i;
    s32 c;
    s32 top;
    s32 *rec;
    s32 *zb;
    GoolObject *hit;
    GoolObject *o;

    hit = NULL;
    best = -999999999;
    b = q->box2;
    if ((obj->state_flags >> 28) == 2) {
        q->box2[0] = pos[0] - 0x4B00;
        b[1] = pos[1] - OBJ_H(obj) / 4;
        b[2] = pos[2] - 0x4B00;
        b[3] = pos[0] + 0x4B00;
        b[4] = pos[1] + OBJ_H(obj) * 2;
    } else {
        q->box2[0] = pos[0] - 0x4B00;
        if (flag) {
            y = pos[1] + OBJ_H(obj) - OBJ_H(obj) / 8;
        } else {
            half = OBJ_H(obj) / 2;
            quarter = OBJ_H(obj) / 4;
            if (half > 0x19000) {
                half = 0x19000;
            }
            y = pos[1] + OBJ_H(obj) - (half < quarter ? quarter : half);
        }
        b[1] = y;
        b[2] = pos[2] - 0x4B00;
        b[3] = pos[0] + 0x4B00;
        b[4] = pos[1] + OBJ_H(obj);
    }
    b[5] = pos[2] + 0x4B00;
    for (i = 0; i < game.unk5C; i++) {
        rec = game.unk390[i];
        o = (GoolObject *)rec[1];
        if ((o->status_b & 0x8000000) && !(o->status_a & 0x4000)) {
            c = func_8002B9A4(rec, b, 0, 1, best, -999999999, NULL);
            if (c != -999999999) {
                best = c;
                hit = o;
            }
        }
    }
    q->box2[0] = pos[0] - 0x2580;
    b[2] = pos[2] - 0x2580;
    b[3] = pos[0] + 0x2580;
    b[5] = pos[2] + 0x2580;
    b[4] = pos[1] + OBJ_H(obj) * 5 / 4;
    *ceil2 = 0x7FFFFFFF;
    c = func_8003C04C(obj, q, q->box, b, ceil2, -999999999, 1);
    if (((ZoneHdr *)obj->zone->items[0])->unk2DC & 0x20000) {
        p[0] = pos[0];
        p[1] = b[4];
        p[2] = pos[2];
        if ((s32)func_80028D8C((Entry *)D_80065C90.unkCC, p) >= -0xFE) {
            zb = (s32 *)obj->zone->items[1];
            top = (zb[1] + zb[4]) << 8;
            if (top < p[1]) {
                c = top;
            }
        }
    }
    if (c != -999999999) {
        obj->status_a |= 0x80;
    }
    if (best != -999999999 && (c == -999999999 || best < c)) {
        hit->status_a |= 0x80;
        obj->status_a |= 0x80;
        func_8001B328(obj, NULL, hit, NULL);
        if (obj->misc_a[1] >= 0 || hit->misc_a[1] < 0) {
            arg = 0x6400;
            GoolSendEvent(obj, hit, 0x1700, 1, &arg);
        }
        if (!(obj->state_flags & 0x80000)) {
            return best;
        }
    }
    if ((obj->state_flags >> 28) == 2 || pos[1] + OBJ_H(obj) >= c) {
        return c;
    }
    return -999999999;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002C604);
#endif

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

#define ABS(x) ((x) < 0 ? -(x) : (x))
/* A collision box of a model (0x28 bytes): kind (bits 0..2), the offset of
 * its centre and the box around it (in 1/256 units of the scale). */
typedef struct {
    s32 v[6];
} Box6;
typedef struct {
    u32 flags;
    s32 off[3]; /* 0x4 */
    Box6 box; /* 0x10 */
} CollPart;
typedef struct {
    u8 unk0[0x14];
    u32 count; /* 0x14 */
    u8 unk18[0x4];
    CollPart parts[1]; /* 0x1C */
} CollModel;
void func_8001FFF0(s32 *in, s32 *pos, s32 *a, s32 *b, s32 unused, s32 *out);
void func_8003148C(s32 *out, s32 *box, void *model, s32 *pos);
void func_8002D85C(void);
/* Adds a collision record for obj: its box number idx (-2: the first one if
 * the animation has boxes), or a box from its scale. Returns the record, or
 * NULL if there is no room or no such box. */

s32 *func_8002D0E8(GoolObject *obj, s32 unused, s32 idx) {
    s32 pos[3];
    s32 off[3];
    s32 b[6];
    s32 sc[3];
    s32 *rec;
    u8 *end;
    s32 kind;
    s32 a;
    CollModel *model;
    CollPart *part;
    s32 *box;
    s32 t;

    if (idx == -2 && *(u8 *)obj->unkE8 == 1) {
        idx = 0;
    }
    model = NULL;
    if (idx < 0 || obj->unkE8 == 0 || *(u8 *)obj->unkE8 != 1) {
        kind = (s32)model; /* 0; written so it matches */
    } else {
        model = *(CollModel **)((u8 *)obj->unk8 + 0x20);
        if (idx >= model->count) {
        fail:
            return NULL;
        }
        part = &model->parts[idx];
        kind = part->flags & 7;
    }
    if (game.unk5C >= 0x48 || (game.unk58 - ((u8 *)&game + 0x4B0)) >> 2 >= 0x240) {
        func_8002D85C();
    }
    rec = (s32 *)game.unk58;
    switch (kind) {
    case 1:
        end = (u8 *)rec + 0x44;
        break;
    default:
        kind = 0;
    case 0:
        end = (u8 *)rec + 0x20;
        break;
    }
    if (end >= (u8 *)&game + 0xDB0) {
        goto fail;
    }
    game.unk58 = end;
    game.unk390[game.unk5C] = rec;
    game.unk5C++;
    rec[1] = (s32)obj;
    rec[0] = kind;
    if (model == NULL) {
        t = obj->xform.scale[0];
        t = t >= 0 ? t : -t;
        rec[2] = obj->xform.trans[0] - ((t * 200) >> 4);
        t = obj->xform.scale[0];
        t = t >= 0 ? t : -t;
        rec[5] = obj->xform.trans[0] + ((t * 200) >> 4);
        rec[3] = obj->xform.trans[1] - ((obj->xform.scale[1] * 200) >> 4);
        rec[6] = obj->xform.trans[1] + ((obj->xform.scale[1] * 200) >> 4);
        rec[4] = obj->xform.trans[2] - ((obj->xform.scale[2] * 200) >> 4);
        rec[7] = obj->xform.trans[2] + ((obj->xform.scale[2] * 200) >> 4);
    } else {
        switch (kind) {
        case 1:
            *(Box6 *)(rec + 2) = model->parts[idx].box;
            rec[8] = obj->xform.trans[0];
            rec[9] = obj->xform.trans[1];
            rec[10] = obj->xform.trans[2];
            func_8003148C(rec + 11, rec + 2, obj->unk8, rec + 8);
            break;
        case 0:
            a = obj->xform.rot[1] & 0xFFF;
            if (a == 0 && obj->xform.rot[1] == 0 && obj->xform.rot[0] == 0 && obj->xform.scale[0] == 0x1000) {
                pos[0] = obj->xform.trans[0] + part->off[0];
                pos[1] = obj->xform.trans[1] + part->off[1];
                pos[2] = obj->xform.trans[2] + part->off[2];
            } else {
                off[0] = part->off[0];
                off[1] = part->off[1];
                off[2] = part->off[2];
                func_8001FFF0(off, obj->xform.trans, obj->xform.rot, obj->xform.scale, 0, pos);
            }
            box = model->parts[idx].box.v;
            sc[0] = ABS(obj->xform.scale[0]);
            sc[1] = obj->xform.scale[1];
            sc[2] = obj->xform.scale[2];
            b[0] = ((box[0] >> 8) * sc[0]) >> 4;
            b[1] = ((box[1] >> 8) * sc[1]) >> 4;
            b[2] = ((box[2] >> 8) * sc[2]) >> 4;
            b[3] = ((box[3] >> 8) * sc[0]) >> 4;
            b[4] = ((box[4] >> 8) * sc[1]) >> 4;
            b[5] = ((box[5] >> 8) * sc[2]) >> 4;
            if ((u32)(a - 0x200) > 0xC00) {
                rec[2] = pos[0] + b[0];
                rec[3] = pos[1] + b[1];
                rec[4] = pos[2] + b[2];
                rec[5] = pos[0] + b[3];
                rec[6] = pos[1] + b[4];
                rec[7] = pos[2] + b[5];
            } else if (a < 0x600) {
                rec[2] = pos[0] + b[2];
                rec[5] = pos[0] + b[5];
                rec[3] = pos[1] + b[1];
                rec[6] = pos[1] + b[4];
                rec[4] = pos[2] - b[3];
                rec[7] = pos[2] - b[0];
            } else if (a < 0xA00) {
                rec[2] = pos[0] + b[0];
                rec[5] = pos[0] + b[3];
                rec[3] = pos[1] + b[1];
                rec[6] = pos[1] + b[4];
                rec[4] = pos[2] - b[5];
                rec[7] = pos[2] - b[2];
            } else {
                rec[2] = pos[0] - b[5];
                rec[5] = pos[0] - b[2];
                rec[3] = pos[1] + b[1];
                rec[6] = pos[1] + b[4];
                rec[4] = pos[2] + b[0];
                rec[7] = pos[2] + b[3];
            }
            switch (crash->state_flags >> 28) {
            case 9:
            case 11:
                rec[2] -= 0x7530;
                rec[4] -= 0x7530;
                rec[5] += 0x7530;
                rec[7] += 0x7530;
                break;
            case 14:
                rec[2] -= 100000;
                rec[4] -= 100000;
                rec[5] += 100000;
                rec[7] += 100000;
                break;
            }
            break;
        }
    }
    return rec;
}

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

    D_80060ADC = (u32 *)(PSX_SCRATCH + 0x0100);
    D_80060AD8 = (u32 *)(PSX_SCRATCH + 0x0180);
    D_80060AE0 = (u32 *)(PSX_SCRATCH + 0x0380);
    for (i = 0; i < 32; i++) {
        ((u32 *)(PSX_SCRATCH + 0x0380))[i] = D_8005C884[i];
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

s32 func_80030AD8(s32 y, s32 *pts, s32 *out); /* cuts the box at height y into segments */
#define CELL(v, o) ((((v) - (o)) << 2) / 0x2000 + 16)
#define CLAMP_SPAN(v, x) \
    v = x; \
    if (v < -1) { \
        v = -1; \
    } \
    if (v >= 33) { \
        v = 32; \
    }
#define ADD_SPAN(lo, hi, v) \
    if (v < *(lo)) { \
        *(lo) = v; \
    } \
    if (*(hi) < v) { \
        *(hi) = v; \
    }
#ifdef NON_MATCHING
/* Marks on the grid D_80060ADC (around pos) the cells taken by the turned
 * box of the collision record rec (kind 1), cut at three heights inside
 * box; with a, the cells of its edges are also given to func_8002D944.
 * Equivalent; the original allocates the registers differently. */
void func_8002DA74(s32 *rec, s32 *box, s32 *pos, s32 a) {
    s32 pts[8][3];
    s32 seg[12][3];
    s8 spans[32][2];
    s32 top;
    s32 mid;
    s32 y;
    s32 lastz;
    s8 *span;
    s32 n;
    s32 i;
    s32 *p;
    s32 minx;
    s32 maxx;
    s32 minz;
    s32 maxz;
    s32 x0;
    s32 z0;
    s32 x1;
    s32 z1;
    s32 dx;
    s32 dz;
    s32 step;
    s32 e;
    s32 inc;
    s32 inc2;
    s32 end;
    s32 cnt;
    s32 last;
    s32 v;
    s8 *lo;
    s8 *hi;
    u32 *grid;

    if (!func_8003191C(box, rec + 11)) {
        return;
    }
    pts[0][0] = pts[1][0] = pts[4][0] = pts[5][0] = rec[2];
    pts[2][0] = pts[3][0] = pts[6][0] = pts[7][0] = rec[5];
    pts[0][1] = pts[3][1] = pts[4][1] = pts[7][1] = rec[3];
    pts[1][1] = pts[2][1] = pts[5][1] = pts[6][1] = rec[6];
    pts[0][2] = pts[1][2] = pts[2][2] = pts[3][2] = rec[4];
    pts[4][2] = pts[5][2] = pts[6][2] = pts[7][2] = rec[7];
    func_8003F520(pts[0], pts[0], ((GoolObject *)rec[1])->unk8, rec + 8, 8, 7);
    top = box[4];
    y = box[1];
    mid = (top + y) >> 1;
    if (rec[15] < top) {
        top = rec[15];
    }
    if (y < rec[12]) {
        y = rec[12];
    }
    if (rec[15] < mid) {
        mid = rec[15];
    }
    if (mid < rec[12]) {
        mid = rec[12];
    }
    for (;;) {
        n = func_80030AD8(y, pts[0], seg[0]);
        if (n < 3) {
            goto next;
        }
        maxx = minx = seg[0][0];
        maxz = minz = seg[0][2];
        for (i = n * 2 - 2, p = seg[1]; i >= 0; i--, p += 3) {
            if (p[0] < minx) {
                minx = p[0];
            }
            if (maxx < p[0]) {
                maxx = p[0];
            }
            if (p[2] < minz) {
                minz = p[2];
            }
            if (maxz < p[2]) {
                maxz = p[2];
            }
        }
        maxx = CELL(maxx, pos[0]);
        maxz = CELL(maxz, pos[2]);
        minx = CELL(minx, pos[0]);
        minz = CELL(minz, pos[2]);
        /* from here: the window of cells looked at */
        if (a) {
            if (maxx < -16 || maxz < -16 || minx >= 48 || minz >= 48) {
                goto next;
            }
            minx = minz = -16;
            maxx = 47;
        } else {
            if (maxx < 0 || maxz < 0 || minx >= 32 || minz >= 32) {
                goto next;
            }
            maxx = 31;
            minx = minz = 0;
        }
        maxz = maxx;
        for (i = 15; i >= 0; i--) {
            ((s32 *)spans)[15 - i] = 0xFE21FE21; /* lo 33, hi -2 */
        }
        for (i = n - 1, p = seg[0]; i >= 0; i--) {
            x0 = CELL(p[0], pos[0]);
            z0 = CELL(p[2], pos[2]);
            p += 3;
            x1 = CELL(p[0], pos[0]);
            z1 = CELL(p[2], pos[2]);
            p += 3;
            if (z1 < z0) {
                v = z0;
                z0 = z1;
                z1 = v;
                v = x0;
                x0 = x1;
                x1 = v;
            }
            if (maxz < z0 || z1 < minz) {
                continue;
            }
            dz = z1 - z0;
            dx = x1 - x0;
            if (dz == 0) {
                if ((u32)z0 < 32) {
                    span = spans[z0];
                    CLAMP_SPAN(v, x0);
                    ADD_SPAN(&span[0], &span[1], v);
                    CLAMP_SPAN(v, x1);
                    ADD_SPAN(&span[0], &span[1], v);
                }
                if (!a) {
                    continue;
                }
                if (x1 < x0) {
                    v = x0;
                    x0 = x1;
                    x1 = v;
                }
                if (maxx < x0 || x1 < minx) {
                    continue;
                }
                if (x0 < minx) {
                    x0 = minx;
                }
                if (maxx < x1) {
                    x1 = maxx;
                }
                do {
                    func_8002D944(x0 - 16, z0 - 16);
                    x0 += 8;
                } while (x0 <= x1);
                if (x0 != x1) {
                    func_8002D944(x1 - 16, z0 - 16);
                }
                continue;
            }
            step = 1;
            if (dx < 0) {
                dx = -dx;
                step = -1;
            }
            if (maxz < z1) {
                z1 = maxz;
            }
            if (dz < dx) {
                /* more along x: one point per column */
                inc = dz * 2;
                inc2 = (dz - dx) * 2;
                e = inc - dx;
                while (z0 < minz) {
                    if (e > 0) {
                        e += inc2;
                        z0++;
                    } else {
                        e += inc;
                    }
                    x0 += step;
                }
                span = spans[z0 < 0 ? 0 : z0];
                if ((u32)z0 < 32) {
                    CLAMP_SPAN(v, x0);
                    ADD_SPAN(&span[0], &span[1], v);
                }
                cnt = 0;
                last = 12345;
                end = x1 + step;
                lo = span;
                hi = span + 1;
                for (;;) {
                    if (a && x0 >= minx && x0 <= maxx) {
                        if (--cnt <= 0) {
                            cnt = 8;
                            func_8002D944(x0 - 16, z0 - 16);
                            last = 12345;
                        } else {
                            last = x0;
                            lastz = z0;
                        }
                    }
                    if (e <= 0) {
                        x0 += step;
                        e += inc;
                        if (x0 == end) {
                            break;
                        }
                        continue;
                    }
                    e += inc2;
                    if ((u32)z0 < 32) {
                        CLAMP_SPAN(v, x0);
                        ADD_SPAN(lo, hi, v);
                        hi += 2;
                        lo += 2;
                    }
                    x0 += step;
                    if (x0 == end) {
                        break;
                    }
                    z0++;
                    if (z1 < z0) {
                        break;
                    }
                    if ((u32)z0 < 32) {
                        CLAMP_SPAN(v, x0);
                        ADD_SPAN(lo, hi, v);
                    }
                }
            } else {
                /* more along z: one point per row */
                inc = dx * 2;
                inc2 = (dx - dz) * 2;
                e = inc - dz;
                while (z0 < minz) {
                    if (e > 0) {
                        e += inc2;
                        x0 += step;
                    } else {
                        e += inc;
                    }
                    z0++;
                }
                cnt = 0;
                last = 12345;
                lo = spans[z0 < 0 ? 0 : z0];
                hi = lo + 1;
                do {
                    if ((u32)z0 < 32) {
                        CLAMP_SPAN(v, x0);
                        ADD_SPAN(lo, hi, v);
                        hi += 2;
                        lo += 2;
                    }
                    if (a && x0 >= minx && x0 <= maxx) {
                        if (--cnt <= 0) {
                            cnt = 8;
                            func_8002D944(x0 - 16, z0 - 16);
                            last = 12345;
                        } else {
                            last = x0;
                            lastz = z0;
                        }
                    }
                    if (e > 0) {
                        e += inc2;
                        x0 += step;
                    } else {
                        e += inc;
                    }
                    z0++;
                } while (z0 <= z1);
            }
            if (a && last != 12345) {
                func_8002D944(last - 16, lastz - 16);
            }
        }
        lo = spans[0];
        grid = D_80060ADC;
        for (i = 31; i >= 0; i--) {
            minx = *lo++;
            maxx = *lo++;
            if (maxx >= -1) {
                if (minx < 0 && maxx >= 0) {
                    minx = 0;
                }
                if (maxx >= 32 && minx < 32) {
                    maxx = 31;
                }
                if ((u32)minx < 32 && (u32)maxx < 32) {
                    *grid |= ((u32)-1 >> minx) & (-1 << (31 - maxx));
                }
            }
            grid++;
        }
    next:
        if (y == top) {
            break;
        }
        y = (y == mid) ? top : mid;
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002DA74);
#endif

void func_8002DA74(s32 *rec, s32 *box, s32 *pos, s32 a);
/* Collides obj, moving from prev to pos, with the objects of the collision
 * records: marks the cells they take on the grid D_80060ADC (with a, also
 * the cells of their edges) and tells obj (func_8001B328) about the ones it
 * runs into. The cell of prev is kept free while doing it. */

void func_8002E4F0(s32 *pos, s32 *prev, GoolObject *obj, CollQuery *q, s32 a) {
    s32 box[6];
    s32 big[6];
    s32 b2[6];
    s32 i;
    s32 nofloor;
    s32 x0;
    s32 z0;
    GoolObject *collider;
    u32 bit;
    s32 gz;
    u32 was;
    s32 *rec;
    GoolObject *o;
    s32 kind;
    s32 *rbox;
    s32 x;
    s32 z;
    s32 cx0;
    s32 cx1;
    s32 cz0;
    s32 cz1;
    s32 t;
    u32 hit;

    nofloor = !(((ZoneHdr *)((Entry *)D_80065C90.unkCC)->items[0])->unk2DC & 0x100000);
    x0 = pos[0];
    z0 = pos[2];
    if (game.unk5C != 0) {
        collider = obj->collider;
        gz = ((prev[2] - z0) << 2) / 0x2000 + 16;
        bit = ((prev[0] - x0) << 2) / 0x2000 + 16;
        if (bit < 32 && gz >= 0 && gz < 32) {
            bit = 0x80000000 >> bit;
            was = D_80060ADC[gz] & bit;
        } else {
            bit = 0;
            gz = 0;
            was = 0;
        }
        box[0] = pos[0] + *(s32 *)((u8 *)obj->unk8 + 0x30);
        box[1] = pos[1] + *(s32 *)((u8 *)obj->unk8 + 0x34);
        box[2] = pos[2] + *(s32 *)((u8 *)obj->unk8 + 0x38);
        box[3] = pos[0] + *(s32 *)((u8 *)obj->unk8 + 0x3C);
        box[4] = pos[1] + *(s32 *)((u8 *)obj->unk8 + 0x40);
        box[5] = pos[2] + *(s32 *)((u8 *)obj->unk8 + 0x44);
        t = pos[0] - 0x7FBC;
        if (box[0] < t) {
            t = box[0];
        }
        big[0] = t - 0x6400;
        big[1] = box[1];
        t = pos[2] - 0x7FBC;
        if (box[2] < t) {
            t = box[2];
        }
        big[2] = t - 0x6400;
        t = pos[0] + 0x7FBC;
        if (t < box[5]) { /* box[3] was meant */
            t = box[5];
        }
        big[3] = t + 0x6400;
        big[4] = box[4];
        t = pos[2] + 0x7FBC;
        if (t < box[5]) {
            t = box[5];
        }
        big[5] = t + 0x6400;
        q->steps += 0x100;
        for (i = 0; i < game.unk5C; i++) {
            rec = game.unk390[i];
            o = (GoolObject *)rec[1];
            kind = rec[0] & 7;
            rbox = rec + 11;
            if (kind == 0) {
                rbox = rec + 2;
            }
            if (o == collider && a && rbox[4] <= big[1]) {
                continue;
            }
            if (!func_8003191C(big, rbox)) {
                continue;
            }
            if (nofloor && (o->status_b & 0x10000)
                && ((((!(obj->state_flags & 0x10) && obj->invincibility_state != 5)
                      || ((GoolObjectType(o) != 0x300 || (o->status_c & 0x412)) && !(o->state_flags & 0x20)))
                     && !(obj->state_flags & 0x4000))
                    || (o->state_flags & 0x4000))
                && !(o->status_a & 0x4000) && ((o->id >> 16) != 0xE || big[1] < rbox[1])) {
                D_80060ADC[gz] &= ~bit;
                switch (kind) {
                case 0:
                    b2[0] = rec[2] + o->hotspot_size;
                    b2[1] = rec[3];
                    b2[2] = rec[4] + o->hotspot_size;
                    b2[3] = rec[5] - o->hotspot_size;
                    b2[4] = rec[6];
                    b2[5] = rec[7] - o->hotspot_size;
                    cx0 = ((b2[0] - x0) << 2) / 0x2000;
                    x = cx0;
                    cx1 = ((b2[3] - x0) << 2) / 0x2000;
                    cz0 = ((b2[2] - z0) << 2) / 0x2000;
                    z = cz0;
                    cz1 = ((b2[5] - z0) << 2) / 0x2000;
                    func_8002D990(cx0, cz0, cx1, cz1, 1);
                    if (!a) {
                        break;
                    }
                    for (; x < cx1; x += 8) {
                        func_8002D944(x, z);
                    }
                    for (x = cx1; z < cz1; z += 8) {
                        func_8002D944(x, z);
                    }
                    for (z = cz1; x > cx0; x -= 8) {
                        func_8002D944(x, z);
                    }
                    for (x = cx0; z > cz0; z -= 8) {
                        func_8002D944(x, z);
                    }
                    break;
                case 1:
                    func_8002DA74(rec, big, pos, a);
                    break;
                }
                if (a) {
                    hit = D_80060ADC[gz] & bit;
                    was |= hit;
                    if (hit) {
                        func_8001B328(obj, box, (GoolObject *)rec[1], kind == 0 ? rbox : NULL);
                    }
                }
            }
            if (kind == 0 && a && func_8003191C(box, rec + 2)) {
                func_8001B328(obj, box, (GoolObject *)rec[1], rec + 2);
            }
        }
        D_80060ADC[gz] |= was;
    }
}

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

extern s8 D_8005C074[]; /* (dx, dz) offsets of the cells around a cell, nearest first */
extern CollQuery D_80067E40;
s32 func_8002EDF0(s32 a, s32 b, s32 *pos, GoolObject *obj);

/* Marks (set) or clears on the grid D_80060ADC the cells covered by the
 * walls among the surfaces of D_80067E40 around pos (mode 0: walls near the
 * floor, mode 1: the other ones above it). Returns how many it marked. */

s32 func_8002EDF0(s32 set, s32 mode, s32 *pos, GoolObject *obj) {
    CollQuery *q;
    s32 i;
    s32 n;
    u16 *rec;
    u32 f;
    s32 kind;
    u16 lx;
    u16 ly;
    u16 lz;
    u16 res;
    s32 x0;
    s32 x1;
    s32 z0;
    s32 b[6];
    s32 size[3];

    q = &D_80067E40;
    i = 0;
    for (n = 0; i < q->count; i++) {
        rec = q->recs[i];
        if (rec[0] == 0) {
            i++;
            size[0] = rec[1] << 8;
            size[1] = rec[2] << 8;
            size[2] = rec[3] << 8;
            rec = q->recs[i];
            lx = rec[1];
            ly = rec[2];
            lz = rec[3];
            continue;
        }
        f = (rec[0] >> 2) | 1;
        if (f != 0) {
            kind = ((f & 0xE) >> 1) + 1;
        } else {
            kind = 0;
        }
        if (kind == 5 || kind == 0) {
            continue;
        }
        switch (mode) {
        case 0:
            if (kind != 2 && kind != 6) {
                continue;
            }
            break;
        case 1:
            if (kind == 2 || kind == 6) {
                continue;
            }
            break;
        }
        /* res is still the one of the previous surface here (a bug) */
        b[1] = q->box[1] + ((s16)rec[2] << 4) - pos[1];
        b[4] = b[1] + (size[1] >> (ly < res ? ly : res));
        switch (mode) {
        case 0:
            if (b[1] > 0x6400 || b[4] < -0x19000) {
                continue;
            }
            break;
        case 1:
            if (b[4] < 0) {
                continue;
            }
            break;
        }
        b[0] = q->box[0] + ((s16)rec[1] << 4) - pos[0];
        b[2] = q->box[2] + ((s16)rec[3] << 4) - pos[2];
        res = rec[0] & 7;
        b[3] = b[0] + (size[0] >> (lx < res ? lx : res));
        b[5] = b[2] + (size[2] >> (lz < res ? lz : res));
        x0 = (b[0] << 2) / 0x2000;
        x1 = (b[3] << 2) / 0x2000;
        z0 = (b[2] << 2) / 0x2000;
        func_8002D990(x0, z0, x1, (b[5] << 2) / 0x2000, set);
        n++;
    }
    return n;
}

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

/* A range lo..hi with its data (one word per 256 steps). */
typedef struct {
    s32 lo;
    s32 hi;
    s32 *data;
} CollRange;
/* Finds the range of the list r (ended by a NULL data) holding x; puts in out
 * x rounded down to a multiple of step from its start and returns its data
 * there. NULL if none. */

s32 *func_8002F42C(s32 *out, s32 x, s32 step, CollRange *r) {
    s32 n;
    s32 *p;

    for (; r->data != NULL; r++) {
        if (x >= r->lo && r->hi >= x) {
            n = (x - r->lo) / step;
            p = r->data + (n >> 8);
            *out = r->lo + n * step;
            return p;
        }
    }
    return NULL;
}

s32 func_8003D9DC(s32 x); /* square root */
/* Puts in out the point p projected (in x, z) on the line through (x0, z0)
 * and (x1, z1). */

void func_8002F4DC(s32 *out, s32 *p, s32 x0, s32 x1, s32 z0, s32 z1) {
    s32 dx;
    s32 dz;
    s32 pz;
    s32 px;
    s32 len;
    s32 nz;
    s32 nx;

    dx = (x1 - x0) >> 8;
    dz = (z1 - z0) >> 8;
    pz = (p[2] - z0) >> 8;
    px = (p[0] - x0) >> 8;
    len = func_8003D9DC(dx * dx + dz * dz);
    nz = (dz << 12) / len;
    nx = (dx << 12) / len;
    pz = (pz * nz - px * nx) >> 12;
    out[0] = x0 - ((pz * nx) >> 16);
    out[1] = p[1];
    out[2] = ((pz * nz) >> 16) + z0;
}

/* Puts in out the point b mirrored (in x) around a. */

void func_8002F648(s32 *out, s32 *a, s32 *b) {
    out[0] = a[0] - (b[0] - a[0]);
    out[1] = b[1];
    out[2] = b[2];
}

extern s32 D_80061034[]; /* the level */
u32 NSStringToEID(char *str);
char NSAlphaNumChar(s32 idx);
/* Marks on the grid D_80060ADC (func_8002D944) the walls of the level's wall
 * map (entry "vcm?X") in the rows around pos (one row each side). */

void func_8002F678(s32 *pos, s32 unused1, s32 unused2) {
    char name[8] = "vcm?X";
    u32 eid;
    s32 *map;
    s32 size;
    s32 x;
    s32 lo;
    s32 hi;
    s32 i;
    s32 z;
    s32 zi;
    u32 *col;
    s32 z0;
    s32 z1;
    u32 w;
    u32 wh;
    s32 x0;
    s32 x1;

    name[3] = NSAlphaNumChar(D_80061034[0]);
    eid = NSStringToEID(name);
    map = (s32 *)((Entry *)NSOpen(&eid, 1, 0))->items[0];
    size = map[3];
    zi = ((pos[2] - map[2]) >> 8) / size;
    x = (pos[0] - map[0]) >> 8;
    col = (u32 *)(map + 7);
    lo = zi - 1;
    hi = zi + 1;
    for (z = 1; z >= 0; z--) {
        for (i = lo; i <= hi; i++) {
            if ((col[i * 3] & 0x1FFF) + 3 < x / size) {
                col++;
                break;
            }
        }
    }
    for (i = lo; i <= hi; i++) {
        z0 = ((((i * size) << 8) + map[2] - pos[2]) << 2) / 0x2000 + 16;
        z1 = (((((i + 1) * size) << 8) + map[2] - pos[2]) << 2) / 0x2000 + 16;
        w = col[i * 3];
        if (w != -1) {
            wh = w >> 16;
            x0 = w >> 30;
            x0 &= 1;
            x0 += wh & 0x1FFF;
            x0 = (((x0 * size << 8) + map[0] - pos[0]) << 2) / 0x2000 + 16;
            x1 = w & 0xC000;
            x1 = (w & 0x1FFF) - (x1 == 0);
            x1 = ((((x1 * size + size / 2) << 8) + map[0] - pos[0]) << 2) / 0x2000 + 16;
            for (z = z0; z <= z1; z++) {
                if ((w >> 16) & 0x4000) {
                    x0--;
                } else if (!((w >> 16) & 0x8000)) {
                    x0++;
                }
                if (w & 0x4000) {
                    x1--;
                } else if (!(w & 0x8000)) {
                    x1++;
                }
                func_8002D944(x0, z);
                func_8002D944(x1, z);
            }
        }
    }
}
