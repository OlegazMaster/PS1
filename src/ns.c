#include "common.h"

/* NSF/NSD paging: loads level data from the disc (names follow c1). */

typedef struct {
    u32 unk0;
    u32 eid; /* 0x4 */
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
    u8 unk0[0x400];
    s32 unk400;
    s32 pte_count; /* 0x404 */
    u8 unk408[0x10];
    s32 unk418;
    u8 unk41C[0x4];
    u32 page_offsets[64]; /* 0x420 */
    NsdPte ptes[1]; /* 0x520 */
} Nsd;



typedef struct {
    s16 x, y, w, h;
} Rect16;

typedef struct PageStruct {
    void *page; /* 0x0 */
    s16 state; /* 0x4 */
    s16 type; /* 0x6 */
    s16 unk8;
    s16 unkA;
    s16 unkC;
    s8 ref_count; /* 0xE */
    s8 unkF;
    u8 unk10[0x4];
    s32 pgid; /* 0x14 */
    u32 eid; /* 0x18 */
    s32 unk1C;
    s32 *unk20;
    struct PageStruct *unk24;
    s32 timestamp; /* 0x28 */
} PageStruct;

typedef struct {
    s32 inited;
    u8 unk4[0x4];
    NsdPte **pte_buckets; /* 0x8: 256 hash buckets of the NSD */
    NsdPte *ptes; /* 0xC */
    u8 unk10[0x8];
    Nsd *nsd; /* 0x18 */
    u8 unk1C[0x4];
    s32 sector; /* 0x20: first sector of the NSF file */
    u8 unk24[0x4];
    s32 physical_page_count; /* 0x28 */
    PageStruct physical_pages[21]; /* 0x2C */
    s32 virtual_page_count; /* 0x3C8 */
    PageStruct virtual_pages[37]; /* 0x3CC */
    s32 page_count; /* 0xA28 */
    struct PageStruct *cur_ps; /* 0xA2C */
    s32 unkA30;
    s32 unkA34;
    s32 unkA38;
    PageStruct **page_map; /* 0xA3C */
    s32 unkA40;
} NsState;

extern NsState ns;
extern s32 ticks_elapsed;

extern u8 D_8005F408;


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
extern PageStruct D_80060D70[16];

typedef struct {
    u32 sector;
    u8 unk4[0x8];
} Unk8006C4D8;

extern Unk8006C4D8 D_8006C4D8[];

/* Per entry type: callbacks run when levels and pages are loaded and freed. */
typedef struct {
    u8 unk0[0x14];
    void (*on_free)(Entry *entry);
    void (*kill)(void);
} NsSubsystem;

extern NsSubsystem subsys[22];

extern char alpha_map[];
extern char eid_str_buf[];
extern char eid_str_buf2[];

int sprintf(char *buf, const char *fmt, ...);
int printf(const char *fmt, ...);
s32 CdSync(s32 mode, u8 *result);
void CdIntToPos(s32 sector, u8 *loc);
s32 CdControl(s32 com, u8 *param);
s32 CdRead(s32 sectors, void *buf, s32 mode);
s32 func_80013000(s32 arg0);
void NSPageDelete(PageStruct *ps);
NsdPte *NSProbe(u32 eid);
PageStruct *NSEntryPageStruct(u32 ref);
void *func_800140C0(u32 pgid, s32 count);
PageStruct *NSPageCreateVirtual(s32 pgid, u32 eid, s32 flag, s32 count);
s32 NSTexturePageFree(s32 idx);

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

typedef struct {
    u32 sector : 23;
    u32 unk0 : 9;
    u32 size : 17;
    u32 unk4 : 15;
} CdFile;

void *malloc(s32 size);
void *realloc(void *ptr, s32 size);
s32 CdControlB(s32 com, u8 *param, u8 *result);

/* Reads a whole file from the disc into buf (allocated when NULL). */
void *NSFileRead(CdFile *file, void *buf, s32 mode) {
    u8 result[8];
    u8 loc[8];
    s32 alloc_size;
    CdFile *f;
    void *ret;

    ret = (void *)use_cd;
    if (ret != NULL) {
        ret = (void *)1;
        alloc_size = 0;
        if (mode == 1) {
            f = file;
            if (buf == NULL) {
                alloc_size = (f->size + 0x7FF) / 0x800 * 0x800;
                buf = malloc(alloc_size);
                if (buf == NULL) {
                    return (void *)-0xF;
                }
            }
        retry:
            CdIntToPos(f->sector, loc);
            if (!CdControlB(2, loc, result)) {
                printf("hrce\n");
                goto retry;
            }
            if (!CdRead((f->size + 0x7FF) / 0x800, buf, 0x80) || func_80013000(1) == -0x10) {
                printf("hde\n");
                goto retry;
            }
            if (alloc_size != 0 && alloc_size != f->size) {
                buf = realloc(buf, f->size);
            }
            ret = buf;
        }
    }
    return ret;
}

/* .sdata continues after the literals of NSFileRead. */
Rect16 tpage_rect = { 0, 0, 256, 128 };
char D_800608C0[] = "de ls\n";
char D_800608C8[] = "de l\n";
#ifndef NON_MATCHING
/* The "!mem\n" literal of NSPageAllocate, which is still assembly in matching builds. */
char D_800608D0[] = "!mem\n";
#endif

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

s32 NSPageFree(PageStruct *ps) {
    Page *page;
    Entry *entry;
    NsdPte *pte;
    s32 pgid;
    u32 i;

    if (ns.cur_ps == ps) {
        ns.cur_ps = NULL;
    }
    if (ps->state == 20 || ps->state == 21) {
        page = ps->page;
        if (page->type != 1 && page->type != 3 && page->type != 4) {
            goto free_entries;
        }
        if (ps->state == 20 || ps->state == 21) {
            ns.page_map[ps->pgid >> 1] = (PageStruct *)-0x12;
        }
    }
    ps->state = 1;
    ps->unkF = 0;
    ps->ref_count = 0;
    return -0xFF;

free_entries:
    pte = ns.pte_buckets[(page->entries[0]->eid >> 15) & 0xFF];
    pgid = ps->pgid;
    for (i = 0; i < ((Page *)ps->page)->entry_count; i++) {
        entry = ((Page *)ps->page)->entries[i];
        while (pte->eid != entry->eid) {
            pte++;
        }
        if (subsys[entry->type].on_free != NULL) {
            subsys[entry->type].on_free(entry);
        }
        pte->value = pgid;
    }
    NSPageDelete(ps);
    return -0xFF;
}

PageStruct *NSPageCreatePhysical(PageStruct *ps, s32 pgid, u32 eid, s32 *count, s32 flag);
s32 func_800132F8(PageStruct *ps, s32 pgid);

/* Starts reading the next run of virtual pages (lowest pgid first, or the
 * requested one) into physical pages, then advances every physical page. */
void NSUpdate(s32 pgid) {
    PageStruct *ps, *next;
    s32 i, idx, count;
    s32 pgid_min, ps_pgid;

    if (ns.page_count == 0) {
        return;
    }
    if (ns.cur_ps == NULL) {
        idx = -1;
        for (i = 0; i < ns.virtual_page_count; i++) {
            ps = &ns.virtual_pages[i];
            if (ps->state == 2) {
                ps_pgid = ps->pgid;
                if ((pgid == -1 && (idx == -1 || ps_pgid < pgid_min)) || ps_pgid == pgid) {
                    pgid_min = ps_pgid;
                    idx = i;
                    if (pgid_min == pgid) {
                        break;
                    }
                }
            }
        }
        if (idx != -1) {
            count = 1;
            if (use_cd) {
                while ((pgid_min >> 1) < ns.nsd->unk400 - 1) {
                    pgid_min = (((pgid_min >> 1) + 1) << 1) | 1; /* pgid of the next page */
                    next = ns.page_map[pgid_min >> 1];
                    if ((s32)next < -0xFE && next->type == 0) {
                        count++;
                    } else {
                        break;
                    }
                }
            }
            NSPageCreatePhysical(&ns.virtual_pages[idx], ns.virtual_pages[idx].pgid, 0x6396347F, &count, pgid != -1);
        }
    }
    for (i = 0; i < ns.physical_page_count; i++) {
        func_800132F8(&ns.physical_pages[i], pgid);
    }
}

typedef struct {
    u8 unk0[0x10];
    u8 *unk10;
} Unk80065D5C;

typedef struct {
    u8 unk0[0x1C];
    s32 unk1C;
} Unk80061A7C;

typedef struct {
    u8 unk0[0xCC];
    Unk80065D5C *unkCC;
} Unk80065C90;

extern Unk80065C90 D_80065C90;
extern s16 D_80060B1C;
extern s32 D_800691FC;
extern s32 D_800608E8;
extern Unk80061A7C *D_80061A7C;
void func_80035078(u8 *);
void func_80016634(s32);

void func_80012EAC(void) {
    ns.unkA40 = 1;
    while (ns.page_count != 0 || D_800691FC >= 0) {
        if (D_80065C90.unkCC != NULL && D_80060B1C != -1) {
            func_80035078(D_80065C90.unkCC->unk10 + 0x2E4);
        }
        NSUpdate(-1);
        if (D_800691FC >= 0 && D_800608E8 != D_80061A7C->unk1C) {
            func_80016634(1);
        }
    }
}


void NSCdSeek(s32 idx) {
    u8 loc[4];

    if (use_cd && ns.cur_ps == 0 && CdSync(1, NULL) == 0) {
        CdIntToPos(D_8006C4D8[idx].sector & 0x7FFFFF, loc);
        CdControl(0x15, loc);
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

s32 NSCdRead(PageStruct *ps, s32 count) {
    u8 loc[16];
    s32 pgid = ps->pgid;
    s32 size;
    void *buf;
    s32 sector;
    s32 status;

    if (ps->unkA != 0) {
        size = (count << 5) - ps->unk8;
        buf = (u8 *)ps->page + (ps->unk8 << 11);
        sector = ns.sector + (ns.nsd->page_offsets[pgid >> 1] >> 6);
    } else {
        size = count << 5;
        buf = ps->page;
        sector = ns.sector + ns.nsd->unk418 + ((pgid >> 1) << 5);
    }
retry:
    CdIntToPos(sector, loc);
    if (!CdControl(2, loc)) {
        goto retry;
    }
    if ((status = CdSync(1, NULL)) ? status == 5 : CdSync(0, NULL) == 5) {
        goto retry;
    }
    if (!CdRead(size, buf, 0x80)) {
        goto retry;
    }
    return -0xFF;
}

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

s32 NSPageFree(PageStruct *ps);

/* Finds the longest run of free (or evictable, preferring the oldest) physical pages. */
#ifdef NON_MATCHING
/* Equivalent; the original computes the address of ns once for both uses. */
s32 NSPageAllocate(s32 *count, s32 flag) {
    s32 page_count;
    s32 wanted;
    s32 i;
    s32 len;
    u32 age;
    s32 idx_max = -1;
    s32 len_max;
    u32 age_max;
    PageStruct *ps;

    i = 0;
    page_count = ns.physical_page_count;
    if (page_count > 0) {
        wanted = *count;
        age = 0;
    next_run:
        len = 0;
    next_page:
        if (len < (wanted < page_count - i ? wanted : page_count - i)) {
            ps = &ns.physical_pages[i + len];
            if (ps->state != 1) {
                if (!(ps->state == 20 || ps->state == 21) || ps->unkF > 0) {
                    goto end_run;
                }
                age += 1 + ps->timestamp;
            }
            if (idx_max == -1 || len + 1 > len_max || (len + 1 == len_max && age >= age_max)) {
                idx_max = i;
                len_max = len + 1;
                age_max = age;
            }
            len++;
            goto next_page;
        }
    end_run:
        i++;
        age = 0;
        if (i < page_count) {
            goto next_run;
        }
    }
    if (idx_max != -1) {
        for (i = idx_max; i < idx_max + len_max; i++) {
            NSPageFree(&ns.physical_pages[i]);
        }
        *count = len_max;
        return idx_max;
    }
    *count = 0;
    if (flag) {
        printf("!mem\n");
    }
    return -1;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/ns", NSPageAllocate);
#endif

s32 NSTexturePageAllocate(void) {
    s32 i;

    for (i = 15; i >= 0; i--) {
        if (D_80060D70[i].state == 1) {
            return i;
        }
    }
    for (i = 15; i >= 0; i--) {
        if (D_80060D70[i].state == 21) {
            return i;
        }
    }
    for (i = 15; i >= 0; i--) {
        if ((D_80060D70[i].state == 20 || D_80060D70[i].state == 21) && D_80060D70[i].unkF == 0) {
            NSTexturePageFree(i);
            return i;
        }
    }
    return -0xC;
}

void func_800181E0(u32 eid, s32 arg1);

#ifdef NON_MATCHING
/* Equivalent; the original loads the array address before scaling the index. */
s32 NSTexturePageFree(s32 idx) {
    PageStruct *ps;

    ps = &D_80060D70[idx];
    switch (ps->state) {
    case 20:
        func_800181E0(ps->eid, 0);
        *ps->unk20 = ps->pgid;
        ns.page_map[ps->pgid >> 1] = (PageStruct *)-0x12;
        /* fallthrough */
    case 21:
        D_80060D70[idx].state = 1;
        break;
    }
    return -0xE;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/ns", NSTexturePageFree);
#endif

void func_80013B00(s32 idx, s32 flag) {
    if (flag) {
        NSTexturePageFree(idx);
        D_80060D70[idx].state = 0x1E;
    } else if (D_80060D70[idx].state == 0x1E) {
        D_80060D70[idx].state = 1;
    }
}

s32 func_80013B8C(s32 type, s32 arg1) {
    s32 i;

    for (i = 9; i >= 0; i--) {
        if (D_80060BB8[i].state == 1 && D_80060BB8[i].type == type &&
            (arg1 == -1 || arg1 == D_80060BB8[i].unkC)) {
            return i;
        }
    }
    if (type == 5) {
        for (i = 9; i >= 0; i--) {
            if ((D_80060BB8[i].state == 20 || D_80060BB8[i].state == 21) &&
                D_80060BB8[i].type == type && D_80060BB8[i].unkF == 0) {
                return i;
            }
        }
    }
    return -0xC;
}

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

s32 NSPageAllocate(s32 *count, s32 flag);

#ifdef NON_MATCHING
/* Equivalent; the original keeps the loop constants out of registers. */
PageStruct *NSPageCreatePhysical(PageStruct *ps, s32 pgid, u32 eid, s32 *count, s32 flag) {
    PageStruct *first;
    PageStruct *dst;
    s32 idx;
    s32 i;

    idx = NSPageAllocate(count, flag);
    if (idx == -1) {
        return (PageStruct *)-0xC;
    }
    dst = &ns.physical_pages[idx + *count - 1];
    if (ps != NULL) {
        first = dst;
        pgid = ps->pgid >> 1;
        for (i = *count - 1; i >= 0; i--, dst--) {
            ps = ns.page_map[pgid + i];
            dst->pgid = ps->pgid;
            if (eid != 0x6396347F) {
                dst->eid = eid;
            } else {
                dst->eid = ps->eid;
            }
            ps->state = 1;
            dst->unkF = ps->unkF;
            ps->unkF = 0;
            dst->ref_count = ps->ref_count;
            dst->timestamp = ps->timestamp;
            dst->unk24 = first;
            dst->unk1C = *count;
            if (i == *count - 1) {
                dst->state = 2;
            } else {
                dst->state = 12;
            }
            ns.page_map[dst->pgid >> 1] = dst;
        }
        return first;
    }
    return dst;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/ns", NSPageCreatePhysical);
#endif

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


s32 func_80014024(s32 pgid, s32 flag, s32 count, u32 eid) {
    PageStruct *ps;

retry:
    ps = NSPageOpen(pgid, eid, flag, count);
    if (ps == (PageStruct *)-0xFF) {
        return 1;
    }
    if ((flag & 1) && !(ps->state == 20 || ps->state == 21)) {
        NSUpdate(pgid);
        count = 0;
        goto retry;
    }
    return ps->unkF;
}

void *func_800140C0(u32 pgid, s32 count) {
    PageStruct *ps = ns.page_map[(s32)pgid >> 1];
    s32 i;

    if ((s32)ps >= -0xFE) {
        return NULL;
    }
    if (ps == (PageStruct *)-0xFF || ps->type == 3 || ps->type == 5) {
        return (void *)1;
    }
    if (count < 0) {
        NSPageDecRef(ps);
    } else {
        for (i = 0; i < count; i++) {
            func_80013E64(ps);
        }
    }
    return (void *)(ps->unkF + 1);
}

PageStruct *NSEntryPageStruct(u32 ref) {
    s32 i;
    s32 idx;

    if (ref & 2) {
        if ((ref & 0xC0000002) == 0x80000002) {
            for (i = 15; i >= 0; i--) {
                if ((D_80060D70[i].state == 20 || D_80060D70[i].state == 21) && ref == D_80060D70[i].unk1C) {
                    return &D_80060D70[i];
                }
            }
        }
        return NULL;
    }
    idx = (s32)(ref - (u32)ns.physical_pages[0].page) / 0x10000;
    if (idx >= 0 && idx < ns.physical_page_count) {
        return &ns.physical_pages[idx];
    }
    return NULL;
}

void *NSEntryPage(void *en_ref) {
    PageStruct *ps = NSEntryPageStruct((u32)en_ref);

    if (ps == NULL) {
        return NULL;
    }
    if (ps->type != 1) {
        return NULL;
    }
    return ps->page;
}

s32 func_80014024(s32 pgid, s32 flag, s32 count, u32 eid);

#ifdef NON_MATCHING
/* Equivalent; only the register of the final load differs. */
u32 NSOpen(u32 *en_ref, s32 flag, s32 count) {
    NsdPte *pte;
    PageStruct *ps;

    if (!(flag & 2)) {
        if (*en_ref == 0x6396347F) {
            return -0xA;
        }
        if (*en_ref & 1) {
            pte = NSProbe(*en_ref);
            *en_ref = (u32)pte;
            en_ref = (u32 *)pte;
        } else {
            en_ref = (u32 *)*en_ref;
        }
    }
    if (!(((NsdPte *)en_ref)->value & 1)) {
        if (count != 0) {
            ps = NSEntryPageStruct(((NsdPte *)en_ref)->value);
            if (ps != NULL) {
                func_80014024(ps->pgid, flag, count, ((NsdPte *)en_ref)->eid);
            }
        }
        return ((NsdPte *)en_ref)->value;
    }
    func_80014024(((NsdPte *)en_ref)->value, flag, count, ((NsdPte *)en_ref)->eid);
    if (((NsdPte *)en_ref)->value & 1) {
        return 0;
    }
    return ((NsdPte *)en_ref)->value;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/ns", NSOpen);
#endif

void *func_8001439C(NsdPte *pte, s32 count) {
    u32 value = pte->value;
    PageStruct *ps;

    if (!(value & 1)) {
        if (count == 0) {
            return pte;
        }
        ps = NSEntryPageStruct(value);
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

void func_80013EFC(PageStruct *ps);

void func_80014468(void) {
    PageStruct *ps;
    s32 i;

    for (i = 0, ps = ns.virtual_pages; i < ns.virtual_page_count; ps++, i++) {
        if (ps->state != 1) {
            func_80013EFC(ps);
        }
    }
    ps = ns.physical_pages;
    for (i = ns.physical_page_count - 1; i >= 0; i--, ps++) {
        if (ps->state != 1) {
            func_80013EFC(ps);
        }
    }
    for (ps = D_80060D70, i = 15; i >= 0; i--, ps++) {
        if (ps->state == 20 || ps->state == 21) {
            func_80013EFC(ps);
        }
    }
    for (ps = D_80060BB8, i = 9; i >= 0; i--, ps++) {
        if (ps->state == 20 || ps->state == 21) {
            func_80013EFC(ps);
        }
    }
}

s32 NSCountAvailablePages(void) {
    PageStruct *ps;
    s32 count;
    s32 i;

    count = ns.physical_page_count;
    for (ps = ns.virtual_pages, i = ns.virtual_page_count - 1; i >= 0; i--, ps++) {
        if (ps->state != 1 && ps->unkF != 0) {
            count--;
        }
    }
    for (ps = ns.physical_pages, i = 0; i < ns.physical_page_count; ps++, i++) {
        if (ps->state != 1 && ps->unkF != 0) {
            count--;
        }
    }
    return count;
}

void func_80014650(u32 *bits) {
    PageStruct *ps;
    s32 i;
    s32 idx;

    for (i = 7; i >= 0; i--) {
        bits[i] = 0;
    }
    ps = ns.virtual_pages;
    for (i = ns.virtual_page_count - 1; i >= 0; i--, ps++) {
        if (ps->state != 1 && ps->unkF != 0) {
            idx = ps->pgid >> 1;
            bits[idx / 32] |= 1 << (idx % 32);
        }
    }
    ps = ns.physical_pages;
    for (i = ns.physical_page_count - 1; i >= 0; i--, ps++) {
        if (ps->state != 1 && ps->unkF != 0) {
            idx = ps->pgid >> 1;
            bits[idx / 32] |= 1 << (idx % 32);
        }
    }
}

void func_80014788(u32 *bits) {
    PageStruct *ps;
    s32 i;
    s32 idx;

    ps = ns.virtual_pages;
    for (i = ns.virtual_page_count - 1; i >= 0; ps++, i--) {
        if (ps->state != 1 && ps->unkF != 0) {
            idx = ps->pgid >> 1;
            if (!(bits[idx / 32] & (1 << (idx % 32)))) {
                func_80013EFC(ps);
            }
        }
    }
    ps = ns.physical_pages;
    for (i = ns.physical_page_count - 1; i >= 0; ps++, i--) {
        if (ps->state != 1 && ps->unkF != 0) {
            idx = ps->pgid >> 1;
            if (!(bits[idx / 32] & (1 << (idx % 32)))) {
                func_80013EFC(ps);
            }
        }
    }
}

/* Number of pages that loading the given entries would need. */
s32 NSCountAvailablePages2(u32 **list, s32 len) {
    s32 cache[40];
    s32 cache_size = 0;
    s32 count = 0;
    s32 i;
    s32 ii;
    u32 value;
    NsdPte *pte;
    PageStruct *ps;

    for (i = len - 1; i >= 0; i--) {
        value = *list[i];
        if (value & 1) {
            pte = NSProbe(value);
            *list[i] = (u32)pte;
        } else {
            pte = (NsdPte *)value;
        }
        if (!(pte->value & 1)) {
            ps = NSEntryPageStruct(pte->value);
            if (ps == NULL || ps->type == 2) {
                continue;
            }
            if (ps->state != 1 && ps->unkF != 0) {
                continue;
            }
            for (ii = 0; ii < cache_size; ii++) {
                if (ps->pgid == cache[ii]) {
                    break;
                }
            }
            if (ii != cache_size) {
                continue;
            }
            count++;
            cache[cache_size++] = ps->pgid;
        } else {
            ps = ns.page_map[(s32)pte->value >> 1];
            if (ps == (PageStruct *)-0x12) {
                for (ii = 0; ii < cache_size; ii++) {
                    if (pte->value == cache[ii]) {
                        break;
                    }
                }
                if (ii != cache_size) {
                    continue;
                }
                count++;
                cache[cache_size++] = pte->value;
            } else if (ps != (PageStruct *)-0xFF && ps->type < 2 && ps->unkF == 0) {
                for (ii = 0; ii < cache_size; ii++) {
                    if (ps->pgid == cache[ii]) {
                        break;
                    }
                }
                if (ii != cache_size) {
                    continue;
                }
                count++;
                cache[cache_size++] = ps->pgid;
            }
        }
    }
    return count;
}

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

void free(void *ptr);
void func_800519C4(s32);
void func_800351EC(void);
void func_80033238(void);
void NSKillPage(NsState *nss, s32 idx);

s32 NSKill(NsState *nss) {
    s32 i;

    if (nss->inited != 1) {
        return -0xFF;
    }
    if (use_cd && ns.cur_ps != 0) {
        func_80013000(1);
    }
    if (ns.unkA34 != 0) {
        func_800519C4(1);
    }
    func_800351EC();
    func_80033238();
    for (i = 0; i < 22; i++) {
        if (subsys[i].kill != NULL) {
            subsys[i].kill();
        }
    }
    for (i = 0; i < nss->physical_page_count; i++) {
        NSKillPage(nss, i);
    }
    free(nss->physical_pages[0].page);
    free(nss->page_map);
    free(nss->nsd);
    nss->inited = 0;
    return -0xFF;
}

#define ABS(x) ((x) < 0 ? -(x) : (x))

/* Lays out the audio pages in sound RAM, downwards from 512 KB. */
void NSInitAudioPages(s32 arg0, s32 reserved, s32 size, s32 inst_size, s32 n) {
    s32 addr = 0x80000 - reserved;
    s32 pages = (size + 0xFF8F) / 0xFF90;
    s32 saved;
    s32 i;

    for (i = pages - 1; i >= 0; i--) {
        D_80060BB8[i].unkC = i;
        D_80060BB8[i].state = 1;
        D_80060BB8[i].type = 4;
        if (i == pages - 1) {
            addr -= (size % 0xFF90 + 0xF) / 16 * 16;
        } else {
            addr -= 0xFF90;
        }
        D_80060BB8[i].unk1C = addr;
    }
    ns.unkA30 = addr;
    saved = addr;
    for (i = pages; i < pages + ABS(n); i++) {
        D_80060BB8[i].unkC = i;
        D_80060BB8[i].state = 1;
        D_80060BB8[i].type = 5;
        D_80060BB8[i].eid = 0x6396347F;
        D_80060BB8[i].unk1C = addr -= inst_size;
    }
    if (n < 0) {
        addr = saved;
    }
    for (; i < 10; i++) {
        D_80060BB8[i].unkC = i;
        D_80060BB8[i].type = 3;
        if (addr - 0x10000 < 0x1010) {
            D_80060BB8[i].state = 30;
        } else {
            addr -= 0x10000;
            D_80060BB8[i].state = 1;
            D_80060BB8[i].unk1C = addr;
        }
    }
}

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

void NSKillPage(NsState *nss, s32 idx) {
    if (nss->physical_pages[idx].state != 0) {
        nss->physical_pages[idx].state = 0;
    }
}

u8 func_8001549C(void) {
    return D_8005F408;
}
