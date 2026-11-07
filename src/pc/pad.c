/* The pads of the PC port (libpad): the receive buffers the game gave to
 * PadInitDirect are filled at every vertical blank, as the pad driver of the
 * console does, from the keyboard or a game controller (SDL2).
 *
 * A buffer: status (0: a pad answered), type (0x41: digital pad), then the
 * buttons, active low: Select L3 R3 Start Up Right Down Left, then L2 R2 L1
 * R1 Triangle Circle Cross Square.
 *
 * Keys: arrows; Z Cross (jump), X Square (spin), C Circle (slide), S
 * Triangle, Enter Start, Backspace Select, Q L1, W R1, A L2, E R2. */
#include "pc_shadow.h"
#include "types.h"

#include <stdio.h>

#ifdef PC_SDL
#define SDL_MAIN_HANDLED
#include <SDL.h>
#endif

/* the buttons as the game keeps them (pad->held): bit 8 Select ... 15 Left,
 * bit 0 L2 ... 7 Square */
#define B_SELECT 0x0100
#define B_START 0x0800
#define B_UP 0x1000
#define B_RIGHT 0x2000
#define B_DOWN 0x4000
#define B_LEFT 0x8000
#define B_L2 0x0001
#define B_R2 0x0002
#define B_L1 0x0004
#define B_R1 0x0008
#define B_TRIANGLE 0x0010
#define B_CIRCLE 0x0020
#define B_CROSS 0x0040
#define B_SQUARE 0x0080

static u8 *bufs[2];
static volatile u16 held; /* from the main thread */
static s32 started;

void PadInitDirect(u8 *buf1, u8 *buf2)
{
    SHADOW_UNSAFE();
    bufs[0] = buf1;
    bufs[1] = buf2;
    if (buf1) {
        buf1[0] = 0xFF;
    }
    if (buf2) {
        buf2[0] = 0xFF;
    }
}

void PadStartCom(void)
{
    SHADOW_UNSAFE();
    started = 1;
}

/* PadStopCom */
void func_80048E88(void)
{
    SHADOW_UNSAFE();
    started = 0;
}

/* PadInfoMode: the kind of the pad (4: a digital pad) */
s32 func_80048F00(s32 port)
{
    return port == 0 ? 4 : 0;
}

/* PadInfoAct */
s32 func_80048FC0(s32 port, s32 term, s32 offs)
{
    return 0;
}

/* PadSetActAlign, PadSetMainMode, PadSetAct: no actuators */
void func_80049234(s32 port, u8 *data)
{
}

void func_8004926C(s32 port, s32 a, s32 b)
{
}

void func_800492B4(s32 port, u8 *data, s32 len)
{
}

/* the buttons from elsewhere (the touch screen and controllers on Android) */
void pc_pad_set(u16 buttons)
{
    if (buttons != held) {
        fprintf(stderr, "pad: %04X (bits: 0x10 triangle, 0x20 circle, 0x40 cross, 0x80 square, 0x800 start)\n",
                buttons);
    }
    held = buttons;
}

/* at a vertical blank (from the interrupt) */
void pc_pad_vblank(void)
{
    u16 b = held;

    if (!started) {
        return;
    }
    if (bufs[0]) {
        bufs[0][0] = 0;
        bufs[0][1] = 0x41;
        bufs[0][2] = (u8)~(b >> 8);
        bufs[0][3] = (u8)~b;
    }
    if (bufs[1]) {
        bufs[1][0] = 0xFF; /* no second pad */
    }
}

/* reads the keyboard and the controllers (the main thread, with the events
 * of the window) */
void pc_pad_poll(void)
{
#ifdef PC_SDL
    static SDL_GameController *gc;
    const u8 *k = SDL_GetKeyboardState(NULL);
    u16 b = 0;

    if (!k) {
        return;
    }
    if (k[SDL_SCANCODE_UP]) b |= B_UP;
    if (k[SDL_SCANCODE_DOWN]) b |= B_DOWN;
    if (k[SDL_SCANCODE_LEFT]) b |= B_LEFT;
    if (k[SDL_SCANCODE_RIGHT]) b |= B_RIGHT;
    if (k[SDL_SCANCODE_Z]) b |= B_CROSS;
    if (k[SDL_SCANCODE_X]) b |= B_SQUARE;
    if (k[SDL_SCANCODE_C]) b |= B_CIRCLE;
    if (k[SDL_SCANCODE_S]) b |= B_TRIANGLE;
    if (k[SDL_SCANCODE_RETURN]) b |= B_START;
    if (k[SDL_SCANCODE_BACKSPACE]) b |= B_SELECT;
    if (k[SDL_SCANCODE_Q]) b |= B_L1;
    if (k[SDL_SCANCODE_W]) b |= B_R1;
    if (k[SDL_SCANCODE_A]) b |= B_L2;
    if (k[SDL_SCANCODE_E]) b |= B_R2;
    if (!gc) {
        s32 i;

        if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER)) {
            SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
        }
        for (i = 0; i < SDL_NumJoysticks() && !gc; i++) {
            if (SDL_IsGameController(i)) {
                gc = SDL_GameControllerOpen(i);
            }
        }
    }
    if (gc) {
        static const struct {
            SDL_GameControllerButton sdl;
            u16 bit;
        } map[] = {
            { SDL_CONTROLLER_BUTTON_DPAD_UP, B_UP },     { SDL_CONTROLLER_BUTTON_DPAD_DOWN, B_DOWN },
            { SDL_CONTROLLER_BUTTON_DPAD_LEFT, B_LEFT }, { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, B_RIGHT },
            { SDL_CONTROLLER_BUTTON_A, B_CROSS },        { SDL_CONTROLLER_BUTTON_X, B_SQUARE },
            { SDL_CONTROLLER_BUTTON_B, B_CIRCLE },       { SDL_CONTROLLER_BUTTON_Y, B_TRIANGLE },
            { SDL_CONTROLLER_BUTTON_START, B_START },    { SDL_CONTROLLER_BUTTON_BACK, B_SELECT },
            { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, B_L1 }, { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, B_R1 },
        };
        u32 i;
        s32 lx = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX);
        s32 ly = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY);

        if (!SDL_GameControllerGetAttached(gc)) {
            SDL_GameControllerClose(gc);
            gc = NULL;
        } else {
            for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
                if (SDL_GameControllerGetButton(gc, map[i].sdl)) {
                    b |= map[i].bit;
                }
            }
            /* the left stick as the d-pad */
            if (lx < -16000) b |= B_LEFT;
            if (lx > 16000) b |= B_RIGHT;
            if (ly < -16000) b |= B_UP;
            if (ly > 16000) b |= B_DOWN;
            if (SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000) b |= B_L2;
            if (SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) b |= B_R2;
        }
    }
    held = b;
#endif
}
