#ifdef __EMSCRIPTEN__
#include "../Battlescape/InventoryLoadState.h"
#include "../Engine/Game.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Mod/Armor.h"
#include "../Mod/RuleInventory.h"
#include "../Savegame/BattleItem.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/EquipmentLayoutItem.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Tile.h"
#include <algorithm>
#include <array>
#include <map>

namespace OpenXcom
{
namespace
{
struct Placement
{
	const RuleInventory* slot;
	const RuleItem* rule;
	int x, y;
	bool overlaps(const EquipmentLayoutItem& entry) const
	{
		if (slot != entry.getSlot())
			return false;
		if (slot->getType() == INV_HAND)
			return true;
		return x < entry.getSlotX() + entry.getItemType()->getInventoryWidth() &&
			   entry.getSlotX() < x + rule->getInventoryWidth() &&
			   y < entry.getSlotY() + entry.getItemType()->getInventoryHeight() &&
			   entry.getSlotY() < y + rule->getInventoryHeight();
	}
};

// Snapshot the same sources as itemDropInventory without moving any live item.
class LoadoutPreview
{
	using Clips = std::array<const BattleItem*, RuleItem::AmmoSlotMax>;
	std::vector<const BattleItem*> _available;
	std::vector<const BattleItem*> _fixed;
	std::vector<Placement> _occupied;
	std::map<const BattleItem*, Clips> _loads;
	Clips& ammunition(const BattleItem* item)
	{
		auto result = _loads.emplace(item, Clips{});
		if (result.second)
			for (int i = 0; i < RuleItem::AmmoSlotMax; ++i)
				result.first->second[i] = item->getAmmoForSlot(i);
		return result.first->second;
	}
	void take(const BattleItem* item)
	{
		_available.erase(std::remove(_available.begin(), _available.end(), item), _available.end());
	}

  public:
	explicit LoadoutPreview(const BattleUnit& unit)
	{
		if (unit.getTile())
			for (auto* item : *unit.getTile()->getInventory())
				_available.push_back(item);
		for (auto* item : *unit.getInventory())
		{
			if (!item->getRules()->isFixed())
				_available.push_back(item);
			else
			{
				_fixed.push_back(item);
				_occupied.push_back({item->getSlot(), item->getRules(), item->getSlotX(), item->getSlotY()});
			}
		}
	}

	const char* next(const EquipmentLayoutItem& entry)
	{
		if (!entry.getItemType() || !entry.getSlot())
			return "MISSING";
		const BattleItem* candidate = nullptr;
		bool exact = false;
		auto consider = [&](const BattleItem* item)
		{
			if (item->getRules() != entry.getItemType())
				return false;
			const auto& load = ammunition(item);
			bool matches = true;
			for (int slot = 0; slot < RuleItem::AmmoSlotMax; ++slot)
			{
				if (!item->needsAmmoForSlot(slot))
					continue;
				if ((load[slot] ? load[slot]->getRules() : nullptr) != entry.getAmmoItemForSlot(slot))
				{
					if (!candidate || ammunition(candidate)[slot])
						candidate = item;
					matches = false;
				}
			}
			if (matches)
			{
				candidate = item;
				exact = true;
			}
			return matches;
		};
		Clips replacements{};
		for (auto* item : _available)
		{
			bool clip = false;
			for (int i = 0; i < RuleItem::AmmoSlotMax; ++i)
				if (!replacements[i] && entry.getAmmoItemForSlot(i) == item->getRules())
				{
					replacements[i] = item;
					clip = true;
				}
			if (!clip && !entry.isFixed() && consider(item))
				break;
		}
		if (entry.isFixed())
			for (auto* item : _fixed)
				if (item->getSlot() == entry.getSlot() && item->getSlotX() == entry.getSlotX() &&
					item->getSlotY() == entry.getSlotY() && consider(item))
					break;
		if (!candidate)
			return "MISSING";
		if (!exact)
		{
			for (int i = 0; i < RuleItem::AmmoSlotMax; ++i)
				if (entry.getAmmoItemForSlot(i) && !replacements[i])
					return "MISSING";
			auto& load = ammunition(candidate);
			for (int i = 0; i < RuleItem::AmmoSlotMax; ++i)
				if (candidate->needsAmmoForSlot(i))
				{
					if (replacements[i])
						take(replacements[i]);
					if (load[i])
						_available.push_back(load[i]);
					load[i] = replacements[i];
				}
		}
		if (!entry.isFixed())
		{
			for (const auto& placement : _occupied)
				if (placement.overlaps(entry))
					return "BLOCKED";
			take(candidate);
			_occupied.push_back({entry.getSlot(), entry.getItemType(), entry.getSlotX(), entry.getSlotY()});
		}
		return exact ? "READY" : "RELOAD";
	}
};
}

void InventoryLoadState::previewSlot(int index)
{
	auto* save = _game->getSavedGame();
	auto* battle = save->getSavedBattle();
	auto* unit = battle ? battle->getSelectedUnit() : nullptr;
	if (!unit)
		return;
	_txtTitle->setText(tr("STR_CAL_PREVIEW_TITLE"));
	_lstLayout->clearList();
	_lstLayout->setSelectable(false);
	auto append = [&](const std::string& text)
	{ _lstLayout->addRow(1, text.c_str()); };
	const auto& requested = save->getGlobalEquipmentLayoutArmor(index);
	if (!requested.empty() && unit->getArmor() && requested != unit->getArmor()->getType())
		append(tr("STR_CAL_PREVIEW_ARMOR").arg(tr(unit->getArmor()->getType())).arg(tr(requested)));
	LoadoutPreview preview(*unit);
	const auto& layout = *save->getGlobalEquipmentLayout(index);
	if (layout.empty())
		append(tr("STR_CAL_PREVIEW_CLEAR"));
	for (auto* entry : layout)
	{
		if (!entry || !entry->getItemType())
			continue;
		const std::string status = tr(std::string("STR_CAL_PREVIEW_") + preview.next(*entry));
		append(status + " — " + std::string(tr(entry->getItemType()->getType())));
	}
	_lstLayout->scrollTo(0);
}
}
#endif
