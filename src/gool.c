#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

/* GOOL objects: allocation, the object tree, messages (names follow c1). */

/* .sbss */
GoolObject *crash; /* the player object */
s32 D_80060A94; /* entries used in D_80067A38 */

s32 func_80032710(s32 a, s32 id, s32 pos, s32 b, s32 *out, s32 c, s32 d, s32 e, s32 f);
s32 func_80032DF8(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g);
s32 GoolObjectInit(GoolObject *obj, s32 exec, s32 subtype, s32 argc, u32 *argv, s32 keep);
void *memset(void *dst, s32 c, s32 n);
extern s32 paused;
extern u32 spawns[];
GoolObject *GoolObjectSpawn(Entry *zone, s32 idx);
void GoolObjectTerminate(GoolObject *obj, s32 unused);

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
    return 2; /* (what the original leaves in v0) */
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

typedef struct {
    u8 unk0[0x184];
    s32 unk184; /* 0x184: items before the entities */
    s32 unk188;
    u8 unk18C[0x4];
    s32 neighbor_count; /* 0x190 */
    u32 neighbors[1]; /* 0x194: references to the neighbor zones */
} ZoneHeader;

s32 func_80032EDC(s32 entity, s32 tag, s32 a, s32 b);
s32 func_80032250(s32 a, s32 b); /* angle/difference */
void func_80020670(GoolObject *obj, s32 a, Transform *xform);
void func_8002FF3C(GoolObject *obj, void *state, s32 a, s32 b);
extern s32 next_lid;
extern u8 savestate[];
typedef struct {
    Vec3 pos;
    s32 unkC[3];
} Unk80061044Pos;
typedef struct {
    u8 unk0[0x2E8];
    Unk80061044Pos unk2E8[1]; /* 0x2E8 */
} Unk80061044;
/* ns.ldat (D_80061044). */
#define D_80061044 ((Unk80061044 *)ns.ldat)
/* A property of an entity: 0x9F id, 0xA9 exec, 0xAA subtype, 0x342 flags. */
#define EntityProp(e, tag) func_80032DF8(e, tag, 0, 0, 0, 2, 0)
/* The spawn flags of an entity (1: spawned, 2: removed); the original reads
 * the id twice. */
#define EntitySpawn(e) (id = (EntityProp(e, 0x9F), EntityProp(e, 0x9F)), spawns[id])

#ifdef NON_MATCHING
/* Spawns the object of entity idx of zone (c1: GoolObjectSpawn); crates
 * (exec 0x22) right above the previous one are stacked on it. Equivalent;
 * the original keeps the list index in s0. */
GoolObject *GoolObjectSpawn(Entry *zone, s32 idx) {
    ZoneHeader *header;
    s32 entity;
    GoolObject *obj;
    GoolObject *found;
    GoolObject *prev;
    s16 *pos;
    s16 *prev_pos;
    s32 is_player;
    s32 list;
    s32 i;
    s32 argc;
    u32 *argv;
    s32 res;
    s32 id;

    if (zone == NULL) {
        goto fail;
    }
    prev = NULL;
    is_player = 0;
    header = (ZoneHeader *)zone->items[0];
    entity = (s32)zone->items[idx + header->unk184 + header->unk188];
    if (EntityProp(entity, 0xA9) == 0 || (EntityProp(entity, 0x9F) > 0 && EntityProp(entity, 0x9F) < 5)) {
        is_player = 1;
    }
    if (is_player && crash != NULL) {
        return (GoolObject *)-0xE;
    }
    if (EntityProp(entity, 0xA9) == 0x22) {
        if (game.unk380 != 0 && (pos = (s16 *)func_80032710(entity, 0x4B, 0, 0, NULL, 2, 0, 0, 0), prev_pos = (s16 *)func_80032710(game.unk380, 0x4B, 0, 0, NULL, 2, 0, 0, 0), game.unk380 != 0) && func_80032250(pos[0], prev_pos[0]) < 0x28 && func_80032250(pos[2], prev_pos[2]) < 0x28 && func_80032250(pos[1], prev_pos[1] + 0x190) < 0x28) {
            prev = game.unk378;
        } else {
            game.unk37C = 0x19000;
            prev = NULL;
            game.unk378 = NULL;
        }
        game.unk380 = entity;
    }
    if ((EntitySpawn(entity) & 1) || (EntitySpawn(entity) & 2)) {
        if (EntityProp(entity, 0xA9) == 0x22) {
            game.unk37C += 0x19000;
        }
        goto fail;
    }
    if (EntityProp(entity, 0x342) & 1) {
        spawns[EntityProp(entity, 0x9F)] |= (EntityProp(entity, 0x9F), 2);
        return (GoolObject *)-0xE;
    }
    if (is_player) {
        obj = game.player;
        crash = obj;
        list = 6;
    } else {
    retry:
        obj = GoolObjectGetChildren((GoolObject *)&game.free_objects);
        list = 3;
        if (obj == NULL) {
            for (i = 7; i >= 0; i--) {
                found = GoolObjectSearchTree((GoolObject *)&game.handles[i], (GoolTraverseFunc)GoolObjectTestStateFlags, paused ? 0x420000 : 0x20000);
                if (found != NULL) {
                    break;
                }
            }
            if (found == NULL) {
                return (GoolObject *)-0x16;
            }
            GoolObjectTerminate(found, 0);
            goto retry;
        }
    }
    GoolObjectAddChild(obj, (GoolObject *)&game.handles[list]);
    obj->subtype = 3;
    obj->pid_flags = EntityProp(entity, 0x9F) << 8;
    memset(&obj->colors, 0, sizeof(ObjColors));
    obj->unkF0 = entity;
    obj->unkF8 = func_80032EDC(entity, 0x4B, 0, 2) << 8;
    argv = (u32 *)func_80032710(entity, 0xA4, 0, 0, &argc, 2, 0, 0, 0);
    if (argv == NULL) {
        argc = 0;
    }
    res = GoolObjectInit(obj, EntityProp(entity, 0xA9), EntityProp(entity, 0xAA), argc, argv, 1);
    if (res > -0xFF) {
        GoolObjectAddChild(obj, (GoolObject *)&game.free_objects);
        return (GoolObject *)res;
    }
    spawns[EntityProp(entity, 0x9F)] |= (EntityProp(entity, 0x9F), 1);
    obj->zone = zone;
    func_80020670(obj, 0, &obj->xform);
    if (obj == crash) {
        *(Vec3 *)obj->xform.trans = D_80061044->unk2E8[game.unk26C].pos;
        if (next_lid == -1) {
            game.unk174 = next_lid;
            func_8002FF3C(obj, savestate, 1, 0);
        }
    }
    if (((s32 *)obj->global->items[0])[1] == 0x300) {
        GoolObjectAddChild(obj, (GoolObject *)&game.handles[4]);
    }
    if (obj->id >> 16 == 0x22) {
        if (argv == NULL || !(argv[1] & 0x80000)) {
            if (prev != NULL) {
                obj->misc_a[0] = (s32)prev;
                obj->misc_a[1] = 0;
                prev->misc_a[1] = (s32)obj;
            } else {
                obj->misc_a[0] = 0;
                obj->misc_a[1] = 0;
            }
            obj->xform.trans[1] += 0x19000 - game.unk37C;
        }
        game.unk378 = obj;
    }
    return obj;
fail:
    return (GoolObject *)-0xE;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/gool", GoolObjectSpawn);
#endif


/* Creates an object under parent (c1: GoolObjectCreate); with exec and
 * subtype 0 it is the player. When no object is free and interrupt is set,
 * it terminates an object that allows it and tries again. */
GoolObject *GoolObjectCreate(GoolObject *parent, s32 exec, s32 subtype, s32 argc, u32 *argv, s32 interrupt) {
    GoolObject *obj;
    GoolObject *found;
    s32 res;
    s32 i;

    if (exec == 0 && subtype == 0) {
        obj = game.player;
        crash = obj;
    } else {
    retry:
        obj = GoolObjectGetChildren((GoolObject *)&game.free_objects);
        if (obj == NULL && interrupt) {
            for (i = 7; i >= 0; i--) {
                found = GoolObjectSearchTree((GoolObject *)&game.handles[i], (GoolTraverseFunc)GoolObjectTestStateFlags, paused ? 0x420000 : 0x20000);
                if (found != NULL) {
                    break;
                }
            }
            if (found != NULL) {
                GoolObjectTerminate(found, 0);
                goto retry;
            }
        }
    }
    if (obj == NULL) {
        return (GoolObject *)-0x16;
    }
    GoolObjectAddChild(obj, parent);
    obj->subtype = 3;
    res = GoolObjectInit(obj, exec, subtype, argc, argv, 0);
    memset(&obj->colors, 0, sizeof(ObjColors));
    if (res > -0xFF) {
        return (GoolObject *)res;
    }
    return obj;
}

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

void GoolObjectTerminate(GoolObject *obj, s32 unused) {
    GoolObjectKill(obj, 1);
}


void GoolZoneObjectsTerminate(s32 arg) {
    GoolForEachHandle(GoolObjectHandleTraverseTreePostorder, (GoolTraverseFunc)GoolZoneObjectTerminate, arg);
}

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

    *(s32 *)(PSX_SCRATCH + 0x0050) = game.unk19C;
    if (ticks >= 0x67) {
        ticks = 0x66;
    }
    *(u32 *)(PSX_SCRATCH + 0x0054) = ticks;
    *(s32 **)(PSX_SCRATCH + 0x0058) = game.unk60;
    func_8003A928(D_8005D120, (void *)(PSX_SCRATCH + 0x0060), D_8005D268 - D_8005D120);
    *(s32 *)(PSX_SCRATCH + 0x005C) = (PSX_SCRATCH + 0x0060);
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


/* Runs the queued entries (all, or those with id): 0x13C spawns the object
 * of an entity (value: bits 0-7 neighbor zone, 8-23 pid, 24-31 entity) unless
 * it was already spawned; 0x13B kills it and removes the entry. */
void func_8001AE64(Entry *zone, s32 id) {
    Unk80067A38 *e;
    GoolObject *obj;
    u32 value;
    u32 pid;
    s32 i;
    s32 j;

    game.unk378 = 0;
    game.unk380 = 0;
    game.unk37C = 0x19000;
    for (i = D_80060A94 - 1; i >= 0; i--) {
        e = &D_80067A38[i];
        if (id != -1 && id != e->id) {
            continue;
        }
        switch (e->id) {
        case 0x13C:
            value = e->value;
            pid = value & 0xFFFF00;
            if (!(spawns[pid >> 8] & 1) && (value & 0xFF) < 0x10) {
                e->unk4 = (s32)GoolObjectSpawn((Entry *)NSOpen(&((ZoneHeader *)zone->items[0])->neighbors[value & 0xFF], 1, 0), *((u8 *)e + 3));
                if (e->unk4 > -0xFF) {
                    goto remove;
                }
            }
            if (e->unk4 == 0) {
                e->unk4 = (s32)func_8001ABF8(pid);
            }
            e->id = 0;
            break;
        case 0x13B:
            pid = e->value & 0xFFFF00;
            if (spawns[pid >> 8] & 1) {
                if (e->unk4 == 0 && (e->unk4 = (s32)func_8001ABF8(pid)) == 0) {
                    goto remove;
                }
                obj = (GoolObject *)e->unk4;
                if (!(obj->status_b & 0x1000000) && !(obj->state_flags & 0x10000)) {
                    GoolObjectKill(obj, 1);
                }
            }
        remove:
            for (j = i + 1; j < D_80060A94; j++) {
                D_80067A38[j - 1] = D_80067A38[j];
            }
            D_80060A94--;
            break;
        }
    }
}
