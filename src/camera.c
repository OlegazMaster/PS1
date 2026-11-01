#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

/* The camera: follows its path in the zone. */

/* .sdata: names of the water and terrain entries (string literals of
 * func_80021E94 while it is still assembly) */
char D_80060930[] = "Wat1R";
char D_80060938[] = "Wat2R";
char D_80060940[] = "Wat6R";
char D_80060948[] = "Wat7R";
char D_80060950[] = "Wat8R";
char D_80060958[] = "Wat3R";
char D_80060960[] = "Wat4R";
char D_80060968[] = "Wat5R";
char D_80060970[] = "Wat9R";
char D_80060978[] = "WatAR";
char D_80060980[] = "WatBR";
char D_80060988[] = "WatCR";
char D_80060990[] = "WatDR";
char D_80060998[] = "WatER";
char D_800609A0[] = "Ter1R";
char D_800609A8[] = "Ter2R";
char D_800609B0[] = "Ter3R";
char D_800609B8[] = "Ter4R";

INCLUDE_ASM("asm/us/nonmatchings/camera", func_80021E94);
