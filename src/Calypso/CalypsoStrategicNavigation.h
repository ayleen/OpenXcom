#pragma once
// F01 strategic navigation bridge (T15): deferred global routes from the
// base rail to live Geoscape handlers. A single pending slot owned by the
// session (at most one Geoscape exists): request records game+save+intent,
// the next Geoscape think consumes it BEFORE timers and ends think, so the
// frame adds no simulation step. No Action pointers are stored; a handler
// that refuses still clears the slot (no retry loop); a save swap drops it.
// Whole-file Emscripten guard (Phase 36).
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

bool calypsoPollStrategicRoute(GeoscapeState &state);

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
