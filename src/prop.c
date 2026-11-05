#include "common.h"
#include "gool.h"
#include "game.h"
#include "ns.h"

/* The properties of the entities (and other items with properties). */

INCLUDE_ASM("asm/us/nonmatchings/prop", func_80032710);

u8 *func_80032710(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, void *buf, s32 one, u8 **hdr);
/* The value of property b of the item a (see func_80032710), or def if it has
 * none. */

s32 func_80032DF8(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 def) {
    u8 buf[16];
    u8 *hdr;
    u8 *v;

    v = func_80032710(a, b, c, d, e, f, buf, 1, &hdr);
    if (v == NULL) {
        return def;
    }
    switch (hdr[4] & 0x1F) {
    case 1:
        return *(u8 *)v;
    case 0x11:
        return *(s8 *)v;
    case 2:
        return *(u16 *)v;
    case 0x12:
        return *(s16 *)v;
    default:
        return *(s32 *)v;
    }
}

/* The count of values of property b of a (0 if none). */

s32 func_80032EDC(s32 a, s32 b, s32 c, s32 f) {
    u8 buf[16];
    s32 count;

    if (func_80032710(a, b, c, 0, (s32)&count, f, buf, 1, NULL) == NULL) {
        return 0;
    }
    return count;
}
