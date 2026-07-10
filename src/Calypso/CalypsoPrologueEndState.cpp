#ifdef __EMSCRIPTEN__
/*
 * Phase 41 (Calypso) -- prologue end-state ("Six months later.") implementation.
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 *
 * _screen=true and no window/background surface is added: Screen::clear()
 * blanks the buffer to black every frame (same trick InfoboxState-adjacent
 * fullscreen states rely on), so the only thing drawn is the centered title.
 */
#include "CalypsoPrologueEndState.h"
#include "CalypsoPrologueCampaign.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Interface/Text.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"

namespace OpenXcom
{

CalypsoPrologueEndState::CalypsoPrologueEndState(int outcome)
	: _txtTitle(nullptr), _outcome(outcome)
{
	_screen = true;

	_txtTitle = new Text(320, 32, 0, 84);

	// Reuse the still-live prologue battle's depth-appropriate palette (the
	// SavedBattleGame is torn down only after this state resolves).
	if (_game->getSavedGame() && _game->getSavedGame()->getSavedBattle())
		_game->getSavedGame()->getSavedBattle()->setPaletteByDepth(this);

	setInterface("pauseMenu", false, _game->getSavedGame() ? _game->getSavedGame()->getSavedBattle() : 0);
	add(_txtTitle, "text", "pauseMenu");

	centerAllSurfaces();
	enableUiScaling(320, 200, 1.0f);
	applyTTFToTexts(_game->getMod()->getTTFFont("FONT_HD_HUD", false), 1.1f);

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setVerticalAlign(ALIGN_MIDDLE);
	_txtTitle->setBig();
	_txtTitle->setHighContrast(true);
	_txtTitle->setText(tr("STR_PROLOGUE_SIX_MONTHS"));
}

CalypsoPrologueEndState::~CalypsoPrologueEndState()
{
}

void CalypsoPrologueEndState::handle(Action* action)
{
	State::handle(action);
	if (action->getDetails()->type == SDL_KEYDOWN || action->getDetails()->type == SDL_MOUSEBUTTONDOWN)
	{
		// Safe to replace `this` from inside its own handler: Game::setState
		// (called at the bottom of finishPrologue's tail) defers deletion to
		// the end-of-cycle queue, same as NewGameState::btnOkClick already
		// relies on when it replaces itself.
		Calypso::finishPrologue(_game, _outcome);
	}
}

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
