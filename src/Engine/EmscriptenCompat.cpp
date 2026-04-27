/*
 * Stub and compat implementations for SDL/SDL_mixer/SDL_gfx symbols not
 * provided by Emscripten's SDL2 port (-sUSE_SDL=2).
 *
 * Phase 6 (SDL2 migration) notes:
 *  - Emscripten's -sUSE_SDL=2 port reports SDL version 1.3.0 in headers and
 *    includes SDL_compat.h which provides SDL1 API aliases (SDL_GrabMode enum,
 *    SDL_WM_*, SDL_EnableUNICODE, SDL_EnableKeyRepeat, SDL_SetColors declarations).
 *    libsdl.js provides JS-side implementations for these.
 *  - We keep C-level no-op overrides for SDL_WM_*, SDL_EnableUNICODE/KeyRepeat
 *    to bypass the JS implementations (no-op is correct for browser).
 *  - SDL_SetColors: override libsdl.js canvas impl with SDL_SetPaletteColors
 *    (correct for real C SDL2 surfaces used by Screen.cpp / Surface.cpp).
 *  - Mix_LoadMUS_RW: Emscripten SDL_mixer.h uses SDL1 1-param signature;
 *    no override needed — precompiled SDL2_mixer handles C-level RWops.
 *  - SDL_gfx rasterizers kept: no SDL2_gfx in Emscripten port.
 */
#ifdef __EMSCRIPTEN__

#include <SDL.h>
#include "SDL2Helpers.h"
#include <emscripten.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <climits>

#define STUB_ONCE() do { \
	static bool _stub_warned = false; \
	if (!_stub_warned) { \
		fprintf(stderr, "[calypso stub] %s called (no-op)\n", __func__); \
		_stub_warned = true; \
	} \
} while (0)

extern "C" {

/* ---- SDL_Delay ---- */

/* SDL_Delay on the Emscripten main thread blocks the event loop and causes
 * the browser to freeze. Game.cpp guards its SDL_Delay calls with
 * #ifndef __EMSCRIPTEN__; this no-op catches any residual calls. */
void SDL_Delay(Uint32 ms)
{
	(void)ms;
}

/* ---- SDL_WarpMouse ---- */
/* SDL1 SDL_WarpMouse(x,y) was removed in SDL2 (use SDL_WarpMouseInWindow).
 * In a browser, pointer warping is impossible without Pointer Lock; no-op. */
void SDL_WarpMouse(Uint16 x, Uint16 y)
{
	(void)x; (void)y;
}

/* ---- SDL1 window-management API no-ops ---- */
/* Emscripten SDL 1.3 provides SDL_WM_* via SDL_compat.h + libsdl.js.
 * We keep no-op C overrides so our build controls the behaviour (no-op is
 * correct — browser handles window title; pointer-grab is meaningless). */

SDL_GrabMode SDL_WM_GrabInput(SDL_GrabMode mode)
{
	(void)mode;
	return (SDL_GrabMode)0;
}

void SDL_WM_SetCaption(const char *title, const char *icon)
{
	(void)title; (void)icon;
}

void SDL_WM_GetCaption(const char **title, const char **icon)
{
	if (title) *title = nullptr;
	if (icon)  *icon  = nullptr;
}

void SDL_WM_SetIcon(SDL_Surface *icon, Uint8 *mask)
{
	(void)icon; (void)mask;
}

/* ---- SDL1 Unicode / key-repeat API no-ops ---- */
/* SDL_EnableUNICODE: SDL2 always produces Unicode text events via SDL_TEXTINPUT.
 * SDL_EnableKeyRepeat: SDL2 key-repeat is built-in (event.key.repeat != 0). */

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

/* ---- Palette: SDL_SetColors compat ---- */

/* SDL_SetColors is declared in Emscripten's SDL_compat.h and implemented in
 * libsdl.js via canvas-based surface manipulation — incorrect for real C SDL2
 * surfaces.  Override with a correct SDL_SetPaletteColors wrapper. */
int SDLCALL SDL_SetColors(SDL_Surface *surface, const SDL_Color *colors, int firstcolor, int ncolors)
{
	if (!surface || !surface->format || !surface->format->palette) return 0;
	return SDL_SetPaletteColors(surface->format->palette,
	                            colors, firstcolor, ncolors) == 0 ? 1 : 0;
}

/* Mix_GroupChannels/Available/Oldest, Mix_SetPosition, Mix_FadeInChannelTimed,
 * Mix_GetMusicType, Mix_HookMusic: all provided by libSDL2_mixer-ogg.a.
 * No overrides needed; duplicate definitions would cause link errors.
 *
 * Mix_LoadMUS_RW: Emscripten SDL_mixer.h declares this with 1 parameter (SDL1
 * style).  Under -sUSE_SDL=2 the precompiled SDL2_mixer handles C-level RWops
 * correctly, so no override is needed.  The engine calls this with 1 arg. */

/* ---- SDL_gfx primitives (real implementations for 8bpp surfaces) ----
 *
 * Emscripten's SDL2 port does not include SDL2_gfx, so we provide C
 * implementations here for all SDL_gfx symbols referenced by OXCE:
 *  - filledCircleColor / texturedPolygon / lineColor / lineRGBA /
 *    filledPolygonColor: used by Globe (ocean, land, borders).
 *  - characterRGBA / stringRGBA / stringColor: used by Surface::drawString
 *    and BattlescapeState tile-debug overlays; no font available in Emscripten,
 *    so these are no-ops (STUB_ONCE warns once on first call).
 *
 * SDL_gfx colour arguments are packed as RGBA (r>>24 g>>16 b>>8 a).
 * For 8bpp surfaces we reverse-lookup the nearest palette index.
 * Palette must already be populated via SDL_SetColors / SDL_SetPaletteColors. */

// Convert SDL_gfx RGBA colour argument to ARGB8888 Uint32 for 32bpp surfaces.
// SDL_gfx packs as (r<<24)|(g<<16)|(b<<8)|a; SDL ARGB8888 is (a<<24)|(r<<16)|(g<<8)|b.
static Uint32 _gfx_rgba_to_argb(Uint32 rgba)
{
    return ((rgba & 0xFFu) << 24) | (rgba >> 8);
}

// Horizontal fill for 32bpp ARGB surfaces.
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

static Uint8 _gfx_pal_idx(SDL_Surface *dst, Uint32 rgba)
{
    if (!dst || !dst->format || !dst->format->palette ||
        !dst->format->palette->colors) return 0;
    Uint8 r = (rgba >> 24) & 0xFF;
    Uint8 g = (rgba >> 16) & 0xFF;
    Uint8 b = (rgba >>  8) & 0xFF;
    SDL_Palette *pal = dst->format->palette;
    int best = 0, best_d = INT_MAX;
    for (int i = 0; i < pal->ncolors; i++) {
        const SDL_Color &c = pal->colors[i];
        int d = (r-c.r)*(r-c.r) + (g-c.g)*(g-c.g) + (b-c.b)*(b-c.b);
        if (d < best_d) { best_d = d; best = i; if (!d) break; }
    }
    return (Uint8)best;
}

static void _gfx_hline(SDL_Surface *dst, int x1, int x2, int y, Uint8 idx)
{
    if (y < 0 || y >= dst->h) return;
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (x1 < 0)  x1 = 0;
    if (x2 >= dst->w) x2 = dst->w - 1;
    if (x1 > x2) return;
    memset((Uint8*)dst->pixels + y * dst->pitch + x1, idx, (size_t)(x2 - x1 + 1));
}

int characterRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, char c,
                  Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	STUB_ONCE();
	return 0;
}

int stringRGBA(SDL_Surface *dst, Sint16 x, Sint16 y, const char *s,
               Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	STUB_ONCE();
	return 0;
}

int stringColor(SDL_Surface *dst, Sint16 x, Sint16 y, const char *s, Uint32 color)
{
	STUB_ONCE();
	return 0;
}

int filledPolygonColor(SDL_Surface *dst, const Sint16 *vx, const Sint16 *vy, int n,
                       Uint32 color)
{
    if (!dst || !dst->pixels || n < 3) return -1;
    const bool is32 = (dst->format->BitsPerPixel == 32);
    Uint32 argbVal = is32 ? _gfx_rgba_to_argb(color) : 0;
    Uint8  idx     = is32 ? 0 : _gfx_pal_idx(dst, color);
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
        for (int j = 0; j + 1 < nxs; j += 2) {
            if (is32) _gfx_hline_argb(dst, xs[j], xs[j+1], y, argbVal);
            else      _gfx_hline(dst, xs[j], xs[j+1], y, idx);
        }
    }
    return 0;
}

int lineColor(SDL_Surface *dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
              Uint32 color)
{
    if (!dst || !dst->pixels) return -1;
    const bool argb = (dst->format->BitsPerPixel == 32);
    Uint32 argbVal = argb ? _gfx_rgba_to_argb(color) : 0;
    Uint8  idx     = argb ? 0 : _gfx_pal_idx(dst, color);
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1, sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy, x = x1, y = y1;
    for (;;) {
        if (x >= 0 && x < dst->w && y >= 0 && y < dst->h)
        {
            if (argb)
                ((Uint32*)((Uint8*)dst->pixels + y * dst->pitch))[x] = argbVal;
            else
                ((Uint8*)dst->pixels)[y * dst->pitch + x] = idx;
        }
        if (x == x2 && y == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 <  dx) { err += dx; y += sy; }
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
    const bool is32 = (dst->format->BitsPerPixel == 32);
    Uint32 argbVal = is32 ? _gfx_rgba_to_argb(color) : 0;
    Uint8  idx     = is32 ? 0 : _gfx_pal_idx(dst, color);
    int r = rad;
    for (int y = cy - r; y <= cy + r; y++) {
        int dy = y - cy;
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        if (is32) _gfx_hline_argb(dst, cx - dx, cx + dx, y, argbVal);
        else      _gfx_hline(dst, cx - dx, cx + dx, y, idx);
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
    const bool is32 = (dst->format->BitsPerPixel == 32);
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
        for (int j = 0; j + 1 < nxs; j += 2) {
            int x1 = xs[j];    if (x1 < 0)       x1 = 0;
            int x2 = xs[j+1];  if (x2 >= dst->w)  x2 = dst->w - 1;
            if (is32) {
                const Uint32 *trow32 = (const Uint32*)((const Uint8*)texture->pixels + ty * texture->pitch);
                Uint32       *drow32 = (Uint32*)((Uint8*)dst->pixels + y * dst->pitch);
                for (int x = x1; x <= x2; x++) {
                    int tx = ((x - texture_dx) % tw + tw) % tw;
                    drow32[x] = trow32[tx];
                }
            } else {
                const Uint8 *trow = (const Uint8*)texture->pixels + ty * texture->pitch;
                Uint8       *drow = (Uint8*)dst->pixels + y * dst->pitch;
                for (int x = x1; x <= x2; x++) {
                    int tx = ((x - texture_dx) % tw + tw) % tw;
                    drow[x] = trow[tx];
                }
            }
        }
    }
    return 0;
}

} /* extern "C" */
#endif /* __EMSCRIPTEN__ */
