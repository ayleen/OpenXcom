#pragma once

#ifdef __EMSCRIPTEN__
#include <SDL.h>
#include <string>
namespace OpenXcom
{
class State;
class Action;
class Game;
class TextList;
class NewResearchListState;
namespace Calypso
{
bool calypsoHdPrepareNativeBlit(const State *state);
Uint16 calypsoResearchReorderWarpDelta(const TextList *list, const Action *action);
void calypsoResearchWarpCursor(Game *game, const Action *action, int delta);
std::string calypsoCaptureNewResearchSelection(NewResearchListState& state);
void calypsoRestoreNewResearchSelection(NewResearchListState& state, const std::string& name);
}
}
#endif
