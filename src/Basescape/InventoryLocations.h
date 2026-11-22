#pragma once

#include <variant>
#include <vector>

namespace OpenXcom
{
class Base;
class BaseFacility;
class Craft;
class ResearchProject;
class RuleItem;
class Soldier;

using InventoryHolder = std::variant<Craft *, Soldier *, BaseFacility *, ResearchProject *>;

struct ItemHolding
{
	InventoryHolder owner;
	int quantity;
	bool inbound;
};

struct BaseItemLocations
{
	int stores = 0;
	int inboundItems = 0;
	std::vector<ItemHolding> holdings;

	int total() const
	{
		int count = stores + inboundItems;
		for (const auto &holding : holdings)
			count += holding.quantity;
		return count;
	}
};

BaseItemLocations locateBaseItems(Base &base, const RuleItem *item);
}
