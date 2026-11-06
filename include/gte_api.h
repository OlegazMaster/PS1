#ifndef GTE_API_H
#define GTE_API_H

/* GTE access for code that runs both on the PlayStation and in the PC port:
 * inline COP2 instructions for the PlayStation build (and the emulator
 * tests, which run them on the software GTE), calls to the software GTE
 * (src/pc/gte.c) otherwise. Register numbers must be constants. */

#ifdef TARGET_PSX
#include "gte_inline.h"
#define GTE_WD(reg, v) gte_mtc2(v, reg)
#define GTE_RD(reg, v) gte_mfc2(v, reg)
#define GTE_WC(reg, v) gte_ctc2(v, reg)
#define GTE_RC(reg, v) __asm__ volatile("cfc2 %0, $" #reg : "=r"(v))
#define GTE_CMD(cmd) __asm__ volatile("cop2 %0" : : "i"(cmd))
#else
#include "gte_soft.h"
#define GTE_WD(reg, v) gte_write_data(reg, (u32)(v))
#define GTE_RD(reg, v) ((v) = gte_read_data(reg))
#define GTE_WC(reg, v) gte_write_ctrl(reg, (u32)(v))
#define GTE_RC(reg, v) ((v) = gte_read_ctrl(reg))
#define GTE_CMD(cmd) gte_command(cmd)
#endif

/* commands as the original code issues them */
#define GTE_MVMVA_RT_V0 0x0486012 /* sf, RT * V0, no translation */

#endif /* GTE_API_H */
