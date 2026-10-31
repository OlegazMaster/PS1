#ifndef GAME_H
#define GAME_H

#include "types.h"

typedef struct {
    u8 unk0[0x44];
    s32 unk44;
} Object;

/* Global game state. Only the fields used so far are named, at their offsets. */
typedef struct {
    u8 unk0[0x40];
    u8 handles[0x30]; /* 0x40 */
    u32 unk70;
    s32 unk74;
    u8 unk78[0xC];
    u32 display_flags; /* 0x84 */
    s32 unk88;
    u8 unk8C[0x4];
    s32 pause_obj; /* 0x90: a pointer or an error code */
    u8 unk94[0x10];
    s32 game_state; /* 0xA4 */
    u8 unkA8[0xBC];
    s32 unk164;
    u8 unk168[0x20];
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
    u8 unk20C[0x14];
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
} GameState;

extern GameState game;

#endif /* GAME_H */
