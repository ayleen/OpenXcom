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

CalypsoSplashState::CalypsoSplashState() : _bg(nullptr), _shownAt(0)
{
	// getSurface returns null (error=false) when the sprite is missing —
	// e.g. when calypso-test-master is not the active master.
	_bg = _game->getMod()->getSurface("CALYPSO_SPLASH_HD", false);
	if (_bg)
		add(_bg);
}

CalypsoSplashState::~CalypsoSplashState()
{
	// _bg is owned by the Mod; do not delete it here.
}

void CalypsoSplashState::init()
{
	State::init();
	_shownAt = SDL_GetTicks();
}

void CalypsoSplashState::think()
{
	State::think();
	// Hold for 1.5 s, then yield to GoToMainMenuState underneath.
	if (SDL_GetTicks() - _shownAt > 1500)
		_game->popState();
}

} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */
