#include "common.h"
#include "gool.h"
#include "game.h"
#include "ns.h"

/* Reading files from the CD (its ISO 9660 file system). */

/* .sdata */
char D_800609DC[] = "CD001";
char D_800609E4[] = "rfs\n";
char D_800609EC[] = ".";
char D_800609F0[] = "..";
char D_800609F4[] = "WARP";
char D_800609FC[] = "";

INCLUDE_ASM("asm/us/nonmatchings/cdfs", func_80032268);

INCLUDE_ASM("asm/us/nonmatchings/cdfs", func_800322D0);
