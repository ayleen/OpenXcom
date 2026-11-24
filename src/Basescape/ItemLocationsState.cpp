/*
 * Copyright 2010-2025 OpenXcom Developers.
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
#include "ItemLocationsState.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoInventoryLedger.h"
#endif
#include "InventoryLocations.h"
#include "BasescapeState.h"
#include "CraftInfoState.h"
#include "CraftsState.h"
#include "ResearchInfoState.h"
#include "SoldierInfoState.h"
#include "StoresState.h"
#include "TransfersState.h"
#include "../Ufopaedia/Ufopaedia.h"
#include "../Geoscape/GeoscapeState.h"
#include <algorithm>
#include "../Interface/ToggleTextButton.h"
#include "../Savegame/Craft.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/ResearchProject.h"
#include "../Savegame/BaseFacility.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Mod/RuleResearch.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Mod/RuleItem.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/Base.h"
#include "../Savegame/SavedGame.h"

namespace OpenXcom
{

/**
 * Initializes all the elements on the UI.
 */
ItemLocationsState::ItemLocationsState(const RuleItem *selectedItem, bool allLocations) : _item(selectedItem)
{
	_screen = false;

	_window = new Window(this, 222, 144, 49, 32);
	_txtTitle = new Text(200, 9, 59, 42);
	_txtBase = new Text(98, 9, 59, 56);
	_txtQuantity = new Text(98, 9, 159, 56);
	_lstLocations = new TextList(186, 64, 57, 70);
	_btnScope = new ToggleTextButton(116, 16, 57, 150);
	_btnOk = new TextButton(82, 16, 177, 150);

	// Set palette
	setInterface("itemLocations");

	add(_window, "window", "itemLocations");
	add(_txtTitle, "text", "itemLocations");
	add(_txtBase, "text", "itemLocations");
	add(_txtQuantity, "text", "itemLocations");
	add(_btnScope, "button", "itemLocations");
	add(_btnOk, "button", "itemLocations");
	add(_lstLocations, "list", "itemLocations");

	centerAllSurfaces();

	// Set up objects
#ifndef __EMSCRIPTEN__
	setWindowBackground(_window, "itemLocations");
#endif

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_TOPIC").arg(tr(selectedItem->getType())));

	_txtBase->setText(tr("STR_BASE"));
	_txtQuantity->setText(tr("STR_QUANTITY_UC"));

	_btnScope->setText(tr("STR_CAL_ALL_LOCATIONS"));
	_btnScope->setPressed(allLocations);
	_btnScope->onMouseClick((ActionHandler)&ItemLocationsState::btnScopeClick);

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ItemLocationsState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ItemLocationsState::btnOkClick, Options::keyCancel);

	_lstLocations->setColumns(3, 100, 40, 30);
	_lstLocations->setBackground(_window);
	_lstLocations->setMargin(2);
	_lstLocations->setSelectable(true);
	_lstLocations->onMouseClick((ActionHandler)&ItemLocationsState::lstLocationClick, SDL_BUTTON_LEFT);

	updateList();
#ifdef __EMSCRIPTEN__
	_screen = true;
	_hdLedger = new Calypso::CalypsoInventoryLedger(*this);
#endif
}

ItemLocationsState::~ItemLocationsState()
{
#ifdef __EMSCRIPTEN__
	delete _hdLedger;
#endif
}

void ItemLocationsState::btnScopeClick(Action *)
{
	updateList();
}

void ItemLocationsState::init()
{
	State::init();
	updateList();
}

void ItemLocationsState::lstLocationClick(Action *)
{
	const auto row = _lstLocations->getSelectedRow();
	if (row < _openLocation.size())
		_openLocation[row]();
}

void ItemLocationsState::updateList()
{
	_lstLocations->clearList();
	_openLocation.clear();
	_scopeTotal = 0;
	_scopeInbound = 0;
	const bool all = _btnScope->getPressed();
	_txtTitle->setText(std::string(tr(_item->getType())) + " · " +
		std::string(tr(all ? "STR_CAL_ALL_LOCATIONS" : "STR_CAL_STOCK_AND_INBOUND")));
	for (auto *base : *_game->getSavedGame()->getBases())
	{
		const auto locations = locateBaseItems(*base, _item);
		auto openStores = [base]() { _game->pushState(new StoresState(base)); };
		auto openTransfers = [base]() { _game->pushState(new TransfersState(base)); };
		_openLocation.emplace_back([base, openStores]()
		{
			if (auto *geo = _game->getGeoscapeState())
				_game->pushState(new BasescapeState(base, geo->getGlobe()));
			else
				openStores();
		});
		const int quantity = all ? locations.total() : locations.stores;
		_scopeTotal += all ? quantity : quantity + locations.inboundItems;
		_scopeInbound += locations.inboundItems;
		if (all)
			for (const auto &holding : locations.holdings)
				if (holding.inbound)
					_scopeInbound += holding.quantity;
		const auto incoming = all ? std::string() : "+" + std::to_string(locations.inboundItems);
		_lstLocations->addRow(3, base->getName().c_str(), std::to_string(quantity).c_str(), incoming.c_str());
		if (!all)
			continue;

		auto addDetail = [&](const std::string &label, int count, std::function<void()> open)
		{
			if (count > 0)
			{
				_lstLocations->addRow(3, ("  " + label).c_str(), std::to_string(count).c_str(), "");
				_openLocation.emplace_back(std::move(open));
			}
		};
		addDetail(tr("STR_STORES"), locations.stores, openStores);
		addDetail(tr("STR_TRANSFERS"), locations.inboundItems, openTransfers);
		for (const auto &holding : locations.holdings)
		{
			std::string label;
			std::function<void()> open;
			if (auto owner = std::get_if<Craft *>(&holding.owner))
			{
				auto *craft = *owner;
				label = craft->getName(_game->getLanguage());
				open = [base, craft]()
				{
					const auto &crafts = *base->getCrafts();
					const auto it = std::find(crafts.begin(), crafts.end(), craft);
					if (it != crafts.end() && craft->getStatus() != "STR_OUT")
						_game->pushState(new CraftInfoState(base, it - crafts.begin()));
					else
						_game->pushState(new CraftsState(base));
				};
			}
			else if (auto owner = std::get_if<Soldier *>(&holding.owner))
			{
				auto *soldier = *owner;
				label = soldier->getName();
				open = [base, soldier]()
				{
					const auto &soldiers = *base->getSoldiers();
					const auto it = std::find(soldiers.begin(), soldiers.end(), soldier);
					if (it != soldiers.end())
						_game->pushState(new SoldierInfoState(base, it - soldiers.begin(), true, true));
				};
			}
			else if (auto owner = std::get_if<BaseFacility *>(&holding.owner))
			{
				const auto *facility = *owner;
				label = std::string(tr(facility->getRules()->getType())) + " (" +
					std::to_string(facility->getX() + 1) + ", " + std::to_string(facility->getY() + 1) + ")";
				open = [facility]()
				{
					auto article = facility->getRules()->getUfopediaType();
					Ufopaedia::openArticle(_game, article);
				};
			}
			else
			{
				auto *project = std::get<ResearchProject *>(holding.owner);
				label = tr(project->getRules()->getName());
				open = [base, project]() { _game->pushState(new ResearchInfoState(base, project)); };
			}
			if (holding.inbound)
			{
				label = tr("STR_CAL_INBOUND_HOLDER").arg(label);
				open = openTransfers;
			}
			addDetail(label, holding.quantity, std::move(open));
		}
	}
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void ItemLocationsState::btnOkClick(Action *)
{
	_game->popState();
}

#ifdef __EMSCRIPTEN__
void ItemLocationsState::resize(int &dX, int &dY)
{
	if (_hdLedger) _hdLedger->refresh();
	else State::resize(dX, dY);
}
#endif

}
