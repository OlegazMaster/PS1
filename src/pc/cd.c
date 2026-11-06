/* libcd of the PC port: the CD is an image of the user's disc (.cue with its
 * .bin, a raw .bin of 2352-byte sectors or an .iso of 2048-byte ones), read
 * at once (the game waits for its reads anyway).
 *
 * The game reads the ISO 9660 file system itself (src/cdfs.c), so only the
 * sector commands are needed: set the position (CdlSetloc, the seeks) and
 * read. pc_cd_find() finds a file in the root directory for the start code
 * (the executable). */
#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#endif

#define CdlNop 0x01
#define CdlSetloc 0x02
#define CdlReadN 0x06
#define CdlPause 0x09
#define CdlSetmode 0x0E
#define CdlSeekL 0x15
#define CdlSeekP 0x16
#define CdlReadS 0x1B

#define CdlComplete 2
#define CdlModeSize1 0x20 /* 2340 bytes: the sector after its sync pattern */

static FILE *disc;
static s32 raw;      /* 2352-byte sectors */
static u32 cd_pos;   /* the next sector */
static u32 cd_mode;
static u32 sectors;  /* in the image */

static u8 bcd(u32 x)
{
    return (u8)((x / 10) << 4 | (x % 10));
}

static u32 unbcd(u8 x)
{
    return (x >> 4) * 10 + (x & 15);
}

/* reads sector lba: 2048 bytes of data, or (whole) the 2340 after the sync */
static s32 read_sector(u32 lba, u8 *buf, s32 whole)
{
    u8 tmp[2352];

    if (!disc || lba >= sectors) {
        return 0;
    }
    if (!raw) {
        if (whole) {
            memset(buf, 0, 2340); /* an .iso has no headers: zeros */
            buf += 12;            /* the data after the header and the subheader */
        }
        fseek(disc, (long)lba * 2048, SEEK_SET);
        return fread(buf, 1, 2048, disc) == 2048;
    }
    fseek(disc, (long)lba * 2352, SEEK_SET);
    if (fread(tmp, 1, 2352, disc) != 2352) {
        return 0;
    }
    if (whole) {
        memcpy(buf, tmp + 12, 2340);
    } else {
        /* mode 1: data after 16 bytes; mode 2 form 1: after the subheader too */
        memcpy(buf, tmp + (tmp[15] == 1 ? 16 : 24), 2048);
    }
    return 1;
}

/* opens the disc image: a .cue (its first FILE), a .bin or an .iso */
s32 pc_cd_open(const char *path)
{
    char line[512];
    char bin[1024];
    const char *ext = strrchr(path, '.');
    u8 head[16];
    long size;

    if (ext && !strcasecmp(ext, ".cue")) {
        FILE *cue = fopen(path, "r");
        char *q1, *q2;

        if (!cue) {
            return 0;
        }
        bin[0] = 0;
        while (fgets(line, sizeof(line), cue)) {
            if (!strncasecmp(line + strspn(line, " \t"), "FILE", 4) && (q1 = strchr(line, '"')) &&
                (q2 = strchr(q1 + 1, '"'))) {
                const char *slash = strrchr(path, '/');
                const char *bslash = strrchr(path, '\\'); /* Windows */
                size_t dir;

                if (bslash && (!slash || bslash > slash)) {
                    slash = bslash;
                }
                dir = slash ? (size_t)(slash - path + 1) : 0;
                *q2 = 0;
                if (q1[1] == '/' || q1[1] == '\\' || (q1[1] && q1[2] == ':')) { /* an absolute path */
                    snprintf(bin, sizeof(bin), "%s", q1 + 1);
                } else {
                    snprintf(bin, sizeof(bin), "%.*s%s", (int)dir, path, q1 + 1);
                }
                break;
            }
        }
        fclose(cue);
        if (!bin[0]) {
            return 0;
        }
        path = bin;
    }
    disc = fopen(path, "rb");
    if (!disc) {
        return 0;
    }
    fseek(disc, 0, SEEK_END);
    size = ftell(disc);
    fseek(disc, 0, SEEK_SET);
    raw = fread(head, 1, 16, disc) == 16 && head[0] == 0 && head[1] == 0xFF && head[11] == 0;
    sectors = (u32)(size / (raw ? 2352 : 2048));
    return 1;
}

/* finds a file in the root directory (name as on the disc, e.g.
 * "SCUS_942.44"); its sector and size */
s32 pc_cd_find(const char *name, u32 *lba, u32 *size)
{
    u8 pvd[2048];
    u8 dir[2048];
    u32 root, root_size, s, pos;
    size_t n = strlen(name);

    if (!read_sector(16, pvd, 0) || memcmp(pvd + 1, "CD001", 5)) {
        return 0;
    }
    memcpy(&root, pvd + 156 + 2, 4);
    memcpy(&root_size, pvd + 156 + 10, 4);
    for (s = 0; s * 2048 < root_size; s++) {
        if (!read_sector(root + s, dir, 0)) {
            return 0;
        }
        for (pos = 0; pos < 2048 && dir[pos];) {
            u8 *r = dir + pos;

            if (r[32] >= n && !strncasecmp((char *)r + 33, name, n) && (r[32] == n || r[33 + n] == ';')) {
                memcpy(lba, r + 2, 4);
                memcpy(size, r + 10, 4);
                return 1;
            }
            pos += r[0];
        }
    }
    return 0;
}

/* reads size bytes of the file at sector lba */
s32 pc_cd_read_file(u32 lba, u32 size, void *buf)
{
    u8 tmp[2048];
    u8 *out = buf;

    while (size) {
        u32 n = size < 2048 ? size : 2048;

        if (!read_sector(lba++, tmp, 0)) {
            return 0;
        }
        memcpy(out, tmp, n);
        out += n;
        size -= n;
    }
    return 1;
}

/* ---- the PSY-Q functions ---- */

s32 CdInit(void)
{
    return disc != NULL;
}

u8 *CdIntToPos(s32 i, u8 *p)
{
    i += 150;
    p[0] = bcd(i / 75 / 60);
    p[1] = bcd(i / 75 % 60);
    p[2] = bcd(i % 75);
    p[3] = 0;
    return p;
}

s32 CdPosToInt(u8 *p)
{
    return (unbcd(p[0]) * 60 + unbcd(p[1])) * 75 + unbcd(p[2]) - 150;
}

static s32 command(s32 com, u8 *param, u8 *result)
{
    switch (com) {
    case CdlSetloc:
    case CdlSeekL:
    case CdlSeekP:
        if (param) {
            cd_pos = (u32)CdPosToInt(param);
        }
        break;
    case CdlSetmode:
        if (param) {
            cd_mode = param[0];
        }
        break;
    default:
        break;
    }
    if (result) {
        result[0] = 0x02; /* status: the motor is on */
    }
    return disc != NULL;
}

/* blocking and not: the same here */
s32 CdControl(s32 com, u8 *param, u8 *result)
{
    return command(com, param, result);
}

s32 CdControlB(s32 com, u8 *param, u8 *result)
{
    return command(com, param, result);
}

s32 CdControlF(s32 com, u8 *param)
{
    return command(com, param, NULL);
}

/* reads n sectors from the position (mode: as CdlSetmode) */
s32 CdRead(s32 n, u32 *buf, s32 mode)
{
    u8 *out = (u8 *)buf;
    s32 whole = mode & CdlModeSize1;

    while (n-- > 0) {
        if (!read_sector(cd_pos++, out, whole)) {
            return 0;
        }
        out += whole ? 2340 : 2048;
    }
    return 1;
}

/* CdReadSync: 0 when the read is over */
s32 func_80047F44(s32 mode, u8 *result)
{
    if (result) {
        result[0] = 0x02;
    }
    return 0;
}

s32 CdSync(s32 mode, u8 *result)
{
    if (result) {
        result[0] = 0x02;
    }
    return CdlComplete;
}
