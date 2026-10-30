#include "common.h"

/* NSF/NSD paging: loads level data from the disc (names follow c1). */

typedef struct {
    u8 unk0[0x8];
    s32 type; /* 0x8 */
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
    u8 unk0[0x18];
    Nsd *nsd; /* 0x18 */
} NsState;

typedef struct {
    s16 x, y, w, h;
} Rect16;

extern char alpha_map[];
extern char eid_str_buf[];
extern char eid_str_buf2[];

int sprintf(char *buf, const char *fmt, ...);
s32 func_80012144(u32 eid);


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

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800120AC);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80012144);

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
    return func_80012144(pte->eid);
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

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800123F4);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_8001242C);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80012588);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80012634);

INCLUDE_RODATA("asm/us/nonmatchings/ns", jtbl_80010000);

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

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800131B0);

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

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013ECC);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013EFC);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013F74);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80013FA8);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80014024);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800140C0);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_8001418C);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80014268);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800142B4);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_8001439C);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_8001440C);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80014468);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800145A0);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80014650);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80014788);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_800148F8);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80014B04);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80014B50);

INCLUDE_ASM("asm/us/nonmatchings/ns", NSInit);

INCLUDE_ASM("asm/us/nonmatchings/ns", NSKill);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80015230);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80015414);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_80015468);

INCLUDE_ASM("asm/us/nonmatchings/ns", func_8001549C);
