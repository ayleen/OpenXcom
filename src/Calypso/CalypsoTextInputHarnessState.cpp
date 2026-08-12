#ifdef __EMSCRIPTEN__

#include "CalypsoTextInputHarnessState.h"

#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextEdit.h"
#include <emscripten.h>

namespace OpenXcom
{

CalypsoTextInputHarnessState::CalypsoTextInputHarnessState(bool multiline) :
	_edit(new TextEdit(this, 400, multiline ? 80 : 24, 120, 120)),
	_changeCount(0), _transitionCount(0), _terminalKeyCount(0),
	_lastTerminalKey(SDLK_UNKNOWN)
{
	_screen = true;
	setStandardPalette("PAL_GEOSCAPE");
	add(_edit);
	_edit->setMultiline(multiline);
	if (multiline) _edit->setEnterPolicy(TEEP_COMMIT);
	_edit->onChange((ActionHandler)&CalypsoTextInputHarnessState::editChanged);
	_edit->onEnter((ActionHandler)&CalypsoTextInputHarnessState::editCommitted);
}

void CalypsoTextInputHarnessState::init()
{
	State::init();
	_edit->setFocus(true, true);
}

void CalypsoTextInputHarnessState::handle(Action *action)
{
	if (action->getDetails()->type == SDL_KEYDOWN)
	{
		const SDL_Keycode key = action->getDetails()->key.keysym.sym;
		if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_ESCAPE)
		{
			++_terminalKeyCount;
			_lastTerminalKey = key;
		}
	}
	State::handle(action);
}

void CalypsoTextInputHarnessState::editChanged(Action *)
{
	++_changeCount;
}

void CalypsoTextInputHarnessState::editCommitted(Action *action)
{
	++_transitionCount;
	if (action && action->getDetails()->type == SDL_KEYDOWN)
		_lastTerminalKey = action->getDetails()->key.keysym.sym;
}

std::string CalypsoTextInputHarnessState::value() const
{
	return _edit->getText();
}

} // namespace OpenXcom

namespace
{
OpenXcom::CalypsoTextInputHarnessState *textInputHarness()
{
	OpenXcom::Game *game = OpenXcom::getCurrentGame();
	return game ? dynamic_cast<OpenXcom::CalypsoTextInputHarnessState *>(game->getTopState()) : nullptr;
}
}

extern "C" {

EMSCRIPTEN_KEEPALIVE int calypso_text_harness_activate(int multiline)
{
	OpenXcom::Game *game = OpenXcom::getCurrentGame();
	if (!game) return 0;
	OpenXcom::Options::keyboardMode = OpenXcom::KEYBOARD_ON;
	game->pushState(new OpenXcom::CalypsoTextInputHarnessState(multiline != 0));
	return 1;
}

EMSCRIPTEN_KEEPALIVE const char *calypso_text_harness_value(void)
{
	static std::string value;
	auto *state = textInputHarness();
	value = state ? state->value() : "";
	return value.c_str();
}

EMSCRIPTEN_KEEPALIVE int calypso_text_harness_changes(void)
{
	auto *state = textInputHarness(); return state ? state->changeCount() : -1;
}

EMSCRIPTEN_KEEPALIVE int calypso_text_harness_transitions(void)
{
	auto *state = textInputHarness(); return state ? state->transitionCount() : -1;
}

EMSCRIPTEN_KEEPALIVE int calypso_text_harness_terminal_keys(void)
{
	auto *state = textInputHarness(); return state ? state->terminalKeyCount() : -1;
}

EMSCRIPTEN_KEEPALIVE int calypso_text_harness_last_key(void)
{
	auto *state = textInputHarness();
	return state ? state->lastTerminalKey() : SDLK_UNKNOWN;
}

} // extern "C"

#endif /* __EMSCRIPTEN__ */
