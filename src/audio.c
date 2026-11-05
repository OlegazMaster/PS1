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
    u8 unk1D; /* priority */
    s16 vol; /* 0x1E */
    s16 unk20;
    u16 pitch; /* 0x22 */
    s32 pos[3]; /* 0x24: of the sound, relative to the camera */
    s32 objpos[3]; /* 0x30: of its owner */
    s16 unk3C;
    s32 unk40;
    s32 unk44;
    u8 unk48[0x8];
} Voice; /* 0x50: a handle then a copy of the Music settings */
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
    u8 unk2C[0x20];
} Music; /* 0x4C */
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

s32 func_80033C54(s32 handle, u32 a, u32 *b, s32 owner);
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
    s16 left;
    s16 right;
} SpuVolume;
typedef struct {
    u32 voice;
    u32 mask;
    s16 vol_left; /* 0x8 */
    s16 vol_right;
    s16 volmode_left;
    s16 volmode_right;
    s16 volx_left;
    s16 volx_right;
    u16 pitch; /* 0x14 */
    u16 note;
    u16 sample_note;
    s16 envx;
    u32 addr; /* 0x1C */
    u32 loop_addr;
    s32 a_mode; /* 0x24 */
    s32 s_mode;
    s32 r_mode;
    u16 ar; /* 0x30 */
    u16 dr;
    u16 sr;
    u16 rr;
    u16 sl; /* 0x38 */
    u16 adsr1;
    u16 adsr2;
} SpuVoiceAttr; /* 0x40 */
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

s32 func_80033760(Music *m, u32 *ref, GoolObject *obj) {
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

s32 func_800202CC(s32 *in, s32 *out, s32 unused); /* interp.c: to camera space */
void func_80051F54(SpuVoiceAttr *attr); /* SpuSetVoiceAttr */
void func_80051274(s32 on, u32 voices); /* SpuSetReverbVoice */
extern s32 D_800690B4[]; /* game.unk21C: the pitch factor */
#ifdef NON_MATCHING
/* Plays the sound ref (an entry, or with bit 0 an address in the sound
 * memory) for obj at volume vol, with the settings in D_8006CF78 (which are
 * reset). Returns the handle of the voice, or -99.
 * Equivalent; the original keeps the voice and ref in swapped registers. */
s32 func_80033864(GoolObject *obj, u32 ref, s32 vol) {
    SpuVoiceAttr attr;
    s32 out[3];
    Voice *v;
    s32 i;
    s32 x;
    GoolObject *o;

    if (game.unkE8 == 0 || (!(game.display_flags & 0x1000) && !(obj->state_flags & 0x40000))) {
        goto fail;
    }
    i = func_8003359C();
    if (i <= 0) {
        goto fail;
    }
    v = &D_8006C7D8[i];
    if (ref & 1) {
        ref &= ~1;
    } else {
        ref = func_80033760(&D_8006CF78, (u32 *)ref, obj);
        if (ref == -14) {
            goto fail;
        }
        if (D_8006CF78.f15 != 0) {
            D_8006CF78.f18 = 1;
        }
    }
    *(Music *)&v->flags = D_8006CF78;
    v->vol = (vol * D_80060AF0[0]) >> 14;
    if ((v->flags >> 6) & 1) {
        v->unk44 = (v->unk3C - v->vol) / v->unk40;
    }
    if (v->owner == 1) {
        v->owner = (s32)obj;
    }
    if (v->owner != 0) {
        o = (GoolObject *)v->owner;
        *(Vec3 *)v->objpos = *(Vec3 *)o->xform.trans;
        func_800202CC(o->xform.trans, out, 1);
        *(Vec3 *)v->pos = *(Vec3 *)out;
    }
    func_80033050();
    v->flags = (v->flags & 0xFE07FFFF) | ((i & 0x3F) << 19);
    attr.mask = 0xFF93;
    attr.voice = 1 << i;
    if ((u8)((GoolObject *)v->owner)->unk120 - 6 < 2u || ((v->flags >> 9) & 0xF) == 0) {
        attr.vol_left = attr.vol_right = v->vol >= 0x3FFF ? 0x3FFF : (v->vol < 0 ? 0 : v->vol);
        v->flags &= ~0x1E00;
    } else {
        if ((v->flags >> 14) & 1) {
            printf(D_8001056C, vol, D_80060AF0[0], v->vol);
        }
        *(SpuVolume *)&attr.vol_left = *(SpuVolume *)func_8003328C((Music *)&v->flags, v->pos, v->vol);
    }
    attr.pitch = v->pitch * D_800690B4[0] / 4096;
    attr.addr = ref;
    attr.a_mode = 1;
    attr.s_mode = 1;
    attr.r_mode = 3;
    attr.ar = 0;
    attr.dr = 0;
    attr.sr = 0;
    attr.rr = 0;
    attr.sl = 15;
    func_80051F54(&attr);
    func_80051274((v->flags >> 13) & 1, attr.voice);
    if (!(v->flags & 0x40010)) {
        func_80051714(1, attr.voice);
    }
    v->handle = ++D_80060AE8;
    v->flags |= 8;
    return v->handle;
fail:
    func_80033050();
    return -99;
}
#else
INCLUDE_ASM("asm/us/nonmatchings/audio", func_80033864);
#endif

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
#ifdef NON_MATCHING
/* Resets the music player.
 * Equivalent; the original keeps the 1 in s0 and does not reorder the stores. */
void func_80034B8C(void) {
    s32 on = 1;

    D_80060A08 = on;
    func_80056B54(on);
    func_80034B00((D_80068F84 * 0x3FFF00) >> 16);
    D_80060AF8 = 0x6396347F;
    D_80060B18 = 0;
    D_80060B1C[1] = -1;
    D_80060B1C[0] = -1;
    D_80060B04 = 16;
    func_8005AA38(*(u8 *)&D_80060B04);
}
#else
INCLUDE_ASM("asm/us/nonmatchings/audio", func_80034B8C);
#endif

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


/* Elements that the assembly of this file reaches by name; ns.c also reads
 * D_80060B1C (static here so that GCC reaches its elements through $gp). */
__asm__(".globl D_80060B1C\n.set D_80060AF2, D_80060AF0 + 2\n.set D_80060AFE, D_80060AFC + 2\n.set D_80060B1E, D_80060B1C + 2");
