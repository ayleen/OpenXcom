#ifdef __EMSCRIPTEN__
/*
 * Calypso browser canvas/scale handling (Phase 46.1.5) -- extracted verbatim
 * from Engine/Screen.cpp (policy R3 / R6 relocation-only).
 *   - normalizeBrowserScales: promote invalid stored scene fractions and
 *     synchronise their pending twins via the pure CalypsoResolutionFloor
 *     helpers.
 *   - reflowCanvasFallback: the flip() canvas-size resize path, routed through
 *     the Calypso viewport bridge (calypsoNotifyCanvasFallback).
 * Behaviour unchanged; see `git diff --color-moved`.
 */
#include <SDL.h>

#include "../Engine/Screen.h"
#include "../Engine/Options.h"
#include "../Engine/Logger.h"
#include "CalypsoResolutionFloor.h"
#include "CalypsoViewportRuntime.h"

namespace OpenXcom
{

void Screen::normalizeBrowserScales()
{
	const bool normalizedNonSquare =
		Calypso::calypsoNormalizeBrowserNonSquarePixels(Options::nonSquarePixelRatio);
	if (Options::nonSquarePixelRatio != normalizedNonSquare)
	{
		Options::nonSquarePixelRatio = normalizedNonSquare;
		Log(LOG_WARNING) << "[ui-resolution] ignored native-only nonSquarePixelRatio in browser options";
	}
	auto normalize = [](int& live, int& pending, const char *scene) {
		const Calypso::CalypsoScaleResult result = Calypso::calypsoPromoteScale(
			Options::displayWidth, Options::displayHeight, Options::nonSquarePixelRatio, live);
		const bool promoted = live != result.scaleType;
		live = pending = result.scaleType;
		if (promoted)
		{
			Log(LOG_WARNING) << "[ui-resolution] promoted " << scene << " scale to "
			                 << result.width << "x" << result.height;
		}
	};
	normalize(Options::geoscapeScale, Options::newGeoscapeScale, "Geoscape");
	normalize(Options::battlescapeScale, Options::newBattlescapeScale, "Battlescape");
}

void Screen::reflowCanvasFallback(int wW, int wH)
{
	const bool bridgeHandled = Calypso::calypsoNotifyCanvasFallback(wW, wH);
	if (bridgeHandled)
	{
		_forceCanvasRebase = false;
	}
	else if (_forceCanvasRebase)
	{
		/* Context recovery can require a scale rebase even when canvas size
		 * is unchanged and therefore no viewport event is queued. Preserve
		 * that established fallback without starting a second state reflow. */
		_forceCanvasRebase = false;
		const bool inBattle = _surface &&
		                      _surface->w == Options::baseXBattlescape &&
		                      _surface->h == Options::baseYBattlescape;
		Screen::updateScale(Options::battlescapeScale,
		                    Options::baseXBattlescape,
		                    Options::baseYBattlescape, inBattle);
		Screen::updateScale(Options::geoscapeScale,
		                    Options::baseXGeoscape,
		                    Options::baseYGeoscape, !inBattle);
	}
}

} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
