#ifdef __EMSCRIPTEN__
/*
 * Calypso viewport reflow (Phase 46.1) -- extracted verbatim from
 * Engine/Game.cpp (policy R3 / R6 relocation-only). The whole-file guard
 * replaces Game.cpp's nested `#ifdef __EMSCRIPTEN__` around the
 * normalizeBrowserScales() call (R1: no nested emscripten guards inside a
 * Calypso TU). Behaviour is unchanged; see `git diff --color-moved`.
 */
#include <algorithm>

#include "../Engine/Game.h"
#include "../Engine/State.h"
#include "../Engine/Screen.h"
#include "../Engine/Options.h"
#include "../Engine/Logger.h"
#include "../Battlescape/BattlescapeState.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Savegame/SavedGame.h"
#include "CalypsoViewportRuntime.h"

namespace OpenXcom
{

/**
 * Apply one bridge-authorized physical viewport resize as a single reflow.
 * The active root sees the previous base resolution and computes the canonical
 * delta. Remaining overlays receive a fresh copy of that delta bottom-to-top;
 * the root is never resized twice. Browser-generated SDL probes are rejected
 * by the pending-transition handshake before this method is entered.
 */
void Game::reflowEmscriptenViewport(int physicalWidth, int physicalHeight)
{
	Calypso::CalypsoPendingViewportResize transition;
	if (!Calypso::calypsoConsumePendingViewportResize(
		physicalWidth, physicalHeight, transition))
	{
		Log(LOG_DEBUG) << "[ui-resize] ignored unbridged SDL window probe "
		               << physicalWidth << "x" << physicalHeight;
		return;
	}
	if (physicalWidth <= 0 || physicalHeight <= 0)
		return;

	const int oldPhysicalWidth = Options::displayWidth;
	const int oldPhysicalHeight = Options::displayHeight;

	const int oldBaseWidth = Options::baseXResolution;
	const int oldBaseHeight = Options::baseYResolution;
	Options::newDisplayWidth = Options::displayWidth =
		std::max(Screen::ORIGINAL_WIDTH, physicalWidth);
	Options::newDisplayHeight = Options::displayHeight =
		std::max(Screen::ORIGINAL_HEIGHT, physicalHeight);
	Screen::normalizeBrowserScales();

	BattlescapeState *battleRoot = nullptr;
	GeoscapeState *geoRoot = nullptr;
	for (State *state : _states)
	{
		if (auto *battle = dynamic_cast<BattlescapeState *>(state)) battleRoot = battle;
		if (auto *geo = dynamic_cast<GeoscapeState *>(state)) geoRoot = geo;
	}
	const bool tactical = battleRoot != nullptr
		|| (_save && _save->getSavedBattle() != nullptr);
	State *root = tactical ? static_cast<State *>(battleRoot)
	                       : static_cast<State *>(geoRoot);

	int dX = 0;
	int dY = 0;
	if (root)
	{
		root->resize(dX, dY);
	}
	else
	{
		int targetWidth = oldBaseWidth;
		int targetHeight = oldBaseHeight;
		if (tactical)
			Screen::updateScale(Options::battlescapeScale, targetWidth, targetHeight, false);
		else
			Screen::updateScale(Options::geoscapeScale, targetWidth, targetHeight, false);
		dX = targetWidth - oldBaseWidth;
		dY = targetHeight - oldBaseHeight;
	}

	const int targetBaseWidth = oldBaseWidth + dX;
	const int targetBaseHeight = oldBaseHeight + dY;
	if (tactical)
	{
		Options::baseXBattlescape = targetBaseWidth;
		Options::baseYBattlescape = targetBaseHeight;
	}
	else
	{
		Options::baseXGeoscape = targetBaseWidth;
		Options::baseYGeoscape = targetBaseHeight;
	}

	for (State *state : _states)
	{
		if (state == root) continue;
		int stateDX = dX;
		int stateDY = dY;
		state->resize(stateDX, stateDY);
	}
	// Rootless stacks (boot/menu/briefing) may contain no state that owns the
	// active base. Commit the precomputed target after their resize hooks.
	Options::baseXResolution = targetBaseWidth;
	Options::baseYResolution = targetBaseHeight;

	const bool displayChanged = Options::displayWidth != oldPhysicalWidth
	                         || Options::displayHeight != oldPhysicalHeight;
	const bool baseChanged = Options::baseXResolution != oldBaseWidth
	                      || Options::baseYResolution != oldBaseHeight;
	if (displayChanged || baseChanged)
		_screen->resetDisplay(false, false);
	Log(LOG_INFO) << "[ui-resize] logical="
	              << transition.previousLogicalWidth << "x" << transition.previousLogicalHeight
	              << "->" << transition.logicalWidth << "x" << transition.logicalHeight
	              << " physical=" << oldPhysicalWidth << "x" << oldPhysicalHeight
	              << "->" << Options::displayWidth << "x" << Options::displayHeight
	              << " base=" << oldBaseWidth << "x" << oldBaseHeight
	              << "->" << Options::baseXResolution << "x" << Options::baseYResolution
	              << " changed=" << (transition.logicalChanged ? "logical" : "-")
	              << "/" << (transition.physicalChanged ? "physical" : "-")
	              << " generation=" << transition.generation;
}

} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
