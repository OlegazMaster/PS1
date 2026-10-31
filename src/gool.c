#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

/* GOOL objects: allocation, the object tree, messages (names follow c1). */

/* .sbss */
GoolObject *crash; /* the player object */
s32 D_80060A94; /* entries used in D_80067A38 */

s32 func_80019624(void) {
    return -0xFF;
}


#ifdef NON_MATCHING
/* Moves child to the front of the children of parent (c1: GoolObjectAddChild).
 * Equivalent; the original tests cur for NULL twice in the search loop. */

s32 GoolObjectAddChild(GoolObject *child, GoolObject *parent) {
    GoolObject *old = child->parent;
    GoolObject *cur;

    if (old != NULL) {
        cur = GoolObjectGetChildren(old);
        if (cur == child) {
            GoolObjectSetChildren(old, child->sibling);
        } else {
            while (1) {
                if (cur == NULL) {
                    return -0x18;
                }
                if (cur->sibling == child) {
                    break;
                }
                cur = cur->sibling;
            }
            cur->sibling = child->sibling;
        }
    }
    child->parent = parent;
    child->sibling = GoolObjectGetChildren(parent);
    GoolObjectSetChildren(parent, child);
}
#else
INCLUDE_ASM("asm/us/nonmatchings/gool", GoolObjectAddChild);
#endif

s32 GoolObjectTraverseTreePreorder(GoolObject *obj, GoolTraverseFunc func, s32 arg) {
    GoolObject *child;
    GoolObject *sibling;
    s32 res;

    res = func(obj, arg);
    if (res == ERROR_INVALID_RETURN) {
        GoolObjectKill(obj, 0);
        return ERROR_INVALID_RETURN;
    }
    child = obj->children;
    while (child != NULL) {
        if (child->type == 0) {
            break;
        }
        sibling = child->sibling;
        GoolObjectTraverseTreePreorder(child, func, arg);
        child = sibling;
    }
    return res;
}

s32 GoolObjectTraverseTreePostorder(GoolObject *obj, GoolTraverseFunc func, s32 arg) {
    GoolObject *child;
    GoolObject *sibling;

    child = obj->children;
    while (child != NULL) {
        sibling = child->sibling;
        if (GoolObjectTraverseTreePostorder(child, func, arg) == ERROR_INVALID_RETURN) {
            GoolObjectKill(obj, 0);
        }
        child = sibling;
    }
    return func(obj, arg);
}

s32 GoolObjectSearchTree(GoolObject *obj, GoolTraverseFunc func, s32 arg) {
    GoolObject *child;
    GoolObject *sibling;
    s32 res = 0;

    if (obj == NULL) {
        return 0;
    }
    if (!ISHANDLE(obj)) {
        res = func(obj, arg);
        if (res != 0) {
            return res;
        }
    }
    child = GoolObjectGetChildren(obj);
    while (child != NULL && res == 0) {
        sibling = child->sibling;
        res = GoolObjectSearchTree(child, func, arg);
        child = sibling;
    }
    return res;
}

s32 GoolObjectHandleTraverseTreePreorder(GoolObject *obj, GoolTraverseFunc func, s32 arg) {
    GoolObject *child;
    GoolObject *sibling;

    child = GoolObjectGetChildren(obj);
    while (child != NULL) {
        sibling = child->sibling;
        GoolObjectTraverseTreePreorder(child, func, arg);
        child = sibling;
    }
    return -0xFF;
}

s32 GoolObjectHandleTraverseTreePostorder(GoolObject *obj, GoolTraverseFunc func, s32 arg) {
    GoolObject *child;
    GoolObject *sibling;

    child = GoolObjectGetChildren(obj);
    while (child != NULL) {
        sibling = child->sibling;
        if (GoolObjectTraverseTreePostorder(child, func, arg) == ERROR_INVALID_RETURN && !ISHANDLE(obj)) {
            GoolObjectKill(obj, 0);
        }
        child = sibling;
    }
    return -0xFF;
}

/* Calls func for each of the 8 object lists. */

void GoolForEachHandle(s32 (*func)(GoolObject *, GoolTraverseFunc, s32), GoolTraverseFunc arg1, s32 arg2) {
    s32 i;

    for (i = 0; i < 8; i++) {
        func((GoolObject *)&game.handles[i], arg1, arg2);
    }
}

GoolObject *GoolObjectHasPidFlags(GoolObject *obj, s32 pid_flags) {
    if (obj->pid_flags == pid_flags) {
        return obj;
    }
    return NULL;
}

typedef struct {
    u32 categories;
    GoolObject *obj;
    GoolObject *nearest_obj;
    s32 dist;
    u32 event;
} GoolNearestQuery;
u32 func_80031A54(s32 *a, s32 *b); /* ApxDist */
s32 func_8001E42C(GoolObject *obj, u32 state, s32 argc, u32 *argv); /* GoolObjectInterrupt */
#ifdef NON_MATCHING
/* Keeps the nearest object that matches the categories of the query and
 * would accept its event (c1: GoolFindNearestObject). Equivalent; the
 * original loads status_c and the state flags into other registers. */

s32 func_80019ACC(GoolObject *obj, GoolNearestQuery *query) {
    s32 dist;
    u32 state_idx;
    u32 flag;
    u32 sflags;
    u32 status;
    u32 st;
    u32 arg;

    if (obj == query->obj) {
        return 0;
    }
    if (!((1 << (((s32 *)obj->global->items[0])[1] >> 8)) & query->categories)) {
        return 0;
    }
    dist = func_80031A54(obj->xform.trans, query->obj->xform.trans);
    if (dist >= query->dist) {
        return 0;
    }
    if ((s32)query->event != 0xFF) {
        state_idx = ((u16 *)obj->global->items[3])[(s32)query->event >> 8];
        if (state_idx == 0xFF) {
            if ((query->event != 0x300 || (st = obj->status_c, (u32)(obj->invincibility_state - 2) < 3) || (st & 2))
                && (query->event != 0xA00 || (obj->state_flags & 0x200))) {
                if (query->event == 0x1D00 && !(obj->state_flags & 0x800000)) {
                    state_idx = 0xFE;
                } else {
                    return 0;
                }
            } else {
                state_idx = 0xFE;
            }
        } else {
            if (!(state_idx & 0x8000)) {
                status = obj->status_c;
                sflags = ((u32 *)obj->global->items[4])[state_idx * 4];
                if ((u32)(obj->invincibility_state - 2) < 3) {
                    flag = (status | 0x402) & sflags;
                } else {
                    flag = status & sflags;
                }
                if (flag) {
                    return 0;
                }
            }
            if ((state_idx & 0x8000) && query->event == 0xF00) {
                arg = 0x100;
                obj->interrupter = query->obj;
                func_8001E42C(obj, state_idx, 1, &arg);
                if (!query->obj->ack) {
                    return 0;
                }
            }
        }
    } else {
        state_idx = 0xFE;
    }
    if (state_idx != 0xFF) {
        query->nearest_obj = obj;
        query->dist = dist;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/gool", func_80019ACC);
#endif

GoolObject *GoolObjectTestStateFlags(GoolObject *obj, u32 flags) {
    if (obj->state_flags & flags) {
        return obj;
    }
    return NULL;
}

INCLUDE_ASM("asm/us/nonmatchings/gool", func_80019CE4);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001A42C);

s32 func_80032DF8(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g);
s32 GoolObjectChangeState(GoolObject *obj, u32 state, s32 argc, u32 *argv);
#ifdef NON_MATCHING
/* Resets obj and starts it as subtype of the GOOL executable exec (c1).
 * Equivalent; the original tests the result of GoolObjectChangeState in v0. */

s32 GoolObjectInit(GoolObject *obj, s32 exec, s32 subtype, s32 argc, u32 *argv, s32 keep) {
    GoolObject *parent;
    Entry *global;
    u32 *p_eid;
    u32 state;

    if (!keep) {
        obj->pid_flags = 0;
        obj->unkF0 = 0;
        obj->unkF8 = 0;
        obj->unk120 = 0;
    } else {
        obj->unk120 = func_80032DF8(obj->unkF0, 0x47, 0, 0, 0, 2, 0);
    }
    parent = obj->parent;
    obj->voice_id = -2;
    obj->unkF4 = 0;
    obj->type = 1;
    obj->misc_a[0] = 0;
    obj->misc_a[1] = 0;
    obj->misc_a[2] = 0;
    obj->misc_b[0] = 0;
    obj->misc_b[1] = 0;
    obj->misc_b[2] = 0;
    obj->misc_c[0] = 0;
    obj->misc_c[1] = 0;
    obj->misc_c[2] = 0;
    obj->speed = 0;
    obj->unkEC = 0;
    obj->ack = 0;
    obj->misc_b[1] = 0;
    obj->status_a = 0;
    obj->status_c = 0;
    obj->status_b = 0;
    obj->invincibility_state = 0;
    obj->unk118 = 0;
    obj->unk110 = 0;
    obj->hotspot_size = 0;
    obj->node = 0xFFFF;
    if (parent->type == 1) {
        obj->zone = parent->zone;
        obj->xform = obj->parent->xform;
    } else {
        obj->zone = NULL;
        obj->xform.rot[0] = 0;
        obj->xform.rot[1] = 0;
        obj->xform.rot[2] = 0;
        obj->xform.scale[0] = 0x1000;
        obj->xform.scale[1] = 0x1000;
        obj->xform.scale[2] = 0x1000;
    }
    if (exec >= 4 && (exec < 6 || exec == 29)) {
        obj->zone = NULL;
    }
    if (obj->subtype == 3) {
        p_eid = &ns.ldat->exec_map[exec];
        if (*p_eid != 0x6396347F && exec < 128) {
            obj->global = (Entry *)NSOpen(p_eid, 1, 0);
            obj->unk10 = obj->global;
            if ((s32)obj->global < -0xFE) {
                obj->self = obj;
                obj->collider = NULL;
                obj->creator = NULL;
                obj->interrupter = NULL;
                obj->player = game.player;
                obj->id = (exec << 16) | subtype;
                obj->unkE8 = 0;
                obj->once_p = 0;
                state = ((u16 *)obj->global->items[3])[((s32 *)obj->global->items[0])[4] + (u16)obj->id];
                if (state == 0xFF) {
                    return -0x1B;
                }
                if (GoolObjectChangeState(obj, state, argc, argv) < -0xFE) {
                    return -0xFF;
                }
            }
        }
    }
    return -0xE;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/gool", GoolObjectInit);
#endif

extern u32 spawns[];
void AudioVoiceFree(GoolObject *obj);
#ifdef NON_MATCHING
/* Kills obj and its descendants and returns them to the free list (c1).
 * Equivalent; the original does not move the store to crash before the last
 * store through game.player. */

s32 GoolObjectKill(GoolObject *obj, s32 sig) {
    GoolObject *child;
    GoolObject *next;
    GoolObject *children;

    if (obj != NULL && obj != crash) {
        if (sig) {
            GoolSendEvent(NULL, obj, 0x1A00, 0, NULL);
        }
        child = obj->children;
        while (child != NULL) {
            next = child->sibling;
            GoolObjectKill(child, sig);
            child = next;
        }
        AudioVoiceFree(obj);
        spawns[obj->pid_flags >> 8] &= ~1;
        obj->type = 0;
        if (obj == crash) {
            children = GoolObjectGetChildren((GoolObject *)&game.free_objects);
            GoolObjectAddChild(obj, (GoolObject *)&game.free_objects);
            GoolObjectSetChildren((GoolObject *)&game.free_objects, children);
            game.player->parent = NULL;
            game.player->children = NULL;
            game.player->sibling = NULL;
            crash = NULL;
        } else {
            GoolObjectAddChild(obj, (GoolObject *)&game.free_objects);
        }
    }
    return -0xFF;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/gool", GoolObjectKill);
#endif

extern Entry *D_80065D60;

#ifdef NON_MATCHING
/* Kills an object of zone when the zone goes away (c1). Equivalent; the
 * original compares obj->zone first and loads the -1 later. */
s32 GoolZoneObjectTerminate(GoolObject *obj, Entry *zone) {
    if (obj->pid_flags != 0 || obj->type == 0 || zone != obj->zone || (obj->status_b & 0x1000000)
        || (obj->state_flags & 0x10000)) {
        return -0xFF;
    }
    GoolSendEvent(NULL, obj, 0x1A00, 0, NULL);
    if (obj->zone == zone || (s32)D_80065D60 == -1) {
        return GoolObjectKill(obj, 0);
    }
    return -0xFF;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/gool", GoolZoneObjectTerminate);
#endif

void GoolObjectTerminate(GoolObject *obj) {
    GoolObjectKill(obj, 1);
}


void GoolZoneObjectsTerminate(s32 arg) {
    GoolForEachHandle(GoolObjectHandleTraverseTreePostorder, (GoolTraverseFunc)GoolZoneObjectTerminate, arg);
}

typedef struct {
    u8 unk0[0x190];
    s32 neighbor_count; /* 0x190 */
    u32 neighbors[1]; /* 0x194: references to the neighbor zones */
} ZoneHeader;
extern Entry *cur_zone;
/* Kills the objects of the zones next to the current one (c1:
 * GoolZoneNeighborObjectsTerminate). */

void GoolZoneNeighborObjectsTerminate(void) {
    ZoneHeader *header;
    s32 i;

    if (cur_zone != NULL) {
        header = (ZoneHeader *)cur_zone->items[0];
        for (i = 0; i < header->neighbor_count; i++) {
            if (func_8001440C(&header->neighbors[i], 0) != NULL) {
                GoolZoneObjectsTerminate(NSOpen(&header->neighbors[i], 1, 0));
            }
        }
    }
}

extern s16 D_8005D120[];
extern s16 D_8005D268[];
void func_8003A928(s16 *src, void *dst, s32 count);
#ifdef NON_MATCHING
/* Parameters for Naughty Dog's assembly, in the scratchpad. Equivalent; the
 * original addresses game.unk19C directly and derives &game.unk60 from it. */

void func_8001AB64(void) {
    u32 ticks = context.c1_p->ticks_per_frame;

    *(s32 *)0x1F800050 = game.unk19C;
    if (ticks >= 0x67) {
        ticks = 0x66;
    }
    *(u32 *)0x1F800054 = ticks;
    *(s32 **)0x1F800058 = game.unk60;
    func_8003A928(D_8005D120, (void *)0x1F800060, D_8005D268 - D_8005D120);
    *(s32 *)0x1F80005C = 0x1F800060;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001AB64);
#endif

/* Finds the object with pid_flags among the lists 3 and 4. */
GoolObject *func_8001ABF8(s32 pid_flags) {
    GoolObject *res;
    s32 i = 3;

    do {
        res = (GoolObject *)GoolObjectSearchTree((GoolObject *)&game.handles[i++], (GoolTraverseFunc)GoolObjectHasPidFlags, pid_flags);
    } while (res == NULL && i < 5);
    return res;
}

typedef struct {
    u32 value;
    s32 unk4;
    s32 id; /* 0x8 */
} Unk80067A38;
extern Unk80067A38 D_80067A38[];

void func_8001AC74(void) {
    s32 i;

    for (i = 0; i < D_80060A94; i++) {
        D_80067A38[i].id = 0x13B;
    }
}

/* Updates (or adds) the entries matching bits 8-23 of each value. */

void func_8001ACAC(s32 id, u32 *values, s32 count) {
    u32 *v = &values[count - 1];
    s32 j;

    for (count--; count >= 0; count--, v--) {
        for (j = 0; j < D_80060A94; j++) {
            if ((D_80067A38[j].value & 0xFFFF00) == (*v & 0xFFFF00)) {
                D_80067A38[j].value = *v;
                D_80067A38[j].id = id;
                break;
            }
        }
        if (j == D_80060A94) {
            D_80067A38[j].value = *v;
            D_80067A38[j].id = id;
            D_80067A38[j].unk4 = 0;
            D_80060A94 = j + 1;
        }
    }
}

s32 func_80032710(s32 a, s32 id, s32 pos, s32 b, s32 *out, s32 c, s32 d, s32 e, s32 f);

void func_8001AD68(s32 a, s32 pos, s32 dir) {
    s32 out;
    s32 handle;

    handle = func_80032710(a, dir == 2 ? 0x13B : 0x13C, dir == 2 ? pos - 0x100 : pos + 0x100, 0, &out, 1, 0, 0, 0);
    if (handle != 0 && out != 0) {
        func_8001ACAC(0x13B, (u32 *)handle, out);
    }
    handle = func_80032710(a, dir == 2 ? 0x13C : 0x13B, pos, 0, &out, 1, 0, 0, 0);
    if (handle != 0 && out != 0) {
        func_8001ACAC(0x13C, (u32 *)handle, out);
    }
}

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001AE64);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001B0BC);

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
INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001B524);
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
INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001B630);
#endif
