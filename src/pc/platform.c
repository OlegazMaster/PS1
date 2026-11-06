/* What the PC port needs from the system (Linux or Windows), kept here: the
 * system headers declare names that are also PSY-Q functions (OpenEvent,
 * EnterCriticalSection, LoadImage on Windows).
 *
 * The interrupts of the console: a signal every millisecond on Linux; on
 * Windows a timer thread that suspends the main thread while it runs the
 * handler. While they are blocked (a critical section of the game) nothing
 * runs; the next millisecond catches up. */
#include "types.h"

#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
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
    sigprocmask(on ? SIG_BLOCK : SIG_UNBLOCK, &s, NULL);
}

static void on_alarm(int sig)
{
    interrupt();
}

void pc_sys_start_interrupts(void (*func)(void))
{
    struct sigaction sa;
    struct itimerval it;

    interrupt = func;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_alarm;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 1000;
    it.it_value = it.it_interval;
    setitimer(ITIMER_REAL, &it, NULL);
}
#endif
