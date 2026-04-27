/*
 * Regression-test harness entry-points exported to JavaScript.
 * Phase 6a.1 — screenshot capture for snapshot diffing.
 *
 * calypso_screenshot(path) — writes a PNG of the current frame to `path`
 *   inside the Emscripten virtual filesystem; JS reads it back via
 *   Module.FS.readFile(path).
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
#include "../Interface/Cursor.h"

extern "C" {

EMSCRIPTEN_KEEPALIVE
void calypso_screenshot(const char *path)
{
	OpenXcom::Game *g = OpenXcom::getCurrentGame();
	if (g && g->getScreen())
		g->getScreen()->screenshot(path);
}

/* The SDL2 Emscripten port routes WebGL-canvas pointermove events as
 * SDL_MOUSEBUTTONDOWN (buttonless), not SDL_MOUSEMOTION, which leaves the
 * OXCE Cursor stuck.  Hosting code in main.js registers a JS mousemove
 * listener that calls this with backing-store coordinates; we update the
 * Cursor directly (the SDL queue path was unreliable). */
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
