#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

/* The camera: follows its path in the zone. */

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
void func_8002616C(s32 *loc, s32 *pos, s32 *a, s32 *b, s32 *angle);
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
