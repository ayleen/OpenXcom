#pragma once
#include <cstdint>
#include <string>
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

#ifdef __EMSCRIPTEN__
void calypsoHdNavigationRecordFinalize(bool first);
#endif

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
	const bool first = !prepared;
	if (!first)
	{
#ifdef __EMSCRIPTEN__
		calypsoHdNavigationRecordFinalize(false);
#endif
		return false;
	}
	prepared = true;
	#ifdef __EMSCRIPTEN__
	calypsoHdNavigationRecordFinalize(true);
	#endif
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

/// The browser acceptance harness consumes this diagnostic record as an
/// observation of the real navigation boundary. It is separate from the
/// operations model so model flags cannot masquerade as lifecycle evidence.
struct CalypsoHdNavigationDiagnostics
{
	std::uint64_t generation = 0;
	std::uint64_t finalizeCalls = 0;
	std::uint64_t duplicateFinalizeCalls = 0;
	std::uint64_t nestedPops = 0;
	std::uint64_t nestedViewerPops = 0;
	std::uint64_t nestedErrorPops = 0;
	std::uint64_t routeRequests = 0;
	std::uint64_t duplicateRouteRequests = 0;
	std::uint64_t routeDispatches = 0;
	std::uint64_t routeDiscards = 0;
	std::uint64_t identityDiscards = 0;
	bool ownerFinalized = false;
	bool nestedOwnerUnwound = false;
	bool pendingRouteDiscarded = false;
	bool saveIdentityChanged = false;
	bool duplicateRequestRejected = false;
	std::string owner;
	std::string lastRoute;
};

/// Reset only the acceptance observation. This does not alter game state or
/// pending navigation; fixture setup calls it before creating an owner.
void calypsoHdNavigationReset(const char *owner = nullptr);
/// Fixture teardown/setup boundary: clear only an unconsumed route request.
/// This does not mutate the saved game or state stack.
void calypsoHdNavigationClearPendingRoute();
void calypsoHdNavigationSetOwnerHint(CalypsoHdExitOwner owner);
void calypsoHdNavigationRecordFinalize(bool first);
/// Record only a known nested owner (viewer/error), never an opaque host or
/// unrelated state popped while a harness is being torn down.
void calypsoHdNavigationRecordNestedPop(const char *kind = nullptr);
void calypsoHdNavigationRecordRouteRequest(CalypsoStrategicRoute route, bool duplicate);
void calypsoHdNavigationRecordRouteDispatch(CalypsoStrategicRoute route);
void calypsoHdNavigationRecordRouteDiscard(bool identityChanged);
const CalypsoHdNavigationDiagnostics& calypsoHdNavigationDiagnostics();
std::string calypsoHdNavigationSnapshotJson();

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
