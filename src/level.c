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
void func_800215E8(Entry *zone, u32 *path, s32 progress, u32 flags);

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

s32 func_80032710(s32 a, s32 id, s32 pos, s32 b, s32 *out, s32 c, s32 d, s32 e, s32 f);
s32 func_80032DF8(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g);
s32 func_80032EDC(s32 entity, s32 tag, s32 a, s32 b);
void func_8001AC74(void);
void func_80014468(void);
s32 func_8002AA14(void *a, s32 b, s32 c, s32 d);
void func_80022C9C(u32 *path, s32 pos, s32 dir);
void func_8001AD68(s32 a, s32 pos, s32 dir);
void func_8001AE64(Entry *zone, s32 id);
void func_80012EAC(void);
void func_80024330(s32 a);
void func_8002437C(s32 a);
void func_80028740(s32 path, s32 progress, void *a, void *b, s32 c);
s32 func_80025C0C(void);
void func_8002FF3C(GoolObject *obj, void *state, s32 a, s32 b);
void func_80021E60(s32 *a, s32 flags);
GoolObject *GoolObjectCreate(GoolObject *parent, s32 exec, s32 subtype, s32 argc, u32 *argv, s32 interrupt);
typedef struct {
    s32 w[17];
} CamState;
typedef struct {
    s32 v[3];
} Vec3i;

#ifdef NON_MATCHING
/* Moves the camera to progress along path in zone (flags 8 forces it):
 * loads the neighbor zones on a zone change, walks the path points between
 * the old and the new progress spawning and killing their objects, then
 * updates the camera; creates the player if there is none. Equivalent; the
 * original allocates registers and stack slots differently. */
void func_800215E8(Entry *zone, u32 *path, s32 progress, u32 flags) {
    s32 changed = 0;
    s32 reload = 1;
    s32 n;
    s32 max;
    s32 p29;
    s32 old29;
    s32 pidx;
    s32 i;
    s32 d;
    s32 a;
    u32 *items;
    Entry *list;
    ZoneHdr *hdr;
    Entry *other;
    s32 r;

    if (flags & 8) {
        changed = 7;
    }
    if (path != NULL && zone != NULL) {
        if (zone != (Entry *)D_80065C90.unkCC) {
            changed |= 1;
            if (D_80065C90.unkCC != NULL) {
                reload = flags & 2;
                if (!(flags & 0x10)) {
                    hdr = (ZoneHdr *)((Entry *)D_80065C90.unkCC)->items[0];
                    D_80065C90.unkD0 = zone;
                    for (i = 0; i < hdr->neighbor_count; i++) {
                        if (func_8001440C(&hdr->neighbors[i], 0) != NULL) {
                            other = (Entry *)NSOpen(&hdr->neighbors[i], 1, 0);
                            if (!func_800214B8(zone, other)) {
                                func_8002157C(other);
                            }
                        }
                    }
                }
            }
        }
        if (path != (u32 *)D_80065C90.unkD4 || (flags & 8)) {
            changed |= 2;
            n = func_80032EDC((s32)path, 0x27F, 0, 2);
            max = (n - 1) << 8;
            p29 = func_80032DF8((s32)path, 0x29, 0, 0, 0, 2, 0);
        } else {
            max = D_80065C90.unkE4;
            n = D_80065C90.unkEC;
            p29 = D_80065C90.unkF0;
        }
        old29 = D_80065C90.unkF0;
        if (p29 != old29) {
            changed |= 0x10;
        }
        if (progress < 0) {
            progress = 0;
        } else if (max < progress) {
            progress = max;
        }
        if (progress != D_80065C90.unkE0) {
            changed |= 4;
        }
        pidx = progress >> 8;
        if (pidx != D_80065C90.unkE8) {
            changed |= 8;
        }
        D_80065C90.unk118[0] = D_80065C90.unk60.rot[0];
        D_80065C90.unk118[1] = D_80065C90.unk60.rot[1];
        D_80065C90.unk118[2] = D_80065C90.unk60.rot[2];
        a = *(s32 *)zone->items[0];
        if (changed & 7) {
            if (a != 0) {
                list = (Entry *)NSOpen((u32 *)func_80032710((s32)path, 0x103, 0, 0, NULL, 2, 0, 0, 0), 1, 1);
            }
            if (changed & 2) {
                func_8001AC74();
                func_80014468();
                r = D_80060AA8[0];
                D_80060AA8[0] = D_80060AB8;
                D_80060AB8 = r;
                if (pidx < n >> 1) {
                    i = 0;
                    if (a != 0) {
                        D_80060AA4 = func_8002AA14(list->items[0], D_80060AA4, r, 1);
                        i = 0;
                    }
                    func_80022C9C(path, i, 2);
                    func_8001AD68((s32)path, i, 2);
                } else {
                    if (a != 0) {
                        D_80060AA4 = func_8002AA14(list->items[n], D_80060AA4, r, 0);
                    }
                    i = n - 1;
                    func_80022C9C(path, i << 8, 1);
                    func_8001AD68((s32)path, i << 8, 1);
                }
            } else {
                i = D_80065C90.unkE8;
            }
            d = pidx - i;
            items = (u32 *)&list->items[i];
            while (d != 0) {
                r = D_80060AA8[0];
                D_80060AA8[0] = D_80060AB8;
                D_80060AB8 = r;
                if (d > 0) {
                    items++;
                    i++;
                    if (a != 0) {
                        D_80060AA4 = func_8002AA14((void *)*items, D_80060AA4, r, 1);
                    }
                    func_80022C9C(path, i << 8, 2);
                    func_8001AD68((s32)path, i << 8, 2);
                    d--;
                } else {
                    if (a != 0) {
                        D_80060AA4 = func_8002AA14((void *)*items, D_80060AA4, r, 0);
                    }
                    items--;
                    i--;
                    func_80022C9C(path, i << 8, 1);
                    func_8001AD68((s32)path, i << 8, 1);
                    d++;
                }
            }
            if (a != 0) {
                func_8001440C((u32 *)func_80032710((s32)path, 0x103, 0, 0, NULL, 2, 0, 0, 0), 1);
            }
            func_8001AE64((Entry *)path[1], 0x13B);
        }
        if (changed & 7) {
            if (changed & 1) {
                hdr = (ZoneHdr *)zone->items[0];
                if (game.game_state == 0x600 && !(flags & 2)) {
                    reload = 0;
                }
                D_80065C90.unkCC = (UnkCC *)zone;
                D_80065C90.unkD4 = (s32)path;
                D_80065C90.unkE0 = progress;
                D_80065C90.unkE8 = pidx;
                D_80065C90.unkEC = n;
                D_80065C90.unkE4 = max;
                D_80065C90.unkF0 = p29;
                func_80021E60((s32 *)((u8 *)hdr + 0x2C0), flags);
                if (reload) {
                    func_80012EAC();
                }
                for (i = 0; i < hdr->neighbor_count; i++) {
                    if (((u32 *)((u8 *)hdr + 0x1D4))[i] & 0x10) {
                        if (func_8001440C(&hdr->neighbors[i], 0) != NULL) {
                            other = (Entry *)NSOpen(&hdr->neighbors[i], 1, 0);
                            if (!(((ZoneHdr *)other->items[0])->flags & 1)) {
                                func_80021530(other);
                            }
                        }
                    }
                }
            } else {
                D_80065C90.unkCC = (UnkCC *)zone;
                D_80065C90.unkD4 = (s32)path;
                D_80065C90.unkE0 = progress;
                D_80065C90.unkE8 = pidx;
                D_80065C90.unkEC = n;
                D_80065C90.unkE4 = max;
                D_80065C90.unkF0 = p29;
            }
            if (changed & 0x10) {
                func_80024330(old29);
            }
            func_8002437C(D_80065C90.unkFC);
            func_80028740(D_80065C90.unkD4, D_80065C90.unkE0, &D_80065C90.unk84, D_80065C90.unk124, 0);
            D_80065C90.unkC8 = func_80032DF8(D_80065C90.unkD4, 0x1AA, D_80065C90.unkE0, 0, 0, 3, 0);
            if (changed & 3) {
                r = func_80032710(D_80065C90.unkD4, 0x176, 0, 0, NULL, 1, 0, 0, 0);
                D_80065C90.unk100 = r != 0 ? *(s16 *)r : 0xFFFF;
                r = func_80032710(D_80065C90.unkD4, 0x176, D_80065C90.unkE4, 0, NULL, 1, 0, 0, 0);
                D_80065C90.unk104 = r != 0 ? *(s16 *)r : 0xFFFF;
            }
            func_8001AE64(cur_zone, -1);
        }
        *(CamState *)&D_80065C90.unk60 = *(CamState *)&D_80065C90.unk84;
        r = func_80025C0C();
        if (D_80065C90.unkC4 != r) {
            D_80065C90.unkC4 = r;
            SetGeomScreen(r);
        }
    }
    ns.unkA40 = 0;
    if (crash == NULL && ns.ldat->exec_map[0] != 0x6396347F) {
        GoolObjectCreate((GoolObject *)&game.handles[6], 0, 0, 0, NULL, 1);
        *(Vec3i *)crash->xform.trans = *(Vec3i *)ns.ldat->checkpoints[game.unk26C].pos;
        crash->zone = (Entry *)D_80065C90.unkCC;
        game.unk174 = -1;
        func_8002FF3C(crash, D_80065C90.unk148, 1, 0);
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/level", func_800215E8);
#endif

void func_8003505C(void *a);
/* Sets game.unkD8 from a and, unless flags bit 4, calls func_8003505C on
 * a + 0x24. */

void func_80021E60(s32 *a, s32 flags) {
    game.unkD8 = a[7];
    if (!(flags & 4)) {
        func_8003505C(a + 9);
    }
}
