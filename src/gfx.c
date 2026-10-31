#include "common.h"
#include "gfx.h"
#include "ns.h"
#include "libgte.h"

/* Textures and sprites. */

s32 func_80018088(Entry *entry, s32 flag, u32 eid, s32 count, void *infos, s32 ref_count, u32 *refs);

s32 func_80018008(Entry *entry, s32 flag, u32 eid) {
    u8 *hdr = entry->items[0];

    return func_80018088(entry, flag, eid, *(s32 *)(hdr + 0x1C), entry->items[4], *(s32 *)(hdr + 0x28), (u32 *)(hdr + 0x2C));
}

s32 func_80018048(Entry *entry, s32 flag, u32 eid) {
    u8 *hdr = entry->items[0];

    return func_80018088(entry, flag, eid, *(s32 *)(hdr + 0x34), entry->items[3], *(s32 *)(hdr + 0x40), (u32 *)(hdr + 0xC));
}

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80018088);

#ifdef NON_MATCHING
/* Updates the texture references of the loaded entries of types 2 and 3
 * after the texture page eid was loaded (flag 1) or dropped. Equivalent; the
 * original keeps the constants 2 and 3 in registers across the loop. */

s32 func_800181E0(u32 eid, s32 flag) {
    Entry *entry;
    s32 i;

    for (i = ns.nsd->pte_count - 1; i >= 0; i--) {
        entry = (Entry *)ns.nsd->ptes[i].value;
        if (!((u32)entry & 3)) {
            switch (entry->type) {
            case 3:
                func_80018008(entry, flag, eid);
                break;
            case 2:
                func_80018048(entry, flag, eid);
                break;
            }
        }
    }
    return -0xFF;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/gfx", func_800181E0);
#endif

s32 func_800182B4(Entry *entry) {
    return func_80018008(entry, 1, 0x6396347F);
}

s32 func_800182DC(Entry *entry) {
    return func_80018008(entry, 0, 0x6396347F);
}

s32 func_80018304(Entry *entry) {
    return func_80018048(entry, 1, 0x6396347F);
}

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_8001832C);

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_800185C8);

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_800186D8);

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80018820);

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_800188EC);

void func_80018988(MATRIX *out, MATRIX *in) {
    out->m[0][0] = in->m[0][0];
    out->m[0][1] = in->m[0][1];
    out->m[0][2] = in->m[0][2];
    out->m[1][0] = -(in->m[1][0] * 5) >> 3;
    out->m[1][1] = -(in->m[1][1] * 5) >> 3;
    out->m[1][2] = -(in->m[1][2] * 5) >> 3;
    out->m[2][0] = -in->m[2][0];
    out->m[2][1] = -in->m[2][1];
    out->m[2][2] = -in->m[2][2];
    out->t[0] = in->t[0];
    out->t[1] = in->t[1];
    out->t[2] = in->t[2];
}

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80018A54);

typedef struct {
    s32 rot[3];
    s32 trans[3];
    s32 scale[3];
} Transform;

void func_80018DA8(Transform *t) {
    t->trans[0] = 0;
    t->trans[1] = 0;
    t->trans[2] = 0;
    t->rot[0] = 0;
    t->rot[1] = 0;
    t->rot[2] = 0;
    t->scale[0] = 0x1000;
    t->scale[1] = 0x1000;
    t->scale[2] = 0x1000;
}

/* out = m * v, in 4.12 fixed point (c1: GfxTransform). */
void func_80018DD4(s32 *v, MATRIX *m, s32 *out) {
    out[0] = (m->m[0][0] * v[0] + m->m[0][1] * v[1] + m->m[0][2] * v[2]) >> 12;
    out[1] = (m->m[1][0] * v[0] + m->m[1][1] * v[1] + m->m[1][2] * v[2]) >> 12;
    out[2] = (m->m[2][0] * v[0] + m->m[2][1] * v[1] + m->m[2][2] * v[2]) >> 12;
}

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80018EBC);

/* Texture coordinates of a polygon: u, v, then CLUT or tpage, for 3 corners. */
typedef struct {
    u8 u0, v0;
    u16 clut;
    u8 u1, v1;
    u16 tpage; /* bits 10-12: texture page reference, 0x200: loaded */
    u8 u2, v2;
    u8 u3, v3;
} TexInfo;
void *func_8001440C(u32 *ref, s32 count);
u32 NSOpen(u32 *en_ref, s32 flag, s32 count);
#ifdef NON_MATCHING
/* Loads the texture page a texture refers to and stores its location.
 * Equivalent; the original schedules the field loads after the arithmetic. */

u32 func_80019090(TexInfo *info, u32 *refs) {
    u32 idx = info->tpage >> 10;
    void *ref;
    u32 tp;
    u32 v;

    if (!(info->tpage & 0x200)) {
        ref = func_8001440C(&refs[idx], 0);
        if (ref != NULL) {
            tp = NSOpen(ref, 3, 0);
            v = tp & 0x80;
            info->tpage = (info->tpage & 0xFFE3) | (tp & 0x1C) | 0x200;
            info->clut = (info->clut & 0x9FCF) | ((tp >> 4) & 0x6030);
            info->v0 = (info->v0 & 0x7F) | v;
            info->v1 = (info->v1 & 0x7F) | v;
            info->v2 = (info->v2 & 0x7F) | v;
            info->v3 = (info->v3 & 0x7F) | v;
        }
    }
    return *(u32 *)refs[idx];
}
#else
INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80019090);
#endif

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80019180);

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_800192F8);
