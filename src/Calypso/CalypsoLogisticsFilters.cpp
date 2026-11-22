#ifdef __EMSCRIPTEN__
#include "../Basescape/StoresState.h"
#include "../Basescape/TransfersState.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Unicode.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleItem.h"
#include "../Savegame/Base.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Transfer.h"
#include <sstream>
namespace OpenXcom
{
namespace
{
const char *cargoName(TransferType type)
{
	switch (type)
	{
	case TRANSFER_CRAFT: return "STR_CRAFT";
	case TRANSFER_SOLDIER: return "STR_SOLDIERS";
	case TRANSFER_SCIENTIST: return "STR_SCIENTISTS";
	case TRANSFER_ENGINEER: return "STR_ENGINEERS";
	default: return "STR_ITEMS";
	}
}
std::string capacity(double used, int available)
{
	std::ostringstream value;
	value << std::round(used * 100) / 100 << " / " << available;
	return value.str();
}
}
std::string StoresState::hdStorageCapacity(bool over) const
{
	const double used = _base->getUsedStores();
	const int available = _base->getAvailableStores();
	if (!over) return capacity(used,available);
	std::ostringstream value;
	value << std::max(0.0,std::round((used - available)*100)/100);
	return value.str();
}
bool StoresState::includeCategory(const RuleItem &rule)
{
	const auto &categories = rule.getCategories();
	const auto *mod = _game->getMod();
	const char *group = rule.isAlien() ? "STR_PRISONERS" :
		rule.getBattleType() == BT_CORPSE ? "STR_ALIENS" :
		mod->isCraftWeaponStorageItem(&rule) ? "STR_CRAFT_ARMAMENT" :
		mod->isArmorStorageItem(&rule) ? "STR_ARMORS" :
		rule.getBattleType() == BT_NONE ? "STR_COMPONENTS" : "STR_EQUIPMENT";
	auto remember = [this](const std::string &category) {
		if (std::find(_availableCategories.begin(),_availableCategories.end(),category) == _availableCategories.end())
			_availableCategories.push_back(category);
	};
	remember(group);
	for (const auto &category : categories) remember(category);
	return _category.empty() || _category == group || rule.belongsToCategory(_category);
}
void StoresState::nextCategory(Action *)
{
	const auto current = std::find(_availableCategories.begin(),_availableCategories.end(),_category);
	auto next = current == _availableCategories.end() ? _availableCategories.begin() : std::next(current);
	_category = next == _availableCategories.end() ? std::string() : *next;
	initList();
}
void TransfersState::configureShipmentFilters()
{
	auto addButton = [&](TextButton *&button) {
		button = new TextButton(1,1,0,0);
		add(button,"button","transferInfo");
		button->onMouseClick((ActionHandler)&TransfersState::refreshShipments);
	};
	addButton(_hdScope);
	addButton(_hdCategory);
	addButton(_hdSort);
	_hdSearch = new TextEdit(this,1,1,0,0);
	add(_hdSearch,"button","transferInfo");
	_hdSearch->onChange((ActionHandler)&TransfersState::refreshShipments);
	_lstTransfers->setColumns(5,80,36,40,62,60);
	refreshShipments(nullptr);
}
void TransfersState::refreshShipments(Action *action)
{
	auto *sender = action ? action->getSender() : nullptr;
	if (sender == _hdScope) _allBases = !_allBases;
	if (sender == _hdSort) _latestFirst = !_latestFirst;
	if (sender == _hdCategory) _cargo = _cargo == TRANSFER_ENGINEER ? -1 : _cargo + 1;
	_hdScope->setText(tr(_allBases ? "STR_CAL_LEDGER_ALL_BASES" : "STR_CAL_LEDGER_THIS_BASE"));
	_hdCategory->setText(tr(_cargo < 0 ? "STR_CAL_LEDGER_ALL_CARGO" : cargoName(static_cast<TransferType>(_cargo))));
	_hdSort->setText(tr(_latestFirst ? "STR_CAL_LEDGER_LATEST" : "STR_CAL_LEDGER_EARLIEST"));
	std::string query = _hdSearch->getText();
	Unicode::upperCase(query);
	struct Shipment { const Transfer *transfer; Base *base; std::string name; };
	std::vector<Shipment> matches;
	for (auto *base : *_game->getSavedGame()->getBases())
	{
		if (!_allBases && base != _base) continue;
		for (const auto *transfer : *base->getTransfers())
		{
			if (_cargo >= 0 && transfer->getType() != _cargo) continue;
			const auto name = transfer->getName(_game->getLanguage());
			if (!query.empty())
			{
				std::string haystack = name + " " + base->getName();
				Unicode::upperCase(haystack);
				if (haystack.find(query) == std::string::npos) continue;
			}
			matches.push_back({transfer,base,name});
		}
	}
	std::stable_sort(matches.begin(),matches.end(),[this](const Shipment &a,const Shipment &b) {
		return _latestFirst ? a.transfer->getHours() > b.transfer->getHours() : a.transfer->getHours() < b.transfer->getHours();
	});
	_nextArrivalHours = -1;
	_lstTransfers->clearList();
	for (const auto &entry : matches)
	{
		const auto *transfer = entry.transfer;
		auto *base = entry.base;
		const int hours = transfer->getHours();
		if (_nextArrivalHours < 0 || hours < _nextArrivalHours) _nextArrivalHours = hours;
		std::string impact;
		switch (transfer->getType())
		{
		case TRANSFER_CRAFT:
			impact = std::string(tr("STR_HANGARS")) + " " + capacity(base->getUsedHangars(),base->getAvailableHangars());
			break;
		case TRANSFER_ITEM:
		{
			const auto *item = transfer->getItems();
			if (item && item->isAlien())
			{
				const int prison = item->getPrisonType();
				impact = std::string(tr("STR_ALIEN_CONTAINMENT")) + " " + capacity(base->getUsedContainment(prison),base->getAvailableContainment(prison));
			}
			else impact = std::string(tr("STR_STORES")) + " " + capacity(base->getUsedStores(),base->getAvailableStores());
			break;
		}
		default:
			impact = std::string(tr("STR_LIVING_QUARTERS")) + " " + capacity(base->getUsedQuarters(),base->getAvailableQuarters());
		}
		_lstTransfers->addRow(5,entry.name.c_str(),std::to_string(transfer->getQuantity()).c_str(),
			std::to_string(hours).c_str(),base->getName().c_str(),impact.c_str());
	}
}
}
#endif
