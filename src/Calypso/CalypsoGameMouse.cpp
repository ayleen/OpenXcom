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

bool Game::dispatchCalypsoMouseWheel()
{
	if (!_mouseActive) return false;
	_runningState = RUNNING;
	Sint32 wheelY = _event.wheel.y;
	float preciseY = 0.0f;
#if SDL_VERSION_ATLEAST(2, 0, 18)
	preciseY = _event.wheel.preciseY;
#endif
	if (_event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
	{
		wheelY = -wheelY;
		preciseY = -preciseY;
	}
	int direction = 0;
	if (wheelY > 0 || (wheelY == 0 && preciseY > 0.0f)) direction = 1;
	else if (wheelY < 0 || (wheelY == 0 && preciseY < 0.0f)) direction = -1;
	else return false;

	const double sx = _screen->getXScale(), sy = _screen->getYScale();
	const int top = _screen->getCursorTopBlackBand();
	const int left = _screen->getCursorLeftBlackBand();
	SDL_Event event;
	SDL_memset(&event, 0, sizeof(event));
	event.button.button = direction > 0 ? SDL_BUTTON_WHEELUP : SDL_BUTTON_WHEELDOWN;
	// Use the bridge-owned pointer, never potentially stale SDL mouse coordinates.
	event.button.x = static_cast<Sint32>(_cursor->getX() * sx) + left;
	event.button.y = static_cast<Sint32>(_cursor->getY() * sy) + top;
	// Keep DOWN and UP inline: queued release can leave the button latched
	// when a burst delivers its next wheel tick before the release is polled.
	event.type = SDL_MOUSEBUTTONDOWN;
	event.button.state = SDL_PRESSED;
	{
		Action action(&event, sx, sy, top, left);
		_screen->handle(&action);
		_cursor->handle(&action);
		_fpsCounter->handle(&action);
		if (!_states.empty()) _states.back()->handle(&action);
	}
	event.type = SDL_MOUSEBUTTONUP;
	event.button.state = SDL_RELEASED;
	{
		Action action(&event, sx, sy, top, left);
		_screen->handle(&action);
		_cursor->handle(&action);
		_fpsCounter->handle(&action);
		if (!_states.empty()) _states.back()->handle(&action);
	}
	return true;
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
