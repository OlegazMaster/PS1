#include "common.h"
#include "gool.h"
#include "gfx.h"
#include "game.h"
#include "ns.h"

/* Starting a level: the checkpoint and spawn state, level data. */

/* The header of a zone entry (item 0); see also gool.c. */
typedef struct {
    u8 unk0[0x184];
    s32 first_item; /* 0x184: index of the first path item */
    s32 path_items; /* 0x188: 3 per camera path */
    s32 entity_count; /* 0x18C */
    s32 neighbor_count; /* 0x190 */
    u32 neighbors[(0x2BC - 0x194) / 4]; /* 0x194: references to the neighbor zones */
    u32 flags; /* 0x2BC: 1 loaded, 2 ... */
} ZoneHdr;

void GoolZoneObjectsTerminate(s32 zone);
void func_8002157C(Entry *zone);
s32 func_800214B8(Entry *zone, Entry *other);
void func_80021530(Entry *zone);
extern Entry *cur_zone;
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

typedef struct {
    u8 unk0[0x24];
    s32 unk24; /* 0x24: checkpoint zone */
    s32 unk28; /* 0x28: checkpoint path */
    s32 progress; /* 0x2C */
} SaveStateView;
extern SaveStateView savestate;
extern s32 D_80060A64;
extern s8 D_8005C5E8[]; /* (level, checkpoint) pairs, ended by a negative level */
void *memset(void *dst, s32 c, s32 n);
extern u32 spawns[];
void func_80020F04(s32 lid);
void func_800188EC(void);
void func_80030940(s32 *a);
s32 func_800215E8(Entry *zone, u32 *path, s32 progress, s32 a);

#ifdef NON_MATCHING
/* Starts the level: clears the spawn flags, picks the checkpoint (for level 2
 * by game.unk1C4 through D_8005C5E8), loads its zone and starts there.
 * Equivalent; the original swaps the registers of &ns and &game, and of
 * &ns and cp * 0x18, and schedules two stores earlier. */
s32 func_80021084(void) {
    s32 cp;
    s32 progress;
    s32 a;
    s8 *e;
    s32 lv;
    Entry *zone;

    game.unk204 = 0;
    if (*ns.unk10 != 0x6396347F) {
        D_80060AB0 = 0;
        D_80060AB4 = 0;
        game.unk70 = 0xC01FFFF;
        game.display_flags = 0xC01FFFF;
        cp = game.unk26C;
        progress = 0;
        if (ns.ldat->lid != D_80060ABC) {
            for (;;) {
            }
        }
        if (ns.ldat->lid == 2) {
            cp = 2;
            if (!(game.unk1C8 & 1)) {
                a = game.unk1C4;
                if (a == 0x63) {
                    a = game.unkB0;
                }
                if (a != 0x63) {
                    a >>= 8;
                }
                e = D_8005C5E8;
                do {
                    lv = *e++;
                    cp = *e++;
                    if (lv < 0) {
                        goto none;
                    }
                } while (lv != a);
            }
        }
    set:
        game.unk26C = cp;
        if (cp >= ns.ldat->checkpoint_count) {
            cp = 0;
            game.unk26C = 0;
        }
        memset(spawns, 0, 0xC00);
        func_80020F04(ns.ldat->lid);
        func_800188EC();
        if (bonus_return == 1) {
            bonus_return = 0;
            ns.ldat->checkpoints[cp].zone = savestate.unk24;
            ns.ldat->checkpoints[cp].path = savestate.unk28;
            progress = savestate.progress;
        }
        D_80065C90.unkCC = NULL;
        D_80065C90.unkD4 = 0;
        D_80065C90.unkE0 = 0;
        zone = (Entry *)NSOpen(&ns.ldat->checkpoints[cp].zone, 1, 1);
        D_80060A64 = 0x6396347F;
        if (game.game_state == 0x600) {
            func_80030940(&D_80060A64);
        }
        func_800215E8(zone, (u32 *)zone->items[ns.ldat->checkpoints[cp].path * 3 + ((ZoneHdr *)zone->items[0])->first_item], progress, 0);
        func_8001440C(&ns.ldat->checkpoints[cp].zone, 1);
        goto end;
    none:
        cp = 1;
        goto set;
    } else {
        ns.ldat->unk4 = 0;
    }
end:
    D_8006092C = 0;
    return -0xFF;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/level", func_80021084);
#endif

#define ZoneHdrOf(zone) ((ZoneHdr *)(zone)->items[0])

#ifdef NON_MATCHING
/* When zone is loaded: links its camera paths (3 items each) and entities
 * to it, and resets the crate stacking if it is next to the current zone.
 * Equivalent; the original uses other argument registers for i and first. */
s32 func_80021330(Entry *zone) {
    s32 n = ZoneHdrOf(zone)->path_items / 3;
    s32 i;
    s32 j;
    u32 *first;
    u32 *item;

    for (i = 0; i < n; i++) {
        first = (u32 *)zone->items[i * 3 + ZoneHdrOf(zone)->first_item];
        first[1] = (u32)zone;
        for (j = 1; j < 3; j++) {
            item = (u32 *)zone->items[i * 3 + j + ZoneHdrOf(zone)->first_item];
            item[1] = (u32)zone;
            item[2] = (u32)first;
        }
    }
    for (i = 0; i < ZoneHdrOf(zone)->entity_count; i++) {
        ((u32 *)zone->items[i + ZoneHdrOf(zone)->first_item + ZoneHdrOf(zone)->path_items])[1] = (u32)zone;
    }
    if (D_80065C90.unkCC != NULL && func_800214B8((Entry *)D_80065C90.unkCC, zone)) {
        func_80021530(zone);
    }
    return -0xFF;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/level", func_80021330);
#endif

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

INCLUDE_ASM("asm/us/nonmatchings/level", func_80021E60);
