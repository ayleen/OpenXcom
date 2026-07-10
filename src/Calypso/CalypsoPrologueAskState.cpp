#ifdef __EMSCRIPTEN__
/*
 * Phase 41 (Calypso): prologue-offer prompt implementation. Whole file
 * Emscripten-only. Cloned structurally from CalypsoTutorialAskState: same
 * setInterface("pauseMenu") + add(*, id, "pauseMenu"), centerAllSurfaces(),
 * enableUiScaling(320,200,0.75f), applyTTFToTexts, setWindowBackground.
 *
 * Shown from NewGameState::btnOkClick (via Calypso::maybeOfferPrologue),
 * BEFORE any SavedGame exists for this campaign -- _game->getSavedGame() is
 * null here, same as at the very start of a fresh New Game.
 */
#include "CalypsoPrologueAskState.h"
#include "CalypsoPrologueCampaign.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Mod/Mod.h"

namespace OpenXcom
{

CalypsoPrologueAskState::CalypsoPrologueAskState()
	: _window(nullptr), _txtTitle(nullptr), _txtBody(nullptr), _btnYes(nullptr), _btnNo(nullptr)
{
	_screen = false;

	_window   = new Window(this, 240, 100, 40, 50, POPUP_BOTH);
	_txtTitle = new Text(228, 16, 46, 58);
	_txtBody  = new Text(228, 44, 46, 78);
	_btnYes   = new TextButton(110, 16, 46, 126);
	_btnNo    = new TextButton(110, 16, 164, 126);

	setInterface("pauseMenu");

	add(_window, "window", "pauseMenu");
	add(_txtTitle, "text", "pauseMenu");
	add(_txtBody, "text", "pauseMenu");
	add(_btnYes, "button", "pauseMenu");
	add(_btnNo, "button", "pauseMenu");

	centerAllSurfaces();
	enableUiScaling(320, 200, 0.75f);
	applyTTFToTexts(_game->getMod()->getTTFFont("FONT_HD_HUD", false), 0.92f);

	setWindowBackground(_window, "pauseMenu");

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_PROLOGUE_ASK_TITLE"));

	_txtBody->setWordWrap(true);
	_txtBody->setText(tr("STR_PROLOGUE_ASK_BODY"));

	_btnYes->setText(tr("STR_PROLOGUE_ASK_YES"));
	_btnYes->onMouseClick((ActionHandler)&CalypsoPrologueAskState::btnYesClick);

	_btnNo->setText(tr("STR_PROLOGUE_ASK_NO"));
	_btnNo->onMouseClick((ActionHandler)&CalypsoPrologueAskState::btnNoClick);
}

CalypsoPrologueAskState::~CalypsoPrologueAskState()
{
}

void CalypsoPrologueAskState::resize(int& dX, int& dY)
{
#ifdef __EMSCRIPTEN__
	applyUiScaling();
#else
	State::resize(dX, dY);
#endif
}

void CalypsoPrologueAskState::btnYesClick(Action*)
{
	Options::calypsoPrologueSeen = true;
	_game->popState();
	// Commit 6 inserts the intro-clip trigger (JS overlay EM_ASM call) right
	// here, before the battle launches -- this is the single documented call
	// site the phase plan (41.5b) points at.
	Calypso::launchPrologueBattle(_game);
}

void CalypsoPrologueAskState::btnNoClick(Action*)
{
	Options::calypsoPrologueSeen = true;
	_game->popState();
	Calypso::vanillaNewGameTail(_game, Calypso::stashedDifficulty());
}

} // namespace OpenXcom
#endif // __EMSCRIPTEN__
