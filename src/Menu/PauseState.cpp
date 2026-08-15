/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "PauseState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "AbandonGameState.h"
#include "ListLoadState.h"
#include "ListSaveState.h"
#include "../Engine/Options.h"
#include "OptionsVideoState.h"
#include "OptionsGeoscapeState.h"
#include "OptionsBattlescapeState.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Battlescape/BattlescapeGame.h"
#include "../version.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "../Calypso/CalypsoDirector.h"
#include "../Calypso/CalypsoPauseMenu.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Pause window.
 * @param game Pointer to the core game.
 * @param origin Game section that originated this state.
 */
PauseState::PauseState(OptionsOrigin origin) : _origin(origin)
{
	_screen = false;

	int x;
	if (_origin == OPT_GEOSCAPE)
	{
		x = 20;
	}
	else
	{
		x = 52;
	}

	// Create objects
	_window = new Window(this, 216, 160, x, 20, POPUP_BOTH);
	_btnLoad = new TextButton(180, 18, x+18, 52);
	_btnSave = new TextButton(180, 18, x+18, 74);
	_btnAbandon = new TextButton(180, 18, x+18, 96);
	_btnOptions = new TextButton(180, 18, x+18, 122);
	_btnCancel = new TextButton(180, 18, x+18, 150);
	_txtTitle = new Text(206, 17, x+5, 32);
	_txtVersion = new Text(216, 9, x, 11);

	// Set palette
	setInterface("pauseMenu", false, _game->getSavedGame() ? _game->getSavedGame()->getSavedBattle() : 0);

	add(_window, "window", "pauseMenu");
	add(_btnLoad, "button", "pauseMenu");
	add(_btnSave, "button", "pauseMenu");
	add(_btnAbandon, "button", "pauseMenu");
	add(_btnOptions, "button", "pauseMenu");
	add(_btnCancel, "button", "pauseMenu");
	add(_txtTitle, "text", "pauseMenu");
	add(_txtVersion, "text", "pauseMenu");

	centerAllSurfaces();

#ifdef __EMSCRIPTEN__
	// Calypso: scale the in-battle pause popup (Window-based; border vector-scales,
	// bg tiles — no snapshot needed). No composite widgets here, so per-surface scaling
	// is sufficient.
	enableUiScaling(320, 200, 0.75f);
	applyTTFToTexts(_game->getMod()->getTTFFont("FONT_HD_HUD", false), 0.92f);
#endif

	// Set up objects
	setWindowBackground(_window, "pauseMenu");

	_btnLoad->setText(tr("STR_LOAD_GAME"));
	_btnLoad->onMouseClick((ActionHandler)&PauseState::btnLoadClick);

	_btnSave->setText(tr("STR_SAVE_GAME"));
	_btnSave->onMouseClick((ActionHandler)&PauseState::btnSaveClick);

	_btnAbandon->setText(tr("STR_ABANDON_GAME"));
	_btnAbandon->onMouseClick((ActionHandler)&PauseState::btnAbandonClick);

	_btnOptions->setText(tr("STR_GAME_OPTIONS"));
	_btnOptions->onMouseClick((ActionHandler)&PauseState::btnOptionsClick);

	_btnCancel->setText(tr("STR_CANCEL_UC"));
	_btnCancel->onMouseClick((ActionHandler)&PauseState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&PauseState::btnCancelClick, Options::keyCancel);
	if (origin == OPT_GEOSCAPE)
	{
		_btnCancel->onKeyboardPress((ActionHandler)&PauseState::btnCancelClick, Options::keyGeoOptions);
	}
	else if (origin == OPT_BATTLESCAPE)
	{
		_btnCancel->onKeyboardPress((ActionHandler)&PauseState::btnCancelClick, Options::keyBattleOptions);
		if (!_game->getSavedGame()->getSavedBattle()->getBattleGame()->getStates().empty())
		{
			_btnOptions->setVisible(false);
		}
	}

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_OPTIONS_UC"));

	std::ostringstream title;
	title << "OpenXcom " << OPENXCOM_VERSION_SHORT;
	_txtVersion->setText(title.str());
	_txtVersion->setAlign(ALIGN_CENTER);

	if (_origin == OPT_BATTLESCAPE)
	{
		applyBattlescapeTheme("pauseMenu");
	}

	if (_game->getSavedGame()->isIronman())
	{
		_btnLoad->setVisible(false);
		_btnSave->setVisible(false);
		_btnAbandon->setText(tr("STR_SAVE_AND_ABANDON_GAME"));
	}

#ifdef __EMSCRIPTEN__
	// A scripted scene forbids manual save/load, not the pause menu itself.
	// This also covers the browser tab-hide path because it pushes the same
	// PauseState. The bridge endpoints repeat the policy as a trust boundary.
	if (_origin == OPT_BATTLESCAPE && CalypsoDirector::get().activeSceneBlocksSaveLoad())
	{
		_btnLoad->setVisible(false);
		_btnSave->setVisible(false);
		// An Ironman campaign normally saves before abandoning, but the scripted
		// prologue deliberately discards its throwaway battle without creating a
		// player-controlled checkpoint. Keep the destructive label truthful.
		_btnAbandon->setText(tr("STR_ABANDON_GAME"));
	}
#endif

	// ENOUGH! No save corruption when trying to save/exit mid-action (e.g. during alien turn)
	if (origin == OPT_BATTLESCAPE)
	{
		bool playerTurn = _game->getSavedGame()->getSavedBattle()->getSide() == FACTION_PLAYER;
		bool debugMode = _game->getSavedGame()->getSavedBattle()->getDebugMode();
		bool busy = !_game->getSavedGame()->getSavedBattle()->getBattleGame()->getStates().empty();

		if ((!playerTurn && !debugMode) || busy)
		{
			_btnSave->setVisible(false); // non-ironman + ironman

			if (_game->getSavedGame()->isIronman())
			{
				_btnAbandon->setVisible(false); // ironman only
			}
		}
	}

#ifdef __EMSCRIPTEN__
	// F33 (2026-08-16): DOM overlay edition. All visibility/label logic above
	// is unchanged; the bitmap widgets are merely hidden and the menu renders
	// as an HTML overlay (pause-menu.js). All pushState(PauseState) call
	// sites (Geoscape options, Battlescape pause, tab-hide) are covered
	// because the hook lives here in the state itself.
	domShow();
	_window->setVisible(false);
	_btnLoad->setVisible(false);
	_btnSave->setVisible(false);
	_btnAbandon->setVisible(false);
	_btnOptions->setVisible(false);
	_btnCancel->setVisible(false);
	_txtTitle->setVisible(false);
	_txtVersion->setVisible(false);
#endif
}

/**
 *
 */
PauseState::~PauseState()
{
#ifdef __EMSCRIPTEN__
	// Safety net for pops that skip btnCancelClick (abandon confirm, etc.).
	Calypso::pauseMenuDomHide();
#endif
}

/**
 * Calypso (Emscripten): rescale to the logical buffer instead of the base recenter.
 */
void PauseState::resize(int &dX, int &dY)
{
#ifdef __EMSCRIPTEN__
	applyUiScaling();
#else
	State::resize(dX, dY);
#endif
}

#ifdef __EMSCRIPTEN__
/**
 * F33: push the pause-menu DOM overlay with the current labels/visibility.
 */
void PauseState::domShow()
{
	Calypso::pauseMenuDomShow(
		(int)_origin,
		_btnLoad->getVisible(), _btnSave->getVisible(), _btnAbandon->getVisible(),
		_btnOptions->getVisible(), _btnCancel->getVisible(),
		std::string(tr("STR_OPTIONS_UC")),
		_btnLoad->getText(), _btnSave->getText(), _btnAbandon->getText(),
		_btnOptions->getText(), _btnCancel->getText());
}

/**
 * F33: re-show the DOM overlay when this state becomes the top state again
 * (e.g. AbandonGameState was cancelled and popped back to the pause menu).
 * Every frame while top, the JS hook no-ops once the overlay is visible.
 */
void PauseState::think()
{
	State::think();
	if (_game->isState(this))
	{
		domShow();
	}
}
#endif

/**
 * Opens the Load Game screen.
 * @param action Pointer to an action.
 */
void PauseState::btnLoadClick(Action *)
{
#ifdef __EMSCRIPTEN__
	if (_origin == OPT_BATTLESCAPE && CalypsoDirector::get().activeSceneBlocksSaveLoad()) return;
#endif
#ifdef __EMSCRIPTEN__
	// Phase 41: route to the HTML overlay when the hook exists (fallback: native).
	int handled = EM_ASM_INT({
		return (typeof window !== 'undefined' && window.calypsoOpenLoad)
			? (window.calypsoOpenLoad($0), 1) : 0;
	}, (int)_origin);
	if (handled) return;
#endif
	_game->pushState(new ListLoadState(_origin));
}

/**
 * Opens the Save Game screen.
 * @param action Pointer to an action.
 */
void PauseState::btnSaveClick(Action *)
{
#ifdef __EMSCRIPTEN__
	if (_origin == OPT_BATTLESCAPE && CalypsoDirector::get().activeSceneBlocksSaveLoad()) return;
#endif
#ifdef __EMSCRIPTEN__
	// Phase 41: route to the HTML overlay when the hook exists (fallback: native).
	int handled = EM_ASM_INT({
		return (typeof window !== 'undefined' && window.calypsoOpenSave)
			? (window.calypsoOpenSave($0), 1) : 0;
	}, (int)_origin);
	if (handled) return;
#endif
	_game->pushState(new ListSaveState(_origin));
}

/**
 * Opens the Game Options screen.
 * @param action Pointer to an action.
 */
void PauseState::btnOptionsClick(Action *)
{
	Options::backupDisplay();
#ifdef __EMSCRIPTEN__
	// Phase 41: route to the HTML overlay when the hook exists (fallback: native).
	int handled = EM_ASM_INT({
		return (typeof window !== 'undefined' && window.calypsoOpenOptions)
			? (window.calypsoOpenOptions($0), 1) : 0;
	}, (int)_origin);
	if (handled) return;
#endif
	if (_origin == OPT_GEOSCAPE)
	{
		_game->pushState(new OptionsGeoscapeState(_origin));
	}
	else if (_origin == OPT_BATTLESCAPE)
	{
		_game->pushState(new OptionsBattlescapeState(_origin));
	}
	else
	{
		_game->pushState(new OptionsVideoState(_origin));
	}
}

/**
 * Opens the Abandon Game window.
 * @param action Pointer to an action.
 */
void PauseState::btnAbandonClick(Action *)
{
#ifdef __EMSCRIPTEN__
	// F33: AbandonGameState renders on the canvas, so the DOM pause overlay
	// must hide first — otherwise the confirm dialog appears behind it. The
	// overlay comes back via think() when this state is top again (cancel).
	Calypso::pauseMenuDomHide();
#endif
	_game->pushState(new AbandonGameState(_origin));
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void PauseState::btnCancelClick(Action *)
{
#ifdef __EMSCRIPTEN__
	// Hide before popState — the destructor runs on a later frame.
	Calypso::pauseMenuDomHide();
#endif
	_game->popState();
}

}
