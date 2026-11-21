#pragma once

/*
 * RR3-002 production campaign fixture.  This header contains only the
 * deterministic profile catalogue and the small fixture factory used by the
 * Emscripten harness.  It deliberately does not register a Base in SavedGame;
 * the harness session owns that lifecycle and its options lease.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdRdFixture.h"

#include <array>
#include <functional>
#include <memory>
#include <string>

#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Mod/RuleManufacture.h"
#include "../Mod/RuleResearch.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/Production.h"
#include "../Savegame/SavedGame.h"

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoHdRdProductionProfile : int
{
	Normal = 0,
	CatalogueFacilityMissing = 100,
	CatalogueFilterMatrix = 101,
	CatalogueOver64 = 102,
	RequirementsFailureMatrix = 103,
	RequirementsPersonNoQuarters = 104,
	RequirementsCraftNoHangar = 105,
	RequirementsRichRows = 106,
	ProductionNew = 107,
	ProductionRefund = 108,
	ProductionOptions = 109,
	Production22Of40 = 110,
	ProductionCraft = 111,
	ProductionInfinite = 112,
	ProductionWheel = 113,
	ProductionInvalidBar = 114,
	ProductionWorkspaceError = 115,
	ProductionBothSteppers = 116,
	CatalogueHighlight = 117,
	RequirementsWorkshopZero = 118,
	ProductionFallback = 119,
	ProductionFiniteInfinityToggle = 120,
	ProductionInvalidNegative = 121,
};

struct CalypsoHdRdProductionProfileSpec
{
	int id;
	const char *name;
	const char *manufacture;
	bool existing;
	int amountTotal;
	int timeSpent;
	int engineers;
	bool infinite;
	bool sell;
	bool fallback;
	bool facilityRequired;
	bool requireIchor;
	int workshopCapacity;
	int hangarCapacity;
};

struct CalypsoHdRdProductionOptions
{
	int wheelDelta = 0;
	bool fallbackButton = false;
	bool infinityButton = false;
	bool highlightNewTopics = true;
	bool manufactureFilterSuppliesOk = true;
};

inline CalypsoHdRdProductionOptions calypsoHdRdProductionOptions(int profileId)
{
	CalypsoHdRdProductionOptions options;
	switch (profileId)
	{
	case 101:
	case 117:
		options.highlightNewTopics = profileId == 117;
		break;
	case 107:
	case 110:
		options.infinityButton = true;
		break;
	case 109:
		options.fallbackButton = true;
		options.infinityButton = true;
		break;
	case 113:
	case 115:
	case 116:
		options.wheelDelta = profileId == 113 ? 10 : 1;
		break;
	case 119:
		options.fallbackButton = true;
		options.infinityButton = false;
		break;
	case 120:
		options.infinityButton = true;
		break;
	default:
		break;
	}
	return options;
}

inline const std::array<CalypsoHdRdProductionProfileSpec, 22>&
calypsoHdRdProductionProfileSpecs()
{
	static const std::array<CalypsoHdRdProductionProfileSpec, 22> specs = {{
		{100, "catalogue-facility-missing", "STR_CALYPSO_RD_FACILITY_REQUIRED", false, 0, 0, 2, false, false, false, true, false, 0, 0},
		{101, "catalogue-filter-matrix", "STR_MAELSTROM_TORPEDOES", false, 0, 0, 2, false, false, false, false, true, 0, 0},
		{102, "catalogue-over64", "STR_CALYPSO_RD_OVER64", false, 0, 0, 2, false, false, false, false, true, 0, 0},
		{103, "requirements-failure-matrix", "STR_CALYPSO_RD_FAILURE", false, 0, 0, 2, false, false, false, false, false, 0, 0},
		{104, "requirements-person-no-quarters", "STR_CALYPSO_RD_PERSON", false, 0, 0, 2, false, false, false, false, true, 0, 0},
		{105, "requirements-craft-no-hangar", "STR_CALYPSO_RD_CRAFT", false, 0, 0, 2, false, false, false, false, true, 0, 0},
		{106, "requirements-rich-rows", "STR_CALYPSO_RD_RICH_ROWS", false, 0, 0, 2, false, false, false, false, true, 0, 0},
		{107, "production-new", "STR_MAELSTROM_TORPEDOES", false, 1, 0, 2, false, false, false, false, true, 0, 0},
		{108, "production-refund", "STR_CALYPSO_RD_REFUND", true, 40, 2640, 2, false, false, false, false, true, 0, 0},
		{109, "production-options", "STR_CALYPSO_RD_OPTIONS", true, 40, 2640, 2, false, false, false, false, true, 0, 0},
		{110, "production-22-of-40", "STR_MAELSTROM_TORPEDOES", true, 40, 2640, 2, false, false, false, false, true, 0, 0},
		{111, "production-craft", "STR_CALYPSO_RD_CRAFT", true, 40, 2640, 2, false, false, false, false, true, 0, 1},
		{112, "production-infinite", "STR_MAELSTROM_TORPEDOES", true, 999, 2640, 2, true, false, false, false, true, 0, 0},
		{113, "production-wheel", "STR_MAELSTROM_TORPEDOES", true, 1, 0, 2, false, false, false, false, true, 0, 0},
		{114, "production-invalid-bar", "STR_MAELSTROM_TORPEDOES", true, 0, 0, 2, false, false, false, false, true, 0, 0},
		{115, "production-workspace-error", "STR_MAELSTROM_TORPEDOES", true, 40, 0, 0, false, false, false, false, true, 0, 0},
		// P21 exercises both native wheels; keep workspace available so the
		// engineer wheel remains a real mutation instead of the workspace error.
		{116, "production-both-steppers", "STR_MAELSTROM_TORPEDOES", true, 1, 0, 2, false, false, false, false, true, 8, 0},
		{117, "catalogue-highlight", "STR_MAELSTROM_TORPEDOES", false, 0, 0, 2, false, false, false, false, true, 0, 0},
		{118, "requirements-workshop-zero", "STR_MAELSTROM_TORPEDOES", false, 0, 0, 0, false, false, false, false, true, 0, 0},
		{119, "production-fallback", "STR_CALYPSO_RD_FALLBACK", true, 40, 2640, 2, false, false, false, false, true, 0, 0},
		{120, "production-finite-infinity-toggle", "STR_MAELSTROM_TORPEDOES", true, 1, 0, 2, false, false, false, false, true, 0, 0},
		{121, "production-invalid-negative", "STR_MAELSTROM_TORPEDOES", true, -1, 0, 2, false, false, false, false, true, 0, 0},
	}};
	return specs;
}

inline bool calypsoHdRdProductionProfileValid(int profileId)
{
	for (const auto &spec : calypsoHdRdProductionProfileSpecs())
		if (spec.id == profileId) return true;
	return false;
}

inline const CalypsoHdRdProductionProfileSpec*
calypsoHdRdProductionProfileSpec(int profileId)
{
	for (const auto &spec : calypsoHdRdProductionProfileSpecs())
		if (spec.id == profileId) return &spec;
	return nullptr;
}

inline Production* calypsoHdRdAddProduction(Base *base,
	RuleManufacture *rule, const CalypsoHdRdProductionProfileSpec &spec)
{
	if (!base || !rule || !spec.existing) return nullptr;
	auto *production = new Production(rule, spec.amountTotal);
	production->setAssignedEngineers(spec.engineers);
	production->setTimeSpent(spec.timeSpent);
	production->setInfiniteAmount(spec.infinite);
	production->setSellItems(spec.sell);
	production->setFallback(spec.fallback);
	base->addProduction(production);
	return production;
}

inline void calypsoHdRdAddFacility(Base *base, Mod *mod,
	const char *ruleName)
{
	if (!base || !mod || !ruleName) return;
	const RuleBaseFacility *rule = mod->getBaseFacility(ruleName, false);
	if (!rule) return;
	auto *facility = new BaseFacility(rule, base);
	facility->setBuildTime(0);
	base->getFacilities()->push_back(facility);
}

/// Build a profile after the host has prepared the save.  The returned Base is
/// intentionally unregistered; the host inserts and tracks it exactly once.
inline RdAcceptanceFixture makeRdProductionFixture(Game *game, int profileId)
{
	RdAcceptanceFixture fixture;
	const auto *spec = calypsoHdRdProductionProfileSpec(profileId);
	if (!game || !game->getMod() || !spec) return fixture;
	Mod *mod = game->getMod();
	fixture.manufacture = mod->getManufacture(spec->manufacture, false);
	if (!fixture.manufacture) return RdAcceptanceFixture();
	fixture.base = new Base(mod);
	fixture.base->setName("Batumi");
	fixture.base->setEngineers(profileId == 115 ? 2 : spec->engineers);
	if (spec->requireIchor)
	{
		if (RuleItem *ichor = mod->getItem("STR_ICHOR", false))
			fixture.base->getStorageItems()->addItem(ichor, 1188);
	}
	if (profileId == 101 || profileId == 117
		|| (profileId != 103 && std::string(spec->manufacture).find("STR_CALYPSO_RD_") == 0))
	{
		if (RuleItem *material = mod->getItem("STR_CALYPSO_RD_MATERIAL", false))
			fixture.base->getStorageItems()->addItem(material, 1188);
	}
	if (profileId == 103)
		game->getSavedGame()->setFunds(0);
	if (profileId == 101)
	{
		game->getSavedGame()->setManufactureRuleStatus(
			"STR_MAELSTROM_TORPEDOES", RuleManufacture::MANU_STATUS_HIDDEN);
		game->getSavedGame()->setManufactureRuleStatus(
			"STR_CALYPSO_RD_OVER64", RuleManufacture::MANU_STATUS_NEW);
		game->getSavedGame()->setManufactureRuleStatus(
			"STR_CALYPSO_RD_FACILITY_REQUIRED", RuleManufacture::MANU_STATUS_NORMAL);
		game->getSavedGame()->setManufactureRuleStatus(
			"STR_CALYPSO_RD_RICH_ROWS", RuleManufacture::MANU_STATUS_NORMAL);
	}
	if (spec->workshopCapacity > 0)
		calypsoHdRdAddFacility(fixture.base, mod, "STR_CALYPSO_RD_WORKSHOP");
	if (spec->hangarCapacity > 0)
		calypsoHdRdAddFacility(fixture.base, mod, "STR_CALYPSO_RD_HANGAR");

	const CalypsoHdRdProductionOptions desired = calypsoHdRdProductionOptions(profileId);
	const int oldWheelDelta = Options::changeValueByMouseWheel;
	const bool oldFallbackButton = Options::oxceBaseManufactureFallbackButton;
	const bool oldInfinityButton = Options::oxceBaseManufactureInfinityButton;
	const bool oldHighlightNewTopics = Options::oxceHighlightNewTopics;
	const bool oldFilterSupplies = Options::oxceManufactureFilterSuppliesOK;
	Options::changeValueByMouseWheel = desired.wheelDelta;
	Options::oxceBaseManufactureFallbackButton = desired.fallbackButton;
	Options::oxceBaseManufactureInfinityButton = desired.infinityButton;
	Options::oxceHighlightNewTopics = desired.highlightNewTopics;
	Options::oxceManufactureFilterSuppliesOK = desired.manufactureFilterSuppliesOk;
	auto restored = std::make_shared<bool>(false);
	fixture.restoreOptions = [restored, oldWheelDelta, oldFallbackButton,
		oldInfinityButton, oldHighlightNewTopics, oldFilterSupplies]() {
		if (*restored) return;
		*restored = true;
		Options::changeValueByMouseWheel = oldWheelDelta;
		Options::oxceBaseManufactureFallbackButton = oldFallbackButton;
		Options::oxceBaseManufactureInfinityButton = oldInfinityButton;
		Options::oxceHighlightNewTopics = oldHighlightNewTopics;
		Options::oxceManufactureFilterSuppliesOK = oldFilterSupplies;
	};
	fixture.production = calypsoHdRdAddProduction(fixture.base, fixture.manufacture, *spec);
	return fixture;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
