#ifndef PAD_H
#define PAD_H

#include "types.h"

/* A controller ("pad" in c1), behind the receive buffer of the PSX pad driver. */
typedef struct {
    u8 status; /* 0x0: 0 when a pad answered */
    u8 raw[0x23];
    u32 tapped; /* 0x24: buttons pressed this frame */
    u32 held; /* 0x28 */
    u32 held_prev; /* 0x2C */
    u32 tapped_prev; /* 0x30 */
    u32 held_prev2; /* 0x34 */
    u8 unk38[0x10];
    s32 unk48;
    u8 unk4C;
    u8 unk4D;
    u8 unk4E[0x2];
} Pad; /* 0x50 */

typedef struct {
    s32 count;
    Pad pads[2]; /* 0x4 */
} PadState;

extern PadState pads;

#endif /* PAD_H */
