#ifndef PC_SHADOW_H
#define PC_SHADOW_H

#include "types.h"

/* The shadow check (src/pc/shadow.c): the functions of the port with effects
 * outside the memory of the PlayStation (the disc, the screen, the pad, the
 * interrupts) end a run of the original code being checked before they do
 * anything (the C version then runs alone, as without the check). */
extern volatile s32 pc_shadow_orig;
void pc_shadow_unsafe(const char *what);

#define SHADOW_UNSAFE()                  \
    do {                                 \
        if (pc_shadow_orig) {            \
            pc_shadow_unsafe(__func__);  \
        }                                \
    } while (0)

/* a function about to change the VRAM: the check keeps it (once per run of
 * the original code), puts it back for the C version and compares them */
void pc_shadow_vram(void);

#define SHADOW_VRAM()             \
    do {                          \
        if (pc_shadow_orig) {     \
            pc_shadow_vram();     \
        }                         \
    } while (0)

#endif /* PC_SHADOW_H */
