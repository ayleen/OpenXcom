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
#include "PromotionsState.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoStrategicList.h"
#endif
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/Transfer.h"
#include "../Engine/Options.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Promotions screen.
 * @param game Pointer to the core game.
 */
PromotionsState::PromotionsState()
{
	// Create object
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(288, 16, 16, 176);
	_txtTitle = new Text(300, 17, 10, 8);
	_txtName = new Text(114, 9, 16, 32);
	_txtRank = new Text(90, 9, 130, 32);
	_txtBase = new Text(80, 9, 220, 32);
	_lstSoldiers = new TextList(288, 128, 8, 40);

	// Set palette
	setInterface("promotions");

	add(_window, "window", "promotions");
	add(_btnOk, "button", "promotions");
	add(_txtTitle, "heading", "promotions");
	add(_txtName, "text", "promotions");
	add(_txtRank, "text", "promotions");
	add(_txtBase, "text", "promotions");
	add(_lstSoldiers, "list", "promotions");

	centerAllSurfaces();

	// Set up object
#ifndef __EMSCRIPTEN__
	setWindowBackground(_window, "promotions");
#endif

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&PromotionsState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&PromotionsState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&PromotionsState::btnOkClick, Options::keyCancel);

	_txtTitle->setText(tr("STR_PROMOTIONS"));
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();

	_txtName->setText(tr("STR_NAME"));

	_txtRank->setText(tr("STR_NEW_RANK"));

	_txtBase->setText(tr("STR_BASE"));

	_lstSoldiers->setColumns(3, 114, 90, 84);
	_lstSoldiers->setSelectable(true);
	_lstSoldiers->setBackground(_window);
	_lstSoldiers->setMargin(8);

	for (auto* xbase : *_game->getSavedGame()->getBases())
	{
		for (auto* soldier : *xbase->getSoldiers())
		{
			if (soldier->isPromoted())
			{
				_lstSoldiers->addRow(3, soldier->getName().c_str(), tr(soldier->getRankString()).c_str(), xbase->getName().c_str());
			}
		}
		for (auto* transfer : *xbase->getTransfers())
		{
			if (transfer->getType() == TRANSFER_SOLDIER)
			{
				if (transfer->getSoldier()->isPromoted())
				{
					_lstSoldiers->addRow(3, transfer->getSoldier()->getName().c_str(), tr(transfer->getSoldier()->getRankString()).c_str(), xbase->getName().c_str());
				}
			}
		}
	}
#ifdef __EMSCRIPTEN__
	for (Text *header : {_txtName, _txtRank, _txtBase}) header->setVisible(false);
	_hdList = new Calypso::CalypsoStrategicList(this, nullptr, _game->getMod(), _window, _txtTitle,
		_lstSoldiers, _btnOk, Calypso::CalypsoStrategicList::Content::Promotions);
	enableUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	_hdList->fitInput();
#endif
}

PromotionsState::~PromotionsState()
{
#ifdef __EMSCRIPTEN__
	delete _hdList;
#endif
}

#ifdef __EMSCRIPTEN__
void PromotionsState::resize(int &dx, int &dy)
{
	if (!_hdList) { State::resize(dx, dy); return; }
	_hdList->layout();
	recaptureUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	_hdList->fitInput();
}
#endif

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void PromotionsState::btnOkClick(Action *)
{
	_game->popState();
}

}
