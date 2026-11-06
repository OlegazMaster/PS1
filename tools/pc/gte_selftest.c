/* Sanity checks of the software GTE (src/pc/gte.c) on the host:
 *   cc -Iinclude -o /tmp/gte_selftest tools/pc/gte_selftest.c src/pc/gte.c && /tmp/gte_selftest */
#include <stdio.h>
#include <stdlib.h>
#include "gte_soft.h"

static int fails;

static void expect(const char *what, long got, long want) {
    if (got != want) {
        printf("FAIL %s: got %ld, want %ld\n", what, got, want);
        fails++;
    }
}

static void identity(void) {
    int i;
    for (i = 0; i < 32; i++) {
        gte_write_ctrl(i, 0);
    }
    gte_write_ctrl(0, 0x1000);     /* RT11 */
    gte_write_ctrl(2, 0x1000);     /* RT22 */
    gte_write_ctrl(4, 0x1000);     /* RT33 */
}

int main(void) {
    long h, z, worst = 0;
    int16_t sx, sy;

    /* RTPS through an identity matrix: x * H / z */
    identity();
    gte_write_ctrl(7, 1000);  /* TRZ */
    gte_write_ctrl(26, 1000); /* H */
    gte_write_ctrl(24, 160 << 16);
    gte_write_ctrl(25, 120 << 16);
    gte_write_data(0, (100 & 0xFFFF) | ((uint32_t)(-50 & 0xFFFF) << 16));
    gte_write_data(1, 0);
    gte_command(GTE_RTPS);
    sx = (int16_t)(gte_read_data(14) & 0xFFFF);
    sy = (int16_t)(gte_read_data(14) >> 16);
    expect("rtps sx", sx, 260);
    expect("rtps sy", sy, 70);
    expect("rtps sz3", gte_read_data(19), 1000);
    expect("rtps flag", gte_read_ctrl(31), 0);

    /* a point behind the screen: divide overflow and the error flag */
    gte_write_ctrl(7, 0);
    gte_command(GTE_RTPS);
    expect("rtps div flag", (gte_read_ctrl(31) >> 17) & 1, 1);
    expect("rtps error bit", gte_read_ctrl(31) >> 31, 1);

    /* NCLIP: a counter-clockwise triangle */
    gte_write_data(12, 0);
    gte_write_data(13, 10);
    gte_write_data(14, (uint32_t)10 << 16);
    gte_command(GTE_NCLIP);
    expect("nclip", (int32_t)gte_read_data(24), 100);

    /* AVSZ3 */
    gte_write_data(17, 100);
    gte_write_data(18, 200);
    gte_write_data(19, 300);
    gte_write_ctrl(29, 0x555); /* about 4096 / 3 */
    gte_command(GTE_AVSZ3);
    expect("avsz3 otz", gte_read_data(7), (0x555 * 600) >> 12);

    /* the divider against the exact quotient */
    for (z = 1; z < 0x10000; z += 7) {
        for (h = 0; h < 0x10000 && h < 2 * z; h += 977) {
            long exact, got, err;
            identity();
            gte_write_ctrl(26, h);
            gte_write_ctrl(7, z);
            gte_write_data(0, 1);
            gte_write_data(1, 0);
            gte_command(GTE_RTPS);
            exact = ((h * 0x20000) / z + 1) / 2;
            if (exact > 0x1FFFF) {
                exact = 0x1FFFF;
            }
            /* the quotient n is read back through MAC0 = n * DQA + DQB */
            gte_write_ctrl(27, 1);
            gte_write_ctrl(28, 0);
            gte_command(GTE_RTPS);
            got = (int32_t)gte_read_data(24);
            err = labs(got - exact);
            if (err > worst) {
                worst = err;
            }
        }
    }
    printf("divider: largest difference from the exact quotient %ld\n", worst);
    /* the hardware divider is an approximation (a table and two
     * Newton-Raphson steps): it is off by up to 3 near the largest
     * quotients, in about 1 case in 30000 */
    if (worst > 3) {
        fails++;
    }
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
