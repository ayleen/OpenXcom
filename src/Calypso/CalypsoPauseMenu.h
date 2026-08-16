#ifdef __EMSCRIPTEN__
#pragma once
/*
 * F33 (Calypso): DOM overlay bridge for the pause menu (PauseState).
 * Whole file Emscripten-only.
 *
 * PauseState (Menu/PauseState.cpp) keeps ALL of its logic — button
 * visibility (ironman, scripted-scene save/load blocking, battlescape
 * turn/busy gating, tab-hide path) — but its bitmap widgets are hidden in
 * the browser and the menu renders as an HTML overlay
 * (web/public/pause-menu.js). The state pushes the computed labels/visibility
 * here via pauseMenuDomShow(), and the calypso_pause_* exports route DOM
 * button clicks back into the SAME PauseState handlers (btnLoadClick etc.).
 *
 * Load/Save/Options open the existing HTML shells (calypsoOpenLoad/Save/
 * Options in screens/common.js) exactly like the native handlers do.
 */
#include <string>

namespace OpenXcom
{
class Game;
class PauseState;

namespace Calypso
{

/// Push the pause-menu DOM overlay with the computed labels and visibility.
void pauseMenuDomShow(
	int origin,
	bool showLoad, bool showSave, bool showAbandon, bool showOptions, bool showCancel,
	const std::string &title,
	const std::string &loadLabel, const std::string &saveLabel,
	const std::string &abandonLabel, const std::string &optionsLabel,
	const std::string &cancelLabel);

/// Hide the pause-menu DOM overlay (called before popState and in ~PauseState).
void pauseMenuDomHide();

/// F33 (Phase 46.4-F33 placement cleanup): the PauseState DOM-overlay hooks.
/// Implemented as STATIC MEMBERS so the friend declaration in PauseState.h
/// (friend class Calypso::CalypsoPauseMenu) grants private access -- a friend
/// class covers its member functions, not free functions.
class CalypsoPauseMenu
{
public:
	/// PauseState constructor hook: hide the bitmap widgets and raise the DOM
	/// overlay with the current labels/visibility. Body lives here, not in
	/// PauseState.cpp (placement policy R3).
	static void configure(PauseState& state);

	/// Re-show the DOM overlay when PauseState becomes the top state again
	/// (e.g. AbandonGameState was cancelled and popped back to the pause menu).
	static void think(PauseState& state, Game& game);
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
