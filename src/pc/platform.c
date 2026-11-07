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
    signal(SIGSEGV, SIG_DFL); /* a real crash */
}

void pc_sys_catch_ps_calls(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_segv;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(SIGSEGV, &sa, NULL);
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
