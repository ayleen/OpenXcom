#pragma once
#ifdef __EMSCRIPTEN__
#include <SDL.h>

namespace OpenXcom
{
class Screen;
void calypsoNormalizeSdlMousePosition(int &x, int &y, Screen *screen);
void calypsoNormalizeSdlMouseMotionEvent(SDL_Event &ev, Screen *screen);
void calypsoNormalizeSdlMouseButtonEvent(SDL_Event &ev, Screen *screen);
} // namespace OpenXcom
#endif /* __EMSCRIPTEN__ */
