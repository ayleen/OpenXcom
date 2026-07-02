/*
 * Regression-test harness entry-points exported to JavaScript.
 * Phase 6a.1 — screenshot capture for snapshot diffing.
 * Phase 8b   — GPU framebuffer screenshot; ShaderManager hadGPUPass auto-route.
 *
 * calypso_screenshot(path) — writes a PNG of the current frame to `path`
 *   inside the Emscripten virtual filesystem; JS reads it back via
 *   Module.FS.readFile(path).  Auto-routes to GPU readback when the last
 *   frame had any registered GPU shader pass.
 * calypso_screenshot_gpu(path) — always uses GPU framebuffer readback.
 *
 * The global `game` pointer is declared in main.cpp (global namespace).
 * Game and Screen are included directly so the call chain resolves at
 * compile time without forward-declaration tricks.
 */
#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/heap.h>
#include <malloc.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <cstring>
#include "Game.h"
#include "Screen.h"
#include "ShaderManager.h"
#include "GpuSmokeState.h"
#include "Logger.h"
#include "FileMap.h"
#include "../Interface/Cursor.h"
// HTML main-menu bridge (Phase 2): the JS overlay drives these to push the same
// OXCE states the vanilla MainMenuState buttons would.
#include "../Menu/NewGameState.h"
#include "../Menu/NewBattleState.h"
#include "../Menu/ListLoadState.h"
#include "../Menu/ModListState.h"
#include "../Menu/OptionsVideoState.h"
#include "../Menu/OptionsBaseState.h"   // OptionsOrigin / OPT_MENU

using namespace OpenXcom;

/* ---- M5: heap-stats primitives -----------------------------------------------
 * mallinfo() fields are signed int — cast through unsigned to avoid negative
 * wrap when the dlmalloc arena grows past 2 GB (ALLOW_MEMORY_GROWTH). */
static size_t s_heapPrevUsed = 0;

static size_t heapUsedBytes()
{
    struct mallinfo mi = mallinfo();
    return (size_t)(unsigned int)mi.uordblks;
}

extern "C" {

/* Log one [HEAP] line: total / used / free in MB (1 decimal) + delta vs the
 * previous calypso_log_heap() call (first call's baseline is 0). */
EMSCRIPTEN_KEEPALIVE
void calypso_log_heap(const char *tag)
{
    const size_t MiB = 1048576;
    size_t total  = (size_t)emscripten_get_heap_size();
    size_t used   = heapUsedBytes();
    size_t free_  = total > used ? total - used : 0;
    long long delta  = (long long)used - (long long)s_heapPrevUsed;
    s_heapPrevUsed   = used;
    size_t adelta = (size_t)(delta >= 0 ? delta : -delta);
    char   sign   = delta >= 0 ? '+' : '-';
    auto   mb     = [MiB](size_t b) { return (long long)(b / MiB); };
    auto   mb1    = [MiB](size_t b) { return (long long)((b % MiB) * 10 / MiB); };
    Log(LOG_INFO) << "[HEAP] " << tag
                  << ": total=" << mb(total)  << "." << mb1(total)  << "MB"
                  << " used="   << mb(used)   << "." << mb1(used)   << "MB"
                  << " free="   << mb(free_)  << "." << mb1(free_)  << "MB"
                  << " delta="  << sign       << mb(adelta) << "." << mb1(adelta) << "MB";
}

/* Return heap utilisation / total linear-memory size in bytes as double for
 * JS-side polling: Module.ccall('calypso_heap_used', 'number', [], []). */
EMSCRIPTEN_KEEPALIVE
double calypso_heap_used(void)
{
    return (double)heapUsedBytes();
}

EMSCRIPTEN_KEEPALIVE
double calypso_heap_total(void)
{
    return (double)(size_t)emscripten_get_heap_size();
}

EMSCRIPTEN_KEEPALIVE
void calypso_screenshot(const char *path)
{
	OpenXcom::Game *g = OpenXcom::getCurrentGame();
	if (!g || !g->getScreen()) return;

	/* Auto-route to GPU framebuffer readback when a GPU pass ran this frame.
	 * Block 11.12: log the route at [INFO] so the regression harness can assert
	 * that Battlescape scenarios always capture via GPU. */
	if (OpenXcom::ShaderManager::instance().hadGPUPass())
	{
		Log(LOG_INFO) << "screenshot via GPU readback";
		g->getScreen()->screenshotGPU(path);
	}
	else
	{
		Log(LOG_INFO) << "screenshot via CPU surface";
		g->getScreen()->screenshot(path);
	}
}

/* Always read back from the GPU framebuffer, regardless of GPU-pass flag. */
EMSCRIPTEN_KEEPALIVE
void calypso_screenshot_gpu(const char *path)
{
	OpenXcom::Game *g = OpenXcom::getCurrentGame();
	if (g && g->getScreen())
		g->getScreen()->screenshotGPU(path);
}

/* Activate the GPU smoke-test scenario (Phase 8b — ?harness=gpu-smoke).
 * Registers a shader pass with Screen that renders for 5 frames then
 * saves a PNG to `path`.  Requires callMain to have been invoked first. */
EMSCRIPTEN_KEEPALIVE
void calypso_gpu_smoke_activate(const char *path)
{
	OpenXcom::Game *g = OpenXcom::getCurrentGame();
	if (!g || !g->getScreen())
	{
		/* Log to stderr so JS can detect the failure. */
		EM_ASM({ console.error('calypso_gpu_smoke_activate: game not running'); });
		return;
	}
	OpenXcom::GpuSmokeState::activate(g->getScreen(), path ? path : "/tmp/gpu-smoke.png");
}

/* ---- HTML main-menu bridge (Phase 2) ----------------------------------------
 * The JS menu overlay (web/public/menu.js) calls these to push the same OXCE
 * states the vanilla MainMenuState buttons would. Called from JS between frames;
 * pushState is applied on the next Game::run iteration. `using namespace OpenXcom`
 * (above) resolves Game/getCurrentGame/the State classes/OPT_MENU.
 *
 * Each returns int: 1 when a live Game handled the call, 0 when the engine isn't
 * ready yet (called before callMain / audio init). The JS bridge treats only a 1
 * as success, so a click that lands before boot is a safe no-op instead of a
 * false "navigated" that would tear the overlay down over a blank canvas. */
EMSCRIPTEN_KEEPALIVE int calypso_menu_new_game()   { if (Game *g = getCurrentGame()) { g->pushState(new NewGameState);            return 1; } return 0; }
EMSCRIPTEN_KEEPALIVE int calypso_menu_new_battle() { if (Game *g = getCurrentGame()) { g->pushState(new NewBattleState);          return 1; } return 0; }
EMSCRIPTEN_KEEPALIVE int calypso_menu_load()       { if (Game *g = getCurrentGame()) { g->pushState(new ListLoadState(OPT_MENU)); return 1; } return 0; }
EMSCRIPTEN_KEEPALIVE int calypso_menu_mods()       { if (Game *g = getCurrentGame()) { g->pushState(new ModListState);            return 1; } return 0; }
EMSCRIPTEN_KEEPALIVE int calypso_menu_options()    { if (Game *g = getCurrentGame()) { g->pushState(new OptionsVideoState(OPT_MENU)); return 1; } return 0; }

/* Silence the engine's menu music while the HTML menu (with its own water-ambience
 * audio) is shown; restore the engine volume when a menu action hands control to a
 * game state. Gated on a live Game so a pre-boot mute (audio not opened yet) can't
 * poison the saved volume — returns 0 until the engine is up, matching the menu_*
 * exports. Saved-volume guard makes repeated mute calls idempotent. */
static int s_calypsoSavedMusicVol = -1;
EMSCRIPTEN_KEEPALIVE int calypso_music_mute()
{
	if (!getCurrentGame()) return 0;
	if (s_calypsoSavedMusicVol < 0) { s_calypsoSavedMusicVol = Mix_VolumeMusic(-1); }
	Mix_VolumeMusic(0);
	return 1;
}
EMSCRIPTEN_KEEPALIVE int calypso_music_unmute()
{
	if (!getCurrentGame()) return 0;
	if (s_calypsoSavedMusicVol >= 0) { Mix_VolumeMusic(s_calypsoSavedMusicVol); s_calypsoSavedMusicVol = -1; }
	return 1;
}

/* The SDL2 Emscripten port routes WebGL-canvas pointermove events as
 * SDL_MOUSEBUTTONDOWN (buttonless), not SDL_MOUSEMOTION, which leaves the
 * OXCE Cursor stuck.  Hosting code in main.js registers a JS mousemove
 * listener that calls this with backing-store coordinates; we update the
 * Cursor directly (the SDL queue path was unreliable). */
/* Phase 8c §C2: opt-in perf log gate for Globe::drawSphereGPU. */
int g_calypsoProfileGlobe = 0;

EMSCRIPTEN_KEEPALIVE
void calypso_set_profile_globe(int on)
{
	g_calypsoProfileGlobe = on ? 1 : 0;
}

/* Phase 11.0: opt-in CPU perf gate for Map::drawTerrain.
 * JS toggles via calypso_set_profile_battlescape(1); production stays 0. */
int g_calypsoProfileBattlescape = 0;

EMSCRIPTEN_KEEPALIVE
void calypso_set_profile_battlescape(int on)
{
	g_calypsoProfileBattlescape = on ? 1 : 0;
}

/* Phase 11.1: opt-in readback-cost probe gate for Map::drawTerrain.
 * Runs FBO solid-colour + glReadPixels at Battlescape surface size;
 * self-terminates after 30 samples. */
int g_calypsoProfileReadback = 0;

EMSCRIPTEN_KEEPALIVE
void calypso_set_profile_readback(int on)
{
	g_calypsoProfileReadback = on ? 1 : 0;
}

/* Asset-audit mode: logs every resolved asset relpath once, tagged VANILLA
 * (served from the streamed TFTD payload) or REPLACED (served from a Calypso
 * mod overlay). JS toggles via ?audit=1 -> calypso_set_audit_mode(1); the
 * printErr handler in main.js parses the "[CALYPSO] ASSET ..." marker into
 * window.__assetAudit. See FileMap::at() and scripts/gen-asset-coverage.py. */
EMSCRIPTEN_KEEPALIVE
void calypso_set_audit_mode(int on)
{
	FileMap::setAuditMode(on != 0);
}

/* Phase 28: underwater colour-grade strength (0 = neutral .. 1 = deepest).
 * Map::drawSceneGrade() reads this each frame as the u_strength uniform.
 * Live-tunable from the JS console: Module._calypso_set_underwater_strength(0.4).
 * Default matches the "L1" starting look chosen during authoring. */
float g_calypsoUnderwaterStrength = 0.20f;

EMSCRIPTEN_KEEPALIVE
void calypso_set_underwater_strength(float v)
{
	g_calypsoUnderwaterStrength = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

/* Phase 28 batch-1 beauty FX amplitudes (0 = off). All read by
 * Map::drawSceneGrade() each frame and live-tunable from the JS console. */
float g_calypsoUwCaustics = 0.55f;
float g_calypsoUwRefract  = 0.40f;   // weaker — subtle wobble, not seasick
float g_calypsoUwBubbles  = 0.0f;    // seabed vents OFF (screen-anchored for now)
float g_calypsoUwSnow     = 0.5f;
float g_calypsoUwUnitBub  = 1.0f;    // HD bubbles, driven by the vanilla breath anim
float g_calypsoUwGodray   = 0.1f;    // batch 2: light shafts — subtle
float g_calypsoUwBloom    = 0.5f;    // batch 2: glow on bright spots
float g_calypsoUwBreath   = 0.6f;    // batch 2: slow global light pulse
float g_calypsoUwChroma   = 0.0f;    // OFF — no visible effect (scene edges are void)
float g_calypsoUwShock    = 0.7f;    // E2: explosion shockwave-ring distortion (underwater)
// Phase 25 (R1): coloured emissive halo amount (fire). The uw_ prefix keeps it in
// the existing knob family, but the emissive pass is mission-agnostic — it fires
// on land maps too (fire tiles), unlike the underwater-only grade/beauty FX.
float g_calypsoUwEmissive = 1.0f;
// Phase 25 (R6): HD material-emissive atlas multiplier (lava / bioluminescence).
// Scales the per-dataset emissiveFile glow added in tile_atlas_rgba.frag, into
// the HDR SSAA buffer (R0). 0 = off; > ~1 pushes bright texels past 1.0 so the
// HDR tonemap blooms them. Default subtle; live-tune via _calypso_set_tile_emissive.
float g_calypsoTileEmissive = 1.5f;
// Phase 25 (R7): unit "fake lighting" amount — a sprite-local vertical AO/relief on
// unit bodies (in tile_atlas.frag) so they gain volume + a grounding shadow without
// an RGBA atlas or baked-AO art. 0 = off (legacy flat units); 1 = full. Tiles +
// floor items are never affected. Live-tune via _calypso_set_unit_shade.
float g_calypsoUnitShade = 1.0f;

static float clamp01p(float v) { return v < 0.0f ? 0.0f : (v > 2.0f ? 2.0f : v); }
static float clamp08 (float v) { return v < 0.0f ? 0.0f : (v > 8.0f ? 8.0f : v); }
static float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

EMSCRIPTEN_KEEPALIVE void calypso_set_uw_caustics(float v) { g_calypsoUwCaustics = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_refract (float v) { g_calypsoUwRefract  = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_bubbles (float v) { g_calypsoUwBubbles  = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_snow    (float v) { g_calypsoUwSnow     = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_unitbub (float v) { g_calypsoUwUnitBub  = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_godray  (float v) { g_calypsoUwGodray   = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_bloom   (float v) { g_calypsoUwBloom    = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_breath  (float v) { g_calypsoUwBreath   = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_chroma  (float v) { g_calypsoUwChroma   = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_shock   (float v) { g_calypsoUwShock    = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_uw_emissive(float v) { g_calypsoUwEmissive = clamp01p(v); }
EMSCRIPTEN_KEEPALIVE void calypso_set_tile_emissive(float v) { g_calypsoTileEmissive = clamp08(v); } // Phase 25 R6
EMSCRIPTEN_KEEPALIVE void calypso_set_unit_shade  (float v) { g_calypsoUnitShade   = clamp01(v); } // Phase 25 R7

/* L2 (memory-reduction): runtime SSAA supersample-factor override.
 * 0 = "unset" — Map::ensureSsaaTarget falls back to _ssaaScale (default 2×).
 * calypso_set_ssaa_scale(1) disables supersampling (HDR retained), freeing
 * ~105 MiB GPU VRAM at FHD.  Valid clamped range: 1–4. */
int g_calypsoSsaaScale = 0;

EMSCRIPTEN_KEEPALIVE
void calypso_set_ssaa_scale(int s)
{
	g_calypsoSsaaScale = s < 1 ? 0 : (s > 4 ? 4 : s);
}

/* Phase 25 (R3): tangent-space sun direction for normal-map relief. The shader
 * normalises it. In production the engine DRIVES this automatically (a per-turn
 * azimuth sweep — "time of day" — in the upper hemisphere, coherent with the
 * surface god-rays); see Map::drawTileGLPass. g_calypsoSunAuto gates that. The
 * relief STRENGTH is baked into the atlas at build time (ruleset normalStrength:).
 * Dev override: Module._calypso_set_sun_dir(x, y, z) freezes a manual direction;
 * Module._calypso_set_sun_auto(1) resumes the automatic sweep. */
float g_calypsoSunDir[3] = { -0.40f, -0.40f, 0.82f };
int   g_calypsoSunAuto   = 1;   // 1 = engine drives the sun; 0 = manual override

EMSCRIPTEN_KEEPALIVE
void calypso_set_sun_dir(float x, float y, float z)
{
	// Reject a degenerate zero vector: the shaders do normalize(u_sunDir), and
	// normalize(vec3(0)) is UB in GLSL ES (NaN on most drivers) — it would
	// corrupt the relief term for every normal-mapped tile. Keep the prior value.
	if (x * x + y * y + z * z < 1e-12f) return;
	g_calypsoSunDir[0] = x; g_calypsoSunDir[1] = y; g_calypsoSunDir[2] = z;
	g_calypsoSunAuto = 0;   // a manual set freezes the automatic sweep (dev override)
}

EMSCRIPTEN_KEEPALIVE
void calypso_set_sun_auto(int on) { g_calypsoSunAuto = on ? 1 : 0; }

/* Phase-14 railings debug: one-shot tile/painter dump.
 * JS toggles via Module._calypso_dump_emit_once() before forcing a redraw;
 * Map::emitTilePass() and Map::draw() (painter) each log every tile they
 * see and reset the flag, so production runs at zero cost. */
int g_calypsoDumpEmit = 0;

EMSCRIPTEN_KEEPALIVE
void calypso_dump_emit_once()
{
	g_calypsoDumpEmit = 1;
}

EMSCRIPTEN_KEEPALIVE
void calypso_push_mouse_motion(int x, int y)
{
	OpenXcom::Game *g = OpenXcom::getCurrentGame();
	if (!g) return;
	OpenXcom::Cursor *c = g->getCursor();
	OpenXcom::Screen *s = g->getScreen();
	if (!c || !s) return;
	/* JS sends canvas-backing pixels; convert to game-coords via the
	 * Screen's current xScale/yScale (canvas / base). */
	double sx = s->getXScale();
	double sy = s->getYScale();
	if (sx <= 0.0) sx = 1.0;
	if (sy <= 0.0) sy = 1.0;
	c->setX((int)(x / sx));
	c->setY((int)(y / sy));
}

} /* extern "C" */

#endif /* __EMSCRIPTEN__ */
