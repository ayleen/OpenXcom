/*
 * Phase 6a.1 — Calypso HD splash screen.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoSplashState.h"
#include "CalypsoHDUIState.h"
#include "../Engine/Game.h"
#include "../Engine/Surface.h"
#include "../Mod/Mod.h"

namespace OpenXcom
{

CalypsoSplashState::CalypsoSplashState() : _bg(nullptr), _frames(0)
{
	// getSurface returns null (error=false) when the sprite is missing —
	// e.g. when calypso-test-master is not the active master.
	Surface *modBg = _game->getMod()->getSurface("CALYPSO_SPLASH_HD", false);
	if (modBg)
	{
		// Deep-copy the Mod-owned surface so State::add() can take ownership
		// without corrupting the Mod's surface registry. The ARGB-aware copy
		// ctor (Surface(const Surface&)) handles 32 bpp via SDL_BlitSurface;
		// add() then runs the standard palette + initText setup on the copy
		// (no-op for HD ARGB — setPalette branches on BitsPerPixel == 8).
		_bg = new Surface(*modBg);
		add(_bg);
	}
}

CalypsoSplashState::~CalypsoSplashState()
{
	// _bg is owned via add() → _surfacesOwned and deleted by ~State().
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
	{
		// Pop the splash first so CalypsoHDUIState lands on top of the main menu.
		_game->popState();
		// If the HD UI demo panel is registered, push the 6a.2 ARGB UI demo.
		if (_game->getMod()->getSurface("CALYPSO_UI_PANEL_HD", false))
			_game->pushState(new CalypsoHDUIState());
	}
}

} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */
