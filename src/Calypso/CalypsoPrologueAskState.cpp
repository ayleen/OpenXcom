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
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Mod/Mod.h"
#include <emscripten.h>

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

	// Review round 2 (P2, finding 4): NO setWindowBackground() here (unlike
	// the structurally-cloned CalypsoTutorialAskState). Window::draw()'s
	// non-ARGB background path (src/Interface/Window.cpp) crops a region out
	// of the interface's fixed 320x200 BACK01.SCR image sized to the
	// window's CURRENT (post-UI-scale) width/height -- but enableUiScaling()
	// above already stretched this window to fill a real HD canvas (e.g.
	// ~3x at 1280x720), so the crop rectangle massively exceeds BACK01.SCR's
	// actual bounds. That produced the review screenshot (afterok.png): only
	// the small, correctly-in-bounds sliver near the crop origin renders (the
	// stray green fragment top-left of the title), the rest of the window
	// gets nothing, and the title/body text -- which position correctly,
	// they scale with the same UI-scaling geometry -- end up floating over
	// broken/absent background art instead of a clean fill, reading as
	// "oversized"/overlapping. CalypsoPrologueEndState (cited in the finding
	// as the working reference) sidesteps this the same way: it never adds a
	// Window at all. Dropping the background image here leaves the window's
	// normal themed bevel fill (still driven by the "pauseMenu" interface's
	// per-element `color:`, applied via add()/setInterface() above,
	// independent of setWindowBackground) -- solid, scale-correct at any
	// resolution, and fully readable. This is scoped to THIS state; the
	// identical pattern in CalypsoTutorialAskState.cpp is a pre-existing,
	// separate bug left untouched per this review's scope.

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
	_game->popState();
	// Commit 6 inserts the intro-clip trigger (JS overlay EM_ASM call) right
	// here, before the battle launches -- this is the single documented call
	// site the phase plan (41.5b) points at.
	EM_ASM({ if (globalThis.__calypsoPlayPrologueIntro) globalThis.__calypsoPlayPrologueIntro(); });
	Calypso::launchPrologueBattle(_game);
}

void CalypsoPrologueAskState::btnNoClick(Action*)
{
	_game->popState();
	Calypso::vanillaNewGameTail(_game, Calypso::stashedDifficulty());
}

} // namespace OpenXcom
#endif // __EMSCRIPTEN__
