#include "common.h"
#include "gool.h"
#include "game.h"
#include "ns.h"

/* GOOL objects: allocation, the object tree, messages (names follow c1). */

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

INCLUDE_ASM("asm/us/nonmatchings/gool", func_80019ACC);

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

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001AAC8);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001AB64);

/* Finds the object with pid_flags among the lists 3 and 4. */
GoolObject *func_8001ABF8(s32 pid_flags) {
    GoolObject *res;
    s32 i = 3;

    do {
        res = (GoolObject *)GoolObjectSearchTree((GoolObject *)&game.handles[i++], (GoolTraverseFunc)GoolObjectHasPidFlags, pid_flags);
    } while (res == NULL && i < 5);
    return res;
}

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001AC74);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001ACAC);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001AD68);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001AE64);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001B0BC);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001B328);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001B524);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001B630);
