#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

/* Starting a level: the checkpoint and spawn state, level data. */

/* The header of a zone entry (item 0); see also gool.c. */
typedef struct {
    u8 unk0[0x190];
    s32 neighbor_count; /* 0x190 */
    u32 neighbors[(0x2BC - 0x194) / 4]; /* 0x194: references to the neighbor zones */
    u32 flags; /* 0x2BC: 1 loaded, 2 ... */
} ZoneHdr;

void GoolZoneObjectsTerminate(s32 zone);
void func_8002157C(Entry *zone);
extern GoolObject *crash;

/* .sdata */
s32 bonus_return = 0;
s32 D_8006092C = 1;

/* .sbss */
s32 D_80060AA0;
s32 D_80060AA4;
s32 D_80060AA8[2];
s32 D_80060AB0;
s32 D_80060AB4;
s32 D_80060AB8;
s32 D_80060ABC;

INCLUDE_ASM("asm/us/nonmatchings/level", func_80021084);

INCLUDE_ASM("asm/us/nonmatchings/level", func_80021330);

/* Unloads zone while it is the current one (c1: zone on_free?). */

s32 func_80021474(Entry *zone) {
    void *prev = D_80065C90.unkD0;

    D_80065C90.unkD0 = D_80065C90.unkCC;
    func_8002157C(zone);
    D_80065C90.unkD0 = prev;
    return -0xFF;
}

/* Whether other is a neighbor of zone. */

/* Whether other is a neighbor of zone. */

s32 func_800214B8(Entry *zone, Entry *other) {
    ZoneHdr *h = (ZoneHdr *)zone->items[0];
    s32 i;

    for (i = 0; i < h->neighbor_count; i++) {
        if (((h->neighbors[i] & 1) ? h->neighbors[i] : ((Entry *)h->neighbors[i])->eid) == other->eid) {
            return 1;
        }
    }
    return 0;
}

/* On the first load of zone, resets the crate stacking (see GoolObjectSpawn). */

void func_80021530(Entry *zone) {
    ZoneHdr *h = (ZoneHdr *)zone->items[0];

    if (!(h->flags & 1)) {
        game.unk378 = NULL;
        game.unk380 = 0;
        game.unk37C = 0x19000;
        h->flags |= 1;
    }
}

/* When zone is freed: moves the player out of it and kills its objects. */

void func_8002157C(Entry *zone) {
    ZoneHdr *h = (ZoneHdr *)zone->items[0];

    if (crash->zone == zone) {
        crash->zone = D_80065C90.unkD0;
    }
    if (h->flags & 1) {
        GoolZoneObjectsTerminate((s32)zone);
        h->flags &= ~3;
    }
}

INCLUDE_ASM("asm/us/nonmatchings/level", func_800215E8);
