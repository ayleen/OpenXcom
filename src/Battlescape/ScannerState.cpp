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
#include "ScannerState.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoTacticalSensor.h"
#endif
#include "ScannerView.h"
#include "BattlescapeGame.h"
#include "../Engine/InteractiveSurface.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Engine/Timer.h"
#include "../Engine/Screen.h"
#include "../Engine/Options.h"
#include "../Engine/TTFUtil.h"
#include "../Savegame/BattleUnit.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"

namespace OpenXcom
{

/**
 * Initializes the Scanner State.
 * @param game Pointer to the core game.
 * @param action Pointer to an action.
 */
ScannerState::ScannerState (BattleAction *action) : _action(action)
{
#ifdef __EMSCRIPTEN__
	_screen = true;
	auto *battle = _game->getSavedGame()->getSavedBattle();
	battle->setPaletteByDepth(this);
	_scannerView = new ScannerView(152,152,0,0,_game,_action->actor);
	add(_scannerView);
	_scannerView->setVisible(false);
	_sensor = new Calypso::CalypsoTacticalSensor(*this,nullptr,_scannerView,nullptr,*battle,
		(ActionHandler)&ScannerState::sensorClick,tr("STR_CAL_SENSOR_TITLE"));
#else
	if (Options::maximizeInfoScreens)
	{
		Options::baseXResolution = Screen::ORIGINAL_WIDTH;
		Options::baseYResolution = Screen::ORIGINAL_HEIGHT;
		_game->getScreen()->resetDisplay(false);
	}
	_bg = new InteractiveSurface(320, 200);
	_scan = new Surface(320, 200);
	_scannerView = new ScannerView(152, 152, 56, 24, _game, _action->actor);

	if (_game->getScreen()->getDY() > 50)
	{
		_screen = false;
	}

	// Set palette
	_game->getSavedGame()->getSavedBattle()->setPaletteByDepth(this);

	add(_scan);
	add(_scannerView);
	add(_bg);

	centerAllSurfaces();

	_game->getMod()->getSurface("DETBORD.PCK")->blitNShade(_bg, 0, 0);
	_game->getMod()->getSurface("DETBORD2.PCK")->blitNShade(_scan, 0, 0);



	_bg->onMouseClick((ActionHandler)&ScannerState::exitClick);
	_bg->onKeyboardPress((ActionHandler)&ScannerState::exitClick, Options::keyCancel);



	update();
#endif
	_timerAnimate = new Timer(125);
	_timerAnimate->onTimer((StateHandler)&ScannerState::animate);
	_timerAnimate->start();
}

ScannerState::~ScannerState()
{
	delete _timerAnimate;
#ifdef __EMSCRIPTEN__
	delete _sensor;
#endif
}

/**
 * Calypso (Emscripten): rescale to the logical buffer and re-stretch both
 * backgrounds, so resolution changes keep the scanner popup filled and aligned.
 */
void ScannerState::resize(int &dX, int &dY)
{
#ifdef __EMSCRIPTEN__
	if (_sensor) _sensor->layout();
#else
	State::resize(dX, dY);
#endif
}

/**
 * Closes the window on right-click.
 * @param action Pointer to an action.
 */
void ScannerState::handle(Action *action)
{
	State::handle(action);
	if (action->getDetails()->type == SDL_MOUSEBUTTONDOWN && _game->isRightClick(action))
	{
		exitClick(action);
	}
}

/**
 * Updates scanner state.
 */
void ScannerState::update()
{
	//_scannerView->draw();
}

/**
 * Animation handler. Updates the minimap view animation.
 */
void ScannerState::animate()
{
	_scannerView->animate();
#ifdef __EMSCRIPTEN__
	_sensor->refresh();
#endif
}

/**
 * Handles timers.
 */
void ScannerState::think()
{
	State::think();
	_timerAnimate->think(this, 0);
}

/**
 * Exits the screen.
 * @param action Pointer to an action.
 */
void ScannerState::exitClick(Action *)
{
#ifndef __EMSCRIPTEN__
	if (Options::maximizeInfoScreens)
	{
		Screen::updateScale(Options::battlescapeScale, Options::baseXBattlescape, Options::baseYBattlescape, true);
		_game->getScreen()->resetDisplay(false);
	}
#endif
	_game->popState();
}

}
