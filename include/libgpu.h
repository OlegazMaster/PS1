#ifndef LIBGPU_H
#define LIBGPU_H

/* The parts of PSY-Q's libgpu used by the game (layouts as in LIBGPU.H). */

#include "types.h"

typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    RECT disp;
    RECT screen;
    u8 isinter, isrgb24, pad0, pad1;
} DISPENV; /* 0x14 */

typedef struct {
    u32 tag;
    u32 code[15];
} DR_ENV; /* 0x40 */

typedef struct {
    RECT clip;
    s16 ofs[2];
    RECT tw;
    u16 tpage;
    u8 dtd, dfe, isbg, r0, g0, b0;
    DR_ENV dr_env;
} DRAWENV; /* 0x5C */

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 w, h;
} BLK_FILL;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 w, h;
} TILE;

#define setlen(p, len) (((u8 *)(p))[3] = (len))
#define setcode(p, c) (((u8 *)(p))[7] = (c))
#define setBlockFill(p) (setlen(p, 3), setcode(p, 0x02))
#define setTile(p) (setlen(p, 3), setcode(p, 0x60))

s32 ResetGraph(s32 mode);
s32 SetGraphDebug(s32 level);
DRAWENV *SetDefDrawEnv(DRAWENV *env, s32 x, s32 y, s32 w, s32 h);
DISPENV *SetDefDispEnv(DISPENV *env, s32 x, s32 y, s32 w, s32 h);
DRAWENV *PutDrawEnv(DRAWENV *env);
DISPENV *PutDispEnv(DISPENV *env);
void SetDispMask(s32 mask);
s32 DrawSync(s32 mode);
void DrawPrim(void *prim);
void TermPrim(void *prim);
void AddPrim(void *ot, void *prim);
s32 LoadImage(RECT *rect, u32 *data);

#endif /* LIBGPU_H */
