#pragma once
#ifdef __EMSCRIPTEN__
namespace OpenXcom {
class Screen;
namespace Calypso {
bool calypsoScreenRecoveryProbeAndInit();
void calypsoScreenResetDisplayRendererOnly(Screen &screen);
void calypsoScreenUploadLogicalTexture(Screen &screen);
bool calypsoScreenRecreateRendererGL(Screen &screen);
} // namespace Calypso
} // namespace OpenXcom
#endif
