#ifdef __EMSCRIPTEN__
#include "../Engine/Screen.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Engine/State.h"
#include "../Interface/Cursor.h"
#include "../Interface/FpsCounter.h"
#include "CalypsoHdUiOverlay.h"
#include <emscripten.h>

extern OpenXcom::Game *game;

namespace OpenXcom
{
void Game::refreshCalypsoMousePosition()
{
	const int x = static_cast<int>(_cursor->getX() * _screen->getXScale())
		+ _screen->getCursorLeftBlackBand();
	const int y = static_cast<int>(_cursor->getY() * _screen->getYScale())
		+ _screen->getCursorTopBlackBand();
	dispatchCalypsoMouseMotion(x, y, 0, 0);
}

bool Game::dispatchCalypsoMouseMotion(int x, int y, int xrel, int yrel)
{
	if (!_init || !_mouseActive || !_screen || !_cursor || !_fpsCounter
		|| _states.empty())
		return false;

	_runningState = RUNNING;
	SDL_Event event;
	SDL_memset(&event, 0, sizeof(event));
	event.type = SDL_MOUSEMOTION;
	event.motion.which = CALYPSO_MOUSE_BRIDGE_ID;
	event.motion.state = SDL_GetMouseState(nullptr, nullptr);
	event.motion.x = static_cast<Sint32>(x);
	event.motion.y = static_cast<Sint32>(y);
	event.motion.xrel = static_cast<Sint32>(xrel);
	event.motion.yrel = static_cast<Sint32>(yrel);

	Action action(&event, _screen->getXScale(), _screen->getYScale(),
		_screen->getCursorTopBlackBand(), _screen->getCursorLeftBlackBand());
	_screen->handle(&action);
	_cursor->handle(&action);
	_fpsCounter->handle(&action);
	if (!_states.empty())
		_states.back()->handle(&action);
	return true;
}

bool Game::dispatchCalypsoMouseButton(int x, int y, int button, bool pressed)
{
	if (!_init || !_mouseActive || !_screen || !_cursor || !_fpsCounter
		|| _states.empty() || button < 1 || button > 5)
		return false;

	_runningState = RUNNING;
	auto dispatch = [this](SDL_Event &event)
	{
		Action action(&event, _screen->getXScale(), _screen->getYScale(),
			_screen->getCursorTopBlackBand(), _screen->getCursorLeftBlackBand());
		_screen->handle(&action);
		_cursor->handle(&action);
		_fpsCounter->handle(&action);
		if (!_states.empty())
			_states.back()->handle(&action);
	};

	// A browser click may arrive without a prior motion record. Refresh the
	// hover owners first so press/release semantics match an ordinary hardware
	// click on every InteractiveSurface.
	if (pressed && !dispatchCalypsoMouseMotion(x, y, 0, 0))
		return false;

	SDL_Event event;
	SDL_memset(&event, 0, sizeof(event));
	event.type = pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.button = static_cast<Uint8>(button);
	event.button.state = pressed ? SDL_PRESSED : SDL_RELEASED;
	event.button.clicks = 1;
	event.button.which = CALYPSO_MOUSE_BRIDGE_ID;
	event.button.x = static_cast<Sint32>(x);
	event.button.y = static_cast<Sint32>(y);
	dispatch(event);
	return true;
}

void Game::recoverContextTick()
{
	if (!_screen)
	{
		Calypso::CalypsoHdUiOverlay::instance().failHdRoute("WebGL recovery tick has no screen");
	}

	bool resetSeen = false;
	SDL_Event event;
	// The reset event is the only event allowed through this bounded callback.
	// Drain stale browser/input events without dispatching them to any owner.
	while (SDL_PollEvent(&event))
	{
		if (!resetSeen && event.type == SDL_RENDER_TARGETS_RESET)
		{
			Action action(&event, _screen->getXScale(), _screen->getYScale(),
				_screen->getCursorTopBlackBand(), _screen->getCursorLeftBlackBand());
			_screen->handle(&action);
			resetSeen = true;
		}
	}
	if (!resetSeen)
	{
		Calypso::CalypsoHdUiOverlay::instance().failHdRoute("WebGL recovery reset event missing");
	}
}

void Game::calypsoRestartMainLoop()
{
	if (!game) return;
	emscripten_cancel_main_loop();
	emscripten_set_main_loop_arg(emscriptenIter, game, 0, 0);
}
} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
