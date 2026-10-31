#include "common.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"
#include "libgte.h"

/* Textures and sprites. */

void func_8001832C(s32 type, void *a, void *b, RECT *rect);
void func_800186D8(s32 load, RECT *rect, u8 *buf, u8 *data, s32 async);
s32 StoreImage(RECT *rect, u32 *data);
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

typedef struct {
    u8 unk0[0xC];
    char text[1]; /* 0xC: NUL-separated strings */
} TextBlock;
#ifdef NON_MATCHING
/* Writes len characters of src over string (idx >> 8) of a text block,
 * mapping the characters the font lacks, and pads it with spaces.
 * Equivalent; the original masks the character to 8 bits before every test. */

void func_800185C8(TextBlock *block, s32 idx, char *src, s32 len) {
    s32 pos = 0;
    s32 i = 0;
    char c;

    for (idx >>= 8; i < idx; i++) {
        while (block->text[pos] != 0) {
            pos++;
        }
        pos++;
    }
    i = 0;
    while (len != 0) {
        c = src[i];
        if (c == 0) {
            break;
        }
        if (c < 0x20) {
            c = ' ';
        }
        if (c == '{') {
            c = '(';
        }
        if (c == '}') {
            c = ')';
        }
        if (c == '~') {
            c = '{';
        }
        if (c == '%') {
            c = '}';
        }
        if (c == '&') {
            c = '_';
        }
        block->text[pos + i] = c;
        len--;
        i++;
    }
    while (len != 0) {
        block->text[pos + i] = ' ';
        len--;
        i++;
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/gfx", func_800185C8);
#endif

/* Moves a rectangle between VRAM and data (load 1: to VRAM). Data that is not
 * 16-byte aligned goes through buf. */
void func_800186D8(s32 load, RECT *rect, u8 *buf, u8 *data, s32 async) {
    u8 *src;
    u8 *dst;
    s32 n;

    if (load == 1) {
        if ((u32)data & 0xF) {
            src = data;
            dst = buf;
            n = rect->w * rect->h * 2;
            while (n--) {
                *dst++ = *src++;
            }
            LoadImage(rect, (u32 *)buf);
            DrawSync(0);
            return;
        }
        LoadImage(rect, (u32 *)data);
    } else {
        if ((u32)data & 0xF) {
            StoreImage(rect, (u32 *)buf);
            DrawSync(0);
            src = buf;
            dst = data;
            n = rect->w * rect->h * 2;
            while (n--) {
                *dst++ = *src++;
            }
            return;
        }
        StoreImage(rect, (u32 *)data);
    }
    if (!async) {
        DrawSync(0);
    }
}

s32 func_80018820(s32 load, void *a, void *b, u8 *data2, u8 *data1, s32 async) {
    RECT rect;
    u8 buf[0x110];
    u8 *aligned = (u8 *)((u32)(buf + 0xF) & ~0xF);

    if (data2 != NULL) {
        func_8001832C(2, a, b, &rect);
        func_800186D8(load, &rect, aligned, data2, async);
    }
    if (data1 != NULL) {
        func_8001832C(1, a, b, &rect);
        func_800186D8(load, &rect, aligned, data1, async);
    }
    return async;
}

typedef struct {
    u8 unk0[0x14];
    s32 unk14;
    s32 unk18;
    s32 unk1C;
} Unk80065C70;
extern Unk80065C70 D_80065C70;
void func_80018DA8(Transform *t);

void func_800188EC(void) {
    Transform *t;

    D_80065C70.unk14 = D_80065C70.unk18 = D_80065C70.unk1C = 0;
    SetGeomScreen(D_80065C90.unkC4 = 0x120);
    SetGeomOffset(0, 0);
    t = &D_80065C90.unk60;
    func_80018DA8(t);
    t->trans[0] = 0;
    t->trans[1] = 0;
    t->trans[2] = 0x1F400;
    t->rot[0] = 0;
    t->rot[1] = 0;
    t->rot[2] = 0;
    D_80065C90.unkA8 = 0;
    game.unk110 = 0;
    game.unk114 = 0;
}

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

void func_80018DA8(Transform *t) {
    t->rot[0] = 0;
    t->rot[1] = 0;
    t->rot[2] = 0;
    t->trans[0] = 0;
    t->trans[1] = 0;
    t->trans[2] = 0;
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
