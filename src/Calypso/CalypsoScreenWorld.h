#pragma once
#ifdef __EMSCRIPTEN__
namespace OpenXcom {
class Screen;
namespace Calypso {
bool calypsoScreenFlipWorldPass(Screen &screen, bool hasWorldPasses);
bool calypsoScreenRenderChrome(Screen &screen);
void calypsoScreenFlipWorldClose(Screen &screen);
} // namespace Calypso
} // namespace OpenXcom
#endif
