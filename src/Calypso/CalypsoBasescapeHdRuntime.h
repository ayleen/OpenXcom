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
		{ "base.divers", "basescape.openSoldiers", "STR_CALYPSO_BASE_DIVERS" },
		{ "base.research", "basescape.openResearch", "STR_CALYPSO_BASE_RESEARCH" },
		{ "base.manufacture", "basescape.openManufacture", "STR_CALYPSO_BASE_PRODUCTION" },
		{ "base.crafts", "basescape.openCrafts", "STR_CALYPSO_BASE_SUBMARINES" },
		{ "base.transfer", "basescape.openTransfer", "STR_CALYPSO_BASE_TRANSFER" },
		{ "base.purchase", "basescape.openPurchase", "STR_CALYPSO_BASE_PURCHASE" },
		{ "base.sell", "basescape.openSell", "STR_CALYPSO_BASE_SELL" },
		{ "base.build", "basescape.openBuild", "STR_CALYPSO_BASE_BUILD" },
		{ "base.info", "basescape.openBaseInfo", "STR_CALYPSO_BASE_INFO" },
		{ "base.new", "basescape.openNewBase", "STR_CALYPSO_BASE_NEW" },
		{ "navigation.world", "basescape.exitToGeoscape", "STR_CALYPSO_BASE_WORLD" },
	};
	return rows;
}

/// Runtime lookup of the recipe labelKey/handler by semantic action id.
/// The holder resolves tr(labelKey) for live labels; the generated fixture
/// label stays the harness-only fallback. Returns nullptr when unbound.
inline const CalypsoBasescapeHdActionBinding *calypsoBasescapeHdBindingFor(const std::string &id)
{
	for (const auto &row : calypsoBasescapeHdActionBindings())
	{
		if (id == row.id)
		{
			return &row;
		}
	}
	return nullptr;
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

/// Placement-mode overlay (F01 construction): the chosen facility identity,
/// its native translated detail lines, and the guidance/cancel labels. Grid
/// position and validity are NOT snapshotted: the renderer reads the live
/// placement BaseView each frame (same as the hover ring), so the preview
/// can never go stale between holder refreshes.
struct CalypsoBasescapeHdPlacementVisual
{
	bool active = false;
	std::string ruleType;
	int sizeX = 1;
	int sizeY = 1;
	bool isMove = false;
	std::string facilityName;
	std::vector<std::string> detailLines;
	std::string guidanceSelect;
	std::string guidanceValid;
	std::string guidanceInvalid;
	std::string cancelLabel;
};

struct CalypsoBasescapeHdSnapshot
{
	std::string baseName;
	std::string baseCaption;
	std::string displayTime;
	std::string displayDate;
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
	CalypsoBasescapeHdPlacementVisual placement;
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
