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
    u8 unk8[0x10];
    struct Entry *zone; /* 0x18 */
    u8 unk1C[0x28];
    struct GoolObject *parent; /* 0x44 */
    struct GoolObject *sibling; /* 0x48 */
    struct GoolObject *children; /* 0x4C */
    u8 unk50[0x5C];
    u32 unkAC;
    u8 unkB0[0x8];
    s32 pid_flags; /* 0xB8 */
    u32 *sp; /* 0xBC: top of the object's GOOL stack */
    u8 unkC0[0x28];
    s32 unkE8;
    u32 unkEC;
    u8 unkF0[0x10];
    u32 state_flags; /* 0x100 */
} GoolObject;

/* The head of an object list; shares type and children with GoolObject. */
typedef struct {
    s32 type; /* 2 */
    GoolObject *children;
} GoolHandle;

#define ERROR_INVALID_RETURN -25

#define ISHANDLE(obj) ((obj)->type == 2)
#define GoolObjectGetChildren(obj) (ISHANDLE(obj) ? (obj)->handle_children : (obj)->children)
#define GoolObjectSetChildren(obj, child) \
    (*(ISHANDLE(obj) ? &(obj)->handle_children : &(obj)->children) = (child))

typedef s32 (*GoolTraverseFunc)(GoolObject *obj, s32 arg);

s32 GoolObjectKill(GoolObject *obj, s32 sig);
s32 GoolZoneObjectTerminate(GoolObject *obj, struct Entry *zone);
s32 GoolSendEvent(GoolObject *sender, GoolObject *recipient, u32 event, s32 argc, u32 *argv);
GoolObject *GoolObjectHasPidFlags(GoolObject *obj, s32 pid_flags);
GoolObject *GoolObjectTestStateFlags(GoolObject *obj, u32 flags);
s32 GoolObjectSearchTree(GoolObject *obj, GoolTraverseFunc func, s32 arg);

#endif /* GOOL_H */
