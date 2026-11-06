#ifndef GTE_INLINE_H
#define GTE_INLINE_H

/* GTE access as in PSY-Q's inline macros: data registers (mtc2/mfc2),
 * control registers (ctc2), the flag register, screen coordinates stored
 * straight to memory, and the perspective transforms. A port replaces
 * these with a software GTE. */
#define gte_mtc2(r, n) __asm__ volatile("mtc2 %0, $" #n : : "r"(r))
#define gte_mfc2(r, n) __asm__ volatile("mfc2 %0, $" #n : "=r"(r))
#define gte_ctc2(r, n) __asm__ volatile("ctc2 %0, $" #n : : "r"(r))
#define gte_stflg(r) __asm__ volatile("cfc2 %0, $31\n\tnop" : "=r"(r))
#define gte_stsxy(n, m) __asm__ volatile("swc2 $" #n ", %0" : "=m"(m))
#define gte_rtps() __asm__ volatile("nop\n\tnop\n\trtps")
#define gte_rtpt() __asm__ volatile("nop\n\tnop\n\trtpt")

#endif /* GTE_INLINE_H */
