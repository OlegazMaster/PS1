#include "common.h"
#include "gool.h"
#include "game.h"

/* Geometry (boxes, quads, angles, distances) and random numbers. */

/* .sdata */
s32 D_800609D4 = 1; /* random seeds */
s32 D_800609D8 = 1;

INCLUDE_ASM("asm/us/nonmatchings/math", func_80030A34);

INCLUDE_ASM("asm/us/nonmatchings/math", func_80030AD8);

INCLUDE_ASM("asm/us/nonmatchings/math", func_80030F88);

INCLUDE_ASM("asm/us/nonmatchings/math", func_8003116C);

INCLUDE_ASM("asm/us/nonmatchings/math", func_8003148C);

INCLUDE_ASM("asm/us/nonmatchings/math", func_800317C0);

INCLUDE_ASM("asm/us/nonmatchings/math", func_80031868);

INCLUDE_ASM("asm/us/nonmatchings/math", func_8003188C);

INCLUDE_ASM("asm/us/nonmatchings/math", func_8003191C);

INCLUDE_ASM("asm/us/nonmatchings/math", func_800319B8);

INCLUDE_ASM("asm/us/nonmatchings/math", func_80031A54);

INCLUDE_ASM("asm/us/nonmatchings/math", func_80031B50);

INCLUDE_ASM("asm/us/nonmatchings/math", func_80031BD0);

INCLUDE_ASM("asm/us/nonmatchings/math", func_80031C74);

INCLUDE_ASM("asm/us/nonmatchings/math", func_80031CF0);

INCLUDE_ASM("asm/us/nonmatchings/math", func_80031D4C);

INCLUDE_ASM("asm/us/nonmatchings/math", func_80031DA8);

INCLUDE_ASM("asm/us/nonmatchings/math", func_80031DDC);

INCLUDE_ASM("asm/us/nonmatchings/math", func_80031E10);

INCLUDE_ASM("asm/us/nonmatchings/math", func_80031E34);

INCLUDE_ASM("asm/us/nonmatchings/math", func_80031E84);

/* Angle a turned halfway towards b (12-bit angles). */

s32 func_80031F9C(s32 a, s32 b) {
    return (a + (((b - a) << 20) >> 21)) & 0xFFF;
}

/* Whether the point p (in 1/256 units) with the size p[3..5] overlaps the
 * box b. */

s32 func_80031FB4(s32 *b, s32 *p) {
    if ((p[0] << 8) < b[3] && (p[1] << 8) < b[4] && (p[2] << 8) < b[5] && ((p[0] + p[3]) << 8) >= b[0]
        && ((p[1] + p[4]) << 8) >= b[1] && ((p[2] + p[5]) << 8) >= b[2]) {
        return 1;
    }
    return 0;
}

void func_8003205C(void) {
}

/* A random number in 0..n-1 (0 for n = 0), from seed D_800609D4. */

s32 func_80032064(s32 n) {
    s32 r;

    if (n == 0) {
        return 0;
    }
    D_800609D4 = D_800609D4 * 0x41C64E6D + 12345;
    r = (u32)D_800609D4 / 15 % 0x7FFFFFF;
    return (r < 0 ? -r : r) % n;
}

void func_80032144(void) {
    D_800609D4 = 12345;
}

void func_80032154(s32 seed) {
    D_800609D4 = seed;
}

/* A random number in 0..n-1 (0 for n = 0), from seed D_800609D8. */

s32 func_80032160(s32 n) {
    s32 r;

    if (n == 0) {
        return 0;
    }
    D_800609D8 = D_800609D8 * 0x41C64E6D + 12345;
    r = (u32)D_800609D8 / 15 % 0x7FFFFFF;
    return (r < 0 ? -r : r) % n;
}

void func_80032240(void) {
    D_800609D8 = 12345;
}

/* |a - b| */

s32 func_80032250(s32 a, s32 b) {
    s32 d = a - b;

    if (d < 0) {
        d = b - a;
    }
    return d;
}
