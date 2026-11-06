/* Interrupts, events and timers of the PC port (libetc, libapi): a signal
 * every millisecond stands for the interrupts of the console. It runs the
 * handlers the game installed at their rates: the root counter 2 event (the
 * game's tick, about 1030 per second) and the vertical blank callback (60
 * per second). EnterCriticalSection/ExitCriticalSection block the signal, as
 * they disable the interrupts on the console. VSync waits for vertical
 * blanks and shows the frame. */
#include "types.h"

#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

void pc_video_present(void);
void pc_video_poll(void);

#define CPU_HZ 33868800.0
#define VBLANK_HZ 60.0
#define MAX_EVENTS 16

typedef struct {
    u32 desc;
    s32 spec, mode;
    void (*func)(void);
    s32 enabled, used;
} Event;

static Event events[MAX_EVENTS];
static void (*vsync_cb)(void);
static volatile u32 vblanks;
static double rcnt_period[3]; /* seconds per interrupt, 0: stopped */
static s32 rcnt_started[3];
static double next_vblank, next_rcnt[3];
static s32 started;
static s32 crit_depth;

static double now(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + t.tv_nsec * 1e-9;
}

static void fire(u32 desc)
{
    s32 i;

    for (i = 0; i < MAX_EVENTS; i++) {
        if (events[i].used && events[i].enabled && events[i].desc == desc && events[i].func) {
            events[i].func();
        }
    }
}

static void tick(int sig)
{
    double t = now();
    s32 i, n;

    for (n = 0; next_vblank <= t && n < 8; n++) {
        next_vblank += 1.0 / VBLANK_HZ;
        vblanks++;
        if (vsync_cb) {
            vsync_cb();
        }
    }
    if (next_vblank <= t) {
        next_vblank = t; /* far behind (a debugger stop): do not catch up */
    }
    for (i = 0; i < 3; i++) {
        if (!rcnt_started[i] || rcnt_period[i] <= 0) {
            continue;
        }
        for (n = 0; next_rcnt[i] <= t && n < 200; n++) {
            next_rcnt[i] += rcnt_period[i];
            fire(0xF2000000 | i);
        }
        if (next_rcnt[i] <= t) {
            next_rcnt[i] = t;
        }
    }
}

static void block(s32 on)
{
    sigset_t s;

    sigemptyset(&s);
    sigaddset(&s, SIGALRM);
    sigprocmask(on ? SIG_BLOCK : SIG_UNBLOCK, &s, NULL);
}

static void start(void)
{
    struct sigaction sa;
    struct itimerval it;

    if (started) {
        return;
    }
    started = 1;
    next_vblank = now() + 1.0 / VBLANK_HZ;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = tick;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 1000;
    it.it_value = it.it_interval;
    setitimer(ITIMER_REAL, &it, NULL);
}

/* ---- libapi ---- */

void EnterCriticalSection(void)
{
    if (crit_depth++ == 0) {
        block(1);
    }
}

void ExitCriticalSection(void)
{
    if (crit_depth > 0 && --crit_depth == 0) {
        block(0);
    }
}

s32 OpenEvent(u32 desc, s32 spec, s32 mode, void (*func)(void))
{
    s32 i;

    start();
    for (i = 0; i < MAX_EVENTS; i++) {
        if (!events[i].used) {
            events[i].used = 1;
            events[i].desc = desc;
            events[i].spec = spec;
            events[i].mode = mode;
            events[i].func = func;
            events[i].enabled = 0;
            return (s32)(0xF1000000 | i);
        }
    }
    return -1;
}

static Event *event(s32 id)
{
    u32 i = (u32)id & 0xFFFF;

    return ((u32)id >> 24) == 0xF1 && i < MAX_EVENTS && events[i].used ? &events[i] : NULL;
}

s32 EnableEvent(s32 id)
{
    Event *e = event(id);

    if (e) {
        e->enabled = 1;
    }
    return e != NULL;
}

s32 DisableEvent(s32 id)
{
    Event *e = event(id);

    if (e) {
        e->enabled = 0;
    }
    return e != NULL;
}

s32 CloseEvent(s32 id)
{
    Event *e = event(id);

    if (e) {
        e->used = 0;
    }
    return e != NULL;
}

/* a root counter: spec 0xF2000000 + n; counter 2 counts the CPU clock / 8 */
s32 SetRCnt(u32 spec, u16 target, s32 mode)
{
    u32 n = spec & 3;

    if (n < 3 && target) {
        rcnt_period[n] = target / (n == 2 ? CPU_HZ / 8 : CPU_HZ);
    }
    return 1;
}

s32 StartRCnt(u32 spec)
{
    u32 n = spec & 3;

    start();
    if (n < 3) {
        rcnt_started[n] = 1;
        next_rcnt[n] = now() + rcnt_period[n];
    }
    return 1;
}

s32 StopRCnt(u32 spec)
{
    u32 n = spec & 3;

    if (n < 3) {
        rcnt_started[n] = 0;
    }
    return 1;
}

void FlushCache(void)
{
}

/* ---- libetc ---- */

void ResetCallback(void)
{
    vsync_cb = NULL;
}

void VSyncCallback(void (*func)(void))
{
    start();
    vsync_cb = func;
}

/* -1: the number of vertical blanks; 0: waits for the next one; n: waits
 * for n after the last call; 1: (the time since the last) 0 */
s32 VSync(s32 mode)
{
    static u32 last;
    u32 target;

    start();
    if (mode < 0) {
        return (s32)vblanks;
    }
    if (mode == 1) {
        return 0;
    }
    target = mode == 0 ? vblanks + 1 : last + (u32)mode;
    if ((s32)(target - vblanks) > 0) {
        /* the interrupts may be blocked by the caller: let them through */
        if (crit_depth) {
            block(0);
        }
        while ((s32)(target - vblanks) > 0) {
            struct timespec t = { 0, 500000 };

            nanosleep(&t, NULL);
        }
        if (crit_depth) {
            block(1);
        }
    }
    last = vblanks;
    pc_video_present();
    pc_video_poll();
    return 0;
}
