/* A software GTE (see include/gte_soft.h). Fixed-point behaviour as on the
 * hardware: products are summed in 44 bits (overflow flags at each step),
 * results shifted by 12 when sf is set, IR saturated to -0x8000..0x7FFF
 * (0..0x7FFF with lm), screen coordinates to -0x400..0x3FF, depths to
 * 0..0xFFFF, colors to 0..0xFF, and the perspective division done with the
 * hardware's table-driven Newton-Raphson divider. */

#include "gte_soft.h"

GteState gte;

/* ---- flags ------------------------------------------------------------- */

#define F_MAC_POS(i) (1u << (31 - (i)))  /* MAC1..3 positive overflow: 30..28 */
#define F_MAC_NEG(i) (1u << (28 - (i)))  /* MAC1..3 negative overflow: 27..25 */
#define F_IR(i) (1u << (25 - (i)))       /* IR1..3 saturated: 24..22 */
#define F_COLOR(i) (1u << (22 - (i)))    /* R, G, B saturated: 21..19 */
#define F_SZ_OTZ (1u << 18)
#define F_DIV (1u << 17)
#define F_MAC0_POS (1u << 16)
#define F_MAC0_NEG (1u << 15)
#define F_SX (1u << 14)
#define F_SY (1u << 13)
#define F_IR0 (1u << 12)

static uint32_t flag;

/* ---- register access helpers ------------------------------------------- */

static int16_t lo16(uint32_t v) {
    return (int16_t)(v & 0xFFFF);
}

static int16_t hi16(uint32_t v) {
    return (int16_t)(v >> 16);
}

/* element r, c (0..2) of the matrix at control register base (0 RT, 8 LLM,
 * 16 LCM) */
static int32_t mat(int base, int r, int c) {
    int i = r * 3 + c;
    uint32_t w = gte.ctrl[base + i / 2];

    return (i & 1) ? hi16(w) : lo16(w);
}

static int32_t vec(int n, int i) { /* V0..V2 component i */
    if (i == 0) {
        return lo16(gte.data[n * 2]);
    }
    if (i == 1) {
        return hi16(gte.data[n * 2]);
    }
    return lo16(gte.data[n * 2 + 1]);
}

static int32_t ir(int i) { /* IR0..IR3 */
    return lo16(gte.data[8 + i]);
}

static void set_ir(int i, int32_t v) {
    gte.data[8 + i] = (uint32_t)(int32_t)(int16_t)v;
}

static int32_t mac(int i) {
    return (int32_t)gte.data[24 + i];
}

static void set_mac(int i, int64_t v) {
    gte.data[24 + i] = (uint32_t)(int32_t)v;
}

/* ---- saturation and overflow ------------------------------------------- */

/* checks a 44-bit stage of MAC1..3 and wraps it to 44 bits */
static int64_t chk44(int i, int64_t v) {
    if (v > 0x7FFFFFFFFFFLL) {
        flag |= F_MAC_POS(i);
    } else if (v < -0x80000000000LL) {
        flag |= F_MAC_NEG(i);
    }
    return (int64_t)((uint64_t)v << 20) >> 20;
}

static int64_t chk_mac0(int64_t v) {
    if (v > 0x7FFFFFFFLL) {
        flag |= F_MAC0_POS;
    } else if (v < -0x80000000LL) {
        flag |= F_MAC0_NEG;
    }
    return v;
}

static int32_t sat_ir(int i, int64_t v, int lm) {
    int32_t lo = lm ? 0 : -0x8000;

    if (v < lo) {
        flag |= F_IR(i);
        return lo;
    }
    if (v > 0x7FFF) {
        flag |= F_IR(i);
        return 0x7FFF;
    }
    return (int32_t)v;
}

static int32_t clampf(int64_t v, int32_t lo, int32_t hi, uint32_t f) {
    if (v < lo) {
        flag |= f;
        return lo;
    }
    if (v > hi) {
        flag |= f;
        return hi;
    }
    return (int32_t)v;
}

/* ---- FIFOs --------------------------------------------------------------- */

static void push_sz(int64_t z) {
    gte.data[16] = gte.data[17];
    gte.data[17] = gte.data[18];
    gte.data[18] = gte.data[19];
    gte.data[19] = (uint32_t)clampf(z, 0, 0xFFFF, F_SZ_OTZ);
}

static void push_sxy(int32_t x, int32_t y) {
    gte.data[12] = gte.data[13];
    gte.data[13] = gte.data[14];
    gte.data[14] = ((uint32_t)x & 0xFFFF) | ((uint32_t)y << 16);
}

static void push_color(void) {
    uint32_t r = (uint32_t)clampf(mac(1) >> 4, 0, 0xFF, F_COLOR(1));
    uint32_t g = (uint32_t)clampf(mac(2) >> 4, 0, 0xFF, F_COLOR(2));
    uint32_t b = (uint32_t)clampf(mac(3) >> 4, 0, 0xFF, F_COLOR(3));

    gte.data[20] = gte.data[21];
    gte.data[21] = gte.data[22];
    gte.data[22] = r | (g << 8) | (b << 16) | (gte.data[6] & 0xFF000000);
}

/* ---- the divider ----------------------------------------------------- */

static uint8_t unr_table[0x101];
static int unr_ready;

static void unr_init(void) {
    int i;

    for (i = 0; i < 0x101; i++) {
        int v = (0x40000 / (i + 0x100) + 1) / 2 - 0x101;
        unr_table[i] = (uint8_t)(v > 0 ? v : 0);
    }
    unr_ready = 1;
}

/* (H * 0x20000 / sz + 1) / 2 as the hardware computes it */
static uint32_t divide(uint32_t h, uint32_t sz) {
    int z;
    uint64_t n;
    uint64_t d;
    uint64_t u;

    if (!(h < sz * 2)) {
        flag |= F_DIV;
        return 0x1FFFF;
    }
    if (!unr_ready) {
        unr_init();
    }
    z = 0;
    while (z < 16 && !(sz & (0x8000 >> z))) {
        z++;
    }
    n = (uint64_t)h << z;
    d = (uint64_t)sz << z;
    u = unr_table[(d - 0x7FC0) >> 7] + 0x101;
    d = (0x2000080 - d * u) >> 8;
    d = (0x0000080 + d * u) >> 8;
    n = (n * d + 0x8000) >> 16;
    return n > 0x1FFFF ? 0x1FFFF : (uint32_t)n;
}

/* ---- commands --------------------------------------------------------- */

static void rtp(int v, int sf, int lm, int last) {
    int64_t acc[4];
    int i;
    int shift = sf ? 12 : 0;
    uint32_t n;
    int64_t m0;
    int64_t z;

    for (i = 1; i <= 3; i++) {
        acc[i] = chk44(i, (int64_t)(int32_t)gte.ctrl[4 + i] << 12);
        acc[i] = chk44(i, acc[i] + (int64_t)mat(0, i - 1, 0) * vec(v, 0));
        acc[i] = chk44(i, acc[i] + (int64_t)mat(0, i - 1, 1) * vec(v, 1));
        acc[i] = chk44(i, acc[i] + (int64_t)mat(0, i - 1, 2) * vec(v, 2));
        set_mac(i, acc[i] >> shift);
    }
    set_ir(1, sat_ir(1, mac(1), lm));
    set_ir(2, sat_ir(2, mac(2), lm));
    /* IR3: saturated from MAC3, but its flag comes from MAC3 >> 12 */
    z = acc[3] >> 12;
    if (z < -0x8000 || z > 0x7FFF) {
        flag |= F_IR(3);
    }
    {
        int64_t m3 = mac(3);
        int32_t lo = lm ? 0 : -0x8000;
        set_ir(3, m3 < lo ? lo : m3 > 0x7FFF ? 0x7FFF : (int32_t)m3);
    }
    push_sz(z);
    n = divide(gte.ctrl[26] & 0xFFFF, gte.data[19]);
    m0 = chk_mac0((int64_t)n * ir(1) + (int32_t)gte.ctrl[24]);
    set_mac(0, m0);
    {
        int32_t x = clampf(m0 >> 16, -0x400, 0x3FF, F_SX);
        m0 = chk_mac0((int64_t)n * ir(2) + (int32_t)gte.ctrl[25]);
        set_mac(0, m0);
        push_sxy(x, clampf(m0 >> 16, -0x400, 0x3FF, F_SY));
    }
    if (last) {
        m0 = chk_mac0((int64_t)n * lo16(gte.ctrl[27]) + (int32_t)gte.ctrl[28]);
        set_mac(0, m0);
        set_ir(0, clampf(m0 >> 12, 0, 0x1000, F_IR0));
    }
}

static void nclip(void) {
    int32_t x0 = lo16(gte.data[12]), y0 = hi16(gte.data[12]);
    int32_t x1 = lo16(gte.data[13]), y1 = hi16(gte.data[13]);
    int32_t x2 = lo16(gte.data[14]), y2 = hi16(gte.data[14]);
    int64_t v = (int64_t)x0 * y1 + (int64_t)x1 * y2 + (int64_t)x2 * y0 - (int64_t)x0 * y2 -
                (int64_t)x1 * y0 - (int64_t)x2 * y1;

    set_mac(0, chk_mac0(v));
}

static void avsz(int four) {
    int64_t sum = (int64_t)(gte.data[17] & 0xFFFF) + (gte.data[18] & 0xFFFF) + (gte.data[19] & 0xFFFF);
    int64_t v;

    if (four) {
        sum += gte.data[16] & 0xFFFF;
        v = (int64_t)lo16(gte.ctrl[30]) * sum;
    } else {
        v = (int64_t)lo16(gte.ctrl[29]) * sum;
    }
    v = chk_mac0(v);
    set_mac(0, v);
    gte.data[7] = (uint32_t)clampf(v >> 12, 0, 0xFFFF, F_SZ_OTZ);
}

/* the depth cue of [MAC1..3] (already set) toward the far color */
static void depth_cue(int64_t m1, int64_t m2, int64_t m3, int sf, int lm) {
    int64_t m[4];
    int32_t t[4];
    int i;
    int shift = sf ? 12 : 0;

    m[1] = m1;
    m[2] = m2;
    m[3] = m3;
    for (i = 1; i <= 3; i++) {
        int64_t v = chk44(i, ((int64_t)(int32_t)gte.ctrl[20 + i] << 12) - m[i]);
        t[i] = sat_ir(i, v >> shift, 0);
    }
    for (i = 1; i <= 3; i++) {
        int64_t v = chk44(i, (int64_t)t[i] * ir(0) + m[i]);
        set_mac(i, v >> shift);
    }
    push_color();
    for (i = 1; i <= 3; i++) {
        set_ir(i, sat_ir(i, mac(i), lm));
    }
}

static void dpcs(uint32_t rgb, int sf, int lm) {
    depth_cue((int64_t)(rgb & 0xFF) << 16, (int64_t)((rgb >> 8) & 0xFF) << 16,
              (int64_t)((rgb >> 16) & 0xFF) << 16, sf, lm);
}

static void gpl(int sf, int lm) {
    int i;
    int shift = sf ? 12 : 0;

    for (i = 1; i <= 3; i++) {
        int64_t v = chk44(i, (int64_t)ir(i) * ir(0) + ((int64_t)mac(i) << shift));
        set_mac(i, v >> shift);
    }
    push_color();
    for (i = 1; i <= 3; i++) {
        set_ir(i, sat_ir(i, mac(i), lm));
    }
}

static void gpf(int sf, int lm) {
    int i;
    int shift = sf ? 12 : 0;

    for (i = 1; i <= 3; i++) {
        set_mac(i, chk44(i, (int64_t)ir(i) * ir(0)) >> shift);
    }
    push_color();
    for (i = 1; i <= 3; i++) {
        set_ir(i, sat_ir(i, mac(i), lm));
    }
}

static void sqr(int sf, int lm) {
    int i;
    int shift = sf ? 12 : 0;

    for (i = 1; i <= 3; i++) {
        set_mac(i, chk44(i, (int64_t)ir(i) * ir(i)) >> shift);
        set_ir(i, sat_ir(i, mac(i), lm));
    }
}

static void op(int sf, int lm) {
    int shift = sf ? 12 : 0;
    int64_t d1 = mat(0, 0, 0), d2 = mat(0, 1, 1), d3 = mat(0, 2, 2);
    int64_t i1 = ir(1), i2 = ir(2), i3 = ir(3);

    set_mac(1, chk44(1, i3 * d2 - i2 * d3) >> shift);
    set_mac(2, chk44(2, i1 * d3 - i3 * d1) >> shift);
    set_mac(3, chk44(3, i2 * d1 - i1 * d2) >> shift);
    set_ir(1, sat_ir(1, mac(1), lm));
    set_ir(2, sat_ir(2, mac(2), lm));
    set_ir(3, sat_ir(3, mac(3), lm));
}

static void mvmva(uint32_t cmd, int sf, int lm) {
    int mx = (cmd >> 17) & 3;
    int v = (cmd >> 15) & 3;
    int cv = (cmd >> 13) & 3;
    int shift = sf ? 12 : 0;
    int32_t m[3][3];
    int32_t x[3];
    int i;
    int j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            m[i][j] = mx == 3 ? 0 : mat(mx * 8, i, j);
        }
    }
    if (mx == 3) { /* the "garbage" matrix */
        int32_t r = (int32_t)(gte.data[6] & 0xFF) << 4;
        m[0][0] = -r;
        m[0][1] = r;
        m[0][2] = ir(0);
        m[1][0] = m[1][1] = m[1][2] = mat(0, 0, 2);
        m[2][0] = m[2][1] = m[2][2] = mat(0, 1, 1);
    }
    for (i = 0; i < 3; i++) {
        x[i] = v == 3 ? ir(i + 1) : vec(v, i);
    }
    for (i = 1; i <= 3; i++) {
        int64_t acc;
        int64_t t = cv == 3 ? 0 : (int64_t)(int32_t)gte.ctrl[5 + cv * 8 + (i - 1)] << 12;

        if (cv == 2) {
            /* the FC bug: the first column only sets flags */
            int64_t f = chk44(i, t + (int64_t)m[i - 1][0] * x[0]);
            sat_ir(i, f >> shift, 0);
            acc = 0;
        } else {
            acc = chk44(i, t);
            acc = chk44(i, acc + (int64_t)m[i - 1][0] * x[0]);
        }
        acc = chk44(i, acc + (int64_t)m[i - 1][1] * x[1]);
        acc = chk44(i, acc + (int64_t)m[i - 1][2] * x[2]);
        set_mac(i, acc >> shift);
    }
    for (i = 1; i <= 3; i++) {
        set_ir(i, sat_ir(i, mac(i), lm));
    }
}

void gte_command(uint32_t cmd) {
    int sf = (cmd >> 19) & 1;
    int lm = (cmd >> 10) & 1;

    flag = 0;
    switch (cmd & 0x3F) {
    case 0x01:
        rtp(0, sf, lm, 1);
        break;
    case 0x30:
        rtp(0, sf, lm, 0);
        rtp(1, sf, lm, 0);
        rtp(2, sf, lm, 1);
        break;
    case 0x06:
        nclip();
        break;
    case 0x0C:
        op(sf, lm);
        break;
    case 0x10:
        dpcs(gte.data[6], sf, lm);
        break;
    case 0x11: /* INTPL */
        depth_cue((int64_t)ir(1) << 12, (int64_t)ir(2) << 12, (int64_t)ir(3) << 12, sf, lm);
        break;
    case 0x12:
        mvmva(cmd, sf, lm);
        break;
    case 0x28:
        sqr(sf, lm);
        break;
    case 0x2A: /* DPCT: three times with the front of the color FIFO */
        dpcs(gte.data[20], sf, lm);
        dpcs(gte.data[20], sf, lm);
        dpcs(gte.data[20], sf, lm);
        break;
    case 0x2D:
        avsz(0);
        break;
    case 0x2E:
        avsz(1);
        break;
    case 0x3D:
        gpf(sf, lm);
        break;
    case 0x3E:
        gpl(sf, lm);
        break;
    default:
        /* lighting commands (NCS, NCT, NCDS, CC, ...): not used by the game
         * code outside the PSY-Q libraries */
        break;
    }
    if (flag & 0x7F87E000) {
        flag |= 0x80000000;
    }
    gte.ctrl[31] = flag;
}

/* ---- mtc2 / mfc2 / ctc2 / cfc2 ------------------------------------------- */

static uint32_t sext16(uint32_t v) {
    return (uint32_t)(int32_t)(int16_t)(v & 0xFFFF);
}

static uint32_t orgb(void) {
    uint32_t c = 0;
    int i;

    for (i = 1; i <= 3; i++) {
        int32_t v = ir(i) >> 7;
        c |= (uint32_t)(v < 0 ? 0 : v > 0x1F ? 0x1F : v) << ((i - 1) * 5);
    }
    return c;
}

void gte_write_data(int reg, uint32_t value) {
    switch (reg) {
    case 1:
    case 3:
    case 5:
    case 8:
    case 9:
    case 10:
    case 11:
        gte.data[reg] = sext16(value);
        break;
    case 7:
    case 16:
    case 17:
    case 18:
    case 19:
        gte.data[reg] = value & 0xFFFF;
        break;
    case 15: /* SXYP pushes the screen FIFO */
        gte.data[12] = gte.data[13];
        gte.data[13] = gte.data[14];
        gte.data[14] = value;
        break;
    case 28: /* IRGB sets IR1..3 */
        gte.data[9] = (value & 0x1F) << 7;
        gte.data[10] = ((value >> 5) & 0x1F) << 7;
        gte.data[11] = ((value >> 10) & 0x1F) << 7;
        break;
    case 29:
    case 31:
        break;
    case 30: {
        uint32_t v = (value & 0x80000000) ? ~value : value;
        uint32_t n = 0;

        while (n < 32 && !(v & (0x80000000u >> n))) {
            n++;
        }
        gte.data[30] = value;
        gte.data[31] = n;
        break;
    }
    default:
        gte.data[reg] = value;
        break;
    }
}

uint32_t gte_read_data(int reg) {
    switch (reg) {
    case 1:
    case 3:
    case 5:
    case 8:
    case 9:
    case 10:
    case 11:
        return sext16(gte.data[reg]);
    case 7:
    case 16:
    case 17:
    case 18:
    case 19:
        return gte.data[reg] & 0xFFFF;
    case 15:
        return gte.data[14];
    case 28:
    case 29:
        return orgb();
    default:
        return gte.data[reg];
    }
}

void gte_write_ctrl(int reg, uint32_t value) {
    switch (reg) {
    case 4:
    case 12:
    case 20:
    case 26:
    case 27:
    case 29:
    case 30:
        gte.ctrl[reg] = sext16(value);
        break;
    case 31:
        gte.ctrl[31] = value & 0x7FFFF000;
        if (value & 0x7F87E000) {
            gte.ctrl[31] |= 0x80000000;
        }
        break;
    default:
        gte.ctrl[reg] = value;
        break;
    }
}

uint32_t gte_read_ctrl(int reg) {
    return gte.ctrl[reg];
}
