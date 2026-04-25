/*
 * Stub implementations for SDL/SDL_mixer/SDL_gfx functions not provided
 * by Emscripten's libsdl.js emulation layer (-sUSE_SDL=1).
 * All stubs are intentionally no-ops or minimal; audio/video playback
 * and threading are not supported in the Phase-2 WASM build.
 *
 * Each no-op stub logs a warning to stderr on its first invocation so
 * that code paths silently hitting a no-op can be spotted during
 * development. Logging is rate-limited to once per call site via a
 * per-function `static bool`.
 */
#ifdef __EMSCRIPTEN__

#include <SDL/SDL.h>
#include <SDL/SDL_mouse.h>
#include <SDL/SDL_mixer.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_thread.h>
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

/* ---- Events ---- */

Uint8 SDL_EventState(Uint32 type, int state)
{
	STUB_ONCE();
	return SDL_ENABLE;
}

/* ---- SDL_Delay ---- */

/* SDL_Delay on the Emscripten main thread blocks the event loop and causes
 * Emscripten to abort with "potential infinite loop". Use a no-op instead.
 * Game.cpp already guards its SDL_Delay calls with #ifndef __EMSCRIPTEN__;
 * this covers the FLC player and any other code paths we missed. */
void SDL_Delay(Uint32 ms)
{
    /* intentional no-op — Emscripten main loop pacing is done by rAF */
    (void)ms;
}

/* ---- Cursor ---- */

SDL_Cursor *SDL_CreateCursor(const Uint8 *data, const Uint8 *mask,
                             int w, int h, int hot_x, int hot_y)
{
	STUB_ONCE();
	return NULL;
}

void SDL_SetCursor(SDL_Cursor *cursor)
{
	STUB_ONCE();
}

SDL_Cursor *SDL_GetCursor(void)
{
	STUB_ONCE();
	return NULL;
}

void SDL_FreeCursor(SDL_Cursor *cursor)
{
	STUB_ONCE();
}

/* ---- Environment ---- */

int SDL_putenv(const char *variable)
{
	STUB_ONCE();
	return 0;
}

/* ---- Threading ---- */

SDL_Thread *SDL_CreateThread(SDL_ThreadFunction fn, void *data)
{
	STUB_ONCE();
	return NULL;  /* caller falls back to synchronous execution */
}

void SDL_WaitThread(SDL_Thread *thread, int *status)
{
	STUB_ONCE();
}

/* ---- Semaphores ---- */

SDL_sem *SDL_CreateSemaphore(Uint32 initial_value)
{
	STUB_ONCE();
	return NULL;
}

void SDL_DestroySemaphore(SDL_sem *sem)
{
	STUB_ONCE();
}

int SDL_SemWait(SDL_sem *sem)
{
	STUB_ONCE();
	return 0;
}

int SDL_SemPost(SDL_sem *sem)
{
	STUB_ONCE();
	return 0;
}

/* ---- RW operations (real implementations, not stubs) ---- */

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
	STUB_ONCE();
}

/* ---- Palette (SDL_SetColors) ---- */

/* Override SDL_SetColors so that surface->format->palette->colors is updated in WASM
 * memory. Under Emscripten SDL1, the JS version only updates SDL.surfaces[surf].colors
 * (the JS canvas palette), NOT the C struct palette. emscripten_flip_8bpp reads the
 * C struct palette directly, so we must update it here.
 *
 * SDL_CreateRGBSurfaceFrom (non-32bpp) returns with format->palette = NULL because
 * Emscripten's JS makeSurface sets palette=0 for all surfaces. We lazily allocate a
 * 256-entry C-side palette on the first SDL_SetColors call so that code reading
 * format->palette->colors finds real data instead of WASM address 0. */
int SDLCALL SDL_SetColors(SDL_Surface *surface, const SDL_Color *colors, int firstcolor, int ncolors)
{
    if (!surface || !surface->format || !colors || ncolors <= 0) return 0;
    if (!surface->format->palette) {
        auto *pal = (SDL_Palette*)SDL_malloc(sizeof(SDL_Palette));
        if (!pal) return 0;
        auto *col = (SDL_Color*)SDL_calloc(256, sizeof(SDL_Color));
        if (!col) { SDL_free(pal); return 0; }
        pal->ncolors  = 256;
        pal->colors   = col;
        pal->version  = 1;
        pal->refcount = 1;
        surface->format->palette = pal;
    }
    SDL_Palette *pal = surface->format->palette;
    int end = firstcolor + ncolors;
    if (end > pal->ncolors) end = pal->ncolors;
    if (firstcolor < end)
        memcpy(pal->colors + firstcolor, colors,
               (size_t)(end - firstcolor) * sizeof(SDL_Color));
    return 1;
}

/* ---- SDL_mixer functions missing from Emscripten's USE_SDL_MIXER=1 port ----
 *
 * These are proper implementations (not no-op stubs) for the subset of
 * SDL1_mixer that Emscripten's port omits. Mix_Playing / Mix_Playing(ch)
 * IS available in the port and is used here. */

static int _emcc_channel_groups[64];
static bool _emcc_groups_init = false;

static void _emcc_init_groups() {
    if (!_emcc_groups_init) {
        for (int i = 0; i < 64; ++i) _emcc_channel_groups[i] = -1;
        _emcc_groups_init = true;
    }
}

/* Tag channels [from..to] with tag. Returns number of channels assigned. */
int Mix_GroupChannels(int from, int to, int tag)
{
    _emcc_init_groups();
    int count = 0;
    for (int i = from; i <= to && i < 64; ++i) {
        _emcc_channel_groups[i] = tag;
        ++count;
    }
    return count;
}

/* Return first idle channel with the given tag, or -1.
 * Also resets SDL.channels[i].audio for channels whose audio has ended
 * (paused=true after onended fires), so Emscripten's Mix_PlayChannelTimed
 * free-channel search (!audio) can reuse them instead of exhausting all 32. */
int Mix_GroupAvailable(int tag)
{
    /* Clear any finished channels so JS sees them as free for reuse. */
    EM_ASM({
        if (typeof SDL !== 'undefined' && SDL.channels) {
            for (var i = 0; i < SDL.channels.length; i++) {
                var ch = SDL.channels[i];
                if (ch && ch.audio && ch.audio.paused) ch.audio = null;
            }
        }
    });
    _emcc_init_groups();
    for (int i = 0; i < 64; ++i) {
        if (_emcc_channel_groups[i] == tag && !Mix_Playing(i))
            return i;
    }
    return -1;
}

/* Return oldest active channel in group (needed by OXCE SFX eviction). */
int Mix_GroupOldest(int tag)
{
    _emcc_init_groups();
    for (int i = 0; i < 64; ++i) {
        if (_emcc_channel_groups[i] == tag && Mix_Playing(i))
            return i;
    }
    return -1;
}

/* Mix_SetPosition is defined in Emscripten's port as a JS TODO that aborts.
 * Override it with a silent no-op — spatial positioning is not needed in browser. */
int Mix_SetPosition(int channel, Sint16 angle, Uint8 distance)
{
    (void)channel; (void)angle; (void)distance;
    return 0;
}

/* Emscripten port omits Mix_FadeInChannelTimed; fall back to plain PlayChannel. */
int Mix_FadeInChannelTimed(int channel, Mix_Chunk *chunk, int loops, int ms, int ticks)
{
    (void)ms; (void)ticks;
    return Mix_PlayChannel(channel, chunk, loops);
}

/* Returns OGG for any loaded music; MUS_NONE for null.
 * The Emscripten port converts all formats to OGG internally anyway. */
Mix_MusicType Mix_GetMusicType(const Mix_Music *music)
{
    return music ? MUS_OGG : MUS_NONE;
}

/* AdlibMusic uses a custom mix hook for its software MIDI renderer.
 * Under Emscripten we serve pre-converted OGG files via Mix_LoadMUS instead,
 * so no custom hook is needed. */
void Mix_HookMusic(void (*mix_func)(void *udata, Uint8 *stream, int len),
                   void *arg)
{
    (void)mix_func; (void)arg;
}

/* Mix_LoadMUS_RW: FileMap::getRWops returns a C-level SDL_RWops (em_file_to_rwops)
 * that is NOT registered in the JS SDL.rwops table. Emscripten's Mix_LoadWAV_RW
 * does SDL.rwops[id] where id is a WASM heap pointer → undefined → returns 0.
 * Fix: drain bytes from the C-level RWops, re-register them via SDL_RWFromMem
 * (which IS Emscripten's JS function and puts the buffer into SDL.rwops),
 * call Mix_LoadWAV_RW with the resulting JS-side integer id, then free. */
Mix_Music *Mix_LoadMUS_RW(SDL_RWops *src)
{
    if (!src) return nullptr;
    /* SDL1 has no SDL_RWsize; compute size by seeking to end. */
    int start = SDL_RWseek(src, 0, SEEK_SET);
    int end   = SDL_RWseek(src, 0, SEEK_END);
    int size  = end - start;
    if (size <= 0) return nullptr;
    SDL_RWseek(src, 0, SEEK_SET);
    void *buf = malloc((size_t)size);
    if (!buf) return nullptr;
    size_t got = SDL_RWread(src, buf, 1, (size_t)size);
    Mix_Music *result = nullptr;
    if ((int)got == size) {
        /* SDL_RWFromMem is Emscripten's JS function: pushes {bytes, count} into
         * SDL.rwops and returns a small integer index that Mix_LoadWAV_RW accepts. */
        SDL_RWops *js_rw = SDL_RWFromMem(buf, size);
        if (js_rw) {
            /* Mix_LoadWAV_RW copies bytes via HEAPU8.buffer.slice() on the webAudio
             * path, so buf can be freed immediately after the call. freesrc=0. */
            result = (Mix_Music *)(intptr_t)Mix_LoadWAV_RW(js_rw, 0);
            SDL_FreeRW(js_rw);
        }
    }
    free(buf);
    return result;
}

/* ---- SDL_gfx primitives (real implementations for 8bpp surfaces) ----
 *
 * OXCE's Globe renders ocean, land polygons, and country borders using
 * filledCircleColor / texturedPolygon / lineColor on 8bpp palettized
 * surfaces. Emscripten's libsdl.js does not include SDL_gfx, so we
 * provide C implementations here.
 *
 * SDL_gfx colour arguments are packed as RGBA (r>>24 g>>16 b>>8 a).
 * For 8bpp surfaces we reverse-lookup the nearest palette index.
 * Palette must already be populated via SDL_SetColors (our override
 * lazily allocates the C-side SDL_Palette on first call). */

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

int lineColor(SDL_Surface *dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
              Uint32 color)
{
    if (!dst || !dst->pixels) return -1;
    Uint8 idx = _gfx_pal_idx(dst, color);
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1, sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy, x = x1, y = y1;
    for (;;) {
        if (x >= 0 && x < dst->w && y >= 0 && y < dst->h)
            ((Uint8*)dst->pixels)[y * dst->pitch + x] = idx;
        if (x == x2 && y == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 <  dx) { err += dx; y += sy; }
    }
    return 0;
}

int filledCircleColor(SDL_Surface *dst, Sint16 cx, Sint16 cy, Sint16 rad,
                      Uint32 color)
{
    if (!dst || !dst->pixels || rad < 0) return -1;
    Uint8 idx = _gfx_pal_idx(dst, color);
    int r = rad;
    for (int y = cy - r; y <= cy + r; y++) {
        int dy = y - cy;
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        _gfx_hline(dst, cx - dx, cx + dx, y, idx);
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
        /* Collect edge intersections for this scan line. */
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
        /* Insertion sort (nxs ≤ 8 in practice). */
        for (int a = 1; a < nxs; a++) {
            int key = xs[a], b = a - 1;
            while (b >= 0 && xs[b] > key) { xs[b+1] = xs[b]; b--; }
            xs[b+1] = key;
        }
        int ty = ((y - texture_dy) % th + th) % th;
        const Uint8 *trow = (const Uint8*)texture->pixels + ty * texture->pitch;
        Uint8 *drow = (Uint8*)dst->pixels + y * dst->pitch;
        for (int j = 0; j + 1 < nxs; j += 2) {
            int x1 = xs[j];     if (x1 < 0)       x1 = 0;
            int x2 = xs[j+1];   if (x2 >= dst->w)  x2 = dst->w - 1;
            for (int x = x1; x <= x2; x++) {
                int tx = ((x - texture_dx) % tw + tw) % tw;
                drow[x] = trow[tx];
            }
        }
    }
    return 0;
}

/* ---- C-side rendering: bypass Emscripten JS canvas so surf->pixels stays valid ----
 *
 * Emscripten SDL1 compat routes SDL_BlitSurface / SDL_FillRect / SDL_LockSurface
 * through JS canvas operations and never touches surf->pixels in C memory.
 * emscripten_flip_8bpp (in Screen.cpp) reads surf->pixels directly → all-black canvas.
 *
 * These C overrides intercept calls from C code only. JS callers (IMG_Load_RW etc.)
 * still use the _SDL_LockSurface JS proxy, which is correct for those code paths.
 *
 * SDL_LockSurface / SDL_UnlockSurface become no-ops so the JS layer never overwrites
 * surf->pixels with its own heap buffer.
 *
 * Colorkey: SDL2 headers (used by Emscripten) have no colorkey field in SDL_PixelFormat.
 * We store the 8-bit colorkey value in surface->userdata (application-use field) and
 * signal its presence via SDL_SRCCOLORKEY (0x00020000, from SDL_compat.h) in flags.
 */

int SDLCALL SDL_LockSurface(SDL_Surface *surface) { (void)surface; return 0; }
void SDLCALL SDL_UnlockSurface(SDL_Surface *surface) { (void)surface; }

int SDLCALL SDL_SetColorKey(SDL_Surface *surface, int flag, Uint32 key)
{
    if (!surface) return -1;
    if (flag) {
        surface->flags |= SDL_SRCCOLORKEY;
        surface->userdata = (void*)(uintptr_t)(key & 0xFF);
    } else {
        surface->flags &= ~SDL_SRCCOLORKEY;
    }
    return 0;
}

int SDLCALL SDL_UpperBlit(SDL_Surface *src, const SDL_Rect *srcrect,
                          SDL_Surface *dst, SDL_Rect *dstrect)
{
    if (!src || !dst || !src->pixels || !dst->pixels) return -1;
    int sx = 0, sy = 0, sw = src->w, sh = src->h;
    if (srcrect) { sx = srcrect->x; sy = srcrect->y; sw = srcrect->w; sh = srcrect->h; }
    int dx = 0, dy = 0;
    if (dstrect) { dx = dstrect->x; dy = dstrect->y; }
    /* clamp source rect to source surface */
    if (sx < 0) { dx -= sx; sw += sx; sx = 0; }
    if (sy < 0) { dy -= sy; sh += sy; sy = 0; }
    if (sx + sw > src->w) sw = src->w - sx;
    if (sy + sh > src->h) sh = src->h - sy;
    /* clamp destination offset to destination surface */
    if (dx < 0) { sx -= dx; sw += dx; dx = 0; }
    if (dy < 0) { sy -= dy; sh += dy; dy = 0; }
    if (dx + sw > dst->w) sw = dst->w - dx;
    if (dy + sh > dst->h) sh = dst->h - dy;
    if (sw <= 0 || sh <= 0) return 0;
    if (dstrect) { dstrect->x = dx; dstrect->y = dy;
                   dstrect->w = sw; dstrect->h = sh; }
    int src_bpp = src->format ? src->format->BytesPerPixel : 1;
    int dst_bpp = dst->format ? dst->format->BytesPerPixel : 1;
    bool use_colorkey = (src->flags & SDL_SRCCOLORKEY) != 0;
    Uint8 colorkey = use_colorkey ? (Uint8)(uintptr_t)(src->userdata) : 0;
    for (int row = 0; row < sh; ++row) {
        const Uint8 *sp = (const Uint8*)src->pixels + (sy + row) * src->pitch + sx * src_bpp;
        Uint8       *dp = (Uint8*)      dst->pixels + (dy + row) * dst->pitch + dx * dst_bpp;
        if (src_bpp == dst_bpp) {
            if (!use_colorkey) {
                memcpy(dp, sp, (size_t)sw * src_bpp);
            } else {
                for (int col = 0; col < sw; ++col)
                    if (sp[col] != colorkey) dp[col] = sp[col];
            }
        } else if (src_bpp == 4 && dst_bpp == 1) {
            /* 32bpp RGBA → 8bpp: dosFont BMP decoded by STB_IMAGE.
             * RGB(0,0,0) = black background → palette index 0.
             * Any non-black pixel → foreground glyph → palette index 1. */
            for (int col = 0; col < sw; ++col) {
                const Uint8 *px = sp + (size_t)col * 4;
                dp[col] = (px[0] | px[1] | px[2]) ? 1 : 0;
            }
        }
        /* other cross-bpp combinations not needed */
    }
    return 0;
}

int SDLCALL SDL_LowerBlit(SDL_Surface *src, SDL_Rect *srcrect,
                          SDL_Surface *dst, SDL_Rect *dstrect)
{
    return SDL_UpperBlit(src, srcrect, dst, dstrect);
}

int SDLCALL SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color)
{
    if (!dst || !dst->pixels) return -1;
    int x = 0, y = 0, w = dst->w, h = dst->h;
    if (rect) { x = rect->x; y = rect->y; w = rect->w; h = rect->h; }
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > dst->w) w = dst->w - x;
    if (y + h > dst->h) h = dst->h - y;
    if (w <= 0 || h <= 0) return 0;
    int bpp = dst->format ? dst->format->BytesPerPixel : 1;
    for (int row = 0; row < h; ++row) {
        Uint8 *ptr = (Uint8*)dst->pixels + (y + row) * dst->pitch + x * bpp;
        if (bpp == 1) {
            memset(ptr, (Uint8)color, (size_t)w);
        } else {
            Uint32 *p32 = (Uint32*)ptr;
            for (int col = 0; col < w; ++col) p32[col] = color;
        }
    }
    return 0;
}

} /* extern "C" */

#endif /* __EMSCRIPTEN__ */
