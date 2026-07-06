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
#include "CalypsoMarketState.h"
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
#include "../Basescape/PurchaseState.h"
#include "../Basescape/SellState.h"

namespace OpenXcom
{

/**
 * tier -> display string key (same mapping as the Economy screen).
 */
static const char* tierKey(Calypso::StandingTier t)
{
	switch (t) {
		case Calypso::StandingTier::Hostile:    return "STR_CAL_ECON_TIER_HOSTILE";
		case Calypso::StandingTier::Distrusted: return "STR_CAL_ECON_TIER_DISTRUSTED";
		case Calypso::StandingTier::Neutral:    return "STR_CAL_ECON_TIER_NEUTRAL";
		case Calypso::StandingTier::Preferred:  return "STR_CAL_ECON_TIER_PREFERRED";
		case Calypso::StandingTier::Trusted:    return "STR_CAL_ECON_TIER_TRUSTED";
	}
	return "STR_CAL_ECON_TIER_NEUTRAL";
}

/**
 * Initializes all the elements in the Calypso Market (counterparty picker) screen.
 * @param base Pointer to the base the Buy/Sell was invoked from.
 * @param sellMode false = Buy (-> PurchaseState), true = Sell (-> SellState).
 */
CalypsoMarketState::CalypsoMarketState(Base* base, bool sellMode) : _base(base), _sellMode(sellMode)
{
	_screen = false;
	_window   = new Window(this, 288, 180, 16, 10, POPUP_BOTH);
	_txtTitle = new Text(268, 17, 26, 20);
	_txtInfo  = new Text(268, 9, 26, 38);
	_lstCounterparties = new TextList(248, 96, 26, 52);
	_btnCancel= new TextButton(160, 16, 80, 158);

	setInterface("fundingWindow");
	add(_window,             "window", "fundingWindow");
	add(_txtTitle,           "text1",  "fundingWindow");
	add(_txtInfo,            "text2",  "fundingWindow");
	add(_lstCounterparties,  "list",   "fundingWindow");
	add(_btnCancel,          "button", "fundingWindow");
	centerAllSurfaces();
	setWindowBackground(_window, "fundingWindow");

	_txtTitle->setAlign(ALIGN_CENTER); _txtTitle->setBig();
	_txtTitle->setText(tr(_sellMode ? "STR_CAL_MARKET_TITLE_SELL" : "STR_CAL_MARKET_TITLE_BUY"));
	_txtInfo->setAlign(ALIGN_CENTER);
	_txtInfo->setText(tr("STR_CAL_MARKET_PICK"));
	_btnCancel->setText(tr("STR_CANCEL"));
	_btnCancel->onMouseClick((ActionHandler)&CalypsoMarketState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&CalypsoMarketState::btnCancelClick, Options::keyCancel);

	_lstCounterparties->setColumns(2, 170, 78);
	_lstCounterparties->setSelectable(true);
	_lstCounterparties->setBackground(_window);
	_lstCounterparties->onMouseClick((ActionHandler)&CalypsoMarketState::lstCounterpartyClick);
}

/**
 * Initialises the screen -- repopulates the counterparty list.
 */
void CalypsoMarketState::init() { State::init(); refresh(); }

/**
 * List all conglomerates (id + standing tier) in ruleset order, then the always-open black market.
 */
void CalypsoMarketState::refresh()
{
	_lstCounterparties->clearList();
	_rowCp.clear();
	const Calypso::EconomyRules& r = _game->getMod()->getCalypsoEconomyRules();
	Calypso::Economy* eco = _game->getSavedGame()->getCalypsoEconomy();
	if (!eco) return;
	for (const Calypso::CounterpartyRules& cp : r.counterparties)
	{
		Calypso::StandingTier tier = eco->getTier(cp.country, r);
		_lstCounterparties->addRow(2, tr(cp.country).c_str(), tr(tierKey(tier)).c_str());
		_rowCp.push_back(cp.country);
	}
	// Black market row -- always available.
	_lstCounterparties->addRow(2, tr("STR_CAL_MARKET_BLACKMARKET").c_str(), tr("STR_CAL_MARKET_OPEN").c_str());
	_rowCp.push_back(Calypso::BLACK_MARKET);
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void CalypsoMarketState::btnCancelClick(Action*) { _game->popState(); }

/**
 * Row click: open Purchase/SellState parameterized with the picked counterparty id.
 * @param action Pointer to an action.
 */
void CalypsoMarketState::lstCounterpartyClick(Action*)
{
	size_t row = _lstCounterparties->getSelectedRow();
	if (row >= _rowCp.size()) return;
	const std::string& cp = _rowCp[row];
	if (_sellMode)
		_game->pushState(new SellState(_base, 0, OPT_GEOSCAPE, cp));
	else
		_game->pushState(new PurchaseState(_base, nullptr, cp));
}

}

#endif /* __EMSCRIPTEN__ */
