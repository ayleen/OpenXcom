/*
 * Stub implementations for SDL/SDL_mixer/SDL_gfx functions not provided
 * by Emscripten's libsdl.js emulation layer (-sUSE_SDL=1).
 * All stubs are intentionally no-ops or minimal; audio/video playback
 * and threading are not supported in the Phase-2 WASM build.
 */
#ifdef __EMSCRIPTEN__

#include <SDL/SDL.h>
#include <SDL/SDL_mouse.h>
#include <SDL/SDL_mixer.h>
#include <SDL/SDL_gfxPrimitives.h>

extern "C" {

/* ---- Events ---- */

Uint8 SDL_EventState(Uint32 type, int state)
{
    return SDL_ENABLE;
}

/* ---- Cursor ---- */

SDL_Cursor *SDL_CreateCursor(const Uint8 *data, const Uint8 *mask,
                             int w, int h, int hot_x, int hot_y)
{
    return NULL;
}

void SDL_SetCursor(SDL_Cursor *cursor)
{
    /* no cursor support in Emscripten build */
}

SDL_Cursor *SDL_GetCursor(void)
{
    return NULL;
}

void SDL_FreeCursor(SDL_Cursor *cursor)
{
    /* no-op */
}

/* ---- Environment ---- */

int SDL_putenv(const char *variable)
{
    return 0;
}

/* ---- Threading / semaphores ---- */

SDL_sem *SDL_CreateSemaphore(Uint32 initial_value)
{
    return NULL;
}

void SDL_DestroySemaphore(SDL_sem *sem)
{
    /* no threading in Emscripten build */
}

int SDL_SemWait(SDL_sem *sem)
{
    return 0;
}

int SDL_SemPost(SDL_sem *sem)
{
    return 0;
}

/* ---- RW operations ---- */

Uint16 SDL_ReadLE16(SDL_RWops *src)
{
    Uint16 value = 0;
    SDL_RWread(src, &value, sizeof(value), 1);
    return SDL_SwapLE16(value);
}

Uint32 SDL_ReadLE32(SDL_RWops *src)
{
    Uint32 value = 0;
    SDL_RWread(src, &value, sizeof(value), 1);
    return SDL_SwapLE32(value);
}

size_t SDL_WriteLE32(SDL_RWops *dst, Uint32 value)
{
    Uint32 swapped = SDL_SwapLE32(value);
    return SDL_RWwrite(dst, &swapped, sizeof(swapped), 1);
}

/* ---- Error ---- */

void SDL_Error(SDL_errorcode code)
{
    /* swallow SDL internal errors */
}

/* ---- Audio / SDL_mixer ---- */

void Mix_HookMusic(void (*mix_func)(void *udata, Uint8 *stream, int len),
                   void *arg)
{
    /* audio hooks not supported in Emscripten build */
}

int Mix_GroupChannels(int from, int to, int tag)
{
    return 0;
}

int Mix_GroupAvailable(int tag)
{
    return -1; /* no channel available */
}

Mix_MusicType Mix_GetMusicType(const Mix_Music *music)
{
    return MUS_NONE;
}

/* ---- SDL_gfx font/primitives not in libsdl.js ---- */

int characterRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, char c,
                  Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    return 0;
}

int stringRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, const char *s,
               Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    return 0;
}

int filledCircleColor(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 rad,
                      Uint32 color)
{
    return 0;
}

int texturedPolygon(SDL_Surface *dst,
                    const Sint16 *vx, const Sint16 *vy, int n,
                    SDL_Surface *texture, int texture_dx, int texture_dy)
{
    return 0;
}

} /* extern "C" */

#endif /* __EMSCRIPTEN__ */
