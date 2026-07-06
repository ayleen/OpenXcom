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
#include "CalypsoEconomyState.h"
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
#include "../Engine/Options.h"
#include "CalypsoEconomy.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Calypso Economy screen.
 */
CalypsoEconomyState::CalypsoEconomyState() : _tab(0), _tabGroupHack(nullptr)
{
	_screen = false;

	// Create objects
	_window       = new Window(this, 320, 200, 0, 0, POPUP_BOTH);
	_txtTitle     = new Text(320, 17, 0, 8);
	_btnGrants    = new TextButton(96, 14,   8, 26);
	_btnContracts = new TextButton(96, 14, 112, 26);
	_btnStanding  = new TextButton(96, 14, 216, 26);
	_lstMain      = new TextList(288, 120, 16, 46);
	_btnOk        = new TextButton(100, 16, 110, 178);

	// Set palette
	setInterface("fundingWindow");

	add(_window,       "window", "fundingWindow");
	add(_txtTitle,     "text1",  "fundingWindow");
	add(_btnGrants,    "button", "fundingWindow");
	add(_btnContracts, "button", "fundingWindow");
	add(_btnStanding,  "button", "fundingWindow");
	add(_lstMain,      "list",   "fundingWindow");
	add(_btnOk,        "button", "fundingWindow");

	centerAllSurfaces();

	// Set up objects
	setWindowBackground(_window, "fundingWindow");

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_CAL_ECON_TITLE"));

	// Tab buttons share a group so the active one stays visually pressed.
	_btnGrants->setText(tr("STR_CAL_ECON_TAB_GRANTS"));
	_btnGrants->onMouseClick((ActionHandler)&CalypsoEconomyState::btnGrantsClick);
	_btnContracts->setText(tr("STR_CAL_ECON_TAB_CONTRACTS"));
	_btnContracts->onMouseClick((ActionHandler)&CalypsoEconomyState::btnContractsClick);
	_btnStanding->setText(tr("STR_CAL_ECON_TAB_STANDING"));
	_btnStanding->onMouseClick((ActionHandler)&CalypsoEconomyState::btnStandingClick);

	// Grants starts pressed: seed the group holder before wiring the group.
	_tabGroupHack = _btnGrants;
	_btnGrants->setGroup(&_tabGroupHack);
	_btnContracts->setGroup(&_tabGroupHack);
	_btnStanding->setGroup(&_tabGroupHack);

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&CalypsoEconomyState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&CalypsoEconomyState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&CalypsoEconomyState::btnOkClick, Options::keyCancel);

	_lstMain->setColumns(3, 150, 80, 58);
	_lstMain->setDot(true);
	_lstMain->setSelectable(true);
	_lstMain->setBackground(_window);
	_lstMain->onMouseClick((ActionHandler)&CalypsoEconomyState::lstMainClick);
}

/**
 * Initialises the screen -- repopulates the active tab.
 */
void CalypsoEconomyState::init()
{
	State::init();

	showTab(_tab);
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void CalypsoEconomyState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Switches to the Grants tab.
 * @param action Pointer to an action.
 */
void CalypsoEconomyState::btnGrantsClick(Action *)
{
	_tabGroupHack = _btnGrants;
	showTab(0);
}

/**
 * Switches to the Contracts tab.
 * @param action Pointer to an action.
 */
void CalypsoEconomyState::btnContractsClick(Action *)
{
	_tabGroupHack = _btnContracts;
	showTab(1);
}

/**
 * Switches to the Standing tab.
 * @param action Pointer to an action.
 */
void CalypsoEconomyState::btnStandingClick(Action *)
{
	_tabGroupHack = _btnStanding;
	showTab(2);
}

/**
 * Switches the active tab and repopulates the list.
 * @param tab 0=Grants, 1=Contracts, 2=Standing.
 */
void CalypsoEconomyState::showTab(int tab)
{
	_tab = tab;
	_lstMain->clearList();
	switch (tab)
	{
	case 0: populateGrants(); break;
	case 1: populateContracts(); break;
	case 2: populateStanding(); break;
	default: break;
	}
}

/**
 * Fills the list with per-country grant payouts for the current month and
 * the remaining grant-schedule months.
 */
void CalypsoEconomyState::populateGrants()
{
	SavedGame *save = _game->getSavedGame();
	const Calypso::EconomyRules &r = _game->getMod()->getCalypsoEconomyRules();
	Calypso::Economy *eco = save->getCalypsoEconomy();
	const int now = save->getMonthsPassed();
	const int scheduleLen = static_cast<int>(r.grantSchedule.size());
	for (auto* c : *save->getCountries())
	{
		const std::string &id = c->getRules()->getType();
		int grant = eco ? eco->grantForMonth(id, now, r) : 0;
		int monthsLeft = scheduleLen - now;
		if (monthsLeft < 0) monthsLeft = 0;
		std::ostringstream ssG;
		ssG << Unicode::formatFunding(grant);
		std::ostringstream ssM;
		ssM << monthsLeft;
		_lstMain->addRow(3, tr(id).c_str(), ssG.str().c_str(), ssM.str().c_str());
	}
}

/**
 * Maps a standing tier to its translation key.
 */
static const char* tierKey(Calypso::StandingTier t)
{
	switch (t)
	{
		case Calypso::StandingTier::Hostile:    return "STR_CAL_ECON_TIER_HOSTILE";
		case Calypso::StandingTier::Distrusted: return "STR_CAL_ECON_TIER_DISTRUSTED";
		case Calypso::StandingTier::Neutral:    return "STR_CAL_ECON_TIER_NEUTRAL";
		case Calypso::StandingTier::Preferred:  return "STR_CAL_ECON_TIER_PREFERRED";
		case Calypso::StandingTier::Trusted:    return "STR_CAL_ECON_TIER_TRUSTED";
	}
	return "STR_CAL_ECON_TIER_NEUTRAL";
}

/**
 * Fills the list with per-country contracts: Offered rows can be Accepted with
 * a click; Accepted rows are placeholders for the Deliver flow (38.3c).
 */
void CalypsoEconomyState::populateContracts()
{
	SavedGame *save = _game->getSavedGame();
	Calypso::Economy *eco = save->getCalypsoEconomy();
	_contractRowId.clear();
	if (!eco) return;
	for (const Calypso::Contract& c : eco->getContracts())
	{
		if (c.status != Calypso::Contract::Status::Offered &&
		    c.status != Calypso::Contract::Status::Accepted)
			continue;
		const char* statusKey = (c.status == Calypso::Contract::Status::Offered)
			? "STR_CAL_ECON_CONTRACT_OFFERED" : "STR_CAL_ECON_CONTRACT_ACCEPTED";
		std::ostringstream col1; col1 << tr(c.countryId) << " - " << tr(c.itemId) << " x" << c.qty;
		std::ostringstream col2; col2 << Unicode::formatFunding(c.rewardTotal);
		_lstMain->addRow(3, col1.str().c_str(), col2.str().c_str(), tr(statusKey).c_str());
		_contractRowId.push_back(c.id);
	}
}

/**
 * Fills the list with each country's standing value + tier label. No trend
 * arrow: per-month standing history is not tracked yet (slice C).
 */
void CalypsoEconomyState::populateStanding()
{
	SavedGame *save = _game->getSavedGame();
	const Calypso::EconomyRules &r = _game->getMod()->getCalypsoEconomyRules();
	Calypso::Economy *eco = save->getCalypsoEconomy();
	if (!eco) return;
	for (auto* c : *save->getCountries())
	{
		const std::string &id = c->getRules()->getType();
		int standing = eco->getStanding(id);
		Calypso::StandingTier tier = eco->getTier(id, r);
		std::ostringstream ssV; ssV << standing;
		_lstMain->addRow(3, tr(id).c_str(), tr(tierKey(tier)).c_str(), ssV.str().c_str());
	}
}

/**
 * Row-click dispatcher. Acts only on the Contracts tab: an Offered row is
 * Accepted in place; an Accepted row is a no-op (deliver flow lands in 38.3c).
 */
void CalypsoEconomyState::lstMainClick(Action *)
{
	if (_tab != 1) return;                              // Contracts tab only
	size_t row = _lstMain->getSelectedRow();
	if (row >= _contractRowId.size()) return;
	int id = _contractRowId[row];
	Calypso::Economy *eco = _game->getSavedGame()->getCalypsoEconomy();
	if (!eco) return;
	// Find the contract to branch on its status.
	for (const Calypso::Contract& c : eco->getContracts())
	{
		if (c.id != id) continue;
		if (c.status == Calypso::Contract::Status::Offered)
		{
			eco->accept(id);
			showTab(1);                                 // refresh: now Accepted
		}
		else if (c.status == Calypso::Contract::Status::Accepted)
		{
			_game->pushState(new CalypsoContractDeliverState(id));
		}
		return;
	}
}

}

#endif /* __EMSCRIPTEN__ */
