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
#include "CalypsoViewportBarrier.h"

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

static Calypso::CalypsoVisualContext visualContextFor(
	Calypso::CalypsoViewportAffinity affinity)
{
	return affinity == Calypso::CalypsoViewportAffinity::Tactical
		? Calypso::CalypsoVisualContext::Tactical
		: Calypso::CalypsoVisualContext::Strategic;
}

static Calypso::CalypsoSafeInsets retainedSafeInsets(
	const Calypso::CalypsoLayoutMetrics& metrics)
{
	return Calypso::CalypsoSafeInsets{
		metrics.safeY,
		metrics.logicalWidth - metrics.safeX - metrics.safeWidth,
		metrics.logicalHeight - metrics.safeY - metrics.safeHeight,
		metrics.safeX};
}

Calypso::CalypsoViewportAffinity Game::calypsoViewportAffinity() const
{
	for (auto it = _states.rbegin(); it != _states.rend(); ++it)
	{
		const Calypso::CalypsoViewportAffinity affinity = (*it)->calypsoViewportAffinity();
		if (affinity != Calypso::CalypsoViewportAffinity::Inherit) return affinity;
	}
	return Calypso::CalypsoViewportAffinity::Strategic;
}

void Game::trackEmscriptenViewportState(State *state)
{
	const std::uint64_t generation = Calypso::calypsoViewportRuntime().generation();
	if (dynamic_cast<BattlescapeState *>(state))
	{
		const Calypso::CalypsoViewportRootSeed seed = Calypso::calypsoViewportRootSeed(
			Calypso::CalypsoViewportScene::Tactical,
			Options::baseXResolution, Options::baseYResolution,
			Options::baseXGeoscape, Options::baseYGeoscape);
		_calypsoViewportScenes.observeRoot(Calypso::CalypsoViewportScene::Tactical,
			state, seed.width, seed.height, generation);
	}
	else if (dynamic_cast<GeoscapeState *>(state))
	{
		const Calypso::CalypsoViewportRootSeed seed = Calypso::calypsoViewportRootSeed(
			Calypso::CalypsoViewportScene::Strategic,
			Options::baseXResolution, Options::baseYResolution,
			Options::baseXGeoscape, Options::baseYGeoscape);
		_calypsoViewportScenes.observeRoot(Calypso::CalypsoViewportScene::Strategic,
			state, seed.width, seed.height, generation);
	}
}

void Game::calypsoNotifyViewportRootApplied(State *state)
{
	if (!dynamic_cast<BattlescapeState *>(state)) return;
	const std::uint64_t generation = Calypso::calypsoViewportRuntime().generation();
	if (!_calypsoViewportScenes.acceptOutOfBandApplied(
		Calypso::CalypsoViewportScene::Tactical, state,
		Options::baseXResolution, Options::baseYResolution, generation))
	{
		Log(LOG_WARNING) << "[ui-resize] ignored out-of-band geometry from untracked root";
	}
}

/**
 * Synchronize a stack-only strategic/tactical context transition. Browser
 * geometry can remain byte-for-byte identical while a hidden persistent root
 * has missed one or more viewport generations. The sole caller is the final
 * pre-init barrier, after any queued physical transaction has been consumed.
 */
void Game::syncEmscriptenViewportContext()
{
	if (_states.empty()) return;
	Calypso::CalypsoViewportRuntime& runtime = Calypso::calypsoViewportRuntime();
	if (!runtime.hasLayout() || !runtime.hasPhysicalSize()) return;

	const Calypso::CalypsoViewportAffinity affinity = calypsoViewportAffinity();
	const Calypso::CalypsoLayoutMetrics previousMetrics = runtime.current();
	const Calypso::CalypsoVisualContext context = visualContextFor(affinity);
	const bool contextChanged = previousMetrics.visualContext != context;
	if (contextChanged)
	{
		runtime.update(previousMetrics.logicalWidth, previousMetrics.logicalHeight,
			runtime.physicalWidth(), runtime.physicalHeight(),
			retainedSafeInsets(previousMetrics), context);
	}
	const std::uint64_t generation = runtime.generation();

	BattlescapeState *battleRoot = nullptr;
	GeoscapeState *geoRoot = nullptr;
	State *visibleBoundary = nullptr;
	for (State *state : _states)
	{
		if (auto *battle = dynamic_cast<BattlescapeState *>(state)) battleRoot = battle;
		if (auto *geo = dynamic_cast<GeoscapeState *>(state)) geoRoot = geo;
	}
	for (auto it = _states.rbegin(); it != _states.rend(); ++it)
	{
		if ((*it)->calypsoViewportAffinity() != Calypso::CalypsoViewportAffinity::Inherit)
		{
			visibleBoundary = *it;
			break;
		}
	}

	const int oldBaseWidth = Options::baseXResolution;
	const int oldBaseHeight = Options::baseYResolution;
	const int oldGeoscapeWidth = Options::baseXGeoscape;
	const int oldGeoscapeHeight = Options::baseYGeoscape;
	const int oldBattlescapeWidth = Options::baseXBattlescape;
	const int oldBattlescapeHeight = Options::baseYBattlescape;

	Screen::normalizeBrowserScales();
	int geoscapeWidth = oldGeoscapeWidth;
	int geoscapeHeight = oldGeoscapeHeight;
	int battlescapeWidth = oldBattlescapeWidth;
	int battlescapeHeight = oldBattlescapeHeight;
	Screen::updateScale(Options::geoscapeScale, geoscapeWidth, geoscapeHeight, false);
	Screen::updateScale(Options::battlescapeScale, battlescapeWidth, battlescapeHeight, false);
	Options::baseXGeoscape = geoscapeWidth;
	Options::baseYGeoscape = geoscapeHeight;
	Options::baseXBattlescape = battlescapeWidth;
	Options::baseYBattlescape = battlescapeHeight;

	_calypsoViewportScenes.observeRoot(Calypso::CalypsoViewportScene::Strategic,
		geoRoot, geoRoot && visibleBoundary == geoRoot ? oldBaseWidth : oldGeoscapeWidth,
		geoRoot && visibleBoundary == geoRoot ? oldBaseHeight : oldGeoscapeHeight,
		generation);
	_calypsoViewportScenes.observeRoot(Calypso::CalypsoViewportScene::Tactical,
		battleRoot, battleRoot && visibleBoundary == battleRoot ? oldBaseWidth : oldBattlescapeWidth,
		battleRoot && visibleBoundary == battleRoot ? oldBaseHeight : oldBattlescapeHeight,
		generation);
	_calypsoViewportScenes.setDesired(Calypso::CalypsoViewportScene::Strategic,
		geoscapeWidth, geoscapeHeight, generation);
	_calypsoViewportScenes.setDesired(Calypso::CalypsoViewportScene::Tactical,
		battlescapeWidth, battlescapeHeight, generation);

	const Calypso::CalypsoViewportOwner owner = Calypso::calypsoViewportOwner(
		affinity, visibleBoundary == battleRoot, visibleBoundary == geoRoot,
		_save && _save->getSavedBattle() != nullptr);
	const bool tactical = owner == Calypso::CalypsoViewportOwner::TacticalRoot;
	State *root = tactical ? static_cast<State *>(battleRoot)
		: (owner == Calypso::CalypsoViewportOwner::StrategicRoot
			? static_cast<State *>(geoRoot) : nullptr);
	const Calypso::CalypsoViewportScene scene = tactical
		? Calypso::CalypsoViewportScene::Tactical
		: Calypso::CalypsoViewportScene::Strategic;
	const Calypso::CalypsoViewportGeometry desired = _calypsoViewportScenes.desired(scene);
	const bool rootNeedsCatchUp = root && _calypsoViewportScenes.needsCatchUp(scene);
	const bool rootlessNeedsResize = !root && desired.valid
		&& (oldBaseWidth != desired.width || oldBaseHeight != desired.height);
	if (!contextChanged && !rootNeedsCatchUp && !rootlessNeedsResize) return;

	Calypso::s_activeReflowMetrics = &runtime.current();
	int sourceBaseWidth = oldBaseWidth;
	int sourceBaseHeight = oldBaseHeight;
	int dX = 0;
	int dY = 0;
	if (root)
	{
		const Calypso::CalypsoViewportGeometry applied = _calypsoViewportScenes.applied(scene);
		if (applied.valid)
		{
			sourceBaseWidth = applied.width;
			sourceBaseHeight = applied.height;
			Options::baseXResolution = sourceBaseWidth;
			Options::baseYResolution = sourceBaseHeight;
		}
		root->resize(dX, dY);
		if (desired.valid && Options::baseXResolution == desired.width
		    && Options::baseYResolution == desired.height)
		{
			_calypsoViewportScenes.markApplied(scene, Options::baseXResolution,
				Options::baseYResolution, generation);
		}
		else
		{
			Log(LOG_WARNING) << "[ui-resize] scene catch-up did not reach desired base "
			                 << desired.width << "x" << desired.height << "; remains pending";
		}
	}
	else if (desired.valid)
	{
		dX = desired.width - sourceBaseWidth;
		dY = desired.height - sourceBaseHeight;
	}

	const int targetBaseWidth = sourceBaseWidth + dX;
	const int targetBaseHeight = sourceBaseHeight + dY;
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
		if (!visibleSegment || state == root) continue;
		int stateDX = dX;
		int stateDY = dY;
		state->resize(stateDX, stateDY);
	}
	if (Options::baseXResolution != oldBaseWidth
	    || Options::baseYResolution != oldBaseHeight || contextChanged)
		_screen->resetDisplay(false, false);
	if (g_calypsoFocusedTextEdit)
		g_calypsoFocusedTextEdit->refreshExternalGeometry();
	Calypso::s_activeReflowMetrics = nullptr;
	Log(LOG_INFO) << "[ui-resize] synchronous context/catch-up base="
	              << oldBaseWidth << "x" << oldBaseHeight << "->"
	              << Options::baseXResolution << "x" << Options::baseYResolution
	              << " generation=" << generation;
}

void Game::initializeEmscriptenTopState()
{
	int pendingWidth = 0;
	int pendingHeight = 0;
	const bool hasPending = Calypso::calypsoPendingViewportSize(pendingWidth, pendingHeight);
	Calypso::calypsoRunPreInitViewportBarrier(hasPending,
		[this, pendingWidth, pendingHeight]()
		{
			reflowEmscriptenViewport(pendingWidth, pendingHeight);
		},
		[this]()
		{
			syncEmscriptenViewportContext();
		},
		[this]()
		{
			_states.back()->init();
		});
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
	const int oldGeoscapeWidth = Options::baseXGeoscape;
	const int oldGeoscapeHeight = Options::baseYGeoscape;
	const int oldBattlescapeWidth = Options::baseXBattlescape;
	const int oldBattlescapeHeight = Options::baseYBattlescape;
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

	// Root identities are normally captured at push time, before any desired
	// scene dimensions can overwrite the active base. The observations here
	// also clear pointers for roots removed through a multi-state transition.
	_calypsoViewportScenes.observeRoot(Calypso::CalypsoViewportScene::Strategic,
		geoRoot, geoRoot && visibleBoundary == geoRoot ? oldBaseWidth : oldGeoscapeWidth,
		geoRoot && visibleBoundary == geoRoot ? oldBaseHeight : oldGeoscapeHeight,
		transition.previousGeneration);
	_calypsoViewportScenes.observeRoot(Calypso::CalypsoViewportScene::Tactical,
		battleRoot, battleRoot && visibleBoundary == battleRoot ? oldBaseWidth : oldBattlescapeWidth,
		battleRoot && visibleBoundary == battleRoot ? oldBaseHeight : oldBattlescapeHeight,
		transition.previousGeneration);
	_calypsoViewportScenes.setDesired(Calypso::CalypsoViewportScene::Strategic,
		geoscapeWidth, geoscapeHeight, transition.generation);
	_calypsoViewportScenes.setDesired(Calypso::CalypsoViewportScene::Tactical,
		battlescapeWidth, battlescapeHeight, transition.generation);

	int dX = 0;
	int dY = 0;
	int sourceBaseWidth = oldBaseWidth;
	int sourceBaseHeight = oldBaseHeight;
	if (root)
	{
		const Calypso::CalypsoViewportScene scene = tactical
			? Calypso::CalypsoViewportScene::Tactical
			: Calypso::CalypsoViewportScene::Strategic;
		const Calypso::CalypsoViewportGeometry applied = _calypsoViewportScenes.applied(scene);
		if (applied.valid)
		{
			sourceBaseWidth = applied.width;
			sourceBaseHeight = applied.height;
			Options::baseXResolution = sourceBaseWidth;
			Options::baseYResolution = sourceBaseHeight;
		}
		root->resize(dX, dY);
		const Calypso::CalypsoViewportGeometry desired =
			_calypsoViewportScenes.desired(scene);
		if (desired.valid && Options::baseXResolution == desired.width
		    && Options::baseYResolution == desired.height)
		{
			_calypsoViewportScenes.markApplied(scene, Options::baseXResolution,
				Options::baseYResolution, transition.generation);
		}
		else
		{
			Log(LOG_WARNING) << "[ui-resize] root resize did not reach desired base "
			                 << desired.width << "x" << desired.height << "; remains pending";
		}
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

	const int targetBaseWidth = sourceBaseWidth + dX;
	const int targetBaseHeight = sourceBaseHeight + dY;
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
