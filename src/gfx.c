#include "common.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"
#include "libgte.h"

/* Textures and sprites. */

void func_8001832C(s32 type, void *a, void *b, RECT *rect);
void func_800186D8(s32 load, RECT *rect, u8 *buf, u8 *data, s32 async);
s32 StoreImage(RECT *rect, u32 *data);
/* Texture coordinates of a polygon: u, v, then CLUT or tpage, for 3 corners. */
typedef struct {
    u8 u0, v0;
    u16 clut;
    u8 u1, v1;
    u16 tpage; /* bits 10-12: texture page reference, 0x200: loaded */
    u8 u2, v2;
    u8 u3, v3;
} TexInfo;
u32 func_80019090(TexInfo *info, u32 *refs);
s32 func_80018088(Entry *entry, s32 flag, u32 eid, s32 count, TexInfo *infos, s32 ref_count, u32 *refs);

s32 func_80018008(Entry *entry, s32 flag, u32 eid) {
    u8 *hdr = entry->items[0];

    return func_80018088(entry, flag, eid, *(s32 *)(hdr + 0x1C), entry->items[4], *(s32 *)(hdr + 0x28), (u32 *)(hdr + 0x2C));
}

s32 func_80018048(Entry *entry, s32 flag, u32 eid) {
    u8 *hdr = entry->items[0];

    return func_80018088(entry, flag, eid, *(s32 *)(hdr + 0x34), entry->items[3], *(s32 *)(hdr + 0x40), (u32 *)(hdr + 0xC));
}

/* Updates the polygons infos[0..count) that use the texture page eid among
 * refs (all of them for eid 0x6396347F): after it was loaded (flag set) or
 * dropped. */

s32 func_80018088(Entry *entry, s32 flag, u32 eid, s32 count, TexInfo *infos, s32 ref_count, u32 *refs) {
    s32 idx = -1;
    s32 i;

    if (eid != 0x6396347F) {
        for (i = ref_count - 1; i >= 0; i--) {
            if ((refs[i] & 1) ? eid == refs[i] : eid == ((Entry *)refs[i])->eid) {
                idx = i;
                goto found;
            }
        }
        return -0xFF;
    }
found:
    if (flag) {
        for (i = count - 1; i >= 0; i--, infos++) {
            if (idx == -1 || idx == ((infos->tpage >> 10) & 7)) {
                func_80019090(infos, refs);
            }
        }
    } else {
        for (i = count - 1; i >= 0; i--, infos++) {
            if (idx == -1 || idx == ((infos->tpage >> 10) & 7)) {
                infos->tpage &= ~0x200;
            }
        }
    }
    return -0xFF;
}

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

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#ifdef NON_MATCHING
/* The VRAM rectangle of the texture (type 1) or the CLUT (type 2) of frame
 * frame of the sprite animation anim. Equivalent; the original keeps the
 * minima and maxima in other registers. For other animation types f is not
 * set, as in the original. */
void func_8001832C(s32 type, void *a, void *b, RECT *rect) {
    u8 *anim = a;
    s32 frame = (s32)b;
    u8 *f;
    TexInfo *t;
    u32 tpage;
    s32 mode;
    s32 minu;
    s32 minv;
    s32 maxu;
    s32 maxv;
    s32 w;
    s32 u0;
    s32 v0;
    s32 h;

    switch (anim[0]) {
    case 2:
        f = anim + ((frame >> 8) * 16 + 8);
        break;
    case 5:
        f = anim + ((frame >> 8) * 24 + 0xC);
        break;
    }
    t = (TexInfo *)(f + 4);
    func_80019090(t, (u32 *)(anim + 4));
    tpage = t->tpage;
    mode = (tpage >> 7) & 3;
    switch (type) {
    case 2:
        rect->x = (t->clut & 0x3F) << 4;
        rect->y = (t->clut >> 6) & 0x1FF;
        rect->w = mode == 0 ? 0x10 : 0x100;
        rect->h = 1;
        break;
    case 1:
        u0 = t->u0;
        v0 = t->v0;
        minu = MIN(u0, t->u1);
        minv = MIN(v0, t->v1);
        maxu = MAX(u0, t->u1);
        maxv = MAX(v0, t->v1);
        minu = MIN(minu, t->u2);
        minv = MIN(minv, t->v2);
        maxu = MAX(maxu, t->u2);
        maxv = MAX(maxv, t->v2);
        w = maxu - minu + 1;
        h = maxv - minv + 1;
        rect->x = (tpage & 0xF) << 6;
        rect->y = ((t->tpage << 4) & 0x100) + minv;
        switch (mode) {
        case 0:
            rect->x += minu / 4;
            rect->w = w / 4;
            break;
        case 1:
            rect->w = w / 2;
            rect->x += minu / 2;
            break;
        case 2:
            rect->w = w;
            rect->x += minu;
            break;
        }
        rect->h = h;
        break;
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/gfx", func_8001832C);
#endif

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

void func_80021E94(void);
s32 func_80032DF8(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g);
void func_80018EBC();
s32 func_8003C2F0(s32 angle); /* sin */
s32 func_8003C364(s32 angle); /* cos */
extern s32 paused;

#ifdef NON_MATCHING
/* Updates the camera for the frame: its matrices from unk60, the screen
 * offset with the shake (game.unk60[2]), the saved position and the
 * forward direction, then the lights of the current zone. Equivalent; the
 * original schedules the matrix copy and the saved position differently. */
void func_80018A54(void) {
    Transform *cam;
    MATRIX *m;
    MATRIX r;
    VECTOR v;
    s32 s;
    s32 c;
    s32 s2;
    s32 c2;
    s32 s3;
    s32 c3;
    s32 x;
    s32 a;
    s32 t;
    s32 *p;

    func_80021E94();
    cam = &D_80065C90.unk60;
    s = func_8003C2F0(-cam->rot[2]);
    c = func_8003C364(-cam->rot[2]);
    m = &D_80065C90.unk0;
    m->m[0][0] = c;
    m->m[0][1] = -s;
    m->m[0][2] = 0;
    m->m[1][0] = s;
    m->m[1][1] = c;
    m->m[1][2] = 0;
    m->m[2][0] = 0;
    m->m[2][1] = 0;
    m->m[2][2] = 0x1000;
    s2 = func_8003C2F0(-cam->rot[0]);
    c2 = func_8003C364(-cam->rot[0]);
    r.m[0][0] = 0x1000;
    r.m[0][1] = 0;
    r.m[0][2] = 0;
    r.m[1][0] = 0;
    r.m[1][1] = c2;
    r.m[1][2] = -s2;
    r.m[2][0] = 0;
    r.m[2][1] = s2;
    r.m[2][2] = c2;
    MulMatrix(m, &r);
    s3 = func_8003C2F0(-cam->rot[1]);
    c3 = func_8003C364(-cam->rot[1]);
    r.m[0][0] = c3;
    r.m[0][1] = 0;
    r.m[0][2] = s3;
    r.m[1][0] = 0;
    r.m[1][1] = 0x1000;
    r.m[1][2] = 0;
    r.m[2][0] = -s3;
    r.m[2][1] = 0;
    r.m[2][2] = c3;
    MulMatrix(m, &r);
    v.vx = cam->trans[0] >> 8;
    v.vy = cam->trans[1] >> 8;
    v.vz = cam->trans[2] >> 8;
    TransMatrix(m, &v);
    func_80018988(&D_80065C90.unk20, m);
    D_80065C90.unk40 = D_80065C90.unk20;
    SetGeomOffset(game.unk110 + 0x100, game.unk114 + (game.unk60[2] >> 8) + 0x6C);
    if (!paused) {
        x = game.unk60[2] >> 8;
        if (x == 0) {
            game.unk60[2] = 0;
        } else {
            game.unk60[2] = (x > 0 ? -x + 1 : ~x) << 8;
            if (game.unk220 == 0) {
                game.unk220 = 1;
                a = x;
                if (a < 0) {
                    a = -a;
                }
                a <<= 5;
                if (a >= 0xFF) {
                    game.unk224 = 0xFF;
                } else if (a < 0xC8) {
                    game.unk224 = 0xC8;
                } else {
                    game.unk224 = a;
                }
            }
        }
    }
    cam = &D_80065C90.unk60;
    game.cam_trans[0] = cam->trans[0];
    game.cam_trans[1] = cam->trans[1];
    game.cam_trans[2] = cam->trans[2];
    game.cam_rot[0] = cam->rot[0];
    game.cam_rot[1] = cam->rot[1];
    game.cam_rot[2] = cam->rot[2];
    t = func_80032DF8(D_80065C90.unkD4, 0x29, 0, 0, 0, 2, 0);
    if (t != 8 && t != 0x800 && t != 0x1000) {
        D_80065C90.unk114 = D_80065C90.unk84.rot[1] & 0xFFF;
    }
    game.unk9C = D_80065C90.unk114;
    p = D_80065C90.unk130;
    p[0] = func_8003C2F0(D_80065C90.unk114) >> 4;
    p[1] = 0;
    p[2] = func_8003C364(D_80065C90.unk114) >> 4;
    func_80018EBC((void *)((Entry *)D_80065C90.unkCC)->items[0]);
}
#else
INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80018A54);
#endif

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

/* A light source read from an entry. */
typedef struct {
    u32 eid;
    s32 pos[3]; /* 0x4 */
    s32 *item; /* 0x10 */
    void *unk14;
    void *unk18;
    void *unk1C;
    void *unk20;
    void *unk24;
    void *unk28;
    void *unk2C;
} Light; /* 0x30 */
typedef struct {
    s32 count;
    Light lights[1]; /* 0x4 */
} LightSet;

#ifdef NON_MATCHING
/* Loads the lights of set enabled in the current zone (bits of property
 * 0x27F of its entity). Equivalent; the original addresses the light
 * through other loop variables. */
void func_80018EBC(LightSet *set) {
    s32 mask;
    s32 i;
    Entry *e;
    s32 *item;
    s32 *p;
    s32 v[3];

    mask = func_80032DF8(D_80065C90.unkD4, 0x27F, -1, D_80065C90.unkE8, 0, 2, 0);
    D_80065C90.unk144 = mask & 0xFF;
    for (i = 0; i < set->count; i++) {
        if (((mask & 0xFF) >> i) & 1) {
            e = (Entry *)NSOpen(&set->lights[i].eid, 1, 0);
            item = (s32 *)e->items[0];
            if (item[3] == 1) {
                p = set->lights[i].pos;
                p[0] = item[0] >> 2;
                p[1] = item[1] >> 2;
                p[2] = item[2] >> 2;
            } else {
                v[0] = (item[0] >> 2) - (D_80065C90.unk60.trans[0] >> 10);
                v[1] = (item[1] >> 2) - (D_80065C90.unk60.trans[1] >> 10);
                v[2] = (item[2] >> 2) - (D_80065C90.unk60.trans[2] >> 10);
                p = set->lights[i].pos;
                p[0] = v[0];
                p[1] = v[1];
                p[2] = v[2];
            }
            set->lights[i].item = item;
            set->lights[i].unk1C = e->items[1];
            set->lights[i].unk14 = e->items[2];
            set->lights[i].unk18 = e->items[3];
            set->lights[i].unk28 = e->items[4];
            set->lights[i].unk20 = e->items[5];
            set->lights[i].unk2C = item + 11;
            set->lights[i].unk24 = e->items[6];
        }
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80018EBC);
#endif

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

#include "gte_inline.h"
typedef struct {
    u32 unk0;
    u32 unk4;
    u32 *prims; /* 0x8: next free primitive */
} PrimBuf;
extern PrimBuf *D_80061A84[];
/* Projects the corners of the rectangle (v[0], v[4]) - (v[3], v[1]) (in the
 * camera space, z 0) and adds it as a textured quad (POLY_FT4 with the
 * color, clut and texture page words of p) to the ordering table ot at
 * its depth (offset by z). The third argument is not used, only reused as
 * the scratch register for the GTE values. */
void func_80019180(u32 *p, s32 z, s32 t, s32 *v, u32 *ot) {
    u32 *prim;
    u8 *q;
    u32 uv;
    u32 *slot;
    s32 sz1;

    t = (u16)v[0] | (v[4] << 16);
    gte_mtc2(t, 0);
    gte_mtc2(0, 1);
    t = (u16)v[3] | (v[4] << 16);
    gte_mtc2(t, 2);
    gte_mtc2(0, 3);
    t = (u16)v[0] | (v[1] << 16);
    gte_mtc2(t, 4);
    gte_mtc2(0, 5);
    gte_rtpt();
    gte_stflg(t);
    if (t < 0) {
        return;
    }
    q = (u8 *)D_80061A84[0]->prims;
    D_80061A84[0]->prims = (u32 *)(q + 0x28);
    prim = (u32 *)q;
    prim[1] = p[0];
    gte_mfc2(sz1, 17);
    gte_mfc2(t, 18);
    gte_stsxy(14, prim[6]);
    sz1 += t;
    gte_mfc2(t, 19);
    gte_stsxy(13, prim[4]);
    sz1 = z + (0x800 - D_80065C90.unkC4 / 2) - ((u32)(sz1 + t) >> 5);
    if (sz1 < 0x7FF) {
        slot = ot;
        if (sz1 >= 0) {
            slot += sz1;
        }
    } else {
        slot = ot + 0x7FF;
    }
    prim[0] = *slot | 0x09000000;
    *slot = (u32)prim & 0xFFFFFF;
    t = (u16)v[3] | (v[1] << 16);
    gte_stsxy(12, prim[2]);
    gte_mtc2(t, 0);
    gte_mtc2(0, 1);
    gte_rtps();
    prim[3] = p[1];
    prim[5] = p[2];
    uv = p[3];
    ((u16 *)prim)[14] = uv;
    uv >>= 16;
    ((u16 *)prim)[18] = uv;
    gte_stsxy(14, prim[8]);
}

/* Like func_80019180 for a glyph g of a font of obj (color bytes 0-2, code
 * byte 3, then the clut, texture page and uv words): adds it as a Gouraud
 * shaded textured quad (POLY_GT4). With shade the corner colors are the
 * glyph color scaled by the colors a, b, c and cur of obj, and
 * with link the quad goes after the primitive already at its depth. */

void func_800192F8(GoolObject *obj, u8 *g, s32 z, s32 t, s32 *v, u32 *ot, s32 shade, s32 link) {
    u32 *prim;
    u8 *q;
    u32 uv;
    u32 *slot;
    s32 sz1;

    t = (u16)v[0] | (v[4] << 16);
    gte_mtc2(t, 0);
    gte_mtc2(0, 1);
    t = (u16)v[3] | (v[4] << 16);
    gte_mtc2(t, 2);
    gte_mtc2(0, 3);
    t = (u16)v[0] | (v[1] << 16);
    gte_mtc2(t, 4);
    gte_mtc2(0, 5);
    gte_rtpt();
    gte_stflg(t);
    if (t < 0) {
        return;
    }
    q = (u8 *)D_80061A84[0]->prims;
    D_80061A84[0]->prims = (u32 *)(q + 0x34);
    prim = (u32 *)q;
    if (shade) {
        ((u8 *)prim)[4] = (g[0] * obj->colors.color_a.r) >> 7;
        ((u8 *)prim)[5] = (g[1] * obj->colors.color_a.g) >> 7;
        ((u8 *)prim)[6] = (g[2] * obj->colors.color_a.b) >> 7;
        ((u8 *)prim)[16] = (g[0] * obj->colors.color_b.r) >> 7;
        ((u8 *)prim)[17] = (g[1] * obj->colors.color_b.g) >> 7;
        ((u8 *)prim)[18] = (g[2] * obj->colors.color_b.b) >> 7;
        ((u8 *)prim)[28] = (g[0] * obj->colors.color_c.r) >> 7;
        ((u8 *)prim)[29] = (g[1] * obj->colors.color_c.g) >> 7;
        ((u8 *)prim)[30] = (g[2] * obj->colors.color_c.b) >> 7;
        ((u8 *)prim)[40] = (g[0] * obj->colors.cur_color.r) >> 7;
        ((u8 *)prim)[41] = (g[1] * obj->colors.cur_color.g) >> 7;
        ((u8 *)prim)[42] = (g[2] * obj->colors.cur_color.b) >> 7;
    } else {
        uv = *(u32 *)g;
        prim[1] = uv;
        prim[4] = uv;
        prim[7] = uv;
        prim[10] = uv;
    }
    gte_mfc2(sz1, 17);
    ((u8 *)prim)[7] = g[3] | 0x3C;
    gte_mfc2(t, 18);
    gte_stsxy(14, prim[8]);
    sz1 += t;
    gte_mfc2(t, 19);
    gte_stsxy(13, prim[5]);
    sz1 = z + (0x800 - D_80065C90.unkC4 / 2) - ((u32)(sz1 + t) >> 5);
    if (sz1 < 0x7FF) {
        slot = ot;
        if (sz1 >= 0) {
            slot += sz1;
        }
    } else {
        slot = ot + 0x7FF;
    }
    z = *slot;
    if (link && (z & 0xFFFFFF) != 0xFFFFFF) {
        slot = (u32 *)(((u32)slot & 0xFF000000) | (z & 0xFFFFFF));
        z = *slot;
    }
    prim[0] = z | 0x0C000000;
    *slot = ((u32)prim & 0xFFFFFF) | (z & 0xFF000000);
    t = (u16)v[3] | (v[1] << 16);
    gte_stsxy(12, prim[2]);
    gte_mtc2(t, 0);
    gte_mtc2(0, 1);
    gte_rtps();
    prim[3] = ((u32 *)g)[1];
    prim[6] = ((u32 *)g)[2];
    uv = ((u32 *)g)[3];
    ((u16 *)prim)[18] = uv;
    uv >>= 16;
    ((u16 *)prim)[24] = uv;
    gte_stsxy(14, prim[11]);
}
