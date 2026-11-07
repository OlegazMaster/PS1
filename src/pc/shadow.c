/* The shadow check: the C versions of the game functions against their
 * original code, on the real data of the game.
 *
 * tools/pc/build.py gives the global functions of the game that compute only
 * in the memory of the PlayStation (camera, collisions, objects, the GOOL
 * operations...) a wrapper that calls pc_shadow. For some of the calls of
 * each (the first ones, then the 100th, 1000th...), with the interrupts
 * held, it:
 *   - keeps the RAM, the scratchpad, the GTE and the memory near pointer
 *     arguments into the C stack;
 *   - runs the original code of the function in the interpreter
 *     (src/pc/mips.c; what it calls runs as C, without checks) and keeps
 *     what it leaves;
 *   - puts the memory back and runs the C version;
 *   - compares the results, and logs the first differences:
 *     "shadow: NAME (ADDR) call N: ...".
 * The game goes on with what the C version did. Enabled on Android, or with
 * PC_SHADOW=1 (PC_SHADOW=0 disables it). */
#include "types.h"
#include "gte_soft.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef u32 (*CFunc)(u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32);
extern u32 pc_mips_extra[4]; /* src/pc/mips.c: arguments 9 to 12 of the next call */

u32 pc_mips_call(u32 addr, u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7);
u32 pc_func_lookup(u32 psx);
u32 nd_native_lookup(u32 addr);
s32 pc_mips_in_stack(u32 a);
static u32 stack_here; /* about where the C stack is (pc_shadow) */
u32 pc_mips_stack_top(s32 set, u32 top);
extern u32 pc_mips_force;
extern volatile u32 pc_mips_pc;

#define RAM_SIZE 0x200000u
#define SCRATCH_SIZE 0x400u /* the part of the scratchpad the PlayStation has */
#define WINDOW 512u         /* the bytes kept after a pointer into the C stack */

static u8 ram_before[RAM_SIZE], ram_orig[RAM_SIZE];
static u8 scr_before[SCRATCH_SIZE], scr_orig[SCRATCH_SIZE];
static GteState gte_before, gte_orig;
static u8 win_before[12][WINDOW], win_orig[12][WINDOW];
static u8 *win_at[12];
static s32 nested, enabled = -1;
static u32 checks, differ_total, unchecked;
static s32 log_stops = 1; /* which functions could not be checked, and why */
static void *jump[5];       /* __builtin_setjmp: back from a crash of the original code */
volatile s32 pc_shadow_orig; /* the original code is running */
#define in_orig pc_shadow_orig
static const char *volatile stopped_by; /* why it stopped (NULL: a crash) */
u32 pc_shadow_steps;                    /* its instructions (src/pc/mips.c) */
static const char *volatile now; /* the function being checked */

const char *pc_shadow_now(void)
{
    return nested ? now : NULL;
}

/* from the handler of a crash (src/pc/android.c, src/pc/platform.c): a crash
 * of the original code the check runs goes back to the check */
void pc_shadow_fault(void)
{
    if (in_orig) {
        in_orig = 0;
        stopped_by = NULL;
        __builtin_longjmp(jump, 1);
    }
}

/* a function of the port with effects outside the PlayStation memory, or
 * too many instructions (a loop waiting for an interrupt): the run of the
 * original code ends there, and the function is not checked any more */
void pc_shadow_unsafe(const char *what)
{
    if (in_orig) {
        in_orig = 0;
        stopped_by = what;
        __builtin_longjmp(jump, 1);
    }
}

/* the calls of each function so far (by its address) */
static struct {
    u32 addr, calls, reports;
} counts[2048];

static u32 *count_of(u32 addr, u32 **reports)
{
    u32 h = (addr >> 2) * 2654435761u >> 21; /* 2048 slots */
    u32 i;

    for (i = 0; i < 2048; i++, h = (h + 1) & 2047) {
        if (counts[h].addr == addr || counts[h].addr == 0) {
            counts[h].addr = addr;
            *reports = &counts[h].reports;
            return &counts[h].calls;
        }
    }
    *reports = &counts[0].reports;
    return &counts[0].calls;
}

static s32 checked_call(u32 n)
{
    return n <= 3 || n == 100 || n == 1000 || n == 10000 || n == 100000;
}

/* the same value, written by the original code and by the port: a pointer
 * to a function (the port's address) or into the scratchpad (where the port
 * has it) */
static s32 same_word(u32 o, u32 n)
{
    if (o == n) {
        return 1;
    }
    if ((o & 0xFFFFC000u) == 0x1F800000u && n == o - 0x1F800000u + PSX_SCRATCH) {
        return 1;
    }
    if (o - 0x80010000u < 0x60000u && (pc_func_lookup(o) == n || nd_native_lookup(n) == o)) {
        return 1; /* a function: of the game, of nd_asm (the recompiled entry) */
    }
    if ((pc_mips_in_stack(o) || o - stack_here < 0x100000u) && n - stack_here < 0x100000u) {
        return 1; /* an address in the stack: of the interpreter, of the C code */
    }
    return 0;
}

/* the first different word of two copies of memory at addr (the PlayStation
 * address of a), how many differ */
static u32 compare(const u8 *a, const u8 *b, u32 len, u32 addr, u32 *first, u32 *va, u32 *vb)
{
    u32 i, n = 0;

    for (i = 0; i + 4 <= len; i += 4) {
        u32 x, y;

        if (memcmp(a + i, b + i, 4) == 0) {
            continue;
        }
        memcpy(&x, a + i, 4);
        memcpy(&y, b + i, 4);
        if (same_word(x, y)) {
            continue;
        }
        if (n++ == 0) {
            *first = addr + i;
            *va = x;
            *vb = y;
        }
    }
    return n;
}

#ifndef _WIN32
static sigset_t held;

static void hold(s32 on)
{
    sigset_t s;

    if (on) {
        sigemptyset(&s);
        sigaddset(&s, SIGALRM);
        sigprocmask(SIG_BLOCK, &s, &held);
    } else {
        sigprocmask(SIG_SETMASK, &held, NULL);
    }
}
#else
static void hold(s32 on)
{
}
#endif

static void save_windows(u8 (*to)[WINDOW])
{
    s32 i;

    for (i = 0; i < 12; i++) {
        if (win_at[i]) {
            memcpy(to[i], win_at[i], WINDOW);
        }
    }
}

static void load_windows(u8 (*from)[WINDOW])
{
    s32 i;

    for (i = 0; i < 12; i++) {
        if (win_at[i]) {
            memcpy(win_at[i], from[i], WINDOW);
        }
    }
}

u32 pc_shadow(const char *name, u32 addr, u32 native, s32 ret, u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5,
              u32 a6, u32 a7, u32 a8, u32 a9, u32 a10, u32 a11)
{
    /* (static: a window of the C stack may cover this frame) */
    static u32 args[12], *calls, *reports, r_orig, r_port, first, va, vb, n, mask, top;
    static s32 i;
    u8 *here = (u8 *)__builtin_frame_address(0);

    stack_here = (u32)here - 0x10000u; /* (the callees' frames are below) */

    if (enabled < 0) {
        const char *e = getenv("PC_SHADOW");
#ifdef __ANDROID__
        enabled = !e || atoi(e) != 0;
#else
        enabled = e && atoi(e) != 0;
#endif
        if (enabled) {
            fprintf(stderr, "port: the shadow check is on\n");
        }
    }
    if (nested || !enabled) {
        return ((CFunc)native)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11);
    }
    calls = count_of(addr, &reports);
    if (!checked_call(++*calls) || *reports >= 3) {
        return ((CFunc)native)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11);
    }
    nested = 1;
    now = name;
    hold(1);
    args[0] = a0, args[1] = a1, args[2] = a2, args[3] = a3, args[4] = a4, args[5] = a5, args[6] = a6, args[7] = a7;
    args[8] = a8, args[9] = a9, args[10] = a10, args[11] = a11;
    /* the arguments that point into the C stack (above this frame) */
    for (i = 0; i < 12; i++) {
        u8 *p = (u8 *)args[i];

        win_at[i] = p > here + 64 && p < here + 0x100000 ? p : NULL;
    }
    memcpy(ram_before, (void *)0x80000000u, RAM_SIZE);
    memcpy(scr_before, (void *)PSX_SCRATCH, SCRATCH_SIZE);
    gte_before = gte;
    save_windows(win_before);

    top = pc_mips_stack_top(0, 0);
    pc_mips_force = addr;
    in_orig = 1;
    pc_shadow_steps = 0;
    if (__builtin_setjmp(jump) == 0) {
        pc_mips_extra[0] = a8, pc_mips_extra[1] = a9, pc_mips_extra[2] = a10, pc_mips_extra[3] = a11;
        r_orig = pc_mips_call(addr, a0, a1, a2, a3, a4, a5, a6, a7);
        in_orig = 0;
    } else {
        /* the original code crashed (it reads something the port keeps
         * otherwise): no more checks of this function */
        pc_mips_force = 0;
        pc_mips_stack_top(1, top);
        if (!stopped_by) {
            fprintf(stderr, "shadow: %s (%08X) call %u: the original code crashed (at %08X); not checked any more\n",
                    name, addr, *calls, pc_mips_pc);
            *reports = 3;
        } else {
            /* (the next checked call may take another way: tried again) */
            if (*calls == 1) {
                unchecked++;
                if (log_stops) {
                    fprintf(stderr, "shadow: %s (%08X) not checked: it uses %s\n", name, addr, stopped_by);
                }
            }
        }
        memcpy((void *)0x80000000u, ram_before, RAM_SIZE);
        memcpy((void *)PSX_SCRATCH, scr_before, SCRATCH_SIZE);
        gte = gte_before;
        load_windows(win_before);
        hold(0);
        nested = 0;
        return ((CFunc)native)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11);
    }
    pc_mips_force = 0;
    memcpy(ram_orig, (void *)0x80000000u, RAM_SIZE);
    memcpy(scr_orig, (void *)PSX_SCRATCH, SCRATCH_SIZE);
    gte_orig = gte;
    save_windows(win_orig);

    memcpy((void *)0x80000000u, ram_before, RAM_SIZE);
    memcpy((void *)PSX_SCRATCH, scr_before, SCRATCH_SIZE);
    gte = gte_before;
    load_windows(win_before);
    r_port = ((CFunc)native)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11);
    checks++;

    mask = ret == 0 ? 0 : ret == 2 ? 0xFFFFu : ret == 3 ? 0xFFu : 0xFFFFFFFFu;
    if ((r_orig & mask) != (r_port & mask) && !same_word(r_orig, r_port)) {
        fprintf(stderr, "shadow: %s (%08X) call %u: returns %08X, the original %08X\n", name, addr, *calls,
                r_port, r_orig);
        ++*reports;
        differ_total++;
    }
    n = compare(ram_orig, (u8 *)0x80000000u, RAM_SIZE, 0x80000000u, &first, &va, &vb);
    if (n) {
        fprintf(stderr, "shadow: %s (%08X) call %u: %u words of the RAM differ, the first at %08X: %08X, the "
                "original %08X\n", name, addr, *calls, n, first, vb, va);
        ++*reports;
        differ_total++;
    }
    n = compare(scr_orig, (u8 *)PSX_SCRATCH, SCRATCH_SIZE, 0x1F800000u, &first, &va, &vb);
    if (n) {
        fprintf(stderr, "shadow: %s (%08X) call %u: %u words of the scratchpad differ, the first at %08X: %08X, "
                "the original %08X\n", name, addr, *calls, n, first, vb, va);
        ++*reports;
        differ_total++;
    }
    for (i = 0; i < 12; i++) {
        if (win_at[i] && (n = compare(win_orig[i], win_at[i], WINDOW, 0, &first, &va, &vb)) != 0) {
            fprintf(stderr, "shadow: %s (%08X) call %u: %u words differ after argument %d (a pointer to the C "
                    "stack), the first at +%X: %08X, the original %08X\n", name, addr, *calls, n, i, first, vb, va);
            ++*reports;
            differ_total++;
        }
    }
    n = compare((u8 *)&gte_orig, (u8 *)&gte, sizeof(gte), 0, &first, &va, &vb);
    if (n && *reports == 0) {
        fprintf(stderr, "shadow: %s (%08X) call %u: %u registers of the GTE differ, the first %s %u: %08X, the "
                "original %08X\n", name, addr, *calls, n, first < 128 ? "data" : "control", first / 4 % 32, vb,
                va);
        ++*reports;
    }
    if (checks % 200 == 0) {
        fprintf(stderr, "shadow: %u checks, %u differences (%u functions stopped at their first call)\n", checks,
                differ_total, unchecked);
    }
    hold(0);
    nested = 0;
    return r_port;
}
