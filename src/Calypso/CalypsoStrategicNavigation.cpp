// F01 strategic navigation bridge (T15). Whole-file Emscripten guard.
#ifdef __EMSCRIPTEN__

#include "CalypsoStrategicNavigation.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdRdNavigationFixture.h"

#include "../Engine/Game.h"
#include "../Basescape/BasescapeState.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Basescape/ResearchInfoState.h"
#include "../Basescape/ManufactureInfoState.h"
#include "../Basescape/TechTreeViewerState.h"
#include "../Menu/ErrorMessageState.h"

#include <sstream>

namespace OpenXcom
{
namespace Calypso
{

namespace
{

CalypsoHdNavigationDiagnostics g_navigationDiagnostics;
CalypsoHdExitOwner g_navigationOwnerHint = CalypsoHdExitOwner::None;
std::uint64_t g_navigationGeneration = 0;

const char *routeName(CalypsoStrategicRoute route)
{
	switch (route)
	{
	case CalypsoStrategicRoute::Intercept: return "operations";
	case CalypsoStrategicRoute::Graphs: return "analytics";
	case CalypsoStrategicRoute::Archive: return "archive";
	case CalypsoStrategicRoute::Options: return "settings";
	case CalypsoStrategicRoute::None: default: return "none";
	}
}

const char *ownerHintName(CalypsoHdExitOwner owner)
{
	switch (owner)
	{
	case CalypsoHdExitOwner::NewResearch:
	case CalypsoHdExitOwner::ExistingResearch: return "research";
	case CalypsoHdExitOwner::NewProduction:
	case CalypsoHdExitOwner::ExistingProduction: return "production";
	case CalypsoHdExitOwner::None: default: return "";
	}
}

struct PendingRoute
{
	Game *game = nullptr;
	SavedGame *save = nullptr;
	CalypsoStrategicRoute route = CalypsoStrategicRoute::None;
	std::uint64_t generation = 0;
};

PendingRoute g_pendingRoute;

bool atOpaqueHarnessHost(Game *game)
{
	return game != nullptr
		&& calypsoHarnessHostUp(calypsoHarnessSession())
		&& dynamic_cast<CalypsoHdHarnessHostState*>(game->getTopState()) != nullptr;
}

void captureNavigationTargetBeforePop(Game *game)
{
	if (!game) return;
	CalypsoHarnessSession& session = calypsoHarnessSession();
	if (session.navigationDestinationActive && session.targetUp
		&& game->getTopState() == session.activeTarget)
		calypsoHdHarnessClose();
}

} // namespace

void calypsoHdNavigationReset(const char *owner)
{
	g_navigationDiagnostics = CalypsoHdNavigationDiagnostics{};
	g_navigationDiagnostics.generation = ++g_navigationGeneration;
	if (owner) g_navigationDiagnostics.owner = owner;
	g_navigationOwnerHint = CalypsoHdExitOwner::None;
}

void calypsoHdNavigationClearPendingRoute()
{
	g_pendingRoute = PendingRoute{};
}

void calypsoHdNavigationSetOwnerHint(CalypsoHdExitOwner owner)
{
	g_navigationOwnerHint = owner;
	if (g_navigationDiagnostics.owner.empty())
		g_navigationDiagnostics.owner = ownerHintName(owner);
}

void calypsoHdNavigationRecordFinalize(bool first)
{
	++g_navigationDiagnostics.finalizeCalls;
	if (first)
	{
		g_navigationDiagnostics.ownerFinalized = true;
		if (g_navigationDiagnostics.owner.empty())
			g_navigationDiagnostics.owner = ownerHintName(g_navigationOwnerHint);
	}
	else
	{
		++g_navigationDiagnostics.duplicateFinalizeCalls;
	}
}

void calypsoHdNavigationRecordNestedPop(const char *kind)
{
	++g_navigationDiagnostics.nestedPops;
	g_navigationDiagnostics.nestedOwnerUnwound = true;
	if (kind && std::string(kind) == "viewer")
		++g_navigationDiagnostics.nestedViewerPops;
	else if (kind && std::string(kind) == "error")
		++g_navigationDiagnostics.nestedErrorPops;
}

void calypsoHdNavigationRecordRouteRequest(CalypsoStrategicRoute route, bool duplicate)
{
	++g_navigationDiagnostics.routeRequests;
	g_navigationDiagnostics.lastRoute = routeName(route);
	if (duplicate)
	{
		++g_navigationDiagnostics.duplicateRouteRequests;
		g_navigationDiagnostics.duplicateRequestRejected = true;
	}
}

void calypsoHdNavigationRecordRouteDispatch(CalypsoStrategicRoute route)
{
	++g_navigationDiagnostics.routeDispatches;
	g_navigationDiagnostics.lastRoute = routeName(route);
}

void calypsoHdNavigationRecordRouteDiscard(bool identityChanged)
{
	++g_navigationDiagnostics.routeDiscards;
	g_navigationDiagnostics.pendingRouteDiscarded = true;
	if (identityChanged)
	{
		++g_navigationDiagnostics.identityDiscards;
		g_navigationDiagnostics.saveIdentityChanged = true;
	}
}

const CalypsoHdNavigationDiagnostics& calypsoHdNavigationDiagnostics()
{
	return g_navigationDiagnostics;
}

std::string calypsoHdNavigationSnapshotJson()
{
	const auto &d = g_navigationDiagnostics;
	std::ostringstream out;
	out << "{\"generation\":" << d.generation
		<< ",\"finalizeCount\":" << d.finalizeCalls
		<< ",\"duplicateFinalizeCount\":" << d.duplicateFinalizeCalls
		<< ",\"acceptedFinalizeCount\":"
		<< (d.finalizeCalls >= d.duplicateFinalizeCalls
			? d.finalizeCalls - d.duplicateFinalizeCalls : 0)
		<< ",\"nestedPops\":" << d.nestedPops
		<< ",\"nestedViewerPops\":" << d.nestedViewerPops
		<< ",\"nestedErrorPops\":" << d.nestedErrorPops
		<< ",\"routeRequests\":" << d.routeRequests
		<< ",\"duplicateRouteRequests\":" << d.duplicateRouteRequests
		<< ",\"routeDispatches\":" << d.routeDispatches
		<< ",\"routeDiscards\":" << d.routeDiscards
		<< ",\"identityDiscards\":" << d.identityDiscards
		<< ",\"ownerFinalized\":" << (d.ownerFinalized ? "true" : "false")
		<< ",\"nestedOwnerUnwound\":" << (d.nestedOwnerUnwound ? "true" : "false")
		<< ",\"pendingRouteDiscarded\":" << (d.pendingRouteDiscarded ? "true" : "false")
		<< ",\"saveIdentityChanged\":" << (d.saveIdentityChanged ? "true" : "false")
		<< ",\"duplicateRequestRejected\":" << (d.duplicateRequestRejected ? "true" : "false")
		<< ",\"owner\":\"" << d.owner << "\",\"lastRoute\":\"" << d.lastRoute << "\"}";
	return out.str();
}

void calypsoRequestStrategicRoute(Game *game, CalypsoStrategicRoute route)
{
	if (game == nullptr || route == CalypsoStrategicRoute::None)
	{
		return;
	}
	const bool duplicate = g_pendingRoute.route != CalypsoStrategicRoute::None;
	calypsoHdNavigationRecordRouteRequest(route, duplicate);
	if (duplicate)
	{
		return;
	}
	g_pendingRoute.game = game;
	g_pendingRoute.save = game->getSavedGame();
	g_pendingRoute.route = route;
	g_pendingRoute.generation = ++g_navigationGeneration;
	g_navigationDiagnostics.generation = g_pendingRoute.generation;
}

void prepareHdStrategicExit(Game *game)
{
	if (game == nullptr)
	{
		return;
	}
	State *top = game->getTopState();
	if (auto* research = dynamic_cast<ResearchInfoState*>(top))
	{
		calypsoHdNavigationSetOwnerHint(CalypsoHdExitOwner::ExistingResearch);
		research->prepareHdStrategicExit();
	}
	else if (auto* manufacture = dynamic_cast<ManufactureInfoState*>(top))
	{
		calypsoHdNavigationSetOwnerHint(CalypsoHdExitOwner::ExistingProduction);
		manufacture->prepareHdStrategicExit();
	}
}

void calypsoNavigateToWorld(Game *game)
{
	if (game == nullptr) return;
	while (game->getTopState() != nullptr
		&& !atOpaqueHarnessHost(game)
		&& dynamic_cast<GeoscapeState*>(game->getTopState()) == nullptr)
	{
		if (dynamic_cast<TechTreeViewerState*>(game->getTopState()))
			calypsoHdNavigationRecordNestedPop("viewer");
		else if (dynamic_cast<ErrorMessageState*>(game->getTopState()))
			calypsoHdNavigationRecordNestedPop("error");
		prepareHdStrategicExit(game);
		const auto *navSpec = calypsoHdRdNavigationProfileSpec(
			calypsoHarnessSession().rdActiveProfile);
		if (navSpec && navSpec->duplicatesBoundary
			&& (dynamic_cast<ResearchInfoState*>(game->getTopState())
				|| dynamic_cast<ManufactureInfoState*>(game->getTopState())))
			prepareHdStrategicExit(game); // real second owner finalizer attempt
		captureNavigationTargetBeforePop(game);
		game->popState();
	}
	// The fixture installs an initial destination only to build the real stack.
	// Shared owner actions can unwind through that stack and leave a different
	// real strategic state on top (for example Basescape -> Geoscape). Register
	// the state that actually survived the unwind; comparing only with the
	// initial pointer would reject a valid destination after a real pop.
	if (calypsoHarnessSession().navigationDestinationActive
		&& dynamic_cast<GeoscapeState*>(game->getTopState()) != nullptr)
	{
		calypsoHarnessNavigationDestination(
			calypsoHarnessSession(), game->getTopState());
	}
}

void calypsoNavigateToBases(Game *game)
{
	if (game == nullptr) return;
	while (game->getTopState() != nullptr
		&& !atOpaqueHarnessHost(game)
		&& dynamic_cast<BasescapeState*>(game->getTopState()) == nullptr
		&& dynamic_cast<GeoscapeState*>(game->getTopState()) == nullptr)
	{
		if (dynamic_cast<TechTreeViewerState*>(game->getTopState()))
			calypsoHdNavigationRecordNestedPop("viewer");
		else if (dynamic_cast<ErrorMessageState*>(game->getTopState()))
			calypsoHdNavigationRecordNestedPop("error");
		prepareHdStrategicExit(game);
		captureNavigationTargetBeforePop(game);
		game->popState();
	}
	if (dynamic_cast<BasescapeState*>(game->getTopState()) != nullptr)
	{
		if (calypsoHarnessSession().navigationDestinationActive)
			calypsoHarnessNavigationDestination(
				calypsoHarnessSession(), game->getTopState());
		return;
	}
	if (auto* geoscape = dynamic_cast<GeoscapeState*>(game->getTopState()))
		geoscape->btnBasesClick(nullptr);
	if (calypsoHarnessSession().navigationDestinationActive
		&& (dynamic_cast<BasescapeState*>(game->getTopState()) != nullptr
			|| dynamic_cast<GeoscapeState*>(game->getTopState()) != nullptr))
	{
		calypsoHarnessNavigationDestination(
			calypsoHarnessSession(), game->getTopState());
	}
}

void calypsoNavigateToStrategicRoute(Game *game, CalypsoStrategicRoute route)
{
	if (game == nullptr || route == CalypsoStrategicRoute::None) return;
	calypsoRequestStrategicRoute(game, route);
	const auto *navSpec = calypsoHdRdNavigationProfileSpec(
		calypsoHarnessSession().rdActiveProfile);
	if (navSpec && navSpec->duplicatesBoundary)
		calypsoRequestStrategicRoute(game, route); // real duplicate pending request
	calypsoNavigateToWorld(game);
}

bool calypsoPollStrategicRoute(GeoscapeState &state)
{
	Game *game = getCurrentGame();
	if (g_pendingRoute.route == CalypsoStrategicRoute::None
		|| g_pendingRoute.game == nullptr)
	{
		return false;
	}
	if (g_pendingRoute.game != game)
	{
		calypsoHdNavigationRecordRouteDiscard(false);
		g_pendingRoute = PendingRoute{};
		return false;
	}
	const auto *navSpec = calypsoHdRdNavigationProfileSpec(
		calypsoHarnessSession().rdActiveProfile);
	if (navSpec && navSpec->replacesSaveIdentity
		&& calypsoHdHarnessReplaceSavedGameForNavigation())
	{
		// The fixture intentionally changes the live SavedGame identity only
		// after the target has been popped and before this real poll.
	}
	if (g_pendingRoute.save != game->getSavedGame())
	{
		calypsoHdNavigationRecordRouteDiscard(true);
		g_pendingRoute = PendingRoute{};
		return false;
	}
	const CalypsoStrategicRoute route = g_pendingRoute.route;
	g_pendingRoute = PendingRoute{};
	calypsoHdNavigationRecordRouteDispatch(route);
	switch (route)
	{
	case CalypsoStrategicRoute::Intercept:
		state.btnInterceptClick(nullptr);
		break;
	case CalypsoStrategicRoute::Graphs:
		state.btnGraphsClick(nullptr);
		break;
	case CalypsoStrategicRoute::Archive:
		state.btnUfopaediaClick(nullptr);
		break;
	case CalypsoStrategicRoute::Options:
		state.btnOptionsClick(nullptr);
		break;
	default:
		return false;
	}
	// Dispatch may push a new native destination (Intercept, Ufopaedia,
	// Options, or a route-specific popup). Register the actual resulting top
	// state rather than retaining the pre-dispatch Geoscape pointer. The host
	// itself is never a destination: accepting it here would make a closed
	// route look like a successful navigation.
	State *destination = game->getTopState();
	if (calypsoHarnessSession().navigationDestinationActive
		&& destination != nullptr
		&& dynamic_cast<CalypsoHdHarnessHostState*>(destination) == nullptr)
	{
		calypsoHarnessNavigationDestination(
			calypsoHarnessSession(), destination);
	}
	return true;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
