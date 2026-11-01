#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

/* Collision of objects with the surfaces of the zones and with other
 * objects. */

/* .sdata */
s32 D_800609C0 = 0xF424;

/* .sbss */
s32 D_80060AC8;
s32 D_80060ACC;
s32 D_80060AD0;
s32 D_80060AD4;
u32 *D_80060AD8; /* scratchpad 0x1F800180 */
u32 *D_80060ADC; /* scratchpad 0x1F800100: 32 rows of 32 bits */
u32 *D_80060AE0; /* scratchpad 0x1F800380 */
s32 D_80060AE4;

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002AA94);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002AF70);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002B1CC);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002B394);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002B4E8);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002B838);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002B9A4);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002C230);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002C604);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002CAE4);

Entry *func_80028D8C(Entry *zone, s32 *pos);
/* Finds the zone obj at pos is in; if there is none and pos is below the
 * floor of its zone, either sends it event 0x900 (zones whose floor kills)
 * or puts it on the floor. */

s32 func_8002CD8C(GoolObject *obj, s32 *pos) {
    Entry *zone;
    s32 *box;
    s32 land;
    u32 arg;
    s32 v;

    zone = func_80028D8C((Entry *)D_80065C90.unkCC, pos);
    if ((s32)zone >= -0xFE) {
        box = (s32 *)obj->zone->items[1];
        if (pos[1] < (box[1] << 8) - *(s32 *)((u8 *)obj->unk8 + 0x34)) {
            land = 1;
            if ((((ZoneHdr *)obj->zone->items[0])->unk2DC & 2) && obj->invincibility_state != 2) {
                arg = 0x6400;
                GoolSendEvent(NULL, obj, 0x900, 1, &arg);
                land = 0;
            }
            if (land) {
                pos[1] = (box[1] << 8) - *(s32 *)((u8 *)obj->unk8 + 0x34);
                v = obj->misc_a[1];
                obj->misc_a[1] = 0;
                obj->status_a |= 1;
                obj->unk114 = v;
                obj->unk110 = game.frames;
            }
        }
    } else {
        obj->zone = zone;
    }
    return -0xFF;
}

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002CEA8);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002D0E8);

/* Drops the last collision record. */

void func_8002D85C(void) {
    u8 *end;

    if (game.unk5C != 0) {
        game.unk5C--;
        end = game.unk58;
        switch (*(u32 *)game.unk390[game.unk5C] & 7) {
        case 0:
            end -= 0x20;
            break;
        case 1:
            end -= 0x44;
            break;
        }
        game.unk58 = end;
    }
}

/* Clears the list of collision records. */

void func_8002D8C8(void) {
    game.unk58 = (u8 *)&game + 0x4B0;
    game.unk5C = 0;
}

extern u32 D_8005C884[];
/* Puts the collision tables in the scratchpad. */

s32 func_8002D8E0(void) {
    s32 i;

    D_80060ADC = (u32 *)0x1F800100;
    D_80060AD8 = (u32 *)0x1F800180;
    D_80060AE0 = (u32 *)0x1F800380;
    for (i = 0; i < 32; i++) {
        ((u32 *)0x1F800380)[i] = D_8005C884[i];
    }
    return -0xFF;
}

s32 func_8002D93C(void) {
    return -0xFF;
}

void func_8003BBF0(s32 x, s32 z);

void func_8002D944(s32 x, s32 z) {
    if ((x < 0 ? -x : x) < 32 && (z < 0 ? -z : z) < 32) {
        func_8003BBF0(x, z);
    }
}

/* Sets (set) or clears the cells x0 + 16 .. x1 + 16, y0 + 16 .. y1 + 16 of the
 * 32x32 grid D_80060ADC (bit 31 of a row is column 0). */

void func_8002D990(s32 x0, s32 y0, s32 x1, s32 y1, s32 set) {
    u32 *row;
    u32 mask = -1;

    x1 += 16;
    if (x1 < 32) {
        if (x1 <= 0) {
            return;
        }
        mask = -1 << (32 - x1);
    }
    x0 += 15;
    if (x0 >= 32) {
        return;
    }
    if (x0 >= 0) {
        mask &= ~((s32)0x80000000 >> x0);
    }
    if (mask == 0) {
        return;
    }
    y0 += 16;
    if (y0 < 0) {
        y0 = 0;
    }
    y1 += 16;
    if (y1 > 32) {
        y1 = 32;
    }
    if (y0 >= 32) {
        return;
    }
    row = &D_80060ADC[y0];
    if (set) {
        for (; y0 < y1; y0++) {
            *row++ |= mask;
        }
    } else {
        mask = ~mask;
        for (; y0 < y1; y0++) {
            *row++ &= mask;
        }
    }
}

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002DA74);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002E4F0);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002EC2C);

/* Whether cell (x, y) of the 32x32 grid D_80060ADC is free. */

s32 func_8002EDA8(u32 x, u32 y) {
    if (x < 32 && y < 32 && !(D_80060ADC[y] & (0x80000000 >> x))) {
        return 1;
    }
    return 0;
}

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002EDF0);

INCLUDE_ASM("asm/us/nonmatchings/coll", func_8002F0F8);
