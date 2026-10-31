#ifndef GAME_H
#define GAME_H

#include "types.h"
#include "gfx.h"
#include "gool.h"

typedef struct {
    u8 unk0[0x44];
    s32 unk44;
} Object;

/* Global game state. Only the fields used so far are named, at their offsets. */
typedef struct {
    struct GoolObject *player; /* 0x0 */
    u8 unk4[0x4];
    GoolHandle handles[8]; /* 0x8: heads of the object lists */
    GoolHandle free_objects; /* 0x48 */
    u8 unk50[0x4];
    u32 frames; /* 0x54: frame counter */
    u8 unk58[0x8];
    s32 unk60[4];
    u32 unk70;
    s32 unk74;
    u8 unk78[0xC];
    u32 display_flags; /* 0x84 */
    s32 unk88;
    u8 unk8C[0x4];
    s32 pause_obj; /* 0x90: a pointer or an error code */
    u8 unk94[0x10];
    s32 game_state; /* 0xA4 */
    u8 unkA8[0x68];
    s32 unk110;
    s32 unk114;
    u8 unk118[0x4C];
    s32 unk164;
    u8 unk168[0xC];
    s32 unk174;
    u8 unk178[0x10];
    s32 unk188;
    s32 unk18C;
    u8 unk190[0xC];
    s32 unk19C;
    u8 unk1A0[0x2C];
    s32 unk1CC;
    u8 unk1D0[0x20];
    s32 unk1F0;
    s32 unk1F4;
    u8 unk1F8[0xC];
    s32 unk204;
    s32 unk208;
    u8 unk20C[0x10];
    s32 unk21C;
    s32 unk220;
    s32 unk224;
    u8 unk228[0x4];
    Object *unk22C;
    u8 unk230[0x3C];
    s32 unk26C;
    u8 unk270[0xD4];
    s32 unk344;
    Object *unk348;
    u8 unk34C[0x10];
    s32 unk35C;
    u8 unk360[0x4];
    s32 unk364;
    u8 unk368[0x4];
    s32 unk36C;
    u8 unk370[0x8];
    struct GoolObject *unk378;
    s32 unk37C;
    s32 unk380;
} GameState;

extern GameState game;

typedef struct {
    u8 unk0[0x10];
    s32 *unk10;
} UnkCC;

/* Render settings passed to the drawing functions (D_800676B0). */
typedef struct {
    u8 unk0[0xC];
    MATRIX *unkC; /* the current model matrix */
    u8 unk10[0x18];
    void *unk28;
    u8 unk2C[0x5C];
    s32 unk88;
    u8 unk8C[0xC];
    s32 unk98;
    u8 unk9C[0x2DC];
    u32 unk378;
} RenderState;

typedef struct {
    u8 unk0[0x20];
    u16 unk20[9];
    u8 unk32[0x2E];
    Transform unk60;
    u8 unk84[0x24];
    s32 unkA8;
    u8 unkAC[0x18];
    s32 unkC4; /* GTE projection distance (H) */
    u8 unkC8[0x4];
    UnkCC *unkCC;
    u8 unkD0[0x74];
    u32 unk144;
    u8 unk148[0x18D8];
    RenderState render; /* 0x1A20 (D_800676B0) */
} Unk80065C90;

extern Unk80065C90 D_80065C90;

#endif /* GAME_H */
