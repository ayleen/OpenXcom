#pragma once
#ifdef __EMSCRIPTEN__
namespace OpenXcom {
class Screen;
namespace Calypso {
bool calypsoScreenRecoveryProbeAndInit();
void calypsoScreenResetDisplayRendererOnly(Screen &screen);
void calypsoScreenUploadLogicalTexture(Screen &screen);
bool calypsoScreenRecreateRendererGL(Screen &screen);
bool calypsoScreenRecoveryCommit(Screen &screen);
void calypsoScreenRefreshLogicalTexture(Screen &screen);
void calypsoScreenRebaseStagingSurface(Screen &screen, int width, int height);
} // namespace Calypso
} // namespace OpenXcom
#endif
