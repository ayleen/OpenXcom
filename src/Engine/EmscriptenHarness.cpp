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
#include "Game.h"
#include "Screen.h"

using namespace OpenXcom;

extern Game *game;

extern "C" {

EMSCRIPTEN_KEEPALIVE
void calypso_screenshot(const char *path)
{
	if (game && game->getScreen())
		game->getScreen()->screenshot(path);
}

} /* extern "C" */

#endif /* __EMSCRIPTEN__ */
