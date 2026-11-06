#ifndef GTE_INLINE_H
#define GTE_INLINE_H

/* GTE access as in PSY-Q's inline macros: data registers (mtc2/mfc2),
 * control registers (ctc2), the flag register, screen coordinates stored
 * straight to memory, and the perspective transforms. A port replaces
 * these with a software GTE. */
#ifndef TARGET_PSX
/* the PC port: the software GTE (src/pc/gte.c) */
#include "gte_soft.h"
#define gte_mtc2(r, n) gte_write_data(n, (u32)(r))
#define gte_mfc2(r, n) ((r) = gte_read_data(n))
#define gte_ctc2(r, n) gte_write_ctrl(n, (u32)(r))
#define gte_stflg(r) ((r) = gte_read_ctrl(31))
#define gte_stsxy(n, m) (*(u32 *)&(m) = gte_read_data(n))
#define gte_rtps() gte_command(0x0180001)
#define gte_rtpt() gte_command(0x0280030)
#else
#define gte_mtc2(r, n) __asm__ volatile("mtc2 %0, $" #n : : "r"(r))
#define gte_mfc2(r, n) __asm__ volatile("mfc2 %0, $" #n : "=r"(r))
#define gte_ctc2(r, n) __asm__ volatile("ctc2 %0, $" #n : : "r"(r))
#define gte_stflg(r) __asm__ volatile("cfc2 %0, $31\n\tnop" : "=r"(r))
#define gte_stsxy(n, m) __asm__ volatile("swc2 $" #n ", %0" : "=m"(m))
#define gte_rtps() __asm__ volatile("nop\n\tnop\n\trtps")
#define gte_rtpt() __asm__ volatile("nop\n\tnop\n\trtpt")
#endif

#endif /* GTE_INLINE_H */
