#pragma once
// F01 strategic navigation bridge (T15): deferred global routes from the
// base rail to live Geoscape handlers. A single pending slot owned by the
// session (at most one Geoscape exists): request records game+save+intent,
// the next Geoscape think consumes it BEFORE timers and ends think, so the
// frame adds no simulation step. No Action pointers are stored; a handler
// that refuses still clears the slot (no retry loop); a save swap drops it.
// Whole-file Emscripten guard (Phase 36).

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoHdExitOwner
{
	None,
	NewResearch,
	ExistingResearch,
	NewProduction,
	ExistingProduction
};

struct CalypsoHdExitPlan
{
	bool cancelResearchPreview = false;
	bool removeUnstartedProduction = false;
	bool applyDeferredProductionOptions = false;
};

inline CalypsoHdExitPlan calypsoHdExitPlan(CalypsoHdExitOwner owner)
{
	switch (owner)
	{
	case CalypsoHdExitOwner::NewResearch:
		return {true, false, false};
	case CalypsoHdExitOwner::NewProduction:
		return {false, true, false};
	case CalypsoHdExitOwner::ExistingProduction:
		return {false, false, true};
	case CalypsoHdExitOwner::ExistingResearch:
	case CalypsoHdExitOwner::None:
	default:
		return {};
	}
}

/// Atomically claims the one allowed HD strategic-exit finalization.
inline bool calypsoHdExitOnce(bool &prepared)
{
	if (prepared) return false;
	prepared = true;
	return true;
}

} // namespace Calypso
} // namespace OpenXcom

#ifdef __EMSCRIPTEN__

namespace OpenXcom
{
class Game;
class GeoscapeState;
namespace Calypso
{

enum class CalypsoStrategicRoute
{
	None,
	Intercept,
	Graphs,
	Archive,
	Options
};

void calypsoRequestStrategicRoute(Game *game, CalypsoStrategicRoute route);
void calypsoNavigateToWorld(Game *game);
void calypsoNavigateToBases(Game *game);
void calypsoNavigateToStrategicRoute(Game *game, CalypsoStrategicRoute route);
/// Finalizes the current research/manufacture owner without changing stack depth.
/// This is intentionally Emscripten-only and never pushes or pops a state.
void prepareHdStrategicExit(Game *game);

bool calypsoPollStrategicRoute(GeoscapeState &state);

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
