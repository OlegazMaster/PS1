#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"
#include "pad.h"

/* The camera: follows its path in the zone. */

/* A place on a camera path. */
typedef struct {
    s32 zone;
    s32 path;
    s32 progress;
    s32 dir; /* 2 forward, 1 backward */
} PathLoc;

#define PathEnd(path) ((func_80032EDC(path, 0x27F, 0, 2) - 1) << 8)

/* A blend between two sets of path parameters. */
typedef struct {
    s32 state; /* 0: set being blended to (0 or 1) */
    s32 want; /* 0x4: requested set */
    s32 delay; /* 0x8: frames since the request */
    s32 t; /* 0xC: 0 first set, 0x400 second set */
    s32 vel; /* 0x10 */
    s32 dir; /* 0x14: last direction */
} CamBlend; /* 0x18 */

/* The camera transition state (D_8005C634). */
typedef struct {
    s32 unk0;
    u8 unk4[0x3];
    s8 unk7; /* 0x7: 0 follow the path, 1 keep the path rotation, 2 look at the target */
    u8 unk8[0x2];
    s8 unkA; /* 0xA: in the middle of the arena */
    s8 unkB;
    GoolObject *target; /* 0xC: object the camera follows */
    s32 unk10; /* 0x10: blend toward the kept yaw (with unk14, unk18) */
    s32 unk14;
    s32 unk18;
    s32 unk1C; /* 0x1C: kept yaw */
    s32 unk20; /* 0x20: smoothed yaw, 0x1001 none */
    u8 unk24[0xC];
    s32 unk30;
    s32 unk34;
    s32 unk38; /* 0x38: transition running */
    s32 unk3C; /* 0x3C: start time */
    s32 unk40; /* 0x40: duration */
    s32 unk44[3]; /* 0x44: start rotation */
    u8 unk50[0x4];
    s32 unk54;
    s32 unk58;
    s32 unk5C;
    s32 unk60;
    s32 unk64;
    s32 unk68; /* 0x68: blend (with unk6C, unk70) */
    s32 unk6C;
    s32 unk70;
    s32 unk74; /* 0x74: frames left of the return */
    s32 unk78; /* 0x78: frames of the return */
    s32 unk7C[3]; /* 0x7C: position at the start of the return */
    CamBlend blends[2]; /* 0x88 */
    s32 msgs[8][4]; /* 0xB8: queued camera messages (unk54 of them) */
    u32 zone; /* 0x138: where to return */
    s32 path; /* 0x13C */
    s32 progress; /* 0x140 */
    s32 dir; /* 0x144 */
} CamTransition;
extern CamTransition D_8005C634;
extern s32 ticks_elapsed;
extern GoolObject *crash;
void func_800215E8(Entry *zone, u32 *path, s32 progress, u32 flags);
void func_80025188(s32 *v);
void func_80012EAC(void);

/* names of the water and terrain entries (defined after func_80021E94) */
extern char D_80060930[];
extern char D_80060938[];
extern char D_80060940[];
extern char D_80060948[];
extern char D_80060950[];
extern char D_80060958[];
extern char D_80060960[];
extern char D_80060968[];
extern char D_80060970[];
extern char D_80060978[];
extern char D_80060980[];
extern char D_80060988[];
extern char D_80060990[];
extern char D_80060998[];
extern char D_800609A0[];
extern char D_800609A8[];
extern char D_800609B0[];
extern char D_800609B8[];

extern s32 paused;
u32 NSStringToEID(char *str);
void func_80011F68(void);
void func_80019090(void *a, void *b);
/* Drawing routines (in nd_asm). */
extern u8 D_8003E520[];
extern u8 D_8003F518[];
extern u8 D_8003FA00[];
extern u8 D_80041550[];
extern u8 D_800415A4[];
extern u8 D_80041618[];
extern u8 nd_entry_80041648[];
extern u8 D_800418C4[];
extern u8 nd_entry_800418CC[];
extern u8 D_80041C64[];
extern u8 D_80041CF0[];
extern u8 D_80041D28[];
extern u8 D_80041DD8[];
extern u8 D_80041E1C[];
extern u8 D_80041E58[];
extern u8 D_80041E88[];
extern u8 D_80042648[];
extern u8 D_80042764[];
extern u8 D_800428D0[];
extern u8 D_80042994[];
extern u8 D_800429E4[];
extern u8 D_80042ADC[];
extern u8 D_80042EF8[];
extern u8 D_80042F70[];
extern u8 D_80043004[];
extern RenderState D_800676B0; /* D_80065C90.render */
#define RS D_800676B0
/* The words after the render state (D_80065C90.unk1D9C..). */
typedef struct {
    RenderState r;
    s32 unk37C;
    s32 unk380;
    s32 unk384;
} RenderStateX;
#define RSX (*(RenderStateX *)&D_800676B0)
#define CAM (*(Unk80065C90 *)((u8 *)&D_800676B0 - 0x1A20))
/* Loads the pages of a texture (the CLUTs of its items). */
#define TPAGE_LOAD(t)                                                     \
    for (i = 0; i < ((u16 *)(t))[1]; i++) {                               \
        func_80019090((u8 *)(t) + (i * 0x10 + 8) + 4, (u8 *)(t) + 4);      \
    }
#ifdef NON_MATCHING
/* Sets up the render settings of the frame (D_800676B0) from the
 * properties of the camera path: the drawing functions of the level type,
 * fog, far color, light, the objects lit by a point, water and terrain.
 * Equivalent; differs in a few registers and in the scheduling of the start. */
void func_80021E94(void) {
    s16 *h1;
    s16 *h2;
    s16 *h3;
    s16 *h4;
    s32 *p1;
    s32 *p3;
    s32 *p4;
    s32 *p5;
    s32 *p6;
    u8 b2AA[8];
    u8 b2ED[8];
    u8 b1B5[16];
    u8 b2F9[8];
    u32 eid[4];
    s32 i;
    u8 *t;
    Entry *e;
    GoolObject *o;
    Transform *c1;
    Transform *c2;
    GoolObject *o1;
    GoolObject *o2;
    u8 v;

    RS.unk0 = &CAM.unk60;
    RS.unk4 = (u8 *)&CAM + 0x90;
    RS.unk8 = (u8 *)&CAM + 0x124;
    RS.unkC = &CAM.unk20;
    RSX.unk384 = 3;
    RSX.unk380 = 4;
    RSX.unk37C = 4;
    RS.unk10 = &CAM;
    RS.unk36C = 6;
    RS.unk14 = D_8003F518;
    RS.unk340 = (crash->xform.trans[0] - RS.unk0->trans[0]) >> 8;
    RS.unk342 = (crash->xform.trans[1] - RS.unk0->trans[1]) >> 8;
    RS.unk344 = (crash->xform.trans[2] - RS.unk0->trans[2]) >> 8;
    h1 = (s16 *)func_80032710(CAM.unkD4, 0x307, CAM.unkE0, 0, NULL, 0, 0, 0, 0);
    if (h1 != NULL) {
        RS.unk30 = *(u16 *)h1;
    } else {
        RS.unk30 = 6;
    }
    p1 = (s32 *)func_80032710(CAM.unkDC, 0x185, CAM.unkE0, 0, NULL, 0, 0, 0, 0);
    if (p1 != NULL) {
        RS.unk378 = *p1;
    } else {
        RS.unk378 = 0;
    }
    game.unk23C = RS.unk378;
    func_80011F68();
    RS.unk28 = (s32)RS.unk378 < 0 ? D_80041618 : D_80041550;
    p3 = NULL;
    if (RS.unk378 & 0x1001) {
        p3 = (s32 *)func_80032710(CAM.unkDC, 0x186, CAM.unkE0, 0, NULL, 0, 0, 0, 0);
    }
    if (p3 != NULL) {
        if (RS.unk378 & 1) {
            RS.unk36C = 10;
        }
        RS.unk38 = p3[2];
        RS.unk3C = p3[1];
        game.unk370 = RS.unk370 = p3[3];
        game.unk374 = RS.unk374 = p3[4];
        if (!(RS.unk378 & 0x40000000)) {
            RS.unk34 = p3[0] << 4;
            game.unk1D4 = game.unk238 = p3[0];
        } else {
            RS.unk34 = game.unk238 << 4;
        }
    } else {
        RS.unk370 = 0;
        RS.unk374 = 0;
        game.unk238 = 0x80000000;
    }
    if (RS.unk378 & 0x400000) {
        h2 = (s16 *)func_80032710(CAM.unkDC, 0x2AA, CAM.unkE0, 0, NULL, 3, (s32)b2AA, 0, 0);
        if (ns.ldat->lid == 0x28) {
            RS.unk28 = D_800415A4;
        }
        if (h2 != NULL) {
            RS.unk98 = h2[0];
            RS.unk8C = h2[1];
            RS.unk94 = h2[2];
            RS.unk90 = h2[3];
        }
    } else {
        RS.unk98 = 0;
    }
    if (RS.unk378 & 0x10) {
        h3 = (s16 *)func_80032710(CAM.unkDC, 0x1B5, CAM.unkE0, 0, NULL, 3, (s32)b1B5, 0, 0);
        if (h3 != NULL) {
            if (paused == 0) {
                RS.unk64 = RS.unk6C;
                RS.unk66 = RS.unk6E;
                RS.unk68 = RS.unk70;
                if (game.display_flags & 0x10000000) {
                    RS.unk6C -= game.unk1B4;
                    RS.unk6E -= game.unk1B8;
                    RS.unk70 -= game.unk1BC;
                } else {
                    RS.unk6C -= ((u16 *)h3)[0];
                    RS.unk6E -= ((u16 *)h3)[1];
                    RS.unk70 -= h3[2];
                }
            }
            if (game.display_flags & 0x10000000) {
                RS.unk88 = game.unk1C0;
            } else {
                game.unk1C0 = RS.unk88 = h3[3];
            }
            RS.unk80 = h3[4];
            RS.unk84 = h3[5];
        }
        p4 = (s32 *)func_80032710(CAM.unkDC, 0x1B6, CAM.unkE0, 0, NULL, 0, 0, 0, 0);
        if (p4 != NULL) {
            RS.unk74 = p4[0];
            RS.unk78 = p4[1];
            RS.unk7C = p4[2];
        }
    } else {
        RS.unk88 = 0;
    }
    if (RS.unk378 & 0x800004) {
        func_80032710(CAM.unkDC, 0x1B7, CAM.unkE0, 0, NULL, 0, 0, 0, 0);
        o1 = (GoolObject *)game.unk240;
        RS.unk14 = D_80041E88;
        RS.unk18 = nd_entry_80041648;
        RS.unk1C = D_800418C4;
        RS.unk20 = D_80042EF8;
        RS.unk24 = D_80042F70;
        RS.unk2C = D_8003E520;
        RS.unk36C = 8;
        RSX.unk37C = 5;
        if (o1 != NULL) {
            c1 = &CAM.unk60;
            RS.unk58 = ((((o1->xform.trans[1] - c1->trans[1]) >> 8) & 0xFFFE) << 16) |
                       (((o1->xform.trans[0] - CAM.unk60.trans[0]) >> 8) & 0xFFFF);
            RS.unk5C = (o1->xform.trans[2] - c1->trans[2]) >> 8;
        } else {
            RS.unk58 = 0x80008000;
            RS.unk5C = -0x8000;
        }
        o2 = (GoolObject *)game.unk138;
        if (o2 != NULL) {
            c2 = &CAM.unk60;
            RS.unk50 = ((((o2->xform.trans[1] - c2->trans[1]) >> 8) & 0xFFFE) << 16) |
                       (((o2->xform.trans[0] - c2->trans[0]) >> 8) & 0xFFFF);
            RS.unk54 = (o2->xform.trans[2] - c2->trans[2]) >> 8;
        } else {
            RS.unk50 = 0x80008000;
            RS.unk54 = -0x8000;
        }
    }
    if (RS.unk378 & 8) {
        p5 = (s32 *)func_80032710(CAM.unkDC, 0x1B8, CAM.unkE0, 0, NULL, 0, 0, 0, 0);
        if (p5 != NULL) {
            RS.unk40 = p5[0];
            RS.unk44 = p5[1];
        }
        RS.unk14 = D_80041E1C;
        RS.unk18 = D_80041C64;
        RS.unk1C = D_80041D28;
        RS.unk20 = D_80041CF0;
        RS.unk24 = D_80041DD8;
        RS.unk360 = game.unk280;
    }
    if (RS.unk378 & 0x8000000) {
        p6 = (s32 *)func_80032710(CAM.unkDC, 0x1B8, CAM.unkE0, 0, NULL, 0, 0, 0, 0);
        if (p6 != NULL) {
            RS.unk40 = p6[0];
            RS.unk44 = p6[1];
        }
        RS.unk14 = D_80043004;
        RS.unk18 = D_800428D0;
        RS.unk1C = D_800429E4;
        RS.unk20 = D_80042994;
        RS.unk24 = D_80042ADC;
        RS.unk2C = D_8003FA00;
        RS.unk36C = 8;
        RS.unk360 = 0x282C300;
        RSX.unk37C = 5;
    }
    if (RS.unk378 & 0x40) {
        RS.unk14 = D_80041E58;
        RS.unk18 = nd_entry_80041648;
        RS.unk1C = nd_entry_800418CC;
        RS.unk20 = D_80042648;
        RS.unk24 = D_80042764;
    }
    if (RS.unk378 & 0x4000000) {
        h4 = (s16 *)func_80032710(CAM.unkD8, 0x2F9, CAM.unkE0, 0, NULL, 3, (s32)b2F9, 0, 0);
        game.unk2F0 = h4[0] << 8;
        game.unk2F4 = h4[1] << 8;
        game.unk2F8 = h4[2] << 8;
        game.unk2FC = h4[3] << 8;
    }
    game.unk360 = func_80032DF8(CAM.unkDC, 0x345, CAM.unkE0, 0, 0, 3, 0);
    if (RS.unk378 & 0x1000000) {
        func_80032710(CAM.unkD8, 0x2ED, CAM.unkE0, 0, NULL, 3, (s32)b2ED, 0, 0);
        eid[0] = NSStringToEID(D_80060930);
        e = (Entry *)NSOpen(eid, 1, 0);
        for (i = 0; i < 14; i++) {
            RS.unkB8[i] = (s32)e->items[i];
        }
        eid[0] = NSStringToEID(D_80060938);
        e = (Entry *)NSOpen(eid, 1, 0);
        for (i = 0; i < 14; i++) {
            RS.unkB8[i + 14] = (s32)e->items[i];
        }
        RS.unk128 = RS.unkB8[0];
        if (RS.unk378 & 0x8000000) {
            RS.unk9C = -0x12C0;
            RS.unk28 = D_800415A4;
            RS.unkAC = 0;
            RS.unkB0 = 0;
            t = e->items[14];
            TPAGE_LOAD(t);
            RS.unkB4 = t + 8;
        } else {
            RS.unk9C = 0;
            switch ((u32)ns.ldat->lid) {
            case 25:
                eid[0] = NSStringToEID(D_80060940);
                eid[1] = NSStringToEID(D_80060948);
                eid[2] = NSStringToEID(D_80060950);
                break;
            case 13:
                eid[0] = NSStringToEID(D_80060958);
                eid[1] = NSStringToEID(D_80060960);
                eid[2] = NSStringToEID(D_80060968);
                break;
            case 33:
                eid[0] = NSStringToEID(D_80060970);
                eid[1] = NSStringToEID(D_80060978);
                eid[2] = NSStringToEID(D_80060980);
                break;
            case 39:
                eid[0] = NSStringToEID(D_80060988);
                eid[1] = NSStringToEID(D_80060990);
                eid[2] = NSStringToEID(D_80060998);
                break;
            }
            RS.unkAC = (s32)((Entry *)NSOpen(&eid[0], 1, 0))->items[0];
            RS.unkB0 = (s32)((Entry *)NSOpen(&eid[1], 1, 0))->items[0];
            e = (Entry *)NSOpen(&eid[2], 1, 0);
            RS.unk130 = (s32)e->items[0];
            t = e->items[1];
            TPAGE_LOAD(t);
            RS.unk134 = t + 8;
            t = e->items[2];
            TPAGE_LOAD(t);
            RS.unk13C = t + 8;
            t = e->items[3];
            TPAGE_LOAD(t);
            RS.unkB4 = t + 8;
            t = e->items[4];
            RS.unk12C = (s32)t;
            v = t[((crash->xform.trans[2] >> 19) << 6) + (crash->xform.trans[0] >> 20)];
            game.unk2F0 = -(((crash->xform.trans[0] >> 19) & 1 ? v >> 4 : v & 0xF) << 8);
        }
    }
    if (RS.unk378 & 0x10000000) {
        switch ((u32)ns.ldat->lid) {
        case 19:
            eid[3] = NSStringToEID(D_800609A0);
            break;
        case 23:
            eid[3] = NSStringToEID(D_800609A8);
            break;
        case 31:
            eid[3] = NSStringToEID(D_800609B0);
            break;
        case 5:
            eid[3] = NSStringToEID(D_800609B8);
            break;
        }
        e = (Entry *)NSOpen(&eid[3], 1, 0);
        t = e->items[0];
        TPAGE_LOAD(t);
        RS.unkB4 = t + 8;
        RS.unkB8[0] = (s32)e->items[1];
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_80021E94);
#endif

/* .sdata: names of the water and terrain entries  */
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

/* Camera on a grid of paths (property 0x309 of the path: cell size, grid
 * size, cell of the path, origin): moves one cell at a time toward the cell
 * of the target and takes its position and rotation. */

void func_80022EB4(void) {
    GoolObject *obj = D_8005C634.target;
    PathLoc loc;
    Entry *zone;
    s32 path;
    s32 *grid;
    s32 row;
    s32 x;
    s32 cur_row;
    s32 cur_col;
    s32 col;
    s32 n;
    s32 m;
    s32 rem;
    Transform *cam;

    if (obj != NULL) {
        zone = (Entry *)D_80065C90.unkCC;
        path = D_80065C90.unkD4;
        grid = (s32 *)func_80032710(D_80065C90.unkD4, 0x309, 0, 0, NULL, 2, 0, 0, 0);
        row = ((obj->xform.trans[2] >> 8) - grid[7]) / grid[1];
        m = grid[3];
        if (row < 0) {
            row = 0;
        }
        if (row >= m) {
            row = m - 1;
        }
        x = ((obj->xform.trans[0] >> 8) - grid[6]) / grid[0];
        cur_row = grid[5];
        m = grid[2];
        if (x < 0) {
            x = 0;
        }
        if (x >= m) {
            x = m - 1;
        }
        n = func_80032EDC(path, 0x27F, 0, 2);
        cur_col = grid[4];
        col = x / n;
        rem = x % n;
        if (row != cur_row || col != cur_col) {
            if (row > cur_row) {
                row = cur_row + 1;
            } else if (row < cur_row) {
                row = cur_row - 1;
            }
            if (col > cur_col) {
                col = cur_col + 1;
            } else if (col < cur_col) {
                col = cur_col - 1;
            }
            func_80023170(col, row, &loc);
            zone = (Entry *)NSOpen((u32 *)&loc.zone, 1, 0);
            path = (s32)zone->items[loc.path * 3 + *(s32 *)(zone->items[0] + 0x184)];
        }
        func_800215E8(zone, (u32 *)path, rem << 8, 0);
        cam = &D_80065C90.unk60;
        cam->trans[0] = obj->xform.trans[0];
        cam->trans[1] = obj->xform.trans[1];
        cam->trans[2] = obj->xform.trans[2];
        cam->rot[0] = obj->xform.rot[0] & 0xFFF;
        cam->rot[1] = (obj->xform.rot[1] + 0x800) & 0xFFF;
        cam->rot[2] = obj->xform.rot[2] & 0xFFF;
        D_80065C90.unk114 = cam->rot[1];
    } else {
        func_800215E8((Entry *)D_80065C90.unkCC, (u32 *)D_80065C90.unkD4, D_80065C90.unkE0, 0);
    }
}

/* Looks through the links at both ends of the current camera path (property
 * 0x109) for a path whose property 0x309 is (a, b); puts its place into
 * loc. Returns 1 when found at the start, 2 at the end, 0 if none. */

s32 func_80023170(s32 a, s32 b, PathLoc *loc) {
    s32 end;
    u8 *link;
    s32 n;
    s32 i;
    u32 *ref;
    Entry *zone;
    s32 path;
    s32 *p;
    s32 ret;

    for (end = 0; end < 2; end++) {
        if (end) {
            link = (u8 *)func_80032710(D_80065C90.unkD4, 0x109, PathEnd(D_80065C90.unkD4), 0, NULL, 1, 0, 0, 0);
        } else {
            link = (u8 *)func_80032710(D_80065C90.unkD4, 0x109, 0, 0, NULL, 1, 0, 0, 0);
        }
        if (link != NULL) {
            if (end) {
                n = func_80032EDC(D_80065C90.unkD4, 0x109, PathEnd(D_80065C90.unkD4), 1);
            } else {
                n = func_80032EDC(D_80065C90.unkD4, 0x109, 0, 1);
            }
            for (i = 0; i < n; link += 4, i++) {
                ref = (u32 *)(((Entry *)((u32 *)D_80065C90.unkD4)[1])->items[0] + 0x194) + link[2];
                zone = (Entry *)NSOpen(ref, 1, 0);
                path = (s32)zone->items[link[1] * 3 + *(s32 *)(zone->items[0] + 0x184)];
                p = (s32 *)func_80032710(path, 0x309, 0, 0, NULL, 2, 0, 0, 0);
                if (p != NULL && p[5] == b && p[4] == a) {
                    ret = end ? 2 : 1;
                    goto found;
                }
            }
        }
    }
    return 0;
found:
    loc->zone = *ref;
    loc->path = link[1];
    if (link[0] == 1) {
        loc->dir = 2;
        loc->progress = 0;
    } else {
        loc->dir = 1;
        loc->progress = PathEnd(path);
    }
    return ret;
}

extern s32 D_8005C794[4];
extern s32 D_8005C7A4[4];
s32 func_8004F794(s32 x, s32 z); /* angle of (x, z) */
void func_80030A34(s32 *out, s32 dist, s32 rx, s32 ry);
s32 func_8003D9DC(s32 x); /* sqrt */
s32 func_80023B68(s32 type, u32 *out, s32 mask); /* linked path of a type */
void func_80024E80(s32 *v); /* end of transition */
void func_8002513C(void); /* start of transition */
void func_80023A94(s32 *v, s32 a); /* arena entry */
s32 func_80023DBC(u32 *loc, s32 *pos, void *out); /* camera state nearest to pos */
s32 func_80031E10(s32 a, s32 b, s32 t); /* blend */

#ifdef NON_MATCHING
/* Camera of the round arenas: turns around the origin toward v, leaves
 * for a linked path (property 0x400) when v is far out between two of
 * the six sectors, and backs away with the distance.
 * Equivalent; the original also computes a dead unk1C - yaw and schedules
 * a few loads differently. */
void func_800233DC(s32 *v, s32 reset) {
    s32 loc[4];
    s32 next[4];
    s32 pos[3];
    s32 off[3];
    s32 saved[3];
    s32 cs[10];
    s32 progress;
    s32 x;
    s32 z;
    s32 a;
    s32 yaw;
    s32 dist;
    s32 d;
    s32 left;
    s32 frac;
    s32 r;
    s32 mask;
    s32 k;
    s32 m;
    u32 *ref;
    Transform *rot;
    Transform *cam;
    Transform *t;
    Unk80065C90 *g;

    if (reset) {
        D_8005C634.unk20 = 0x1001;
    }
    func_800255BC(&D_8005C634.unk68, D_8005C794, 0);
    func_800255BC(&D_8005C634.unk10, D_8005C7A4, 0);
    pos[0] = v[0];
    pos[1] = v[1];
    pos[2] = v[2];
    x = -pos[0];
    z = -pos[2];
    a = func_8004F794(x, z) & 0xFFF;
    yaw = a;
    dist = func_8003D9DC((z >> 8) * (z >> 8) + (x >> 8) * (x >> 8)) << 8;
    if (dist > 0x138800 && (a % 682 < 0xE4 || a % 682 > 0x1C6)) {
        mask = 1;
        if (D_80065C90.unkE0 >= D_80065C90.unkE4 >> mask) {
            mask = 2;
        }
        if (func_80023B68(0x400, &D_8005C634.zone, mask)) {
            D_8005C634.unkB = 1;
            func_80024E80(v);
            func_8002513C();
            func_80023A94(v, 1);
            return;
        }
    }
    if (D_8005C634.unkA) {
        if (dist > 0x209E0) {
            D_8005C634.unk10 = 0x400;
            D_8005C634.unkA = 0;
            D_8005C634.unk14 = 0;
            D_8005C634.unk18 = -1;
        }
    } else if (dist <= 0x1404F) {
        D_8005C634.unkA = 1;
        D_8005C634.unk1C = yaw;
    }
    if (D_8005C634.unkA) {
        yaw = D_8005C634.unk1C;
    } else if (D_8005C634.unk10 != 0) {
        yaw = func_80031E10(yaw, D_8005C634.unk1C, D_8005C634.unk10 >> 2);
        D_8005C634.unk1C = yaw;
    }
    if (D_8005C634.unk68 != 0) {
        yaw = func_80031E10(yaw, D_8005C634.unk60, D_8005C634.unk68 >> 2);
    }
    D_80065C90.unk114 = yaw;
    if (D_8005C634.unk20 != 0x1001) {
        yaw = func_80031E10(D_8005C634.unk20, yaw, 0x80);
    }
    D_8005C634.unk20 = yaw;
    d = yaw - (func_8004F794(D_80065C90.unk84.trans[0], D_80065C90.unk84.trans[2]) & 0xFFF);
    if (d > 0x800) {
        d -= 0x1000;
    }
    if (d < -0x800) {
        d += 0x1000;
    }
    if (d < -2 || d > 2) {
        if (d < 0) {
            D_80065C90.unkF8 = 1;
        } else {
            D_80065C90.unkF8 = 2;
        }
    } else {
        D_80065C90.unkF8 = 0;
    }
    if (D_80065C90.unkF8 != 0) {
        if (d < 0) {
            d = -d;
        }
        d = (d * 9 << 9) >> 8;
        g = &D_80065C90;
        loc[0] = (s32)g->unkCC;
        loc[1] = g->unkD4;
        loc[2] = g->unkE0;
        loc[3] = g->unkF8;
        left = func_800260B4(loc, d, &progress);
        if (left != 0 && func_80028484(loc, next)) {
            g->unkF8 = next[3];
            func_800260B4(next, left, &progress);
            func_800215E8((Entry *)next[0], (u32 *)next[1], progress, 0);
        } else {
            func_800215E8((Entry *)D_80065C90.unkCC, (u32 *)D_80065C90.unkD4, progress, 0);
        }
    } else {
        func_800215E8((Entry *)D_80065C90.unkCC, (u32 *)D_80065C90.unkD4, D_80065C90.unkE0, 0);
    }
    if (D_8005C634.unk68 != 0) {
        t = &D_80065C90.unk60;
        t->trans[1] -= (D_8005C634.unk68 * 0x194B0) >> 10;
    }
    frac = 0;
    if (dist > 0xC7FFF && (a % 682 < 0xE4 || a % 682 > 0x1C6)) {
        ref = &D_8005C634.zone;
        mask = 1;
        if (D_80065C90.unkE0 >= D_80065C90.unkE4 >> mask) {
            mask = 2;
        }
        if (func_80023B68(0x400, ref, mask) && !func_80023DBC(ref, v, cs)) {
            frac = dist - 0xC8000;
            if (frac > 0x64000) {
                frac = 0x64000;
            }
            frac = (frac << 8) / 0x64000;
        }
    }
    if (frac != 0) {
        rot = &D_80065C90.unk84;
        saved[0] = rot->rot[0];
        saved[1] = rot->rot[1];
        rot->rot[0] = func_80031E10(rot->rot[0], cs[3], frac);
        rot->rot[1] = func_80031E10(rot->rot[1], cs[4], frac);
        func_80025188(pos);
        rot->rot[0] = saved[0];
        rot->rot[1] = saved[1];
    } else {
        func_80025188(pos);
    }
    r = dist;
    if (r > 0xEB1B8) {
        r = 0xEB1B8;
    }
    r -= 0x416E0;
    if (r < 0) {
        r = 0;
    }
    k = D_8005C634.unk68;
    m = D_8005C634.unk64;
    if (k != 0 && m >= 0) {
        r += ((m - r) * k) >> 10;
    }
    cam = &D_80065C90.unk60;
    func_80030A34(off, -r, cam->rot[0], cam->rot[1]);
    cam->trans[0] += off[0];
    cam->trans[1] += off[1];
    cam->trans[2] += off[2];
    if (frac != 0) {
        cam->trans[0] += ((cs[0] - cam->trans[0]) * frac) >> 8;
        cam->trans[1] += ((cs[1] - cam->trans[1]) * frac) >> 8;
        cam->trans[2] += ((cs[2] - cam->trans[2]) * frac) >> 8;
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_800233DC);
#endif

s32 func_8003D9DC(s32 x); /* square root */
s32 func_80023B68(s32 type, u32 *out, s32 mask);
void func_8002513C(void);
void func_80024E80(s32 *v);
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
s32 func_8002616C(); /* (PathLoc *loc, s32 *pos, s32 *lo, s32 *hi, s32 *angle) */
void func_80024590(s32 *pos);
s32 func_8002697C(s32 *loc, s32 *pos, s32 a, s32 c, s32 s, s32 n, s32 *out);
void func_80028740(); /* (u32 *path, s32 progress, s32 *out, s32 *extra) */
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

s32 func_80026464(); /* (PathLoc *locs, s32 *pos, s32 dist, s32 backward, s32 angle, s32 n) */
void func_80025CE8(); /* (PathLoc *locs, s32 n, PathLoc *out, s32 frac) */
/* Follows the target object sideways: moves along the path when the target
 * leaves the band 0xE1000..0xE5E20 across the path direction, and takes its
 * position and rotation. */

void func_80023EBC(void) {
    GoolObject *obj = D_8005C634.target;
    s32 cs[10];
    s32 loc[8];
    s32 res[4];
    s32 pos[3];
    s32 d;
    s32 dir;
    s32 angle;
    s32 s;
    Transform *cam;
    s32 near;
    s32 far;

    if (obj == NULL) {
        func_800215E8((Entry *)D_80065C90.unkCC, (u32 *)D_80065C90.unkD4, D_80065C90.unkE0, 0);
        return;
    }
    near = 0xE1000;
    far = 0xE5E20;
    pos[0] = crash->xform.trans[0];
    pos[1] = crash->xform.trans[1];
    pos[2] = crash->xform.trans[2];
    loc[0] = (s32)D_80065C90.unkCC;
    loc[1] = D_80065C90.unkD4;
    loc[2] = D_80065C90.unkE0;
    func_80028740(D_80065C90.unkD4, D_80065C90.unkE0, cs, NULL, 0);
    angle = -cs[4];
    s = func_8003C2F0(angle);
    d = ((((pos[0] - cs[0]) >> 8) * s) >> 4) - ((((pos[2] - cs[2]) >> 8) * func_8003C364(angle)) >> 4);
    if (d > far) {
        dir = 1;
    } else if (d < near) {
        dir = -1;
    } else {
        dir = 0;
    }
    if (dir > 0) {
        func_80025CE8(loc, func_80026464(loc, pos, far, 0, angle, 1), res, 0x100);
        func_800215E8((Entry *)res[0], (u32 *)res[1], res[2], 0);
    } else if (dir < 0) {
        func_80025CE8(loc, func_80026464(loc, pos, near, 1, angle, 1), res, 0x100);
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
}

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

/* Returns the camera to the saved place (zone, path, progress): along the
 * paths toward pos, moving its position from the start over unk78 frames. */

void func_80024AD4(s32 *pos) {
    PathLoc locs[3];
    s32 res[4];
    s32 cs[10];
    s32 lo;
    s32 hi;
    s32 angle;
    s32 r;
    s32 n;
    s32 t;
    s32 k;
    Entry *zone;
    Transform *cam;

    locs[0].zone = (s32)D_80065C90.unkCC;
    locs[0].path = D_80065C90.unkD4;
    locs[0].progress = D_80065C90.unkE0;
    locs[0].dir = D_8005C634.dir;
    zone = (Entry *)NSOpen(&D_8005C634.zone, 1, 0);
    locs[1].zone = (s32)zone;
    locs[1].path = (s32)zone->items[D_8005C634.path * 3 + *(s32 *)(zone->items[0] + 0x184)];
    locs[1].progress = D_80065C90.unkD4 == locs[1].path ? D_80065C90.unkE0 : D_8005C634.progress;
    locs[1].dir = D_8005C634.progress == 0 ? 2 : 1;
    r = func_8002616C(&locs[1], pos, &lo, &hi, &angle);
    if (D_8005C634.unk74 != 0) {
        n = D_80065C90.unkD4 == locs[1].path ? 1 : 2;
        if (r > 0) {
            n = func_80026464(locs, pos, hi, 0, angle, n);
        } else if (r < 0) {
            n = func_80026464(locs, pos, lo, 1, angle, n);
        } else {
            locs[1] = locs[0];
        }
        func_80028740(locs[1].path, locs[1].progress, cs, NULL, 0);
        t = 0x100 / D_8005C634.unk74;
        if (r != 0) {
            func_80025CE8(locs, n, res, t);
            func_800215E8((Entry *)res[0], (u32 *)res[1], res[2], 0);
        } else {
            func_800215E8((Entry *)D_80065C90.unkCC, (u32 *)D_80065C90.unkD4, D_80065C90.unkE0, 0);
        }
        k = D_8005C634.unk74 - 1;
        t = ((D_8005C634.unk78 - k) << 10) / D_8005C634.unk78;
        cam = &D_80065C90.unk60;
        cam->trans[0] = (((cs[0] - D_8005C634.unk7C[0]) * t) >> 10) + D_8005C634.unk7C[0];
        cam->trans[1] = (((cs[1] - D_8005C634.unk7C[1]) * t) >> 10) + D_8005C634.unk7C[1];
        cam->trans[2] = (((cs[2] - D_8005C634.unk7C[2]) * t) >> 10) + D_8005C634.unk7C[2];
        func_80025188(pos);
        if (--D_8005C634.unk74 == 0) {
            goto done;
        }
    } else {
        n = 2;
        if (r > 0) {
            n = func_80026464(locs, pos, hi, 0, angle, n);
        } else if (r < 0) {
            n = func_80026464(locs, pos, lo, 1, angle, n);
        }
        func_80025CE8(locs, n, res, -1);
        func_800215E8((Entry *)res[0], (u32 *)res[1], res[2], 0);
        func_80025188(pos);
        if (D_80065C90.unkD4 == locs[1].path) {
        done:
            D_8005C634.unk0 = 0x10;
            func_80025070(D_80065C90.unkD4, D_8005C634.progress);
        }
    }
}

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

void func_80031E84(s32 *out, s32 *v, s32 yaw); /* angles of the vector v */
s32 func_80031F9C(s32 a, s32 b);
s32 func_80031E34(s32 a, s32 b, s32 c, s32 d);
/* Sets the camera rotation: toward v, from the path point (blended by
 * property 0x183), during a transition blended from its start rotation,
 * limited by property 0x119 around the path rotation. */

void func_80025188(s32 *v) {
    s32 ang[3];
    s32 d[3];
    s32 tmp[2];
    s32 tmp2[2];
    Transform *cam;
    Transform *path;
    Transform *out;
    s16 *p;
    s16 *lim;
    s32 x;
    s32 y;
    s32 z;
    u32 t;
    s32 base;
    s32 diff;
    s32 hi;
    s32 lo;

    cam = &D_80065C90.unk60;
    path = &D_80065C90.unk84;
    d[0] = v[0] - cam->trans[0];
    d[1] = v[1] - cam->trans[1];
    d[2] = v[2] - cam->trans[2];
    func_80031E84(ang, d, path->rot[1]);
    ang[0] = (ang[0] + D_80065C90.unkC8) & 0xFFF;
    if (D_8005C634.unk7 != 0) {
        if (D_8005C634.unk7 == 1) {
            x = path->rot[0];
            y = path->rot[1];
            z = path->rot[2];
        } else {
            x = ang[0];
            y = ang[1];
            z = ang[2];
        }
    } else {
        p = (s16 *)func_80032710(D_80065C90.unkDC, 0x183, D_80065C90.unkE0, 0, NULL, 3, (s32)tmp2, 0, 0);
        if (p != NULL || (p = (s16 *)func_80032710(D_80065C90.unkD8, 0x183, D_80065C90.unkE0, 0, NULL, 3, (s32)tmp2, 0, 0)) != NULL) {
            x = func_80031E10(ang[0], path->rot[0], p[0]);
            y = func_80031E10(ang[1], path->rot[1], p[1]);
            z = func_80031E10(ang[2], path->rot[2], p[2]);
        } else {
            x = func_80031F9C(ang[0], path->rot[0]);
            y = func_80031F9C(ang[1], path->rot[1]);
            z = func_80031F9C(ang[2], path->rot[2]);
        }
        if (D_8005C634.unk68 != 0) {
            x = func_80031E10(x, D_8005C634.unk5C, D_8005C634.unk68 >> 2);
        }
        x = (x + func_80025C54()) & 0xFFF;
        if (D_8005C634.unk38 != 0) {
            t = ticks_elapsed - D_8005C634.unk3C;
            if (t >= D_8005C634.unk40) {
                D_8005C634.unk38 = 0;
            } else {
                t = (t << 8) / D_8005C634.unk40;
                x = func_80031E10(D_8005C634.unk44[0], x, t);
                y = func_80031E10(D_8005C634.unk44[1], y, t);
                z = func_80031E10(D_8005C634.unk44[2], z, t);
            }
        }
        if (!D_8005C634.unkB && D_80065C90.unk118[0] != 0x1001 && D_80065C90.unkF0 != 2) {
            x = func_80031E34(D_80065C90.unk118[0], x, 0x80, 0x2D);
            y = func_80031E34(D_80065C90.unk118[1], y, 0x80, 0x2D);
            z = func_80031E34(D_80065C90.unk118[2], z, 0x80, 0x2D);
        }
    }
    lim = (s16 *)func_80032710(D_80065C90.unkD8, 0x119, D_80065C90.unkE0, 0, NULL, 3, (s32)tmp, 0, 0);
    if (lim != NULL) {
        base = D_80065C90.unk84.rot[0] & 0xFFF;
        diff = (x & 0xFFF) - base;
        if (diff > 0x800) {
            diff -= 0x1000;
        } else if (diff < -0x800) {
            diff += 0x1000;
        }
        if (diff > lim[2]) {
            diff = lim[2];
        }
        if (diff < -lim[3]) {
            diff = -lim[3];
        }
        x = (base + diff) & 0xFFF;
        base = D_80065C90.unk84.rot[1] & 0xFFF;
        diff = (y & 0xFFF) - base;
        if (diff > 0x800) {
            diff -= 0x1000;
        } else if (diff < -0x800) {
            diff += 0x1000;
        }
        hi = lim[0];
        lo = -lim[1];
        if (diff > hi) {
            diff = hi;
        }
        if (diff < lo) {
            diff = lo;
        }
        y = (base + diff) & 0xFFF;
        z &= 0xFFF;
    }
    out = &D_80065C90.unk60;
    out->rot[0] = x;
    out->rot[1] = y;
    out->rot[2] = z;
}

#ifdef NON_MATCHING
/* Moves the blend s[0] toward 0x400 (dir > 0) or 0 (dir < 0; 0 keeps s[2])
 * with speed s[1]: p = {slow-down range, max speed, acceleration, shift}.
 * Equivalent; the original negates the copy ad instead of d. */
s32 func_800255BC(s32 *s, s32 *p, s32 dir) {
    s32 v = s[0];
    s32 target;
    s32 d;
    s32 ad;
    s32 vel;

    if (dir != 0 || (dir = s[2]) != 0) {
        target = (~dir >> 31) & 0x400;
        if (v != target) {
            d = target - v;
            ad = d;
            if (ad < 0) {
                ad = -ad;
            }
            vel = s[1];
            if (ad <= p[0]) {
                ad >>= p[3];
                if (ad == 0) {
                    ad = 1;
                }
                d = d < 0 ? -ad : ad;
            }
            if (p[1] < ad) {
                d = d < 0 ? -p[1] : p[1];
            }
            if (vel < d) {
                vel += p[2];
                if (d < vel) {
                    vel = d;
                }
            } else if (d < vel) {
                vel -= p[2];
                if (vel < d) {
                    vel = d;
                }
            }
            v += vel;
            if (v < 0) {
                v = 0;
            } else if (v > 0x400) {
                v = 0x400;
            }
            s[1] = vel;
            s[0] = v;
        } else {
            s[1] = 0;
        }
    }
    return v;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_800255BC);
#endif

extern s32 D_8005C7B4[2][4]; /* speeds of the two blends */
/* Picks the parameter sets of the two blends from the direction Crash
 * faces relative to the camera (or from the held buttons) and moves the
 * blends toward them. */

void func_800256D4(void) {
    s32 locked;
    s32 input;
    s32 angle;
    s32 i;
    CamBlend *b;
    s32 (*p)[4];
    u32 st;

    locked = 0;
    if (!(crash->status_b & 0x80) || (st = crash->state_flags >> 28) == 8 || st == 1) {
        locked = 1;
    }
    input = 0;
    if (!locked) {
        angle = ((crash->xform.rot[1] - D_80065C90.unk114) << 20) >> 20;
        if ((u32)(angle - 0x71) < 0x71E) {
            input = 2;
        } else if ((u32)(angle + 0x78E) < 0x71E) {
            input = 8;
        }
        if ((u32)(angle + 0x38D) < 0x71B) {
            input |= 4;
        } else if ((u32)(angle + 0x471) >= 0x8E3) {
            input |= 1;
        }
        if (D_8005C634.unk30 != input) {
            D_8005C634.unk30 = input;
            input = 0;
            D_8005C634.unk34 = 0;
        } else if (++D_8005C634.unk34 >= 0x10) {
            D_8005C634.unk34 = 0xF;
        } else {
            input = 0;
        }
        if (pads.pads[0].held & 0x1000) {
            input |= 1;
        } else if (pads.pads[0].held & 0x4000) {
            input |= 4;
        }
        if (pads.pads[0].held & 0x2000) {
            input |= 2;
        } else if (pads.pads[0].held & 0x8000) {
            input |= 8;
        }
    }
    b = D_8005C634.blends;
    p = D_8005C7B4;
    for (i = 0; i < 2; b++, i++, p++) {
        if (!locked) {
            if (i == 1) {
                if (input & 1) {
                    if (b->want == i) {
                        b->want = 0;
                        b->delay = 0;
                    }
                } else if ((input & 4) && b->want == 0) {
                    b->want = i;
                    b->delay = 0;
                }
            } else if (input & 2) {
                if (b->want == 1) {
                    b->want = 0;
                    b->delay = 0;
                }
            } else if ((input & 8) && b->want == 0) {
                b->want = 1;
                b->delay = 0;
            }
        }
        if (b->want != b->state && ++b->delay > 0) {
            b->state = b->want;
            b->vel = 0;
        }
        func_800255BC(&b->t, *p, b->state == 0 ? -1 : 1);
    }
}

#ifdef NON_MATCHING
/* The two parameters of property 0x142 at the place loc on a camera path
 * (blended for path types 0x240D by the blend table of the type).
 * Equivalent; the original swaps two temporary registers in the fallback. */
void func_800259A0(PathLoc *loc, s32 *out_a, s32 *out_b) {
    s32 type;
    s32 item;
    s32 k;
    s32 progress;
    CamBlend *blend;
    s16 *p;
    s32 tmp;
    s32 a;
    s32 b;
    s32 a2;
    s32 b2;
    s32 t;

    if (loc->path == D_80065C90.unkD4) {
        type = D_80065C90.unkF0;
        item = D_80065C90.unkDC;
    } else {
        type = func_80032DF8(loc->path, 0x29, 0, 0, 0, 2, 0);
        k = func_80032DF8(loc->path, 0x173, 0, 0, 0, 2, 0);
        item = (s32)((Entry *)loc->zone)->items[k * 3 + D_80065C90.unkFC + *(s32 *)(((Entry *)loc->zone)->items[0] + 0x184)];
    }
    progress = loc->progress;
    if (type & 0x2401) {
        blend = &D_8005C634.blends[1];
    } else {
        blend = &D_8005C634.blends[0];
    }
    p = (s16 *)func_80032710(item, 0x142, progress, 0, NULL, 3, (s32)&tmp, 0, 0);
    if (p == NULL) {
        if (loc->path == D_80065C90.unkD4) {
            item = D_80065C90.unkD8;
        } else {
            item = (s32)((Entry *)loc->zone)->items[(*(s32 *)(((Entry *)loc->zone)->items[0] + 0x184) + 1) + k * 3];
        }
        p = (s16 *)func_80032710(item, 0x142, progress, 0, NULL, 3, (s32)&tmp, 0, 0);
    }
    if (type & 0x240D) {
        a = p[0] << 8;
        b = p[1] << 8;
        t = blend->t;
        if (t != 0) {
            a2 = p[2] << 8;
            b2 = p[3] << 8;
            if (t == 0x400) {
                a = a2;
                b = b2;
            } else {
                a += ((a2 - a) * t) >> 10;
                b += ((b2 - b) * t) >> 10;
            }
        }
    } else {
        a = p[0] << 8;
        b = p[1] << 8;
    }
    *out_a = a;
    *out_b = b;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_800259A0);
#endif

/* The GTE projection distance at the camera's point of the path. */

s32 func_80025C0C(void) {
    return func_80032DF8(D_80065C90.unkD4, 0x130, D_80065C90.unkE0, 0, 0, 3, 0x120);
}

s32 func_80031E10(s32 a, s32 b, s32 t); /* blends a and b by t */
/* Property 0x16E of the camera path point, blended by D_8005C634.blends[1].t. */

s32 func_80025C54(void) {
    s32 tmp;
    s16 *p;
    s32 a;
    s32 b;
    s32 t;

    p = (s16 *)func_80032710(D_80065C90.unkDC, 0x16E, D_80065C90.unkE0, 0, NULL, 3, (s32)&tmp, 0, 0);
    if (p == NULL) {
        return 0;
    }
    a = p[0];
    t = D_8005C634.blends[1].t;
    if (t != 0) {
        b = p[1];
        if (t == 0x400) {
            a = b;
        } else {
            a = func_80031E10(a, b, t >> 2);
        }
    }
    return a;
}

#ifdef NON_MATCHING
/* Walks the chain of n path places from locs[0] by a fraction frac / 0x100
 * of its length (frac < 0: a smoothed step) and puts the place reached in out.
 * Equivalent; the original walks with a pointer to the direction field and
 * lays out the walk loop differently. */
void func_80025CE8(PathLoc *locs, s32 n, PathLoc *out, s32 frac) {
    PathLoc *last = &locs[n - 1];
    PathLoc *l;
    s32 total;
    s32 p;
    s32 i;
    s32 k;
    s32 d;
    s32 max;
    s32 rem;
    s32 len;

    if (locs[0].path == last->path) {
        total = last->progress - locs[0].progress;
        if (total < 0) {
            total = -total;
        }
    } else {
        p = locs[0].progress;
        total = p;
        if (locs[0].dir == 2) {
            total = PathEnd(locs[0].path) - p;
        }
        p = last->progress;
        if (last->dir == 1) {
            p = PathEnd(last->path) - p;
        }
        total += p;
        i = n - 2;
        if (i > 0) {
            l = &locs[1];
            do {
                len = PathEnd(l->path);
                l++;
                total = total - 2 + len;
            } while (--i != 0);
        }
    }
    if (frac < 0) {
        k = func_80032DF8(locs[0].path, 0xC9, 0, 0, 0, 2, 0);
        d = (total * k) >> 8;
        if (d < 9) {
            *out = *last;
            return;
        }
        if (d > 0x7D000) {
            d = 0x7D000;
        }
        d = ((d * 300) / 512000 + 100) << 16;
        d /= k;
        if (d < total) {
            total = d;
        }
    } else {
        total = (total * frac) >> 8;
    }
    l = locs;
    max = l->dir == 2 ? PathEnd(l->path) : 0;
    p = l->progress;
loop:
    if (l->dir == 2) {
        rem = max - p;
        if (rem < total) {
            total -= rem;
        } else {
            p += total;
            goto done;
        }
    } else if (p < total) {
        total -= p;
    } else {
        p -= total;
        goto done;
    }
    l++;
    len = PathEnd(l->path);
    if (l->dir == 2) {
        max = len;
        p = 1;
    } else {
        max = 0;
        p = len - 1;
    }
    goto loop;
done:
    out->zone = l->zone;
    out->path = l->path;
    out->progress = p;
    out->dir = l->dir;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_80025CE8);
#endif

void func_80026038(void);

s32 func_80026018(void) {
    func_80026038();
    return -0xFF;
}

void func_8002437C(s32 n);
/* Resets the camera state. */

void func_80026038(void) {
    D_80065C90.unkF8 = 0;
    D_8005C634.unk0 = 0x10;
    D_8005C634.unk54 = 0;
    D_8005C634.unk58 = 0;
    D_8005C634.unk38 = 0;
    D_8005C634.unkA = 0;
    D_80065C90.unk118[0] = 0x1001;
    D_8005C634.unk20 = 0x1001;
    D_8005C634.unk30 = 0;
    D_8005C634.unk34 = 0;
    D_80065C90.unkF0 = 0x10;
    D_8005C634.unk70 = 0;
    D_8005C634.unk6C = 0;
    D_8005C634.unk68 = 0;
    game.unk288 = 0x100;
    func_8002437C(1);
}

/* Advances the progress of loc (zone, path, progress, direction) by dist
 * into *progress; returns how far it went past the end of the path. */

s32 func_800260B4(s32 *loc, s32 dist, s32 *progress) {
    s32 p = loc[2];
    s32 max;
    s32 over;

    if (loc[3] == 2) {
        p += dist;
        max = (func_80032EDC(loc[1], 0x27F, 0, 2) - 1) << 8;
        over = p - max;
        if (over > 0) {
            *progress = max;
            return over;
        }
    } else {
        p -= dist;
        if (p < 0) {
            *progress = 0;
            return -p;
        }
    }
    *progress = p;
    return 0;
}

/* The direction of the camera path for the flags: 4, 1, 2 or 0. */

s32 func_80026140(u32 flags) {
    if (flags & 0x3E01) {
        return 4;
    }
    if (flags & 4) {
        return 1;
    }
    return (flags & 0x80) ? 2 : 0;
}

s32 func_80026140(u32 flags);

#ifdef NON_MATCHING
/* Where pos is relative to the allowed range around the place loc on a
 * camera path: 1 beyond the far edge, -1 before the near edge, 0 inside.
 * Also returns the range and the angle of the path there. Equivalent; the
 * original schedules the arguments of the second lookup differently. */
s32 func_8002616C(PathLoc *loc, s32 *pos, s32 *out_lo, s32 *out_hi, s32 *out_angle) {
    s32 lo;
    s32 hi;
    s32 d[3];
    s32 path;
    s32 progress;
    s32 dir;
    s32 angle;
    s16 *pt;
    s32 *origin;
    s32 k;
    s32 c;
    s32 s;
    s32 x;
    s32 z;
    s32 v;
    Transform *cam;

    func_800259A0(loc, &lo, &hi);
    if (out_lo != NULL) {
        *out_lo = lo;
    }
    if (out_hi != NULL) {
        *out_hi = hi;
    }
    path = loc->path;
    progress = loc->progress;
    dir = func_80026140(func_80032DF8(path, 0x29, 0, 0, 0, 2, 0));
    if (path == D_80065C90.unkD4) {
        cam = &D_80065C90.unk60;
        d[0] = pos[0] - cam->trans[0];
        d[1] = pos[1] - cam->trans[1];
        d[2] = pos[2] - cam->trans[2];
        angle = D_80065C90.unk84.rot[1];
    } else {
        progress >>= 8;
        pt = (s16 *)func_80032710(path, 0x4B, 0, progress, NULL, 2, 0, 0, 0);
        origin = (s32 *)((Entry *)((s32 *)path)[1])->items[1];
        d[0] = pos[0] - ((origin[0] + pt[0]) << 8);
        d[1] = pos[1] - ((origin[1] + pt[1]) << 8);
        d[2] = pos[2] - ((origin[2] + pt[2]) << 8);
        if (dir == 2) {
            goto vertical;
        }
        k = func_80032DF8(path, 0x173, 0, 0, 0, 2, 0);
        pt = (s16 *)func_80032710((s32)((Entry *)loc->zone)->items[k * 3 + (*(s32 *)(((Entry *)loc->zone)->items[0] + 0x184) + 1)], 0x4B, 0, progress * 2, NULL, 2, 0, 0, 0);
        angle = pt[1];
    }
    if (dir == 2) {
    vertical:
        v = -d[1];
    } else {
        angle = -angle;
        if (out_angle != NULL) {
            *out_angle = angle;
        }
        c = func_8003C364(angle);
        s = func_8003C2F0(angle);
        x = d[0] >> 8;
        z = d[2] >> 8;
        if (dir == 1) {
            v = ((z * s) >> 4) + ((x * c) >> 4);
        } else {
            v = ((x * s) >> 4) - ((z * c) >> 4);
        }
    }
    if (v > hi) {
        k = 1;
    } else if (v < lo) {
        k = -1;
    } else {
        k = 0;
    }
    return k;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_8002616C);
#endif

s32 func_80027298(s32 *loc, s32 *pos, s32 a, s32 c, s32 s, s32 n, s32 *out);
s32 func_8002719C(s32 *loc, s32 *pos, s32 a, s32 c, s32 s, s32 n, s32 *out);
/* Finds where on the camera paths the camera should go to keep pos at
 * distance dist: searches the path of locs[n - 1] (backward or forward)
 * and, when the path ends, the paths linked to it; the result goes into
 * locs[1]. Returns the number of locations in locs. */

s32 func_80026464(PathLoc *locs, s32 *pos, s32 dist, s32 backward, s32 angle, s32 n) {
    PathLoc cands[7];
    s32 progress;
    s32 lo;
    s32 hi;
    s32 a;
    s32 type;
    s32 ret;
    PathLoc *cur;
    PathLoc *c;
    s32 c_cos;
    s32 c_sin;
    s32 best;
    s32 d;
    s32 i;
    s32 t;

    ret = n;
    cur = &locs[n - 1];
    type = func_80026140(func_80032DF8(cur->path, 0x29, 0, 0, 0, 2, 0));
    if (type != 2) {
        c_cos = func_8003C364(angle);
        c_sin = func_8003C2F0(angle);
    }
    if (ret == 1) {
        if (backward) {
            best = func_8002719C((s32 *)cur, pos, dist, c_cos, c_sin, type, &progress);
        } else {
            best = func_80026880((s32 *)cur, pos, dist, c_cos, c_sin, type, &progress);
        }
    } else if (backward) {
        best = func_80027298((s32 *)cur, pos, dist, c_cos, c_sin, type, &progress);
    } else {
        best = func_8002697C((s32 *)cur, pos, dist, c_cos, c_sin, type, &progress);
    }
    if (n == 1) {
        ret = 2;
        cur++;
        locs[1].zone = locs[0].zone;
        locs[1].path = locs[0].path;
        locs[1].dir = locs[0].dir;
    }
    cur->progress = progress;
    if (best == 0 || n != 1) {
        return ret;
    }
    i = func_800284E8((s32 *)cands, (s32 *)locs, 0);
    c = cands;
    while (i-- != 0) {
        t = func_80026140(func_80032DF8(c->path, 0x29, 0, 0, 0, 2, 0));
        if (type == t) {
            if (backward) {
                d = func_80027298((s32 *)c, pos, dist, c_cos, c_sin, t, &progress);
            } else {
                d = func_8002697C((s32 *)c, pos, dist, c_cos, c_sin, t, &progress);
            }
            if (best < 0 || (d >= 0 && d < best)) {
                c->progress = progress;
                locs[1] = *c;
                best = d;
            }
        } else if (t != 0) {
            backward = func_8002616C(c, pos, &lo, &hi, &a);
            if (backward == 0) {
                locs[1] = *c;
                break;
            }
            if (t != 2) {
                c_cos = func_8003C364(a);
                c_sin = func_8003C2F0(a);
            }
            if (backward < 0) {
                d = func_80027298((s32 *)c, pos, hi, c_cos, c_sin, t, &progress);
            } else {
                d = func_8002697C((s32 *)c, pos, lo, c_cos, c_sin, t, &progress);
            }
            if (best < 0 || (d >= 0 && d < best)) {
                c->progress = progress;
                best = d;
                locs[1] = *c;
            }
        }
        c++;
    }
    return ret;
}

/* Searches the path of loc in both directions for the progress nearest to
 * pos; sets loc[3] to the better direction. */

s32 func_80026880(s32 *loc, s32 *pos, s32 a, s32 c, s32 s, s32 n, s32 *out) {
    s32 r;
    s32 r2;
    s32 dir = 2;
    s32 p;
    s32 p2;

    loc[3] = 2;
    r = func_8002697C(loc, pos, a, c, s, n, &p);
    if (r != 0) {
        loc[3] = 1;
        r2 = func_8002697C(loc, pos, a, c, s, n, &p2);
        if (r2 >= 0 && (r < 0 || r2 < r)) {
            r = r2;
            dir = 1;
            p = p2;
        }
    }
    loc[3] = dir;
    *out = p;
    return r;
}

/* The projection of pos (>> 8) on point pt of the path relative to the
 * point o: with n & 5 horizontal along the angle (cos c, sin s), else the
 * height (<< 8). */
#define PATH_VALUE(n, px, pz, c, s)                                            \
    ((n) == 1 ? (((pz) * (s)) >> 4) + (((px) * (c)) >> 4)                       \
              : -(((pz) * (c)) >> 4) + (((px) * (s)) >> 4))
#ifdef NON_MATCHING
/* From the progress of loc (zone, path, progress, direction 2 forward or
 * else backward) searches along the path for the progress where the
 * projection of pos falls to a (see PATH_VALUE); puts it into out and
 * returns 0, or the progress nearest to it (giving up after 3 points that
 * get no nearer: -1, or the distance at the end of the path).
 * NON_MATCHING: only the register allocation differs. */
s32 func_8002697C(s32 *loc, s32 *pos, s32 a, s32 c, s32 s, s32 n, s32 *out) {
    s32 p[3];
    s32 A[3];
    s32 B[3];
    s32 last;
    s32 item;
    s32 *origin;
    s16 *pt;
    s16 *q;
    s32 prog;
    s32 i;
    s32 j;
    s32 d;
    s32 d0;
    s32 d1;
    s32 best;
    s32 best_prog;
    s32 count;
    s32 e;
    s32 f;
    s32 cx;
    s32 cz;
    s32 dx;
    s32 dz;

    item = loc[1];
    prog = loc[2];
    p[0] = pos[0] >> 8;
    p[1] = pos[1] >> 8;
    p[2] = pos[2] >> 8;
    origin = *(s32 **)(*(u8 **)(item + 4) + 0x14);
    i = prog >> 8;
    last = func_80032EDC(item, 0x27F, 0, 2) - 1;
    pt = (s16 *)func_80032710(item, 0x4B, 0, i, NULL, 2, 0, 0, 0);
    if (i >= last) {
        j = i;
        q = pt;
    } else {
        j = i + 1;
        q = pt + 3;
    }
    f = prog & 0xFF;
    if (n & 5) {
        A[0] = pt[0] + origin[0];
        A[2] = pt[2] + origin[2];
        B[0] = q[0] + origin[0];
        B[2] = q[2] + origin[2];
        cx = A[0] + (((B[0] - A[0]) * f) >> 8);
        cz = A[2] + (((B[2] - A[2]) * f) >> 8);
        dx = p[0] - cx;
        dz = p[2] - cz;
        d = PATH_VALUE(n, dx, dz, c, s);
    } else {
        A[1] = pt[1] + origin[1];
        B[1] = q[1] + origin[1];
        d = (A[1] + (((B[1] - A[1]) * f) >> 8) - p[1]) << 8;
    }
    best_prog = prog;
    if (a >= d) {
        *out = prog;
        return a - d;
    }
    best = d - a;
    if (loc[3] == 2) {
        if (n & 5) {
            dx = p[0] - B[0];
            dz = p[2] - B[2];
            d1 = PATH_VALUE(n, dx, dz, c, s);
        } else {
            d1 = (-p[1] + B[1]) << 8;
        }
        if (a >= d1) {
            *out = prog + ((((j << 8) - prog) * (((d - a) << 8) / (d - d1))) >> 8);
            return 0;
        }
        count = 0;
        e = d1 - a;
        if (e < best) {
        nearer:
            best = e;
            best_prog = j << 8;
            count = 0;
        }
        for (;;) {
            if (j == last) {
                *out = best_prog;
                return best;
            }
            i = j;
            d0 = d1;
            j = i + 1;
            pt = (s16 *)func_80032710(item, 0x4B, 0, (j << 8) >> 8, NULL, 2, 0, 0, 0);
            if (n & 5) {
                dx = p[0] - (pt[0] + origin[0]);
                dz = p[2] - (pt[2] + origin[2]);
                d1 = PATH_VALUE(n, dx, dz, c, s);
            } else {
                d1 = ((pt[1] + origin[1]) - p[1]) << 8;
            }
            if (a >= d1) {
                *out = (i << 8) + (((d0 - a) << 8) / (d0 - d1));
                return 0;
            }
            e = d1 - a;
            if (e < best) {
                goto nearer;
            }
            if (++count >= 3) {
                *out = best_prog;
                return -1;
            }
        }
    } else {
        if (n & 5) {
            dx = p[0] - A[0];
            dz = p[2] - A[2];
            d0 = PATH_VALUE(n, dx, dz, c, s);
        } else {
            d0 = (-p[1] + A[1]) << 8;
        }
        if (a >= d0) {
            *out = prog - (((prog - (i << 8)) * (((d - a) << 8) / (d - d0))) >> 8);
            return 0;
        }
        if (d0 - a < best) {
            best = d0 - a;
            best_prog = i << 8;
        }
        count = 0;
        for (;;) {
            if (i == 0) {
                *out = best_prog;
                return best;
            }
            j = i;
            d1 = d0;
            i--;
            pt = (s16 *)func_80032710(item, 0x4B, 0, (i << 8) >> 8, NULL, 2, 0, 0, 0);
            if (n & 5) {
                dx = p[0] - (pt[0] + origin[0]);
                dz = p[2] - (pt[2] + origin[2]);
                d0 = PATH_VALUE(n, dx, dz, c, s);
            } else {
                d0 = ((pt[1] + origin[1]) - p[1]) << 8;
            }
            if (a >= d0) {
                *out = (j << 8) - (((d1 - a) << 8) / (d1 - d0));
                return 0;
            }
            if (d0 - a < best) {
                best = d0 - a;
                best_prog = i << 8;
                count = 0;
            } else if (++count >= 3) {
                *out = best_prog;
                return -1;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_8002697C);
#endif

/* Like func_80026880 with func_80027298: searches both directions of the
 * path; sets loc[3] to the better one. */

s32 func_8002719C(s32 *loc, s32 *pos, s32 a, s32 c, s32 s, s32 n, s32 *out) {
    s32 r;
    s32 r2;
    s32 dir = 2;
    s32 p;
    s32 p2;

    loc[3] = 2;
    r = func_80027298(loc, pos, a, c, s, n, &p);
    if (r != 0) {
        loc[3] = 1;
        r2 = func_80027298(loc, pos, a, c, s, n, &p2);
        if (r2 >= 0 && (r < 0 || r2 < r)) {
            r = r2;
            dir = 1;
            p = p2;
        }
    }
    loc[3] = dir;
    *out = p;
    return r;
}

#ifdef NON_MATCHING
/* The mirror of func_8002697C: searches along the path for the progress
 * where the projection of pos rises to a.
 * NON_MATCHING: only the register allocation differs. */
s32 func_80027298(s32 *loc, s32 *pos, s32 a, s32 c, s32 s, s32 n, s32 *out) {
    s32 p[3];
    s32 A[3];
    s32 B[3];
    s32 last;
    s32 item;
    s32 *origin;
    s16 *pt;
    s16 *q;
    s32 prog;
    s32 i;
    s32 j;
    s32 d;
    s32 d0;
    s32 d1;
    s32 best;
    s32 best_prog;
    s32 count;
    s32 e;
    s32 f;
    s32 cx;
    s32 cz;
    s32 dx;
    s32 dz;

    item = loc[1];
    prog = loc[2];
    p[0] = pos[0] >> 8;
    p[1] = pos[1] >> 8;
    p[2] = pos[2] >> 8;
    origin = *(s32 **)(*(u8 **)(item + 4) + 0x14);
    i = prog >> 8;
    last = func_80032EDC(item, 0x27F, 0, 2) - 1;
    pt = (s16 *)func_80032710(item, 0x4B, 0, i, NULL, 2, 0, 0, 0);
    if (i >= last) {
        j = i;
        q = pt;
    } else {
        j = i + 1;
        q = pt + 3;
    }
    f = prog & 0xFF;
    if (n & 5) {
        A[0] = pt[0] + origin[0];
        A[2] = pt[2] + origin[2];
        B[0] = q[0] + origin[0];
        B[2] = q[2] + origin[2];
        cx = A[0] + (((B[0] - A[0]) * f) >> 8);
        cz = A[2] + (((B[2] - A[2]) * f) >> 8);
        dx = p[0] - cx;
        dz = p[2] - cz;
        d = PATH_VALUE(n, dx, dz, c, s);
    } else {
        A[1] = pt[1] + origin[1];
        B[1] = q[1] + origin[1];
        d = (A[1] + (((B[1] - A[1]) * f) >> 8) - p[1]) << 8;
    }
    if (d >= a) {
        *out = prog;
        return d - a;
    }
    best = a - d;
    best_prog = prog;
    if (loc[3] == 2) {
        if (n & 5) {
            dx = p[0] - B[0];
            dz = p[2] - B[2];
            d1 = PATH_VALUE(n, dx, dz, c, s);
        } else {
            d1 = (-p[1] + B[1]) << 8;
        }
        if (d1 >= a) {
            *out = prog + ((((j << 8) - prog) * (((a - d) << 8) / (d1 - d))) >> 8);
            return 0;
        }
        count = 0;
        e = a - d1;
        if (e < best) {
        nearer:
            best = e;
            best_prog = j << 8;
            count = 0;
        }
        for (;;) {
            if (j == last) {
                *out = best_prog;
                return best;
            }
            i = j;
            d0 = d1;
            j = i + 1;
            pt = (s16 *)func_80032710(item, 0x4B, 0, (j << 8) >> 8, NULL, 2, 0, 0, 0);
            if (n & 5) {
                dx = p[0] - (pt[0] + origin[0]);
                dz = p[2] - (pt[2] + origin[2]);
                d1 = PATH_VALUE(n, dx, dz, c, s);
            } else {
                d1 = ((pt[1] + origin[1]) - p[1]) << 8;
            }
            if (d1 >= a) {
                *out = (i << 8) + (((a - d0) << 8) / (d1 - d0));
                return 0;
            }
            e = a - d1;
            if (e <= best) {
                goto nearer;
            }
            if (++count >= 3) {
                *out = best_prog;
                return -1;
            }
        }
    } else {
        if (n & 5) {
            dx = p[0] - A[0];
            dz = p[2] - A[2];
            d0 = PATH_VALUE(n, dx, dz, c, s);
        } else {
            d0 = (-p[1] + A[1]) << 8;
        }
        if (d0 >= a) {
            *out = prog - (((prog - (i << 8)) * (((a - d) << 8) / (d0 - d))) >> 8);
            return 0;
        }
        if (a - d0 < best) {
            best = a - d0;
            best_prog = i << 8;
        }
        count = 0;
        for (;;) {
            if (i == 0) {
                *out = best_prog;
                return best;
            }
            j = i;
            d1 = d0;
            i--;
            pt = (s16 *)func_80032710(item, 0x4B, 0, (i << 8) >> 8, NULL, 2, 0, 0, 0);
            if (n & 5) {
                dx = p[0] - (pt[0] + origin[0]);
                dz = p[2] - (pt[2] + origin[2]);
                d0 = PATH_VALUE(n, dx, dz, c, s);
            } else {
                d0 = ((pt[1] + origin[1]) - p[1]) << 8;
            }
            if (d0 >= a) {
                *out = (j << 8) - (((a - d1) << 8) / (d0 - d1));
                return 0;
            }
            if (a - d0 < best) {
                best = a - d0;
                best_prog = i << 8;
                count = 0;
            } else if (++count >= 3) {
                *out = best_prog;
                return -1;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_80027298);
#endif

#ifdef NON_MATCHING
/* Posts the path messages of the triggers of the camera path point: the
 * mask (third value) of each range of properties 0x1E3 (height), 0x1E2
 * (across the path) and 0x1E4 (along the path) that pos is outside of.
 * Equivalent; the original allocates the registers differently. */
void func_80027AB8(s32 *pos) {
    s16 *p;
    s16 *px;
    s16 *pz;
    s32 lo;
    s32 hi;
    s32 zlo;
    s32 zhi;
    s32 d;
    s32 y;
    s32 x;
    s32 z;
    s32 c;
    s32 s;
    s32 a;
    s32 mask;
    Transform *cam;

    p = (s16 *)func_80032710(D_80065C90.unkD8, 0x1E3, D_80065C90.unkE0, 0, NULL, 0, 0, 0, 0);
    if (p != NULL) {
        lo = p[0];
        hi = p[1];
        if (hi >= lo) {
            y = pos[1] - D_80065C90.unk60.trans[1];
            if (y < lo << 8 || y > hi << 8) {
                mask = p[2] << 8;
                func_80027D6C(0, &mask);
            }
        }
    }
    px = (s16 *)func_80032710(D_80065C90.unkD8, 0x1E2, D_80065C90.unkE0, 0, NULL, 0, 0, 0, 0);
    if (px != NULL) {
        lo = px[0];
        hi = px[1];
        if (hi < lo) {
            px = NULL;
        }
    }
    pz = (s16 *)func_80032710(D_80065C90.unkD8, 0x1E4, D_80065C90.unkE0, 0, NULL, 0, 0, 0, 0);
    if (pz != NULL) {
        zlo = pz[0];
        zhi = pz[1];
        if (zhi < zlo) {
            pz = NULL;
        }
    }
    if (px != NULL || pz != NULL) {
        cam = &D_80065C90.unk60;
        x = pos[0] - cam->trans[0];
        a = -D_80065C90.unk84.rot[1];
        z = pos[2] - cam->trans[2];
        c = func_8003C364(a);
        s = func_8003C2F0(a);
        d = (((z >> 8) * s) >> 4) + (((x >> 8) * c) >> 4);
        z = (((x >> 8) * s) >> 4) - (((z >> 8) * c) >> 4);
        if (px != NULL && (d < lo << 8 || d > hi << 8)) {
            mask = px[2] << 8;
            func_80027D6C(0, &mask);
        }
        if (pz != NULL && (z < zlo << 8 || z > zhi << 8)) {
            mask = pz[2] << 8;
            func_80027D6C(0, &mask);
        }
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_80027AB8);
#endif

extern u32 D_80068F3C[];
s32 func_80028A78(void);
void func_80018A54(void);
#ifdef NON_MATCHING
/* Queues the camera message type with its arguments (CamPostMessage);
 * 0xB00 is handled at once.
 * Equivalent; the original keeps the count in another register. */
void func_80027D6C(s32 type, s32 *args) {
    s32 n;
    s32 *m;
    s32 *dst;

    if (D_8005C634.unk54 < 8) {
        switch (type) {
        case 0x500:
        case 0x600:
            n = 3;
            break;
        case 0x900:
            n = 1;
            break;
        case 0xB00:
            if (D_80068F3C[0] <= 0x100) {
                func_80028A78();
                func_80018A54();
            }
            return;
        default:
            n = 1;
            break;
        }
        m = D_8005C634.msgs[D_8005C634.unk54];
        dst = &m[1];
        m[0] = type;
        while (--n >= 0) {
            *dst++ = *args++;
        }
        D_8005C634.unk54++;
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_80027D6C);
#endif

void func_80027EC0(s32 *msg); /* CamProcessMessage */
/* Handles the queued camera messages. */

void func_80027E68(void) {
    s32 (*msg)[4] = D_8005C634.msgs;
    s32 n = D_8005C634.unk54;

    while (n > 0) {
        func_80027EC0(*msg);
        n--;
        msg++;
    }
    D_8005C634.unk54 = 0;
}

int printf(const char *fmt, ...);
void func_800280A4(s32 mask);
/* Handles a queued camera message msg (type, arguments). */

void func_80027EC0(s32 *msg) {
    s32 *p;
    s32 *q;
    s32 i;

    switch (msg[0]) {
    case 0x0:
        func_800280A4(msg[1] >> 8);
        break;
    case 0x100:
        if (D_80065C90.unkFC != msg[1] >> 8) {
            func_8002437C(msg[1] >> 8);
            func_8002513C();
        }
        break;
    case 0x200:
        D_8005C634.unk58 |= msg[1] >> 8;
        break;
    case 0x300:
        D_8005C634.unk58 &= ~(msg[1] >> 8);
        break;
    case 0x400:
        D_8005C634.unkB = 1;
        break;
    case 0x500:
        q = &D_8005C634.unk5C + msg[1] * 3;
        q[0] = msg[2];
        q[1] = msg[3];
        break;
    case 0x600:
        i = msg[1] * 3;
        p = &D_8005C634.unk68 + i;
        if (msg[2] != 0) {
            p[2] = 1;
            (&D_8005C634.unk64)[i] = msg[3];
        } else {
            p[2] = -1;
        }
        p[1] = 0;
        break;
    case 0x900:
        break;
    case 0xA00:
        D_8005C634.target = (GoolObject *)msg[1];
        break;
    default:
        printf("ERROR CamProcessMessage():  Unknown opcode (%d)\n", msg[0]);
        break;
    }
}

void func_800281F8(s8 *ev, s32 end); /* runs a path end event */
/* Runs the events (property 0x1A8) at the end and at the start of the
 * current camera path whose mask is the new bits of mask. */

void func_800280A4(s32 mask) {
    s8 *ev;

    mask &= ~D_8005C634.unk58;
    if (mask != 0) {
        ev = (s8 *)func_80032710(D_80065C90.unkD4, 0x1A8, PathEnd(D_80065C90.unkD4), 0, NULL, 1, 0, 0, 0);
        if (ev != NULL && ev[3] != 0) {
            do {
                if (*(s32 *)(ev + 4) == mask) {
                    func_800281F8(ev, 2);
                }
                ev += ev[3];
            } while (ev[3] != 0);
        }
        ev = (s8 *)func_80032710(D_80065C90.unkD4, 0x1A8, 0, 0, NULL, 1, 0, 0, 0);
        if (ev != NULL && ev[3] != 0) {
            do {
                if (*(s32 *)(ev + 4) == mask) {
                    func_800281F8(ev, 1);
                }
                ev += ev[3];
            } while (ev[3] != 0);
        }
    }
}

void func_8002437C(s32 a);
/* Runs the path end event ev (end 1: start of the path, 2: its end):
 * 1 and 3 leave for the linked path ev[1] (3 also moves the camera there
 * over 16 frames), 2 sets the allowed links, 4 changes the path parameter
 * set, 5 switches to mode 0x100, 6 sets unkB. */

void func_800281F8(s8 *ev, s32 end) {
    s32 pos;
    s32 mask;
    u8 *link;
    u32 *ref;
    Entry *zone;
    s32 path;
    Transform *cam;
    Unk80065C90 *g;

    switch (ev[2]) {
    case 4:
        if (D_80065C90.unkFC != ev[1]) {
            func_8002437C(ev[1]);
            func_8002513C();
        }
        break;
    case 2:
        mask = (u8)ev[1];
        if (end == 1) {
            D_80065C90.unk100 = mask;
        } else {
            D_80065C90.unk104 = mask;
        }
        break;
    case 6:
        D_8005C634.unkB = 1;
        break;
    case 1:
        if (D_8005C634.unk0 == 0x40) {
            break;
        }
        D_8005C634.unk0 = 0x40;
        goto leave;
    case 3:
        if (D_8005C634.unk0 == 0x20) {
            break;
        }
        D_8005C634.unk0 = 0x20;
    leave:
        pos = 0;
        g = &D_80065C90;
        if (end != 1) {
            pos = g->unkE4;
        }
        link = (u8 *)func_80032710(g->unkD4, 0x109, pos, 0, NULL, 1, 0, 0, 0) + ev[1] * 4;
        ref = &((ZoneHdr *)((Entry *)((u32 *)g->unkD4)[1])->items[0])->neighbors[link[2]];
        zone = (Entry *)NSOpen(ref, 1, 0);
        path = (s32)zone->items[link[1] * 3 + ((ZoneHdr *)zone->items[0])->first_item];
        D_8005C634.zone = *ref;
        D_8005C634.path = link[1];
        if (link[0] == 1) {
            D_8005C634.progress = 0;
        } else {
            D_8005C634.progress = PathEnd(path);
        }
        D_8005C634.dir = end;
        if (ev[2] == 3 && ev[0] != 0) {
            cam = &D_80065C90.unk60;
            D_8005C634.unk74 = D_8005C634.unk78 = 0x10;
            D_8005C634.unk7C[0] = cam->trans[0];
            D_8005C634.unk7C[1] = cam->trans[1];
            D_8005C634.unk7C[2] = cam->trans[2];
        }
        if (ev[2] == 1) {
            func_80025070(path, D_8005C634.progress);
        }
        break;
    case 5:
        if (D_8005C634.unk0 != 0x100) {
            D_8005C634.unk0 = 0x100;
            D_80065C90.unkF8 = end;
        }
        break;
    }
}

#ifdef NON_MATCHING
/* The first place linked to the end of the path at loc (see func_800284E8);
 * 0 if there is none. Equivalent; the original puts the copy last. */
s32 func_80028484(s32 *loc, s32 *next) {
    PathLoc cands[6];

    if (func_800284E8((s32 *)cands, loc, 0) == 0) {
        return 0;
    }
    *(PathLoc *)next = cands[0];
    return 1;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_80028484);
#endif

#ifdef NON_MATCHING
/* Collects the places linked to the end of the path at loc (its start when
 * loc[3] is 1) into out as (zone, path, progress, dir); unless all is set,
 * only the links whose bit is set in D_80065C90.unk100 (start) or unk104
 * (end). Returns how many.
 * Equivalent; the original allocates the registers differently. */
s32 func_800284E8(s32 *out, s32 *loc, s32 all) {
    u8 *link;
    s32 n;
    s32 mask;
    s32 bit;
    s32 i;
    s32 count;
    Entry *zone;
    s32 path;

    if (loc[3] == 1) {
        link = (u8 *)func_80032710(loc[1], 0x109, 0, 0, NULL, 1, 0, 0, 0);
        if (link != NULL) {
            n = func_80032EDC(loc[1], 0x109, 0, 1);
            if (all == 0) {
                mask = D_80065C90.unk100;
            } else {
                mask = 0xFFFF;
            }
        } else {
            n = 0;
        }
    } else {
        link = (u8 *)func_80032710(loc[1], 0x109, PathEnd(loc[1]), 0, NULL, 1, 0, 0, 0);
        if (link != NULL) {
            n = func_80032EDC(loc[1], 0x109, PathEnd(loc[1]), 1);
            if (all == 0) {
                mask = D_80065C90.unk104;
            } else {
                mask = 0xFFFF;
            }
        } else {
            n = 0;
        }
    }
    bit = 1;
    if (n == 0) {
        return 0;
    }
    count = 0;
    for (i = 0; i < n; link += 4, i++, bit <<= 1) {
        if (mask & bit) {
            zone = (Entry *)NSOpen((u32 *)(((Entry *)loc[0])->items[0] + 0x194) + link[2], 1, 0);
            path = (s32)zone->items[link[1] * 3 + *(s32 *)(zone->items[0] + 0x184)];
            out[0] = (s32)zone;
            out[1] = path;
            if (link[0] == 1) {
                out[3] = 2;
                out[2] = 0;
            } else {
                out[3] = 1;
                out[2] = PathEnd(path);
            }
            out += 4;
            count++;
        }
    }
    return count;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_800284E8);
#endif

#ifdef NON_MATCHING
/* The camera state at progress on path: position (property 0x4B of the
 * path point plus the zone origin, << 8) and rotation (property 0x4B of
 * the camera path), interpolated by the low 8 bits of progress; extra gets
 * the three values after the rotation.
 * Equivalent; the original keeps the constant 2 in a register and shares more of the call. */
void func_80028740(u32 *path, s32 progress, s32 *out, s32 *extra) {
    Entry *zone;
    s32 *origin;
    s16 *p;
    s16 *q;
    s16 *np;
    s16 *nq;
    s32 frac;
    s32 next[10];

    zone = (Entry *)path[1];
    origin = (s32 *)zone->items[1];
    p = (s16 *)func_80032710((s32)path, 0x4B, 0, progress >> 8, NULL, 2, 0, 0, 0);
    if (p == NULL) {
        out[0] = out[1] = out[2] = 0;
        out[3] = out[4] = out[5] = 0;
        if (extra != NULL) {
            extra[0] = extra[1] = extra[2] = 0;
        }
        return;
    }
    if ((s32)path == D_80065C90.unkD4) {
        q = (s16 *)func_80032710(D_80065C90.unkD8, 0x4B, 0, (progress >> 8) * 2, NULL, 2, 0, 0, 0);
    } else {
        q = (s16 *)func_80032710((s32)zone->items[func_80032DF8((s32)path, 0x173, 0, 0, 0, 2, 0) * 3 + (*(s32 *)(zone->items[0] + 0x184) + 1)], 0x4B, 0, (progress >> 8) * 2, NULL, 2, 0, 0, 0);
    }
    out[0] = (origin[0] + p[0]) << 8;
    out[1] = (origin[1] + p[1]) << 8;
    out[2] = (origin[2] + p[2]) << 8;
    out[3] = q[0];
    out[4] = q[1];
    out[5] = q[2];
    if (extra != NULL) {
        extra[0] = q[3];
        extra[1] = q[4];
        extra[2] = q[5];
    }
    frac = progress & 0xFF;
    if (frac != 0 && (progress >> 8) + 1 < func_80032EDC((s32)path, 0x27F, 0, 2)) {
        np = p + 3;
        next[0] = (origin[0] + np[0]) << 8;
        next[1] = (origin[1] + np[1]) << 8;
        next[2] = (origin[2] + np[2]) << 8;
        nq = q + 6;
        next[3] = nq[0];
        next[4] = nq[1];
        next[5] = nq[2];
        out[0] += ((next[0] - out[0]) * frac) >> 8;
        out[1] += ((next[1] - out[1]) * frac) >> 8;
        out[2] += ((next[2] - out[2]) * frac) >> 8;
        out[3] = func_80031E10(out[3], next[3], frac);
        out[4] = func_80031E10(out[4], next[4], frac);
        out[5] = func_80031E10(out[5], next[5], frac);
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_80028740);
#endif

extern s32 D_800690A0[];
extern u32 D_80067A28[];
s32 func_8001E34C(GoolObject *sender, u32 event, s32 type, s32 argc, u32 *argv);
void func_80027AB8(s32 *pos);
void func_800256D4(void);
/* Updates the camera for the frame: handles the messages, then moves it
 * by its transition (unk0) and, when it follows its path (0x10), by the
 * type of the path. Returns 1 if it was updated. The path modes take
 * (pos, snap); those that use neither are declared (void). */
s32 func_80028A78(void) {
    s32 arg;
    s32 *pos;
    s32 snap;

    if ((u32)ns.ldat->lid < 8 && D_800690A0[0] == 0 && (pads.pads[0].held & 0x10) && (D_80067A28[0] & 0x400)) {
        arg = 0x8000;
        func_80027D6C(0, &arg);
        func_8001E34C(NULL, 0x4300, 0, 0, NULL);
    }
    if (crash == NULL) {
        return 0;
    }
    if (!(game.display_flags & 2)) {
        return 0;
    }
    D_8005C634.unkB = 0;
    snap = 0;
    if (game.display_flags & 0x20000) {
        pos = game.unk260;
    } else {
        pos = crash->xform.trans;
    }
    func_80027AB8(pos);
    func_800256D4();
    func_80027E68();
    D_80068F3C[0] = ((D_80067A28[0] & 0x400) == 0) << 8;
    if (D_8005C634.unk0 != 0x10) {
        switch (D_8005C634.unk0) {
        case 0x20:
            func_80024AD4(pos);
            break;
        case 0x40:
            func_80024E80(pos);
            snap = 1;
            break;
        case 0x100:
            func_80024F54(pos);
            snap = 1;
            break;
        }
        if (D_8005C634.unk0 != 0x10) {
            return 1;
        }
    }
    switch (D_80065C90.unkF0) {
    case 1:
        func_800247DC(pos, snap);
        break;
    case 2:
        func_80024918(pos);
        break;
    case 8:
        func_800233DC(pos, snap);
        break;
    case 4:
        func_8002444C(pos, snap);
        break;
    case 0x80:
        func_800246A0(pos, snap);
        break;
    case 0x200:
        ((void (*)(s32 *, s32))func_80023EBC)(pos, snap);
        break;
    case 0x1000:
        ((void (*)(s32 *, s32))func_80022EB4)(pos, snap);
        break;
    case 0x800:
        ((void (*)(s32 *, s32))func_80024124)(pos, snap);
        break;
    case 0x400:
        func_80023A94(pos, snap);
        break;
    default:
        func_800215E8((Entry *)D_80065C90.unkCC, (u32 *)D_80065C90.unkD4, D_80065C90.unkE0, 0);
        break;
    }
    return 1;
}

s32 func_80031868(Entry *zone, s32 *pos); /* whether pos is in zone */
/* The neighbor zone (with flag 4) of zone, or else of the current zone, in
 * which pos lies; -0x17 if none. */

Entry *func_80028D8C(Entry *zone, s32 *pos) {
    ZoneHdr *hdr;
    Entry *n;
    s32 i;

    hdr = (ZoneHdr *)zone->items[0];
    for (i = hdr->neighbor_count - 1; i >= 0; i--) {
        if (hdr->neighbor_flags[i] & 4) {
            n = (Entry *)NSOpen(&hdr->neighbors[i], 1, 0);
            if (func_80031868(n, pos)) {
                return n;
            }
        }
    }
    if (zone != (Entry *)D_80065C90.unkCC) {
        hdr = (ZoneHdr *)((Entry *)D_80065C90.unkCC)->items[0];
        for (i = hdr->neighbor_count - 1; i >= 0; i--) {
            if (hdr->neighbor_flags[i] & 4) {
                n = (Entry *)NSOpen(&hdr->neighbors[i], 1, 0);
                if (func_80031868(n, pos)) {
                    return n;
                }
            }
        }
    }
    return (Entry *)-0x17;
}

INCLUDE_ASM("asm/us/nonmatchings/camera", func_80028ED8);

s32 func_8003191C(s32 *box, s32 *box2); /* whether the boxes overlap */
void *func_80028ED8(void *hdr, u16 a, s32 *box, s32 *pos, s32 *step, s32 b);
/* Moves pos by step until it hits something: the box of an object (flag
 * 0x10 of status_b) within 20000 (returned), the surface of a neighbor zone
 * (returned; NULL if pos did not move); 5 when it leaves the zones (40
 * steps) or, for Crash in state 8, goes above the floor (80 steps). */

void *func_800296F8(s32 *pos, s32 *step) {
    void *found;
    s32 below;
    s32 floor;
    ZoneHdr *hdr;
    s32 n;
    s32 start[3];
    s32 box[6];
    s32 zbox[6];
    s32 i;
    u32 *rec;
    GoolObject *obj;
    Entry *e;
    s32 *b;
    s32 ok;

    found = NULL;
    below = 0;
    floor = crash->xform.trans[1];
    hdr = (ZoneHdr *)((Entry *)D_80065C90.unkCC)->items[0];
    n = 0;
    if (step[0] == 0 && step[1] == 0 && step[2] == 0) {
        return NULL;
    }
    if ((crash->state_flags >> 28) == 8) {
        below = 1;
        if (((ZoneHdr *)((Entry *)D_80065C90.unkCC)->items[0])->unk2DC & 0x800) {
            floor += 0xFFD12000;
        }
    }
    do {
        *(Vec3 *)start = *(Vec3 *)pos;
        if (game.unk5C > 0) {
            box[0] = pos[0] - 20000;
            box[3] = pos[0] + 20000;
            box[2] = pos[2] - 20000;
            box[5] = pos[2] + 20000;
            box[1] = pos[1] - 20000;
            box[4] = pos[1] + 20000;
            for (i = 0; i < game.unk5C; i++) {
                rec = game.unk390[i];
                if (!(rec[0] & 7)) {
                    obj = (GoolObject *)rec[1];
                    if ((obj->status_b & 0x10) && !(obj->state_flags & 0x800000) &&
                        func_8003191C(box, (s32 *)(rec + 2))) {
                        return obj;
                    }
                }
            }
        }
        if (below && crash->xform.trans[2] < pos[2] && floor < pos[1]) {
            n++;
            ok = n < 81;
        } else {
            for (i = 0; i < hdr->neighbor_count; i++) {
                if (hdr->neighbor_flags[i] & 4) {
                    e = (Entry *)NSOpen(&hdr->neighbors[i], 1, 0);
                    if (func_80031868(e, pos)) {
                        b = (s32 *)e->items[1];
                        zbox[0] = b[0] << 8;
                        zbox[1] = b[1] << 8;
                        zbox[2] = b[2] << 8;
                        zbox[3] = b[3] << 8;
                        zbox[4] = b[4] << 8;
                        zbox[5] = b[5] << 8;
                        found = func_80028ED8(b, ((u16 *)b)[14], zbox, pos, step, 0);
                        if (pos[0] == start[0] && pos[1] == start[1] && pos[2] == start[2]) {
                            return NULL;
                        }
                        n = 0;
                        break;
                    }
                }
            }
            if (i != hdr->neighbor_count) {
                continue;
            }
            n++;
            ok = n < 41;
        }
        if (!ok) {
            return (void *)5;
        }
        pos[0] += step[0];
        pos[1] += step[1];
        pos[2] += step[2];
    } while (found == NULL);
    return found;
}

extern s32 D_8005C7D4[];

void func_80029B10(GoolObject *obj, s32 idx, s32 val) {
    if (obj->status_b & 0x4000000) {
        if (idx >= 0) {
            obj->unk118 = D_8005C7D4[idx] + 0x20;
        } else {
            obj->unk118 = val + 0x20;
        }
    }
}

extern u8 D_8005C814[];
#ifdef NON_MATCHING
/* The light intensity b of the surface type c (0x30..0x3F).
 * Equivalent; the original has a second return after the lookup. */
void func_80029B5C(s32 c, s32 *out) {
    if ((u32)(c - 0x30) >= 0x10) {
        *out = 0;
        return;
    }
    *out = 0;
    if (c >= 0x40) {
        *out = 0;
        return;
    }
    if (c >= 0x30) {
        *out = 0x1000 - D_8005C814[c - 0x30] * 20;
        return;
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/camera", func_80029B5C);
#endif

s32 func_8001FD08(s32 cur, s32 target, s32 step);
/* Sets the light b of obj for the surface type c (blend: move toward it). */

void func_80029BBC(GoolObject *obj, s32 c, s32 blend) {
    ObjColors *col = &obj->colors;
    s32 b;

    func_80029B5C(c, &b);
    if (blend == 0) {
        col->b = b;
    } else {
        col->b = func_8001FD08(col->b, b, 0x100);
    }
}

typedef struct {
    s32 v[3];
} Vec3i;
void *func_8003C990(); /* (void *item, s32 *pos, s32 a, ...) */
s32 func_8002B9A4(void *rec, s32 *box, s32 a, s32 b, s32 best, s32 y, s32 c);
/* The floor under pos: the surface of the neighbor zone that contains pos,
 * or a higher object (flag 0x20000) standing within 20000 of it, whose
 * height then goes into pos[1]. */



void *func_80029C1C(void *unused, s32 *pos) {
    s32 p[3];
    s32 box[6];
    ZoneHdr *hdr;
    Entry *n;
    void *res;
    void *rec;
    GoolObject *obj;
    GoolObject *found;
    s32 best;
    s32 h;
    s32 i;

    res = NULL;
    *(Vec3i *)p = *(Vec3i *)pos;
    hdr = (ZoneHdr *)((Entry *)D_80065C90.unkCC)->items[0];
    do {
        for (i = 0; i < hdr->neighbor_count; i++) {
            if (hdr->neighbor_flags[i] & 4) {
                n = (Entry *)NSOpen(&hdr->neighbors[i], 1, 0);
                if (func_80031868(n, pos)) {
                    res = func_8003C990(n->items[1], pos, 9);
                    break;
                }
            }
        }
        if (i == hdr->neighbor_count) {
            res = NULL;
            break;
        }
    } while (res == NULL);
    if (game.unk5C > 0) {
        best = -999999999;
        found = NULL;
        box[0] = p[0] - 20000;
        box[3] = p[0] + 20000;
        box[2] = p[2] - 20000;
        box[5] = p[2] + 20000;
        box[1] = box[4] = p[1];
        for (i = 0; i < game.unk5C; i++) {
            rec = game.unk390[i];
            obj = ((GoolObject **)rec)[1];
            if (obj->status_b & 0x20000) {
                h = func_8002B9A4(rec, box, 1, 0, best, p[1], 0);
                if (h != -999999999) {
                    found = obj;
                    best = h;
                }
            }
        }
        if (found != NULL && (res == NULL || best >= pos[1])) {
            pos[1] = best;
            res = found;
        }
    }
    return res;
}

/* The floor under the object obj at pos (only with flag 0x4000000): the
 * surface of the neighbor zone containing pos, or an object (flag 0x20000,
 * not 0x40000000) standing within 0x88B8 whose top is under pos; its
 * height goes into pos[1]. Sets the light b of obj->parent from it. */

void *func_80029E40(GoolObject *obj, s32 *pos) {
    s32 *item;
    s32 p[3];
    s32 on_obj;
    ZoneHdr *hdr;
    Entry *n;
    void *res;
    s32 *rec;
    s32 *box;
    GoolObject *o;
    GoolObject *found;
    s32 best;
    s32 top;
    s32 i;
    s32 val;
#ifdef TARGET_PSX
    register s32 unused asm("$7"); /* the fourth argument is not set */
#else
    s32 unused = 0;
#endif

    best = -999999999;
    res = NULL;
    on_obj = 0;
    found = NULL;
    if (!(obj->status_b & 0x4000000)) {
        return NULL;
    }
    hdr = (ZoneHdr *)((Entry *)D_80065C90.unkCC)->items[0];
    *(Vec3i *)p = *(Vec3i *)pos;
    do {
        for (i = 0; i < hdr->neighbor_count; i++) {
            if (hdr->neighbor_flags[i] & 4) {
                n = (Entry *)NSOpen(&hdr->neighbors[i], 1, 0);
                if (func_80031868(n, pos)) {
                    item = (s32 *)n->items[1];
                    res = func_8003C990(item, pos, 1, unused, item[0] << 8, item[1] << 8, item[2] << 8, item[3] << 8, item[4] << 8, item[5] << 8);
                    break;
                }
            }
        }
        if (i == hdr->neighbor_count) {
            res = NULL;
            break;
        }
    } while (res == NULL);
    if (game.unk5C > 0) {
        for (i = 0; i < game.unk5C; i++) {
            rec = game.unk390[i];
            o = (GoolObject *)rec[1];
            if ((o->status_b & 0x40020000) == 0x20000) {
                box = (rec[0] & 7) == 1 ? rec + 11 : rec + 2;
                top = box[4];
                if (p[0] >= box[0] - 0x88B8 && box[3] + 0x88B8 >= p[0] && p[2] >= box[2] - 0x88B8 && box[5] + 0x88B8 >= p[2]) {
                    if (top >= pos[1] && pos[1] >= box[1] && p[1] >= (top + box[1]) / 2) {
                        res = o;
                        pos[1] = top;
                        on_obj = 1;
                        goto done;
                    }
                    if (best < top && p[1] >= (top + box[1]) / 2) {
                        found = o;
                        best = top;
                    }
                }
            }
        }
        if (found != NULL && (res == NULL || best >= pos[1])) {
            pos[1] = best;
            res = found;
            on_obj = 1;
        }
    }
done:
    if (res != NULL) {
        if (on_obj) {
            val = ((GoolObject *)res)->unk118;
            func_80029B10(obj->parent, -1, val);
        } else {
            func_80029B10(obj->parent, ((u32)res & 0x3C00) >> 10, -1);
        }
    } else {
        val = ((ZoneHdr *)obj->parent->zone->items[0])->unk2CC;
        if (val != 0) {
            func_80029B10(obj->parent, -1, val);
        } else {
            func_80029B10(obj->parent, 0, -1);
        }
    }
    return res;
}

/* The surface (of a neighbor zone, mode flags & 3) or the object (with a
 * floor or a ceiling: flags 1 or 2) at pos for obj, whose color b (and with
 * flags 4 its parent's) then follows the surface type, blended if blend is
 * set; returns it (NULL if none). */

void *func_8002A22C(GoolObject *obj, s32 *pos, u32 flags, s32 blend) {
    s32 best;
    void *res;
    s32 isobj;
    GoolObject *near;
    s32 q[3];
    s32 p[3];
    s32 *pt;
    ZoneHdr *hdr;
    Entry *e;
    s32 *b;
    s32 i;
    u32 *rec;
    GoolObject *o;
    s32 *box;
    s32 top;
    s32 c;
#ifdef TARGET_PSX
    register s32 unused asm("$7"); /* the fourth argument is not set */
#else
    s32 unused = 0;
#endif

    best = -999999999;
    res = NULL;
    isobj = 0;
    near = NULL;
    if (!(obj->status_b & 0x4000000) || ((flags & 4) && !(obj->parent->status_b & 0x4000000))) {
        return NULL;
    }
    pt = p;
    *(Vec3i *)p = *(Vec3i *)pos;
    pt[1] += 25000;
    hdr = (ZoneHdr *)((Entry *)D_80065C90.unkCC)->items[0];
    *(Vec3i *)q = *(Vec3i *)p;
    do {
        for (i = 0; i < hdr->neighbor_count; i++) {
            if (hdr->neighbor_flags[i] & 4) {
                e = (Entry *)NSOpen(&hdr->neighbors[i], 1, 0);
                if (func_80031868(e, pt)) {
                    b = (s32 *)e->items[1];
                    res = func_8003C990(b, pt, flags & 3, unused, b[0] << 8, b[1] << 8, b[2] << 8, b[3] << 8,
                                        b[4] << 8, b[5] << 8);
                    break;
                }
            }
        }
        if (i == hdr->neighbor_count) {
            res = NULL;
            break;
        }
    } while (res == NULL);
    if (game.unk5C > 0) {
        for (i = 0; i < game.unk5C; i++) {
            rec = game.unk390[i];
            o = (GoolObject *)rec[1];
            if (o == obj || o->node == 0xFFFF) {
                continue;
            }
            box = (s32 *)((rec[0] & 7) == 1 ? rec + 11 : rec + 2);
            if (flags & 1) {
                top = box[4];
                if (q[0] < box[0] || box[3] < q[0] || q[2] < box[2] || box[5] < q[2]) {
                    continue;
                }
                if (top >= pt[1] && pt[1] >= box[1]) {
                    res = o;
                    pt[1] = box[1];
                    isobj = 1;
                    goto done;
                }
                if (best < top && q[1] >= top) {
                    near = o;
                    best = top;
                }
            } else if (flags & 2) {
                top = box[5];
                if (q[0] < box[0] || box[3] < q[0] || q[1] < box[1] || box[4] < q[1]) {
                    continue;
                }
                if (top >= pt[2] && pt[2] >= box[2]) {
                    res = o;
                    pt[2] = box[2];
                    isobj = 1;
                    goto done;
                }
                if (best < top && q[2] >= top) {
                    near = o;
                    best = top;
                }
            }
        }
        if (near != NULL) {
            if (flags & 1) {
                if (res == NULL || best >= pt[1]) {
                    res = near;
                    pt[1] = best;
                    isobj = 1;
                }
            } else if (flags & 2) {
                if (res == NULL || best >= pt[2]) {
                    pt[2] = best;
                    res = near;
                    isobj = 1;
                }
            }
        }
    }
done:
    c = -1;
    if (res != NULL) {
        if (isobj) {
            c = ((GoolObject *)res)->node;
            if (c < 0) {
                c = -c;
                blend = 0;
            }
        } else {
            c = ((u32)res & 0x3F0) >> 4;
            if ((u32)(c - 0x30) >= 0x10) {
                c = -1;
            }
        }
    }
    func_80029BBC(obj, c, blend);
    if (flags & 4) {
        func_80029BBC(obj->parent, c, blend);
    }
    return res;
}

/* Sets the light b of obj from the surface type under it (property 0xF2
 * of the entity at obj->unkF0). */

void func_8002A728(GoolObject *obj) {
    s32 v;
    s32 c;
    s32 e;

    e = obj->unkF0;
    if (e != 0) {
        v = func_80032DF8(e, 0xF2, 0, 0, 0, 2, 0);
        if ((v & 0xFFFF) != 0xFFFF) {
            c = (u32)(v & 0x3F0) >> 4;
            if (c < 0x30 || c >= 0x40) {
                c = -1;
            }
            func_80029BBC(obj, c, 0);
        }
    }
}

/* The surfaces near an object: records of 8 bytes ended by 0xFFFF, their
 * count and the box searched. */
typedef struct {
    u16 recs[0x200][4];
    s32 unk1000;
    s32 count; /* 0x1004 */
    s32 box[6]; /* 0x1008: min x, y, z, max x, y, z */
} SurfaceQuery;
s32 func_80031FB4(s32 *box, void *item); /* whether box overlaps item */
s32 func_8003AF78(void *item, s32 *box, u16 *out, s32 n);
/* Collects into q the surfaces of the neighbor zones (flag 4) of the current
 * zone near pos (a box around pos sized by the object's height). */

s32 func_8002A7A8(s32 *pos, GoolObject *obj, SurfaceQuery *q) {
    ZoneHdr *hdr;
    void *item;
    s32 i;
    s32 unused[6]; /* takes stack space in the original */

    q->count = 0;
    q->unk1000 = 1;
    q->box[0] = pos[0] - 0x12C00;
    q->box[1] = pos[1] + *(s32 *)((u8 *)obj->unk8 + 0x40) / 2 - 0x25800;
    q->box[2] = pos[2] - 0x12C00;
    q->box[3] = pos[0] + 0x12C00;
    q->box[4] = pos[1] + *(s32 *)((u8 *)obj->unk8 + 0x40) / 2 + 0x25800;
    q->box[5] = pos[2] + 0x12C00;
    hdr = (ZoneHdr *)((Entry *)D_80065C90.unkCC)->items[0];
    for (i = 0; i < hdr->neighbor_count; i++) {
        if (hdr->neighbor_flags[i] & 4) {
            item = ((Entry *)NSOpen(&hdr->neighbors[i], 1, 0))->items[1];
            if (func_80031FB4(q->box, item)) {
                q->count = func_8003AF78(item, q->box, q->recs[q->count], q->count);
            }
        }
    }
    q->recs[q->count][0] = 0xFFFF;
    return q->count;
}


void func_8002A950(GoolObject *obj, SurfaceQuery *q) {
    func_8002A7A8(obj->xform.trans, obj, q);
}

extern s32 D_80060AA4; /* level.c */
extern s32 D_80060AA8[2];
extern s32 D_80060AB8;
void *malloc(s32 size);
/* Allocates the two buffers of level.c. */

s32 func_8002A97C(void) {
    u8 *p;

    D_80060AB8 = D_80060AA4 = (s32)(p = malloc(0xBE4));
    if (p != NULL) {
        *(s16 *)(p + 2) = 0;
        D_80060AA8[0] = (s32)(p = malloc(0xBE4));
        if (p != NULL) {
            *(s16 *)(p + 2) = 0;
            return -0xFF;
        }
    }
    return -0xF;
}

void free(void *ptr);
/* Frees the two buffers of level.c. */

s32 func_8002A9DC(void) {
    free((void *)D_80060AB8);
    free((void *)D_80060AA8[0]);
    return -0xFF;
}

void func_8003A928(u16 *src, u16 *dst, s32 n);
void func_80039E3C(u16 *src, s32 a, u16 *dst);
void func_8003A234(u16 *src, s32 a, u16 *dst);
/* Copies the list src into dst (bit 0 of src[1]: unpack it with
 * func_80039E3C or func_8003A234). */

u16 *func_8002AA14(u16 *src, s32 a, u16 *dst, s32 flag) {
    if (!(src[1] & 1)) {
        func_8003A928(src + 2, dst + 2, src[0]);
        *dst = *src;
        return dst;
    } else if (flag) {
        func_80039E3C(src, a, dst);
    } else {
        func_8003A234(src, a, dst);
    }
    return dst;
}
