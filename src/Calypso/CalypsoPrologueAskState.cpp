#ifdef __EMSCRIPTEN__
/*
 * Phase 41 (Calypso): prologue-offer prompt — DOM overlay edition.
 * Whole file Emscripten-only.
 *
 * 2026-08-16 (F38 bugfix): the prompt was originally a bitmap-widget Window
 * (setInterface("pauseMenu") + Text + TextButton, enableUiScaling(320,200))
 * which read as an old-generation OXCE popup against the new HD screens.
 * Converted to the same invisible-modal + DOM-overlay pattern as
 * CalypsoTutorialState: _screen=false, no surfaces, page data pushed to JS
 * via calypso_notify_prologue_ask_show(), button clicks routed back through
 * the EMSCRIPTEN_KEEPALIVE exports at the bottom of this file. Design
 * follows tutorial-overlay.js / design/v1.1 components.
 *
 * Flow:
 *   constructor → init() → calypso_notify_prologue_ask_show(title, body, yes, no)
 *   JS button click → export → btnYesClick / btnNoClick → hide() + popState()
 *   unexpected pop → ~dtor → calypso_notify_prologue_ask_hide() (safety net)
 *
 * Shown from NewGameState::btnOkClick (via Calypso::maybeOfferPrologue),
 * BEFORE any SavedGame exists for this campaign.
 */
#include "CalypsoPrologueAskState.h"
#include "CalypsoPrologueCampaign.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include <emscripten.h>
#include <string>

namespace OpenXcom
{

/// Static pointer used by the JS callable exports to reach the active prompt.
CalypsoPrologueAskState* CalypsoPrologueAskState::_active = nullptr;

/* ── JS bridge ────────────────────────────────────────────────────────── */

static void calypso_notify_prologue_ask_show(
	const std::string& title, const std::string& body,
	const std::string& yesLabel, const std::string& noLabel)
{
	EM_ASM_({
		if (globalThis.__calypsoPrologueAskShow)
			globalThis.__calypsoPrologueAskShow(
				UTF8ToString($0), UTF8ToString($1),
				UTF8ToString($2), UTF8ToString($3));
	}, title.c_str(), body.c_str(), yesLabel.c_str(), noLabel.c_str());
}

static void calypso_notify_prologue_ask_hide()
{
	EM_ASM_({
		if (globalThis.__calypsoPrologueAskHide)
			globalThis.__calypsoPrologueAskHide();
	});
}

/* ── State ────────────────────────────────────────────────────────────── */

CalypsoPrologueAskState::CalypsoPrologueAskState()
{
	// _screen=false → non-fullscreen modal state (like CalypsoTutorialState).
	// No surfaces are added — the prompt is pure DOM overlay.
	_screen = false;
	_active = this;
}

CalypsoPrologueAskState::~CalypsoPrologueAskState()
{
	// Hide the DOM overlay in case the state was popped without an explicit
	// close (e.g. load-game, state-stack reset).
	calypso_notify_prologue_ask_hide();
	_active = nullptr;
}

void CalypsoPrologueAskState::init()
{
	State::init();
	const std::string title    = std::string(tr("STR_PROLOGUE_ASK_TITLE"));
	const std::string body     = std::string(tr("STR_PROLOGUE_ASK_BODY"));
	const std::string yesLabel = std::string(tr("STR_PROLOGUE_ASK_YES"));
	const std::string noLabel  = std::string(tr("STR_PROLOGUE_ASK_NO"));
	calypso_notify_prologue_ask_show(title, body, yesLabel, noLabel);
}

void CalypsoPrologueAskState::hide()
{
	calypso_notify_prologue_ask_hide();
}

void CalypsoPrologueAskState::btnYesClick()
{
	// Hide BEFORE popState: the destructor (where the hide hook lives) runs
	// only when Game::iterate() processes _deleted on a later frame, so the
	// DOM overlay would linger one frame otherwise.
	calypso_notify_prologue_ask_hide();
	_game->popState();
	// Commit 6 inserts the intro-clip trigger (JS overlay EM_ASM call) right
	// here, before the battle launches -- this is the single documented call
	// site the phase plan (41.5b) points at.
	EM_ASM({ if (globalThis.__calypsoPlayPrologueIntro) globalThis.__calypsoPlayPrologueIntro(); });
	Calypso::launchPrologueBattle(_game);
}

void CalypsoPrologueAskState::btnNoClick()
{
	calypso_notify_prologue_ask_hide();
	_game->popState();
	Calypso::vanillaNewGameTail(_game, Calypso::stashedDifficulty());
}

} // namespace OpenXcom

/* ── JS callable exports ──────────────────────────────────────────────── */

extern "C" {

EMSCRIPTEN_KEEPALIVE
void calypso_prologue_ask_yes()
{
	if (auto* s = OpenXcom::CalypsoPrologueAskState::getActive())
		s->btnYesClick();
}

EMSCRIPTEN_KEEPALIVE
void calypso_prologue_ask_no()
{
	if (auto* s = OpenXcom::CalypsoPrologueAskState::getActive())
		s->btnNoClick();
}

} /* extern "C" */

#endif // __EMSCRIPTEN__
