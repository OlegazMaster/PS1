#include "common.h"
#include "gool.h"
#include "game.h"
#include "ns.h"

/* Geometry (boxes, quads, angles, distances) and random numbers. */

/* .sdata */
s32 D_800609D4 = 1; /* random seeds */
s32 D_800609D8 = 1;

s32 func_8003C2F0(s32 angle); /* sin */
s32 func_8003C364(s32 angle); /* cos */
/* The vector of length len at angles pitch and yaw. */

void func_80030A34(s32 *out, s32 len, s32 pitch, s32 yaw) {
    s32 t;

    out[1] = ((len >> 8) * func_8003C2F0(pitch)) >> 4;
    t = (len >> 8) * func_8003C364(pitch);
    t >>= 12;
    out[0] = (t * func_8003C2F0(yaw)) >> 4;
    out[2] = (t * func_8003C364(yaw)) >> 4;
}

INCLUDE_ASM("asm/us/nonmatchings/math", func_80030AD8);

/* The height (y) at (x, z) of the quad (4 points x, y, z): found on the
 * line between the two edges that cross z. -999999999 if there are not two. */

s32 func_80030F88(s32 x, s32 z, s32 *quad) {
    s32 found;
    s32 i;
    s32 *p;
    s32 *q;
    s32 *p1;
    s32 *q1;
    s32 d0;
    s32 d1;
    s32 e0;
    s32 e1;
    s32 f;
    s32 xa;
    s32 ya;
    s32 xb;
    s32 yb;
    s32 n;

    found = 0;
    p = quad;
    for (i = 0; i < 4; i++, p = q) {
        q = &quad[((i + 1) & 3) * 3];
        d1 = q[2] - z;
        d0 = z - p[2];
        if ((d0 <= 0 && d1 <= 0) || (d0 >= 0 && d1 >= 0)) {
            if (!found) {
                found = 1;
                e0 = d0;
                e1 = d1;
                p1 = p;
                q1 = q;
                continue;
            }
            n = e0 + e1;
            if (n == 0) {
                n = 1;
            }
            f = (e0 << 12) / n;
            xa = ((f * (q1[0] - p1[0])) >> 12) + p1[0];
            ya = ((f * (q1[1] - p1[1])) >> 12) + p1[1];
            n = d0 + d1;
            if (n == 0) {
                n = 1;
            }
            f = (d0 << 12) / n;
            xb = ((f * (q[0] - p[0])) >> 12) + p[0];
            yb = ((f * (q[1] - p[1])) >> 12) + p[1];
            n = xb - xa;
            if (n == 0) {
                n = 1;
            }
            f = ((x - xa) << 12) / n;
            return ((f * (yb - ya)) >> 12) + ya;
        }
    }
    return -999999999;
}

INCLUDE_ASM("asm/us/nonmatchings/math", func_8003116C);

INCLUDE_ASM("asm/us/nonmatchings/math", func_8003148C);

/* Whether p is inside the box b (position and size, in 1/256 units). */

s32 func_800317C0(s32 *b, s32 *p) {
    if (p[0] >= (b[0] << 8) && p[1] >= (b[1] << 8) && p[2] >= (b[2] << 8) && p[0] < ((b[0] + b[3]) << 8)
        && p[1] < ((b[1] + b[4]) << 8) && p[2] < ((b[2] + b[5]) << 8)) {
        return 1;
    }
    return 0;
}


/* Whether pos is in zone. */

s32 func_80031868(Entry *zone, s32 *pos) {
    return func_800317C0((s32 *)zone->items[1], pos);
}

/* Whether the point p is inside the box b (x0, y0, z0, x1, y1, z1). */

s32 func_8003188C(s32 *p, s32 *b) {
    if (p[0] >= b[0] && p[1] >= b[1] && p[2] >= b[2] && p[0] < b[3] && p[1] < b[4] && p[2] < b[5]) {
        return 1;
    }
    return 0;
}

/* Whether the boxes a and b overlap. */

s32 func_8003191C(s32 *a, s32 *b) {
    if (a[1] <= b[4] && a[4] > b[1] && a[3] > b[0] && a[5] > b[2] && a[0] <= b[3] && a[2] <= b[5]) {
        return 1;
    }
    return 0;
}

/* Whether the box a is inside the box b. */

s32 func_800319B8(s32 *a, s32 *b) {
    if (a[0] >= b[0] && a[1] >= b[1] && a[2] >= b[2] && a[3] < b[3] && a[4] < b[4] && a[5] < b[5]) {
        return 1;
    }
    return 0;
}

#define ABS_DIFF(a, b) ((a) - (b) >= 0 ? (a) - (b) : (b) - (a))
/* Approximate distance between a and b: the largest difference plus a
 * quarter of the others. */

s32 func_80031A54(s32 *a, s32 *b) {
    s32 d[3];

    d[0] = ABS_DIFF(a[0], b[0]);
    d[1] = ABS_DIFF(a[1], b[1]);
    d[2] = ABS_DIFF(a[2], b[2]);
    if (d[0] >= d[1] && d[0] >= d[2]) {
        return d[0] + (d[1] + d[2]) / 4;
    }
    if (d[1] >= d[0] && d[1] >= d[2]) {
        return d[1] + (d[0] + d[2]) / 4;
    }
    return d[2] + (d[1] + d[0]) / 4;
}

/* The same in x, z. */

s32 func_80031B50(s32 *a, s32 *b) {
    s32 d[3];

    d[0] = ABS_DIFF(a[0], b[0]);
    d[2] = ABS_DIFF(a[2], b[2]);
    if (d[0] >= d[2]) {
        return d[0] + d[2] / 4;
    }
    return d[2] + d[0] / 4;
}

/* Whether a is farther than (mx, my, mz) from b in any axis; if so, obj (if
 * any) gets status flag 0x8000. */

s32 func_80031BD0(GoolObject *obj, s32 *a, s32 *b, s32 mx, s32 my, s32 mz) {
    s32 d;
    s32 nx = -mx;
    s32 ny = -my;
    s32 nz = -mz;

    if ((d = a[0] - b[0], d > mx || d < nx) || (d = a[1] - b[1], d > my || d < ny)
        || (d = a[2] - b[2], d > mz || d < nz)) {
        if (obj != NULL) {
            obj->status_a |= 0x8000;
        }
        return 1;
    }
    return 0;
}

/* Distance between a and b. */

s32 func_80031C74(s32 *a, s32 *b) {
    s32 d[3];

    d[0] = (a[0] - b[0]) >> 8;
    d[1] = (a[1] - b[1]) >> 8;
    d[2] = (a[2] - b[2]) >> 8;
    return func_8003D9DC(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) << 8;
}

s32 func_8003D9DC(s32 x); /* square root */
/* Distance (in x, z) between a and b. */

s32 func_80031CF0(s32 *a, s32 *b) {
    s32 d[3];

    d[0] = (a[0] - b[0]) >> 8;
    d[2] = (a[2] - b[2]) >> 8;
    return func_8003D9DC(d[0] * d[0] + d[2] * d[2]) << 8;
}

/* The same, coarser (for far points). */

s32 func_80031D4C(s32 *a, s32 *b) {
    s32 d[3];

    d[0] = (a[0] - b[0]) >> 16;
    d[2] = (a[2] - b[2]) >> 16;
    return func_8003D9DC(d[0] * d[0] + d[2] * d[2]) << 16;
}

s32 func_8004F794(s32 x, s32 z); /* angle of (x, z) */
/* Angle (in x, z) from a to b. */

s32 func_80031DA8(s32 *a, s32 *b) {
    return func_8004F794(b[0] - a[0], b[2] - a[2]) & 0xFFF;
}

/* Angle (in x, y) from a to b. */

s32 func_80031DDC(s32 *a, s32 *b) {
    return func_8004F794(b[0] - a[0], b[1] - a[1]) & 0xFFF;
}

/* Angle a turned towards b by t / 256 of the way. */

s32 func_80031E10(s32 a, s32 b, s32 t) {
    return (a + ((t * (((b - a) << 20) >> 20)) >> 8)) & 0xFFF;
}

/* The same, turning by at most max. */

s32 func_80031E34(s32 a, s32 b, s32 t, s32 max) {
    s32 d = ((b - a) << 20) >> 20;

    if (d > 0 && d > max) {
        d = max;
    } else if (d < 0 && d < -max) {
        d = -max;
    }
    return (a + ((t * d) >> 8)) & 0xFFF;
}

#ifdef NON_MATCHING
/* The yaw (out[1]) and pitch (out[0]) of the direction -v; pitch -0x400
 * when it points more than a quarter turn away from yaw.
 * Equivalent; the original keeps the first product in another register. */
void func_80031E84(s32 *out, s32 *v, s32 yaw) {
    s32 x;
    s32 z;
    s32 a;
    s32 d;
    s32 c;

    x = -v[0];
    z = -v[2];
    a = func_8004F794(x, z) & 0xFFF;
    yaw &= 0xFFF;
    d = a - yaw;
    if (d > 0x800) {
        d -= 0x1000;
    } else if (d < -0x800) {
        d += 0x1000;
    }
    out[1] = a;
    if ((u32)(d + 0x400) <= 0x800) {
        a = -a;
        c = func_8003C364(a);
        yaw = (((z >> 8) * c) >> 4) - (((x >> 8) * func_8003C2F0(a)) >> 4);
        func_8003C2F0(a);
        func_8003C364(a);
        out[0] = func_8004F794(v[1], yaw) & 0xFFF;
    } else {
        out[0] = -0x400;
    }
    out[2] = 0;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/math", func_80031E84);
#endif

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
