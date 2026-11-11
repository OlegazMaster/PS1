/* What the PC port needs from the system (Linux or Windows), kept here: the
 * system headers declare names that are also PSY-Q functions (OpenEvent,
 * EnterCriticalSection, LoadImage on Windows).
 *
 * The interrupts of the console: a signal every millisecond on Linux; on
 * Windows a timer thread that suspends the main thread while it runs the
 * handler. While they are blocked (a critical section of the game) nothing
 * runs; the next millisecond catches up. */
#define _GNU_SOURCE /* REG_EIP, ... (ucontext) */
#include "types.h"

#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#ifdef __ANDROID__
#include <pthread.h>
#endif
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#endif

/* zeroed memory at a fixed address: whether it worked */
s32 pc_sys_map(u32 addr, u32 size)
{
#ifdef _WIN32
    void *p = VirtualAlloc((void *)addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void *p = mmap((void *)addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
#endif
    return p == (void *)addr;
}

/* seconds, monotonic */
double pc_sys_now(void)
{
#ifdef _WIN32
    static LARGE_INTEGER freq;
    LARGE_INTEGER c;

    if (!freq.QuadPart) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)freq.QuadPart;
#else
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + t.tv_nsec * 1e-9;
#endif
}

void pc_sys_sleep(void)
{
#ifdef _WIN32
    Sleep(1);
#else
    struct timespec t = { 0, 500000 };

    nanosleep(&t, NULL);
#endif
}

static void (*interrupt)(void);

#if defined(__i386__) || (defined(_WIN32) && !defined(__ANDROID__))
/* x86: a load or store through a pointer below 2 MB (the game reads through
 * NULL objects: on the console that is the low mirror of the RAM). The
 * fault handler finds the base register of the instruction at ip, adds
 * 0x80000000 to it and runs the instruction once more, one step only; the
 * step handler then puts the register back (unless the instruction wrote
 * it). regs: eax, ecx, edx, ebx, esp, ebp, esi, edi as the instruction
 * numbers them. Returns the number of the register to rebase, or -1. */
static s32 low_base(const u8 *ip, u32 *const regs[8], u32 addr)
{
    u32 modrm, mod, rm, base;

    while (*ip == 0x66 || *ip == 0xF2 || *ip == 0xF3 || *ip == 0x2E || *ip == 0x3E || *ip == 0x26 ||
           *ip == 0x36 || *ip == 0x64 || *ip == 0x65 || *ip == 0xF0) {
        ip++; /* prefixes */
    }
    if (*ip == 0x0F) {
        ip++;
        if (*ip == 0x38 || *ip == 0x3A) {
            ip++;
        }
    } else if ((*ip >= 0xA0 && *ip <= 0xA7) || (*ip >= 0xAA && *ip <= 0xAF) || *ip == 0xD7) {
        return -1; /* absolute addresses, string instructions */
    }
    modrm = ip[1];
    mod = modrm >> 6;
    rm = modrm & 7;
    if (mod == 3) {
        return -1;
    }
    if (rm == 4) {
        base = ip[2] & 7;
        if (base == 5 && mod == 0) {
            return -1;
        }
    } else if (rm == 5 && mod == 0) {
        return -1;
    } else {
        base = rm;
    }
    if (*regs[base] >= 0x200000u || *regs[base] > addr || addr - *regs[base] >= 0x200000u) {
        return -1; /* (the index, not the base, would be the NULL pointer) */
    }
    return (s32)base;
}

static s32 low_reg = -1;      /* the rebased register (of the fault being stepped) */
static u32 low_was, low_now; /* its value before, after the rebase */
static u32 low_count;

static void low_log(u32 addr, u32 ip)
{
    if (low_count++ < 20) {
        fprintf(stderr, "port: an access at the low mirror %08X (instruction at %08X)\n", addr, ip);
    }
}
#endif

#if !defined(_WIN32) && !defined(__ANDROID__)
/* Native code that calls a PlayStation address (a pointer to a function in
 * the data of the game) faults at that address: the handler runs the call in
 * the interpreter (src/pc/mips.c) and returns to the caller with its result,
 * as if the call had been native. (Android: src/pc/android.c.) */
#include <ucontext.h>

u32 pc_mips_call(u32 addr, u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7);

static void on_segv(int sig, siginfo_t *si, void *ucv)
{
    ucontext_t *uc = ucv;
    u32 addr = (u32)si->si_addr;
#if defined(__i386__)
    greg_t *g = uc->uc_mcontext.gregs;
    u32 pc = (u32)g[REG_EIP];
    u32 *st = (u32 *)g[REG_ESP]; /* the return address, then the arguments */

    if (addr == pc && pc - 0x80010000u < 0x1F0000u && !(pc & 3)) {
        g[REG_EAX] = (greg_t)pc_mips_call(pc, st[1], st[2], st[3], st[4], st[5], st[6], st[7], st[8]);
        g[REG_EIP] = (greg_t)st[0];
        g[REG_ESP] += 4;
        return;
    }
    if (addr < 0x200000u && low_reg < 0) {
        u32 *const regs[8] = { (u32 *)&g[REG_EAX], (u32 *)&g[REG_ECX], (u32 *)&g[REG_EDX], (u32 *)&g[REG_EBX],
                               (u32 *)&g[REG_ESP], (u32 *)&g[REG_EBP], (u32 *)&g[REG_ESI], (u32 *)&g[REG_EDI] };
        s32 k = low_base((const u8 *)pc, regs, addr);

        if (k >= 0) {
            low_log(addr, pc);
            low_reg = k;
            low_was = *regs[k];
            low_now = *regs[k] |= 0x80000000u;
            g[REG_EFL] |= 0x100; /* one step: SIGTRAP */
            return;
        }
    }
#elif defined(__arm__)
    u32 *r = (u32 *)&uc->uc_mcontext.arm_r0; /* r0..r10, fp, ip, sp, lr, pc, cpsr */
    u32 pc = r[15];

    if (addr == pc && pc - 0x80010000u < 0x1F0000u && !(pc & 3)) {
        const u32 *st = (const u32 *)r[13];

        r[0] = pc_mips_call(pc, r[0], r[1], r[2], r[3], st[0], st[1], st[2], st[3]);
        r[15] = r[14] & ~1u;
        r[16] = (r[14] & 1) ? r[16] | 0x20 : r[16] & ~0x20u;
        return;
    }
#endif
    {
        void pc_shadow_fault(void);

        pc_shadow_fault(); /* a crash of the original code in the shadow check: back there */
    }
    signal(SIGSEGV, SIG_DFL); /* a real crash */
}

#if defined(__i386__)
/* the step after a rebased access: the register back */
static void on_trap(int sig, siginfo_t *si, void *ucv)
{
    ucontext_t *uc = ucv;
    greg_t *g = uc->uc_mcontext.gregs;

    if (low_reg >= 0) {
        u32 *const regs[8] = { (u32 *)&g[REG_EAX], (u32 *)&g[REG_ECX], (u32 *)&g[REG_EDX], (u32 *)&g[REG_EBX],
                               (u32 *)&g[REG_ESP], (u32 *)&g[REG_EBP], (u32 *)&g[REG_ESI], (u32 *)&g[REG_EDI] };

        if (*regs[low_reg] == low_now) {
            *regs[low_reg] = low_was;
        }
        low_reg = -1;
        g[REG_EFL] &= ~0x100;
    }
}
#endif

void pc_sys_catch_ps_calls(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_segv;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(SIGSEGV, &sa, NULL);
#if defined(__i386__)
    sa.sa_sigaction = on_trap;
    sigaction(SIGTRAP, &sa, NULL);
#endif
}
#elif defined(_WIN32)
/* Windows: a vectored exception handler does what the SIGSEGV handler does
 * on Linux. The RAM is not executable (DEP, on for the process at start), so
 * a native call of a PlayStation address faults at that address. */
u32 pc_mips_call(u32 addr, u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7);

static LONG CALLBACK on_exception(EXCEPTION_POINTERS *e)
{
    CONTEXT *c = e->ContextRecord;
    u32 pc = (u32)c->Eip;

    if (e->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && pc - 0x80010000u < 0x1F0000u &&
        !(pc & 3) && (u32)e->ExceptionRecord->ExceptionInformation[1] == pc) {
        u32 *st = (u32 *)c->Esp; /* the return address, then the arguments */

        c->Eax = pc_mips_call(pc, st[1], st[2], st[3], st[4], st[5], st[6], st[7], st[8]);
        c->Eip = st[0];
        c->Esp += 4;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (e->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && low_reg < 0 &&
        (u32)e->ExceptionRecord->ExceptionInformation[1] < 0x200000u) {
        u32 addr = (u32)e->ExceptionRecord->ExceptionInformation[1];
        u32 *const regs[8] = { (u32 *)&c->Eax, (u32 *)&c->Ecx, (u32 *)&c->Edx, (u32 *)&c->Ebx,
                               (u32 *)&c->Esp, (u32 *)&c->Ebp, (u32 *)&c->Esi, (u32 *)&c->Edi };
        s32 k = low_base((const u8 *)pc, regs, addr);

        if (k >= 0) {
            low_log(addr, pc);
            low_reg = k;
            low_was = *regs[k];
            low_now = *regs[k] |= 0x80000000u;
            c->EFlags |= 0x100; /* one step */
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    if (e->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP && low_reg >= 0) {
        u32 *const regs[8] = { (u32 *)&c->Eax, (u32 *)&c->Ecx, (u32 *)&c->Edx, (u32 *)&c->Ebx,
                               (u32 *)&c->Esp, (u32 *)&c->Ebp, (u32 *)&c->Esi, (u32 *)&c->Edi };

        if (*regs[low_reg] == low_now) {
            *regs[low_reg] = low_was;
        }
        low_reg = -1;
        c->EFlags &= ~0x100;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (e->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
        e->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION ||
        e->ExceptionRecord->ExceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        void pc_shadow_fault(void);

        pc_shadow_fault(); /* a crash of the original code in the shadow check: back there */
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* the diagnostics (src/pc/shadow.c): on when a file crash3_debug.txt is
 * next to crash3.exe (or in the current folder); its lines NAME=VALUE set
 * their variables (PC_FOCUS=22 ...) */
int pc_windows_debug(void)
{
    static int on = -1;

    if (on < 0) {
        char path[MAX_PATH + 32];
        char *slash;
        FILE *f;

        GetModuleFileNameA(NULL, path, MAX_PATH);
        slash = strrchr(path, '\\');
        strcpy(slash ? slash + 1 : path, "crash3_debug.txt");
        f = fopen(path, "r");
        if (!f) {
            f = fopen("crash3_debug.txt", "r");
        }
        on = f != NULL;
        if (f) {
            char line[200];

            while (fgets(line, sizeof(line), f)) {
                char *end = line + strlen(line);

                while (end > line && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ')) {
                    *--end = 0;
                }
                if (strncmp(line, "PC_", 3) == 0 && strchr(line, '=')) {
                    _putenv(line);
                    fprintf(stderr, "port: %s (crash3_debug.txt)\n", line);
                }
            }
            fclose(f);
        }
    }
    return on > 0;
}

/* every 3 s in the log: the time, the vertical blanks, the frames; when no
 * new frame came, where the game is (the instruction, the interpreter, the
 * function in the shadow check) */
static HANDLE game_thread;

static DWORD WINAPI monitor_thread(LPVOID arg)
{
    u32 pc_frames(void);
    u32 pc_vblanks(void);
    const char *pc_shadow_now(void);
    extern volatile u32 pc_mips_pc;
    u32 n = 0, last = 0, base = (u32)GetModuleHandleA(NULL);

    for (;;) {
        u32 f;

        Sleep(3000);
        f = pc_frames();
        fprintf(stderr, "port: alive %u s, %u vblanks, %u frames\n", n += 3, pc_vblanks(), f);
        if (f == last && f != 0) {
            CONTEXT c;

            memset(&c, 0, sizeof(c));
            c.ContextFlags = CONTEXT_CONTROL;
            SuspendThread(game_thread);
            GetThreadContext(game_thread, &c);
            ResumeThread(game_thread);
            fprintf(stderr, "port: no new frames; the game is at %08X (crash3.exe+%X), interpreter %08X%s%s\n",
                    (u32)c.Eip, (u32)c.Eip - base, pc_mips_pc, pc_shadow_now() ? ", shadow check of " : "",
                    pc_shadow_now() ? pc_shadow_now() : "");
        }
        fflush(stderr);
        last = f;
    }
    return 0;
}

/* a crash: what and where, in the log (the instruction relative to the
 * executable, for addr2line on crash3.exe; the interpreter's last pc) */
static LONG WINAPI on_crash(EXCEPTION_POINTERS *e)
{
    extern volatile u32 pc_mips_pc;
    u32 base = (u32)GetModuleHandleA(NULL);

    fprintf(stderr, "CRASH: exception %08X at %08X (crash3.exe+%X), address %08X, interpreter %08X\n",
            (u32)e->ExceptionRecord->ExceptionCode, (u32)e->ContextRecord->Eip, (u32)e->ContextRecord->Eip - base,
            (u32)e->ExceptionRecord->ExceptionInformation[1], pc_mips_pc);
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}

void pc_sys_catch_ps_calls(void)
{
    /* DEP for this 32-bit process (off by default on some systems) */
    BOOL(WINAPI * dep)(DWORD) = (BOOL(WINAPI *)(DWORD))GetProcAddress(GetModuleHandleA("kernel32.dll"),
                                                                      "SetProcessDEPPolicy");

    if (dep) {
        dep(1); /* PROCESS_DEP_ENABLE */
    }
    AddVectoredExceptionHandler(1, on_exception);
    SetUnhandledExceptionFilter(on_crash);
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &game_thread, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    CreateThread(NULL, 0, monitor_thread, NULL, 0, NULL);
}
#else
void pc_sys_catch_ps_calls(void)
{
}
#endif

#ifdef _WIN32
static HANDLE main_thread;
static volatile LONG blocked;

void pc_sys_block(s32 on)
{
    InterlockedExchange(&blocked, on);
}

/* blocks (or not) and gives the state before (the shadow check) */
s32 pc_sys_block_swap(s32 on)
{
    return (s32)InterlockedExchange(&blocked, on);
}

static DWORD WINAPI timer_thread(LPVOID arg)
{
    for (;;) {
        Sleep(1);
        if (blocked) {
            continue;
        }
        SuspendThread(main_thread);
        if (!blocked) { /* it may have blocked them meanwhile */
            interrupt();
        }
        ResumeThread(main_thread);
    }
    return 0;
}

/* calls func every millisecond, as an interrupt of the main thread */
void pc_sys_start_interrupts(void (*func)(void))
{
    interrupt = func;
    timeBeginPeriod(1);
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &main_thread, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    CreateThread(NULL, 0, timer_thread, NULL, 0, NULL);
}
#else
void pc_sys_block(s32 on)
{
    sigset_t s;

    sigemptyset(&s);
    sigaddset(&s, SIGALRM);
#ifdef __ANDROID__
    pthread_sigmask(on ? SIG_BLOCK : SIG_UNBLOCK, &s, NULL);
#else
    sigprocmask(on ? SIG_BLOCK : SIG_UNBLOCK, &s, NULL);
#endif
}

static void on_alarm(int sig)
{
    interrupt();
}

#ifdef __ANDROID__
/* the signal goes to the game thread only (the other threads of the app are
 * not ours): a timer thread sends it */
static pthread_t game;

static void *timer_thread(void *arg)
{
    for (;;) {
        struct timespec t = { 0, 1000000 };

        nanosleep(&t, NULL);
        pthread_kill(game, SIGALRM);
    }
    return NULL;
}
#endif

void pc_sys_start_interrupts(void (*func)(void))
{
    struct sigaction sa;

    interrupt = func;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_alarm;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);
#ifdef __ANDROID__
    {
        pthread_t t;

        game = pthread_self();
        pthread_create(&t, NULL, timer_thread, NULL);
    }
#else
    {
        struct itimerval it;

        it.it_interval.tv_sec = 0;
        it.it_interval.tv_usec = 1000;
        it.it_value = it.it_interval;
        setitimer(ITIMER_REAL, &it, NULL);
    }
#endif
}
#endif
