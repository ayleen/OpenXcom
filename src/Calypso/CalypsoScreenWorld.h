#pragma once
#ifdef __EMSCRIPTEN__
namespace OpenXcom {
class Screen;
namespace Calypso {
bool calypsoScreenFlipWorldPass(Screen &screen, bool hasWorldPasses);
void calypsoScreenFlipWorldClose(Screen &screen);
} // namespace Calypso
} // namespace OpenXcom
#endif
