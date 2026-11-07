#ifndef PC_GPU_H
#define PC_GPU_H

/* The GPU of the PlayStation in software, for the PC port (src/pc/gpu.c):
 * the 1024x512 VRAM of 15-bit pixels and the drawing commands (GP0) as the
 * game sends them in its primitive lists. */

#include "types.h"

#define GPU_VRAM_W 1024
#define GPU_VRAM_H 512

extern u16 gpu_vram[GPU_VRAM_H][GPU_VRAM_W];

void gpu_reset(void);
/* runs the GP0 commands in words[0..count) (a packet of a primitive list) */
void gpu_commands(const u32 *words, u32 count);
/* copies to/from the VRAM (LoadImage/StoreImage/MoveImage) */
void gpu_load(s32 x, s32 y, s32 w, s32 h, const u16 *src);
void gpu_store(s32 x, s32 y, s32 w, s32 h, u16 *dst);
void gpu_move(s32 sx, s32 sy, s32 dx, s32 dy, s32 w, s32 h);
void gpu_fill(s32 x, s32 y, s32 w, s32 h, u32 rgb);

/* a recording of the GPU work between begin and end (tools/pc/gpu_replay.c) */
void gpu_trace_begin(const char *path);
void gpu_trace_end(void);
u32 gpu_state_words(void);
void *gpu_state(void);

/* what the screen shows: the display area of the VRAM */
typedef struct {
    s32 x, y, w, h;
    s32 rgb24; /* 24-bit pixels (the movies) */
    s32 enabled;
} GpuDisplay;
extern GpuDisplay gpu_display;
/* the display area as 0xRRGGBB pixels (w * h) */
void gpu_display_rgb(u32 *out);

#endif /* PC_GPU_H */
