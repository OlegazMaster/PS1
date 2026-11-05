#include "common.h"
#include "gool.h"
#include "game.h"
#include "ns.h"

/* Sound effects and music (voices of the SPU, volumes, the music sequence). */

/* .rodata: the debug messages of func_8003328C and func_80033864 */
const char D_8001052C[] = "x:%d y:%d z:%d dist2:%d\ndir:%d cos:%d vol:%d svol:%d   %d/%d\n";
const char D_8001056C[] = "vol %d*%d = %d\n";

/* .sdata */
s32 D_80060A00 = 30;
s32 D_80060A04 = 0;
s32 D_80060A08 = 0;
s16 D_80060A0C = 0; /* frames left of the music fade */
u32 *D_80060A10 = NULL; /* the music to play after the fade */
s32 D_80060A14 = -1;
s32 D_80060A18 = -1;
s32 D_80060A1C = 0;
s32 D_80060A20 = 0;
s32 D_80060A24 = 0x1F800040;
s32 D_80060A28 = 0;

extern s32 D_80068F7C[]; /* game.unkE4: mono sound */

/* .sbss */
static s32 D_80060AE8;
static s16 D_80060AEC[2]; /* volume (left, right) of a sound */
static s16 D_80060AF0[4];
static s32 D_80060AF8;
static s16 D_80060AFC[2];
static s16 D_80060B00;
static s32 D_80060B04;
static s32 D_80060B08[2];
static s32 D_80060B10[2];
static s32 D_80060B18;
static s16 D_80060B1C[2];

/* A voice of the SPU (D_8006C7D8, 24 of them). */
typedef struct {
    s32 handle;
    u32 flags; /* 0x4: bit 3 in use */
    u8 unk8[0x4];
    s32 owner; /* 0xC */
    u8 unk10[0x8];
    s32 unk18;
    u8 unk1C;
    u8 unk1D;
    u8 unk1E[0x32];
} Voice; /* 0x50 */
extern Voice D_8006C7D8[24];

/* A sequence of the music (D_8006CFC8: the two of the music). */
typedef struct {
    s16 id;
    u8 unk2[0x2];
    s32 state; /* 0x4: 0 stopped, 1 playing, 2 paused */
    u8 unk8[0x8];
    s16 vol_left; /* 0x10 */
    s16 vol_right;
} Seq; /* 0x14 */
extern Seq D_8006CFC8[2];



/* An empty debug function. */


/* Two words passed by value (to the empty functions below). */
typedef struct {
    s32 v[2];
} Pair;

/* Empty functions (left out of this version of the game). */
void func_80032F28(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032F30(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032F38(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032F40(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032F48(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032F50(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032F58(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032F60(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032F68(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032F70(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032F78(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032F80(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032F88(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032F90(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032F98(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032FA0(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032FA8(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032FB0(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032FB8(s32 a, s32 b, s32 c, Pair d) {
}

void func_80032FC0(s32 a, s32 b, s32 c, Pair d) {
}

/* Sets the main volume (0..255 -> 0..0x3FFF). */

typedef struct {
    s32 mask;
    s16 mvol_left;
    s16 mvol_right;
    s16 mvolmode_left;
    s16 mvolmode_right;
    s16 mvolx_left;
    s16 mvolx_right;
    s16 cd_left;
    s16 cd_right;
    s32 cd_reverb;
    s32 cd_mix;
    s16 ext_left;
    s16 ext_right;
    s32 ext_reverb;
    s32 ext_mix;
} SpuCommonAttr;
void func_80051A74(SpuCommonAttr *attr); /* SpuSetCommonAttr */

void func_80032FC8(s32 vol, s32 unused) {
    SpuCommonAttr attr;

    attr.mask = 3;
    attr.mvol_left = vol * 0x81;
    attr.mvol_right = vol * 0x81;
    func_80051A74(&attr);
}

/* The music type of the level (6 for some levels, else 5); eid is not used. */

s32 func_80033000(u32 eid) {
    switch (ns.ldat->lid) {
    case 5:
    case 10:
    case 13:
    case 14:
    case 17:
    case 19:
    case 20:
    case 21:
    case 23:
    case 25:
    case 28:
    case 31:
    case 32:
    case 33:
    case 37:
    case 39:
        return 6;
    default:
        return 5;
    }
}

/* The music being played (D_8006CF78). */
typedef struct {
    u32 f0 : 1;
    u32 f1 : 1;
    u32 f2 : 1;
    u32 f3 : 1;
    u32 f4 : 1;
    u32 f5 : 1;
    u32 f6 : 1;
    u32 f7 : 1;
    u32 f8 : 1;
    u32 type : 4; /* bits 9..12 */
    u32 f13 : 1;
    u32 f14 : 1;
    u32 f15 : 3;
    u32 f18 : 1;
    u32 : 13;
    u8 unk4[0x4];
    s32 unk8;
    u32 ref; /* 0xC: the entry played (EID or pointer) */
    u32 eid; /* 0x10 */
    s32 unk14;
    u8 unk18;
    u8 unk19;
    s16 unk1A;
    s16 unk1C;
    s16 unk1E;
    s32 unk20;
    s32 unk24;
    s32 unk28;
} Music;
extern Music D_8006CF78;
s32 func_80033000(u32 eid);
#ifdef NON_MATCHING
/* Resets the music state.
 * Equivalent; the original loads the null EID only once. */
void func_80033050(void) {
    D_8006CF78.unk8 = 1;
    D_8006CF78.unk18 = 1;
    D_8006CF78.unk19 = 0x80;
    D_8006CF78.unk1A = 0x3FFF;
    D_8006CF78.eid = 0x6396347F;
    D_8006CF78.unk1E = 0x1000;
    D_8006CF78.unk1C = 0;
    D_8006CF78.unk14 = 0;
    D_8006CF78.unk20 = 0;
    D_8006CF78.unk24 = 0;
    D_8006CF78.unk28 = 0;
    D_8006CF78.f1 = 0;
    D_8006CF78.f0 = 0;
    D_8006CF78.f2 = 0;
    D_8006CF78.f3 = 0;
    D_8006CF78.f4 = 0;
    D_8006CF78.f5 = 0;
    D_8006CF78.f6 = 0;
    D_8006CF78.f7 = 0;
    D_8006CF78.f8 = 0;
    D_8006CF78.type = func_80033000(D_8006CF78.eid);
    D_8006CF78.f13 = 1;
    D_8006CF78.f14 = 0;
    D_8006CF78.f15 = 0;
    D_8006CF78.f18 = 0;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/audio", func_80033050);
#endif

void func_8003314C(void) {
}

void func_80033154(void) {
}

extern s32 D_80068F80;
extern u8 D_8006CFF8[];
void func_800533C8(s32 a, s32 b);
void func_8005697C(void *a, s32 b, s32 c);
void func_8004F9B4(s32 a, void *b);
void func_80051994(s32 a);
/* Starts the sound: frees the voices, turns the reverb on, sets the volumes. */

void func_8003315C(void) {
    SpuCommonAttr attr;
    s32 i;
    s32 m;

    D_80060A04 = 1;
    for (i = 0, m = ~8; i < 24; i++) {
        D_8006C7D8[i].flags &= m;
    }
    func_800533C8(i, m);
    func_8005697C(D_8006CFF8, 1, 2);
    func_8004F9B4(0, D_80060B10);
    attr.mask = 3;
    attr.mvol_left = 0x3FFF;
    attr.mvol_right = 0x3FFF;
    func_80051A74(&attr);
    func_80051994(0);
    func_80033050();
    func_80033280((D_80068F80 * 0x3FFF00) >> 16);
    D_80060AFC[1] = 0x3FFF;
    D_80060B00 = 0;
    func_80032FC8(0x7F, 0x7F);
    D_80060B08[0] = 0;
}

void func_80050554(s32 on); /* SpuSetReverb */
/* Turns the reverb off. */

typedef struct {
    s32 mask;
    s32 mode;
    s16 depth_left;
    s16 depth_right;
    s32 delay;
    s32 feedback;
} SpuReverbAttr;
void func_800508C4(SpuReverbAttr *attr); /* SpuSetReverbModeParam */

void func_80033238(void) {
    SpuReverbAttr attr;

    if (D_80060A04 != 0) {
        attr.mask = 7;
        attr.mode = 0;
        attr.depth_left = 0;
        attr.depth_right = 0;
        func_800508C4(&attr);
        func_80050554(0);
        D_80060A04 = 0;
    }
}

void func_80033280(s16 a) {
    D_80060AF0[0] = a;
}

s32 func_8003AE90(s32 x, s32 z); /* length of (x, z) */
s32 func_8004F794(s32 x, s32 z); /* angle of (x, z) */
s32 func_8003C364(s32 angle); /* cos */
s32 printf(const char *fmt, ...);
/* The volume (left, right) of a sound of volume vol at pos (relative to the
 * camera), louder on the side it is on (unless the sound is mono). */

s16 *func_8003328C(Music *m, s32 *pos, s32 vol) {
    s32 sh;
    s32 x;
    s32 z;
    s32 d;
    s32 ang;
    s32 c;
    s32 v;
    s32 l;
    s32 r;

    if (m != NULL) {
        sh = m->type;
    } else {
        sh = 5;
    }
    x = pos[0] >> (sh + 6);
    z = (pos[2] - 0x4B000) >> (sh + 6);
    d = func_8003AE90(x, z);
    if (d <= 0x7FFF) {
        D_80060AEC[0] = vol;
        D_80060AEC[1] = vol;
    } else {
        ang = func_8004F794(z, x);
        c = func_8003C364(ang) + 0x1000;
        v = (vol << 15) / d;
        if (D_80068F7C[0]) {
            r = v;
            l = v;
        } else {
            l = (v + v * (0x2000 - c)) >> 11;
            r = (v + v * c) >> 11;
        }
        if (l < vol) {
            D_80060AEC[0] = l < 0 ? 0 : l;
        } else {
            D_80060AEC[0] = vol;
        }
        if (r < vol) {
            D_80060AEC[1] = r < 0 ? 0 : r;
        } else {
            D_80060AEC[1] = vol;
        }
    }
    if (m->f14) {
        printf(D_8001052C, x, z, 0, d, ang, c, vol, v, D_80060AEC[0], D_80060AEC[1]);
    }
    D_80060AEC[0] = D_80060AEC[0] >= 0x4000 ? 0x3FFF : D_80060AEC[0];
    D_80060AEC[1] = D_80060AEC[1] >= 0x4000 ? 0x3FFF : D_80060AEC[1];
    return D_80060AEC;
}

void func_80033C54(s32 handle, u32 a, u32 *b, s32 owner);
/* Stops the voices of owner (from voice D_80060B04 on). */

void AudioVoiceFree(s32 owner) {
    u32 arg;
    Voice *v;
    s32 i;

    if (owner == 0) {
        return;
    }
    for (i = D_80060B04, v = &D_8006C7D8[i]; i < 24; i++, v++) {
        if (((v->flags >> 3) & 1) && v->owner == owner) {
            arg = 0;
            D_80060A00 = 60;
            func_80033C54(v->handle, 0x40000000, &arg, v->owner);
            v->owner = 0;
            v->unk1D = 0;
        }
    }
}

typedef struct {
    u32 voice;
    u32 mask;
    s16 vol_left;
    s16 vol_right;
    u8 unkC[0x34];
} SpuVoiceAttr;
void func_80052844(SpuVoiceAttr *attr); /* SpuGetVoiceAttr */
#define ABS(x) ((x) < 0 ? -(x) : (x))
s32 func_80032160(s32 n);
/* A voice for a sound of priority D_8006CF78.unk19: a free one, or else the
 * quietest of the lowest priority (only half of the time when they are
 * loud). -1 if none. */

s32 func_8003359C(void) {
    SpuVoiceAttr attr;
    Voice *v;
    s32 i;
    s32 best;
    s32 prio;
    s32 minvol;
    s32 vol;
    s32 a;
    s32 r;
    s32 l;

    minvol = 0x3FFF;
    prio = 0xFF;
    for (best = D_80060B04, v = &D_8006C7D8[best]; best < 24; best++, v++) {
        vol = v->unk1D;
        if (prio < vol) {
            vol = prio;
        }
        prio = vol;
        if (!((v->flags >> 3) & 1)) {
            return best;
        }
    }
    if (D_8006CF78.unk19 < prio) {
        return -1;
    }
    best = -1;
    for (i = D_80060B04, v = &D_8006C7D8[i]; i < 24; i++, v++) {
        if (v->unk1D == prio) {
            attr.voice = 1 << i;
            func_80052844(&attr);
            vol = ABS(attr.vol_left) > ABS(attr.vol_right) ? attr.vol_left : attr.vol_right;
            if (vol < 0) {
                vol = -vol;
            }
            if (vol < minvol) {
                best = i;
                minvol = vol;
            }
        }
    }
    if (prio == D_8006CF78.unk19 && minvol >= 0x800 && func_80032160(100) >= 50) {
        return -1;
    }
    return best;
}

void *func_80015414(u32 eid); /* the page of an entry, if in memory */
/* Prepares m to play the entry *ref: from memory if its page is there (state
 * 2), else it is loaded (state 1). Returns 0, or an error code. */

s32 func_80033760(Music *m, u32 *ref) {
    u32 eid;
    void *page;
    u32 e;
    s32 ret;

    eid = *ref;
    page = func_80015414((eid & 1) ? eid : ((Entry *)eid)->eid);
    m->f15 = 0;
    m->ref = *ref;
    m->unk14 = (s32)page;
    if (page != NULL) {
        ret = 0;
        m->f15 = 2;
        m->f18 = 1;
    } else {
        e = NSOpen(ref, 0, 1);
        if (e != 0) {
            if ((s32)e >= -0xFE) {
                return -14;
            }
            if (e != 0 && (e & 0xC0000002) == 0xC0000002) {
                ret = (e & 0x3FFFFFFD) >> 2;
                goto end;
            }
        }
        ret = 0;
        m->f15 = 1;
        m->f18 = 1;
    }
end:
    return ret;
}

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80033864);

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80033C54);

void func_80051714(s32 on, s32 voices); /* SpuSetKey */
void func_80013E64(s32 a);
/* Stops the voice v. */

void func_800342B8(Voice *v) {
    func_80051714(0, 1 << (v->flags >> 19));
    v->flags &= ~8;
    if (v->unk18 != 0) {
        func_80013E64(v->unk18);
    }
}

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80034310);

void func_80056DAC(s16 a, s16 b, s32 c, s32 d);
#ifdef NON_MATCHING
/* Sets the volume of the music.
 * Equivalent; the original computes vol >> 7 in another register first. */
void func_80034B00(s16 vol) {
    s32 i;
    s32 v;

    D_80060AF0[1] = vol;
    if (D_80060B18 == 3) {
        v = vol >> 7;
        for (i = 0; i < D_80060AFC[0]; i++) {
            func_80056DAC(D_80060B1C[1], D_8006CFC8[i].id, v, v);
        }
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/audio", func_80034B00);
#endif

extern s32 D_80068F84;
void func_80056B54(s32 a);
void func_8005AA38(u8 a);
/* Resets the music player. */

INCLUDE_ASM("asm/us/nonmatchings/audio", func_80034B8C);

void func_80034C0C(void) {
    D_80060B00 = -0x1F0;
}

void func_800352FC(Seq *s, s32 cmd, s32 arg);
/* Switches the music to the part of level progress p (>> 8): 3 plays the
 * second sequence (with the volume of the boss) and pauses the first, the
 * others the opposite. */

void func_80034C1C(s32 p) {
    if (D_80060AFC[0] < 2 || D_80060B18 != 3) {
        return;
    }
    p >>= 8;
    switch (p) {
    case 1:
    case 2:
    default:
        func_800352FC(&D_8006CFC8[1], 1, 0);
        func_800352FC(&D_8006CFC8[0], 3, 0);
        D_80060B08[0] = 0;
        break;
    case 3:
        if (game.unkEC != 0 || game.unkE8 != 0) {
            D_80060B08[0] = 0x3FFF;
        } else {
            D_80060B08[0] = 0;
        }
        func_800352FC(&D_8006CFC8[0], 2, 0);
        func_800352FC(&D_8006CFC8[1], 0, 1);
        break;
    }
    func_80056DAC(D_80060B1C[1], D_8006CFC8[1].id, (s16)D_80060B08[0], (s16)D_80060B08[0]);
}

s32 func_8005AF3C(void *vh, s32 a, s32 b); /* SsVabOpenHead */
s32 func_8005AE6C(void); /* SsVabTransBody */
void func_80055480(void);
s32 func_80053418(void *seq, s16 vab, s16 count); /* SsSepOpen */
void func_80052F60(s16 a, s16 b, s32 c, s32 d);
void func_80035224(Seq *s, s32 vol, s32 id);
void exit(s32 code);
/* Loads and starts the music *ref (an entry with the sound bank and the
 * sequences, items 2..8: their sample pages), in several frames: state 1
 * frees the previous one, 2 waits for the pages, then 3 plays. Returns -1
 * when muted. */

extern s32 D_80061A60[];
/* func_80035224 called with a fourth argument that is never set. */
#define SEQ_START(s, vol, id) ((void (*)(Seq *, s32, s32, s32))func_80035224)(s, vol, id, unset)

#ifdef NON_MATCHING
/* Equivalent; the original checks the first bank handle in another register. */
s32 func_80034D14(u32 *ref) {
    u32 eid;
    Entry *e;
    Entry *vh;
    s32 i;
    s32 fresh;
    s32 v;
    s32 unset;

    fresh = D_80060B1C[0] == -1;
    if (D_80060B08[0] != 0) {
        return -1;
    }
    if (D_80060B18 == 1) {
        if (D_80060A1C++ < 3) {
            return 0;
        }
        D_80060A1C = 0;
        if (D_80060AF8 != 0x6396347F) {
            func_80052DD4(D_80060B1C[1]);
            eid = D_80060AF8;
            func_8001440C(&eid, 1);
        }
        if (*ref != 0x6396347F) {
            e = (Entry *)NSOpen(ref, 1, 1);
            D_80060AF8 = e->eid;
            for (i = 0; i < 7; i++) {
                if (((u32 *)e->items[0])[i + 2] != 0x6396347F) {
                    NSOpen(&((u32 *)e->items[0])[i + 2], 0, 1);
                }
            }
        } else {
            D_80060AF8 = *ref;
            return 0;
        }
    }
    if (*ref == 0x6396347F) {
        D_80060AF8 = *ref;
        D_80060B18 = 0;
        return 0;
    }
    D_80060B18 = 2;
    e = (Entry *)NSOpen(ref, 1, 0);
    D_80060A10 = NULL;
    for (i = 0; i < 7; i++) {
        if (((u32 *)e->items[0])[i + 2] != 0x6396347F && !func_8001440C(&((u32 *)e->items[0])[i + 2], 0)) {
            return 0;
        }
    }
    D_80060AFC[0] = *(u16 *)e->items[0];
    D_80060B18 = 3;
    if (D_80060B1C[0] == -1) {
        vh = (Entry *)NSOpen((u32 *)e->items[0] + 1, 1, 1);
        if ((D_80060B1C[0] = func_8005AF3C(vh->items[1], -1, D_80061A60[0])) == -1) {
            exit(-1);
        }
        if ((D_80060B1C[0] = func_8005AE6C()) == -1) {
            exit(-1);
        }
        func_80055480();
    }
    if ((D_80060B1C[1] = func_80053418(e->items[2], D_80060B1C[0], D_80060AFC[0])) == -1) {
        exit(-1);
    }
    for (i = 0; i < D_80060AFC[0]; i++) {
        if (fresh) {
            v = D_80060AF0[1] >> 7;
            SEQ_START(&D_8006CFC8[i], v | (v << 16), i);
        } else {
            SEQ_START(&D_8006CFC8[i], 0, i);
            func_80052F60(D_80060B1C[1], D_8006CFC8[i].id, D_80060AF0[1] >> 7, 30);
        }
    }
    func_800352FC(D_8006CFC8, 0, 0);
    return 0;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/audio", func_80034D14);
#endif

void func_8003505C(void) {
    if (D_80060B18 == 0) {
        D_80060B18 = 1;
    }
}

void func_800531E8(s16 a, s16 b, s32 c, s32 d); /* SsSeqSetCrescendo-like: fades the volume */
s32 func_80034D14(u32 *ref);
void func_8005AA14(void);
void func_8005AA28(void);
/* Updates the music every frame: fades out (state 4) before starting the
 * music *ref when it changes. */


void func_80035078(u32 *ref) {
    u32 eid;

    if (D_80060B18 == 0) {
        return;
    }
    if (D_80060B18 == 4) {
        if (D_80060A0C == 0 || --D_80060A0C == 0) {
            D_80060B18 = 1;
        }
    }
    if (D_80060B18 == 3) {
        eid = *ref;
        if (eid & 1) {
            if (D_80060AF8 == eid) {
                goto next;
            }
        } else if (D_80060AF8 == ((Entry *)eid)->eid) {
            goto next;
        }
        if (D_80060B08[0] == 0 && D_80060B1C[0] != -1 && D_80060A10 == NULL) {
            func_800531E8(D_80060B1C[1], D_8006CFC8[0].id, D_80060AF0[1] >> 7, 30);
            D_80060A0C = 30;
            D_80060A10 = ref;
            D_80060B18 = 4;
            return;
        }
    }
next:
    switch (D_80060B18) {
    case 1:
    case 2:
        func_80034D14(ref);
        break;
    case 3:
        if (D_80068F7C[0]) {
            func_8005AA14();
        } else {
            func_8005AA28();
        }
        break;
    }
}

void func_8005321C(void);
void func_8003529C(s16 a, s32 all);
/* Stops the music. */

void func_800351EC(void) {
    if (D_80060A08 != 0) {
        func_8003529C(D_80060B1C[1], 1);
        func_8005321C();
        D_80060A08 = 0;
    }
}

#ifdef NON_MATCHING
/* Starts the sequence s as id with the volume vol (left << 16 | right).
 * Equivalent; the original stores unk4 after loading the arguments. */
void func_80035224(Seq *s, s32 vol, s32 id) {
    if (D_80060AFC[0] >= 2 && id == 1) {
        s->vol_left = s->vol_right = *(u16 *)D_80060B08;
    } else {
        s->vol_left = vol >> 16;
        s->vol_right = vol;
    }
    s->id = id;
    s->state = -1;
    func_80056DAC(D_80060B1C[1], id, s->vol_left, s->vol_right);
}
#else
INCLUDE_ASM("asm/us/nonmatchings/audio", func_80035224);
#endif

void func_80052DD4(s16 a);
void func_8005AA68(s16 a);
/* Stops the sequence a and (with all) the other one. */

void func_8003529C(s16 a, s32 all) {
    if (a != -1) {
        func_80052DD4(a);
    }
    if (all && D_80060B1C[0] != -1) {
        func_8005AA68(D_80060B1C[0]);
    }
}

void func_80054FD4(s16 a, s16 b, s32 c, s16 d); /* SsSeqPlay */
void func_80056950(s16 a, s16 b); /* SsSeqPause */
void func_80054F70(s16 a, s16 b); /* SsSeqStop */
void func_800550D0(s16 a, s16 b); /* SsSeqReplay */
#ifdef NON_MATCHING
/* Command cmd for the sequence s: 0 play (arg: times), 1 pause, 2 stop,
 * 3 resume, 4 volume (arg: left << 16 | right).
 * Equivalent; the original reads the count before setting the volume. */
void func_800352FC(Seq *s, s32 cmd, s32 arg) {
    s32 i;
    s32 a;
    s32 b;

    if (s == NULL) {
        return;
    }
    switch (cmd) {
    case 0:
        func_80054FD4(D_80060B1C[1], s->id, s->state = 1, arg);
        break;
    case 1:
        a = D_80060B1C[1];
        b = s->id;
        s->state = 2;
        func_80056950(a, b);
        break;
    case 2:
        a = D_80060B1C[1];
        b = s->id;
        s->state = 0;
        func_80054F70(a, b);
        break;
    case 3:
        a = D_80060B1C[1];
        b = s->id;
        s->state = 1;
        func_800550D0(a, b);
        break;
    case 4:
        i = 0;
        s->vol_left = arg >> 16;
        s->vol_right = arg;
        for (; i < D_80060AFC[0]; i++) {
            func_80056DAC(D_80060B1C[1], s->id, s->vol_left, s->vol_right);
        }
        break;
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/audio", func_800352FC);
#endif

#ifdef NON_MATCHING
/* The value that GOOL operand op of obj refers to: a field of a linked
 * object, a constant (put in one of two words of the scratchpad), a word of
 * the stack frame, or a word of the code (global or of the state).
 * Equivalent; the original shares the NULL return and keeps the code word in v0. */
u32 *func_80035410(GoolObject *obj, u32 op) {
    u32 *buf = (u32 *)0x1F800040;
    GoolObject *link;
    s32 unused[4]; /* takes stack space in the original */

    if (op & 0x800) {
        if (op & 0x400) {
            link = *(GoolObject **)((u8 *)obj + ((op >> 4) & 0x1C) + 0x40);
            if (link == NULL) {
                return NULL;
            }
            return (u32 *)&link->self + (op & 0x3F);
        }
        if (op & 0x200) {
            if (op & 0x100) {
                if (op & 0x80) {
                    if (op == 0xBE0) {
                        return NULL;
                    }
                    if (op == 0xBF0) {
                        return (u32 *)1;
                    }
                } else {
                    return obj->fp + ((s32)(op << 25) >> 25);
                }
            } else {
                D_80060A20 = D_80060A20 == 0;
                buf[D_80060A20] = (s32)(op << 24) >> 20;
                return &buf[D_80060A20];
            }
        } else {
            D_80060A20 = D_80060A20 == 0;
            buf[D_80060A20] = (s32)(op << 23) >> 15;
            return &buf[D_80060A20];
        }
    } else if (op & 0x400) {
        return &((u32 *)obj->external->items[2])[op & 0x3FF];
    } else {
        return &((u32 *)obj->unk10->items[2])[op & 0x3FF];
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/audio", func_80035410);
#endif

#ifdef NON_MATCHING
/* The word that GOOL operand op of obj refers to, as func_80035410, with the
 * fields of obj itself and its stack (0x1F: pops, or with push pushes).
 * Equivalent; the original shares the NULL return and allocates the registers differently. */
u32 *func_80035538(GoolObject *obj, u32 op, s32 push) {
    GoolObject *link;
    u32 *p;
    u32 idx;
    s32 unused[4]; /* takes stack space in the original */

    if (op & 0x800) {
        if (op & 0x400) {
            if (op & 0x200) {
                idx = op & 0x1FF;
                if (idx == 0x1F) {
                    if (!push) {
                        p = --obj->sp;
                    } else {
                        p = obj->sp++;
                    }
                } else {
                    p = (u32 *)&obj->self + idx;
                }
                return p;
            }
            link = *(GoolObject **)((u8 *)obj + ((op >> 4) & 0x1C) + 0x40);
            if (link == NULL) {
                return NULL;
            }
            return (u32 *)&link->self + (op & 0x3F);
        }
        if (op & 0x200) {
            if (op & 0x100) {
                if (op & 0x80) {
                    if (op == 0xBE0) {
                        return NULL;
                    }
                    if (op == 0xBF0) {
                        return (u32 *)1;
                    }
                } else {
                    return obj->fp + ((s32)(op << 25) >> 25);
                }
            } else {
                D_80060A28 = D_80060A28 == 0;
                ((u32 *)D_80060A24)[D_80060A28] = (s32)(op << 24) >> 20;
                return &((u32 *)D_80060A24)[D_80060A28];
            }
        } else {
            D_80060A28 = D_80060A28 == 0;
            ((u32 *)D_80060A24)[D_80060A28] = (s32)(op << 23) >> 15;
            return &((u32 *)D_80060A24)[D_80060A28];
        }
    } else if (op & 0x400) {
        return &((u32 *)obj->external->items[2])[op & 0x3FF];
    } else {
        return &((u32 *)obj->unk10->items[2])[op & 0x3FF];
    }
}
#else
INCLUDE_ASM("asm/us/nonmatchings/audio", func_80035538);
#endif

/* Elements that the assembly of this file reaches by name; ns.c also reads
 * D_80060B1C (static here so that GCC reaches its elements through $gp). */
__asm__(".globl D_80060B1C\n.set D_80060AF2, D_80060AF0 + 2\n.set D_80060AFE, D_80060AFC + 2\n.set D_80060B1E, D_80060B1C + 2");
