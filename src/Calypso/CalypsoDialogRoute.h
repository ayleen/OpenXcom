#pragma once
#ifdef __EMSCRIPTEN__
namespace OpenXcom {
class Game;
namespace Calypso {
bool calypsoDialogUsesHd(Game *game, const char *family, bool explicitForm = false);
}}
#endif
