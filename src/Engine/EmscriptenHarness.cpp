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
#include <SDL.h>
#include <cstring>
#include "Game.h"
#include "Screen.h"
#include "ShaderManager.h"
#include "GpuSmokeState.h"
#include "../Interface/Cursor.h"

extern "C" {

EMSCRIPTEN_KEEPALIVE
void calypso_screenshot(const char *path)
{
	OpenXcom::Game *g = OpenXcom::getCurrentGame();
	if (!g || !g->getScreen()) return;

	/* Auto-route to GPU framebuffer readback when a GPU pass ran this frame. */
	if (OpenXcom::ShaderManager::instance().hadGPUPass())
		g->getScreen()->screenshotGPU(path);
	else
		g->getScreen()->screenshot(path);
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
