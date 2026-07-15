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
#include "../Interface/TextEdit.h"
#include "CalypsoViewportRuntime.h"
#include "CalypsoViewportOwner.h"

namespace OpenXcom
{

namespace Calypso
{
static const CalypsoLayoutMetrics *s_activeReflowMetrics = nullptr;

bool calypsoProjectedSafeRectForLayout(int baseWidth, int baseHeight,
	                                   CalypsoBaseSafeRect& out)
{
	const CalypsoLayoutMetrics *metrics = s_activeReflowMetrics;
	if (!metrics)
	{
		CalypsoViewportRuntime& runtime = calypsoViewportRuntime();
		if (!runtime.hasLayout()) return false;
		metrics = &runtime.current();
	}
	out = calypsoProjectSafeRect(*metrics, baseWidth, baseHeight);
	return true;
}
} // namespace Calypso

extern TextEdit *g_calypsoFocusedTextEdit;

Calypso::CalypsoViewportAffinity Game::calypsoViewportAffinity() const
{
	std::vector<Calypso::CalypsoViewportAffinity> topDown;
	topDown.reserve(_states.size());
	for (auto it = _states.rbegin(); it != _states.rend(); ++it)
		topDown.push_back((*it)->calypsoViewportAffinity());
	return Calypso::calypsoResolveViewportAffinity(topDown);
}

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
	Calypso::s_activeReflowMetrics = &transition.metrics;

	const int oldPhysicalWidth = Options::displayWidth;
	const int oldPhysicalHeight = Options::displayHeight;

	const int oldBaseWidth = Options::baseXResolution;
	const int oldBaseHeight = Options::baseYResolution;
	Options::newDisplayWidth = Options::displayWidth =
		std::max(Screen::ORIGINAL_WIDTH, physicalWidth);
	Options::newDisplayHeight = Options::displayHeight =
		std::max(Screen::ORIGINAL_HEIGHT, physicalHeight);
	Screen::normalizeBrowserScales();

	int geoscapeWidth = Options::baseXGeoscape;
	int geoscapeHeight = Options::baseYGeoscape;
	int battlescapeWidth = Options::baseXBattlescape;
	int battlescapeHeight = Options::baseYBattlescape;
	Screen::updateScale(Options::geoscapeScale, geoscapeWidth, geoscapeHeight, false);
	Screen::updateScale(Options::battlescapeScale, battlescapeWidth, battlescapeHeight, false);
	Options::baseXGeoscape = geoscapeWidth;
	Options::baseYGeoscape = geoscapeHeight;
	Options::baseXBattlescape = battlescapeWidth;
	Options::baseYBattlescape = battlescapeHeight;

	BattlescapeState *battleRoot = nullptr;
	GeoscapeState *geoRoot = nullptr;
	State *visibleBoundary = nullptr;
	for (State *state : _states)
	{
		if (auto *battle = dynamic_cast<BattlescapeState *>(state)) battleRoot = battle;
		if (auto *geo = dynamic_cast<GeoscapeState *>(state)) geoRoot = geo;
	}
	const Calypso::CalypsoViewportAffinity affinity = calypsoViewportAffinity();
	for (auto it = _states.rbegin(); it != _states.rend(); ++it)
	{
		if ((*it)->calypsoViewportAffinity() != Calypso::CalypsoViewportAffinity::Inherit)
		{
			visibleBoundary = *it;
			break;
		}
	}
	const Calypso::CalypsoViewportOwner owner = Calypso::calypsoViewportOwner(
		affinity, visibleBoundary == battleRoot, visibleBoundary == geoRoot,
		_save && _save->getSavedBattle() != nullptr);
	const bool tactical = owner == Calypso::CalypsoViewportOwner::TacticalRoot;
	State *root = owner == Calypso::CalypsoViewportOwner::TacticalRoot
		? static_cast<State *>(battleRoot)
		: (owner == Calypso::CalypsoViewportOwner::StrategicRoot
			? static_cast<State *>(geoRoot) : nullptr);

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
	// Rootless stacks do not have a scene resize override to commit the active
	// base before their overlays run. Publish it now so applyUiScaling projects
	// the immutable safe rect against the new framebuffer, not the stale one.
	Options::baseXResolution = targetBaseWidth;
	Options::baseYResolution = targetBaseHeight;
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

	bool visibleSegment = visibleBoundary == nullptr;
	for (State *state : _states)
	{
		if (state == visibleBoundary) visibleSegment = true;
		if (!visibleSegment) continue;
		if (state == root) continue;
		int stateDX = dX;
		int stateDY = dY;
		state->resize(stateDX, stateDY);
	}
	const bool displayChanged = Options::displayWidth != oldPhysicalWidth
	                         || Options::displayHeight != oldPhysicalHeight;
	const bool baseChanged = Options::baseXResolution != oldBaseWidth
	                      || Options::baseYResolution != oldBaseHeight;
	if (displayChanged || baseChanged)
		_screen->resetDisplay(false, false);
	if (g_calypsoFocusedTextEdit)
		g_calypsoFocusedTextEdit->refreshExternalGeometry();
	Calypso::s_activeReflowMetrics = nullptr;
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
