#pragma once
#include <map>
#include <string>

namespace OpenXcom
{
template<typename CraftType, typename Generate>
void generateLandingWithSupplies(CraftType &craft, const std::map<std::string, int> &cost, Generate generate)
{
	auto available = *craft.getItems();
	for (const auto &item : cost)
	{
		available.removeItem(item.first, item.second);
	}
	generate(available);
	craft.destroyRequiredItems(cost);
}
}
