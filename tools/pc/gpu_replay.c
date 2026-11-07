/* Plays back a recording of the GPU work of the port (crash3_gpu_N.bin, see
 * src/pc/gpu.c) on the software GPU: writes the display (OUT_display.ppm)
 * and the VRAM (OUT_vram.ppm) and prints what the frame drew.
 *
 * build: gcc -m32 -O1 -Iinclude -o build/pc/gpu_replay tools/pc/gpu_replay.c src/pc/gpu.c
 * usage: gpu_replay crash3_gpu_1.bin OUT [-v] */
#include "pc_gpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ppm(const char *path, s32 w, s32 h, const u32 *rgb)
{
    FILE *f = fopen(path, "wb");
    s32 i;

    if (!f) {
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (i = 0; i < w * h; i++) {
        fputc(rgb[i] >> 16, f);
        fputc(rgb[i] >> 8 & 255, f);
        fputc(rgb[i] & 255, f);
    }
    fclose(f);
}

static u32 counts[256];
static u32 tex_polys, tex_pages[32], cluts;

/* what the packet words draw (the counts of the GP0 commands) */
static void survey(const u32 *w, u32 n, s32 verbose)
{
    u32 k = 0;

    while (k < n) {
        u32 c = w[k] >> 24, len = 1;

        counts[c]++;
        if (c >= 0x20 && c < 0x40) {
            s32 quad = c & 8, g = c & 0x10, t = c & 4, nv = quad ? 4 : 3;

            len = 1 + nv + (t ? nv : 0) + (g ? nv - 1 : 0);
            if (t) {
                u32 uv0 = w[k + 2], uv1 = w[k + (g ? 5 : 4)];
                u32 tp = uv1 >> 16;

                tex_polys++;
                tex_pages[tp & 15 | (tp >> 4 & 1) << 4]++;
                if (verbose && tex_polys <= 20) {
                    printf("  textured %02X: xy0 %08X uv0 %04X clut %04X tpage %04X (x %d y %d depth %d abr %d)\n", c,
                           w[k + 1], uv0 & 0xFFFF, uv0 >> 16, tp, (tp & 15) * 64, (tp >> 4 & 1) * 256, tp >> 7 & 3,
                           tp >> 5 & 3);
                }
            }
        } else if (c >= 0x40 && c < 0x60) {
            len = c & 8 ? n - k : 3 + ((c & 0x10) ? 1 : 0);
        } else if (c >= 0x60 && c < 0x80) {
            len = 2 + ((c & 4) ? 1 : 0) + (((c >> 3) & 3) == 0 ? 1 : 0);
        } else if (c == 0x02) {
            len = 3;
        } else if (c >= 0x80 && c < 0xA0) {
            len = 4;
        } else if (c >= 0xA0 && c < 0xC0) {
            len = n - k;
        } else if (c >= 0xC0 && c < 0xE0) {
            len = 3;
        } else if (c == 0xE1 && verbose > 1) {
            printf("  E1 %06X\n", w[k] & 0xFFFFFF);
        }
        k += len;
    }
}

int main(int argc, char **argv)
{
    FILE *f;
    u32 head[2], *buf = NULL;
    char path[512];
    s32 verbose = argc > 3 ? (s32)strlen(argv[3]) - 1 : 0, i;
    static u32 rgb[1024 * 512];
    u32 records = 0, packets = 0, loads = 0;

    if (argc < 3 || !(f = fopen(argv[1], "rb"))) {
        fprintf(stderr, "usage: gpu_replay crash3_gpu_N.bin OUT [-v]\n");
        return 1;
    }
    gpu_reset();
    while (fread(head, 4, 2, f) == 2) {
        buf = realloc(buf, (head[1] + 1) * 4);
        if (fread(buf, 4, head[1], f) != head[1]) {
            break;
        }
        records++;
        switch (head[0]) {
        case 0:
            memcpy(gpu_vram, buf, sizeof(gpu_vram));
            break;
        case 1:
            packets++;
            survey(buf, head[1], verbose);
            gpu_commands(buf, head[1]);
            break;
        case 2:
            loads++;
            gpu_load(buf[0], buf[1], buf[2], buf[3], (const u16 *)(buf + 4));
            break;
        case 3:
            gpu_move(buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
            break;
        case 4:
            if (head[1] == gpu_state_words()) {
                memcpy(gpu_state(), buf, head[1] * 4);
            } else {
                printf("the state has %u words, this GPU %u: not restored\n", head[1], gpu_state_words());
            }
            break;
        case 5:
            gpu_display.x = buf[0];
            gpu_display.y = buf[1];
            gpu_display.w = buf[2];
            gpu_display.h = buf[3];
            gpu_display.rgb24 = buf[4];
            gpu_display.enabled = buf[5];
            break;
        }
    }
    fclose(f);
    printf("%u records: %u packets, %u loads; display %d,%d %dx%d%s\n", records, packets, loads, gpu_display.x,
           gpu_display.y, gpu_display.w, gpu_display.h, gpu_display.enabled ? "" : " (off)");
    printf("commands:");
    for (i = 0; i < 256; i++) {
        if (counts[i]) {
            printf(" %02X:%u", i, counts[i]);
        }
    }
    printf("\ntextured polygons %u; by texture page:", tex_polys);
    for (i = 0; i < 32; i++) {
        if (tex_pages[i]) {
            printf(" (%d,%d):%u", (i & 15) * 64, (i >> 4) * 256, tex_pages[i]);
        }
    }
    printf("\n");
    if (gpu_display.w > 0 && gpu_display.h > 0 && gpu_display.w <= 1024 && gpu_display.h <= 512) {
        gpu_display_rgb(rgb);
        snprintf(path, sizeof(path), "%s_display.ppm", argv[2]);
        ppm(path, gpu_display.w, gpu_display.h, rgb);
    }
    gpu_display.x = gpu_display.y = 0;
    gpu_display.w = 1024;
    gpu_display.h = 512;
    gpu_display.rgb24 = 0;
    gpu_display_rgb(rgb);
    snprintf(path, sizeof(path), "%s_vram.ppm", argv[2]);
    ppm(path, 1024, 512, rgb);
    (void)cluts;
    return 0;
}
