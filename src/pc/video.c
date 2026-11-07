/* The screen of the PC port: the display area of the VRAM in an SDL2 window
 * (PC_SDL), scaled with the aspect of a TV. Without SDL, or with
 * PC_DUMP_FRAMES=dir, frames are written as dir/frame_NNNNN.ppm (every
 * PC_DUMP_EVERY-th, default 30): for tests without a screen. */
#include "pc_gpu.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef PC_SDL
#define SDL_MAIN_HANDLED
#include <SDL.h>

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static s32 tex_w, tex_h;
#endif

static u32 frame_rgb[640 * 512];
static u32 frames;

u32 pc_frames(void)
{
    return frames;
}

static void dump(void)
{
    const char *dir = getenv("PC_DUMP_FRAMES");
    const char *every = getenv("PC_DUMP_EVERY");
    char path[512];
    FILE *f;
    s32 i;

    if (!dir || frames % (every ? atoi(every) : 30)) {
        return;
    }
    snprintf(path, sizeof(path), "%s/frame_%05u.ppm", dir, frames);
    f = fopen(path, "wb");
    if (!f) {
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", gpu_display.w, gpu_display.h);
    for (i = 0; i < gpu_display.w * gpu_display.h; i++) {
        fputc(frame_rgb[i] >> 16, f);
        fputc(frame_rgb[i] >> 8 & 255, f);
        fputc(frame_rgb[i] & 255, f);
    }
    fclose(f);
}

/* Recordings of whole frames of the GPU work (PC_GPU_TRACE=dir, or on
 * Android next to the disc image): frames 600, 1200 and 1800, each from
 * 4 presents on (a whole frame of the game), as crash3_gpu_N.bin. */
#ifdef __ANDROID__
const char *pc_android_dir(void);
#endif

static void trace_frames(void)
{
    static s32 active;
    const char *dir = getenv("PC_GPU_TRACE");
    const char *at_s = getenv("PC_GPU_TRACE_AT");
    u32 at = at_s ? (u32)atoi(at_s) : 600;
    char path[600];

#ifdef __ANDROID__
    dir = pc_android_dir();
#endif
    const char *len_s = getenv("PC_GPU_TRACE_LEN");

    if (active && --active == 0) {
        gpu_trace_end(); /* 4 presents: a whole frame of the game (30 per second) */
    }
    if (dir && !active && frames % at == 0 && frames <= 3 * at) {
        snprintf(path, sizeof(path), "%s/crash3_gpu_%u.bin", dir, frames / at);
        gpu_trace_begin(path);
        active = len_s ? atoi(len_s) : 4;
    }
}

void pc_video_present(void)
{
    if (gpu_display.w <= 0 || gpu_display.h <= 0 || gpu_display.w > 640 || gpu_display.h > 512) {
        return;
    }
    frames++;
    trace_frames();
    gpu_display_rgb(frame_rgb);
    dump();
#ifdef __ANDROID__
    {
        void pc_android_present(const u32 *rgb, s32 w, s32 h);

        pc_android_present(gpu_display.enabled ? frame_rgb : NULL, gpu_display.w, gpu_display.h);
    }
#endif
#ifdef PC_SDL
    if (!window) {
        if (getenv("PC_NO_WINDOW") || SDL_Init(SDL_INIT_VIDEO) != 0) {
            return;
        }
        window = SDL_CreateWindow("Crash Bandicoot 3", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 720,
                                  SDL_WINDOW_RESIZABLE);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!window || !renderer) {
            return;
        }
        SDL_RenderSetLogicalSize(renderer, 640, 480);
    }
    if (!texture || tex_w != gpu_display.w || tex_h != gpu_display.h) {
        if (texture) {
            SDL_DestroyTexture(texture);
        }
        tex_w = gpu_display.w;
        tex_h = gpu_display.h;
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, tex_w, tex_h);
    }
    SDL_UpdateTexture(texture, NULL, frame_rgb, tex_w * 4);
    SDL_RenderClear(renderer);
    if (gpu_display.enabled) {
        SDL_RenderCopy(renderer, texture, NULL, NULL);
    }
    SDL_RenderPresent(renderer);
#endif
}

void pc_pad_poll(void);

/* the events of the window (closing it ends the game), the pads */
void pc_video_poll(void)
{
#ifdef PC_SDL
    SDL_Event e;

    if (!window) {
        return;
    }
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            exit(0);
        }
    }
    pc_pad_poll();
#endif
}
