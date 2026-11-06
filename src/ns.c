#include "common.h"
#include "ns.h"

/* NSF/NSD paging: loads level data from the disc (names follow c1). */

typedef struct {
    s16 x, y, w, h;
} Rect16;

extern s32 ticks_elapsed;

extern u8 D_8005F408;

extern PageStruct D_80060BB8[10];
extern PageStruct D_80060D70[16];





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

char *func_80012288(s32 type, s32 lid) {
    return "";
}

char *func_80012294(s32 type, s32 lid) {
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


void *malloc(s32 size);
void *realloc(void *ptr, s32 size);
s32 CdControlB(s32 com, u8 *param, u8 *result);

/* Reads a whole file from the disc into buf (allocated when NULL). */
void *NSFileRead(CdFile *file, void *buf, s32 mode, s32 arg3) {
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

/* Turns the entry and item offsets of a loaded page into pointers. */
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
        entry = (Entry *)((u32)page + (u32)entries[i]);
        entries[i] = entry;
        items = entry->items;
        for (ii = entry->item_count; ii >= 0; ii--) {
            items[ii] = (u8 *)((u32)entry + (u32)items[ii]);
        }
    }
    entries[count] = (Entry *)((u32)page + (u32)entries[count]);
    return -0xFF;
}

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

/* GCC emits the literals of inline functions where they are defined, so these
 * helpers keep the strings in .rodata in the order of the original. */
static inline void NSPrintRetrySlave(PageStruct *ps) {
    printf("ERROR: frame retry slave %s\n", NSEIDValToString(ps->pgid));
}

static inline void NSPrintRetryLoading(PageStruct *ps) {
    printf("ERROR: frame retry loading %s\n", NSEIDValToString(ps->pgid));
}

INCLUDE_RODATA("asm/us/nonmatchings/ns", D_800100A8);

extern u32 D_80060B90[];
s32 NSTexturePageAllocate(u32 eid);
s32 NSPageFree(PageStruct *ps);
void LoadImage(Rect16 *rect, void *data);
void func_800181E0(u32 eid, s32 arg1);

/* Makes the entries of a freshly loaded page usable: links them into the NSD
 * table, uploads texture pages to VRAM and binds audio pages to sound RAM. */
s32 NSPageUpdateEntries(s32 idx) {
    PageStruct *ps;
    PageStruct *tps;
    PageStruct *aps;
    Entry *entry;
    NsdPte *pte;
    s32 i;
    u8 *item;

    ps = &ns.physical_pages[idx];
    if (ns.cur_ps == ps) {
        ns.cur_ps = NULL;
    }
    ps->state = 30;
    switch (((Page *)ps->page)->type) {
    case 0:
    case 5:
        pte = ns.pte_buckets[(((Page *)ps->page)->entries[0]->eid >> 15) & 0xFF];
        for (i = 0; i < ((Page *)ps->page)->entry_count; i++) {
            entry = ((Page *)ps->page)->entries[i];
            while (pte->eid != entry->eid) {
                pte++;
            }
            pte->value = (u32)entry;
            ps->unkF++;
            if (subsys[entry->type].on_load != NULL) {
                subsys[entry->type].on_load(entry);
            }
            ps->unkF--;
        }
        break;
    case 1:
        i = NSTexturePageAllocate(ps->eid);
        if (i == -0xC) {
            NSPageFree(ps);
            return 30;
        }
        tps = &D_80060D70[i];
        tpage_rect.x = (i % 4) * 256;
        tpage_rect.y = (i / 4) * 128;
        LoadImage(&tpage_rect, ps->page);
        tps->state = 20;
        tps->eid = ((Page *)ps->page)->id;
        tps->pgid = ps->pgid;
        tps->unk20 = (s32 *)NSProbe(tps->eid);
        tps->unkF = ps->unkF;
        tps->ref_count = ps->ref_count;
        *tps->unk20 = tps->unk1C;
        ps->state = 1;
        ps->unkF = 0;
        ns.page_map[ps->pgid >> 1] = tps;
        func_800181E0(((Page *)ps->page)->id, 1);
        return i << 1;
    case 3:
    case 4:
        pte = ns.pte_buckets[(((Page *)ps->page)->entries[0]->eid >> 15) & 0xFF];
        aps = ps->unk24;
        for (i = 0; i < ((Page *)ps->page)->entry_count; i++) {
            entry = ((Page *)ps->page)->entries[i];
            item = entry->items[0];
            while (pte->eid != entry->eid) {
                pte++;
            }
            if (i == 0) {
                if (((Page *)ps->page)->type == 4) {
                    D_80060B90[aps->unkC] = pte->eid;
                } else {
                    D_80060B90[aps->unkC] = 0x6396347F;
                }
            }
            pte->value = ((item - (u8 *)ps->page + aps->unk1C) << 2) | 0xC0000002;
            if (subsys[entry->type].on_load != NULL) {
                subsys[entry->type].on_load(entry);
            }
        }
        aps->state = 20;
        ps->state = 1;
        ps->unkF = 0;
        ns.page_map[ps->pgid >> 1] = aps;
        return aps->unk1C;
    }
    ps->state = 20;
    return (s32)ps->page;
}

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
void NSPageUpdate(PageStruct *ps, s32 pgid);

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
        NSPageUpdate(&ns.physical_pages[i], pgid);
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

/* Marks a run of pages as read (state 3) or to be decompressed (13) once
 * their sectors are in; this version never uses compression. Returns the
 * first page of the run. */
PageStruct *NSPageTranslate(PageStruct *ps) {
    PageStruct *head;
    s32 state = ps->state;
    s32 count = ps->unk1C;
    s32 savings = 0;
    s32 compressed;
    s32 i;

    if (state == 3 || state == 13) {
        ps->state = 4;
        NSPageTranslateOffsets(ps->page);
        return NULL;
    }
    compressed = 0;
    for (i = 0; i < count; i++) {
        if (state == 2) {
            ps->unkA = compressed;
            if (compressed) {
                savings += 0x20 - (ns.nsd->page_offsets[ps->pgid >> 1] & 0x3F);
                ps->unk8 = savings;
            }
            if (ps->state == 2) {
                ps->state = 3;
            } else {
                ps->state = 13;
                if (compressed) {
                    ps->unk10 = (u16 *)((u8 *)ps->page + (((ns.nsd->page_offsets[ps->pgid >> 1] & 0x3F) + ps->unk8) << 11));
                } else {
                    ps->unk10 = ps[1].page;
                }
                *ps->unk10 = 0x8765;
            }
        }
        head = ps;
        ps--;
    }
    return head;
}

s32 func_800519C4(s32);
s32 func_80013B8C(s32 type, s32 arg1);
void func_80051934(s32);
void func_800518D4(void *, s32);

/* Advances a page through its states: CD read, decompression, upload of audio
 * pages to sound RAM. Returns when the page has to wait. */
void NSPageUpdate(PageStruct *ps, s32 pgid) {
    PageStruct *head;
    PageStruct *aps;
    Page *page;
    Entry *entry;
    s32 status;
    s32 idx;
    s32 arg;
    u32 i;
    s32 type;

loop:
    switch (ps->state) {
    case 13:
        if (*ps->unk10 == 0x8765) {
            return;
        }
        if ((s32)NSPageTranslate(ps) == -0x10) {
            NSPrintRetrySlave(ps);
            goto loop;
        }
        goto case_4;
    case 2:
        if (ns.cur_ps != NULL) {
            return;
        }
        if (pgid != -1 && !func_80013158(ps, pgid)) {
            return;
        }
        ns.unkA38 = ps->pgid;
        if (!use_cd) {
            goto delete;
        }
        if (CdSync(1, NULL) != 2 && pgid == -1) {
            return;
        }
        head = NSPageTranslate(ps);
        ns.cur_ps = ps;
    read:
        if (NSCdRead(head, ps->unk1C) >= -0xFE) {
            goto read;
        }
        ps = head;
        goto loop;
    case 3:
        if (!use_cd || ns.cur_ps != ps) {
            goto delete;
        }
        status = func_80013000(0);
        if (status == 1) {
            if ((s32)NSPageTranslate(ps) == -0x10) {
                NSPrintRetryLoading(ps);
                ns.cur_ps = NULL;
                goto loop;
            }
            ns.cur_ps = NULL;
            goto case_4;
        }
        if (status != -0x10) {
            return;
        }
        printf("de l\n");
        ns.cur_ps = NULL;
        func_800131B0(ps);
        goto loop;
    delete:
        NSPageDelete(ps);
        ns.page_count--;
        ns.cur_ps = NULL;
        return;
    case 4:
    case_4:
        switch (((Page *)ps->page)->type) {
        case 3:
        case 4:
            ps->state = 10;
            break;
        default:
            NSPageUpdateEntries(ps->unkC);
            ns.page_count--;
            return;
        }
    case 10:
        if (ns.cur_audio_ps != NULL) {
            return;
        }
        if (!func_800519C4(0)) {
            return;
        }
        arg = -1;
        type = ((Page *)ps->page)->type;
        if (type == 4) {
            arg = *(s32 *)((Page *)ps->page)->entries[0]->items[0];
        }
        idx = func_80013B8C(type, arg);
        if (idx == -0xC) {
            if (((Page *)ps->page)->type == 5) {
                return;
            }
            NSPageDelete(ps);
            ns.page_count--;
            return;
        }
        aps = &D_80060BB8[idx];
        ps->state = 11;
        ps->unk24 = aps;
        ns.cur_audio_ps = ps;
        aps->state = 11;
        aps->pgid = ps->pgid;
        aps->eid = ps->eid;
        aps->unkF = 0;
        aps->ref_count = 0;
        func_80051934(aps->unk1C);
        page = ps->page;
        switch (page->type) {
        case 4:
            entry = page->entries[0];
            func_800518D4(entry->items[1], ((s32 *)entry->items[0])[1]);
            break;
        case 5:
            for (i = 0; i < page->entry_count; i++) {
                if (page->entries[i]->eid == ps->eid) {
                    func_800518D4(((Page *)ps->page)->entries[i]->items[0], NSEntryItemSize(page->entries[i], 0));
                    break;
                }
            }
            break;
        case 3:
            func_800518D4(page, 0x10000);
            break;
        }
    case 11:
        if (ns.cur_audio_ps != ps) {
            return;
        }
        if (!func_800519C4(0)) {
            return;
        }
        if (((Page *)ps->page)->type == 5) {
            ps->state = 20;
            ps->unk24->state = 20;
        } else {
            NSPageUpdateEntries(ps->unkC);
        }
        ns.cur_audio_ps = NULL;
        ns.page_count--;
        return;
    case 31:
        ps->state = 1;
        ps->unkF = 0;
        return;
    }
}

#ifndef NON_MATCHING
/* The "!mem\n" literal of NSPageAllocate, which is still assembly in matching builds. */
char D_800608D0[] = "!mem\n";
#endif

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

s32 NSTexturePageAllocate(u32 eid) {
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

/* Opens the entry that en_ref refers to (an entry ID, resolved and replaced
 * by its page table entry, or the entry itself with flag 2), loading its page
 * with count references; returns the entry, or 0 while it is not loaded. */
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
    if (!(((NsdPte *)en_ref)->value & 1)) {
        return ((NsdPte *)en_ref)->value;
    }
    return 0;
}

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

extern s32 next_lid;
extern struct GoolObject *crash;
extern s32 D_80060ABC;
extern char D_800100A8[];
void func_80016234(s32 lid);
char *strcpy(char *dst, const char *src);
NsdPte *NSProbeSafe(u32 eid);
void NSInitAudioPages(NsState *nss, s32 reserved, s32 size, s32 inst_size, s32 n);

#ifdef NON_MATCHING
/* "Cr30T": the literal stays in the .sdata of the file after ns.c. */
extern char D_800608D8[];
/* Loads the NSD of a level, then sets up the page tables and the physical,
 * virtual, texture and audio pages. Equivalent; the original keeps more copies
 * of the NSD pointer, initializes the page counter before storing the page
 * count, and builds the texture page info in another order. */
s32 NSInit(NsState *nss, s32 lid) {
    char filename[0x100];
    PageStruct *ps;
    NsdPte *pte;
    Nsd *nsd;
    s32 i;
    s32 level = lid; /* a second copy of lid, as in the original */
    s32 count;
    s32 blx, bly, pagexy, info, clut;
    s32 size, inst_size, n;

    if (nss->inited) {
        return -0xFF;
    }
    next_lid = -1;
    crash = NULL;
    D_80060ABC = lid;
    nss->inited = 1;
    nss->cur_ps = NULL;
    nss->page_count = 0;
    nss->cur_audio_ps = NULL;
    nss->lid = lid;
    nss->unkA38 = 0;
    nss->unkA40 = 1;
    func_80016234(lid);
    strcpy(filename, func_80012288(0, lid));
    if (use_cd) {
        nss->nsd = NSFileRead((CdFile *)&D_8006C4D8[lid], NULL, 1, 0);
    } else {
        nss->nsd = NSFileRead((CdFile *)filename, NULL, 0, 0);
    }
    nsd = nss->nsd;
    nss->ptes = nsd->ptes;
    nss->unk10 = (u32 *)nsd->unk408;
    nss->pte_buckets = (NsdPte **)nsd;
    nss->ldat = (LevelData *)&nsd->ptes[nsd->pte_count];
    nss->nsd = realloc(nsd, sizeof(Nsd) - sizeof(NsdPte) + nsd->pte_count * sizeof(NsdPte) + 0x2F4 + (nss->ldat->checkpoint_count - 1) * 0x18);
    for (i = 255; i >= 0; i--) {
        nss->pte_buckets[i] = &nss->ptes[(s32)nss->pte_buckets[i]];
    }
    strcpy(filename, func_80012288(1, level));
    if (use_cd) {
        ns.sector = D_8006C4D8[level].nsf_sector & 0xFFFFFF;
    }
    nss->page_map = malloc(nss->nsd->unk400 * sizeof(PageStruct *));
    for (i = 0; i < nss->nsd->unk400; i++) {
        nss->page_map[i] = (PageStruct *)-0x12;
    }
    for (i = 0; i < 22; i++) {
        if (subsys[i].init != NULL) {
            subsys[i].init();
        }
    }
    nss->physical_page_count = 21;
    for (i = 21; i >= 0; i--) {
        if ((nss->physical_pages[0].page = malloc(i << 16)) != NULL) {
            break;
        }
    }
    nss->physical_page_count = i;
    for (i = count = 0; i < nss->physical_page_count; i++) {
        count++;
        nss->physical_pages[i].type = 1;
        nss->physical_pages[i].page = (u8 *)nss->physical_pages[0].page + (i << 16);
        nss->physical_pages[i].unkC = i;
        nss->physical_pages[i].unkF = 0;
        nss->physical_pages[i].state = 1;
    }
    nss->virtual_page_count = 37;
    for (i = 0; i < nss->virtual_page_count; i++) {
        nss->virtual_pages[i].state = 1;
        nss->virtual_pages[i].type = 0;
        nss->virtual_pages[i].page = NULL;
        nss->virtual_pages[i].unkF = 0;
    }
    printf(D_800100A8, count, NSAlphaNumChar(level));
    /* Texture pages: VRAM as 4x4 blocks of 256x128. The upper half holds the
     * frame buffers; the lower half keeps textures still valid for this level. */
    for (i = 0; i < 16; i++) {
        ps = &D_80060D70[i];
        ps->type = 2;
        blx = i % 4;
        bly = i / 4;
        pagexy = blx << 2;
        if (i >= 8) {
            pagexy |= 0x10;
        }
        clut = (blx << 8) | (bly << 17);
        if (bly & 1) {
            info = pagexy | (clut | 0x80);
        } else {
            info = pagexy | clut;
        }
        ps->unk1C = info | 0x80000002;
        if ((u32)i < 8) {
            ps->state = 30;
        } else if (ps->state == 20 || ps->state == 21) {
            pte = NSProbeSafe(ps->eid);
            if ((s32)pte < -0xFE && ps->eid != NSStringToEID(D_800608D8)) {
                ps->state = 20;
                ps->pgid = pte->value;
                ps->unk20 = (s32 *)pte;
                ps->unkF = 0;
                ps->ref_count = 0;
                ns.page_map[ps->pgid >> 1] = ps;
                pte->value = ps->unk1C;
            } else {
                ps->state = 21;
                ps->unkF = 0;
                ps->ref_count = 0;
            }
        } else {
            ps->state = 1;
        }
    }
    /* Audio pages: sound RAM per level, sized by the number of sound entries. */
    inst_size = 0x10000;
    n = -1;
    size = NSCountEntries(nss, 14) * 0xFF90 - 0x1110;
    switch (lid) {
    case 41:
    case 42:
    case 58:
    case 59:
        inst_size = 0x10000;
        n = 2;
        break;
    case 40:
        inst_size = 0x10000;
        n = 3;
        break;
    case 60:
        n = 1;
        break;
    case 3:
    case 4:
    case 5:
    case 7:
        inst_size = 0x8000;
        n = 2;
        break;
    }
    NSInitAudioPages(nss, 0, size, inst_size, n);
    for (i = 0; i < 22; i++) {
        if (subsys[i].init2 != NULL) {
            subsys[i].init2();
        }
    }
    return -0xFF;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/ns", NSInit);
#endif

void free(void *ptr);
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
    if (ns.cur_audio_ps != 0) {
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
void NSInitAudioPages(NsState *nss, s32 reserved, s32 size, s32 inst_size, s32 n) {
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
