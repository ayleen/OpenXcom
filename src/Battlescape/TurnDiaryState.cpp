/*
 * Copyright 2010-2019 OpenXcom Developers.
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
#include "TurnDiaryState.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoStrategicList.h"
#include "../Calypso/CalypsoReportLines.h"
#endif
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Mod/Mod.h"
#include "../Savegame/HitLog.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/SavedGame.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Turn Diary window.
 */
TurnDiaryState::TurnDiaryState(const HitLog *hitLog)
{
#ifdef __EMSCRIPTEN__
	_screen = true;
	_window = new Window(this, 216, 160, 52, 20);
#else
	_screen = false;

	_window = new Window(this, 216, 160, 52, 20, POPUP_BOTH);
#endif
	_txtTitle = new Text(206, 17, 57, 32);
	_btnCancel = new TextButton(140, 16, 90, 156);
	_lstTurnDiary = new TextList(180, 96, 65, 52);

	// Set palette
	_game->getSavedGame()->getSavedBattle()->setPaletteByDepth(this);

	add(_window, "messageWindowBorder", "battlescape");
	add(_txtTitle, "messageWindows", "battlescape");
	add(_btnCancel, "messageWindowButtons", "battlescape");
	add(_lstTurnDiary, "optionLists", "battlescape");

	centerAllSurfaces();

	// Set up objects
	_window->setHighContrast(true);
#ifndef __EMSCRIPTEN__
	_window->setBackground(_game->getMod()->getSurface("TAC00.SCR"));
#endif

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();
	_txtTitle->setHighContrast(true);
	_txtTitle->setText(tr("STR_HIT_LOG"));

	_btnCancel->setText(tr("STR_CANCEL_UC"));
	_btnCancel->setHighContrast(true);
	_btnCancel->onMouseClick((ActionHandler)&TurnDiaryState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&TurnDiaryState::btnCancelClick, Options::keyCancel);

	_lstTurnDiary->setColumns(1, 172);
	_lstTurnDiary->setSelectable(true);
	_lstTurnDiary->setBackground(_window);
	_lstTurnDiary->setMargin(8);
	_lstTurnDiary->setAlign(ALIGN_CENTER);
	_lstTurnDiary->setHighContrast(true);
	_lstTurnDiary->setWordWrap(true);

#ifdef __EMSCRIPTEN__
	_entries.push_back(hitLog->getHitLogText(true));
	const auto &history = hitLog->getTurnDiary();
	_entries.insert(_entries.end(), history.rbegin(), history.rend());
	_hdList = new Calypso::CalypsoStrategicList(this, nullptr, _game->getMod(), _window,
		_txtTitle, _lstTurnDiary, _btnCancel, Calypso::CalypsoStrategicList::Content::TurnDiary);
	layoutDiary(true);
#else
	// in reverse order
	_lstTurnDiary->addRow(1, hitLog->getHitLogText(true).c_str());
	size_t row = 1;
	for (std::vector<std::string>::const_reverse_iterator i = hitLog->getTurnDiary().rbegin(); i != hitLog->getTurnDiary().rend(); ++i)
	{
		_lstTurnDiary->addRow(1, (*i).c_str());
		if (row % 2 != 0)
		{
			_lstTurnDiary->setRowColor(row, _lstTurnDiary->getSecondaryColor());
		}
		++row;
	}
#endif
}

TurnDiaryState::~TurnDiaryState()
{
#ifdef __EMSCRIPTEN__
	delete _hdList;
#endif
}

#ifdef __EMSCRIPTEN__
void TurnDiaryState::layoutDiary(bool initial)
{
	const auto scroll = _lstTurnDiary->getScroll();
	if (!initial) _hdList->layout();
	_lstTurnDiary->clearList();
	_lstTurnDiary->setWordWrap(false);
	for (const auto &entry : _entries)
		for (const auto &line : Calypso::calypsoWrapReportText(*_game->getMod(), entry, _hdList->firstColumnWidth(), _hdList->bodyFontSize()))
			_lstTurnDiary->addRow(1, line.c_str());
	if (initial) enableUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	else recaptureUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	_hdList->fitInput();
	_lstTurnDiary->scrollTo(scroll);
}

void TurnDiaryState::resize(int &dX, int &dY)
{
	if (_hdList) layoutDiary(false);
	else State::resize(dX, dY);
}
#endif

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void TurnDiaryState::btnCancelClick(Action *)
{
	_game->popState();
}

}
