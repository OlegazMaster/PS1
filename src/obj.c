#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

/* Object models: animation frames, collision boxes, drawing. */

u32 func_80031A54(s32 *a, s32 *b); /* ApxDist */

INCLUDE_ASM("asm/us/nonmatchings/obj", func_8001B0BC);

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
