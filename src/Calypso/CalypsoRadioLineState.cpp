#ifdef __EMSCRIPTEN__
/*
 * Phase 41 (Calypso) -- transient radio-line toast popup implementation.
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 */

#include <utility>
#include <map>
#include <string>
#include <emscripten.h>

#include "CalypsoRadioLineState.h"
#include "CalypsoRadioLineTiming.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"  // Game::popState (State.h only forward-declares Game)

namespace OpenXcom
{

namespace
{
	unsigned s_nextNarrativeToken = 1;
	std::map<unsigned, std::function<void()>> s_narrativeCallbacks;

	unsigned storeNarrativeCallback(std::function<void()> callback)
	{
		if (!callback) return 0;
		while (s_nextNarrativeToken == 0 || s_narrativeCallbacks.count(s_nextNarrativeToken))
			++s_nextNarrativeToken;
		const unsigned token = s_nextNarrativeToken++;
		s_narrativeCallbacks.emplace(token, std::move(callback));
		return token;
	}
}

CalypsoRadioLineState::CalypsoRadioLineState(std::string stringId,
	std::function<void()> onDismissed)
	: _stringId(std::move(stringId)), _onDismissed(std::move(onDismissed))
{
	// _screen=false -> underlying Battlescape remains visible. This state is
	// nevertheless intentionally modal: explicit guidance waits for Continue.
	_screen = false;
}

CalypsoRadioLineState::~CalypsoRadioLineState()
{
	// Hide the overlay in case the state was popped without explicit dismissal
	// (e.g. load-game, state-stack reset) -- mirrors CalypsoTutorialState's dtor.
	EM_ASM_({ if (globalThis.__calypsoRadioHide) globalThis.__calypsoRadioHide(); });
}

void CalypsoRadioLineState::init()
{
	State::init();
	std::string body = std::string(tr(_stringId));
	// Bug 4 fix (QA round 1): radio lines used to reuse the tutorial popup's
	// __calypsoTutorialShow channel, which a same-frame tutorial popup could
	// clobber (battle start fires both). Dedicated radio channel now -- see
	// web/public/radio-overlay.js. This state is instruction-only.
	EM_ASM_(
	{
		if (globalThis.__calypsoRadioShow)
			globalThis.__calypsoRadioShow(UTF8ToString($0), 1);
	},
		body.c_str());
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
	// Move the callback out before popState(): the state leaves the live stack
	// immediately and is queued for deletion. The continuation is allowed to
	// continue scripted work only after this instruction was visibly dismissed.
	std::function<void()> onDismissed = std::move(_onDismissed);
	_game->popState();
	if (onDismissed) onDismissed();
}

void enqueueCalypsoNarrativeRadioLine(const std::string &body,
	std::function<void()> onDismissed)
{
	const unsigned token = storeNarrativeCallback(std::move(onDismissed));
	const unsigned durationMs = Calypso::radioNarrativeDurationMs(body);
	EM_ASM_(
	{
		var text = UTF8ToString($0);
		if (globalThis.__calypsoRadioNarrativeEnqueue)
		{
			globalThis.__calypsoRadioNarrativeEnqueue(text, $1, $2);
		}
		else if ($2)
		{
			// Do not strand an ending continuation if the optional DOM shell was
			// not loaded. Keep it asynchronous so radioLine() never re-enters the
			// scene in the middle of the current engine callback.
			setTimeout(function() {
				Module.ccall('calypso_radio_narrative_complete', null, ['number'], [$2]);
			}, 0);
		}
	},
		body.c_str(), durationMs, token);
}

void completeCalypsoNarrativeRadioLine(unsigned token)
{
	auto it = s_narrativeCallbacks.find(token);
	if (it == s_narrativeCallbacks.end()) return;
	std::function<void()> onDismissed = std::move(it->second);
	s_narrativeCallbacks.erase(it);
	if (onDismissed) onDismissed();
}

void cancelCalypsoNarrativeRadioLines()
{
	s_narrativeCallbacks.clear();
	EM_ASM_({
		if (globalThis.__calypsoRadioNarrativeClear)
			globalThis.__calypsoRadioNarrativeClear();
	});
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

EMSCRIPTEN_KEEPALIVE
void calypso_radio_narrative_complete(unsigned token)
{
	OpenXcom::completeCalypsoNarrativeRadioLine(token);
}

}

#endif // __EMSCRIPTEN__
