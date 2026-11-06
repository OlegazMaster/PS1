#include "common.h"
#include "gte_api.h"

/* Naughty Dog's hand-written assembly (the nd_asm segment, 0x80039E3C-
 * 0x80045750) rewritten in C for the PC port, one function at a time. Like
 * goolvm.c this file is not linked into the PlayStation build, which keeps
 * the original assembly; tools/nd_test.sh checks each function against it.
 * See docs/nd_asm.md for the list of functions. */

extern s16 D_8005C91C[]; /* sine table: a quarter wave, 0x401 entries */

/* Sine of a 12-bit angle, 0x1000 = 1. */
s32 func_8003C2F0(s32 a) {
    a &= 0xFFF;
    if (a < 0x800) {
        if (a < 0x400) {
            return D_8005C91C[a];
        }
        return D_8005C91C[0x800 - a];
    }
    if (a < 0xC00) {
        return -D_8005C91C[a - 0x800];
    }
    return -D_8005C91C[0x1000 - a];
}

/* Cosine of a 12-bit angle. */
s32 func_8003C364(s32 a) {
    a &= 0xFFF;
    if (a < 0x800) {
        if (a < 0x400) {
            return D_8005C91C[0x400 - a];
        }
        return -D_8005C91C[a - 0x400];
    }
    if (a < 0xC00) {
        return -D_8005C91C[0xC00 - a];
    }
    return D_8005C91C[a - 0xC00];
}

/* (x * x + z * z) >> 1, with the sum in 64 bits (the high word of a 32-bit
 * multiply: no long long in this compiler, so done by halves). */
static u32 MulHi(u32 a, u32 b) {
    u32 al = a & 0xFFFF;
    u32 ah = a >> 16;
    u32 bl = b & 0xFFFF;
    u32 bh = b >> 16;
    u32 lo = al * bl;
    u32 m1 = ah * bl;
    u32 m2 = al * bh;
    u32 mid = (lo >> 16) + (m1 & 0xFFFF) + (m2 & 0xFFFF);

    return ah * bh + (m1 >> 16) + (m2 >> 16) + (mid >> 16);
}

/* The 64-bit square of |x| as hi:lo (the R3000's signed mult of two
 * non-negative values; |0x80000000| stays negative, as there). */
static void Square(s32 x, u32 *hi, u32 *lo) {
    u32 ux;

    if (x < 0) {
        x = -x;
    }
    ux = x;
    *lo = ux * ux;
    *hi = MulHi(ux, ux);
}

s32 func_8003AE90(s32 x, s32 z) {
    u32 xh, xl, zh, zl, lo, hi;

    Square(x, &xh, &xl);
    Square(z, &zh, &zl);
    lo = xl + zl;
    hi = xh + zh + (lo < xl);
    return (lo >> 1) | (hi << 31);
}

/* (x * x + y * y + z * z) >> 20 (0x8003AEEC, no symbol). */
s32 func_8003AEEC(s32 x, s32 y, s32 z) {
    u32 xh, xl, yh, yl, zh, zl, lo, hi, lo2;

    Square(x, &xh, &xl);
    Square(y, &yh, &yl);
    Square(z, &zh, &zl);
    lo = xl + yl;
    hi = xh + yh + (lo < xl);
    lo2 = lo + zl;
    hi = hi + zh + (lo2 < lo);
    return (lo2 >> 20) | (hi << 12);
}

/* Copies count halfwords from src to dst (the original picks word or
 * halfword moves by the alignment of both; for areas that do not overlap
 * the result is the same). A negative count copies nothing (the original
 * jumps through a table with it). */
void func_8003A928(u16 *src, u16 *dst, s32 count) {
    while (count > 0) {
        *dst++ = *src++;
        count--;
    }
}

/* Clears an ordering table: each of the (count >> 6) * 64 entries links to
 * the next one (24-bit addresses), the last one ends the list (0xFFFFFF).
 * count is always 0x800 in the game; below 64 the original runs past the
 * end of the table. */
void func_8003AC44(u32 *ot, u32 count) {
    u32 link = (u32)ot & 0xFFFFFF;
    u32 n = (count >> 6) * 64;

    if (n == 0) {
        return;
    }
    while (n-- != 0) {
        link += 4;
        *ot++ = link;
    }
    ot[-1] = 0xFFFFFF;
}

extern s32 ticks_elapsed;

/* The root counter interrupt: counts the ticks. */
void func_8003AE70(void) {
    ticks_elapsed++;
}

/* Integer square root (bit by bit, two bits of x per step). */
s32 func_8003D9DC(s32 x) {
    u32 v = x;
    u32 root = 0;
    s32 rem = 0;
    s32 t;
    s32 i;

    if (v == 0) {
        return 0;
    }
    for (i = 0; i < 16; i++) {
        rem |= v >> 30;
        t = rem - (s32)((root << 2) + 1);
        root <<= 1;
        if (t >= 0) {
            root++;
            rem = t;
        }
        v <<= 2;
        rem <<= 2;
    }
    return root;
}

#ifndef ND_SCRATCHPAD
#define ND_SCRATCHPAD ((u32 *)0x1F800000)
#endif

/* Marks the cell (x, z) (-32..31) of the grid of free cells around an
 * object (coll.c): once per cell (bit map at scratchpad 0x180, two words per
 * row), ORs the 32-row mask at scratchpad 0x380, shifted by x, into the rows
 * from z of the map at scratchpad 0x100. (0x8003BBFC is the same code with
 * the arguments in v1 and a3.) */
void func_8003BBF0(s32 x, s32 z) {
    u32 *cell = ND_SCRATCHPAD + 0x180 / 4 + (z + 0x20) * 2;
    u32 *mask = ND_SCRATCHPAD + 0x380 / 4;
    u32 *row;
    u32 bit;
    s32 i;
    s32 end;

    if (x < 0) {
        bit = 1 << ((x + 0x20) & 31);
    } else {
        cell++;
        bit = 1 << (x & 31);
    }
    if (*cell & bit) {
        return;
    }
    *cell |= bit;
    if (z < 0) {
        i = -z;
        end = 0x20;
        z = 0;
    } else {
        i = 0;
        end = 0x20 - z;
    }
    row = ND_SCRATCHPAD + 0x100 / 4 + z;
    if (x >= 0) {
        for (; i < end; i++) {
            *row++ |= mask[i] >> (x & 31);
        }
    } else {
        for (; i < end; i++) {
            *row++ |= mask[i] << (-x & 31);
        }
    }
}

/* Marks in the grid of free cells (func_8003BBF0) the edges of the boxes
 * of the list rec that are in the height range: rec holds 16-byte headers
 * (the sizes of the boxes that follow: x, y, z << 8 and their shift limits)
 * and 8-byte boxes (type in bits 1-3, kind in bits 4-9 of the low halfword,
 * position (s16 << 4) relative to origin), ended by 0xFFFF. Boxes of types
 * 3 and 4 are skipped, and of the kinds 1..0x1A unless mode is set. The
 * grid is in cells of 1 << 11 around (x, z), 64 cells across. */
void func_8003B874(u32 *rec, s32 *origin, s32 mode, s32 y0, s32 y1, s32 top, s32 x, s32 z) {
    u32 sx = 0, sy = 0, sz = 0;
    u32 lx = 0, ly = 0, lz = 0;
    u32 w0;
    u32 w1;
    u32 flags;
    u32 t;
    u32 kind;
    s32 shift;
    s32 x0, x1, y0b, y1b, z0, z1;
    s32 i;
    s32 j;
    s32 hit;

    for (;;) {
        w0 = rec[0];
        w1 = rec[1];
        if ((w0 & 0xFFFF) == 0xFFFF) {
            return;
        }
        if ((w0 & 0xFFFF) == 0) {
            sx = (w0 >> 16) << 8;
            sy = (w1 & 0xFFFF) << 8;
            sz = (w1 >> 16) << 8;
            lx = rec[2] >> 16;
            ly = rec[3] & 0xFFFF;
            lz = rec[3] >> 16;
            rec += 4;
            continue;
        }
        shift = w0 & 7;
        flags = ((w0 & 0xFFF8) >> 2) | 1;
        x0 = ((s32)(w0 & 0xFFFF0000) >> 12) + origin[0];
        y0b = ((s32)(w1 << 16) >> 12) + origin[1];
        z0 = ((s32)(w1 & 0xFFFF0000) >> 12) + origin[2];
        x1 = x0 + (sx >> ((lx < (u32)shift ? lx : shift) & 31));
        y1b = y0b + (sy >> ((ly < (u32)shift ? ly : shift) & 31));
        z1 = z0 + (sz >> ((lz < (u32)shift ? lz : shift) & 31));
        t = (flags & 0xE) >> 1;
        kind = (flags & 0x3F0) >> 4;
        rec += 2;
        if (t == 3 || t == 4) {
            continue;
        }
        if (t == 1 || t == 5) {
            hit = y0 < y1b && y0b < top;
        } else if (t == 6) {
            hit = y0b < y0 && y1 < y1b;
        } else {
            if (kind != 0 && kind < 0x1B && mode == 0) {
                continue;
            }
            hit = y1 < y1b && y0b < top;
        }
        if (!hit) {
            continue;
        }
        x0 = ((x0 - x) << 2) >> 13;
        x1 = ((x1 - x) << 2) >> 13;
        z0 = ((z0 - z) << 2) >> 13;
        z1 = ((z1 - z) << 2) >> 13;
        /* the four edges, as the original walks them */
        if (-0x20 < z0 && z0 < 0x20) {
            for (i = x0;; i += 8) {
                if (-0x20 < i) {
                    if (i >= 0x20 || i >= x1) {
                        break;
                    }
                    func_8003BBF0(i, z0);
                }
            }
        }
        j = z0;
        if (-0x20 < x1 && x1 < 0x20) {
            for (;; j += 8) {
                if (-0x20 < j) {
                    if (j >= 0x20 || j >= z1) {
                        break;
                    }
                    func_8003BBF0(x1, j);
                }
            }
        }
        i = x1;
        j = z1;
        if (-0x20 < j && j < 0x20) {
            for (;; i -= 8) {
                if (i < 0x20) {
                    if (!(-0x20 < i) || !(x0 < i)) {
                        break;
                    }
                    func_8003BBF0(i, j);
                }
            }
        }
        if (x0 < 0x20 && -0x20 < x0) {
            for (;; j -= 8) {
                if (j < 0x20) {
                    if (!(-0x20 < j) || !(z0 < j)) {
                        break;
                    }
                    func_8003BBF0(x0, j);
                }
            }
        }
    }
}

/* Goes through the boxes of the list rec (as in func_8003B874) that
 * overlap the box b (min x, y, z, max x, y, z): calls cb(obj, flags) for
 * boxes of types 2..7 and for those of kinds 1..0x2F (a non-zero result
 * skips the box), and averages the tops of the rest at or below top, the
 * floors (type 0) into a[1] and the others into a[2] (best if none). The
 * value the original leaves in v0 is not meaningful; this returns 0. */
s32 func_8003BD18(void *obj, u32 *rec, s32 *origin, s32 *b, s32 top, void *a, s32 best,
                  s32 (*cb)(void *, u32)) {
    s32 *out = (s32 *)a;
    u32 sx = 0, sy = 0, sz = 0;
    u32 lx = 0, ly = 0, lz = 0;
    u32 w0;
    u32 w1;
    u32 flags;
    u32 t;
    s32 shift;
    s32 x0, x1, y0, y1, z0, z1;
    s32 n0 = 0, sum0 = 0, n1 = 0, sum1 = 0;

    for (;;) {
        w0 = rec[0];
        w1 = rec[1];
        if ((w0 & 0xFFFF) == 0xFFFF) {
            break;
        }
        if ((w0 & 0xFFFF) == 0) {
            sx = (w0 >> 16) << 8;
            sy = (w1 & 0xFFFF) << 8;
            sz = (w1 >> 16) << 8;
            lx = rec[2] >> 16;
            ly = rec[3] & 0xFFFF;
            lz = rec[3] >> 16;
            rec += 4;
            continue;
        }
        rec += 2;
        flags = ((w0 & 0xFFF8) >> 2) | 1;
        t = flags & 0xE;
        shift = w0 & 7;
        x0 = ((s32)(w0 & 0xFFFF0000) >> 12) + origin[0];
        y0 = ((s32)(w1 << 16) >> 12) + origin[1];
        z0 = ((s32)(w1 & 0xFFFF0000) >> 12) + origin[2];
        if (b[3] < x0 || b[4] < y0 || b[5] < z0) {
            continue;
        }
        x1 = x0 + (sx >> ((lx < (u32)shift ? lx : shift) & 31));
        y1 = y0 + (sy >> ((ly < (u32)shift ? ly : shift) & 31));
        z1 = z0 + (sz >> ((lz < (u32)shift ? lz : shift) & 31));
        if (x1 < b[0] || y1 < b[1] || z1 < b[2]) {
            continue;
        }
        if (t >= 4 || ((flags & 0x3F0) != 0 && (flags & 0x3F0) < 0x300)) {
            if (cb(obj, flags) != 0) {
                continue;
            }
        }
        if (top < y1) {
            continue;
        }
        if (t != 0) {
            n1++;
            sum1 += y1;
        } else {
            n0++;
            sum0 += y1;
        }
    }
    out[1] = n0 != 0 ? sum0 / n0 : best;
    out[2] = n1 != 0 ? sum1 / n1 : best;
    return 0;
}

/* The ceiling in the box b (min x, y, z, max x, y, z): of the boxes of the
 * list rec (as in func_8003B874) of types 0, 1, 5 and 6 that overlap b,
 * the lowest bottom (returned when a is set) or the average bottom
 * (otherwise); best if there is none. The lowest bottom of type 6 below
 * *ceil2 goes to *ceil2. obj is not used. */
s32 func_8003C04C(void *obj, u32 *rec, s32 *origin, s32 *b, s32 *ceil2, s32 best, s32 a) {
    u32 sx = 0, sy = 0, sz = 0;
    u32 lx = 0, ly = 0, lz = 0;
    u32 w0;
    u32 w1;
    u32 t;
    s32 shift;
    s32 x0, x1, y0, y1, z0, z1;
    s32 low = 0x7FFFFFFF;
    s32 c = *ceil2;
    s32 n = 0;
    s32 sum = 0;

    for (;;) {
        w0 = rec[0];
        w1 = rec[1];
        if ((w0 & 0xFFFF) == 0xFFFF) {
            break;
        }
        if ((w0 & 0xFFFF) == 0) {
            sx = (w0 >> 16) << 8;
            sy = (w1 & 0xFFFF) << 8;
            sz = (w1 >> 16) << 8;
            lx = rec[2] >> 16;
            ly = rec[3] & 0xFFFF;
            lz = rec[3] >> 16;
            rec += 4;
            continue;
        }
        rec += 2;
        t = (((w0 & 0xFFF8) >> 2) | 1) & 0xE;
        if (!((1 << t) & 0x1405)) { /* types 0, 1, 5, 6 */
            continue;
        }
        shift = w0 & 7;
        x0 = ((s32)(w0 & 0xFFFF0000) >> 12) + origin[0];
        y0 = ((s32)(w1 << 16) >> 12) + origin[1];
        z0 = ((s32)(w1 & 0xFFFF0000) >> 12) + origin[2];
        if (b[3] < x0 || b[4] < y0 || b[5] < z0) {
            continue;
        }
        x1 = x0 + (sx >> ((lx < (u32)shift ? lx : shift) & 31));
        y1 = y0 + (sy >> ((ly < (u32)shift ? ly : shift) & 31));
        z1 = z0 + (sz >> ((lz < (u32)shift ? lz : shift) & 31));
        if (x1 < b[0] || y1 < b[1] || z1 < b[2]) {
            continue;
        }
        n++;
        sum += y0;
        if (y0 < low) {
            low = y0;
            if (t == 12 && y0 < c) {
                c = y0;
            }
        }
    }
    *ceil2 = c;
    if (n == 0) {
        return best;
    }
    if (a != 0) {
        return low;
    }
    return sum / n;
}

/* The state of the walk of func_8003AF78. Sizes and positions change in
 * place as in the original (halved going down, doubled coming back up, so a
 * size that is not a multiple of the halvings loses its low bits). */
typedef struct {
    u8 *item;
    u32 *out;
    s32 n;
    s32 x, y, z;    /* min corner of the node, relative to the box min */
    u32 sx, sy, sz; /* size of the node */
    s32 bx, by, bz; /* size of the box */
    s32 dx, dy, dz; /* depths split along each axis */
} TreeWalk;

static void TreeVisit(TreeWalk *w, u32 node, s32 depth);

/* Visits child i of children c if the bits want of the overlap flags are
 * all set, with the position moved to that child. */
static void TreeChild(TreeWalk *w, u16 *c, s32 i, s32 depth, u32 flags, u32 want, s32 mx, s32 my, s32 mz) {
    if ((flags & want) != want) {
        return;
    }
    if (mx) {
        w->x += w->sx;
    }
    if (my) {
        w->y += w->sy;
    }
    if (mz) {
        w->z += w->sz;
    }
    TreeVisit(w, c[i], depth + 1);
    if (mx) {
        w->x -= w->sx;
    }
    if (my) {
        w->y -= w->sy;
    }
    if (mz) {
        w->z -= w->sz;
    }
}

static void TreeVisit(TreeWalk *w, u32 node, s32 depth) {
    u16 *c;
    u32 flags = 0;
    s32 ax, ay, az;

    if (node == 0) {
        return;
    }
    if (node & 1) {
        if (w->n < 0x200) {
            w->out[0] = ((node << 2) & 0xFFF8) | depth | (((u32)w->x >> 4) << 16);
            w->out[1] = (((u32)w->z >> 4) << 16) | (((u32)w->y >> 4) & 0xFFFF);
            w->out += 2;
            w->n++;
        }
        return;
    }
    c = (u16 *)(w->item + node);
    ax = depth < w->dx;
    ay = depth < w->dy;
    az = depth < w->dz;
    if (ax) {
        w->sx >>= 1;
    }
    if (ay) {
        w->sy >>= 1;
    }
    if (az) {
        w->sz >>= 1;
    }
    if (ax + ay + az == 1) {
        /* one axis: the low half if its end is past the box min, then the
         * high half if it starts before the box max */
        if (ax) {
            if (w->x + (s32)w->sx >= 0) {
                TreeVisit(w, c[0], depth + 1);
            }
            w->x += w->sx;
            if (!(w->bx < w->x)) {
                TreeVisit(w, c[1], depth + 1);
            }
            w->x -= w->sx;
        } else if (ay) {
            if (w->y + (s32)w->sy >= 0) {
                TreeVisit(w, c[0], depth + 1);
            }
            w->y += w->sy;
            if (!(w->by < w->y)) {
                TreeVisit(w, c[1], depth + 1);
            }
            w->y -= w->sy;
        } else {
            if (w->z + (s32)w->sz >= 0) {
                TreeVisit(w, c[0], depth + 1);
            }
            w->z += w->sz;
            if (!(w->bz < w->z)) {
                TreeVisit(w, c[1], depth + 1);
            }
            w->z -= w->sz;
        }
    } else if (ax + ay + az > 1) {
        /* overlap flags: 0x10000/0x20000 low/high x, 0x40000/0x80000 y,
         * 0x100000/0x200000 z */
        if (ax) {
            if (w->x + (s32)w->sx >= 0) {
                flags |= 0x10000;
            }
            if (!(w->bx < w->x + (s32)w->sx)) {
                flags |= 0x20000;
            }
        }
        if (ay) {
            if (w->y + (s32)w->sy >= 0) {
                flags |= 0x40000;
            }
            if (!(w->by < w->y + (s32)w->sy)) {
                flags |= 0x80000;
            }
        }
        if (az) {
            if (w->z + (s32)w->sz >= 0) {
                flags |= 0x100000;
            }
            if (!(w->bz < w->z + (s32)w->sz)) {
                flags |= 0x200000;
            }
        }
        if (!az) { /* x and y: children (x, y) = 00, 01, 10, 11 */
            TreeChild(w, c, 0, depth, flags, 0x50000, 0, 0, 0);
            TreeChild(w, c, 1, depth, flags, 0x90000, 0, 1, 0);
            TreeChild(w, c, 2, depth, flags, 0x60000, 1, 0, 0);
            TreeChild(w, c, 3, depth, flags, 0xA0000, 1, 1, 0);
        } else if (!ay) { /* x and z */
            TreeChild(w, c, 0, depth, flags, 0x110000, 0, 0, 0);
            TreeChild(w, c, 1, depth, flags, 0x210000, 0, 0, 1);
            TreeChild(w, c, 2, depth, flags, 0x120000, 1, 0, 0);
            TreeChild(w, c, 3, depth, flags, 0x220000, 1, 0, 1);
        } else if (!ax) { /* y and z */
            TreeChild(w, c, 0, depth, flags, 0x140000, 0, 0, 0);
            TreeChild(w, c, 1, depth, flags, 0x240000, 0, 0, 1);
            TreeChild(w, c, 2, depth, flags, 0x180000, 0, 1, 0);
            TreeChild(w, c, 3, depth, flags, 0x280000, 0, 1, 1);
        } else { /* all three: children (x, y, z) = 000 .. 111 */
            TreeChild(w, c, 0, depth, flags, 0x150000, 0, 0, 0);
            TreeChild(w, c, 1, depth, flags, 0x250000, 0, 0, 1);
            TreeChild(w, c, 2, depth, flags, 0x190000, 0, 1, 0);
            TreeChild(w, c, 3, depth, flags, 0x290000, 0, 1, 1);
            TreeChild(w, c, 4, depth, flags, 0x160000, 1, 0, 0);
            TreeChild(w, c, 5, depth, flags, 0x260000, 1, 0, 1);
            TreeChild(w, c, 6, depth, flags, 0x1A0000, 1, 1, 0);
            TreeChild(w, c, 7, depth, flags, 0x2A0000, 1, 1, 1);
        }
    }
    /* (a node split along no axis hangs the original; here it is skipped) */
    if (ax) {
        w->sx <<= 1;
    }
    if (ay) {
        w->sy <<= 1;
    }
    if (az) {
        w->sz <<= 1;
    }
}

/* Collects the leaves of the surface tree of a zone (item: origin x, y, z,
 * size x, y, z, root node at 0x1C, depth splits at 0x1E) that overlap box
 * (min x, y, z, max x, y, z) into out: two header records (sizes and
 * depths), then one 8-byte record per leaf (node, depth, position relative
 * to the box min >> 4), at most 0x200 in all counting n. Returns the new
 * count (n + 2 + leaves). The original keeps its stack in 7 registers, so
 * trees deeper than 7 crash it; here they work. */
s32 func_8003AF78(void *item, s32 *box, u16 *out, s32 n) {
    u32 *it = (u32 *)item;
    u16 *h = (u16 *)item;
    TreeWalk w;

    out[0] = 0;
    out[1] = it[3];
    out[2] = it[4];
    out[3] = it[5];
    out[4] = 0;
    out[5] = h[0xF];
    out[6] = h[0x10];
    out[7] = h[0x11];
    w.item = (u8 *)item;
    w.out = (u32 *)(out + 8);
    w.n = n + 2;
    w.x = (it[0] << 8) - box[0];
    w.y = (it[1] << 8) - box[1];
    w.z = (it[2] << 8) - box[2];
    w.sx = it[3] << 8;
    w.sy = it[4] << 8;
    w.sz = it[5] << 8;
    w.bx = box[3] - box[0];
    w.by = box[4] - box[1];
    w.bz = box[5] - box[2];
    w.dx = h[0xF];
    w.dy = h[0x10];
    w.dz = h[0x11];
    TreeVisit(&w, h[0xE], 0);
    return w.n;
}

/* ---- Deltas of halfword lists (func_80039E3C applies one, func_8003A234
 * undoes it) ----
 *
 * A list is a u16 count, a u16 pad, then the items. A delta is a header of
 * four u16 (h0: end of the swap commands, h2: end of the first stream, h3:
 * end of the second; ends in halfwords from delta + 4) and three streams of
 * u16 from delta + 8: removals, insertions, swaps.
 * - A removal/insertion command is 0xFFFF (none) or (count - 1) << 12 |
 *   index, followed (insertions) by the values. As in the original, the
 *   last halfword before a stream's end is never read as a command.
 * - A swap command is 0x8000 | b << 11 | a (swap a and a + b + 1),
 *   0x4000 | d << 5 | b (a += d, then swap a and a + b + 0x11),
 *   0x2000 | x | a (item a ^= x, x of 0x1800), or a, b in two halfwords
 *   (swap a and a + b + 1); 0xFFFF ends them.
 * These functions are the original's state machines written out with the
 * same variables. */

typedef struct {
    u16 *p;   /* next halfword */
    u16 *end; /* end of the stream */
    s32 idx;  /* position of the command (t1/t0) */
    s32 left; /* items left in the command (t3/t2) */
    u32 val;  /* the next value (t8/s7) */
} DeltaStream;

static void DeltaRead(DeltaStream *s) {
    u32 at = *s->p++;

    s->left = 0;
    if (s->p < s->end && at != 0xFFFF) {
        s->val = at;
        s->idx = at & 0xFFF;
        s->left = ((at & 0xF000) >> 12) + 1;
    } else {
        s->idx = 0xFFFF << 8;
    }
    if (s->left > 0) {
        s->val = *s->p++;
    }
}

/* Copies n items (at least one) from *src to *dst. */
static void DeltaCopy(u16 **src, u16 **dst, s32 *out, s32 *in, s32 n) {
    if (n < 2) {
        n = 1;
    }
    *out += n;
    *in += n;
    while (n-- > 0) {
        *(*dst)++ = *(*src)++;
    }
}

static void DeltaSwap(u16 *items, s32 a, s32 b) {
    u16 t = items[a];

    items[a] = items[b];
    items[b] = t;
}

void func_80039E3C(u16 *delta, u16 *in, u16 *out) {
    DeltaStream rem;
    DeltaStream ins;
    u16 *src = in + 2;
    u16 *dst = out + 2;
    u16 *items = out + 2;
    s32 count = in[0]; /* gp */
    s32 n = 0;         /* v0: items written */
    s32 pos = 0;       /* v1: items of in done */
    s32 removed = 0;   /* t5 */
    s32 a, b, m;
    u16 *p;
    u16 *end;
    u32 c;
    s32 last = 0; /* s4 (the original leaves the caller's value) */

    rem.p = delta + 4;
    rem.end = delta + delta[2] + 2;
    ins.p = rem.end;
    ins.end = delta + delta[3] + 2;
    DeltaRead(&rem);
    DeltaRead(&ins);
    do {
        if (pos < count && rem.left > 0 && rem.idx - 1 - pos == 0) {
            rem.left--;
            removed++;
            pos++;
            src++;
            if (rem.left > 0) {
                rem.val = *rem.p++;
            } else {
                DeltaRead(&rem);
            }
        } else if (ins.left > 0 && ins.idx + removed - pos == 0) {
            *dst++ = ins.val;
            n++;
            ins.left--;
            if (ins.left > 0) {
                ins.val = *ins.p++;
            } else {
                DeltaRead(&ins);
            }
        } else if (pos < count) {
            a = rem.idx - pos - 1;
            b = ins.idx + removed - pos;
            m = a < b ? a : b;
            b = count - pos - 1;
            m = m < b ? m : b;
            DeltaCopy(&src, &dst, &n, &pos, m);
        }
    } while (pos < count || ins.left > 0 || rem.left > 0);

    p = delta + delta[3] + 2;
    end = delta + delta[0] + 2;
    while (p < end) {
        c = *p++;
        if (c == 0xFFFF) {
            break;
        }
        if (c & 0x8000) {
            b = (c & 0x7800) >> 11;
            last = c & 0x7FF;
        } else if (c & 0x4000) {
            b = (c & 0x1F) + 0x10;
            last += (c & 0x3FE0) >> 5;
        } else if (c & 0x2000) {
            items[c & 0x7FF] ^= c & 0x1800;
            /* the original leaves the item's address as the base of the next
             * relative swap (such data would crash it) */
            last = (s32)&items[c & 0x7FF];
            continue;
        } else {
            last = c & 0xFFF;
            b = *p++ & 0xFFF;
        }
        DeltaSwap(items, last, last + b + 1);
    }
    out[0] = n;
}

void func_8003A234(u16 *delta, u16 *in, u16 *out) {
    DeltaStream rem;
    DeltaStream ins;
    u16 *items = in + 2;
    u16 *src = in + 2;
    u16 *dst = out + 2;
    s32 count = in[0];
    s32 n = 0;
    s32 pos = 0;
    s32 removed = 0; /* t5 */
    s32 added = 0;   /* t4 */
    s32 a, b, m;
    u16 *p;
    u16 *start;
    u32 c;
    u32 prev;

    /* undo the swaps on in, last first */
    p = delta + delta[0] + 1;
    start = delta + delta[3] + 2;
    while (!(p < start)) {
        c = *p--;
        if (c == 0xFFFF) {
            continue;
        }
        if (c & 0x8000) {
            b = (c & 0x7800) >> 11;
            a = c & 0x7FF;
        } else if (c & 0x4000) {
            /* the base index from the command before */
            prev = *p;
            a = prev & 0x7FF;
            if (!(prev & 0xE000)) {
                a = p[-1] & 0xFFF;
            }
            b = (c & 0x1F) + 0x10;
            a += (c & 0x3FE0) >> 5;
        } else if (c & 0x2000) {
            items[c & 0x7FF] ^= c & 0x1800;
            continue;
        } else {
            b = c & 0xFFF;
            a = *p-- & 0xFFF;
        }
        DeltaSwap(items, a, a + b + 1);
    }

    /* the streams the other way round: insertions are removed, removals
     * are put back */
    rem.p = delta + delta[2] + 2;
    rem.end = delta + delta[3] + 2;
    ins.p = delta + 4;
    ins.end = delta + delta[2] + 2;
    DeltaRead(&rem);
    DeltaRead(&ins);
    do {
        if (pos < count && rem.left > 0 && rem.idx + removed - pos == 0) {
            rem.left--;
            removed++;
            pos++;
            src++;
            if (rem.left > 0) {
                rem.val = *rem.p++;
            } else {
                DeltaRead(&rem);
            }
        } else if (ins.left > 0 && ins.idx + removed - added - 1 - pos == 0) {
            *dst++ = ins.val;
            n++;
            ins.left--;
            added++;
            if (ins.left > 0) {
                ins.val = *ins.p++;
            } else {
                DeltaRead(&ins);
            }
        } else if (pos < count) {
            a = rem.idx + removed - pos;
            b = ins.idx + removed - added - 1 - pos;
            m = a < b ? a : b;
            b = count - pos - 1;
            m = m < b ? m : b;
            DeltaCopy(&src, &dst, &n, &pos, m);
        }
    } while (pos < count || ins.left > 0 || rem.left > 0);
    out[0] = n;
}

/* Turns n points (3 words each) of in by the rotation matrix m (the five
 * words of a GTE matrix) into out (may be in): each coordinate is shifted
 * down by shift for the GTE and back up after, then pos is added. */
void func_8003F520(s32 *out, s32 *in, u32 *m, s32 *pos, s32 n, s32 shift) {
    s32 x, y, z;

    GTE_WC(0, m[0]);
    GTE_WC(1, m[1]);
    GTE_WC(2, m[2]);
    GTE_WC(3, m[3]);
    GTE_WC(4, m[4]);
    while (n > 0) {
        n--;
        x = in[0] >> (shift & 31);
        y = in[1] >> (shift & 31);
        z = in[2] >> (shift & 31);
        GTE_WD(0, (x & 0xFFFF) | (y << 16));
        GTE_WD(1, z);
        in += 3;
        GTE_CMD(GTE_MVMVA_RT_V0);
        GTE_RD(9, x);
        GTE_RD(10, y);
        GTE_RD(11, z);
        out[0] = (x << (shift & 31)) + pos[0];
        out[1] = (y << (shift & 31)) + pos[1];
        out[2] = (z << (shift & 31)) + pos[2];
        out += 3;
    }
}

/* ---- GTE matrix helpers. In the original they take the matrix (five GTE
 * words) in t0..t4 and leave the result there; here m points to it. ---- */

/* RT = m (func_80041FBC) */
void func_80041FBC(u32 *m) {
    GTE_WC(0, m[0]);
    GTE_WC(1, m[1]);
    GTE_WC(2, m[2]);
    GTE_WC(3, m[3]);
    GTE_WC(4, m[4]);
}

/* m = RT * m, and RT = m: the columns of m go through MVMVA as V0..V2. */
void func_80041F18(u32 *m) {
    u32 t0 = m[0], t1 = m[1], t2 = m[2], t3 = m[3], t4 = m[4];
    s32 a, b, c;

    GTE_WD(0, (t0 & 0xFFFF) | (t1 & 0xFFFF0000));
    GTE_WD(1, t3);
    GTE_CMD(0x0486012); /* RT * V0 */
    GTE_WD(2, (t0 >> 16) | (t2 << 16));
    GTE_WD(3, t3 >> 16);
    GTE_RD(9, a);
    GTE_RD(10, b);
    GTE_RD(11, c);
    t0 = a & 0xFFFF;
    t1 = (u32)b << 16;
    t3 = c & 0xFFFF;
    GTE_CMD(0x048E012); /* RT * V1 */
    GTE_WD(4, (m[1] & 0xFFFF) | (m[2] & 0xFFFF0000));
    GTE_WD(5, t4);
    GTE_RD(9, a);
    GTE_RD(10, b);
    GTE_RD(11, c);
    t0 |= (u32)a << 16;
    t2 = b & 0xFFFF;
    t3 |= (u32)c << 16;
    GTE_CMD(0x0496012); /* RT * V2 */
    GTE_RD(9, a);
    GTE_RD(10, b);
    GTE_RD(11, c);
    t1 |= a & 0xFFFF;
    t2 |= (u32)b << 16;
    t4 = c;
    m[0] = t0;
    m[1] = t1;
    m[2] = t2;
    m[3] = t3;
    m[4] = t4;
    func_80041FBC(m);
}

/* RT = rt, then as func_80041F18 (the original falls through into it). */
void func_80041EF0(u32 *rt, u32 *m) {
    GTE_WC(0, rt[0]);
    GTE_WC(1, rt[1]);
    GTE_WC(2, rt[2]);
    GTE_WC(3, rt[3]);
    GTE_WC(4, rt[4]);
    func_80041F18(m);
}

/* Mirrors m and loads it into RT (rows 2 and 3; row 1 is left as it is in
 * RT and in m): row 2 times -0xA0 / 0x100, row 3 negated. */
void func_80041FD8(u32 *m) {
    s32 m21 = (s32)m[1] >> 16;
    s32 m22 = (s32)(m[2] << 16) >> 16;
    s32 m23 = (s32)m[2] >> 16;
    s32 m32 = (s32)m[3] >> 16;

    m[1] = (m[1] & 0xFFFF) | ((u32)-((0xA0 * m21) >> 8) << 16);
    m[2] = ((u32)-((0xA0 * m22) >> 8) & 0xFFFF) | ((u32)-((0xA0 * m23) >> 8) << 16);
    m[3] = (((u32)-(m[3] << 16)) >> 16) | ((u32)-m32 << 16);
    m[4] = -m[4];
    GTE_WC(1, m[1]);
    GTE_WC(2, m[2]);
    GTE_WC(3, m[3]);
    GTE_WC(4, m[4]);
}

/* ---- Height fields: terrain heights on a grid of 0x40000-unit cells
 * (func_80044CA0, func_800450D4 and their helper 0x80044F74). The state
 * (rs, a RenderState) holds: 0xA0 the blend weight (IR0), 0xA4 and 0xA8 two
 * 32x32 tables whose top bytes are the low and high heights, 0xAC the
 * 256x256 cell types (scale in bits 0-4), 0xB0 the cell walls. ---- */

#define RS_W(rs, off) (*(u32 *)((u8 *)(rs) + (off)))
#define OBJ_S(obj, off) (*(s32 *)((u8 *)(obj) + (off)))

/* the byte of a 256x256 grid for cell (x, z << 8); the last row reads the
 * first one */
static u8 *GridCell(u8 *grid, u32 x, u32 z) {
    u32 i = (x & 0xFF) | (z & 0xFF00);

    if ((z & 0xFF00) == 0xFF00) {
        i &= 0xFF;
    }
    return grid + i;
}

/* the low height of corner (x, z << 8) into MAC n and high - low into IR n */
#define CORNER_HEIGHTS(x, z, mac, irn)                                         \
    do {                                                                       \
        u32 k_ = ((((z) & 0x1F00) >> 3) | ((x) & 0x1F)) << 2;                  \
        s32 lo_ = *(s32 *)((u8 *)RS_W(rs, 0xA4) + k_) >> 24;                   \
        s32 hi_ = *(s32 *)((u8 *)RS_W(rs, 0xA8) + k_) >> 24;                   \
        GTE_WD(mac, lo_ << 3);                                                 \
        GTE_WD(irn, hi_ - lo_);                                                \
    } while (0)

/* 0x80044F74: the heights of three corners of cells (blend by IR0 of low
 * and high, times the cell type's scale) */
static void CornerHeights(void *rs, u32 x0, u32 z0, u32 x1, u32 z1, u32 x2, u32 z2, s32 *h) {
    u8 *types = (u8 *)RS_W(rs, 0xAC);
    u32 s0, s1, s2;
    s32 a, b, c;

    CORNER_HEIGHTS(x0, z0, 25, 9);
    s0 = *GridCell(types, x0, z0);
    CORNER_HEIGHTS(x1, z1, 26, 10);
    s1 = *GridCell(types, x1, z1);
    CORNER_HEIGHTS(x2, z2, 27, 11);
    GTE_CMD(0x1A0003E); /* GPL, no shift: MAC += IR * IR0 */
    s2 = *GridCell(types, x2, z2);
    GTE_RD(9, a);
    GTE_RD(10, b);
    GTE_RD(11, c);
    h[0] = a * (s32)((s0 & 0x1F) << 4);
    h[1] = b * (s32)((s1 & 0x1F) << 4);
    h[2] = c * (s32)((s2 & 0x1F) << 4);
}

/* The height at (x, z) (of one of the two triangles of its cell) and, when
 * out is set, the normalized slope (-dx, 1, -dz) in 0x1000 units. */
static s32 Height(s32 *out, s32 x, s32 z, void *rs) {
    u32 fx, fz, cx, cz;
    s32 h[3];
    s32 dx, dz, res, n;

    GTE_WD(8, RS_W(rs, 0xA0)); /* IR0 */
    x >>= 8;
    z >>= 8;
    fx = x & 0x3FF;
    fz = z & 0x3FF;
    cx = (u32)x >> 10;
    cz = ((u32)z >> 10) << 8;
    if ((s32)(fx + fz) - 0x400 < 0) {
        CornerHeights(rs, cx, cz, cx + 1, cz, cx, cz + 0x100, h);
        dx = h[1] - h[0];
        dz = h[2] - h[0];
        res = h[0] + ((dx * (s32)fx) >> 10) + ((dz * (s32)fz) >> 10);
    } else {
        CornerHeights(rs, cx + 1, cz + 0x100, cx, cz + 0x100, cx + 1, cz, h);
        dx = h[1] - h[0];
        dz = h[2] - h[0];
        res = h[0] + ((dx * (s32)(0x400 - fx)) >> 10) + ((dz * (s32)(0x400 - fz)) >> 10);
        dx = h[0] - h[1];
        dz = h[0] - h[2];
    }
    if (out != NULL) {
        dx = -dx >> 5;
        dz = -dz >> 5;
        n = func_8003D9DC(dx * dx + 0x1000000 + dz * dz);
        out[0] = (dx << 12) / n;
        out[1] = 0x1000000 / n;
        out[2] = (dz << 12) / n;
    }
    return res;
}

s32 func_800450D4(s32 *out, s32 *pos, void *rs) {
    return Height(out, pos[0], pos[2], rs);
}

/* whether a move from a to b (each relative to an edge) crosses it, b
 * ending on the side given by the sign wanted (the original's xor test) */
#define CROSSES(a, b) ((((a) ^ (b)) < 0))

/* Moves obj by delta over the height field: the walls of the cells (bits
 * of the 0xB0 grid: 0x80 low z, 0x20 low x, 0x08 high z, 0x02 high x, and
 * the diagonals 0x40, 0x10, 0x04, 0x01) push it back and change its speed
 * (0x84, 0x8C), then it stands at the height of the ground; out gets the
 * slope (see Height). */
s32 func_80044CA0(s32 *out, void *obj, void *rs, s32 *delta) {
    u8 *walls = (u8 *)RS_W(rs, 0xB0);
    s32 x0 = OBJ_S(obj, 0x60), y0 = OBJ_S(obj, 0x64), z0 = OBJ_S(obj, 0x68);
    s32 x = x0 + delta[0];
    s32 y = y0 + delta[1];
    s32 z = z0 + delta[2];
    const s32 c = 0x40000;
    s32 cx, cz, a, b, t;
    u32 w;

    (void)y0;
    for (;;) {
        w = *GridCell(walls, (u32)(x >> 18), (u32)((z >> 18) << 8));
        cx = x & 0xFFFC0000;
        cz = z & 0xFFFC0000;
        if (w == 0) {
            break;
        }
        if (w & 0x80) {
            a = z0 - cz;
            b = z - cz;
            if (CROSSES(a, b) && b >= 0) {
                z = cz - 0x100;
                OBJ_S(obj, 0x8C) = 0;
                continue;
            }
        }
        if (w & 0x40) {
            a = (x0 - cx) + (z0 - cz) - c;
            b = (z - cz) + (x - cx) - c;
            if (CROSSES(a, b) && b >= 0) {
                t = (b >> 1) + 0x100;
                x -= t;
                z -= t;
                goto slide;
            }
        }
        if (w & 0x20) {
            a = x0 - cx;
            b = x - cx;
            if (CROSSES(a, b) && b >= 0) {
                x = cx - 0x100;
                OBJ_S(obj, 0x84) = 0;
                continue;
            }
        }
        cz += c;
        if (w & 0x10) {
            a = (x0 - cx) + (cz - z0) - c;
            b = (cz - z) + (x - cx) - c;
            if (CROSSES(a, b) && b >= 0) {
                t = (b >> 1) + 0x100;
                x -= t;
                z += t;
                goto slide;
            }
        }
        if (w & 0x08) {
            a = z0 - cz;
            b = z - cz;
            if (CROSSES(a, b) && b < 0) {
                z = cz + 0x100;
                OBJ_S(obj, 0x8C) = 0;
                continue;
            }
        }
        cx += c;
        if (w & 0x04) {
            a = (cx - x0) + (cz - z0) - c;
            b = (cz - z) + (cx - x) - c;
            if (CROSSES(a, b) && b >= 0) {
                t = (b >> 1) + 0x100;
                x += t;
                z += t;
                goto slide;
            }
        }
        if (w & 0x02) {
            a = x0 - cx;
            b = x - cx;
            if (CROSSES(a, b) && b < 0) {
                x = cx + 0x100;
                OBJ_S(obj, 0x84) = 0;
                continue;
            }
        }
        cz -= c;
        if (w & 0x01) {
            a = (cx - x0) + (z0 - cz) - c;
            b = (z - cz) + (cx - x) - c;
            if (CROSSES(a, b) && b >= 0) {
                t = (b >> 1) + 0x100;
                x += t;
                z -= t;
                goto slide;
            }
        }
        break;
    slide:
        OBJ_S(obj, 0x84) = 30 * (x - x0);
        OBJ_S(obj, 0x8C) = 30 * (z - z0);
    }
    OBJ_S(obj, 0x60) = x;
    OBJ_S(obj, 0x64) = y;
    OBJ_S(obj, 0x68) = z;
    return Height(out, x, z, rs);
}
