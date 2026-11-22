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
#include "TransfersState.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoBaseLedger.h"
#endif
#include <sstream>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/Base.h"
#include "../Savegame/Transfer.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Transfers window.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 */
TransfersState::TransfersState(Base *base) : _base(base)
{
#ifdef __EMSCRIPTEN__
	_screen = true;
	_window = new Window(this, 320, 184, 0, 8);
#else
	_screen = false;
	_window = new Window(this, 320, 184, 0, 8, POPUP_BOTH);
#endif
	_btnOk = new TextButton(288, 16, 16, 166);
	_txtTitle = new Text(278, 17, 21, 18);
	_txtItem = new Text(114, 9, 16, 34);
	_txtQuantity = new Text(54, 9, 152, 34);
	_txtArrivalTime = new Text(112, 9, 212, 34);
	_lstTransfers = new TextList(273, 112, 14, 50);

	// Set palette
	setInterface("transferInfo");

	add(_window, "window", "transferInfo");
	add(_btnOk, "button", "transferInfo");
	add(_txtTitle, "text", "transferInfo");
	add(_txtItem, "text", "transferInfo");
	add(_txtQuantity, "text", "transferInfo");
	add(_txtArrivalTime, "text", "transferInfo");
	add(_lstTransfers, "list", "transferInfo");

	centerAllSurfaces();

	// Set up objects
#ifndef __EMSCRIPTEN__
	setWindowBackground(_window, "transferInfo");
#endif

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&TransfersState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&TransfersState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&TransfersState::btnOkClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_TRANSFERS"));

	_txtItem->setText(tr("STR_ITEM"));

	_txtQuantity->setText(tr("STR_QUANTITY_UC"));

	_txtArrivalTime->setText(tr("STR_ARRIVAL_TIME_HOURS"));

	_lstTransfers->setColumns(3, 155, 75, 46);
	_lstTransfers->setSelectable(true);
	_lstTransfers->setBackground(_window);
	_lstTransfers->setMargin(2);

	for (const auto* transfer : *_base->getTransfers())
	{
		if (_nextArrivalHours < 0 || transfer->getHours() < _nextArrivalHours)
			_nextArrivalHours = transfer->getHours();
		std::ostringstream ss, ss2;
		ss << transfer->getQuantity();
		ss2 << transfer->getHours();
		_lstTransfers->addRow(3, transfer->getName(_game->getLanguage()).c_str(), ss.str().c_str(), ss2.str().c_str());
	}
#ifdef __EMSCRIPTEN__
	_hdLedger = new Calypso::CalypsoBaseLedger(*this);
#endif
}

/**
 *
 */
TransfersState::~TransfersState()
{
#ifdef __EMSCRIPTEN__
	delete _hdLedger;
#endif

}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void TransfersState::btnOkClick(Action *)
{
	_game->popState();
}

#ifdef __EMSCRIPTEN__
void TransfersState::resize(int &dX, int &dY)
{
	if (_hdLedger) _hdLedger->refresh();
	else State::resize(dX, dY);
}
#endif

}
