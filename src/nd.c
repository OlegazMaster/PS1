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
