#ifndef NS_H
#define NS_H

#include "types.h"

/* NSF/NSD paging: level data streamed from the disc (names follow c1). */

typedef struct Entry {
    u32 unk0;
    u32 eid; /* 0x4 */
    s32 type; /* 0x8 */
    s32 item_count; /* 0xC */
    u8 *items[1]; /* 0x10: item_count + 1 offsets */
} Entry;

/* The header of a zone entry (item 0); see also gool.c. */
typedef struct {
    u8 unk0[0x184];
    s32 first_item; /* 0x184: index of the first path item */
    s32 path_items; /* 0x188: 3 per camera path */
    s32 entity_count; /* 0x18C */
    s32 neighbor_count; /* 0x190 */
    u32 neighbors[16]; /* 0x194: references to the neighbor zones */
    u32 neighbor_flags[16]; /* 0x1D4 */
    u8 unk214[0xA8];
    u32 flags; /* 0x2BC: 1 loaded, 2 ... */
    u8 unk2C0[0xC];
    s32 unk2CC; /* 0x2CC: light of the zone's floor */
} ZoneHdr;

/* An NSD page table entry: either an entry pointer or an EID (low bits set). */
typedef struct {
    u32 value;
    u32 eid;
} NsdPte;

typedef struct {
    u8 unk0[0x400];
    s32 unk400;
    s32 pte_count; /* 0x404 */
    u8 unk408[0x10];
    s32 unk418;
    u8 unk41C[0x4];
    u32 page_offsets[64]; /* 0x420 */
    NsdPte ptes[1]; /* 0x520 */
} Nsd;

/* Level header stored after the NSD page table. */
/* A checkpoint (start point) of a level. */
typedef struct {
    u32 zone; /* reference to the zone */
    s32 path; /* camera path in the zone */
    s32 progress; /* 0x8 */
    s32 pos[3]; /* 0xC */
} Checkpoint; /* 0x18 */

typedef struct {
    s32 checkpoint_count; /* 0x0 */
    s32 unk4;
    s32 lid; /* 0x8: level id */
    u8 unkC[0x4];
    u32 exec_map[64]; /* 0x10: EIDs of the GOOL executables */
    u8 unk110[0x1CC];
    Checkpoint checkpoints[1]; /* 0x2DC */
} LevelData;

typedef struct PageStruct {
    void *page; /* 0x0 */
    s16 state; /* 0x4 */
    s16 type; /* 0x6 */
    s16 unk8;
    s16 unkA;
    s16 unkC;
    s8 ref_count; /* 0xE */
    s8 unkF;
    u16 *unk10;
    s32 pgid; /* 0x14 */
    u32 eid; /* 0x18 */
    s32 unk1C;
    s32 *unk20;
    struct PageStruct *unk24;
    s32 timestamp; /* 0x28 */
} PageStruct;

typedef struct {
    s32 inited;
    s32 lid; /* 0x4 */
    NsdPte **pte_buckets; /* 0x8: 256 hash buckets of the NSD */
    NsdPte *ptes; /* 0xC */
    u32 *unk10; /* 0x10 */
    LevelData *ldat; /* 0x14 */
    Nsd *nsd; /* 0x18 */
    u8 unk1C[0x4];
    s32 sector; /* 0x20: first sector of the NSF file */
    s32 draw_skip_counter; /* 0x24 */
    s32 physical_page_count; /* 0x28 */
    PageStruct physical_pages[21]; /* 0x2C */
    s32 virtual_page_count; /* 0x3C8 */
    PageStruct virtual_pages[37]; /* 0x3CC */
    s32 page_count; /* 0xA28 */
    struct PageStruct *cur_ps; /* 0xA2C */
    s32 unkA30;
    struct PageStruct *cur_audio_ps; /* 0xA34 */
    s32 unkA38;
    PageStruct **page_map; /* 0xA3C */
    s32 unkA40;
} NsState;

/* A 64 KB page of an NSF file. */
typedef struct {
    u16 magic; /* 0x1234 */
    u16 type;
    u32 id;
    u32 entry_count; /* 0x8 */
    s32 unkC;
    Entry *entries[1]; /* 0x10: entry_count + 1 offsets, then pointers */
} Page;

extern NsState ns;

/* A file on the disc, as stored in the file table. */
typedef struct {
    u32 sector : 23;
    u32 unk0 : 9;
    u32 size : 17;
    u32 unk4 : 15;
} CdFile;

/* An entry of the file table: the NSD of a level, then the sector of its NSF.
 * Entry 61 is the overlay S0/WARPSCUS.BIN. */
typedef struct {
    u32 sector;
    u32 unk4;
    u32 nsf_sector; /* 0x8 */
} Unk8006C4D8;
extern Unk8006C4D8 D_8006C4D8[];

/* Per entry type: callbacks run when levels and pages are loaded and freed. */
typedef struct {
    u8 unk0[0x8];
    void (*init)(void);
    void (*init2)(void);
    void (*on_load)(Entry *entry);
    void (*on_free)(Entry *entry);
    void (*kill)(void);
} NsSubsystem;
extern NsSubsystem subsys[22];

u32 NSOpen(u32 *en_ref, s32 flag, s32 count);
void *func_8001440C(u32 *ref, s32 count);
void *NSFileRead(CdFile *file, void *buf, s32 mode, s32 arg3);

#endif /* NS_H */
