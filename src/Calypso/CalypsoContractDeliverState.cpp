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
#include "CalypsoBaseLedger.h"
#include <algorithm>
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
	_screen = true;
	_window   = new Window(this, 240, 160, 40, 20, POPUP_BOTH);
	_txtTitle = new Text(220, 17, 50, 30);
	_txtInfo  = new Text(220, 9, 50, 48);
	_lstBases = new TextList(200, 72, 50, 62);
	_btnDeliver = new TextButton(70, 16, 16, 138);
	_btnCancel= new TextButton(140, 16, 90, 138);

	// Set palette
	setInterface("fundingWindow");
	add(_window,    "window", "fundingWindow");
	add(_txtTitle,  "text1",  "fundingWindow");
	add(_txtInfo,   "text2",  "fundingWindow");
	add(_lstBases,  "list",   "fundingWindow");
	add(_btnDeliver, "button", "fundingWindow");
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

	_btnDeliver->setText(tr("STR_CAL_DELIVER"));
	_btnDeliver->onMouseClick((ActionHandler)&CalypsoContractDeliverState::btnDeliverClick);
	_btnDeliver->setVisible(false);
	_lstBases->setColumns(3, 100, 50, 50);
	_lstBases->setSelectable(true);
	_lstBases->setBackground(_window);
	_lstBases->onMouseClick((ActionHandler)&CalypsoContractDeliverState::lstBaseClick);
	if (auto *eco = _game->getSavedGame()->getCalypsoEconomy())
		eco->ensureInitialized(_game->getSavedGame(), _game->getMod());
	refresh();
	_hdLedger = new Calypso::CalypsoBaseLedger(*this);
}

/**
 * Initialises the screen -- rebuilds the eligible-bases list.
 */
void CalypsoContractDeliverState::init()
{
	State::init();
	refresh();
}

const Calypso::Contract *CalypsoContractDeliverState::currentContract() const
{
	auto *save = _game->getSavedGame();
	auto *eco = save->getCalypsoEconomy();
	if (!eco || !eco->active()) return nullptr;
	for (const auto &contract : eco->getContracts())
		if (contract.id == _contractId && contract.status == Calypso::Contract::Status::Accepted
			&& contract.qty > 0 && contract.deadlineMonth > save->getMonthsPassed())
			return &contract;
	return nullptr;
}

void CalypsoContractDeliverState::refresh()
{
	_selected = nullptr;
	_btnDeliver->setVisible(false);
	_eligible.clear();
	_lstBases->clearList();
	const auto *contract = currentContract();
	_quote = contract ? *contract : Calypso::Contract{};
	if (!contract)
	{
		_notice = "STR_CAL_CONTRACT_UNAVAILABLE";
		_txtTitle->setText(tr(_notice));
		return;
	}
	for (auto *base : *_game->getSavedGame()->getBases())
	{
		const int have = base->getStorageItems()->getItem(contract->itemId);
		if (have < contract->qty) continue;
		_lstBases->addRow(3, base->getName().c_str(), std::to_string(have).c_str(),
			std::to_string(have - contract->qty).c_str());
		_eligible.push_back(base);
	}
	_notice = _eligible.empty() ? "STR_CAL_ECON_DELIVER_NONE" : "";
	_txtTitle->setText(std::string(tr("STR_CAL_ECON_DELIVER_TITLE")) + " · " + std::string(tr(contract->itemId)));
}

void CalypsoContractDeliverState::showReview()
{
	auto *save = _game->getSavedGame();
	auto *eco = save->getCalypsoEconomy();
	_held = _selected->getStorageItems()->getItem(_quote.itemId);
	_funds = save->getFunds();
	_standing = eco->getStanding(_quote.countryId);
	_standingAfter = Calypso::clampStanding(_standing + eco->rules().onContractDelivered);
	_lstBases->clearList();
	auto row = [&](const std::string &label, const std::string &before, const std::string &after)
	{
		_lstBases->addRow(3, label.c_str(), before.c_str(), after.c_str());
	};
	row(tr(_quote.itemId), std::to_string(_held), std::to_string(_held - _quote.qty));
	row(tr("STR_CAL_FUNDS"), Unicode::formatFunding(_funds), Unicode::formatFunding(_funds + _quote.rewardTotal));
	row(tr(_quote.countryId), std::to_string(_standing), std::to_string(_standingAfter));
	_txtTitle->setText(std::string(tr("STR_CAL_ECON_DELIVER_TITLE")) + " · " + _selected->getName());
	_btnDeliver->setVisible(true);
}

/**
 * Returns to the previous screen without delivering.
 * @param action Pointer to an action.
 */
void CalypsoContractDeliverState::btnCancelClick(Action *)
{
	_game->popState();
}

void CalypsoContractDeliverState::lstBaseClick(Action *)
{
	if (_selected) return;
	const size_t index = _lstBases->getSelectedRow();
	if (index >= _eligible.size()) return;
	auto *base = _eligible[index];
	auto *bases = _game->getSavedGame()->getBases();
	const auto *contract = currentContract();
	if (!contract || std::find(bases->begin(), bases->end(), base) == bases->end()
		|| base->getStorageItems()->getItem(contract->itemId) < contract->qty)
	{
		refresh();
		return;
	}
	_selected = base;
	_quote = *contract;
	showReview();
}

void CalypsoContractDeliverState::btnDeliverClick(Action *)
{
	if (!_selected) return;
	auto *save = _game->getSavedGame();
	auto *eco = save->getCalypsoEconomy();
	const auto *c = currentContract();
	const auto &bases = *save->getBases();
	if (c && std::find(bases.begin(), bases.end(), _selected) != bases.end()
		&& c->itemId == _quote.itemId && c->countryId == _quote.countryId
		&& c->qty == _quote.qty && c->rewardTotal == _quote.rewardTotal
		&& c->deadlineMonth == _quote.deadlineMonth && save->getFunds() == _funds
		&& eco->getStanding(c->countryId) == _standing
		&& Calypso::clampStanding(_standing + eco->rules().onContractDelivered) == _standingAfter
		&& _selected->getStorageItems()->getItem(c->itemId) == _held
		&& eco->deliver(_contractId, _selected, save, eco->rules()))
	{
		_selected = nullptr;
		_game->popState();
		return;
	}
	refresh();
	if (_notice.empty()) _txtTitle->setText(tr("STR_CAL_DELIVERY_CHANGED"));
}

CalypsoContractDeliverState::~CalypsoContractDeliverState()
{
	delete _hdLedger;
}

void CalypsoContractDeliverState::resize(int &dX, int &dY)
{
	if (_hdLedger) _hdLedger->refresh();
	else State::resize(dX, dY);
}

}

#endif /* __EMSCRIPTEN__ */
