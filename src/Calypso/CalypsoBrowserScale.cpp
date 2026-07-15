#ifdef __EMSCRIPTEN__
/*
 * Calypso browser canvas/scale handling (Phase 46.1.5), extracted from
 * Engine/Screen.cpp under policy R3 / R6.
 *   - normalizeBrowserScales: independently promote invalid live and pending
 *     scene fractions via the pure CalypsoResolutionFloor helpers.
 *   - reflowCanvasFallback: the flip() canvas-size resize path, routed through
 *     the Calypso viewport bridge (calypsoNotifyCanvasFallback).
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
	const int storedGeoscapeLive = Options::geoscapeScale;
	const int storedGeoscapePending = Options::newGeoscapeScale;
	const int storedBattlescapeLive = Options::battlescapeScale;
	const int storedBattlescapePending = Options::newBattlescapeScale;
	Calypso::calypsoNormalizeBrowserScaleSnapshot(
		Options::displayWidth, Options::displayHeight, Options::nonSquarePixelRatio,
		Options::geoscapeScale, Options::newGeoscapeScale,
		Options::battlescapeScale, Options::newBattlescapeScale);

	auto logPromotion = [](int storedScale, int normalizedScale, const char *scene, const char *setting) {
		if (storedScale != normalizedScale)
		{
			const Calypso::CalypsoScaleResult result = Calypso::calypsoEvaluateScale(
				Options::displayWidth, Options::displayHeight,
				Options::nonSquarePixelRatio, normalizedScale);
			Log(LOG_WARNING) << "[ui-resolution] promoted " << scene << " " << setting << " scale to "
			                 << result.width << "x" << result.height;
		}
	};
	logPromotion(storedGeoscapeLive, Options::geoscapeScale, "Geoscape", "live");
	logPromotion(storedGeoscapePending, Options::newGeoscapeScale, "Geoscape", "pending");
	logPromotion(storedBattlescapeLive, Options::battlescapeScale, "Battlescape", "live");
	logPromotion(storedBattlescapePending, Options::newBattlescapeScale, "Battlescape", "pending");
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
