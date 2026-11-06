/* libgpu of the PC port: the PSY-Q graphics functions the game uses, on the
 * software GPU (src/pc/gpu.c). Primitive lists are linked as on the console:
 * a tag word holds the length of the packet (top byte) and the low 24 bits
 * of the address of the next one, in the PlayStation RAM. */
#include "libgpu.h"
#include "pc_gpu.h"

#include <string.h>

void pc_video_present(void);

#define RAM(a) ((u32 *)(0x80000000u | ((a) & 0xFFFFFF)))

static DRAWENV cur_draw;
static DISPENV cur_disp;

s32 ResetGraph(s32 mode)
{
    if (mode == 0 || mode == 3) {
        gpu_reset();
    }
    return 0;
}

s32 SetGraphDebug(s32 level)
{
    return 0;
}

u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y)
{
    return (u16)(((tp & 3) << 7) | ((abr & 3) << 5) | ((y & 0x100) >> 4) | ((x & 0x3FF) >> 6) | ((y & 0x200) << 2));
}

DRAWENV *SetDefDrawEnv(DRAWENV *env, s32 x, s32 y, s32 w, s32 h)
{
    env->clip.x = (s16)x;
    env->clip.y = (s16)y;
    env->clip.w = (s16)w;
    env->clip.h = (s16)h;
    env->ofs[0] = (s16)x;
    env->ofs[1] = (s16)y;
    env->tw.x = env->tw.y = env->tw.w = env->tw.h = 0;
    env->tpage = GetTPage(0, 0, 640, 0);
    env->dtd = 1;
    env->dfe = h < 289 ? 0 : 1;
    env->isbg = 0;
    env->r0 = env->g0 = env->b0 = 0;
    return env;
}

DISPENV *SetDefDispEnv(DISPENV *env, s32 x, s32 y, s32 w, s32 h)
{
    env->disp.x = (s16)x;
    env->disp.y = (s16)y;
    env->disp.w = (s16)w;
    env->disp.h = (s16)h;
    env->screen.x = env->screen.y = env->screen.w = env->screen.h = 0;
    env->isinter = env->isrgb24 = 0;
    env->pad0 = env->pad1 = 0;
    return env;
}

/* the texture window of a DRAWENV as an E2 command */
static u32 tw_command(const RECT *tw)
{
    u32 mx = (u32)((-tw->w) & 0xFF) >> 3, my = (u32)((-tw->h) & 0xFF) >> 3;
    u32 ox = (u32)(tw->x & 0xFF) >> 3, oy = (u32)(tw->y & 0xFF) >> 3;

    return 0xE2000000 | mx | my << 5 | ox << 10 | oy << 15;
}

/* the commands of a drawing environment (and the clear of its area) */
static u32 env_commands(DRAWENV *env, u32 *c)
{
    s32 x2 = env->clip.x + env->clip.w - 1, y2 = env->clip.y + env->clip.h - 1;
    u32 n = 0;

    c[n++] = 0xE1000000 | (env->tpage & 0x9FF) | (env->dtd ? 0x200 : 0) | (env->dfe ? 0x400 : 0);
    c[n++] = tw_command(&env->tw);
    c[n++] = 0xE3000000 | (env->clip.x & 1023) | (env->clip.y & 1023) << 10;
    c[n++] = 0xE4000000 | (x2 & 1023) | (y2 & 1023) << 10;
    c[n++] = 0xE5000000 | (env->ofs[0] & 0x7FF) | (env->ofs[1] & 0x7FF) << 11;
    if (env->isbg) {
        c[n++] = 0x02000000 | env->r0 | env->g0 << 8 | env->b0 << 16;
        c[n++] = (env->clip.x & 0xFFFF) | (u32)env->clip.y << 16;
        c[n++] = (env->clip.w & 0xFFFF) | (u32)env->clip.h << 16;
    }
    return n;
}

void SetDrawEnv(DR_ENV *dr, DRAWENV *env)
{
    u32 n = env_commands(env, dr->code);

    dr->tag = (dr->tag & 0xFFFFFF) | n << 24;
}

DRAWENV *PutDrawEnv(DRAWENV *env)
{
    u32 c[16];
    u32 n = env_commands(env, c);

    gpu_commands(c, n);
    if (env != &cur_draw) {
        memcpy(&cur_draw, env, sizeof(cur_draw));
    }
    return env;
}

DISPENV *PutDispEnv(DISPENV *env)
{
    if (env != &cur_disp) {
        memcpy(&cur_disp, env, sizeof(cur_disp));
    }
    gpu_display.x = env->disp.x;
    gpu_display.y = env->disp.y;
    gpu_display.w = env->disp.w;
    gpu_display.h = env->disp.h;
    gpu_display.rgb24 = env->isrgb24;
    return env;
}

void SetDispMask(s32 mask)
{
    gpu_display.enabled = mask != 0;
}

/* everything is drawn at once */
s32 DrawSync(s32 mode)
{
    return 0;
}

void DrawPrim(void *prim)
{
    u32 *p = prim;

    gpu_commands(p + 1, p[0] >> 24);
}

/* DrawOTag: draws a primitive list, up to the tag 0xFFFFFF */
void func_8004CBF4(void *ot)
{
    u32 *p = ot;
    u32 guard = 0;

    while (guard++ < 0x100000) {
        u32 tag = *p;

        if (tag >> 24) {
            gpu_commands(p + 1, tag >> 24);
        }
        if ((tag & 0xFFFFFF) == 0xFFFFFF) {
            break;
        }
        p = RAM(tag);
    }
}

void TermPrim(void *prim)
{
    *(u32 *)prim = 0x00FFFFFF;
}

void AddPrim(void *ot, void *prim)
{
    u32 *o = ot, *p = prim;

    *p = (*p & 0xFF000000) | (*o & 0xFFFFFF);
    *o = (*o & 0xFF000000) | ((u32)p & 0xFFFFFF);
}

s32 LoadImage(RECT *r, u32 *data)
{
    gpu_load(r->x, r->y, r->w, r->h, (const u16 *)data);
    return 0;
}

s32 StoreImage(RECT *r, u32 *data)
{
    gpu_store(r->x, r->y, r->w, r->h, (u16 *)data);
    return 0;
}

s32 MoveImage(RECT *r, s32 x, s32 y)
{
    gpu_move(r->x, r->y, x, y, r->w, r->h);
    return 0;
}

void SetPolyFT4(POLY_FT4 *p)
{
    setlen(p, 9);
    setcode(p, 0x2C);
}

void SetPolyF4(POLY_F4 *p)
{
    setlen(p, 5);
    setcode(p, 0x28);
}

void SetSemiTrans(void *p, s32 abe)
{
    if (abe) {
        ((u8 *)p)[7] |= 2;
    } else {
        ((u8 *)p)[7] &= ~2;
    }
}

void SetDrawMove(DR_MOVE *p, RECT *r, s32 x, s32 y)
{
    setlen(p, 5);
    p->code[0] = 0x01000000;
    p->code[1] = 0x80000000;
    p->code[2] = (r->x & 0xFFFF) | (u32)r->y << 16;
    p->code[3] = (x & 0xFFFF) | (u32)y << 16;
    p->code[4] = (r->w & 0xFFFF) | (u32)r->h << 16;
    if (r->w == 0 || r->h == 0) {
        setlen(p, 0);
    }
}

void SetDrawMode(DR_MODE *p, s32 dfe, s32 dtd, s32 tpage, RECT *tw)
{
    setlen(p, 2);
    p->code[0] = 0xE1000000 | (tpage & 0x9FF) | (dtd ? 0x200 : 0) | (dfe ? 0x400 : 0);
    p->code[1] = tw ? tw_command(tw) : 0;
}
