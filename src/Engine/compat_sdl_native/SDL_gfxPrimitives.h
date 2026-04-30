/*
 * Stub header replacing SDL_gfxPrimitives.h for native (non-Emscripten) SDL2 builds.
 * Declarations for SDL_gfx functions implemented in SDL2CompatNative.cpp.
 * Uses the SDL_Surface* API (SDL1-style) — SDL2_gfx is incompatible (SDL_Renderer*).
 */
#pragma once
#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

int lineColor(SDL_Surface *dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint32 color);
int lineRGBA(SDL_Surface *dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
             Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int filledCircleColor(SDL_Surface *dst, Sint16 cx, Sint16 cy, Sint16 rad, Uint32 color);
int texturedPolygon(SDL_Surface *dst,
                    const Sint16 *vx, const Sint16 *vy, int n,
                    SDL_Surface *texture, int texture_dx, int texture_dy);
int characterRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, char c,
                  Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int stringRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, const char *s,
               Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int stringColor(SDL_Surface *dst, Sint16 x, Sint16 y, const char *s, Uint32 color);
int filledPolygonColor(SDL_Surface *dst, const Sint16 *vx, const Sint16 *vy, int n,
                       Uint32 color);

#ifdef __cplusplus
}
#endif
