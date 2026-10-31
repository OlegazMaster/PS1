#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"
#include "libgpu.h"

/* Object models: animation frames, collision boxes, drawing. */

/* .sdata */
s32 D_80060920 = 0;

/* .sbss */
s32 D_80060A98;
s32 D_80060A9C;

u32 func_80031A54(s32 *a, s32 *b); /* ApxDist */
u32 func_8001B524(GoolObject *obj, u32 *frame_a, u32 *frame_b);

extern GoolObject *crash;

/* A frame of a model animation (entry item). */
typedef struct {
    u8 unk0[0x20];
    s32 trans[3]; /* 0x20 */
    s32 min[3]; /* 0x2C */
    s32 max[3]; /* 0x38 */
} ModelFrame;

/* The model state of an object (GoolObject.unk8). */
typedef struct {
    u8 unk0[0x20];
    u32 frame_a; /* 0x20 */
    u32 frame_b; /* 0x24 */
    s32 unk28; /* the model/texture entry, 0 or an error code */
    s32 frame; /* 0x2C */
    s32 box[6]; /* 0x30 */
} ObjModel;

/* Opens the entries of the current animation of obj; for the player it also
 * scales the collision box of the frame by scale (x taken as positive). */
void func_8001B0BC(GoolObject *obj, s32 *scale) {
    u8 *anim;
    ModelFrame *frame;
    u32 *eid;
    s32 res;
    s32 sx;
    s32 *p;
    s32 b[6];

    anim = (u8 *)obj->unkE8;
    ((ObjModel *)obj->unk8)->unk28 = 0;
    if (anim == NULL) {
        return;
    }
    if (*anim != 4) {
        if (*anim == 1) {
            eid = (u32 *)(anim + 0x10);
        } else {
            eid = (u32 *)(anim + 4);
        }
        if (((u32)eid & 0xFF800003) != 0x80000000) {
            return;
        }
        res = NSOpen(eid, 1, 0);
        ((ObjModel *)obj->unk8)->unk28 = res;
        if (res > -0xFF) {
            ((ObjModel *)obj->unk8)->unk28 = 0;
            return;
        }
    }
    if (*(u8 *)obj->unkE8 == 1) {
        ((ObjModel *)obj->unk8)->frame = func_8001B524(obj, &((ObjModel *)obj->unk8)->frame_a, &((ObjModel *)obj->unk8)->frame_b);
        frame = (ModelFrame *)((ObjModel *)obj->unk8)->frame_a;
        ((ObjModel *)obj->unk8)->unk28 = NSOpen((u32 *)(anim + 0xC), 1, 0);
        if (obj != crash) {
            return;
        }
        sx = scale[0];
        if (sx < 0) {
            scale[0] = -sx;
        }
        b[0] = frame->trans[0] + frame->min[0];
        b[1] = frame->trans[1] + frame->min[1];
        b[2] = frame->trans[2] + frame->min[2];
        b[3] = frame->trans[0] + frame->max[0];
        b[4] = frame->trans[1] + frame->max[1];
        b[5] = frame->trans[2] + frame->max[2];
        p = b;
        ((ObjModel *)obj->unk8)->box[0] = (p[0] >> 8) * scale[0] >> 4;
        ((ObjModel *)obj->unk8)->box[1] = (p[1] >> 8) * scale[1] >> 4;
        ((ObjModel *)obj->unk8)->box[2] = (p[2] >> 8) * scale[2] >> 4;
        ((ObjModel *)obj->unk8)->box[3] = (p[3] >> 8) * scale[0] >> 4;
        ((ObjModel *)obj->unk8)->box[4] = (p[4] >> 8) * scale[1] >> 4;
        ((ObjModel *)obj->unk8)->box[5] = (p[5] >> 8) * scale[2] >> 4;
        scale[0] = sx;
    } else {
        ((ObjModel *)obj->unk8)->unk28 = -0xE;
    }
}

s32 func_8003191C(s32 *box_a, s32 *box_b); /* boxes overlap */
/* Makes obj and other collide (unless obj already collides with a nearer
 * object) and marks them when their hotspot boxes overlap. */

s32 func_8001B328(GoolObject *obj, s32 *box, GoolObject *other, s32 *other_box) {
    GoolObject *prev = obj->collider;
    s32 dist;
    s32 b[6];
    s32 b2[6];

    if (prev != NULL && prev != other) {
        if (other->state_flags & 0x200) {
            other->collider = obj;
            return -0x1C;
        }
        dist = func_80031A54(obj->xform.trans, prev->xform.trans);
        if ((s32)func_80031A54(obj->xform.trans, other->xform.trans) >= dist && !(prev->state_flags & 0x200)) {
            return -0x1C;
        }
    }
    obj->collider = other;
    other->collider = obj;
    if (box != NULL) {
        if (other_box == NULL) {
            return -0xFF;
        }
        if (obj->hotspot_size != 0) {
            b[0] = box[0] + obj->hotspot_size;
            b[1] = box[1];
            b[2] = box[2] + obj->hotspot_size;
            b[3] = box[3] - obj->hotspot_size;
            b[4] = box[4];
            b[5] = box[5] - obj->hotspot_size;
            if (func_8003191C(b, other_box)) {
                obj->status_a |= 0x1000;
            }
        }
        if (other->hotspot_size != 0) {
            b2[0] = other_box[0] + other->hotspot_size;
            b2[1] = other_box[1];
            b2[2] = other_box[2] + other->hotspot_size;
            b2[3] = other_box[3] - other->hotspot_size;
            b2[4] = other_box[4];
            b2[5] = other_box[5] - other->hotspot_size;
            if (func_8003191C(b2, box)) {
                other->status_a |= 0x1000;
            }
        }
    }
    return -0xFF;
}

typedef struct {
    u8 unk0[0x2];
    u16 frame_count; /* 0x2 */
    u32 flags; /* 0x4: bit 0: frames at half rate */
    u8 unk8[0x8];
    u32 eid; /* 0x10 */
} AnimInfo;
#ifdef NON_MATCHING
/* Finds the frame (or the two frames to blend) for the current animation
 * frame of obj; returns that frame number. Equivalent; the original keeps
 * the entry and the frame in other registers. */

u32 func_8001B524(GoolObject *obj, u32 *frame_a, u32 *frame_b) {
    AnimInfo *anim = (AnimInfo *)obj->unkE8;
    Entry *entry = (Entry *)NSOpen(&anim->eid, 1, 0);
    s32 half = anim->flags & 1;
    u32 frame = obj->unkEC >> 8;

    if (frame_b == NULL) {
        *frame_a = (u32)entry->items[half ? frame >> 1 : frame];
        return frame;
    }
    if (!half) {
        *frame_a = *frame_b = (u32)entry->items[frame];
    } else if ((frame & 1) && (s32)(frame + 1) < anim->frame_count) {
        *frame_a = (u32)entry->items[frame >> 1];
        *frame_b = (u32)entry->items[(frame >> 1) + 1];
    } else {
        *frame_a = *frame_b = (u32)entry->items[(s32)(frame + 1) >> 1];
    }
    return frame;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/obj", func_8001B524);
#endif

extern s32 D_8005C1F0[];
extern void (*D_8005C1B0[])(void *, s32 *, s32 *, s32);
extern s32 D_80067A30;
int abs(int x);
extern s32 D_80067A34;

#ifdef NON_MATCHING
/* Calls the drawing function for the model type of obj (an index at 0x120,
 * with a default). Equivalent; the original loads the default index with the
 * high part of its address in another register. */
void func_8001B630(GoolObject *obj, s32 *out) {
    u8 *anim = (u8 *)obj->unkE8;
    u32 idx;
    u32 idx2;

    if (anim != NULL) {
        switch (*anim) {
        case 2:
        case 5:
            out[1] = abs(obj->xform.scale[0]) / 27279;
            idx = (u8)obj->unk120;
            if ((out[0] = D_8005C1F0[idx]) == 0) {
                idx = D_80067A30;
                out[0] = D_8005C1F0[idx];
            }
            D_8005C1B0[idx](obj->unk8, obj->xform.rot, obj->xform.scale, 0);
            break;
        case 1:
            idx2 = (u8)obj->unk120;
            if ((out[0] = D_8005C1F0[idx2]) == 0) {
                idx2 = D_80067A34;
                out[0] = D_8005C1F0[idx2];
            }
            D_8005C1B0[idx2](obj->unk8, obj->xform.rot, obj->xform.scale, *(s32 *)(*(u8 **)((u8 *)obj->unk8 + 0x28) + 0x10));
            break;
        }
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/obj", func_8001B630);
#endif

INCLUDE_ASM("asm/us/nonmatchings/obj", func_8001B7A0);

INCLUDE_ASM("asm/us/nonmatchings/obj", func_8001C3F8);

INCLUDE_ASM("asm/us/nonmatchings/obj", func_8001C824);

void func_8001CA60(s16 *a) {
    a[0x12] = 0;
}

INCLUDE_ASM("asm/us/nonmatchings/obj", func_8001CA68);

void func_8001832C(s32 type, void *a, void *b, RECT *rect);
extern RECT D_8006AEC8[];
/* Uploads the texture of the animation of obj to slot idx of D_8006AEC8. */

void func_8001CD28(GoolObject *obj, s32 unused, s32 idx) {
    func_8001832C(1, (void *)obj->unkE8, NULL, &D_8006AEC8[idx]);
    D_80060920 |= 1 << idx;
}

s32 func_8003C2F0(s32 angle); /* sin */
s32 func_8003C364(s32 angle); /* cos */
typedef struct {
    s32 unk0;
    u8 unk4[0x4];
    s32 unk8;
    s32 unkC;
    u8 unk10[0xC];
    u32 flags; /* 0x1C */
    u8 unk20[0x8];
    s32 unk28;
    s32 unk2C;
} ObjDrawState;
void func_8003F3AC(ObjDrawState *state, DrawContext *ctx, RenderState *rs);
extern s32 D_8005C244[];
extern s32 D_8005C264[];
extern s32 D_8005C288[];
extern s32 D_8005C28C[];
extern s32 D_8005C310[];
extern s32 D_8005C314[];
/* Draws the model of obj (posed like model) into the texture slot idx of
 * D_8006AEC8, e.g. for the counters of the interface. */

void func_8001CD80(GoolObject *obj, GoolObject *model, ObjDrawState *state, s32 idx) {
    BLK_FILL *fill = GpuAllocPrim(BLK_FILL);
    DR_ENV *env_prim = GpuAllocPrim(DR_ENV);
    DR_ENV *env_tex = GpuAllocPrim(DR_ENV);
    RECT *rect = &D_8006AEC8[idx];
    DRAWENV env;
    MATRIX m;
    MATRIX rot;
    s32 s;
    s32 c;
    s32 s2;
    s32 c2;
    s32 ofx;
    s32 ofy;
    u32 status;

    if (state == NULL || obj == NULL || obj->unkE8 == 0 || model == NULL || model->unkE8 == 0 || *(u8 *)model->unkE8 != 1 || (obj->status_b & 0x100)) {
        return;
    }
    if (!((D_80060920 >> idx) & 1)) {
        func_8001CD28(obj, (s32)model, idx);
    }
    SetDrawEnv(env_tex, &context.cur->draw);
    AddPrim(context.cur->ot, env_tex);
    s = func_8003C2F0(0x400);
    c = func_8003C364(0x400);
    m.m[0][0] = 0x1000;
    m.m[0][1] = 0;
    m.m[0][2] = 0;
    m.m[1][0] = 0;
    m.m[1][1] = c;
    m.m[1][2] = -s;
    m.m[2][0] = 0;
    m.m[2][1] = s;
    m.m[2][2] = c;
    s2 = func_8003C2F0(-obj->xform.rot[1]);
    c2 = func_8003C364(-obj->xform.rot[1]);
    rot.m[0][0] = c2;
    rot.m[0][1] = 0;
    rot.m[0][2] = s2;
    rot.m[1][0] = 0;
    rot.m[1][1] = 0x1000;
    rot.m[1][2] = 0;
    rot.m[2][0] = -s2;
    rot.m[2][1] = 0;
    rot.m[2][2] = c2;
    MulMatrix(&m, &rot);
    m.t[0] = obj->xform.trans[0] >> 8;
    if (model->state_flags & 0x10) {
        m.t[1] = (obj->xform.trans[1] - 0x258000) >> 8;
    } else {
        m.t[1] = (obj->xform.trans[1] - 0x1A9000) >> 8;
    }
    m.t[2] = obj->xform.trans[2] >> 8;
    ReadGeomOffset(&ofx, &ofy);
    SetGeomOffset(rect->w / 2, rect->h / 2);
    SetGeomScreen(0x1EB);
    D_80065C90.render.unkC = &m;
    status = model->status_a;
    state->unk0 = 0;
    state->unk28 = D_8005C244[0];
    state->unk2C = D_8005C264[0];
    if (state->flags & 0x20000000) {
        state->unk8 = D_8005C314[0];
        state->unkC = D_8005C28C[0];
    } else {
        state->unk8 = D_8005C310[0];
        state->unkC = D_8005C288[0];
    }
    state->flags |= 0x10000000;
    func_8003F3AC(state, context.cur, &D_80065C90.render);
    SetGeomScreen(D_80065C90.unkC4);
    SetGeomOffset(ofx, ofy);
    model->status_a = status;
    setBlockFill(fill);
    fill->r0 = 0;
    fill->g0 = 0;
    fill->b0 = 0;
    fill->w = rect->w;
    fill->h = rect->h;
    fill->x0 = rect->x;
    fill->y0 = rect->y;
    AddPrim(context.cur->ot, fill);
    SetDefDrawEnv(&env, rect->x, rect->y, rect->w, rect->h);
    env.dfe = 0;
    SetDrawEnv(env_prim, &env);
    AddPrim(context.cur->ot, env_prim);
}
