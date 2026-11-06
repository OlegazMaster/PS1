#include "common.h"

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
