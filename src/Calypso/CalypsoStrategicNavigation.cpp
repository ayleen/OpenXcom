// F01 strategic navigation bridge (T15). Whole-file Emscripten guard.
#ifdef __EMSCRIPTEN__

#include "CalypsoStrategicNavigation.h"

#include "../Engine/Game.h"
#include "../Basescape/BasescapeState.h"
#include "../Geoscape/GeoscapeState.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

struct PendingRoute
{
	Game *game = nullptr;
	SavedGame *save = nullptr;
	CalypsoStrategicRoute route = CalypsoStrategicRoute::None;
};

PendingRoute g_pendingRoute;

} // namespace

void calypsoRequestStrategicRoute(Game *game, CalypsoStrategicRoute route)
{
	if (game == nullptr || route == CalypsoStrategicRoute::None)
	{
		return;
	}
	g_pendingRoute.game = game;
	g_pendingRoute.save = game->getSavedGame();
	g_pendingRoute.route = route;
}

void calypsoNavigateToWorld(Game *game)
{
	if (game == nullptr) return;
	while (game->getTopState() != nullptr
		&& dynamic_cast<GeoscapeState*>(game->getTopState()) == nullptr)
		game->popState();
}

void calypsoNavigateToBases(Game *game)
{
	if (game == nullptr) return;
	while (game->getTopState() != nullptr
		&& dynamic_cast<BasescapeState*>(game->getTopState()) == nullptr
		&& dynamic_cast<GeoscapeState*>(game->getTopState()) == nullptr)
		game->popState();
	if (dynamic_cast<BasescapeState*>(game->getTopState()) != nullptr)
		return;
	if (auto* geoscape = dynamic_cast<GeoscapeState*>(game->getTopState()))
		geoscape->btnBasesClick(nullptr);
}

void calypsoNavigateToStrategicRoute(Game *game, CalypsoStrategicRoute route)
{
	if (game == nullptr || route == CalypsoStrategicRoute::None) return;
	calypsoRequestStrategicRoute(game, route);
	calypsoNavigateToWorld(game);
}

bool calypsoPollStrategicRoute(GeoscapeState &state)
{
	Game *game = getCurrentGame();
	if (g_pendingRoute.route == CalypsoStrategicRoute::None
		|| g_pendingRoute.game == nullptr || g_pendingRoute.game != game)
	{
		return false;
	}
	if (g_pendingRoute.save != game->getSavedGame())
	{
		g_pendingRoute = PendingRoute{};
		return false;
	}
	const CalypsoStrategicRoute route = g_pendingRoute.route;
	g_pendingRoute = PendingRoute{};
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
	return true;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
