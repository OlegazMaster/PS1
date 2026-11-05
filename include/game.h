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
    void *objects; /* 0x4: memory of all objects */
    GoolHandle handles[8]; /* 0x8: heads of the object lists */
    GoolHandle free_objects; /* 0x48 */
    struct GoolObject *unk50;
    u32 frames; /* 0x54: frame counter */
    u8 *unk58; /* 0x58: end of the records (from game + 0x4B0) */
    s32 unk5C; /* 0x5C: count of unk390 */
    s32 unk60[4];
    u32 unk70;
    s32 unk74;
    s32 unk78;
    s32 unk7C;
    s32 unk80;
    u32 display_flags; /* 0x84 */
    s32 unk88;
    s32 unk8C;
    s32 pause_obj; /* 0x90: a pointer or an error code */
    s32 unk94;
    s32 unk98;
    s32 unk9C; /* 0x9C: camera yaw (copy) */
    s32 unkA0;
    s32 game_state; /* 0xA4 */
    s32 unkA8;
    s32 unkAC;
    s32 unkB0;
    s32 unkB4;
    s32 unkB8;
    s32 unkBC;
    s32 unkC0;
    s32 unkC4;
    s32 unkC8;
    s32 unkCC;
    s32 unkD0;
    s32 unkD4;
    s32 unkD8;
    s32 unkDC;
    s32 unkE0;
    s32 unkE4;
    s32 unkE8;
    s32 unkEC;
    s32 unkF0;
    s32 cam_trans[3]; /* 0xF4: camera position of the last frame */
    s32 cam_rot[3]; /* 0x100 */
    s32 unk10C;
    s32 unk110;
    s32 unk114;
    s32 unk118;
    s32 unk11C;
    s32 unk120;
    s32 unk124;
    s32 unk128;
    s32 unk12C;
    s32 unk130;
    s32 unk134;
    s32 unk138;
    s32 unk13C;
    s32 unk140;
    s32 unk144;
    s32 unk148;
    s32 unk14C;
    s32 unk150;
    s32 unk154;
    s32 unk158;
    s32 unk15C;
    s32 unk160;
    s32 unk164;
    s32 unk168;
    s32 unk16C;
    s32 unk170;
    s32 unk174;
    s32 unk178;
    s32 unk17C;
    s32 unk180;
    s32 unk184;
    s32 unk188;
    s32 unk18C;
    s32 unk190;
    s32 unk194;
    s32 unk198;
    s32 unk19C;
    s32 unk1A0;
    s32 unk1A4;
    s32 unk1A8;
    s32 unk1AC;
    s32 unk1B0;
    s32 unk1B4;
    s32 unk1B8;
    s32 unk1BC;
    s32 unk1C0;
    s32 unk1C4;
    s32 unk1C8;
    s32 unk1CC;
    s32 unk1D0;
    s32 unk1D4;
    s32 unk1D8;
    s32 unk1DC;
    s32 unk1E0;
    s32 unk1E4;
    s32 unk1E8;
    s32 unk1EC;
    s32 unk1F0;
    s32 unk1F4;
    s32 unk1F8;
    s32 unk1FC;
    s32 unk200;
    s32 unk204;
    s32 unk208;
    s32 unk20C;
    s32 unk210;
    s32 unk214;
    s32 unk218;
    s32 unk21C;
    s32 unk220;
    s32 unk224;
    s32 unk228;
    Object *unk22C;
    s32 unk230;
    s32 unk234;
    s32 unk238;
    s32 unk23C;
    s32 unk240;
    s32 unk244;
    s32 unk248;
    s32 unk24C;
    s32 unk250;
    s32 unk254;
    s32 unk258;
    s32 unk25C;
    s32 unk260[3]; /* 0x260: point the camera follows instead of Crash */
    s32 unk26C;
    s32 unk270;
    s32 unk274;
    s32 unk278;
    s32 unk27C;
    s32 unk280;
    s32 unk284;
    s32 unk288; /* 0x288: camera zoom factor (0x100 = 1) */
    s32 unk28C;
    s32 unk290;
    s32 unk294;
    s32 unk298;
    s32 unk29C;
    s32 unk2A0;
    s32 unk2A4;
    s32 unk2A8;
    s32 unk2AC;
    s32 unk2B0;
    s32 unk2B4;
    s32 unk2B8;
    s32 unk2BC;
    s32 unk2C0;
    s32 unk2C4;
    s32 unk2C8;
    s32 unk2CC;
    s32 unk2D0;
    s32 unk2D4;
    s32 unk2D8;
    s32 unk2DC;
    s32 unk2E0;
    s32 unk2E4;
    s32 unk2E8;
    s32 unk2EC;
    s32 unk2F0;
    s32 unk2F4;
    s32 unk2F8;
    s32 unk2FC;
    s32 unk300;
    s32 unk304;
    s32 unk308;
    s32 unk30C;
    s32 unk310;
    s32 unk314;
    s32 unk318;
    s32 unk31C;
    s32 unk320;
    s32 unk324;
    s32 unk328;
    s32 unk32C;
    s32 unk330;
    s32 unk334;
    s32 unk338;
    s32 unk33C;
    s32 unk340;
    s32 unk344;
    Object *unk348;
    s32 unk34C;
    s32 unk350;
    s32 unk354;
    s32 unk358;
    s32 unk35C;
    s32 unk360;
    s32 unk364;
    s32 unk368;
    s32 unk36C;
    s32 unk370;
    s32 unk374;
    struct GoolObject *unk378;
    s32 unk37C;
    s32 unk380;
    s32 unk384[3]; /* 0x384 */
    void *unk390[1]; /* 0x390: unk5C records whose +4 is an object */
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
    u8 unk9C[0x9C];
    s32 unk138;
    u8 unk13C[0x4];
    s32 unk140[64];
    u8 unk240[0x138];
    u32 unk378;
} RenderState;

typedef struct {
    MATRIX unk0; /* 0x0: camera matrix */
    MATRIX unk20; /* 0x20: camera rotation for the GTE */
    MATRIX unk40; /* 0x40: copy of unk20 */
    Transform unk60; /* 0x60: camera transform */
    Transform unk84; /* 0x84: camera transform at the path point */
    s32 unkA8;
    u8 unkAC[0x18];
    s32 unkC4; /* GTE projection distance (H) */
    s32 unkC8;
    UnkCC *unkCC; /* 0xCC: the current zone (cur_zone) */
    void *unkD0; /* 0xD0: the previous zone */
    s32 unkD4; /* 0xD4: entity of the current zone */
    s32 unkD8; /* 0xD8: camera path item */
    s32 unkDC;
    s32 unkE0; /* 0xE0: progress along the path */
    s32 unkE4; /* 0xE4: last progress of the path */
    s32 unkE8; /* 0xE8: path point */
    s32 unkEC; /* 0xEC: path length */
    s32 unkF0; /* 0xF0: property 0x29 of the path */
    s32 unkF4;
    s32 unkF8;
    s32 unkFC;
    s32 unk100;
    s32 unk104;
    u8 unk108[0xC];
    s32 unk114; /* 0x114: camera yaw */
    s32 unk118[3]; /* 0x118: saved camera rotation */
    u8 unk124[0xC];
    s32 unk130[3]; /* 0x130: camera forward direction */
    u8 unk13C[0x8];
    u32 unk144;
    u8 unk148[0x18D8];
    RenderState render; /* 0x1A20 (D_800676B0) */
    u8 unk1D9C[0x4];
    s32 unk1DA0; /* default model type for animation types 2 and 5 */
    s32 unk1DA4; /* default model type for animation type 1 */
} Unk80065C90;

extern Unk80065C90 D_80065C90;

#endif /* GAME_H */
