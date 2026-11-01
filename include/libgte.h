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

void MulMatrix(MATRIX *m0, MATRIX *m1); /* m0 = m0 * m1 */
void ReadGeomOffset(s32 *ofx, s32 *ofy);
void SetGeomOffset(s32 ofx, s32 ofy);
void SetGeomScreen(s32 h);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
void RotTrans(SVECTOR *v0, VECTOR *v1, s32 *flag);

#endif /* LIBGTE_H */
