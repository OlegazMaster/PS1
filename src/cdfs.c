#include "common.h"
#include "gool.h"
#include "game.h"
#include "ns.h"

/* Reading files from the CD (its ISO 9660 file system). */

#ifndef NON_MATCHING
/* The literals of func_800322D0. */
char D_800609DC[] = "CD001";
char D_800609E4[] = "rfs\n";
char D_800609EC[] = ".";
char D_800609F0[] = "..";
char D_800609F4[] = "WARP";
char D_800609FC[] = "";
#endif


void CdIntToPos(s32 sector, u8 *loc);
s32 CdControlB(s32 com, u8 *param, u8 *result);
s32 CdRead(s32 sectors, void *buf, s32 mode);
s32 func_80047F44(s32 mode, u8 *result); /* CdReadSync */
/* Reads count sectors from sector into buf and waits; whether it worked. */

s32 func_80032268(s32 count, s32 sector, u32 *buf) {
    u8 loc[4];

    CdIntToPos(sector, loc);
    CdControlB(2, loc, 0);
    CdRead(count, buf, 0x80);
    return func_80047F44(0, 0) == 0;
}

void *malloc(s32 size);
void free(void *p);
void *memcpy(void *dst, const void *src, s32 n);
s32 strncmp(const char *a, const char *b, s32 n);
s32 strlen(const char *s);
char *strcpy(char *dst, const char *src);
s32 printf(const char *fmt, ...);
/* The files of a level on the CD (D_8006C4D8, by level). */
typedef struct {
    u32 nsd : 23; /* sector of the NSD file */
    u32 : 9;
    u32 nsd_size : 17;
    u32 : 15;
    u32 nsf : 24; /* sector of the NSF file */
    u32 : 8;
} CdLevelFiles;
#define LEVEL_FILES ((CdLevelFiles *)D_8006C4D8)
#ifdef NON_MATCHING
/* Finds the files of the levels on the CD: the directories S0..S3 hold
 * "S...XY.NSD" and "S...XY.NSF" (XY: the level in hex); WARP goes to entry
 * 61. Returns whether it could read the CD.
 * Equivalent; the original enters the first loop and keeps name in other registers. */
s32 func_800322D0(void) {
    char name[32];
    s32 dirs[4];
    u32 lba;
    u32 ext;
    u32 size;
    u8 *buf;
    u8 *p;
    s32 i;
    s32 n;
    s32 idx;
    s32 ok;
    char *nm;
    CdLevelFiles *files;

    buf = malloc(0x800);
    ok = func_80032268(1, 16, buf);
    if (ok != 1 || strncmp((char *)buf + 1, "CD001", 5)) {
        goto fail;
    }
    printf("rfs\n");
    __builtin_memcpy(&lba, buf + 0x8C, 4);
    if (func_80032268(1, lba, buf) != ok) {
        goto fail;
    }
    p = buf;
    for (i = 3; i >= 0; i--) {
        dirs[i] = -1;
    }
    nm = name;
    for (n = 0; p < buf + 0x800; ) {
        if (*p == 0) {
            break;
        }
        memcpy(nm, p + 8, *p);
        nm[*p] = 0;
        __builtin_memcpy(&ext, p + 2, 4);
        if (strlen(nm) == 2 && (name[0] == 'S' || name[0] == 's')) {
            dirs[name[1] - '0'] = ext;
        }
        p += *p + (*p & 1) + 8;
        if (++n >= 0x80) {
            break;
        }
    }
    files = LEVEL_FILES;
    for (i = 0; i < 4; i++) {
        if (dirs[i] != -1 && func_80032268(1, dirs[i], buf) != 1) {
            goto fail;
        }
        for (p = buf, n = 0; p < buf + 0x800;) {
            if (*p == 0) {
                break;
            }
            __builtin_memcpy(&lba, p + 2, 4);
            __builtin_memcpy(&size, p + 10, 4);
            switch (n) {
            case 0:
                strcpy(name, ".");
                break;
            case 1:
                strcpy(name, "..");
                break;
            default:
                memcpy(nm, p + 33, p[32]);
                nm[p[32]] = 0;
                break;
            }
            if (name[0] == 'S' || name[0] == 's') {
                if ((u8)name[7] > '@') {
                    idx = (name[6] - '0') * 16 + (name[7] - 'A' + 10);
                } else {
                    idx = (name[6] - '0') * 16 + (name[7] - '0');
                }
                if (name[11] == 'D' || name[11] == 'd') {
                    files[idx].nsd = lba;
                    files[idx].nsd_size = size;
                } else {
                    files[idx].nsf = lba;
                }
            }
            if (!strncmp(name, "WARP", 4)) {
                files[61].nsd = lba;
                files[61].nsd_size = size;
            }
            p += *p;
            if (++n >= 0x40) {
                break;
            }
        }
    }
    free(buf);
    return 1;
fail:
    free(buf);
    return 0;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/cdfs", func_800322D0);
#endif
