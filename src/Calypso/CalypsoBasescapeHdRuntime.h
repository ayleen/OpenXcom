#pragma once
// F01 read-only runtime snapshot (T09): value-only base visuals, the pure
// craft-slot resolver shared with BaseView, and the audited action binding
// table. No SDL, no game state, no allocation policy: natively unit-tested
// and consumed by the Emscripten adapter. Not #ifdef-guarded, matching the
// Calypso pure-helper convention.

#include <cstdint>
#include <string>
#include <vector>

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoBasescapeHdActionBinding
{
	const char *id;
	const char *handler;
	const char *labelKey;
};

inline const std::vector<CalypsoBasescapeHdActionBinding> &calypsoBasescapeHdActionBindings()
{
	static const std::vector<CalypsoBasescapeHdActionBinding> rows = {
		{ "base.divers", "basescape.openSoldiers", "STR_SOLDIERS_UC" },
		{ "base.research", "basescape.openResearch", "STR_RESEARCH" },
		{ "base.manufacture", "basescape.openManufacture", "STR_MANUFACTURE" },
		{ "base.crafts", "basescape.openCrafts", "STR_EQUIP_CRAFT" },
		{ "base.transfer", "basescape.openTransfer", "STR_TRANSFER_UC" },
		{ "base.purchase", "basescape.openPurchase", "STR_PURCHASE_RECRUIT" },
		{ "base.sell", "basescape.openSell", "STR_SELL_SACK_UC" },
		{ "base.build", "basescape.openBuild", "STR_BUILD_FACILITIES" },
		{ "base.info", "basescape.openBaseInfo", "STR_BASE_INFORMATION" },
		{ "base.new", "basescape.openNewBase", "STR_BUILD_NEW_BASE_UC" },
		{ "navigation.world", "basescape.exitToGeoscape", "STR_GEOSCAPE_UC" },
	};
	return rows;
}

struct CalypsoBasescapeHdFacilityVisual
{
	int x = 0;
	int y = 0;
	int sizeX = 1;
	int sizeY = 1;
	std::string ruleType;
	int buildTime = 0;
	bool disabled = false;
	bool hadPrevious = false;
	bool connectorsDisabled = false;
	int ammo = 0;
	int ammoMax = 0;
	int craftIndex = -1;
	bool craftDrawn = false;
};

struct CalypsoBasescapeHdCraftVisual
{
	std::string name;
	bool away = false;
	std::string artKey;
};

struct CalypsoBasescapeHdSelectorCell
{
	int x = 0;
	int y = 0;
	int sizeX = 1;
	int sizeY = 1;
	bool built = false;
	bool disabled = false;
};

struct CalypsoBasescapeHdSelectorEntry
{
	std::string name;
	bool selected = false;
	std::vector<CalypsoBasescapeHdSelectorCell> cells;
};

struct CalypsoBasescapeHdAuthoredRect
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

struct CalypsoBasescapeHdSnapshot
{
	std::string baseName;
	std::string region;
	std::string funds;
	std::string hoverFacility;
	int selectedBase = 0;
	int baseCount = 0;
	std::vector<CalypsoBasescapeHdFacilityVisual> facilities;
	std::vector<CalypsoBasescapeHdCraftVisual> crafts;
	std::vector<CalypsoBasescapeHdSelectorEntry> bases;
	CalypsoBasescapeHdAuthoredRect selectorRect;
	CalypsoBasescapeHdAuthoredRect deckRect;
	int deckCell = 0;
	bool hasHoverCell = false;
	int hoverX = 0;
	int hoverY = 0;
	int hoverSizeX = 1;
	int hoverSizeY = 1;
	std::uint64_t viewportGeneration = 0;
};

struct CalypsoBasescapeHdPenInput
{
	bool finished = false;
};

struct CalypsoBasescapeHdCraftInput
{
	bool away = false;
};

struct CalypsoBasescapeHdCraftSlot
{
	int craftIndex = -1;
	bool drawn = false;
};

inline std::vector<CalypsoBasescapeHdCraftSlot> calypsoBasescapeHdAssignCrafts(
	const std::vector<CalypsoBasescapeHdPenInput> &pens,
	const std::vector<CalypsoBasescapeHdCraftInput> &crafts)
{
	std::vector<CalypsoBasescapeHdCraftSlot> slots;
	slots.reserve(pens.size());
	size_t next = 0;
	for (const auto &pen : pens)
	{
		if (!pen.finished || next >= crafts.size())
		{
		slots.push_back({-1, false});
			continue;
		}
		const int index = static_cast<int>(next);
		++next;
		slots.push_back({index, !crafts[static_cast<size_t>(index)].away});
	}
	return slots;
}

} // namespace Calypso
} // namespace OpenXcom
