#include "common.h"
#include "gool.h"

/* GOOL objects: allocation, the object tree, messages (names follow c1). */

INCLUDE_ASM("asm/us/nonmatchings/gool", func_80019624);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001962C);

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

INCLUDE_ASM("asm/us/nonmatchings/gool", func_80019A38);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_80019AAC);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_80019ACC);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_80019CC0);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_80019CE4);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001A42C);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001A5C8);

INCLUDE_ASM("asm/us/nonmatchings/gool", GoolObjectKill);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001A9A8);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001AA78);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001AA98);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001AAC8);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001AB64);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001ABF8);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001AC74);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001ACAC);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001AD68);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001AE64);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001B0BC);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001B328);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001B524);

INCLUDE_ASM("asm/us/nonmatchings/gool", func_8001B630);
