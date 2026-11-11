/* The recording of the PC port (PC_RECORD=1, e.g. in crash3_debug.txt): into
 * the folder crash3_rec (next to the disc image on Android, the current
 * folder elsewhere)
 *   - screen_NNNNN.png: the screen, 3 times a second (every 10th frame shown);
 *   - vram_NNNNN.png: the whole VRAM, every 5 seconds;
 *   - gpu_NNNNN.bin: the GPU work of a frame, every 10 seconds (for
 *     tools/pc/gpu_replay.c);
 *   - frames.txt: a line a frame: Crash (state, flags, position, speed) and
 *     each collision record near him (the object, its state, its box).
 * The PNG files are compressed (deflate with fixed Huffman codes). */
#include "pc_gpu.h"
#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

#ifdef __ANDROID__
const char *pc_android_dir(void);
#endif

static s32 on = -1;
static char dir[600];
static FILE *frames_txt;

s32 pc_record_on(void)
{
    if (on < 0) {
        const char *e = getenv("PC_RECORD");

        on = e && atoi(e) != 0;
        if (on) {
#ifdef __ANDROID__
            snprintf(dir, sizeof(dir), "%s/crash3_rec", pc_android_dir() ? pc_android_dir() : ".");
#else
            snprintf(dir, sizeof(dir), "crash3_rec");
#endif
            MKDIR(dir);
            fprintf(stderr, "port: recording into %s\n", dir);
        }
    }
    return on;
}

const char *pc_record_dir(void)
{
    return dir;
}

/* ---- PNG ---- */

static u32 crc_table[256];

static u32 crc32(u32 c, const u8 *p, u32 n)
{
    u32 i, k;

    if (!crc_table[1]) {
        for (i = 0; i < 256; i++) {
            u32 x = i;

            for (k = 0; k < 8; k++) {
                x = x & 1 ? 0xEDB88320u ^ (x >> 1) : x >> 1;
            }
            crc_table[i] = x;
        }
    }
    c = ~c;
    while (n--) {
        c = crc_table[(c ^ *p++) & 255] ^ (c >> 8);
    }
    return ~c;
}

/* a bit writer for deflate */
typedef struct {
    u8 *out;
    u32 len, cap;
    u32 bits, nbits;
} Bits;

static void put_bits(Bits *b, u32 v, u32 n) /* LSB first */
{
    b->bits |= v << b->nbits;
    b->nbits += n;
    while (b->nbits >= 8) {
        if (b->len < b->cap) {
            b->out[b->len++] = (u8)b->bits;
        }
        b->bits >>= 8;
        b->nbits -= 8;
    }
}

static void put_rev(Bits *b, u32 code, u32 n) /* a Huffman code: MSB first */
{
    u32 r = 0, i;

    for (i = 0; i < n; i++) {
        r |= ((code >> i) & 1) << (n - 1 - i);
    }
    put_bits(b, r, n);
}

/* the fixed Huffman code of a literal/length symbol */
static void put_lit(Bits *b, u32 s)
{
    if (s < 144) {
        put_rev(b, 0x30 + s, 8);
    } else if (s < 256) {
        put_rev(b, 0x190 + s - 144, 9);
    } else if (s < 280) {
        put_rev(b, s - 256, 7);
    } else {
        put_rev(b, 0xC0 + s - 280, 8);
    }
}

static const u16 len_base[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                  35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const u8 len_extra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const u16 dist_base[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                   257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const u8 dist_extra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

#define HASH_BITS 15
static s32 head[1 << HASH_BITS];
static s32 *prev_pos;

/* zlib data of in (deflate, one block with the fixed codes, LZ77 with a
 * hash chain); returns its length */
static u32 zlib(const u8 *in, u32 n, u8 *out, u32 cap)
{
    Bits b = { out, 0, cap, 0, 0 };
    u32 i = 0, a = 1, c = 0, k;

    put_bits(&b, 0x78, 8);
    put_bits(&b, 0x01, 8);
    put_bits(&b, 1, 1); /* the last block */
    put_bits(&b, 1, 2); /* fixed codes */
    prev_pos = realloc(prev_pos, n * sizeof(s32) + 4);
    memset(head, 0xFF, sizeof(head));
    while (i < n) {
        u32 best = 0, best_d = 0;

        if (i + 3 <= n) {
            u32 h = ((in[i] << 10) ^ (in[i + 1] << 5) ^ in[i + 2]) & ((1 << HASH_BITS) - 1);
            s32 p = head[h];
            s32 tries = 32;

            while (p >= 0 && i - (u32)p <= 32768 && tries--) {
                u32 l = 0;

                while (l < 258 && i + l < n && in[p + l] == in[i + l]) {
                    l++;
                }
                if (l > best) {
                    best = l;
                    best_d = i - (u32)p;
                    if (l == 258) {
                        break;
                    }
                }
                p = prev_pos[p];
            }
            prev_pos[i] = head[h];
            head[h] = (s32)i;
        }
        if (best >= 3) {
            for (k = 0; k < 28 && len_base[k + 1] <= best; k++) {
            }
            put_lit(&b, 257 + k);
            put_bits(&b, best - len_base[k], len_extra[k]);
            for (k = 0; k < 29 && dist_base[k + 1] <= best_d; k++) {
            }
            put_rev(&b, k, 5);
            put_bits(&b, best_d - dist_base[k], dist_extra[k]);
            /* (the positions inside the match go in the hash chains too) */
            for (k = 1; k < best && i + k + 3 <= n; k++) {
                u32 h = ((in[i + k] << 10) ^ (in[i + k + 1] << 5) ^ in[i + k + 2]) & ((1 << HASH_BITS) - 1);

                prev_pos[i + k] = head[h];
                head[h] = (s32)(i + k);
            }
            i += best;
        } else {
            put_lit(&b, in[i]);
            i++;
        }
    }
    put_lit(&b, 256);
    put_bits(&b, 0, (8 - b.nbits) & 7); /* to the byte */
    for (i = 0; i < n; i++) {
        a = (a + in[i]) % 65521;
        c = (c + a) % 65521;
    }
    for (k = 0; k < 4; k++) {
        put_bits(&b, ((c << 16 | a) >> (24 - 8 * k)) & 255, 8);
    }
    return b.len;
}

static void chunk(FILE *f, const char *type, const u8 *data, u32 n)
{
    u8 h[8];
    u32 c;

    h[0] = n >> 24, h[1] = n >> 16, h[2] = n >> 8, h[3] = n;
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    fwrite(data, 1, n, f);
    c = crc32(crc32(0, (const u8 *)type, 4), data, n);
    h[0] = c >> 24, h[1] = c >> 16, h[2] = c >> 8, h[3] = c;
    fwrite(h, 1, 4, f);
}

/* rgb: 0x00RRGGBB per pixel */
static void write_png(const char *path, const u32 *rgb, s32 w, s32 h)
{
    static const u8 sig[8] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };
    u32 raw_n = (u32)(w * 3 + 1) * (u32)h;
    u8 *raw = malloc(raw_n), *z = malloc(raw_n + raw_n / 2 + 64);
    u8 ihdr[13];
    s32 x, y;
    FILE *f;

    if (!raw || !z) {
        free(raw);
        free(z);
        return;
    }
    for (y = 0; y < h; y++) {
        u8 *row = raw + (u32)y * (u32)(w * 3 + 1);

        row[0] = 0;
        for (x = 0; x < w; x++) {
            u32 c = rgb[y * w + x];

            row[1 + x * 3] = c >> 16;
            row[2 + x * 3] = c >> 8;
            row[3 + x * 3] = c;
        }
    }
    f = fopen(path, "wb");
    if (f) {
        ihdr[0] = w >> 24, ihdr[1] = w >> 16, ihdr[2] = w >> 8, ihdr[3] = w;
        ihdr[4] = h >> 24, ihdr[5] = h >> 16, ihdr[6] = h >> 8, ihdr[7] = h;
        ihdr[8] = 8, ihdr[9] = 2, ihdr[10] = 0, ihdr[11] = 0, ihdr[12] = 0;
        fwrite(sig, 1, 8, f);
        chunk(f, "IHDR", ihdr, 13);
        chunk(f, "IDAT", z, zlib(raw, raw_n, z, raw_n + raw_n / 2 + 64));
        chunk(f, "IEND", NULL, 0);
        fclose(f);
    }
    free(raw);
    free(z);
}

/* ---- what is recorded ---- */

/* from src/pc/video.c at each frame shown */
void pc_record_present(const u32 *rgb, s32 w, s32 h, u32 frame)
{
    static u32 shots;
    char path[700];

    if (!pc_record_on() || shots >= 3000) {
        return;
    }
    if (frame % 10 == 0) {
        snprintf(path, sizeof(path), "%s/screen_%05u.png", dir, frame);
        write_png(path, rgb, w, h);
        shots++;
    }
    if (frame % 150 == 0) {
        static u32 vram_rgb[GPU_VRAM_W * GPU_VRAM_H];
        u32 i;

        for (i = 0; i < GPU_VRAM_W * GPU_VRAM_H; i++) {
            u16 c = ((u16 *)gpu_vram)[i];

            vram_rgb[i] = (u32)(c & 31) << 19 | (u32)(c >> 5 & 31) << 11 | (u32)(c >> 10 & 31) << 3;
        }
        snprintf(path, sizeof(path), "%s/vram_%05u.png", dir, frame);
        write_png(path, vram_rgb, GPU_VRAM_W, GPU_VRAM_H);
    }
}

/* a line of frames.txt (src/coll.c writes its parts) */
void pc_record_line(const char *fmt, ...);

#include <stdarg.h>

void pc_record_line(const char *fmt, ...)
{
    static u32 lines;
    va_list ap;

    if (!pc_record_on() || lines >= 200000) {
        return;
    }
    if (!frames_txt) {
        char path[700];

        snprintf(path, sizeof(path), "%s/frames.txt", dir);
        frames_txt = fopen(path, "w");
        if (!frames_txt) {
            on = 0;
            return;
        }
    }
    va_start(ap, fmt);
    vfprintf(frames_txt, fmt, ap);
    va_end(ap);
    if (strchr(fmt, '\n')) {
        lines++;
        if (lines % 30 == 0) {
            fflush(frames_txt);
        }
    }
}
