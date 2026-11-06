#ifndef GTE_SOFT_H
#define GTE_SOFT_H

/* A software Geometry Transformation Engine (the PlayStation's COP2) for
 * the PC port: the 32 data and 32 control registers and the commands, with
 * the hardware's fixed-point rounding, saturation, flags and divider.
 * Written from the public hardware description (psx-spx). */

#include <stdint.h>

typedef struct {
    uint32_t data[32];
    uint32_t ctrl[32];
} GteState;

extern GteState gte;

/* mtc2 / mfc2 / ctc2 / cfc2 (with the hardware's read and write quirks) */
void gte_write_data(int reg, uint32_t value);
uint32_t gte_read_data(int reg);
void gte_write_ctrl(int reg, uint32_t value);
uint32_t gte_read_ctrl(int reg);

/* Runs a command: the low 25 bits of the cop2 instruction (opcode in bits
 * 0-5, lm bit 10, cv 13-14, v 15-16, mx 17-18, sf bit 19). */
void gte_command(uint32_t cmd);

/* The commands as the PSY-Q macros issue them. */
#define GTE_RTPS 0x0180001
#define GTE_RTPT 0x0280030
#define GTE_NCLIP 0x1400006
#define GTE_AVSZ3 0x158002D
#define GTE_AVSZ4 0x168002E
#define GTE_DPCS 0x0780010
#define GTE_DPCT 0x0F8002A
#define GTE_GPL 0x1A8003E

#endif /* GTE_SOFT_H */
