#ifdef __EMSCRIPTEN__
#pragma once
/*
 * Phase 41 (Calypso): "play the prologue mission?" prompt — DOM overlay
 * edition (2026-08-16, F38 bugfix). Rendered by web/public/prologue-ask.js;
 * the state is an invisible modal (_screen=false, no surfaces) exactly like
 * CalypsoTutorialState, so the underlying game frame stays visible behind
 * the HTML popup.
 *
 * C++ keeps the decision flow: the constructor resolves the translated
 * strings, init() pushes them to JS via EM_ASM, and the two
 * EMSCRIPTEN_KEEPALIVE exports (calypso_prologue_ask_yes / _no) route the
 * button clicks back into btnYesClick()/btnNoClick().
 *
 * Shown from NewGameState::btnOkClick (via Calypso::maybeOfferPrologue),
 * BEFORE any SavedGame exists for this campaign -- _game->getSavedGame() is
 * null here, same as at the very start of a fresh New Game.
 */
#include "../Engine/State.h"

namespace OpenXcom
{
class CalypsoPrologueAskState : public State
{
private:
	/// Singleton-style pointer so the JS exports can reach the active prompt.
	static CalypsoPrologueAskState* _active;

public:
	CalypsoPrologueAskState();
	~CalypsoPrologueAskState();
	void init() override;

	/// Returns the currently active prompt, or nullptr (used by JS exports).
	static CalypsoPrologueAskState* getActive() { return _active; }

	/// Handler for "PLAY THE PROLOGUE" (called from JS export).
	void btnYesClick();
	/// Handler for "SKIP TO THE CAMPAIGN" (called from JS export).
	void btnNoClick();
	/// Hide the DOM overlay without popping (used before popState()).
	void hide();
};

} // namespace OpenXcom
#endif // __EMSCRIPTEN__
