#include "common.h"
#include "gfx.h"
#include "ns.h"

/* Textures and sprites. */

s32 func_80018088(Entry *entry, s32 flag, u32 eid, s32 count, void *infos, s32 ref_count, u32 *refs);

s32 func_80018008(Entry *entry, s32 flag, u32 eid) {
    u8 *hdr = entry->items[0];

    return func_80018088(entry, flag, eid, *(s32 *)(hdr + 0x1C), entry->items[4], *(s32 *)(hdr + 0x28), (u32 *)(hdr + 0x2C));
}

s32 func_80018048(Entry *entry, s32 flag, u32 eid) {
    u8 *hdr = entry->items[0];

    return func_80018088(entry, flag, eid, *(s32 *)(hdr + 0x34), entry->items[3], *(s32 *)(hdr + 0x40), (u32 *)(hdr + 0xC));
}

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80018088);

#ifdef NON_MATCHING
/* Updates the texture references of the loaded entries of types 2 and 3
 * after the texture page eid was loaded (flag 1) or dropped. Equivalent; the
 * original keeps the constants 2 and 3 in registers across the loop. */

s32 func_800181E0(u32 eid, s32 flag) {
    Entry *entry;
    s32 i;

    for (i = ns.nsd->pte_count - 1; i >= 0; i--) {
        entry = (Entry *)ns.nsd->ptes[i].value;
        if (!((u32)entry & 3)) {
            switch (entry->type) {
            case 3:
                func_80018008(entry, flag, eid);
                break;
            case 2:
                func_80018048(entry, flag, eid);
                break;
            }
        }
    }
    return -0xFF;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/gfx", func_800181E0);
#endif

s32 func_800182B4(Entry *entry) {
    return func_80018008(entry, 1, 0x6396347F);
}

s32 func_800182DC(Entry *entry) {
    return func_80018008(entry, 0, 0x6396347F);
}

s32 func_80018304(Entry *entry) {
    return func_80018048(entry, 1, 0x6396347F);
}

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_8001832C);

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_800185C8);

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_800186D8);

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80018820);

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_800188EC);

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80018988);

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80018A54);

typedef struct {
    s32 rot[3];
    s32 trans[3];
    s32 scale[3];
} Transform;

void func_80018DA8(Transform *t) {
    t->trans[0] = 0;
    t->trans[1] = 0;
    t->trans[2] = 0;
    t->rot[0] = 0;
    t->rot[1] = 0;
    t->rot[2] = 0;
    t->scale[0] = 0x1000;
    t->scale[1] = 0x1000;
    t->scale[2] = 0x1000;
}

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80018DD4);

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80018EBC);

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80019090);

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_80019180);

INCLUDE_ASM("asm/us/nonmatchings/gfx", func_800192F8);
