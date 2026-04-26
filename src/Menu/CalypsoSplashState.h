#pragma once
/*
 * Phase 6a.1 — Calypso HD splash screen.
 *
 * Shown after mod loading completes (pushed on top of GoToMainMenuState).
 * Displays the CALYPSO_SPLASH_HD extraSprite (hd:true, 640×400) so that
 * the HD overlay path (Surface::blit → HDQueue::push → HDQueue::flush)
 * is exercised by the regression harness.
 *
 * Emscripten-only: native builds never see this state.
 */
#ifdef __EMSCRIPTEN__

#include "../Engine/State.h"

namespace OpenXcom
{

class Surface;

class CalypsoSplashState : public State
{
private:
	Surface *_bg;
	int      _frames;

public:
	CalypsoSplashState();
	~CalypsoSplashState();
	void init()  override;
	void think() override;
};

} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */
