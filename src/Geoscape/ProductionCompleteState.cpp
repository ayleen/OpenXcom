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
#include <assert.h>
#include "ProductionCompleteState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleManufacture.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "GeoscapeState.h"
#include "../Engine/Options.h"
#include "../Basescape/BasescapeState.h"
#include "../Basescape/ManufactureState.h"
#include "../Savegame/Base.h"
#include "../Savegame/ItemContainer.h"
#include "../Mod/RuleItem.h"
#include "../Engine/Unicode.h"
#include "../Savegame/SavedGame.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoBaseLedger.h"
#include "../Calypso/CalypsoMarket.h"
#include "../Calypso/CalypsoMarketState.h"
#include "../Calypso/CalypsoEconomy.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in a Production Complete window.
 * @param game Pointer to the core game.
 * @param base Pointer to base the production belongs to.
 * @param item Item that finished producing.
 * @param state Pointer to the Geoscape state.
 * @param endType What ended the production.
 * @param production Pointer to the production details.
 */
ProductionCompleteState::ProductionCompleteState(Base *base, const std::string &item, GeoscapeState *state, productionProgress_e endType, Production *production) : _base(base), _state(state), _endType(endType)
{
#ifdef __EMSCRIPTEN__
	_screen = true;
	_window = new Window(this, 256, 160, 32, 20);
#else
	_screen = false;
	_window = new Window(this, 256, 160, 32, 20, POPUP_BOTH);
#endif
	_btnOk = new TextButton(118, 18, 40, 154);
	_btnGotoBase = new TextButton(118, 18, 162, 154);
	_btnSummary = new TextButton(118, 18, 162, 154);
	_txtMessage = new Text(246, 110, 37, 35);
	_txtItem = new Text(160, 9, 47, 35);
	_txtQuantity = new Text(70, 9, 209, 35);
	_lstSummary = new TextList(224, 96, 39, 45);

	// Set palette
	setInterface("geoManufactureComplete");

	add(_window, "window", "geoManufactureComplete");
	add(_btnOk, "button", "geoManufactureComplete");
	add(_btnGotoBase, "button", "geoManufactureComplete");
	add(_btnSummary, "button", "geoManufactureComplete");
	add(_txtMessage, "text1", "geoManufactureComplete");
	add(_txtItem, "text1", "geoManufactureComplete");
	add(_txtQuantity, "text1", "geoManufactureComplete");
	add(_lstSummary, "list", "geoManufactureComplete");

	centerAllSurfaces();
	// Set up objects
	setWindowBackground(_window, "geoManufactureComplete");

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ProductionCompleteState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ProductionCompleteState::btnOkClick, Options::keyCancel);

	if (_endType != PROGRESS_CONSTRUCTION)
	{
		_btnGotoBase->setText(tr("STR_ALLOCATE_MANUFACTURE"));
	}
	else
	{
		_btnGotoBase->setText(tr("STR_GO_TO_BASE"));
	}
	_btnGotoBase->onMouseClick((ActionHandler)&ProductionCompleteState::btnGotoBaseClick);

	_btnSummary->setText(tr("STR_RANDOM_PRODUCTION_SUMMARY"));
	_btnSummary->onMouseClick((ActionHandler)&ProductionCompleteState::btnSummaryClick);

	_txtItem->setText(tr("STR_ITEM"));
	_txtItem->setVisible(false);

	_txtQuantity->setText(tr("STR_QUANTITY_UC"));
	_txtQuantity->setVisible(false);

	_lstSummary->setColumns(2, 162, 46);
	_lstSummary->setBackground(_window);
	_lstSummary->setSelectable(true);
	_lstSummary->setMargin(8);
	_lstSummary->setVisible(false);
	_lstSummary->onMouseClick((ActionHandler)&ProductionCompleteState::lstSummaryClick);

	if (production && !production->getRules()->getRandomProducedItems().empty())
	{
		_btnGotoBase->setVisible(false);
		_randomProductionInfo = production->getRandomProductionInfo(); // local copy is necessary

	}
	else
	{
		_btnSummary->setVisible(false);
	}

	_txtMessage->setAlign(ALIGN_CENTER);
	_txtMessage->setVerticalAlign(ALIGN_MIDDLE);
	_txtMessage->setBig();
	_txtMessage->setWordWrap(true);
	std::string s;
	switch(_endType)
	{
	case PROGRESS_CONSTRUCTION:
		s = tr("STR_CONSTRUCTION_OF_FACILITY_AT_BASE_IS_COMPLETE").arg(item).arg(base->getName());
		break;
	case PROGRESS_COMPLETE:
		s = tr("STR_PRODUCTION_OF_ITEM_AT_BASE_IS_COMPLETE").arg(item).arg(base->getName());
		break;
	case PROGRESS_NOT_ENOUGH_MONEY:
		s = tr("STR_NOT_ENOUGH_MONEY_TO_PRODUCE_ITEM_AT_BASE").arg(item).arg(base->getName());
		break;
	case PROGRESS_NOT_ENOUGH_MATERIALS:
		s = tr("STR_NOT_ENOUGH_SPECIAL_MATERIALS_TO_PRODUCE_ITEM_AT_BASE").arg(item).arg(base->getName());
		break;
	case PROGRESS_NOT_ENOUGH_LIVING_SPACE:
		s = tr("STR_NOT_ENOUGH_LIVING_SPACE_AT_BASE").arg(item).arg(base->getName());
		break;
	default:
		assert(false);
	}
	_txtMessage->setText(s);
#ifdef __EMSCRIPTEN__
	_noticeRows = new TextList(224, 96, 39, 45);
	add(_noticeRows, "list", "geoManufactureComplete");
	_noticeRows->setColumns(2, 80, 140);
	_noticeRows->setSelectable(true);
	_ledger = new Calypso::CalypsoBaseLedger(*this);
#endif
}

/**
 *
 */
ProductionCompleteState::~ProductionCompleteState()
{
#ifdef __EMSCRIPTEN__
	delete _ledger;
#endif

}

/**
 * Closes the window.
 * @param action Pointer to an action.
 */
void ProductionCompleteState::btnOkClick(Action *)
{
	if (!_saleItem.empty())
	{
		showOutput();
		return;
	}
	_game->popState();
}

/**
 * Goes to the base for the respective production.
 * @param action Pointer to an action.
 */
void ProductionCompleteState::btnGotoBaseClick(Action *)
{
	_state->timerReset();
	_game->popState();
	if (_endType != PROGRESS_CONSTRUCTION)
	{
		_game->pushState(new ManufactureState(_base));
	}
	else
	{
		_game->pushState(new BasescapeState(_base, _state->getGlobe()));
	}
}

/**
 * Displays the random production info.
 * @param action Pointer to an action.
 */
void ProductionCompleteState::btnSummaryClick(Action *action)
{
	if (!_saleItem.empty()) confirmSale();
	else if (_lstSummary->getVisible()) lstSummaryClick(action);
	else showOutput();
}

void ProductionCompleteState::showOutput()
{
	_saleItem.clear();
	_buyer.clear();
	_index.clear();
	_lstSummary->clearList();
	for (auto &output : _randomProductionInfo)
	{
		const auto *rule = _game->getMod()->getItem(output.first, false);
		if (!rule) continue;
		output.second = std::min(output.second, _base->getStorageItems()->getItem(rule));
		if (output.second <= 0) continue;
		_index.push_back(output.first);
		_lstSummary->addRow(2, tr(output.first).c_str(), std::to_string(output.second).c_str());
	}
	_txtMessage->setVisible(false);
	_txtItem->setVisible(true);
	_txtQuantity->setVisible(true);
	_lstSummary->setVisible(true);
	_lstSummary->setSelectable(true);
	_lstSummary->setSelectedRow(0);
	_btnGotoBase->setVisible(true);
	_btnOk->setText(tr("STR_OK"));
	_btnSummary->setText(tr("STR_CAL_OUTPUT_SELL"));
	_btnSummary->setVisible(!_index.empty());
#ifdef __EMSCRIPTEN__
	_noticeRows->setVisible(false);
	_ledger->refresh();
#else
	const int left = _window->getX() + 8;
	_btnOk->setWidth(76);
	_btnSummary->setWidth(76);
	_btnGotoBase->setWidth(84);
	_btnOk->setX(left);
	_btnSummary->setX(left + 78);
	_btnGotoBase->setX(left + 156);
#endif
}

void ProductionCompleteState::lstSummaryClick(Action *)
{
	const size_t row = _lstSummary->getSelectedRow();
	if (!_saleItem.empty() || row >= _index.size()) return;
	const auto item = _index[row];
#ifdef __EMSCRIPTEN__
	// A new campaign can finish production before the player opens the market.
	if (auto *economy = _game->getSavedGame()->getCalypsoEconomy())
		economy->bindRules(&_game->getMod()->getCalypsoEconomyRules());
	if (Calypso::marketActive(_game))
	{
		_game->pushState(new CalypsoMarketState(_base, true, [this, item](const std::string &buyer) {
			showSale(item, buyer);
		}));
		return;
	}
#endif
	showSale(item, "");
}

ProductionOutputSale ProductionCompleteState::quoteSale() const
{
	const auto it = _randomProductionInfo.find(_saleItem);
	const auto *rule = _game->getMod()->getItem(_saleItem, false);
	if (it == _randomProductionInfo.end() || !rule || (rule->isAlien() && !Options::canSellLiveAliens)) return {};
	int demand = it->second;
	int64_t price = rule->getSellCostAdjusted(_base, _game->getSavedGame());
#ifdef __EMSCRIPTEN__
	if (Calypso::marketActive(_game))
	{
		if (_buyer.empty() || !Calypso::marketCanSell(_game, _buyer, rule)) return {};
		demand = Calypso::marketDemand(_game, _buyer, rule);
		price = Calypso::marketSellPrice(_game, _buyer, rule);
	}
	else if (!_buyer.empty()) return {};
#endif
	return ProductionOutputSale::quote(it->second, _base->getStorageItems()->getItem(rule), demand, price,
		_game->getSavedGame()->getFunds());
}

void ProductionCompleteState::showSale(const std::string &item, const std::string &buyer)
{
	_saleItem = item;
	_buyer = buyer;
	_shownSale = quoteSale();
	_lstSummary->clearList();
	_lstSummary->setSelectable(false);
	auto row = [&](const char *label, const std::string &value) {
		_lstSummary->addRow(2, tr(label).c_str(), value.c_str());
	};
	row("STR_ITEM", tr(item));
#ifdef __EMSCRIPTEN__
	if (!buyer.empty()) row("STR_CAL_OUTPUT_BUYER", tr(buyer == Calypso::BLACK_MARKET ? "STR_CAL_MARKET_BLACKMARKET" : buyer));
#endif
	row("STR_QUANTITY_UC", std::to_string(_shownSale.quantity));
	row("STR_CAL_PROCEEDS", Unicode::formatFunding(_shownSale.proceeds()));
	const auto *rule = _game->getMod()->getItem(item, false);
	std::ostringstream space;
	space << (_base->getUsedStores() - (rule ? rule->getSize() * _shownSale.quantity : 0)) << " / " << _base->getAvailableStores();
	row("STR_CAL_CAPACITY", space.str());
	_btnGotoBase->setVisible(false);
	_btnOk->setText(tr("STR_CANCEL"));
	_btnSummary->setText(tr("STR_CAL_OUTPUT_CONFIRM"));
	_btnSummary->setVisible(_shownSale.quantity > 0);
#ifdef __EMSCRIPTEN__
	_ledger->refresh();
#endif
}

void ProductionCompleteState::confirmSale()
{
	const auto current = quoteSale();
	if (!_shownSale.canConfirm(current))
	{
		showSale(_saleItem, _buyer);
		return;
	}
	const auto *rule = _game->getMod()->getItem(_saleItem, true);
#ifdef __EMSCRIPTEN__
	if (!_buyer.empty()) Calypso::marketRecordSell(_game, _buyer, rule, current.quantity);
#endif
	_base->getStorageItems()->removeItem(rule, current.quantity);
	auto *save = _game->getSavedGame();
	save->setFunds(save->getFunds() + current.proceeds());
	_randomProductionInfo.at(_saleItem) -= current.quantity;
	showOutput();
}

}

#ifdef __EMSCRIPTEN__
namespace OpenXcom {
void ProductionCompleteState::resize(int &dX, int &dY)
{
	if (_ledger) _ledger->refresh();
	else State::resize(dX, dY);
}
}
#endif
