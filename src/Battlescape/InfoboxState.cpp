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
#include "InfoboxState.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoContactNotice.h"
#include "../Interface/Window.h"
#include "../Interface/TextButton.h"
#include "../Engine/Options.h"
#endif
#include "../Engine/Game.h"
#include "../Engine/Timer.h"
#include "../Interface/Text.h"
#include "../Interface/Frame.h"
#include "../Engine/Action.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"

namespace OpenXcom
{

/**
 * Initializes all the elements.
 * @param game Pointer to the core game.
 * @param msg Message string.
 */
InfoboxState::InfoboxState(const std::string &msg)
{
	_screen = false;

	// Create objects
	_frame = new Frame(261, 122, 34, 10);
	_text = new Text(251, 112, 39, 15);

	// Set palette
	_game->getSavedGame()->getSavedBattle()->setPaletteByDepth(this);

	add(_frame, "infoBox", "battlescape");
	add(_text, "infoBoxText", "battlescape");

	centerAllSurfaces();

	_frame->setHighContrast(true);
	_frame->setThickness(9);

	_text->setAlign(ALIGN_CENTER);
	_text->setVerticalAlign(ALIGN_MIDDLE);
	_text->setBig();
	_text->setWordWrap(true);
	_text->setText(msg);
	_text->setHighContrast(true);

	int delay = INFOBOX_DELAY;
	if (msg.empty())
	{
		delay = 500;
		_frame->setVisible(false);
		_text->setVisible(false);
	}

	_timer = new Timer(delay);
	_timer->onTimer((StateHandler)&InfoboxState::close);
	_timer->start();
#ifdef __EMSCRIPTEN__
	_frame->setVisible(false);
	if (msg.empty()) return;
	_screen = true;
	auto *window = new Window(this, 1, 1, 0, 0);
	add(window, "messageWindowBorder", "battlescape");
	auto *button = new TextButton(1, 1, 0, 0);
	add(button, "infoBoxOKButton", "battlescape");
	button->setText(tr("STR_OK"));
	button->onMouseClick((ActionHandler)&InfoboxState::dismiss);
	button->onKeyboardPress((ActionHandler)&InfoboxState::dismiss, Options::keyOk);
	button->onKeyboardPress((ActionHandler)&InfoboxState::dismiss, Options::keyCancel);
	_hdNotice = new Calypso::CalypsoContactNotice(Calypso::ContactNoticeType::TacticalNotice, this, nullptr,
		_game->getMod(), window, _text, button, tr("STR_CAL_BATTLE_MESSAGE"));
	enableUiScaling(_hdNotice->designWidth(), _hdNotice->designHeight(), 1.0f, false);
#endif
}

/**
 *
 */
InfoboxState::~InfoboxState()
{
#ifdef __EMSCRIPTEN__
	delete _hdNotice;
#endif
	delete _timer;
}

/**
 * Closes the window.
 * @param action Pointer to an action.
 */
void InfoboxState::handle(Action *action)
{
	State::handle(action);
#ifdef __EMSCRIPTEN__
	return; // Only the acknowledgement and the original timer dismiss the HD notice.
#endif

	if (action->getDetails()->type == SDL_KEYDOWN || action->getDetails()->type == SDL_MOUSEBUTTONDOWN)
	{
		close();
	}
}

/**
 * Keeps the animation timers running.
 */
void InfoboxState::think()
{
	_timer->think(this, 0);
}

/**
 * Closes the window.
 */
void InfoboxState::close()
{
	if (!_game->isState(this)) return;
	_game->popState();
}

#ifdef __EMSCRIPTEN__
void InfoboxState::resize(int &dx, int &dy)
{
	if (!_hdNotice) { State::resize(dx, dy); return; }
	_hdNotice->layout();
	recaptureUiScaling(_hdNotice->designWidth(), _hdNotice->designHeight(), 1.0f, false);
}
void InfoboxState::dismiss(Action *) { close(); }
#endif

}
