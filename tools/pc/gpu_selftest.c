/* Self-test of the software GPU of the port (src/pc/gpu.c): properties the
 * hardware has, checked pixel by pixel, and a picture of a test scene
 * (build/pc/gpu_test.ppm). build: see tools/pc/gpu_test.sh */
#include "pc_gpu.h"

#include <stdio.h>
#include <string.h>

static int fails;

#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static u32 xy(s32 x, s32 y)
{
    return (u32)(x & 0xFFFF) | (u32)y << 16;
}

static void area(s32 x1, s32 y1, s32 x2, s32 y2, s32 ox, s32 oy)
{
    u32 c[4] = { 0xE3000000 | x1 | y1 << 10, 0xE4000000 | x2 | y2 << 10,
                 0xE5000000 | (ox & 0x7FF) | (oy & 0x7FF) << 11, 0xE6000000 };

    gpu_commands(c, 4);
}

static s32 count(s32 x0, s32 y0, s32 w, s32 h, u16 v)
{
    s32 n = 0, x, y;

    for (y = y0; y < y0 + h; y++) {
        for (x = x0; x < x0 + w; x++) {
            n += gpu_vram[y][x] == v;
        }
    }
    return n;
}

int main(void)
{
    u32 cmd[32];
    s32 x, y, n;

    gpu_reset();
    area(0, 0, 1023, 511, 0, 0);

    /* a flat quad (0,0)-(10,10): exactly 10x10 pixels, the right and
     * bottom edges are not drawn */
    cmd[0] = 0x28FF0000; /* blue */
    cmd[1] = xy(0, 0);
    cmd[2] = xy(10, 0);
    cmd[3] = xy(0, 10);
    cmd[4] = xy(10, 10);
    gpu_commands(cmd, 5);
    n = count(0, 0, 20, 20, 0x7C00);
    CHECK(n == 100, "quad 10x10: %d pixels", n);

    /* additive semi-transparent quad on black: no pixel drawn twice on the
     * shared diagonal (a pixel drawn twice would be brighter) */
    memset(gpu_vram, 0, sizeof(gpu_vram));
    cmd[0] = 0xE1000000 | 1 << 5; /* abr 1: B + F */
    gpu_commands(cmd, 1);
    cmd[0] = 0x2A080808; /* semi, colour 8,8,8 -> 1 */
    cmd[1] = xy(100, 100);
    cmd[2] = xy(137, 103);
    cmd[3] = xy(96, 131);
    cmd[4] = xy(141, 129);
    gpu_commands(cmd, 5);
    n = count(80, 80, 80, 80, 0x0421 * 2);
    CHECK(n == 0, "quad with a shared edge: %d pixels drawn twice", n);
    n = count(80, 80, 80, 80, 0x0421);
    CHECK(n > 1000, "quad: %d pixels", n);

    /* two triangles sharing an edge, any slope: together no gaps and no
     * overlap with a third one around them */
    memset(gpu_vram, 0, sizeof(gpu_vram));
    cmd[0] = 0x22080808;
    cmd[1] = xy(10, 10);
    cmd[2] = xy(50, 23);
    cmd[3] = xy(27, 60);
    gpu_commands(cmd, 4);
    cmd[1] = xy(50, 23);
    cmd[2] = xy(27, 60);
    cmd[3] = xy(70, 70);
    gpu_commands(cmd, 4);
    n = count(0, 0, 100, 100, 0x0842);
    CHECK(n == 0, "triangles sharing an edge: %d pixels drawn twice", n);

    /* the drawing area clips; the offset moves */
    memset(gpu_vram, 0, sizeof(gpu_vram));
    area(200, 200, 209, 209, 200, 200);
    cmd[0] = 0x600000FF;
    cmd[1] = xy(-5, -5);
    cmd[2] = xy(100, 100);
    gpu_commands(cmd, 3);
    n = count(190, 190, 40, 40, 0x001F);
    CHECK(n == 100, "rectangle clipped to a 10x10 area: %d pixels", n);
    area(0, 0, 1023, 511, 0, 0);

    /* a 4-bit textured sprite through a CLUT: texel 0 is transparent, the
     * others take the CLUT colour (raw: not modulated) */
    memset(gpu_vram, 0, sizeof(gpu_vram));
    for (x = 0; x < 16; x++) {
        gpu_vram[480][x] = (u16)(x ? (x | 0x8000 * (x == 3)) : 0); /* CLUT at (0, 480) */
    }
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 4; x++) {
            gpu_vram[y][640 + x] = 0x3210 + (u16)(y & 1); /* texels 0,1,2,3 (or 1,1,2,3) */
        }
    }
    cmd[0] = 0xE1000000 | 10; /* texture page x = 640, 4-bit */
    gpu_commands(cmd, 1);
    cmd[0] = 0x65808080; /* raw textured sprite */
    cmd[1] = xy(300, 300);
    cmd[2] = (u32)(480 << 6) << 16; /* CLUT y 480 x 0, u v 0 */
    cmd[3] = xy(16, 16);
    gpu_commands(cmd, 4);
    CHECK(gpu_vram[300][300] == 0, "texel 0 is transparent: %04X", gpu_vram[300][300]);
    CHECK(gpu_vram[301][300] == 1, "texel 1 of an odd row: %04X", gpu_vram[301][300]);
    CHECK(gpu_vram[300][301] == 1 && gpu_vram[300][302] == 2, "CLUT colours: %04X %04X", gpu_vram[300][301],
          gpu_vram[300][302]);
    CHECK(gpu_vram[300][303] == 0x8003, "the STP bit of a texel stays: %04X", gpu_vram[300][303]);

    /* modulation: texel * colour / 128 (0x80 leaves it) */
    gpu_vram[480][1] = 16; /* red 16 */
    cmd[0] = 0x64000040; /* modulated, red 0x40 (the word is B G R): half */
    cmd[1] = xy(400, 300);
    cmd[2] = (u32)(480 << 6) << 16;
    cmd[3] = xy(4, 1);
    gpu_commands(cmd, 4);
    CHECK((gpu_vram[300][401] & 31) == 8, "modulation 16 * 0x40 / 0x80: %d", gpu_vram[300][401] & 31);

    /* the mask: with the check on, a pixel with bit 15 is kept */
    gpu_vram[350][350] = 0x8000;
    cmd[0] = 0xE6000002;
    gpu_commands(cmd, 1);
    cmd[0] = 0x600000FF;
    cmd[1] = xy(349, 349);
    cmd[2] = xy(3, 3);
    gpu_commands(cmd, 3);
    CHECK(gpu_vram[350][350] == 0x8000 && gpu_vram[349][349] == 0x1F, "mask check: %04X %04X",
          gpu_vram[350][350], gpu_vram[349][349]);
    cmd[0] = 0xE6000000;
    gpu_commands(cmd, 1);

    /* fill: 16-pixel steps, ignores the drawing area */
    area(0, 0, 0, 0, 0, 0);
    cmd[0] = 0x0200FF00;
    cmd[1] = xy(500, 400);
    cmd[2] = xy(10, 2);
    gpu_commands(cmd, 3);
    n = count(480, 395, 60, 10, 0x03E0);
    CHECK(n == 32, "fill 10x2 at x 500: %d pixels (x 496, w 16)", n);
    area(0, 0, 1023, 511, 0, 0);

    /* a scene to look at: Gouraud triangles, lines, a textured quad */
    memset(gpu_vram, 0, sizeof(gpu_vram));
    cmd[0] = 0x300000FF;
    cmd[1] = xy(160, 10);
    cmd[2] = 0x0000FF00;
    cmd[3] = xy(300, 220);
    cmd[4] = 0x00FF0000;
    cmd[5] = xy(20, 200);
    gpu_commands(cmd, 6);
    cmd[0] = 0x50FFFFFF;
    cmd[1] = xy(0, 0);
    cmd[2] = 0x000000FF;
    cmd[3] = xy(319, 239);
    gpu_commands(cmd, 4);
    for (y = 0; y < 64; y++) {
        for (x = 0; x < 64; x++) {
            gpu_vram[y][640 + x] = (u16)(((x / 8 + y / 8) & 1) ? 0x7FFF : 0x001F);
        }
    }
    cmd[0] = 0xE1000000 | 10 | 2 << 7; /* 15-bit page at 640 */
    gpu_commands(cmd, 1);
    cmd[0] = 0x2C808080;
    cmd[1] = xy(200, 20);
    cmd[2] = 0;
    cmd[3] = xy(300, 40);
    cmd[4] = (u32)(10 | 2 << 7) << 16 | 63;
    cmd[5] = xy(180, 120);
    cmd[6] = 63 << 8;
    cmd[7] = xy(290, 110);
    cmd[8] = 63 | 63 << 8;
    gpu_commands(cmd, 9);
    {
        static u32 rgb[320 * 240];
        FILE *f = fopen("build/pc/gpu_test.ppm", "wb");
        s32 i;

        gpu_display.x = gpu_display.y = 0;
        gpu_display.w = 320;
        gpu_display.h = 240;
        gpu_display_rgb(rgb);
        if (f) {
            fprintf(f, "P6\n320 240\n255\n");
            for (i = 0; i < 320 * 240; i++) {
                fputc(rgb[i] >> 16, f);
                fputc(rgb[i] >> 8 & 255, f);
                fputc(rgb[i] & 255, f);
            }
            fclose(f);
        }
    }
    printf("%s\n", fails ? "GPU self-test failed" : "GPU self-test passed");
    return fails != 0;
}
