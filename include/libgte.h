#ifndef LIBGTE_H
#define LIBGTE_H

/* The parts of PSY-Q's libgte used by the game (layouts as in LIBGTE.H). */

#include "types.h"

typedef struct {
    s16 m[3][3];
    s32 t[3]; /* 0x14 */
} MATRIX; /* 0x20 */

typedef struct {
    s32 vx, vy, vz;
    s32 pad;
} VECTOR;

typedef struct {
    s16 vx, vy, vz;
    s16 pad;
} SVECTOR;

#endif /* LIBGTE_H */
