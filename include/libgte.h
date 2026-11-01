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

typedef struct {
    s16 vx, vy;
} DVECTOR;

void MulMatrix(MATRIX *m0, MATRIX *m1); /* m0 = m0 * m1 */
void ReadGeomOffset(s32 *ofx, s32 *ofy);
void SetGeomOffset(s32 ofx, s32 ofy);
void SetGeomScreen(s32 h);
void SetRotMatrix(MATRIX *m);
MATRIX *TransMatrix(MATRIX *m, VECTOR *v);
void SetTransMatrix(MATRIX *m);
void RotTrans(SVECTOR *v0, VECTOR *v1, s32 *flag);
s32 RotTransPers(SVECTOR *v0, s32 *sxy, s32 *p, s32 *flag);
void ReadSZfifo3(s32 *sz0, s32 *sz1, s32 *sz2);

#endif /* LIBGTE_H */
