#ifdef __EMSCRIPTEN__
/*
 * Phase 41 (Calypso) -- transient radio-line toast popup implementation.
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 */

#include <utility>
#include <string>
#include <emscripten.h>

#include "CalypsoRadioLineState.h"
#include "../Engine/Game.h"  // Game::popState (State.h only forward-declares Game)

namespace OpenXcom
{

/// Auto-dismiss after this many think() ticks (~2s at the main-loop frame rate;
/// intentionally short for a radio blip -- tunable in a later commit if needed).
static const int RADIO_LINE_TICKS = 120;

CalypsoRadioLineState::CalypsoRadioLineState(std::string stringId)
	: _stringId(std::move(stringId))
{
	// _screen=false -> non-fullscreen modal (like PauseState / CalypsoTutorialState).
	// No surfaces are added; the popup is pure DOM overlay.
	_screen = false;
}

CalypsoRadioLineState::~CalypsoRadioLineState()
{
	// Hide the overlay in case the state was popped without think() finishing
	// (e.g. load-game, state-stack reset) -- mirrors CalypsoTutorialState's dtor.
	EM_ASM_({ if (globalThis.__calypsoRadioHide) globalThis.__calypsoRadioHide(); });
}

void CalypsoRadioLineState::init()
{
	State::init();
	std::string body = std::string(tr(_stringId));
	// Bug 4 fix (QA round 1): radio lines used to reuse the tutorial popup's
	// __calypsoTutorialShow channel, which a same-frame tutorial popup could
	// clobber (battle start fires both). Dedicated non-modal toast channel
	// now -- see web/public/radio-overlay.js.
	EM_ASM_(
	{
		if (globalThis.__calypsoRadioShow)
			globalThis.__calypsoRadioShow(UTF8ToString($0));
	},
		body.c_str());
}

void CalypsoRadioLineState::think()
{
	State::think();
	if (++_ticks >= RADIO_LINE_TICKS)
	{
		EM_ASM_({ if (globalThis.__calypsoRadioHide) globalThis.__calypsoRadioHide(); });
		_game->popState();
	}
}

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
