#ifdef __EMSCRIPTEN__
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
#include "CalypsoContractDeliverState.h"
#include <sstream>
#include "../Engine/Game.h"
#include "../Engine/State.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Unicode.h"
#include "../Interface/Window.h"
#include "../Interface/TextButton.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/Country.h"
#include "../Mod/RuleCountry.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/ItemContainer.h"
#include "../Engine/Options.h"
#include "CalypsoEconomy.h"

namespace OpenXcom
{

/**
 * Builds the Deliver modal: a centred fundingWindow-style popup listing every
 * base that can satisfy the contract right now.
 * @param contractId Id of the Accepted contract the player wants to fulfil.
 */
CalypsoContractDeliverState::CalypsoContractDeliverState(int contractId) : _contractId(contractId)
{
	_screen = false;
	_window   = new Window(this, 240, 160, 40, 20, POPUP_BOTH);
	_txtTitle = new Text(220, 17, 50, 30);
	_txtInfo  = new Text(220, 9, 50, 48);
	_lstBases = new TextList(200, 72, 50, 62);
	_btnCancel= new TextButton(140, 16, 90, 138);

	// Set palette
	setInterface("fundingWindow");
	add(_window,    "window", "fundingWindow");
	add(_txtTitle,  "text1",  "fundingWindow");
	add(_txtInfo,   "text2",  "fundingWindow");
	add(_lstBases,  "list",   "fundingWindow");
	add(_btnCancel, "button", "fundingWindow");
	centerAllSurfaces();

	// Set up objects
	setWindowBackground(_window, "fundingWindow");

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_CAL_ECON_DELIVER_TITLE"));
	_txtInfo->setAlign(ALIGN_CENTER);
	_btnCancel->setText(tr("STR_CANCEL"));
	_btnCancel->onMouseClick((ActionHandler)&CalypsoContractDeliverState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&CalypsoContractDeliverState::btnCancelClick, Options::keyCancel);

	_lstBases->setColumns(2, 150, 50);
	_lstBases->setSelectable(true);
	_lstBases->setBackground(_window);
	_lstBases->onMouseClick((ActionHandler)&CalypsoContractDeliverState::lstBaseClick);
}

/**
 * Initialises the screen -- rebuilds the eligible-bases list.
 */
void CalypsoContractDeliverState::init()
{
	State::init();
	refresh();
}

/**
 * Builds the list of bases that hold at least the contract quantity of the
 * requested item. The info line shows the item id + requested qty, or a
 * "nothing in stores anywhere" message if no base qualifies.
 */
void CalypsoContractDeliverState::refresh()
{
	_lstBases->clearList();
	_eligible.clear();
	SavedGame* save = _game->getSavedGame();
	Calypso::Economy* eco = save->getCalypsoEconomy();
	if (!eco) return;
	// Look up the contract (item + qty) by id.
	const Calypso::Contract* found = nullptr;
	for (const Calypso::Contract& c : eco->getContracts())
		if (c.id == _contractId) { found = &c; break; }
	if (!found) { _txtInfo->setText(tr("STR_CAL_ECON_DELIVER_NONE")); return; }

	std::ostringstream info;
	info << tr(found->itemId) << " x" << found->qty;
	_txtInfo->setText(info.str());

	for (auto* b : *save->getBases())
	{
		int have = b->getStorageItems()->getItem(found->itemId);
		if (have < found->qty) continue;
		std::ostringstream ss; ss << have;
		_lstBases->addRow(2, b->getName().c_str(), ss.str().c_str());
		_eligible.push_back(b);
	}
	if (_eligible.empty())
		_txtInfo->setText(tr("STR_CAL_ECON_DELIVER_NONE"));   // nothing in stores anywhere
}

/**
 * Returns to the previous screen without delivering.
 * @param action Pointer to an action.
 */
void CalypsoContractDeliverState::btnCancelClick(Action *)
{
	_game->popState();
}

/**
 * Attempts delivery from the clicked base. On success, pops back to the
 * Economy screen (whose init() refreshes the Contracts list); on failure,
 * rebuilds the list to reflect whatever changed.
 * @param action Pointer to an action.
 */
void CalypsoContractDeliverState::lstBaseClick(Action *)
{
	size_t row = _lstBases->getSelectedRow();
	if (row >= _eligible.size()) return;
	SavedGame* save = _game->getSavedGame();
	Calypso::Economy* eco = save->getCalypsoEconomy();
	if (!eco) return;
	const Calypso::EconomyRules& r = _game->getMod()->getCalypsoEconomyRules();
	if (eco->deliver(_contractId, _eligible[row], save, r))
		_game->popState();          // delivered -> back to the Economy screen (its init() refreshes)
	else
		refresh();                  // race/edge: rebuild the list
}

}

#endif /* __EMSCRIPTEN__ */
