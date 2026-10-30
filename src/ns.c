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
    u8 unk0[0x8];
    NsdPte **pte_buckets; /* 0x8: 256 hash buckets of the NSD */
    u8 unkC[0xC];
    Nsd *nsd; /* 0x18 */
    u8 unk1C[0xA20];
    struct PageStruct **page_map; /* 0xA3C */
} NsState;

extern NsState ns;

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
    u8 unk18[0x4];
    s32 unk1C;
    u8 unk20[0x4];
    struct PageStruct *unk24;
} PageStruct;

extern u8 D_8005F408;

typedef struct {
    s16 unk0;
    u8 unk2[0x2A];
} Unk2C;

typedef struct {
    u8 unk0[0x30];
    Unk2C items[1];
} Unk15468;

extern char alpha_map[];
extern char eid_str_buf[];
extern char eid_str_buf2[];

int sprintf(char *buf, const char *fmt, ...);


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

INCLUDE_ASM("asm/us/nonmatchings/ns", func_8001235C);

s32 NSEntryItemSize(Entry *entry, s32 idx) {
    if (entry->item_count != 0) {
        return entry->items[idx + 1] - entry->items[idx];
    }
    return 0;
}

INCLUDE_ASM("asm/us/nonmatchings/ns", func_8001242C);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80012588);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80012634);

INCLUDE_RODATA("asm/us/nonmatchings/ns", D_80010068);

INCLUDE_RODATA("asm/us/nonmatchings/ns", D_80010088);

INCLUDE_RODATA("asm/us/nonmatchings/ns", D_800100A8);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800126D0);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80012B00);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80012CBC);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80012EAC);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80012F7C);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013000);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013050);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013158);

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

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013B00);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013B8C);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013C54);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013D04);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013E64);

void func_80013E64(PageStruct *ps);

void NSPageDecRef(PageStruct *ps) {
    if (ps->ref_count != 0) {
        ps->ref_count--;
        func_80013E64(ps);
    }
}

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013EFC);

void NSPageDelete(PageStruct *ps) {
    ps->state = 1;
    ps->unkF = 0;
    ps->ref_count = 0;
    ns.page_map[ps->pgid >> 1] = (PageStruct *)-0x12;
}

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013FA8);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80014024);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800140C0);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_8001418C);

PageStruct *NSEntryPageStruct(void *en_ref);

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

INCLUDE_ASM("asm/us/nonmatchings/ns", func_8001439C);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_8001440C);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80014468);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800145A0);

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

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80014B50);

INCLUDE_ASM("asm/us/nonmatchings/ns", NSInit);

INCLUDE_ASM("asm/us/nonmatchings/ns", NSKill);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80015230);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80015414);

void func_80015468(Unk15468 *arg0, s32 idx) {
    if (arg0->items[idx].unk0 != 0) {
        arg0->items[idx].unk0 = 0;
    }
}

u8 func_8001549C(void) {
    return D_8005F408;
}
