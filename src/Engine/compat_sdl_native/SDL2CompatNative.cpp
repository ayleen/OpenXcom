/*
 * Native SDL2 compatibility implementations for SDL1 APIs still referenced
 * in the OXCE codebase.
 *
 * Emscripten builds get the equivalent stubs from EmscriptenCompat.cpp.
 * This file is compiled only for non-Emscripten builds (src/CMakeLists.txt).
 *
 * SDL_gfxPrimitives: copied from EmscriptenCompat.cpp (SDL_Surface* based,
 * ARGB8888). SDL2_gfx uses SDL_Renderer* which is incompatible.
 */
#include <SDL.h>
#include "../SDL2Helpers.h"
#include <cmath>
#include <cstdio>

/* ---- SDL_WarpMouse ---- */
void SDL_WarpMouse(Uint16 x, Uint16 y)
{
    SDL_WarpMouseInWindow(SDL_GL_GetCurrentWindow(),
                         static_cast<int>(x), static_cast<int>(y));
}

/* ---- SDL_WM_* window-management API ---- */
SDL_GrabMode SDL_WM_GrabInput(SDL_GrabMode mode)
{
    SDL_Window *win = SDL_GL_GetCurrentWindow();
    if (win)
        SDL_SetWindowGrab(win, mode ? SDL_TRUE : SDL_FALSE);
    return mode;
}

void SDL_WM_SetCaption(const char *title, const char *icon)
{
    (void)icon;
    SDL_Window *win = SDL_GL_GetCurrentWindow();
    if (win && title)
        SDL_SetWindowTitle(win, title);
}

void SDL_WM_GetCaption(const char **title, const char **icon)
{
    SDL_Window *win = SDL_GL_GetCurrentWindow();
    if (title) *title = win ? SDL_GetWindowTitle(win) : nullptr;
    if (icon)  *icon  = nullptr;
}

void SDL_WM_SetIcon(SDL_Surface *icon, Uint8 *mask)
{
    (void)mask;
    SDL_Window *win = SDL_GL_GetCurrentWindow();
    if (win && icon)
        SDL_SetWindowIcon(win, icon);
}

/* ---- SDL1 Unicode / key-repeat API ---- */
int SDL_EnableUNICODE(int enable)
{
    (void)enable;
    return 1;
}

int SDL_EnableKeyRepeat(int delay, int interval)
{
    (void)delay; (void)interval;
    return 0;
}

/* ---- SDL_gfx primitives (ARGB8888 implementations) ----
 *
 * These match the implementations in EmscriptenCompat.cpp.
 * SDL_gfx colour arguments are packed as RGBA (r<<24|g<<16|b<<8|a);
 * _gfx_rgba_to_argb converts to SDL ARGB8888 (a<<24|r<<16|g<<8|b). */

extern "C" {

static Uint32 _gfx_rgba_to_argb(Uint32 rgba)
{
    return ((rgba & 0xFFu) << 24) | (rgba >> 8);
}

static void _gfx_hline_argb(SDL_Surface *dst, int x1, int x2, int y, Uint32 argb)
{
    if (y < 0 || y >= dst->h) return;
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (x1 < 0) x1 = 0;
    if (x2 >= dst->w) x2 = dst->w - 1;
    if (x1 > x2) return;
    Uint32 *row = (Uint32*)((Uint8*)dst->pixels + y * dst->pitch) + x1;
    for (int i = 0; i <= x2 - x1; ++i) row[i] = argb;
}

int characterRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, char c,
                  Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    (void)dst; (void)x; (void)y; (void)c;
    (void)r; (void)g; (void)b; (void)a;
    return 0;
}

int stringRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, const char *s,
               Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    (void)dst; (void)x; (void)y; (void)s;
    (void)r; (void)g; (void)b; (void)a;
    return 0;
}

int stringColor(SDL_Surface *dst, Sint16 x, Sint16 y, const char *s, Uint32 color)
{
    (void)dst; (void)x; (void)y; (void)s; (void)color;
    return 0;
}

int filledPolygonColor(SDL_Surface *dst, const Sint16 *vx, const Sint16 *vy, int n,
                       Uint32 color)
{
    if (!dst || !dst->pixels || n < 3) return -1;
    Uint32 argbVal = _gfx_rgba_to_argb(color);
    int y_min = vy[0], y_max = vy[0];
    for (int i = 1; i < n; i++) {
        if (vy[i] < y_min) y_min = vy[i];
        if (vy[i] > y_max) y_max = vy[i];
    }
    if (y_min < 0)       y_min = 0;
    if (y_max >= dst->h) y_max = dst->h - 1;
    for (int y = y_min; y <= y_max; y++) {
        int xs[16]; int nxs = 0;
        for (int i = 0; i < n && nxs < 15; i++) {
            int i2 = (i + 1) % n;
            int ya = vy[i], yb = vy[i2];
            if (ya == yb) continue;
            int lo = ya < yb ? ya : yb;
            int hi = ya < yb ? yb : ya;
            if (y < lo || y >= hi) continue;
            xs[nxs++] = vx[i] + (vx[i2] - vx[i]) * (y - ya) / (yb - ya);
        }
        if (nxs < 2) continue;
        for (int a = 1; a < nxs; a++) {
            int key = xs[a], b = a - 1;
            while (b >= 0 && xs[b] > key) { xs[b+1] = xs[b]; b--; }
            xs[b+1] = key;
        }
        for (int j = 0; j + 1 < nxs; j += 2)
            _gfx_hline_argb(dst, xs[j], xs[j+1], y, argbVal);
    }
    return 0;
}

int lineColor(SDL_Surface *dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
              Uint32 color)
{
    if (!dst || !dst->pixels) return -1;
    Uint32 argbVal = _gfx_rgba_to_argb(color);
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1, sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy, px = x1, py = y1;
    for (;;) {
        if (px >= 0 && px < dst->w && py >= 0 && py < dst->h)
            ((Uint32*)((Uint8*)dst->pixels + py * dst->pitch))[px] = argbVal;
        if (px == x2 && py == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; px += sx; }
        if (e2 <  dx) { err += dx; py += sy; }
    }
    return 0;
}

int lineRGBA(SDL_Surface *dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
             Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    Uint32 color = ((Uint32)r << 24) | ((Uint32)g << 16) | ((Uint32)b << 8) | a;
    return lineColor(dst, x1, y1, x2, y2, color);
}

int filledCircleColor(SDL_Surface *dst, Sint16 cx, Sint16 cy, Sint16 rad,
                      Uint32 color)
{
    if (!dst || !dst->pixels || rad < 0) return -1;
    Uint32 argbVal = _gfx_rgba_to_argb(color);
    int r = rad;
    for (int y = cy - r; y <= cy + r; y++) {
        int dy = y - cy;
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        _gfx_hline_argb(dst, cx - dx, cx + dx, y, argbVal);
    }
    return 0;
}

int texturedPolygon(SDL_Surface *dst,
                    const Sint16 *vx, const Sint16 *vy, int n,
                    SDL_Surface *texture, int texture_dx, int texture_dy)
{
    if (!dst || !dst->pixels || !texture || !texture->pixels || n < 3) return -1;
    int y_min = vy[0], y_max = vy[0];
    for (int i = 1; i < n; i++) {
        if (vy[i] < y_min) y_min = vy[i];
        if (vy[i] > y_max) y_max = vy[i];
    }
    if (y_min < 0)       y_min = 0;
    if (y_max >= dst->h) y_max = dst->h - 1;
    int tw = texture->w, th = texture->h;
    for (int y = y_min; y <= y_max; y++) {
        int xs[16]; int nxs = 0;
        for (int i = 0; i < n && nxs < 15; i++) {
            int i2 = (i + 1) % n;
            int ya = vy[i], yb = vy[i2];
            if (ya == yb) continue;
            int lo = ya < yb ? ya : yb;
            int hi = ya < yb ? yb : ya;
            if (y < lo || y >= hi) continue;
            xs[nxs++] = vx[i] + (vx[i2] - vx[i]) * (y - ya) / (yb - ya);
        }
        if (nxs < 2) continue;
        for (int a = 1; a < nxs; a++) {
            int key = xs[a], b = a - 1;
            while (b >= 0 && xs[b] > key) { xs[b+1] = xs[b]; b--; }
            xs[b+1] = key;
        }
        int ty = ((y - texture_dy) % th + th) % th;
        const Uint32 *trow = (const Uint32*)((const Uint8*)texture->pixels + ty * texture->pitch);
        Uint32       *drow = (Uint32*)((Uint8*)dst->pixels + y * dst->pitch);
        for (int j = 0; j + 1 < nxs; j += 2) {
            int lx = xs[j];    if (lx < 0)       lx = 0;
            int rx = xs[j+1];  if (rx >= dst->w)  rx = dst->w - 1;
            for (int px = lx; px <= rx; px++) {
                int tx = ((px - texture_dx) % tw + tw) % tw;
                drow[px] = trow[tx];
            }
        }
    }
    return 0;
}

} /* extern "C" */
