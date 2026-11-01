#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

/* The camera: follows its path in the zone. */

/* The camera transition state (D_8005C634). */
typedef struct {
    s32 unk0;
    u8 unk4[0x7];
    s8 unkB;
    GoolObject *target; /* 0xC: object the camera follows */
    u8 unk10[0x28];
    s32 unk38; /* 0x38: transition running */
    s32 unk3C; /* 0x3C: start time */
    s32 unk40; /* 0x40: duration */
    s32 unk44[3]; /* 0x44: start rotation */
    u8 unk50[0xE8];
    u32 zone; /* 0x138: where to return */
    s32 path; /* 0x13C */
    s32 progress; /* 0x140 */
} CamTransition;
extern CamTransition D_8005C634;
extern s32 ticks_elapsed;
extern GoolObject *crash;
void func_800215E8(Entry *zone, u32 *path, s32 progress, u32 flags);
void func_80025188(s32 *v);
void func_80012EAC(void);

/* .sdata: names of the water and terrain entries (string literals of
 * func_80021E94 while it is still assembly) */
char D_80060930[] = "Wat1R";
char D_80060938[] = "Wat2R";
char D_80060940[] = "Wat6R";
char D_80060948[] = "Wat7R";
char D_80060950[] = "Wat8R";
char D_80060958[] = "Wat3R";
char D_80060960[] = "Wat4R";
char D_80060968[] = "Wat5R";
char D_80060970[] = "Wat9R";
char D_80060978[] = "WatAR";
char D_80060980[] = "WatBR";
char D_80060988[] = "WatCR";
char D_80060990[] = "WatDR";
char D_80060998[] = "WatER";
char D_800609A0[] = "Ter1R";
char D_800609A8[] = "Ter2R";
char D_800609B0[] = "Ter3R";
char D_800609B8[] = "Ter4R";

INCLUDE_ASM("asm/us/nonmatchings/camera", func_80021E94);

s32 func_80032710(s32 a, s32 id, s32 pos, s32 b, s32 *out, s32 c, s32 d, s32 e, s32 f);
void func_80022DA0(s32 count, u32 *refs, u32 flags, s32 n);
/* Loads the entries of path point pos and frees those of the point left
 * behind (dir 2: moving forward). */

void func_80022C9C(u32 *path, s32 pos, s32 dir) {
    s32 count;
    s32 refs;

    refs = func_80032710((s32)path, dir == 2 ? 0x209 : 0x208, dir == 2 ? pos - 0x100 : pos + 0x100, 0, &count, 1, 0, 0, 0);
    if (refs != 0 && count != 0) {
        func_80022DA0(count, (u32 *)refs, 0x14, 1);
    }
    refs = func_80032710((s32)path, dir == 2 ? 0x208 : 0x209, pos, 0, &count, 1, 0, 0, 0);
    if (refs != 0 && count != 0) {
        func_80022DA0(count, (u32 *)refs, 0xC, 1);
    }
}

void func_8001439C(NsdPte *pte, s32 count);
/* Opens (flags 8) or references (count, negative with flags 4) the count
 * entries refs points to. */

void func_80022DA0(s32 count, u32 *refs, u32 flags, s32 n) {
    NsdPte *pte = NULL;
    NsdPte *bucket;
    u32 ref;

    do {
        ref = *refs;
        if (ref & 1) {
            bucket = ns.pte_buckets[(ref >> 15) & 0xFF];
            if (pte == NULL || pte < bucket) {
                pte = bucket;
            }
            while (pte->eid != ref) {
                pte++;
            }
        } else {
            pte = (NsdPte *)ref;
        }
        if (flags & 8) {
            NSOpen((u32 *)pte, flags | 2, n);
        } else {
            func_8001439C(pte, (flags & 4) ? -n : n);
        }
        refs++;
    } while (--count != 0);
}

INCLUDE_ASM("asm/us/nonmatchings/camera", func_80022EB4);

INCLUDE_ASM("asm/us/nonmatchings/camera", func_80023170);

INCLUDE_ASM("asm/us/nonmatchings/camera", func_800233DC);

s32 func_8003D9DC(s32 x); /* square root */
s32 func_80023B68(s32 type, u32 *out, s32 mask);
void func_8002513C(void);
void func_80024E80(s32 *v);
void func_8002513C(void);
void func_800233DC(s32 *v, s32 a);
void func_800247DC(s32 *v, s32 a);
extern u8 D_8005C63F[];
/* Within 0x12C000 of the origin (x, z of v) and if func_80023B68 allows it,
 * switches the camera mode; otherwise updates it normally. */

void func_80023A94(s32 *v, s32 a) {
    if (func_8003D9DC((-v[2] >> 8) * (-v[2] >> 8) + (-v[0] >> 8) * (-v[0] >> 8)) << 8 <= 0x12C000) {
        if (func_80023B68(8, (u32 *)(D_8005C63F + 0x12D), 3)) {
            D_8005C63F[0] = 1;
            func_80024E80(v);
            func_8002513C();
            func_800233DC(v, 1);
            return;
        }
    }
    func_800247DC(v, a);
}

s32 func_80032EDC(s32 entity, s32 tag, s32 a, s32 b);
s32 func_80032DF8(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g);

#ifdef NON_MATCHING
/* Looks at the paths linked to the start (mask 1) and the end (mask 2) of
 * the current camera path for one of type type; puts its zone, path and
 * end into out and returns the end found, or 0. Equivalent; the original
 * keeps &D_80065C90 in s7 and out on the stack. */
s32 func_80023B68(s32 type, u32 *out, s32 mask) {
    s32 end;
    s32 bit;
    u8 *links;
    u8 *p;
    s32 count;
    s32 i;
    u32 *ref;
    Entry *zone;
    s32 other;

    for (end = 0, bit = 1; end < 2; end++, bit = 2) {
        if (!(mask & bit)) {
            continue;
        }
        links = (u8 *)func_80032710(D_80065C90.unkD4, 0x109, end ? (func_80032EDC(D_80065C90.unkD4, 0x27F, 0, 2) - 1) << 8 : 0, 0, NULL, 1, 0, 0, 0);
        if (links == NULL) {
            continue;
        }
        count = func_80032EDC(D_80065C90.unkD4, 0x109, end ? (func_80032EDC(D_80065C90.unkD4, 0x27F, 0, 2) - 1) << 8 : 0, 1);
        for (i = 0; i < count; i++, links += 4) {
            p = links + 1;
            ref = (u32 *)(((Entry *)((u32 *)D_80065C90.unkD4)[1])->items[0] + 0x194 + p[1] * 4);
            zone = (Entry *)NSOpen(ref, 1, 0);
            other = (s32)zone->items[p[0] * 3 + *(s32 *)(zone->items[0] + 0x184)];
            if (func_80032DF8(other, 0x29, 0, 0, 0, 2, 0) == type) {
                goto found;
            }
        }
    }
    return 0;
found:
    out[0] = *ref;
    out[1] = links[1];
    if (links[0] == 1) {
        out[3] = 2;
        out[2] = 0;
    } else {
        out[3] = 1;
        out[2] = (func_80032EDC(other, 0x27F, 0, 2) - 1) << 8;
    }
    return bit;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_80023B68);
#endif

s32 func_8003C2F0(s32 angle); /* sin */
s32 func_8003C364(s32 angle); /* cos */
s32 func_8002616C(s32 *loc, s32 *pos, s32 *a, s32 *b, s32 *angle);
void func_80024590(s32 *pos);
s32 func_8002697C(s32 *loc, s32 *pos, s32 a, s32 c, s32 s, s32 n, s32 *out);
void func_80028740(s32 path, s32 progress, void *a, void *b, s32 c);
/* Finds the progress on the path of loc (zone, path, ...) nearest to pos and
 * puts the camera state there into out; returns 1 if there is none. */

s32 func_80023DBC(u32 *loc, s32 *pos, void *out) {
    s32 q[4];
    s32 a;
    s32 b;
    s32 angle;
    s32 progress;
    s32 c;
    Entry *zone;

    q[0] = NSOpen(loc, 1, 0);
    zone = (Entry *)q[0];
    q[1] = (s32)zone->items[loc[1] * 3 + *(s32 *)(zone->items[0] + 0x184)];
    q[2] = loc[2];
    q[3] = loc[3];
    func_8002616C(q, pos, &a, &b, &angle);
    c = func_8003C364(angle);
    if (func_8002697C(q, pos, a, c, func_8003C2F0(angle), 4, &progress)) {
        return 1;
    }
    func_80028740(q[1], progress, out, NULL, 0);
    return 0;
}

INCLUDE_ASM("asm/us/nonmatchings/camera", func_80023EBC);

s32 func_80026464(s32 *loc, s32 *pos, s32 dist, s32 a, s32 b, s32 c);
void func_80025CE8(s32 *loc, s32 a, s32 *out, s32 b);
/* Follows the target object: moves along the path when it gets too far or
 * too near, and takes its position and rotation. */

void func_80024124(void) {
    GoolObject *obj = D_8005C634.target;
    s32 cs[10];
    s32 loc[8];
    s32 res[4];
    s32 pos[3];
    s32 d;
    s32 dir;
    Entry *zone;
    u32 *path;
    s32 progress;
    Transform *cam;
    s32 near;
    s32 far;

    if (obj == NULL) {
        func_800215E8((Entry *)D_80065C90.unkCC, (u32 *)D_80065C90.unkD4, D_80065C90.unkE0, 0);
        return;
    }
    near = 0x7D000;
    far = 0x83400;
    pos[0] = obj->xform.trans[0];
    pos[1] = obj->xform.trans[1];
    pos[2] = obj->xform.trans[2] + 0x5DC000;
    loc[0] = (s32)D_80065C90.unkCC;
    loc[1] = D_80065C90.unkD4;
    loc[2] = D_80065C90.unkE0;
    func_80028740(D_80065C90.unkD4, D_80065C90.unkE0, cs, NULL, 0);
    d = cs[2] - pos[2];
    if (d > far) {
        dir = 1;
    } else if (d < near) {
        dir = -1;
    } else {
        dir = 0;
    }
    if (dir > 0) {
        func_80025CE8(loc, func_80026464(loc, pos, far, 0, 0, 1), res, 0x100);
        func_800215E8((Entry *)res[0], (u32 *)res[1], res[2], 0);
    } else if (dir < 0) {
        func_80025CE8(loc, func_80026464(loc, pos, near, 1, 0, 1), res, 0x100);
        func_800215E8((Entry *)res[0], (u32 *)res[1], res[2], 0);
    } else {
        func_800215E8((Entry *)D_80065C90.unkCC, (u32 *)D_80065C90.unkD4, D_80065C90.unkE0, 0);
    }
    cam = &D_80065C90.unk60;
    cam->trans[0] = obj->xform.trans[0];
    cam->trans[1] = obj->xform.trans[1];
    cam->trans[2] = obj->xform.trans[2];
    cam->rot[0] = obj->xform.rot[0] & 0xFFF;
    cam->rot[1] = (obj->xform.rot[1] + 0x800) & 0xFFF;
    cam->rot[2] = obj->xform.rot[2] & 0xFFF;
    D_80065C90.unk114 = cam->rot[1];
}

/* On a change of the path type: stops or starts a camera transition. */
void func_80024330(s32 type) {
    D_80065C90.unkF8 = 0;
    if (D_8005C634.unkB) {
        D_8005C634.unk38 = 0;
    } else if (type != 0x10) {
        func_8002513C();
    }
}

#ifdef NON_MATCHING
/* Selects the camera path items for the mode n. Equivalent; the original
 * adds the index terms in another order. */
void func_8002437C(s32 n) {
    s32 k;
    Entry *zone;
    s32 item;

    D_80065C90.unkFC = n;
    if (D_80065C90.unkD4 != 0 && D_80065C90.unkCC != NULL) {
        k = func_80032DF8(D_80065C90.unkD4, 0x173, 0, 0, 0, 2, 0);
        zone = (Entry *)D_80065C90.unkCC;
        item = (s32)zone->items[k * 3 + (*(s32 *)(zone->items[0] + 0x184) + 1)];
        D_80065C90.unkD8 = item;
        if (n != 1) {
            D_80065C90.unkDC = (s32)zone->items[k * 3 + n + *(s32 *)(zone->items[0] + 0x184)];
        } else {
            D_80065C90.unkDC = item;
        }
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_8002437C);
#endif

/* Moves the camera along its path toward pos (snap: jump there), then
 * adjusts its distance. */

void func_8002444C(s32 *pos, s32 snap) {
    s32 loc[8];
    s32 res[4];
    s32 a;
    s32 b;
    s32 c;
    s32 r;

    loc[0] = (s32)D_80065C90.unkCC;
    loc[1] = D_80065C90.unkD4;
    loc[2] = D_80065C90.unkE0;
    r = func_8002616C(loc, pos, &a, &b, &c);
    if (r > 0) {
        func_80025CE8(loc, func_80026464(loc, pos, b, 0, c, 1), res, snap ? 0x100 : -1);
        func_800215E8((Entry *)res[0], (u32 *)res[1], res[2], 0);
    } else if (r < 0) {
        func_80025CE8(loc, func_80026464(loc, pos, a, 1, c, 1), res, snap ? 0x100 : -1);
        func_800215E8((Entry *)res[0], (u32 *)res[1], res[2], 0);
    } else {
        func_800215E8((Entry *)D_80065C90.unkCC, (u32 *)D_80065C90.unkD4, D_80065C90.unkE0, 0);
    }
    func_80024590(pos);
    func_80025188(pos);
}

/* Pulls the camera toward pos in z by the zoom range of the path point
 * (properties 0x252 and 0x254: near, far, strength). */

void func_80024590(s32 *pos) {
    s16 *p;
    s32 tmp;
    s32 k;
    s32 lo;
    s32 hi;
    s32 z;
    s32 d;
    Transform *cam;

    if (func_80032710(D_80065C90.unkD8, 0x252, D_80065C90.unkE0, 0, NULL, 0, 0, 0, 0) == 0) {
        return;
    }
    p = (s16 *)func_80032710(D_80065C90.unkD8, 0x254, D_80065C90.unkE0, 0, NULL, 3, (s32)&tmp, 0, 0);
    if (p == NULL) {
        return;
    }
    k = p[2];
    lo = p[0] << 8;
    hi = p[1] << 8;
    if (game.unk288 != 0x100) {
        k = (k * game.unk288) >> 8;
    }
    cam = &D_80065C90.unk60;
    z = cam->trans[2];
    d = z - pos[2];
    if (hi < d) {
        d = hi;
    }
    d -= lo;
    if (d < 0) {
        d = 0;
    }
    cam->trans[2] = z - ((d * k) >> 8);
}

/* Moves the camera along its path toward pos (snap: jump there). */

void func_800246A0(s32 *pos, s32 snap) {
    s32 loc[8];
    s32 res[4];
    s32 a;
    s32 b;
    s32 c;
    s32 r;

    loc[0] = (s32)D_80065C90.unkCC;
    loc[1] = D_80065C90.unkD4;
    loc[2] = D_80065C90.unkE0;
    r = func_8002616C(loc, pos, &a, &b, &c);
    if (r > 0) {
        func_80025CE8(loc, func_80026464(loc, pos, b, 0, c, 1), res, snap ? 0x100 : -1);
        func_800215E8((Entry *)res[0], (u32 *)res[1], res[2], 0);
    } else if (r < 0) {
        func_80025CE8(loc, func_80026464(loc, pos, a, 1, c, 1), res, snap ? 0x100 : -1);
        func_800215E8((Entry *)res[0], (u32 *)res[1], res[2], 0);
    } else {
        func_800215E8((Entry *)D_80065C90.unkCC, (u32 *)D_80065C90.unkD4, D_80065C90.unkE0, 0);
    }
    func_80025188(pos);
}

/* Moves the camera along its path toward pos (snap: jump there). */

void func_800247DC(s32 *pos, s32 snap) {
    s32 loc[8];
    s32 res[4];
    s32 a;
    s32 b;
    s32 c;
    s32 r;

    loc[0] = (s32)D_80065C90.unkCC;
    loc[1] = D_80065C90.unkD4;
    loc[2] = D_80065C90.unkE0;
    r = func_8002616C(loc, pos, &a, &b, &c);
    if (r > 0) {
        func_80025CE8(loc, func_80026464(loc, pos, b, 0, c, 1), res, snap ? 0x100 : -1);
        func_800215E8((Entry *)res[0], (u32 *)res[1], res[2], 0);
    } else if (r < 0) {
        func_80025CE8(loc, func_80026464(loc, pos, a, 1, c, 1), res, snap ? 0x100 : -1);
        func_800215E8((Entry *)res[0], (u32 *)res[1], res[2], 0);
    } else {
        func_800215E8((Entry *)D_80065C90.unkCC, (u32 *)D_80065C90.unkD4, D_80065C90.unkE0, 0);
    }
    func_80025188(pos);
}

s32 func_800260B4(s32 *loc, s32 dist, s32 *progress);
s32 func_80028484(s32 *loc, s32 *next);
void func_80025070(s32 path, s32 pos);
/* Moves the camera along its path at the speed of the path point (property
 * 0x160, its sign flips the direction), going on to the linked path at the
 * end; then adjusts its distance to pos. */

void func_80024918(s32 *pos) {
    s32 loc[4];
    s32 next[4];
    s32 progress;
    s32 speed;
    s32 left;
    Unk80065C90 *g;

    if (D_80065C90.unkF8 == 0) {
        D_80065C90.unkF8 = D_80065C90.unkEC < D_80065C90.unkE8 * 2 ? 1 : 2;
    }
    speed = func_80032DF8(D_80065C90.unkDC, 0x160, D_80065C90.unkE0, 0, 0, 3, 0x100);
    if (speed < 0) {
        speed = -speed;
        D_80065C90.unkF8 = D_80065C90.unkF8 == 1 ? 2 : 1;
    }
    g = &D_80065C90;
    loc[0] = (s32)g->unkCC;
    loc[1] = g->unkD4;
    loc[2] = g->unkE0;
    loc[3] = g->unkF8;
    left = func_800260B4(loc, speed, &progress);
    if (left != 0 && func_80028484(loc, next)) {
        g->unkF8 = next[3];
        progress = next[2];
        if (func_80032DF8(next[1], 0x29, 0, 0, 0, 2, 0) == 2) {
            func_800260B4(next, left, &progress);
        } else {
            func_80025070(next[1], next[2]);
        }
        func_800215E8((Entry *)next[0], (u32 *)next[1], progress, 0);
    } else {
        func_800215E8((Entry *)g->unkCC, (u32 *)g->unkD4, progress, 0);
    }
    func_80024590(pos);
    func_80025188(pos);
}

INCLUDE_ASM("asm/us/nonmatchings/camera", func_80024AD4);

/* Ends a camera transition: returns to the saved place, or moves on. */

void func_80024E80(s32 *v) {
    Entry *zone;

    if (D_8005C634.unkB) {
        zone = (Entry *)NSOpen(&D_8005C634.zone, 1, 0);
        func_800215E8(zone, (u32 *)zone->items[D_8005C634.path * 3 + *(s32 *)(zone->items[0] + 0x184)], D_8005C634.progress, 0);
        D_8005C634.unk0 = 0x10;
        D_80065C90.unkF8 = 0;
        crash->zone = (Entry *)D_80065C90.unkCC;
    } else {
        func_800215E8((Entry *)D_80065C90.unkCC, (u32 *)D_80065C90.unkD4, D_80065C90.unkE0, 0);
        func_80025188(v);
    }
}

s32 func_800284E8(s32 *out, s32 *loc, s32 a);
/* Moves the camera along the linked paths until the transition ends. */

void func_80024F54(s32 *v) {
    s32 out[24];
    s32 loc[4];

    do {
        loc[0] = (s32)D_80065C90.unkCC;
        loc[1] = D_80065C90.unkD4;
        loc[2] = D_80065C90.unkE0;
        loc[3] = D_80065C90.unkF8;
        if (func_800284E8(out, loc, 1) == 1) {
            if (func_80032DF8(out[1], 0x29, 0, 0, 0, 2, 0) != D_80065C90.unkF0) {
                D_8005C634.unk0 = 0x10;
            }
            func_800215E8((Entry *)out[0], (u32 *)out[1], out[2], 0);
        } else {
            func_800215E8((Entry *)D_80065C90.unkCC, (u32 *)D_80065C90.unkD4, (func_80032EDC(D_80065C90.unkD4, 0x27F, 0, 2) - 1) << 8, 0);
            D_8005C634.unk0 = 0x10;
        }
        func_80025188(v);
    } while (D_8005C634.unk0 != 0x10);
}

/* Sends the player the events listed in property 0x198 of path at pos
 * (each: length, event, arguments; a negative event calls func_80012EAC). */

void func_80025070(s32 path, s32 pos) {
    s32 *ev;
    s32 n;
    s32 len;
    s32 event;

    ev = (s32 *)func_80032710(path, 0x198, pos, 0, NULL, 1, 0, 0, 0);
    if (ev != NULL && (n = func_80032EDC(path, 0x198, pos, 1)) != 0) {
        do {
            event = ev[1];
            len = ev[0];
            if (event >= 0) {
                GoolSendEvent(NULL, crash, event, len - 2, (u32 *)(ev + 2));
            } else {
                func_80012EAC();
            }
            ev += len;
            n -= len;
        } while (n != 0);
    }
}

#ifdef NON_MATCHING
/* Starts a camera transition from the current rotation. Equivalent; the
 * original schedules the stores differently. */
void func_8002513C(void) {
    D_8005C634.unk38 = 1;
    D_8005C634.unk40 = 0x154;
    D_8005C634.unk3C = ticks_elapsed;
    D_8005C634.unk44[0] = D_80065C90.unk118[0];
    D_8005C634.unk44[1] = D_80065C90.unk118[1];
    D_8005C634.unk44[2] = D_80065C90.unk118[2];
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_8002513C);
#endif
