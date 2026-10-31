#ifndef GOOL_H
#define GOOL_H

#include "types.h"

/* GOOL objects: the game's actors, driven by bytecode from the level data. */

typedef struct ObjectLink {
    struct ObjectLink *next;
    struct GoolObject *obj;
} ObjectLink;

/* An object, or a handle (type 2) that only heads a list of objects. */
typedef struct GoolObject {
    s32 type; /* 0: free, 2: handle */
    struct GoolObject *handle_children; /* 0x4: children of a handle */
    u8 unk8[0x3C];
    struct GoolObject *parent; /* 0x44 */
    struct GoolObject *sibling; /* 0x48 */
    struct GoolObject *children; /* 0x4C */
    u8 unk50[0x5C];
    u32 unkAC;
    u8 unkB0[0xC];
    u32 *sp; /* 0xBC: top of the object's GOOL stack */
    u8 unkC0[0x28];
    s32 unkE8;
    u32 unkEC;
} GoolObject;

#define ERROR_INVALID_RETURN -25

#define ISHANDLE(obj) ((obj)->type == 2)
#define GoolObjectGetChildren(obj) (ISHANDLE(obj) ? (obj)->handle_children : (obj)->children)

typedef s32 (*GoolTraverseFunc)(GoolObject *obj, s32 arg);

s32 GoolObjectKill(GoolObject *obj, s32 sig);

#endif /* GOOL_H */
