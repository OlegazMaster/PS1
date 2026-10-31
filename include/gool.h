#ifndef GOOL_H
#define GOOL_H

#include "types.h"
#include "gfx.h"

/* GOOL objects: the game's actors, driven by bytecode from the level data. */

typedef struct ObjectLink {
    struct ObjectLink *next;
    struct GoolObject *obj;
} ObjectLink;

typedef struct {
    s16 r, g, b;
} Rgb16;

/* The light of an object: three sources (a, b, c), each an intensity and a
 * color; the strongest one set is copied to cur. */
typedef struct {
    s16 a; /* 0x0 */
    s16 b; /* 0x2 */
    s16 c; /* 0x4 */
    s16 cur; /* 0x6 */
    Rgb16 color_a; /* 0x8 */
    Rgb16 color_b; /* 0xE */
    Rgb16 color_c; /* 0x14 */
    Rgb16 cur_color; /* 0x1A */
} ObjColors;

/* An object, or a handle (type 2) that only heads a list of objects
 * (names follow c1's gool_object). */
typedef struct GoolObject {
    s32 type; /* 0: free, 1: object, 2: handle */
    s32 subtype; /* 0x4: for a handle, its first child (see GoolHandle) */
    void *unk8;
    struct Entry *global; /* 0xC: the GOOL executable of the object */
    struct Entry *unk10;
    u8 unk14[0x4];
    struct Entry *zone; /* 0x18 */
    u8 unk1C[0x4];
    ObjColors colors; /* 0x20 */
    struct GoolObject *self; /* 0x40 */
    struct GoolObject *parent; /* 0x44 */
    struct GoolObject *sibling; /* 0x48 */
    struct GoolObject *children; /* 0x4C */
    struct GoolObject *creator; /* 0x50 */
    struct GoolObject *player; /* 0x54 */
    struct GoolObject *collider; /* 0x58 */
    struct GoolObject *interrupter; /* 0x5C */
    Transform xform; /* 0x60: translation, rotation, scale */
    s32 misc_a[3]; /* 0x84 */
    s32 misc_b[3]; /* 0x90 */
    s32 misc_c[3]; /* 0x9C */
    u32 status_a; /* 0xA8 */
    u32 status_b; /* 0xAC */
    u32 status_c; /* 0xB0 */
    u32 id; /* 0xB4: exec << 16 | subtype */
    s32 pid_flags; /* 0xB8 */
    u32 *sp; /* 0xBC: top of the object's GOOL stack */
    u32 *pc; /* 0xC0: GOOL program counter */
    u32 *fp; /* 0xC4: GOOL frame pointer */
    u8 unkC8[0x8];
    s32 once_p; /* 0xD0 */
    s32 unkD4; /* 0xD4: result of the last broadcast */
    s32 ack; /* 0xD8 */
    u8 unkDC[0xC];
    s32 unkE8; /* anim_seq */
    u32 unkEC; /* anim_frame */
    s32 unkF0;
    s32 unkF4;
    s32 unkF8;
    u8 unkFC[0x4];
    u32 state_flags; /* 0x100 */
    s32 speed; /* 0x104 */
    s32 invincibility_state; /* 0x108 */
    u32 invincibility_stamp; /* 0x10C */
    s32 unk110;
    u8 unk114[0x4];
    s32 unk118;
    u8 unk11C[0x4];
    s32 unk120;
    u8 unk124[0x4];
    s32 hotspot_size; /* 0x128 */
    s32 voice_id; /* 0x12C */
    u8 unk130[0x8];
    s32 node; /* 0x138 */
} GoolObject;

/* The head of an object list; shares type and children with GoolObject. */
typedef struct {
    s32 type; /* 2 */
    GoolObject *children;
} GoolHandle;

#define ERROR_INVALID_RETURN -25

#define ISHANDLE(obj) ((obj)->type == 2)
#define GoolObjectGetChildren(obj) (ISHANDLE(obj) ? ((GoolHandle *)(obj))->children : (obj)->children)
#define GoolObjectSetChildren(obj, child) \
    (*(ISHANDLE(obj) ? &((GoolHandle *)(obj))->children : &(obj)->children) = (child))

typedef s32 (*GoolTraverseFunc)(GoolObject *obj, s32 arg);

s32 GoolObjectKill(GoolObject *obj, s32 sig);
s32 GoolZoneObjectTerminate(GoolObject *obj, struct Entry *zone);
s32 GoolSendEvent(GoolObject *sender, GoolObject *recipient, u32 event, s32 argc, u32 *argv);
GoolObject *GoolObjectHasPidFlags(GoolObject *obj, s32 pid_flags);
GoolObject *GoolObjectTestStateFlags(GoolObject *obj, u32 flags);
GoolObject *GoolObjectCreate(GoolObject *parent, s32 exec, s32 subtype, s32 argc, u32 *argv, s32 interrupt);
s32 GoolObjectSearchTree(GoolObject *obj, GoolTraverseFunc func, s32 arg);
s32 GoolObjectHandleTraverseTreePreorder(GoolObject *obj, GoolTraverseFunc func, s32 arg);
s32 GoolObjectHandleTraverseTreePostorder(GoolObject *obj, GoolTraverseFunc func, s32 arg);
void GoolForEachHandle(s32 (*func)(GoolObject *, GoolTraverseFunc, s32), GoolTraverseFunc arg1, s32 arg2);

#endif /* GOOL_H */
