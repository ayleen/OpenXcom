#include "InventoryLocations.h"
#include "../Mod/Armor.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Mod/RuleResearch.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/Craft.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/ResearchProject.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/Transfer.h"

namespace OpenXcom
{
BaseItemLocations locateBaseItems(Base &base, const RuleItem *item)
{
	BaseItemLocations result;
	result.stores = base.getStorageItems()->getItem(item);
	auto record = [&](InventoryHolder owner, int count, bool inbound = false)
	{
		if (count > 0)
			result.holdings.push_back({owner, count, inbound});
	};
	for (auto *facility : *base.getFacilities())
	{
		const auto *rules = facility->getRules();
		if (rules->getAmmoMax() > 0 && rules->getAmmoItem() == item)
			record(facility, facility->getAmmo());
	}
	for (auto *craft : *base.getCrafts())
		record(craft, craft->getTotalItemCount(item));
	for (auto *soldier : *base.getSoldiers())
		if (soldier->getArmor()->getStoreItem() == item)
			record(soldier, 1);
	for (auto *project : base.getResearch())
	{
		const auto *rules = project->getRules();
		if (rules->isHoldingNeededItem() && rules->getNeededItem() == item)
			record(project, 1);
	}
	for (auto *transfer : *base.getTransfers())
	{
		if (auto *craft = transfer->getCraft())
			record(craft, craft->getTotalItemCount(item), true);
		else if (auto *soldier = transfer->getSoldier())
		{
			if (soldier->getArmor()->getStoreItem() == item)
				record(soldier, 1, true);
		}
		else if (transfer->getItems() == item)
			result.inboundItems += transfer->getQuantity();
	}
	return result;
}
}
