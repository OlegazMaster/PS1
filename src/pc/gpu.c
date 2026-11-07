/* The GPU of the PlayStation in software (the PC port): the VRAM and the
 * drawing commands (GP0), as the game sends them in its primitive lists.
 *
 * Written from the description of the hardware (psx-spx): polygons (flat or
 * Gouraud shaded, textured from a 4/8/15-bit texture page through a CLUT,
 * texture window, raw or modulated texture), lines and polylines,
 * rectangles and sprites, the four semi-transparency modes, dithering, the
 * mask bit, the drawing area and offset, filling and copying the VRAM.
 * Triangles are drawn with the rules of the hardware: the left and top edges
 * belong to a triangle, the right and bottom ones do not; texture
 * coordinates are interpolated affinely; a quad is the triangles 0-1-2 and
 * 1-2-3; a polygon larger than 1023x511 is not drawn. */
#include "pc_gpu.h"

#include <stdio.h>
#include <string.h>

u16 gpu_vram[GPU_VRAM_H][GPU_VRAM_W];

/* A recording of the GPU work (gpu_trace_begin/end): records of u32 words,
 * [type, count, count words]. 0: the VRAM (u16 pixels), 1: GP0 words,
 * 2: a load (x, y, w, h, the pixels), 3: a move (sx, sy, dx, dy, w, h),
 * 4: the state of the GPU (the struct below, raw), 5: the display (x, y, w,
 * h, rgb24, enabled). tools/pc/gpu_replay.c plays it back. */
static FILE *trace;
static s32 tracing_commands;

static void trace_rec(u32 type, const void *a, u32 na, const void *b, u32 nb)
{
    u32 head[2];

    head[0] = type;
    head[1] = na + nb;
    fwrite(head, 4, 2, trace);
    if (na) {
        fwrite(a, 4, na, trace);
    }
    if (nb) {
        fwrite(b, 4, nb, trace);
    }
}
GpuDisplay gpu_display = { 0, 0, 320, 240, 0, 0 };

static struct {
    s32 tx, ty;       /* the texture page: x (multiple of 64), y (0 or 256) */
    s32 abr;          /* semi-transparency mode */
    s32 tp;           /* texture depth: 0 4-bit, 1 8-bit, 2 15-bit */
    s32 dither;
    s32 tex_disable;
    s32 twmx, twmy, twox, twoy; /* the texture window (in 8-pixel steps) */
    s32 x1, y1, x2, y2;         /* the drawing area (inclusive) */
    s32 ox, oy;                 /* the drawing offset */
    s32 mask_set, mask_check;
} st;

static const s8 dither_matrix[4][4] = {
    { -4, 0, -3, 1 },
    { 2, -2, 3, -1 },
    { -3, 1, -4, 0 },
    { 3, -1, 2, -2 },
};

void gpu_trace_begin(const char *path)
{
    trace = fopen(path, "wb");
    if (trace) {
        trace_rec(4, &st, sizeof(st) / 4, NULL, 0);
        trace_rec(0, gpu_vram, sizeof(gpu_vram) / 4, NULL, 0);
        fflush(trace);
    }
}

void gpu_trace_end(void)
{
    if (trace) {
        u32 d[6];

        d[0] = gpu_display.x;
        d[1] = gpu_display.y;
        d[2] = gpu_display.w;
        d[3] = gpu_display.h;
        d[4] = gpu_display.rgb24;
        d[5] = gpu_display.enabled;
        trace_rec(5, d, 6, NULL, 0);
        fclose(trace);
        trace = NULL;
    }
}

/* for the replay: the state as raw words */
u32 gpu_state_words(void)
{
    return sizeof(st) / 4;
}

void *gpu_state(void)
{
    return &st;
}

void gpu_reset(void)
{
    memset(&st, 0, sizeof(st));
    st.x2 = GPU_VRAM_W - 1;
    st.y2 = GPU_VRAM_H - 1;
}

static s32 sx11(u32 v)
{
    return (s32)(v << 21) >> 21;
}

/* ---- pixels ---- */

typedef struct {
    s32 textured, raw, semi, abr, dither;
    s32 cx, cy; /* the CLUT */
    s32 tx, ty, tp;
} Mode;

static u16 texel(const Mode *m, s32 u, s32 v)
{
    u &= 0xFF;
    v &= 0xFF;
    u = (u & ~(st.twmx * 8)) | ((st.twox & st.twmx) * 8);
    v = (v & ~(st.twmy * 8)) | ((st.twoy & st.twmy) * 8);
    switch (m->tp) {
    case 0: {
        u16 w = gpu_vram[(m->ty + v) & 511][(m->tx + u / 4) & 1023];
        u32 i = (w >> ((u & 3) * 4)) & 15;

        return gpu_vram[m->cy][(m->cx + i) & 1023];
    }
    case 1: {
        u16 w = gpu_vram[(m->ty + v) & 511][(m->tx + u / 2) & 1023];
        u32 i = (w >> ((u & 1) * 8)) & 255;

        return gpu_vram[m->cy][(m->cx + i) & 1023];
    }
    default:
        return gpu_vram[(m->ty + v) & 511][(m->tx + u) & 1023];
    }
}

static s32 clamp(s32 x, s32 lo, s32 hi)
{
    return x < lo ? lo : x > hi ? hi : x;
}

/* draws pixel (x, y) (drawing area checked by the caller): colour r, g, b
 * (8 bits each: the vertex colour, or the modulation of a texture), the
 * texel at u, v when textured */
static void pixel(const Mode *m, s32 x, s32 y, s32 r, s32 g, s32 b, s32 u, s32 v)
{
    u16 *dst = &gpu_vram[y][x];
    u16 t = 0;
    s32 semi = m->semi;
    s32 r5, g5, b5;
    u16 out;

    if (st.mask_check && (*dst & 0x8000)) {
        return;
    }
    if (m->textured) {
        t = texel(m, u, v);
        if (t == 0) {
            return; /* transparent */
        }
        semi = semi && (t & 0x8000);
        if (m->raw) {
            r = (t & 31) << 3;
            g = ((t >> 5) & 31) << 3;
            b = ((t >> 10) & 31) << 3;
        } else {
            r = ((t & 31) << 3) * r >> 7;
            g = (((t >> 5) & 31) << 3) * g >> 7;
            b = (((t >> 10) & 31) << 3) * b >> 7;
        }
    }
    if (m->dither) {
        s32 d = dither_matrix[y & 3][x & 3];

        r += d;
        g += d;
        b += d;
    }
    r5 = clamp(r, 0, 255) >> 3;
    g5 = clamp(g, 0, 255) >> 3;
    b5 = clamp(b, 0, 255) >> 3;
    if (semi) {
        s32 br = *dst & 31, bg = (*dst >> 5) & 31, bb = (*dst >> 10) & 31;

        switch (m->abr) {
        case 0:
            r5 = (br + r5) >> 1;
            g5 = (bg + g5) >> 1;
            b5 = (bb + b5) >> 1;
            break;
        case 1:
            r5 = clamp(br + r5, 0, 31);
            g5 = clamp(bg + g5, 0, 31);
            b5 = clamp(bb + b5, 0, 31);
            break;
        case 2:
            r5 = clamp(br - r5, 0, 31);
            g5 = clamp(bg - g5, 0, 31);
            b5 = clamp(bb - b5, 0, 31);
            break;
        default:
            r5 = clamp(br + (r5 >> 2), 0, 31);
            g5 = clamp(bg + (g5 >> 2), 0, 31);
            b5 = clamp(bb + (b5 >> 2), 0, 31);
            break;
        }
    }
    out = (u16)(r5 | g5 << 5 | b5 << 10);
    if (st.mask_set || (m->textured && (t & 0x8000))) {
        out |= 0x8000;
    }
    *dst = out;
}

/* ---- triangles ---- */

typedef struct {
    s32 x, y;
    s32 r, g, b;
    s32 u, v;
} Vertex;

static s64 edge(const Vertex *a, const Vertex *b, s32 x, s32 y)
{
    return (s64)(b->x - a->x) * (y - a->y) - (s64)(b->y - a->y) * (x - a->x);
}

/* whether the edge a->b of a triangle (counter-clockwise in screen space,
 * y down) is a top or left edge: its pixels are drawn */
static s32 top_left(const Vertex *a, const Vertex *b)
{
    return (a->y == b->y && b->x < a->x) || b->y > a->y;
}

static void triangle(const Mode *m, Vertex v0, Vertex v1, Vertex v2, s32 gouraud)
{
    const Vertex *p[3];
    s64 area;
    s32 minx, maxx, miny, maxy, x, y;
    s32 bias0, bias1, bias2;

    /* too large: not drawn */
    if (v0.x - v1.x >= 1024 || v1.x - v0.x >= 1024 || v1.x - v2.x >= 1024 || v2.x - v1.x >= 1024 ||
        v0.x - v2.x >= 1024 || v2.x - v0.x >= 1024 || v0.y - v1.y >= 512 || v1.y - v0.y >= 512 ||
        v1.y - v2.y >= 512 || v2.y - v1.y >= 512 || v0.y - v2.y >= 512 || v2.y - v0.y >= 512) {
        return;
    }
    area = edge(&v0, &v1, v2.x, v2.y);
    if (area == 0) {
        return;
    }
    p[0] = &v0;
    if (area < 0) {
        p[1] = &v1;
        p[2] = &v2;
        area = -area;
    } else {
        p[1] = &v2;
        p[2] = &v1;
    }
    /* the bounding box inside the drawing area (inclusive) */
    minx = v0.x < v1.x ? (v0.x < v2.x ? v0.x : v2.x) : (v1.x < v2.x ? v1.x : v2.x);
    maxx = v0.x > v1.x ? (v0.x > v2.x ? v0.x : v2.x) : (v1.x > v2.x ? v1.x : v2.x);
    miny = v0.y < v1.y ? (v0.y < v2.y ? v0.y : v2.y) : (v1.y < v2.y ? v1.y : v2.y);
    maxy = v0.y > v1.y ? (v0.y > v2.y ? v0.y : v2.y) : (v1.y > v2.y ? v1.y : v2.y);
    minx = minx < st.x1 ? st.x1 : minx;
    miny = miny < st.y1 ? st.y1 : miny;
    maxx = maxx > st.x2 ? st.x2 : maxx;
    maxy = maxy > st.y2 ? st.y2 : maxy;
    /* the right and bottom edges are not drawn */
    bias0 = top_left(p[1], p[2]) ? 0 : -1;
    bias1 = top_left(p[2], p[0]) ? 0 : -1;
    bias2 = top_left(p[0], p[1]) ? 0 : -1;
    if (minx > maxx || miny > maxy) {
        return;
    }
    {
        /* The edge functions w0..w2 (positive inside: counter-clockwise in
         * y-down coordinates as ordered above) are linear in x and y, and so
         * is the numerator of each attribute, n = w0 a0 + w1 a1 + w2 a2
         * (attribute = n / area, as the barycentric weights): the edges step
         * by 32-bit additions, the attributes in fixed point with 32
         * fraction bits (exact at the start of each row, a division there;
         * the steps lose under 2^-21 over a row, covered by the bias). */
        const Vertex *a = p[0], *b = p[1], *c = p[2];
        s32 e0x = c->y - b->y, e0y = -(c->x - b->x); /* steps of w0 */
        s32 e1x = a->y - c->y, e1y = -(a->x - c->x);
        s32 e2x = b->y - a->y, e2y = -(b->x - a->x);
        s32 w0r = -(s32)edge(b, c, minx, miny), w1r = -(s32)edge(c, a, minx, miny), w2r = -(s32)edge(a, b, minx, miny);
        s32 at[5][3], na = 0, k;
        s64 step[5];

        if (gouraud) {
            at[0][0] = a->r, at[0][1] = b->r, at[0][2] = c->r;
            at[1][0] = a->g, at[1][1] = b->g, at[1][2] = c->g;
            at[2][0] = a->b, at[2][1] = b->b, at[2][2] = c->b;
            na = 3;
        }
        if (m->textured) {
            at[na][0] = a->u, at[na][1] = b->u, at[na][2] = c->u;
            na++;
            at[na][0] = a->v, at[na][1] = b->v, at[na][2] = c->v;
            na++;
        }
        for (k = 0; k < na; k++) {
            s64 dn = (s64)e0x * at[k][0] + (s64)e1x * at[k][1] + (s64)e2x * at[k][2];

            step[k] = (s64)(((dn < 0 ? -dn : dn) << 32) / area) * (dn < 0 ? -1 : 1);
        }
        for (y = miny; y <= maxy; y++, w0r += e0y, w1r += e1y, w2r += e2y) {
            s32 w0 = w0r, w1 = w1r, w2 = w2r;
            s64 f[5];
            s32 val[5];
            s32 started = 0;

            for (x = minx; x <= maxx; x++, w0 += e0x, w1 += e1x, w2 += e2x) {
                s32 r, g, bl, u = 0, v = 0;

                if (w0 + bias0 < 0 || w1 + bias1 < 0 || w2 + bias2 < 0) {
                    if (started) {
                        break; /* a triangle is convex: the row is done */
                    }
                    continue;
                }
                if (!started) {
                    started = 1;
                    for (k = 0; k < na; k++) {
                        s64 n = (s64)w0 * at[k][0] + (s64)w1 * at[k][1] + (s64)w2 * at[k][2];

                        f[k] = (s64)(((u64)n << 32) / (u64)area) + (1 << 11);
                    }
                }
                for (k = 0; k < na; k++) {
                    val[k] = (s32)(f[k] >> 32);
                    f[k] += step[k];
                }
                if (gouraud) {
                    r = val[0];
                    g = val[1];
                    bl = val[2];
                } else {
                    r = v0.r;
                    g = v0.g;
                    bl = v0.b;
                }
                if (m->textured) {
                    u = val[na - 2];
                    v = val[na - 1];
                }
                pixel(m, x, y, r, g, bl, u, v);
            }
        }
    }
}

/* ---- commands ---- */

static void set_texpage(u32 w)
{
    st.tx = (w & 15) * 64;
    st.ty = (w >> 4 & 1) * 256;
    st.abr = w >> 5 & 3;
    st.tp = w >> 7 & 3;
    if (st.tp == 3) {
        st.tp = 2;
    }
}

static void mode_from_state(Mode *m, u32 cmd)
{
    m->semi = (cmd >> 25) & 1;
    m->raw = (cmd >> 24) & 1;
    m->abr = st.abr;
    m->tx = st.tx;
    m->ty = st.ty;
    m->tp = st.tp;
    m->textured = 0;
    m->dither = 0;
    m->cx = m->cy = 0;
}

static void color(Vertex *v, u32 w)
{
    v->r = w & 255;
    v->g = (w >> 8) & 255;
    v->b = (w >> 16) & 255;
}

static void xy(Vertex *v, u32 w)
{
    v->x = sx11(w) + st.ox;
    v->y = sx11(w >> 16) + st.oy;
}

/* a polygon: returns the number of words */
static u32 polygon(const u32 *w)
{
    u32 cmd = w[0] >> 24;
    s32 quad = cmd & 8, gouraud = cmd & 0x10, textured = cmd & 4;
    s32 n = quad ? 4 : 3, i, k = 0;
    Vertex v[4];
    Mode m;

    mode_from_state(&m, w[0]);
    for (i = 0; i < n; i++) {
        if (i == 0 || gouraud) {
            color(&v[i], w[k++]);
        } else {
            v[i].r = v[0].r;
            v[i].g = v[0].g;
            v[i].b = v[0].b;
        }
        xy(&v[i], w[k++]);
        if (textured) {
            u32 t = w[k++];

            v[i].u = t & 255;
            v[i].v = (t >> 8) & 255;
            if (i == 0) {
                m.cx = ((t >> 16) & 0x3F) * 16;
                m.cy = (t >> 22) & 0x1FF;
            } else if (i == 1) {
                /* the texture page of the polygon becomes the current one */
                set_texpage(t >> 16);
                st.tex_disable = 0;
            }
        } else {
            v[i].u = v[i].v = 0;
        }
    }
    if (textured) {
        m.textured = 1;
        m.tx = st.tx;
        m.ty = st.ty;
        m.tp = st.tp;
        m.abr = st.abr;
    } else {
        m.raw = 0;
    }
    m.dither = st.dither && (gouraud || (textured && !m.raw));
    triangle(&m, v[0], v[1], v[2], gouraud);
    if (quad) {
        triangle(&m, v[1], v[2], v[3], gouraud);
    }
    return (u32)k;
}

static void line(const Mode *m, Vertex a, Vertex b, s32 gouraud)
{
    s32 dx = b.x - a.x, dy = b.y - a.y;
    s32 n = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
    s32 i;

    if (dx >= 1024 || -dx >= 1024 || dy >= 512 || -dy >= 512) {
        return;
    }
    for (i = 0; i <= n; i++) {
        s32 x = n ? a.x + (dx * i + (dx >= 0 ? n / 2 : -n / 2)) / n : a.x;
        s32 y = n ? a.y + (dy * i + (dy >= 0 ? n / 2 : -n / 2)) / n : a.y;
        s32 r = a.r, g = a.g, bl = a.b;

        if (gouraud && n) {
            r = a.r + (b.r - a.r) * i / n;
            g = a.g + (b.g - a.g) * i / n;
            bl = a.b + (b.b - a.b) * i / n;
        }
        if (x >= st.x1 && x <= st.x2 && y >= st.y1 && y <= st.y2) {
            pixel(m, x, y, r, g, bl, 0, 0);
        }
    }
}

/* a line or polyline: returns the number of words (count: the words there) */
static u32 lines(const u32 *w, u32 count)
{
    u32 cmd = w[0] >> 24;
    s32 gouraud = cmd & 0x10, poly = cmd & 8;
    Vertex a, b;
    Mode m;
    u32 k = 0;

    mode_from_state(&m, w[0]);
    m.raw = 0;
    m.dither = st.dither && gouraud;
    color(&a, w[k++]);
    xy(&a, w[k++]);
    for (;;) {
        if (k >= count) {
            break;
        }
        if (poly && (w[k] & 0xF000F000) == 0x50005000) {
            k++;
            break;
        }
        if (gouraud) {
            color(&b, w[k++]);
        } else {
            b.r = a.r;
            b.g = a.g;
            b.b = a.b;
        }
        if (k >= count) {
            break;
        }
        xy(&b, w[k++]);
        line(&m, a, b, gouraud);
        a = b;
        if (!poly) {
            break;
        }
    }
    return k;
}

/* a rectangle or sprite: returns the number of words */
static u32 rectangle(const u32 *w)
{
    u32 cmd = w[0] >> 24;
    s32 textured = cmd & 4, size = (cmd >> 3) & 3;
    s32 x0, y0, ww, hh, x, y, u0 = 0, v0 = 0;
    u32 k = 1;
    Mode m;
    Vertex c;

    mode_from_state(&m, w[0]);
    color(&c, w[0]);
    x0 = sx11(w[k]) + st.ox;
    y0 = sx11(w[k] >> 16) + st.oy;
    k++;
    if (textured) {
        u0 = w[k] & 255;
        v0 = (w[k] >> 8) & 255;
        m.cx = ((w[k] >> 16) & 0x3F) * 16;
        m.cy = (w[k] >> 22) & 0x1FF;
        m.textured = 1;
        k++;
    } else {
        m.raw = 0;
    }
    if (size == 0) {
        ww = w[k] & 0x3FF;
        hh = (w[k] >> 16) & 0x1FF;
        k++;
    } else {
        ww = hh = size == 1 ? 1 : size == 2 ? 8 : 16;
    }
    for (y = 0; y < hh; y++) {
        if (y0 + y < st.y1 || y0 + y > st.y2) {
            continue;
        }
        for (x = 0; x < ww; x++) {
            if (x0 + x < st.x1 || x0 + x > st.x2) {
                continue;
            }
            pixel(&m, x0 + x, y0 + y, c.r, c.g, c.b, u0 + x, v0 + y);
        }
    }
    return k;
}

void gpu_fill(s32 x, s32 y, s32 w, s32 h, u32 rgb)
{
    u16 c = (u16)(((rgb & 255) >> 3) | (((rgb >> 8) & 255) >> 3) << 5 | (((rgb >> 16) & 255) >> 3) << 10);
    s32 i, j;

    x &= 0x3F0;
    y &= 0x1FF;
    w = ((w & 0x3FF) + 15) & ~15;
    h &= 0x1FF;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            gpu_vram[(y + j) & 511][(x + i) & 1023] = c;
        }
    }
}

void gpu_move(s32 sx, s32 sy, s32 dx, s32 dy, s32 w, s32 h)
{
    if (trace && !tracing_commands) {
        u32 d[6];

        d[0] = sx; d[1] = sy; d[2] = dx; d[3] = dy; d[4] = w; d[5] = h;
        trace_rec(3, d, 6, NULL, 0);
    }
    static u16 line_buf[GPU_VRAM_W];
    s32 i, j;

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            line_buf[i] = gpu_vram[(sy + j) & 511][(sx + i) & 1023];
        }
        for (i = 0; i < w; i++) {
            u16 *d = &gpu_vram[(dy + j) & 511][(dx + i) & 1023];

            if (!(st.mask_check && (*d & 0x8000))) {
                *d = line_buf[i] | (st.mask_set ? 0x8000 : 0);
            }
        }
    }
}

void gpu_load(s32 x, s32 y, s32 w, s32 h, const u16 *src)
{
    if (trace && !tracing_commands && w > 0 && h > 0) {
        u32 d[4];

        d[0] = x; d[1] = y; d[2] = w; d[3] = h;
        trace_rec(2, d, 4, src, ((u32)(w * h) + 1) / 2);
    }
    s32 i, j;

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            gpu_vram[(y + j) & 511][(x + i) & 1023] = *src++;
        }
    }
}

void gpu_store(s32 x, s32 y, s32 w, s32 h, u16 *dst)
{
    s32 i, j;

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            *dst++ = gpu_vram[(y + j) & 511][(x + i) & 1023];
        }
    }
}

void gpu_commands(const u32 *w, u32 count)
{
    u32 k = 0;

    if (trace && count) {
        trace_rec(1, w, count, NULL, 0);
    }
    tracing_commands = 1;
    while (k < count) {
        u32 cmd = w[k] >> 24;
        u32 n = 1;

        if (cmd >= 0x20 && cmd < 0x40) {
            n = polygon(w + k);
        } else if (cmd >= 0x40 && cmd < 0x60) {
            n = lines(w + k, count - k);
        } else if (cmd >= 0x60 && cmd < 0x80) {
            n = rectangle(w + k);
        } else if (cmd == 0x02) {
            gpu_fill(w[k + 1] & 0xFFFF, w[k + 1] >> 16, w[k + 2] & 0xFFFF, w[k + 2] >> 16, w[k]);
            n = 3;
        } else if (cmd >= 0x80 && cmd < 0xA0) {
            gpu_move(w[k + 1] & 1023, (w[k + 1] >> 16) & 511, w[k + 2] & 1023, (w[k + 2] >> 16) & 511,
                     ((w[k + 3] - 1) & 1023) + 1, (((w[k + 3] >> 16) - 1) & 511) + 1);
            n = 4;
        } else if (cmd >= 0xA0 && cmd < 0xC0) {
            s32 ww = ((w[k + 2] - 1) & 1023) + 1, hh = (((w[k + 2] >> 16) - 1) & 511) + 1;

            gpu_load(w[k + 1] & 1023, (w[k + 1] >> 16) & 511, ww, hh, (const u16 *)(w + k + 3));
            n = 3 + (ww * hh + 1) / 2;
        } else if (cmd >= 0xC0 && cmd < 0xE0) {
            n = 3; /* VRAM to CPU: StoreImage does it */
        } else {
            switch (cmd) {
            case 0xE1:
                set_texpage(w[k]);
                st.dither = (w[k] >> 9) & 1;
                st.tex_disable = (w[k] >> 11) & 1;
                break;
            case 0xE2:
                st.twmx = w[k] & 31;
                st.twmy = (w[k] >> 5) & 31;
                st.twox = (w[k] >> 10) & 31;
                st.twoy = (w[k] >> 15) & 31;
                break;
            case 0xE3:
                st.x1 = w[k] & 1023;
                st.y1 = (w[k] >> 10) & 511;
                break;
            case 0xE4:
                st.x2 = w[k] & 1023;
                st.y2 = (w[k] >> 10) & 511;
                break;
            case 0xE5:
                st.ox = sx11(w[k]);
                st.oy = sx11(w[k] >> 11);
                break;
            case 0xE6:
                st.mask_set = w[k] & 1;
                st.mask_check = (w[k] >> 1) & 1;
                break;
            default:
                break; /* nop, cache clear, interrupt */
            }
        }
        k += n;
    }
    tracing_commands = 0;
}

/* the display area as 0xRRGGBB */
void gpu_display_rgb(u32 *out)
{
    s32 x, y;

    for (y = 0; y < gpu_display.h; y++) {
        const u16 *row = gpu_vram[(gpu_display.y + y) & 511];

        if (gpu_display.rgb24) {
            const u8 *b = (const u8 *)row;

            for (x = 0; x < gpu_display.w; x++) {
                u32 o = (u32)(gpu_display.x * 2 + x * 3) & 2047;

                *out++ = (u32)b[o] << 16 | (u32)b[(o + 1) & 2047] << 8 | b[(o + 2) & 2047];
            }
        } else {
            for (x = 0; x < gpu_display.w; x++) {
                u16 c = row[(gpu_display.x + x) & 1023];
                u32 r = (c & 31) << 3, g = ((c >> 5) & 31) << 3, b = ((c >> 10) & 31) << 3;

                *out++ = (r | r >> 5) << 16 | (g | g >> 5) << 8 | (b | b >> 5);
            }
        }
    }
}
