#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

/* GOOL objects: allocation, the object tree, messages (names follow c1). */

/* .sbss */
void *D_80060A90;
s32 D_80060A94; /* entries used in D_80067A38 */

s32 func_80019624(void) {
    return -0xFF;
}


#ifdef NON_MATCHING
/* Moves child to the front of the children of parent (c1: GoolObjectAddChild).
 * Equivalent; the original tests cur for NULL twice in the search loop. */

s32 func_8001962C(GoolObject *child, GoolObject *parent) {
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
INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001962C);
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
    dist = func_80031A54(obj->trans, query->obj->trans);
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

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001A5C8);

INCLUDE_ASM("asm/us/nonmatchings/gool", GoolObjectKill);

extern Entry *D_80065D60;

#ifdef NON_MATCHING
/* Kills an object of zone when the zone goes away (c1). Equivalent; the
 * original compares obj->zone first and loads the -1 later. */
s32 GoolZoneObjectTerminate(GoolObject *obj, Entry *zone) {
    if (obj->pid_flags != 0 || obj->type == 0 || zone != obj->zone || (obj->unkAC & 0x1000000)
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
void *func_8001440C(u32 *ref, s32 count);
u32 NSOpen(u32 *en_ref, s32 flag, s32 count);
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

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001B328);

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

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001B630);
