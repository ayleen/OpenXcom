#pragma once
#include <SDL_version.h>
#include <SDL.h>       /* needed for SDL_Keycode, SDL_Surface, SDL_Color, SDLCALL */
#include <SDL_rwops.h>

/*
 * SDL version shims in both directions:
 * - Under SDL1: provide SDL2 API that SDL1 lacks (implemented in FileMap.cpp)
 * - Under SDL2: provide SDL1 API aliases that SDL2 dropped
 */

/* ---- SDL2 functions missing from SDL1 (shim implementations in FileMap.cpp) ---- */
# if !SDL_VERSION_ATLEAST(2,0,0)
extern "C"
{
Uint8 SDL_ReadU8(SDL_RWops *src);
Sint64 SDL_RWsize(SDL_RWops *src);
}
# endif
#if !SDL_VERSION_ATLEAST(2,0,6)
extern "C"
{
void *SDL_LoadFile_RW(SDL_RWops *src, size_t *datasize, int freesrc);
}
#endif

/* ---- SDL1 API removed in SDL2 — compat macros and types ---- */
#if SDL_VERSION_ATLEAST(2,0,0)
/* SDL_SRCCOLORKEY (SDL1 flag) → SDL_TRUE (SDL2 bool enable).
 * Both signal "enable colour-key transparency" in SDL_SetColorKey. */
#ifndef SDL_SRCCOLORKEY
#define SDL_SRCCOLORKEY SDL_TRUE
#endif

/* SDL_AllocSurface was renamed SDL_CreateRGBSurface in SDL2; signatures identical. */
#ifndef SDL_AllocSurface
#define SDL_AllocSurface SDL_CreateRGBSurface
#endif

/* SDL_SWSURFACE / SDL_HWSURFACE flags dropped in SDL2; 0 is the correct substitute. */
#ifndef SDL_SWSURFACE
#define SDL_SWSURFACE 0
#endif
#ifndef SDL_HWSURFACE
#define SDL_HWSURFACE 0
#endif

/* SDL_NOEVENT: SDL1 event type 0; renamed SDL_FIRSTEVENT in SDL2. */
#ifndef SDL_NOEVENT
#define SDL_NOEVENT SDL_FIRSTEVENT
#endif

/* SDL_GrabMode: SDL1 enum for mouse-grab state; removed in SDL2. */
#ifndef SDL_GrabMode
typedef int SDL_GrabMode;
#endif

/* SDLKey: SDL1 typedef for key codes; renamed SDL_Keycode in SDL2. */
#ifndef SDLKey
typedef SDL_Keycode SDLKey;
#endif

/* KMOD_LMETA / KMOD_RMETA: renamed KMOD_LGUI / KMOD_RGUI in SDL2. */
#ifndef KMOD_LMETA
#define KMOD_LMETA KMOD_LGUI
#endif
#ifndef KMOD_RMETA
#define KMOD_RMETA KMOD_RGUI
#endif

/* SDLK_SCROLLOCK: renamed SDLK_SCROLLLOCK in SDL2. */
#ifndef SDLK_SCROLLOCK
#define SDLK_SCROLLOCK SDLK_SCROLLLOCK
#endif

/* SDL_GrabMode enum values: SDL_GRAB_OFF/ON removed in SDL2 (SDL_GrabMode is now
 * just int and SDL_WM_GrabInput is a no-op stub).  Provide the SDL1 constants. */
#ifndef SDL_GRAB_OFF
#define SDL_GRAB_OFF 0
#endif
#ifndef SDL_GRAB_ON
#define SDL_GRAB_ON  1
#endif

/* SDL_KillThread: removed in SDL2 (no safe way to kill a thread).
 * SDL_DetachThread lets the thread run to completion without leaking handles. */
#ifndef SDL_KillThread
static inline void SDL_KillThread(SDL_Thread *t) { SDL_DetachThread(t); }
#endif

/* SDL_SetPalette / SDL_LOGPAL / SDL_PHYSPAL: removed in SDL2.
 * Shim wraps SDL_SetPaletteColors; flags argument is ignored (SDL2 always
 * updates both logical and physical palette simultaneously). */
#ifndef SDL_LOGPAL
#define SDL_LOGPAL 1
#endif
#ifndef SDL_PHYSPAL
#define SDL_PHYSPAL 2
#endif
static inline int SDL_SetPalette(SDL_Surface *surface, int flags,
                                 SDL_Color *colors, int firstcolor, int ncolors)
{
    (void)flags;
    if (!surface || !surface->format || !surface->format->palette) return 0;
    return SDL_SetPaletteColors(surface->format->palette,
                                colors, firstcolor, ncolors) == 0 ? 1 : 0;
}

#endif /* SDL_VERSION_ATLEAST(2,0,0) */

/* SDL_SetColors: removed in SDL2.
 * On Emscripten: declared here, implemented in EmscriptenCompat.cpp.
 * On native SDL2: inline shim wrapping SDL_SetPaletteColors. */
#if SDL_VERSION_ATLEAST(2,0,0) && !defined(__EMSCRIPTEN__)
static inline int SDL_SetColors(SDL_Surface *surface, const SDL_Color *colors, int firstcolor, int ncolors)
{
    if (!surface || !surface->format || !surface->format->palette) return 0;
    return SDL_SetPaletteColors(surface->format->palette, colors, firstcolor, ncolors) == 0 ? 1 : 0;
}
#endif

/* ---- Forward declarations for SDL1 functions shim'd by EmscriptenCompat.cpp ----
 *
 * SDL2 headers don't declare these; the shim definitions live in EmscriptenCompat.cpp
 * (compiled only under __EMSCRIPTEN__).  These declarations let callers compile
 * without error under -sUSE_SDL=2.
 */
#if SDL_VERSION_ATLEAST(2,0,0) && defined(__EMSCRIPTEN__)

#ifdef __cplusplus
extern "C" {
#endif

/* SDL_SetColors: removed in SDL2; shim wraps SDL_SetPaletteColors (EmscriptenCompat.cpp). */
int SDLCALL SDL_SetColors(SDL_Surface *surface, const SDL_Color *colors, int firstcolor, int ncolors);

/* SDL_WM_*: window-management API removed in SDL2; all no-ops in browser. */
SDL_GrabMode SDL_WM_GrabInput(SDL_GrabMode mode);
void         SDL_WM_SetCaption(const char *title, const char *icon);
void         SDL_WM_GetCaption(const char **title, const char **icon);
void         SDL_WM_SetIcon(SDL_Surface *icon, Uint8 *mask);

/* SDL_WarpMouse: renamed SDL_WarpMouseInWindow in SDL2; no-op in browser. */
void SDL_WarpMouse(Uint16 x, Uint16 y);

/* SDL1 Unicode / key-repeat API: removed in SDL2 (SDL2 handles these natively). */
int SDL_EnableUNICODE(int enable);
int SDL_EnableKeyRepeat(int delay, int interval);

/* SDL1 mouse-wheel button constants (button 4/5 = wheel up/down in SDL1 events).
 * In SDL2 wheel input arrives as SDL_MOUSEWHEEL; Game.cpp translates to fake
 * SDL_MOUSEBUTTONDOWN with these button codes so all existing wheel-check code works. */
#ifndef SDL_BUTTON_WHEELUP
#  define SDL_BUTTON_WHEELUP   4
#endif
#ifndef SDL_BUTTON_WHEELDOWN
#  define SDL_BUTTON_WHEELDOWN 5
#endif

#ifdef __cplusplus
}
#endif

#ifndef SDL_DEFAULT_REPEAT_DELAY
#define SDL_DEFAULT_REPEAT_DELAY    500
#endif
#ifndef SDL_DEFAULT_REPEAT_INTERVAL
#define SDL_DEFAULT_REPEAT_INTERVAL  30
#endif

#endif /* SDL_VERSION_ATLEAST(2,0,0) && __EMSCRIPTEN__ */
