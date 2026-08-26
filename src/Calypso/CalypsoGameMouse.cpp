#ifdef __EMSCRIPTEN__
#include "CalypsoGameMouse.h"
#include "CalypsoCanvasCoordinateMapping.h"
#include "CalypsoViewportRuntime.h"
#include "../Engine/Screen.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Interface/Cursor.h"
#include "../Interface/FpsCounter.h"
#include "../Engine/Logger.h"
#include "CalypsoHdUiOverlay.h"
#include <cmath>

namespace OpenXcom
{
void calypsoNormalizeSdlMousePosition(int &x, int &y, Screen *screen)
{
	if (!screen) return;
	const Calypso::CalypsoViewportRuntime &rt = Calypso::calypsoViewportRuntime();
	if (!rt.hasLayout()) return;
	int logicalW = rt.current().logicalWidth;
	int logicalH = rt.current().logicalHeight;
	int displayW = screen->getWidth();
	int displayH = screen->getHeight();
	double nx = Calypso::calypsoCanvasToDisplayCoordinate(static_cast<double>(x), logicalW, displayW);
	double ny = Calypso::calypsoCanvasToDisplayCoordinate(static_cast<double>(y), logicalH, displayH);
	x = static_cast<int>(std::lround(nx));
	y = static_cast<int>(std::lround(ny));
}
void calypsoNormalizeSdlMouseMotionEvent(SDL_Event &ev, Screen *screen)
{
	if (ev.type != SDL_MOUSEMOTION) return;
	int x = ev.motion.x;
	int y = ev.motion.y;
	calypsoNormalizeSdlMousePosition(x, y, screen);
	ev.motion.x = static_cast<Sint32>(x);
	ev.motion.y = static_cast<Sint32>(y);
	const Calypso::CalypsoViewportRuntime &rt = Calypso::calypsoViewportRuntime();
	if (!rt.hasLayout()) return;
	int logicalW = rt.current().logicalWidth;
	int logicalH = rt.current().logicalHeight;
	int displayW = screen->getWidth();
	int displayH = screen->getHeight();
	if (logicalW <= 0 || logicalH <= 0 || displayW <= 0 || displayH <= 0) return;
	double factorX = static_cast<double>(displayW) / static_cast<double>(logicalW);
	double factorY = static_cast<double>(displayH) / static_cast<double>(logicalH);
	ev.motion.xrel = static_cast<Sint16>(std::lround(ev.motion.xrel * factorX));
	ev.motion.yrel = static_cast<Sint16>(std::lround(ev.motion.yrel * factorY));
}
void calypsoNormalizeSdlMouseButtonEvent(SDL_Event &ev, Screen *screen)
{
	if (ev.type != SDL_MOUSEBUTTONDOWN && ev.type != SDL_MOUSEBUTTONUP) return;
	int x = ev.button.x;
	int y = ev.button.y;
	calypsoNormalizeSdlMousePosition(x, y, screen);
	ev.button.x = static_cast<Sint32>(x);
	ev.button.y = static_cast<Sint32>(y);
}
} // namespace OpenXcom

namespace OpenXcom
{
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

	// A browser click may arrive without a usable SDL motion event. Refresh the
	// native hover owners first so press/release semantics match an ordinary
	// hardware click on every InteractiveSurface.
	if (pressed)
	{
		SDL_Event motion;
		SDL_memset(&motion, 0, sizeof(motion));
		motion.type = SDL_MOUSEMOTION;
		motion.motion.x = static_cast<Sint32>(x);
		motion.motion.y = static_cast<Sint32>(y);
		dispatch(motion);
	}

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
	if (!::game) return;
	emscripten_cancel_main_loop();
	emscripten_set_main_loop_arg(emscriptenIter, ::game, 0, 0);
}
} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
