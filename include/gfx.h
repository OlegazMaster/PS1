#ifndef GFX_H
#define GFX_H

#include "types.h"

/* One of the two draw buffers: ordering table and primitive memory. */
typedef struct DrawContext {
    s32 unk0;
    void *prims_head; /* 0x4 */
    void *prims_tail; /* 0x8 */
    s32 unkC;
    s32 draw_stamp; /* 0x10 */
    u8 unk14[0xC];
    u8 ot[0x2070]; /* 0x20: ordering table */
    s32 unk2090;
    void *unk2094;
    void *unk2098;
    struct ObjectLink *unk209C;
} DrawContext; /* 0x20A0 */

/* Double-buffered graphics context. */
typedef struct {
    s32 count; /* number of draw buffers */
    s32 unk4;
    DrawContext *c2_p; /* 0x8 */
    DrawContext *c1_p; /* 0xC */
    DrawContext buffers[2]; /* 0x10 */
} GfxContext;

extern GfxContext context;

#endif /* GFX_H */
