#ifdef __EMSCRIPTEN__
/*
 * Phase 39 (Calypso): first-run enable-tutorial prompt implementation.
 * Whole file Emscripten-only. Cloned structurally from CalypsoTutorialState:
 * setInterface("pauseMenu") + add(*, id, "pauseMenu"), centerAllSurfaces(),
 * enableUiScaling(320,200,0.75f), applyTTFToTexts. NO setWindowBackground --
 * it breaks under enableUiScaling (see the note at the former call site).
 */
#include "CalypsoTutorialAskState.h"
#include "CalypsoTutorial.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"

namespace OpenXcom
{

CalypsoTutorialAskState::CalypsoTutorialAskState()
	: _window(nullptr), _txtTitle(nullptr), _txtBody(nullptr), _btnYes(nullptr), _btnNo(nullptr)
{
	_screen = false;

	_window   = new Window(this, 240, 100, 40, 50, POPUP_BOTH);
	_txtTitle = new Text(228, 16, 46, 58);
	_txtBody  = new Text(228, 44, 46, 78);
	_btnYes   = new TextButton(110, 16, 46, 126);
	_btnNo    = new TextButton(110, 16, 164, 126);

	setInterface("pauseMenu", false, _game->getSavedGame() ? _game->getSavedGame()->getSavedBattle() : 0);

	add(_window, "window", "pauseMenu");
	add(_txtTitle, "text", "pauseMenu");
	add(_txtBody, "text", "pauseMenu");
	add(_btnYes, "button", "pauseMenu");
	add(_btnNo, "button", "pauseMenu");

	centerAllSurfaces();
	enableUiScaling(320, 200, 0.75f);
	applyTTFToTexts(_game->getMod()->getTTFFont("FONT_HD_HUD", false), 0.92f);

	// NO setWindowBackground() here -- same bug as CalypsoPrologueAskState
	// (Phase 41 review round 2, finding 4): Window::draw()'s background path
	// crops the fixed 320x200 BACK01.SCR to the window's post-enableUiScaling
	// size, which at HD resolutions overruns the source image and renders a
	// broken sliver with text floating over it. The themed bevel fill from
	// add()/setInterface() is scale-correct on its own; full root-cause
	// analysis at the matching site in CalypsoPrologueAskState.cpp.

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_CAL_TUT_ASK_TITLE"));

	_txtBody->setWordWrap(true);
	_txtBody->setText(tr("STR_CAL_TUT_ASK_1"));

	_btnYes->setText(tr("STR_CAL_TUT_ASK_YES"));
	_btnYes->onMouseClick((ActionHandler)&CalypsoTutorialAskState::btnYesClick);

	_btnNo->setText(tr("STR_CAL_TUT_ASK_NO"));
	_btnNo->onMouseClick((ActionHandler)&CalypsoTutorialAskState::btnNoClick);

	if (_game->getSavedGame() && _game->getSavedGame()->getSavedBattle())
		applyBattlescapeTheme("pauseMenu");
}

CalypsoTutorialAskState::~CalypsoTutorialAskState()
{
	// Let pump() push the next queued batch (geoWelcome) after we close.
	CalypsoTutorial::get().notifyPopupClosed();
}

void CalypsoTutorialAskState::resize(int& dX, int& dY)
{
#ifdef __EMSCRIPTEN__
	applyUiScaling();
#else
	State::resize(dX, dY);
#endif
}

void CalypsoTutorialAskState::btnYesClick(Action*)
{
	CalypsoTutorial::get().markShown("askEnable");
	_game->popState();
}

void CalypsoTutorialAskState::btnNoClick(Action*)
{
	CalypsoTutorial::get().markShown("askEnable");
	CalypsoTutorial::get().disableForCampaign();
	_game->popState();
}

} // namespace OpenXcom
#endif // __EMSCRIPTEN__
