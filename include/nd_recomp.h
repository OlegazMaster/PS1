#ifndef ND_RECOMP_H
#define ND_RECOMP_H

/* Support for the C that tools/recomp.py generates from hand-written
 * assembly (build/us/nd_recomp.c). The generated code keeps the registers in
 * variables and uses PlayStation addresses for its own code; memory and the
 * GTE go through the macros below.
 *
 * RC_MIPS: the code is compiled for MIPS and runs in the emulator tests next
 * to the original (GTE as inline COP2 instructions, which the test performs
 * on the software GTE; memory is the emulated memory).
 * otherwise: the PC port (software GTE, scratchpad in an array). */

#include "types.h"

typedef struct RcRegs {
    u32 r[32];
    u32 hi, lo;
    u32 ld_reg, ld_old; /* a load in the delay slot of jr/jalr, not yet seen */
} RcRegs;

/* the return address that ends a call of the generated code */
#ifndef RC_RETURN
#define RC_RETURN 0xFFFFFFF0u
#endif
/* the value of $gp in the game */
#define RC_GP 0x80060878u

/* called before each instruction (a trace for debugging the recompiler) */
#ifdef RC_TRACE
void rc_step(u32 pc);
#define RC_STEP(pc) rc_step(pc)
#else
#define RC_STEP(pc)
#endif

/* %hi/%lo of a symbol outside the segment */
#define RC_HI(sym) ((((u32)(sym)) + 0x8000u) & 0xFFFF0000u)
#define RC_LO(sym) ((u32)(s32)(s16)((u32)(sym) & 0xFFFFu))

/* memory at PlayStation addresses: the port maps the scratchpad at
 * 0x1F800000 as the C code of the game uses it there too */
#define RC_PTR(a) ((u8 *)(a))

#define RC_LB(a) (*(s8 *)RC_PTR(a))
#define RC_LBU(a) (*(u8 *)RC_PTR(a))
#define RC_LH(a) (*(s16 *)RC_PTR(a))
#define RC_LHU(a) (*(u16 *)RC_PTR(a))
#define RC_LW(a) (*(u32 *)RC_PTR(a))
#define RC_SB(a, v) (*(u8 *)RC_PTR(a) = (u8)(v))
#define RC_SH(a, v) (*(u16 *)RC_PTR(a) = (u16)(v))
#define RC_SW(a, v) (*(u32 *)RC_PTR(a) = (u32)(v))

static inline u32 rc_lwl(u32 a, u32 old)
{
    u32 w = RC_LW(a & ~3u);
    u32 sh = (3 - (a & 3)) * 8;
    return (old & (0x00FFFFFFu >> (24 - sh))) | (w << sh);
}

static inline u32 rc_lwr(u32 a, u32 old)
{
    u32 w = RC_LW(a & ~3u);
    u32 sh = (a & 3) * 8;
    return (old & ~(0xFFFFFFFFu >> sh)) | (w >> sh);
}

static inline void rc_swl(u32 a, u32 v)
{
    u32 *p = (u32 *)RC_PTR(a & ~3u);
    u32 sh = (3 - (a & 3)) * 8;
    *p = (*p & ~(0xFFFFFFFFu >> sh)) | (v >> sh);
}

static inline void rc_swr(u32 a, u32 v)
{
    u32 *p = (u32 *)RC_PTR(a & ~3u);
    u32 sh = (a & 3) * 8;
    *p = (*p & ~(0xFFFFFFFFu << sh)) | (v << sh);
}

/* division as the R3000 does it, by zero too */
static inline void rc_div(u32 *lo, u32 *hi, u32 a, u32 b)
{
    if (b == 0) {
        *lo = (s32)a < 0 ? 1 : 0xFFFFFFFFu;
        *hi = a;
    } else if (a == 0x80000000u && b == 0xFFFFFFFFu) {
        *lo = a;
        *hi = 0;
    } else {
        *lo = (u32)((s32)a / (s32)b);
        *hi = (u32)((s32)a % (s32)b);
    }
}

static inline void rc_divu(u32 *lo, u32 *hi, u32 a, u32 b)
{
    if (b == 0) {
        *lo = 0xFFFFFFFFu;
        *hi = a;
    } else {
        *lo = a / b;
        *hi = a % b;
    }
}

#ifdef RC_MIPS
/* the GTE through one function, which the test performs on the software GTE
 * (op: 0x000 + reg mtc2, 0x100 + reg mfc2, 0x200 + reg ctc2, 0x300 + reg
 * cfc2, 0x400 a command) */
u32 rc_gte_op(u32 op, u32 v);
#define RC_MTC2(reg, v) rc_gte_op(0x000 + (reg), (u32)(v))
#define RC_MFC2(reg, v) ((v) = rc_gte_op(0x100 + (reg), 0))
#define RC_CTC2(reg, v) rc_gte_op(0x200 + (reg), (u32)(v))
#define RC_CFC2(reg, v) ((v) = rc_gte_op(0x300 + (reg), 0))
#define RC_COP2(cmd) rc_gte_op(0x400, cmd)
#define RC_BREAK(addr, code) __builtin_trap()
#define RC_SYSCALL(addr) __builtin_trap()
#define RC_BAD(addr, word) __builtin_trap()
/* calls a function of the game as the original does: with $sp and $gp of
 * the recompiled code (the callee reads arguments 5.. from its stack) */
u32 rc_ext_tramp(u32 a0, u32 a1, u32 a2, u32 a3, u32 pc, u32 sp, u32 gp);
#define RC_EXT_CALL(pc, a0, a1, a2, a3, sp) rc_ext_tramp(a0, a1, a2, a3, pc, sp, r28)
#ifdef RC_TEST
__asm__(".text\n"
        ".set noreorder\n"
        ".globl rc_ext_tramp\n"
        "rc_ext_tramp:\n"
        "  addiu $sp, $sp, -32\n"
        "  sw $ra, 16($sp)\n"
        "  sw $gp, 20($sp)\n"
        "  sw $s0, 24($sp)\n"
        "  lw $t0, 48($sp)\n"
        "  lw $t1, 52($sp)\n"
        "  lw $t2, 56($sp)\n"
        "  move $s0, $sp\n"
        "  move $sp, $t1\n"
        "  jalr $t0\n"
        "  move $gp, $t2\n"
        "  move $sp, $s0\n"
        "  lw $ra, 16($sp)\n"
        "  lw $gp, 20($sp)\n"
        "  lw $s0, 24($sp)\n"
        "  jr $ra\n"
        "  addiu $sp, $sp, 32\n"
        ".set reorder\n");
#endif
#else
#include "gte_soft.h"
#define RC_MTC2(reg, v) gte_write_data(reg, (u32)(v))
#define RC_CTC2(reg, v) gte_write_ctrl(reg, (u32)(v))
#define RC_MFC2(reg, v) ((v) = gte_read_data(reg))
#define RC_CFC2(reg, v) ((v) = gte_read_ctrl(reg))
#define RC_COP2(cmd) gte_command(cmd)
void rc_trap(u32 addr, u32 what);
#define RC_BREAK(addr, code) rc_trap(addr, code)
#define RC_SYSCALL(addr) rc_trap(addr, 0)
#define RC_BAD(addr, word) rc_trap(addr, word)
/* a procedure of the segment by the address of its C wrapper (0: none) */
u32 rc_native_lookup(u32 addr);
/* calls a C function by its address (4 arguments in registers) */
typedef u32 (*RcExtFunc)(u32, u32, u32, u32);
#define RC_EXT_CALL(pc, a0, a1, a2, a3, sp) ((RcExtFunc)(pc))(a0, a1, a2, a3)
#endif

/* runs the generated code from addr with the arguments of a C call; the
 * stack is a local array, arguments 5..8 at sp + 0x10 as in the original */
static inline u32 rc_call(u32 (*exec)(u32, RcRegs *), u32 addr, u32 a0, u32 a1, u32 a2, u32 a3,
                          u32 a4, u32 a5, u32 a6, u32 a7)
{
    u32 stack[0x200];
    RcRegs regs;
    u32 i;

    for (i = 0; i < 32; i++) {
        regs.r[i] = 0;
    }
    regs.hi = regs.lo = 0;
    regs.ld_reg = regs.ld_old = 0;
    regs.r[4] = a0;
    regs.r[5] = a1;
    regs.r[6] = a2;
    regs.r[7] = a3;
    regs.r[29] = (u32)&stack[0x1F0];
    stack[0x1F4] = a4;
    stack[0x1F5] = a5;
    stack[0x1F6] = a6;
    stack[0x1F7] = a7;
    regs.r[28] = RC_GP;
    regs.r[31] = RC_RETURN;
    return exec(addr, &regs);
}

#ifdef RC_TEST
/* the entry of the emulator tests: runs the code at rc_target with the
 * registers in rc_state (the arguments, $sp, $gp and $ra set as for a call)
 * and leaves the registers there */
RcRegs rc_state;
u32 rc_target;
u32 RC_TEST(u32 pc, RcRegs *regs);

u32 rc_go(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7)
{
    u32 stack[0x200];

    rc_state.r[4] = a0;
    rc_state.r[5] = a1;
    rc_state.r[6] = a2;
    rc_state.r[7] = a3;
    rc_state.r[29] = (u32)&stack[0x1F0];
    stack[0x1F4] = a4;
    stack[0x1F5] = a5;
    stack[0x1F6] = a6;
    stack[0x1F7] = a7;
    rc_state.r[28] = RC_GP;
    rc_state.r[31] = RC_RETURN;
    return RC_TEST(rc_target, &rc_state);
}

/* runs the code at rc_target with all the registers taken from rc_state */
u32 rc_go_state(void)
{
    return RC_TEST(rc_target, &rc_state);
}
#endif

#endif /* ND_RECOMP_H */
