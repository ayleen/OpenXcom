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
	EM_ASM_({ if (globalThis.__calypsoTutorialHide) globalThis.__calypsoTutorialHide(); });
}

void CalypsoRadioLineState::init()
{
	State::init();
	std::string body = std::string(tr(_stringId));
	// Reuse the tutorial overlay: single page, last step, no anchor, no disable
	// toggle. The overlay's Next/Got-it buttons route to the tutorial EMSCRIPTEN
	// exports (a no-op against this non-tutorial state); the toast self-dismisses
	// via think(), so those buttons being inert is harmless for this commit.
	EM_ASM_(
	{
		if (globalThis.__calypsoTutorialShow)
			globalThis.__calypsoTutorialShow(
				$0, $1,
				UTF8ToString($2), UTF8ToString($3),
				UTF8ToString($4), $5, $6,
				$7, $8, $9, $10, $11);
	},
		1, 1,            // pageNum, totalPages
		"",              // title -- a radio line has none
		body.c_str(),    // body = tr(stringId)
		"",              // nextLabel
		1,               // isLastStep
		0,               // disabled flag
		0,               // hasAnchor
		0, 0, 0, 0);     // anchor rect (unused)
}

void CalypsoRadioLineState::think()
{
	State::think();
	if (++_ticks >= RADIO_LINE_TICKS)
	{
		EM_ASM_({ if (globalThis.__calypsoTutorialHide) globalThis.__calypsoTutorialHide(); });
		_game->popState();
	}
}

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
