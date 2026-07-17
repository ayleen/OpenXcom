#ifdef __EMSCRIPTEN__
/*
 * Phase 41 (Calypso) -- transient radio-line toast popup implementation.
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 */

#include <utility>
#include <string>
#include <emscripten.h>

#include "CalypsoRadioLineState.h"
#include "CalypsoRadioLineTiming.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"  // Game::popState (State.h only forward-declares Game)

namespace OpenXcom
{

CalypsoRadioLineState::CalypsoRadioLineState(std::string stringId, CalypsoRadioLineKind kind)
	: _stringId(std::move(stringId)), _kind(kind)
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
			globalThis.__calypsoRadioShow(UTF8ToString($0), $1);
	},
		body.c_str(), _kind == CalypsoRadioLineKind::Instruction ? 1 : 0);
	_shownAt = SDL_GetTicks();
	_durationMs = Calypso::radioNarrativeDurationMs(body);
}

void CalypsoRadioLineState::think()
{
	State::think();
	if (_kind == CalypsoRadioLineKind::Narrative && SDL_GetTicks() - _shownAt >= _durationMs)
		dismiss();
}

void CalypsoRadioLineState::handle(Action *action)
{
	if (action && action->getDetails()->type == SDL_KEYDOWN)
	{
		const SDL_Keycode key = action->getDetails()->key.keysym.sym;
		if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE || key == SDLK_ESCAPE)
		{
			dismiss();
			return; // do not leak an intentional dismissal into Battlescape
		}
	}
	State::handle(action);
}

void CalypsoRadioLineState::dismiss()
{
	EM_ASM_({ if (globalThis.__calypsoRadioHide) globalThis.__calypsoRadioHide(); });
	_game->popState();
}

} // namespace OpenXcom

extern "C" {

EMSCRIPTEN_KEEPALIVE
int calypso_radio_dismiss()
{
	OpenXcom::Game *game = OpenXcom::getCurrentGame();
	if (!game) return 0;
	auto *state = dynamic_cast<OpenXcom::CalypsoRadioLineState *>(game->getTopState());
	if (!state) return 0;
	state->dismiss();
	return 1;
}

}

#endif // __EMSCRIPTEN__
