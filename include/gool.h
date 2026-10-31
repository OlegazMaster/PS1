#ifndef GOOL_H
#define GOOL_H

#include "types.h"

/* GOOL objects: the game's actors, driven by bytecode from the level data. */

typedef struct ObjectLink {
    struct ObjectLink *next;
    struct GoolObject *obj;
} ObjectLink;

typedef struct GoolObject {
    u8 unk0[0xAC];
    u32 unkAC;
    u8 unkB0[0xC];
    u32 *sp; /* 0xBC: top of the object's GOOL stack */
    u8 unkC0[0x28];
    s32 unkE8;
    u32 unkEC;
} GoolObject;

#endif /* GOOL_H */
