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

#ifdef __EMSCRIPTEN__
	// Calypso: scale this popup to fill the logical buffer (0.5x of fill — it is a
	// small popup like the minimap/medikit per QA). Snapshot both painted 320x200
	// backgrounds, scale every widget (the ScannerView composites native then
	// scale-blits), then bilinear-stretch both backgrounds back in.
	{
		SDL_Surface* sb = _bg->getSurface();
		if (sb && sb->format->BitsPerPixel == 32) _bgNative = SDL_ConvertSurface(sb, sb->format, 0);
		SDL_Surface* ss = _scan->getSurface();
		if (ss && ss->format->BitsPerPixel == 32) _scanNative = SDL_ConvertSurface(ss, ss->format, 0);
	}
	enableUiScaling(320, 200, 0.5f);
	applyTTFToTexts(_game->getMod()->getTTFFont("FONT_HD_HUD", false), 0.92f);
	if (_bgNative)   { TTFUtil::blitStretch(_bgNative, _bg);   _bg->setRedraw(false); }
	if (_scanNative) { TTFUtil::blitStretch(_scanNative, _scan); _scan->setRedraw(false); }
#endif

	_bg->onMouseClick((ActionHandler)&ScannerState::exitClick);
	_bg->onKeyboardPress((ActionHandler)&ScannerState::exitClick, Options::keyCancel);

	_timerAnimate = new Timer(125);
	_timerAnimate->onTimer((StateHandler)&ScannerState::animate);
	_timerAnimate->start();

	update();
}

ScannerState::~ScannerState()
{
	delete _timerAnimate;
#ifdef __EMSCRIPTEN__
	if (_bgNative)   SDL_FreeSurface(_bgNative);
	if (_scanNative) SDL_FreeSurface(_scanNative);
#endif
}

/**
 * Calypso (Emscripten): rescale to the logical buffer and re-stretch both
 * backgrounds, so resolution changes keep the scanner popup filled and aligned.
 */
void ScannerState::resize(int &dX, int &dY)
{
#ifdef __EMSCRIPTEN__
	applyUiScaling();
	if (_bgNative)   { TTFUtil::blitStretch(_bgNative, _bg);   _bg->setRedraw(false); }
	if (_scanNative) { TTFUtil::blitStretch(_scanNative, _scan); _scan->setRedraw(false); }
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
	if (Options::maximizeInfoScreens)
	{
		Screen::updateScale(Options::battlescapeScale, Options::baseXBattlescape, Options::baseYBattlescape, true);
		_game->getScreen()->resetDisplay(false);
	}
	_game->popState();
}

}
