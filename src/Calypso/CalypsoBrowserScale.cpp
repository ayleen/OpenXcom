#ifdef __EMSCRIPTEN__
/*
 * Calypso browser canvas/scale handling (Phase 46.1.5), extracted from
 * Engine/Screen.cpp under policy R3 / R6.
 *   - normalizeBrowserScales: independently promote invalid live and pending
 *     scene fractions via the pure CalypsoResolutionFloor helpers.
 *   - reflowCanvasFallback: the flip() canvas-size resize path. Phase 46.4
 *     10.2.9 classifies every polled divergence through the pure
 *     CalypsoBackingStorePolicy: only an exactly matching PENDING viewport
 *     notification may adopt the observed size (the existing
 *     calypsoNotifyCanvasFallback flow); any unauthorized divergence is a
 *     hostile browser rewrite and is restored to Options::displayWidth/Height
 *     through the Emscripten canvas-size API -- never adopted, never
 *     aspect-clamped, never reflowed -- and reported to flip() so the stale
 *     frame is skipped instead of presented. While the HD harness host is up
 *     (calypsoHarnessHostUp), this classification is bypassed and the old
 *     unconditional bridge/rebase fallback runs instead -- harness fixtures
 *     only; production behavior is untouched.
 */
#include <emscripten.h>
#include <emscripten/html5.h>
#include <SDL.h>

#include "../Engine/Screen.h"
#include "../Engine/Options.h"
#include "../Engine/Logger.h"
#include "CalypsoBackingStorePolicy.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoResolutionFloor.h"
#include "CalypsoViewportRuntime.h"

extern "C" int g_calypsoHarnessPage;

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

bool Screen::reflowCanvasFallback(int wW, int wH)
{
	/* The established pre-policy bridge/rebase fallback: Adopt consumes/queues
	 * the authorized transition through calypsoNotifyCanvasFallback; when no
	 * viewport event is queued, context recovery can still require a scale
	 * rebase even with canvas size unchanged (preserved via _forceCanvasRebase
	 * without starting a second state reflow). Shared verbatim by the harness
	 * path below and the non-harness None/Adopt tail. */
	const auto runLegacyBridge = [this](int bridgeW, int bridgeH) {
		const bool bridgeHandled = Calypso::calypsoNotifyCanvasFallback(bridgeW, bridgeH);
		if (bridgeHandled)
		{
			_forceCanvasRebase = false;
		}
		else if (_forceCanvasRebase)
		{
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
	};

	if (g_calypsoHarnessPage != 0 || Calypso::calypsoHarnessHostUp(Calypso::calypsoHarnessSession()))
	{
		/* HD harness host is up: bypass the pending-only classification and
		 * run the OLD bridge behavior unconditionally so scripted harness
		 * resizes are always bridged. Production policy is unchanged. */
		runLegacyBridge(wW, wH);
		return false;
	}

	/* Pending viewport notification is the ONLY divergence authority. The HD
	 * overlay's earlier per-frame poll can already have observed (and echoed)
	 * a hostile reduced canvas, so committed runtime physical state must never
	 * authorize adoption -- only an unconsumed queued transition may. */
	int pendingWidth = 0;
	int pendingHeight = 0;
	Calypso::CalypsoCanvasAuthorization authorization;
	authorization.hasPendingViewport =
		Calypso::calypsoPendingViewportSize(pendingWidth, pendingHeight);
	authorization.pendingWidth = pendingWidth;
	authorization.pendingHeight = pendingHeight;

	const Calypso::CalypsoCanvasMismatchAction action = Calypso::calypsoClassifyCanvasResize(
		wW, wH, Options::displayWidth, Options::displayHeight, authorization);
	if (action == Calypso::CalypsoCanvasMismatchAction::Restore)
	{
		/* Unauthorized browser rewrite (SDL2's Emscripten backend maintains
		 * the element at CSS pixels because the engine window lacks
		 * SDL_WINDOW_ALLOW_HIGHDPI). Reassert the authoritative physical
		 * backing store through the Emscripten canvas-size API without
		 * adopting or reflowing layout, and report the skip so flip() never
		 * presents the frame prepared against the hostile polled size. */
		const EMSCRIPTEN_RESULT restored = emscripten_set_canvas_element_size(
			"#canvas", Options::displayWidth, Options::displayHeight);
		Log(LOG_WARNING) << "[ui-resolution] unauthorized canvas " << wW << "x" << wH
		                 << "; restoring backing store "
		                 << Options::displayWidth << "x" << Options::displayHeight
		                 << " (emscripten_set_canvas_element_size result "
		                 << static_cast<int>(restored) << ")";
		return true;
	}

	/* None (already equal) and Adopt (pending resize matches exactly) keep the
	 * existing bridge flow: Adopt consumes/queues the authorized transition,
	 * None still services the context-recovery rebase below. */
	runLegacyBridge(wW, wH);
	return false;
}

} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
