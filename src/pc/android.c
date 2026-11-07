/* The Android side of the PC port: a NativeActivity (no Java code).
 *
 * The game runs in a thread of its own (its main() never returns); the
 * frames it shows (pc_video_present) are drawn into the window of the app,
 * 4:3 in the middle, with the touch buttons over it. Touches and the keys of
 * a game controller become the buttons of the pad. Messages (where the disc
 * image is looked for, what went wrong) are shown on the screen and written
 * to Crash3/log.txt.
 *
 * The disc image (.cue with its .bin, .bin or .iso) is looked for in
 * /sdcard/Crash3, /sdcard/Download and the folder of the app
 * (Android/data/<package>/files). Reading /sdcard needs the permission to
 * read files, which the app asks for at its start. */
#include "types.h"
#include "android_ndk.h"

#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* src/pc/startup.c */
s32 pc_start(const char *disc, const char *exe);
/* src/pc/pad.c */
void pc_pad_set(u16 buttons);

static ANativeActivity *act;
static ANativeWindow *volatile window;
static volatile s32 window_lock;
static ALooper *input_looper;
static char status[8][96];
static s32 status_lines;
static FILE *logf;

/* ---- a small lock between the game thread and the callbacks ---- */

static void lock(void)
{
    while (__atomic_exchange_n(&window_lock, 1, __ATOMIC_ACQUIRE)) {
        struct timespec t = { 0, 100000 };

        nanosleep(&t, NULL);
    }
}

static void unlock(void)
{
    __atomic_store_n(&window_lock, 0, __ATOMIC_RELEASE);
}

/* ---- messages ---- */

static void say(const char *fmt, ...)
{
    char line[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    __android_log_write(ANDROID_LOG_INFO, "crash3", line);
    if (logf) {
        fprintf(logf, "%s\n", line);
        fflush(logf);
    }
    if (status_lines == 8) {
        memmove(status[0], status[1], sizeof(status[0]) * 7);
        status_lines = 7;
    }
    snprintf(status[status_lines++], sizeof(status[0]), "%s", line);
}

/* a 5x7 font: rows of 5 bits, for the characters of the messages */
static const struct {
    char c;
    u8 rows[7];
} font[] = {
    { 'A', { 14, 17, 17, 31, 17, 17, 17 } }, { 'B', { 30, 17, 17, 30, 17, 17, 30 } },
    { 'C', { 14, 17, 16, 16, 16, 17, 14 } }, { 'D', { 30, 17, 17, 17, 17, 17, 30 } },
    { 'E', { 31, 16, 16, 30, 16, 16, 31 } }, { 'F', { 31, 16, 16, 30, 16, 16, 16 } },
    { 'G', { 14, 17, 16, 23, 17, 17, 15 } }, { 'H', { 17, 17, 17, 31, 17, 17, 17 } },
    { 'I', { 14, 4, 4, 4, 4, 4, 14 } },      { 'J', { 7, 2, 2, 2, 2, 18, 12 } },
    { 'K', { 17, 18, 20, 24, 20, 18, 17 } }, { 'L', { 16, 16, 16, 16, 16, 16, 31 } },
    { 'M', { 17, 27, 21, 21, 17, 17, 17 } }, { 'N', { 17, 17, 25, 21, 19, 17, 17 } },
    { 'O', { 14, 17, 17, 17, 17, 17, 14 } }, { 'P', { 30, 17, 17, 30, 16, 16, 16 } },
    { 'Q', { 14, 17, 17, 17, 21, 18, 13 } }, { 'R', { 30, 17, 17, 30, 20, 18, 17 } },
    { 'S', { 15, 16, 16, 14, 1, 1, 30 } },   { 'T', { 31, 4, 4, 4, 4, 4, 4 } },
    { 'U', { 17, 17, 17, 17, 17, 17, 14 } }, { 'V', { 17, 17, 17, 17, 17, 10, 4 } },
    { 'W', { 17, 17, 17, 21, 21, 21, 10 } }, { 'X', { 17, 17, 10, 4, 10, 17, 17 } },
    { 'Y', { 17, 17, 10, 4, 4, 4, 4 } },     { 'Z', { 31, 1, 2, 4, 8, 16, 31 } },
    { '0', { 14, 17, 19, 21, 25, 17, 14 } }, { '1', { 4, 12, 4, 4, 4, 4, 14 } },
    { '2', { 14, 17, 1, 2, 4, 8, 31 } },     { '3', { 31, 2, 4, 2, 1, 17, 14 } },
    { '4', { 2, 6, 10, 18, 31, 2, 2 } },     { '5', { 31, 16, 30, 1, 1, 17, 14 } },
    { '6', { 6, 8, 16, 30, 17, 17, 14 } },   { '7', { 31, 1, 2, 4, 8, 8, 8 } },
    { '8', { 14, 17, 17, 14, 17, 17, 14 } }, { '9', { 14, 17, 17, 15, 1, 2, 12 } },
    { '.', { 0, 0, 0, 0, 0, 12, 12 } },      { ',', { 0, 0, 0, 0, 12, 4, 8 } },
    { ':', { 0, 12, 12, 0, 12, 12, 0 } },    { '/', { 1, 1, 2, 4, 8, 16, 16 } },
    { '-', { 0, 0, 0, 31, 0, 0, 0 } },       { '_', { 0, 0, 0, 0, 0, 0, 31 } },
    { '!', { 4, 4, 4, 4, 4, 0, 4 } },        { '?', { 14, 17, 1, 2, 4, 0, 4 } },
    { '(', { 2, 4, 8, 8, 8, 4, 2 } },        { ')', { 8, 4, 2, 2, 2, 4, 8 } },
    { '>', { 8, 4, 2, 1, 2, 4, 8 } },        { '=', { 0, 0, 31, 0, 31, 0, 0 } },
    { '%', { 24, 25, 2, 4, 8, 19, 3 } },     { '\'', { 4, 4, 8, 0, 0, 0, 0 } },
};

static void text(u32 *px, s32 stride, s32 w, s32 h, s32 x, s32 y, s32 scale, const char *s, u32 color)
{
    for (; *s; s++, x += 6 * scale) {
        char c = *s >= 'a' && *s <= 'z' ? *s - 32 : *s;
        u32 i, r, b, dx, dy;

        for (i = 0; i < sizeof(font) / sizeof(font[0]) && font[i].c != c; i++) {
        }
        if (i == sizeof(font) / sizeof(font[0])) {
            continue;
        }
        for (r = 0; r < 7; r++) {
            for (b = 0; b < 5; b++) {
                if (!(font[i].rows[r] & (16 >> b))) {
                    continue;
                }
                for (dy = 0; dy < (u32)scale; dy++) {
                    for (dx = 0; dx < (u32)scale; dx++) {
                        s32 xx = x + (s32)(b * scale + dx), yy = y + (s32)(r * scale + dy);

                        if (xx >= 0 && xx < w && yy >= 0 && yy < h) {
                            px[yy * stride + xx] = color;
                        }
                    }
                }
            }
        }
    }
}

/* ---- the touch buttons ---- */

#define BUF_H 720 /* the height of the window buffer (the system scales it) */

typedef struct {
    float x, y, r; /* in buffer pixels */
    u16 bit;
    const char *label;
} TouchButton;

static TouchButton buttons[14];
static s32 nbuttons;
static s32 buf_w = 1152;
static volatile u16 touch_held, key_held;
static float touch_x[10], touch_y[10];
static s32 touch_down[10];

static void layout(s32 w)
{
    float s = BUF_H / 10.0f;
    s32 n = 0;

#define B(xx, yy, rr, b, l) (buttons[n].x = (xx), buttons[n].y = (yy), buttons[n].r = (rr), buttons[n].bit = (b), \
                             buttons[n].label = (l), n++)
    B(1.6f * s, 6.2f * s, 0.75f * s, 0x1000, "");         /* up */
    B(1.6f * s, 8.6f * s, 0.75f * s, 0x4000, "");         /* down */
    B(0.4f * s + 0.0f, 7.4f * s, 0.75f * s, 0x8000, "");  /* left */
    B(2.8f * s, 7.4f * s, 0.75f * s, 0x2000, "");         /* right */
    B(w - 1.8f * s, 8.6f * s, 0.8f * s, 0x0040, "X");     /* cross: jump */
    B(w - 0.6f * s, 7.4f * s, 0.8f * s, 0x0020, "O");     /* circle */
    B(w - 3.0f * s, 7.4f * s, 0.8f * s, 0x0080, "SQ");    /* square: spin */
    B(w - 1.8f * s, 6.2f * s, 0.8f * s, 0x0010, "TR");    /* triangle */
    B(w / 2.0f + 1.2f * s, 0.6f * s, 0.5f * s, 0x0800, "START");
    B(w / 2.0f - 1.2f * s, 0.6f * s, 0.5f * s, 0x0100, "SEL");
    B(0.8f * s, 0.8f * s, 0.6f * s, 0x0004, "L1");
    B(w - 0.8f * s, 0.8f * s, 0.6f * s, 0x0008, "R1");
    B(0.8f * s, 2.3f * s, 0.6f * s, 0x0001, "L2");
    B(w - 0.8f * s, 2.3f * s, 0.6f * s, 0x0002, "R2");
#undef B
    nbuttons = n;
}

static void update_touch(void)
{
    u16 b = 0;
    s32 i, j;

    for (i = 0; i < 10; i++) {
        if (!touch_down[i]) {
            continue;
        }
        for (j = 0; j < nbuttons; j++) {
            float dx = touch_x[i] - buttons[j].x, dy = touch_y[i] - buttons[j].y;
            float r = buttons[j].r * 1.25f;

            if (dx * dx + dy * dy < r * r) {
                b |= buttons[j].bit;
            }
        }
    }
    touch_held = b;
    pc_pad_set(touch_held | key_held);
}

static void draw_buttons(u32 *px, s32 stride, s32 w, s32 h)
{
    s32 j, x, y;

    for (j = 0; j < nbuttons; j++) {
        TouchButton *b = &buttons[j];
        s32 r = (s32)b->r, pressed = (touch_held | key_held) & b->bit;

        for (y = -r; y <= r; y++) {
            for (x = -r; x <= r; x++) {
                s32 d = x * x + y * y, xx = (s32)b->x + x, yy = (s32)b->y + y;

                if (d > r * r || xx < 0 || xx >= w || yy < 0 || yy >= h) {
                    continue;
                }
                if (d > (r - 3) * (r - 3) || pressed) {
                    u32 c = px[yy * stride + xx];
                    /* half-transparent white */
                    px[yy * stride + xx] = ((c & 0xFEFEFE) >> 1) + 0x7F7F7F;
                }
            }
        }
        text(px, stride, w, h, (s32)b->x - (s32)strlen(b->label) * 6 + 1, (s32)b->y - 7, 2, b->label, 0xFFFFFF);
    }
}

/* ---- the screen ---- */

/* a frame of the game (w x h, 0xRRGGBB), or NULL: the messages only */
void pc_android_present(const u32 *rgb, s32 fw, s32 fh)
{
    ANativeWindow_Buffer buf;
    u32 *px;
    s32 x, y, i;

    lock();
    if (!window || ANativeWindow_lock(window, &buf, NULL) != 0) {
        unlock();
        return;
    }
    px = buf.bits;
    for (y = 0; y < buf.height; y++) {
        memset(px + y * buf.stride, 0, buf.width * 4);
    }
    if (rgb && fw > 0 && fh > 0) {
        /* 4:3 in the middle, nearest pixel */
        s32 ch = buf.height, cw = ch * 4 / 3, cx = (buf.width - cw) / 2;
        static s32 xmap[4096];

        if (cw > 4096) {
            cw = 4096;
        }
        for (x = 0; x < cw; x++) {
            xmap[x] = x * fw / cw;
        }
        for (y = 0; y < ch; y++) {
            const u32 *src = rgb + (y * fh / ch) * fw;
            u32 *dst = px + y * buf.stride + cx;

            for (x = 0; x < cw; x++) {
                u32 c = src[xmap[x]];
                /* RGBX in memory: R, G, B, X */
                dst[x] = (c >> 16 & 0xFF) | (c & 0xFF00) | (c & 0xFF) << 16;
            }
        }
    }
    if (!rgb) {
        for (i = 0; i < status_lines; i++) {
            text(px, buf.stride, buf.width, buf.height, 20, 20 + i * 40, 4, status[i], 0xFFFFFF);
        }
    }
    draw_buttons(px, buf.stride, buf.width, buf.height);
    ANativeWindow_unlockAndPost(window);
    unlock();
}

static void set_window(ANativeWindow *w)
{
    lock();
    if (window) {
        ANativeWindow_release(window);
    }
    window = w;
    if (w) {
        s32 ww = ANativeWindow_getWidth(w), wh = ANativeWindow_getHeight(w);

        ANativeWindow_acquire(w);
        buf_w = wh > 0 ? BUF_H * ww / wh : 1152;
        if (buf_w < BUF_H * 4 / 3) {
            buf_w = BUF_H * 4 / 3;
        }
        ANativeWindow_setBuffersGeometry(w, buf_w, BUF_H, WINDOW_FORMAT_RGBX_8888);
        layout(buf_w);
    }
    unlock();
}

/* ---- input (in a thread of its own, with a looper) ---- */

static u16 key_bit(s32 code)
{
    switch (code) {
    case AKEYCODE_DPAD_UP: return 0x1000;
    case AKEYCODE_DPAD_DOWN: return 0x4000;
    case AKEYCODE_DPAD_LEFT: return 0x8000;
    case AKEYCODE_DPAD_RIGHT: return 0x2000;
    case AKEYCODE_BUTTON_A: return 0x0040;
    case AKEYCODE_BUTTON_B: return 0x0020;
    case AKEYCODE_BUTTON_X: return 0x0080;
    case AKEYCODE_BUTTON_Y: return 0x0010;
    case AKEYCODE_BUTTON_L1: return 0x0004;
    case AKEYCODE_BUTTON_R1: return 0x0008;
    case AKEYCODE_BUTTON_L2: return 0x0001;
    case AKEYCODE_BUTTON_R2: return 0x0002;
    case AKEYCODE_BUTTON_START: case AKEYCODE_ENTER: return 0x0800;
    case AKEYCODE_BUTTON_SELECT: return 0x0100;
    default: return 0;
    }
}

static s32 on_input(int fd, int events, void *data)
{
    AInputQueue *q = data;
    AInputEvent *e;

    while (AInputQueue_getEvent(q, &e) >= 0) {
        s32 handled = 0;

        if (AInputQueue_preDispatchEvent(q, e)) {
            continue;
        }
        if (AInputEvent_getType(e) == AINPUT_EVENT_TYPE_KEY) {
            u16 bit = key_bit(AKeyEvent_getKeyCode(e));

            if (bit) {
                if (AKeyEvent_getAction(e) == AKEY_EVENT_ACTION_DOWN) {
                    key_held |= bit;
                } else if (AKeyEvent_getAction(e) == AKEY_EVENT_ACTION_UP) {
                    key_held &= ~bit;
                }
                update_touch();
                handled = 1;
            }
        } else if (AInputEvent_getType(e) == AINPUT_EVENT_TYPE_MOTION) {
            s32 a = AMotionEvent_getAction(e), kind = a & AMOTION_EVENT_ACTION_MASK;
            s32 idx = (a & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
            size_t i, n = AMotionEvent_getPointerCount(e);

            if (AInputEvent_getSource(e) & AINPUT_SOURCE_CLASS_JOYSTICK) {
                /* a controller: the left stick and the hat as the d-pad */
                float x = AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_X, 0);
                float y = AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_Y, 0);
                float hx = AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_HAT_X, 0);
                float hy = AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_HAT_Y, 0);
                u16 k = key_held & ~0xF000;

                if (x < -0.5f || hx < -0.5f) k |= 0x8000;
                if (x > 0.5f || hx > 0.5f) k |= 0x2000;
                if (y < -0.5f || hy < -0.5f) k |= 0x1000;
                if (y > 0.5f || hy > 0.5f) k |= 0x4000;
                key_held = k;
                update_touch();
                handled = 1;
            } else {
                s32 ww = window ? ANativeWindow_getWidth(window) : 1, wh = window ? ANativeWindow_getHeight(window) : 1;

                for (i = 0; i < n && i < 10; i++) {
                    s32 id = AMotionEvent_getPointerId(e, i);

                    if (id < 0 || id >= 10) {
                        continue;
                    }
                    touch_x[id] = AMotionEvent_getX(e, i) * buf_w / (ww > 0 ? ww : 1);
                    touch_y[id] = AMotionEvent_getY(e, i) * BUF_H / (wh > 0 ? wh : 1);
                    if (kind == AMOTION_EVENT_ACTION_DOWN || kind == AMOTION_EVENT_ACTION_MOVE ||
                        (kind == AMOTION_EVENT_ACTION_POINTER_DOWN && (s32)i == idx)) {
                        touch_down[id] = 1;
                    }
                    if (kind == AMOTION_EVENT_ACTION_UP || kind == AMOTION_EVENT_ACTION_CANCEL ||
                        (kind == AMOTION_EVENT_ACTION_POINTER_UP && (s32)i == idx)) {
                        touch_down[id] = 0;
                    }
                }
                update_touch();
                handled = 1;
            }
        }
        AInputQueue_finishEvent(q, e, handled);
    }
    return 1;
}

static void *input_thread(void *arg)
{
    input_looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    for (;;) {
        ALooper_pollOnce(-1, NULL, NULL, NULL);
    }
    return NULL;
}

/* ---- the permission to read files (Activity.requestPermissions, via JNI) ---- */

typedef void *(*JniFn)();
#define JNI(env, i) ((JniFn)((void *const *)*(env))[i])
#define J_FindClass 6
#define J_ExceptionClear 17
#define J_GetObjectClass 31
#define J_GetMethodID 33
#define J_CallVoidMethod 61
#define J_NewStringUTF 167
#define J_NewObjectArray 172
#define J_SetObjectArrayElement 174

static void ask_permission(ANativeActivity *a)
{
    JNIEnv *env = a->env;
    void *cls = JNI(env, J_GetObjectClass)(env, a->clazz);
    void *m = JNI(env, J_GetMethodID)(env, cls, "requestPermissions", "([Ljava/lang/String;I)V");
    void *scls, *arr;

    if (!m) {
        JNI(env, J_ExceptionClear)(env);
        return;
    }
    scls = JNI(env, J_FindClass)(env, "java/lang/String");
    arr = JNI(env, J_NewObjectArray)(env, 2, scls, NULL);
    JNI(env, J_SetObjectArrayElement)(env, arr, 0, JNI(env, J_NewStringUTF)(env, "android.permission.READ_EXTERNAL_STORAGE"));
    JNI(env, J_SetObjectArrayElement)(env, arr, 1, JNI(env, J_NewStringUTF)(env, "android.permission.WRITE_EXTERNAL_STORAGE"));
    JNI(env, J_CallVoidMethod)(env, a->clazz, m, arr, 1);
    JNI(env, J_ExceptionClear)(env);
}

/* ---- diagnostics: a crash is shown and logged; a heartbeat in the log ---- */

void ANativeActivity_onCreate(ANativeActivity *a, void *saved, size_t saved_size);

u32 pc_vblanks(void);
u32 pc_frames(void);
static volatile s32 game_started;

/* bionic on 32-bit ARM: siginfo (si_addr at 12) and ucontext (the
 * sigcontext at 20: r0 at 12, ..., lr at 68, pc at 72) */
static void on_crash(int sig, void *info, void *uc)
{
    u32 addr = *(u32 *)((u8 *)info + 12);
    u32 lr = *(u32 *)((u8 *)uc + 20 + 68);
    u32 pc = *(u32 *)((u8 *)uc + 20 + 72);
    u32 sp = *(u32 *)((u8 *)uc + 20 + 64);

    say("CRASH: SIGNAL %d ADDRESS %08X", sig, addr);
    say("PC %08X LR %08X SP %08X", pc, lr, sp);
    say("BASE %08X (ANATIVEACTIVITY_ONCREATE)", (u32)ANativeActivity_onCreate);
    say("PLEASE SEND A PHOTO OF THIS SCREEN");
    for (;;) {
        struct timespec t = { 1, 0 };

        pc_android_present(NULL, 0, 0);
        nanosleep(&t, NULL);
    }
}

static void catch_crashes(void)
{
    struct {
        void (*handler)(int, void *, void *);
        unsigned long mask;
        int flags;
        void (*restorer)(void);
    } sa;
    static const int sigs[] = { 4, 7, 8, 11 }; /* SIGILL SIGBUS SIGFPE SIGSEGV */
    u32 i;

    memset(&sa, 0, sizeof(sa));
    sa.handler = on_crash;
    sa.flags = 4; /* SA_SIGINFO */
    for (i = 0; i < 4; i++) {
        sigaction(sigs[i], (void *)&sa, NULL);
    }
}

/* where the game thread is (sampled with SIGUSR1 when it shows nothing) */
extern char __ehdr_start[]; /* the start of this library */
extern volatile u32 pc_mips_pc;
static pthread_t game_tid;
static volatile u32 sample_pc, sample_lr;

static void on_sample(int sig, void *info, void *uc)
{
    sample_lr = *(u32 *)((u8 *)uc + 20 + 68);
    sample_pc = *(u32 *)((u8 *)uc + 20 + 72);
}

static void *monitor_thread(void *arg)
{
    u32 n = 0, last_frames = 0;
    struct {
        void (*handler)(int, void *, void *);
        unsigned long mask;
        int flags;
        void (*restorer)(void);
    } sa;

    memset(&sa, 0, sizeof(sa));
    sa.handler = on_sample;
    sa.flags = 4 | 0x10000000; /* SA_SIGINFO, SA_RESTART */
    sigaction(10, (void *)&sa, NULL); /* SIGUSR1 */
    for (;;) {
        struct timespec t = { 3, 0 };
        u32 f;

        nanosleep(&t, NULL);
        f = pc_frames();
        if (game_started && logf) {
            fprintf(logf, "port: alive %u s, %u vblanks, %u frames\n", (n += 3), pc_vblanks(), f);
            if (f == last_frames) {
                u32 base = (u32)__ehdr_start;

                sample_pc = sample_lr = 0;
                pthread_kill(game_tid, 10);
                nanosleep(&(struct timespec){ 0, 50000000 }, NULL);
                fprintf(logf, "port: no new frames; the game is at +%X (called from +%X), interpreter %08X\n",
                        sample_pc - base, sample_lr - base, pc_mips_pc);
            }
            fflush(logf);
        }
        last_frames = f;
        if (pc_frames() == 0) {
            pc_android_present(NULL, 0, 0); /* the messages until the game shows something */
        }
    }
    return NULL;
}

/* ---- the game thread ---- */

static s32 find_image(const char *dir, char *out, size_t n)
{
    static const char *kinds[] = { ".cue", ".iso", ".bin" };
    s32 k;

    for (k = 0; k < 3; k++) {
        DIR *d = opendir(dir);
        struct dirent *e;

        if (!d) {
            return -errno;
        }
        while ((e = readdir(d))) {
            size_t len = strlen(e->d_name);

            if (len > 4 && !strcasecmp(e->d_name + len - 4, kinds[k])) {
                snprintf(out, n, "%s/%s", dir, e->d_name);
                closedir(d);
                return 1;
            }
        }
        closedir(d);
    }
    return 0;
}

static void *game_thread(void *arg)
{
    const char *dirs[3];
    char image[512];
    s32 i, denied = 0;

    dirs[0] = "/sdcard/Crash3";
    dirs[1] = "/sdcard/Download";
    dirs[2] = act->externalDataPath;
    for (;;) {
        for (i = 0; i < 3; i++) {
            s32 r = dirs[i] ? find_image(dirs[i], image, sizeof(image)) : 0;

            if (r == 1) {
                break;
            }
            denied |= r == -EACCES || r == -EPERM;
        }
        if (i < 3) {
            break;
        }
        status_lines = 0;
        say("CRASH BANDICOOT 3 - PC PORT FOR ANDROID");
        say("PUT YOUR DISC IMAGE (.CUE+.BIN OR .ISO)");
        say("IN /SDCARD/CRASH3 OR /SDCARD/DOWNLOAD");
        if (denied) {
            say("ALLOW FILES: SETTINGS > APPS > PERMISSIONS");
        }
        pc_android_present(NULL, 0, 0);

        {
            struct timespec t = { 1, 0 };

            nanosleep(&t, NULL);
        }
    }
    if (!logf) {
        char path[600];
        const char *slash = strrchr(image, '/');

        snprintf(path, sizeof(path), "%.*s/crash3_log.txt", (int)(slash - image), image);
        logf = fopen(path, "w");
    }
    status_lines = 0;
    say("DISC IMAGE: %s", image);
    say("STARTING...");
    pc_android_present(NULL, 0, 0);
    /* the messages of the port and the game go to the log */
    if (logf) {
        setvbuf(logf, NULL, _IOLBF, 0);
        stdout = logf;
        stderr = logf;
    }
    catch_crashes();
    game_tid = pthread_self();
    {
        pthread_t m;

        pthread_create(&m, NULL, monitor_thread, NULL);
    }
    game_started = 1;
    i = pc_start(image, NULL);
    say("THE GAME STOPPED (%d)", i);
    for (;;) {
        struct timespec t = { 1, 0 };

        pc_android_present(NULL, 0, 0);
        nanosleep(&t, NULL);
    }
    return NULL;
}

/* an error of the start (the message is in the log): shown, the app stays */
void pc_android_fatal(void)
{
    say("ERROR - SEE CRASH3_LOG.TXT NEXT TO THE IMAGE");
    for (;;) {
        struct timespec t = { 1, 0 };

        pc_android_present(NULL, 0, 0);
        nanosleep(&t, NULL);
    }
}

/* where the port writes messages (printf of the game, stderr of the port) */
FILE *pc_android_log(void)
{
    return logf;
}

void pc_android_message(const char *s)
{
    say("%s", s);
}

/* ---- the activity ---- */

static void on_window_created(ANativeActivity *a, ANativeWindow *w)
{
    set_window(w);
}

static void on_window_destroyed(ANativeActivity *a, ANativeWindow *w)
{
    set_window(NULL);
}

static void on_queue_created(ANativeActivity *a, AInputQueue *q)
{
    while (!input_looper) {
        struct timespec t = { 0, 1000000 };

        nanosleep(&t, NULL);
    }
    AInputQueue_attachLooper(q, input_looper, 1, on_input, q);
}

static void on_queue_destroyed(ANativeActivity *a, AInputQueue *q)
{
    AInputQueue_detachLooper(q);
}

static void on_destroy(ANativeActivity *a)
{
    exit(0); /* the game keeps its state in memory: start it anew next time */
}

void ANativeActivity_onCreate(ANativeActivity *a, void *saved, size_t saved_size)
{
    static s32 started;
    pthread_t t;
    pthread_attr_t attr;

    act = a;
    a->callbacks->onNativeWindowCreated = on_window_created;
    a->callbacks->onNativeWindowDestroyed = on_window_destroyed;
    a->callbacks->onInputQueueCreated = on_queue_created;
    a->callbacks->onInputQueueDestroyed = on_queue_destroyed;
    a->callbacks->onDestroy = on_destroy;
    ANativeActivity_setWindowFlags(a, AWINDOW_FLAG_KEEP_SCREEN_ON | AWINDOW_FLAG_FULLSCREEN, 0);
    layout(buf_w);
    if (started) {
        return;
    }
    started = 1;
    if (a->sdkVersion >= 23) {
        ask_permission(a);
    }
    pthread_create(&t, NULL, input_thread, NULL);
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8 << 20);
    pthread_create(&t, &attr, game_thread, NULL);
}

/* Division by zero on 32-bit ARM goes to these (libgcc raises SIGFPE in
 * them); the R3000 does not trap: the game goes on. */
int __aeabi_idiv0(int r)
{
    return 0;
}

long long __aeabi_ldiv0(long long r)
{
    return 0;
}
