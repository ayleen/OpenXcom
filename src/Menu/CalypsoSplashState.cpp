/*
 * Phase 6a.1 — Calypso HD splash screen.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoSplashState.h"
#include "../Engine/Game.h"
#include "../Engine/Surface.h"
#include "../Mod/Mod.h"

namespace OpenXcom
{

CalypsoSplashState::CalypsoSplashState() : _bg(nullptr), _frames(0)
{
	// getSurface returns null (error=false) when the sprite is missing —
	// e.g. when calypso-test-master is not the active master.
	_bg = _game->getMod()->getSurface("CALYPSO_SPLASH_HD", false);
	if (_bg)
	{
		// Mod owns _bg; push directly into the render list (_surfaces) without
		// going through add() / preAdd() — those mark the surface as owned and
		// ~State() would delete it, corrupting the Mod's surface registry on
		// any subsequent splash construction.
		_surfaces.push_back(_bg);
	}
}

CalypsoSplashState::~CalypsoSplashState()
{
	// _bg is referenced, not owned; ~State() deletes only _surfacesOwned.
}

void CalypsoSplashState::init()
{
	State::init();
	_frames = 0;
}

void CalypsoSplashState::think()
{
	State::think();
	// Hold for 90 game frames (~1.5 s at 60 fps).
	// Frame-based timing is deterministic in headless Playwright as well as
	// real browsers where RAF may run uncapped.
	if (++_frames >= 90)
		_game->popState();
}

} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */
