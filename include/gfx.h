#ifndef GFX_H
#define GFX_H

#include "types.h"
#include "libgpu.h"

/* One of the two draw buffers ("gfx_context" in c1). */
typedef struct DrawContext {
    s32 idx;
    void *prims_head; /* 0x4: primitive memory */
    void *prims_tail; /* 0x8 */
    s32 unkC;
    s32 draw_stamp; /* 0x10 */
    s32 unk14;
    s32 ticks_per_frame; /* 0x18 */
    s32 unk1C;
    void *ot[0x800]; /* 0x20: ordering table */
    DISPENV disp; /* 0x2020 */
    DRAWENV draw; /* 0x2034 */
    s32 unk2090;
    void *unk2094;
    void *unk2098;
    struct ObjectLink *unk209C;
} DrawContext; /* 0x20A0 */

/* Double-buffered graphics context: c1_p is displayed while c2_p is drawn. */
typedef struct {
    s32 count; /* number of draw buffers */
    DrawContext *c1_p; /* 0x4 */
    DrawContext *c2_p; /* 0x8 */
    DrawContext *cur; /* 0xC */
    DrawContext buffers[2]; /* 0x10 */
} GfxContext;

extern GfxContext context;

#endif /* GFX_H */
