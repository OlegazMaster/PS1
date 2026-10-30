#include "common.h"

/* NSF/NSD paging: loads level data from the disc (names follow c1). */

typedef struct {
    u8 unk0[0x8];
    s32 type; /* 0x8 */
    s32 item_count; /* 0xC */
    u8 *items[1]; /* 0x10: item_count + 1 offsets */
} Entry;

/* An NSD page table entry: either an entry pointer or an EID (low bits set). */
typedef struct {
    u32 value;
    u32 eid;
} NsdPte;

typedef struct {
    u8 unk0[0x404];
    s32 pte_count; /* 0x404 */
    u8 unk408[0x118];
    NsdPte ptes[1]; /* 0x520 */
} Nsd;



typedef struct {
    s16 x, y, w, h;
} Rect16;

typedef struct PageStruct {
    void *page; /* 0x0 */
    s16 state; /* 0x4 */
    s16 type; /* 0x6 */
    u8 unk8[0x6];
    s8 ref_count; /* 0xE */
    s8 unkF;
    u8 unk10[0x4];
    s32 pgid; /* 0x14 */
    u32 eid; /* 0x18 */
    s32 unk1C;
    u8 unk20[0x4];
    struct PageStruct *unk24;
    s32 timestamp; /* 0x28 */
} PageStruct;

typedef struct {
    u8 unk0[0x8];
    NsdPte **pte_buckets; /* 0x8: 256 hash buckets of the NSD */
    NsdPte *ptes; /* 0xC */
    u8 unk10[0x8];
    Nsd *nsd; /* 0x18 */
    u8 unk1C[0xC];
    s32 physical_page_count; /* 0x28 */
    PageStruct physical_pages[21]; /* 0x2C */
    s32 virtual_page_count; /* 0x3C8 */
    PageStruct virtual_pages[37]; /* 0x3CC */
    s32 page_count; /* 0xA28 */
    s32 unkA2C;
    u8 unkA30[0xC];
    PageStruct **page_map; /* 0xA3C */
} NsState;

extern NsState ns;
extern s32 ticks_elapsed;

extern u8 D_8005F408;

typedef struct {
    s16 unk0;
    u8 unk2[0x2A];
} Unk2C;

/* A 64 KB page of an NSF file. */
typedef struct {
    u16 magic; /* 0x1234 */
    u16 type;
    u32 id;
    s32 entry_count; /* 0x8 */
    s32 unkC;
    Entry *entries[1]; /* 0x10: entry_count + 1 offsets, then pointers */
} Page;

extern PageStruct D_80060BB8[10];
extern PageStruct D_80060D70[];

typedef struct {
    u32 sector;
    u8 unk4[0x8];
} Unk8006C4D8;

extern Unk8006C4D8 D_8006C4D8[];

typedef struct {
    u8 unk0[0x30];
    Unk2C items[1];
} Unk15468;

extern char alpha_map[];
extern char eid_str_buf[];
extern char eid_str_buf2[];

int sprintf(char *buf, const char *fmt, ...);
void NSPageDelete(PageStruct *ps);
NsdPte *NSProbe(u32 eid);
PageStruct *NSEntryPageStruct(void *en_ref);
void *func_800140C0(u32 pgid, s32 count);
PageStruct *NSPageCreateVirtual(s32 pgid, u32 eid, s32 flag, s32 count);

char *NSEIDToString(u32 eid) {
    s32 i;

    if (eid & 1) {
        eid_str_buf[5] = 0;
        eid >>= 1;
        for (i = 4; i >= 0; i--) {
            eid_str_buf[i] = alpha_map[eid & 0x3F];
            eid >>= 6;
        }
    } else {
        sprintf(eid_str_buf, "[%X]", eid);
    }
    return eid_str_buf;
}

char *NSEIDValToString(s32 eid) {
    if (eid & 1) {
        sprintf(eid_str_buf2, "p%d", eid >> 1);
    } else {
        sprintf(eid_str_buf2, "[%X]", eid);
    }
    return eid_str_buf2;
}

#ifdef NON_MATCHING
/* Equivalent; the original's alphabet search loop keeps its exit test at the top. */
u32 NSStringToEID(char *str) {
    s32 i;
    s32 n;
    u32 eid = 0;
    u8 c;

    for (n = 5; n != 0; n--) {
        c = *str++;
        if (c == 0) {
            while (n--) {
                eid = (eid << 6) | 0x3E;
            }
            return (eid << 1) | 1;
        }
        for (i = 0; i < 64; i++) {
            if (alpha_map[i] == c) {
                break;
            }
        }
        if (i == 64) {
            i = 0x3E;
        }
        eid = (eid << 6) | i;
    }
    return (eid << 1) | 1;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/ns", NSStringToEID);
#endif

/* Entry type from the last character of an EID. */
s32 NSEIDTypeFromEID(u32 eid) {
    switch (alpha_map[(eid >> 1) & 0x3F]) {
    case 'V':
        return 1;
    case 'S':
        return 4;
    case 'G':
        return 2;
    case 'W':
        return 3;
    case 'T':
        return 5;
    case 'L':
        return 6;
    case 'Z':
        return 7;
    case 'C':
        return 11;
    case 'A':
        return 12;
    case 'O':
        return 20;
    case 'M':
        return 13;
    case 'I':
        return 16;
    case 'N':
        return 14;
    case 'R':
        return 17;
    case 'U':
        return 18;
    case 'B':
        return 19;
    case 'D':
        return 21;
    case 'X':
        return 15;
    }
    return 0;
}

char NSAlphaNumChar(s32 idx) {
    return alpha_map[idx];
}

s32 NSAlphaNumIndex(char c) {
    s32 i;

    for (i = 63; i >= 0; i--) {
        if (alpha_map[i] == c) {
            return i;
        }
    }
    if (c == '-') {
        return 99;
    }
    return -14;
}

char *func_80012288(void) {
    return "";
}

char *func_80012294(void) {
    return "";
}

/* .sdata, in the order of the original. The strings stand in for the literals
 * of functions that are still assembly. */
s32 use_cd = 1;
char D_800608A8[] = "hrce\n";
char D_800608B0[] = "hde\n";
Rect16 tpage_rect = { 0, 0, 256, 128 };
char D_800608C0[] = "de ls\n";
char D_800608C8[] = "de l\n";
char D_800608D0[] = "!mem\n";

s32 NSEIDType(NsdPte *pte) {
    if (!(pte->value & 3)) {
        return ((Entry *)pte->value)->type;
    }
    return NSEIDTypeFromEID(pte->eid);
}

s32 NSCountEntries(NsState *nss, s32 type) {
    Nsd *nsd = nss->nsd;
    NsdPte *pte = nsd->ptes;
    s32 count = 0;
    s32 i;

    for (i = nsd->pte_count - 1; i >= 0; i--, pte++) {
        if (NSEIDType(pte) == type) {
            count++;
        }
    }
    return count;
}

#ifdef NON_MATCHING
static inline s32 PageEntryOffset(Page *page, s32 idx) {
    s32 value = (s32)page->entries[idx];

    if (value < 0) {
        return value - (s32)page;
    }
    return value;
}
#endif

#ifdef NON_MATCHING
/* Equivalent; the original's case blocks are laid out in another order. */
s32 func_8001235C(Page *page) {
    switch (page->type) {
    case 0:
    case 3:
    case 4:
    case 5:
        return PageEntryOffset(page, page->entry_count);
    case 1:
        return 0x10000;
    case 0x100:
        return PageEntryOffset(page, page->unkC);
    }
    return -0xE;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/ns", func_8001235C);
#endif

s32 NSEntryItemSize(Entry *entry, s32 idx) {
    if (entry->item_count != 0) {
        return entry->items[idx + 1] - entry->items[idx];
    }
    return 0;
}

INCLUDE_ASM("asm/us/nonmatchings/ns", func_8001242C);

#ifdef NON_MATCHING
/* Equivalent; the original stores the entry pointer from the register it is computed in. */
s32 NSPageTranslateOffsets(Page *page) {
    s32 count = page->entry_count;
    Entry **entries = page->entries;
    s32 i;
    s32 ii;
    Entry *entry;
    u8 **items;

    if (page->type == 1) {
        return -0xFF;
    }
    if (page->magic != 0x1234) {
        return -0x12;
    }
    for (i = count - 1; i >= 0; i--) {
        entry = entries[i] = (Entry *)((u32)page + (u32)entries[i]);
        items = entry->items;
        for (ii = entry->item_count; ii >= 0; ii--) {
            items[ii] = (u8 *)((u32)entry + (u32)items[ii]);
        }
    }
    entries[count] = (Entry *)((u32)page + (u32)entries[count]);
    return -0xFF;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/ns", NSPageTranslateOffsets);
#endif

u32 NSChecksum(u32 *buf, s32 size) {
    u32 checksum = 0x12345678;
    s32 i;
    u32 word;

    for (i = 0; i < size / 4; i++) {
        word = *buf++;
        checksum += word & 0xFF;
        checksum = (checksum << 3) | (checksum >> 29);
        checksum += (word >> 8) & 0xFF;
        checksum = (checksum << 3) | (checksum >> 29);
        checksum += (word >> 16) & 0xFF;
        checksum = (checksum << 3) | (checksum >> 29);
        checksum += word >> 24;
        checksum = (checksum << 3) | (checksum >> 29);
    }
    return checksum;
}

INCLUDE_RODATA("asm/us/nonmatchings/ns", D_80010068);

INCLUDE_RODATA("asm/us/nonmatchings/ns", D_80010088);

INCLUDE_RODATA("asm/us/nonmatchings/ns", D_800100A8);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800126D0);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80012B00);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80012CBC);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80012EAC);

s32 func_800459AC(s32, s32);
void func_80045E68(u32, u8 *);
void func_80045B54(s32, u8 *);

void NSCdSeek(s32 idx) {
    u8 loc[4];

    if (use_cd && ns.unkA2C == 0 && func_800459AC(1, 0) == 0) {
        func_80045E68(D_8006C4D8[idx].sector & 0x7FFFFF, loc);
        func_80045B54(0x15, loc);
    }
}

s32 func_80047F44(s32, s32);
s32 func_80045E48(s32);

s32 func_80013000(s32 arg0) {
    s32 ret = func_80047F44(arg0 == 0, 0);

    if (ret < 0) {
        return -0x10;
    }
    if (ret != 0) {
        return 0;
    }
    ret = func_80045E48(arg0 == 0);
    if (ret < 0) {
        return -0x10;
    }
    return ret == 0;
}

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013050);

s32 func_80013158(PageStruct *ps, s32 pgid) {
    s32 last;

    if (pgid == -1) {
        return 1;
    }
    pgid >>= 1;
    last = ps->unk24->pgid >> 1;
    if (pgid <= last) {
        if (pgid > last - ps->unk1C) {
            return 1;
        }
    }
    return 0;
}

s32 func_800131B0(PageStruct *ps) {
    ps->state = 2;
    ps->unk1C = 1;
    ps->unk24 = ps;
    return -0x10;
}

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800131CC);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800132F8);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800137AC);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_8001395C);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013A30);

void func_80013A30(s32 idx);

void func_80013B00(s32 idx, s32 flag) {
    if (flag) {
        func_80013A30(idx);
        D_80060D70[idx].state = 0x1E;
    } else if (D_80060D70[idx].state == 0x1E) {
        D_80060D70[idx].state = 1;
    }
}

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013B8C);

#ifdef NON_MATCHING
/* Equivalent; GCC picks other registers for the page pointer and counter. */
PageStruct *NSPageCreateVirtual(s32 pgid, u32 eid, s32 flag, s32 count) {
    NsState *nss = &ns;
    PageStruct *ps;
    s32 i;

    for (i = 0, ps = nss->virtual_pages; i < nss->virtual_page_count; i++, ps++) {
        if (ps->state == 1) {
            ps->state = 2;
            ps->pgid = pgid;
            ps->unkF = count;
            if (flag & 4) {
                ps->ref_count = count;
            } else {
                ps->ref_count = 0;
            }
            ps->eid = eid;
            ps->timestamp = ticks_elapsed;
            nss->page_map[pgid >> 1] = ps;
            nss->page_count++;
            return ps;
        }
    }
    return NULL;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/ns", NSPageCreateVirtual);
#endif

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013D04);

void func_80013E64(PageStruct *ps) {
    if (ps->unkF > 0) {
        if (--ps->unkF <= 0 && ps->type == 0) {
            NSPageDelete(ps);
            ns.page_count--;
        }
    }
}


void NSPageDecRef(PageStruct *ps) {
    if (ps->ref_count != 0) {
        ps->ref_count--;
        func_80013E64(ps);
    }
}

void func_80013EFC(PageStruct *ps) {
    if (ps->ref_count != 0) {
        ps->unkF -= ps->ref_count;
        ps->ref_count = 0;
        if (ps->unkF <= 0 && ps->type == 0) {
            NSPageDelete(ps);
            ns.page_count--;
        }
    }
}

void NSPageDelete(PageStruct *ps) {
    ps->state = 1;
    ps->unkF = 0;
    ps->ref_count = 0;
    ns.page_map[ps->pgid >> 1] = (PageStruct *)-0x12;
}

PageStruct *NSPageOpen(s32 pgid, u32 eid, s32 flag, s32 count) {
    PageStruct *ps = ns.page_map[pgid >> 1];

    if (ps != (PageStruct *)-0x12) {
        if (ps != (PageStruct *)-0xFF) {
            if (flag & 4) {
                ps->ref_count += count;
            }
            ps->eid = eid;
            ps->unkF += count;
        }
        return ps;
    }
    return NSPageCreateVirtual(pgid, eid, flag, count);
}

void func_80012CBC(s32 pgid);

s32 func_80014024(s32 pgid, s32 flag, s32 count, u32 eid) {
    PageStruct *ps;

retry:
    ps = NSPageOpen(pgid, eid, flag, count);
    if (ps == (PageStruct *)-0xFF) {
        return 1;
    }
    if ((flag & 1) && !(ps->state == 20 || ps->state == 21)) {
        func_80012CBC(pgid);
        count = 0;
        goto retry;
    }
    return ps->unkF;
}

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800140C0);

INCLUDE_ASM("asm/us/nonmatchings/ns", NSEntryPageStruct);

void *NSEntryPage(void *en_ref) {
    PageStruct *ps = NSEntryPageStruct(en_ref);

    if (ps == NULL) {
        return NULL;
    }
    if (ps->type != 1) {
        return NULL;
    }
    return ps->page;
}

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800142B4);

void *func_8001439C(NsdPte *pte, s32 count) {
    u32 value = pte->value;
    PageStruct *ps;

    if (!(value & 1)) {
        if (count == 0) {
            return pte;
        }
        ps = NSEntryPageStruct((void *)value);
        if (ps != NULL) {
            value = ps->pgid;
        } else {
            return (void *)1;
        }
    } else if (count == 0) {
        return NULL;
    }
    return func_800140C0(value, count);
}

void *func_8001440C(u32 *ref, s32 count) {
    u32 value = *ref;
    NsdPte *pte;

    if (value & 1) {
        pte = NSProbe(value);
        *ref = (u32)pte;
    } else {
        pte = (NsdPte *)value;
    }
    return func_8001439C(pte, count);
}

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80014468);

#ifdef NON_MATCHING
/* Equivalent; the original walks the virtual pages with a reversed counter. */
s32 NSCountAvailablePages(void) {
    s32 count = ns.physical_page_count;
    s32 i;

    for (i = 0; i < ns.virtual_page_count; i++) {
        if (ns.virtual_pages[i].state != 1 && ns.virtual_pages[i].unkF != 0) {
            count--;
        }
    }
    for (i = 0; i < ns.physical_page_count; i++) {
        if (ns.physical_pages[i].state != 1 && ns.physical_pages[i].unkF != 0) {
            count--;
        }
    }
    return count;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/ns", NSCountAvailablePages);
#endif

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80014650);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80014788);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800148F8);

NsdPte *NSProbe(u32 eid) {
    NsdPte *pte = ns.pte_buckets[(eid >> 15) & 0xFF];

    while (pte->eid != eid) {
        pte++;
    }
    return pte;
}

NsdPte *NSProbeSafe(u32 eid) {
    NsdPte *pte = ns.pte_buckets[(eid >> 15) & 0xFF];

    while (pte->eid != eid) {
        if (++pte - ns.ptes >= ns.nsd->pte_count) {
            return (NsdPte *)-0xA;
        }
    }
    return pte;
}

INCLUDE_ASM("asm/us/nonmatchings/ns", NSInit);

INCLUDE_ASM("asm/us/nonmatchings/ns", NSKill);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80015230);

PageStruct *func_80015414(s32 arg0) {
    PageStruct *p = D_80060BB8;
    s32 i;

    for (i = 9; i >= 0; p++, i--) {
        if (p->type == 5 && p->eid == arg0) {
            return p;
        }
    }
    return NULL;
}

void func_80015468(Unk15468 *arg0, s32 idx) {
    if (arg0->items[idx].unk0 != 0) {
        arg0->items[idx].unk0 = 0;
    }
}

u8 func_8001549C(void) {
    return D_8005F408;
}
