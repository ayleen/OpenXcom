#pragma once

/*
 * RR3-002 research behavior fixtures.  These are harness-only, deterministic
 * profiles.  The factory deliberately returns an unregistered Base; the
 * harness owns save registration, teardown and option restoration.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdRdFixture.h"

#include <array>
#include <memory>
#include <string>

#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleResearch.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/ResearchProject.h"

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoHdRdResearchProfile : int
{
	NewFree = 1,
	NewFreeCancel = 2,
	NewFreeEscape = 3,
	NewHeldItem = 4,
	NewNoCapacity = 5,
	ExistingAssignment = 6,
	ExistingEscape = 7,
	ExistingCancel = 8,
	CatalogueVisibility = 9,
	NewFreeWide = 10,
	NewHeldItemWide = 11,
	ExistingAssignmentWide = 12,
	ExistingCancelWide = 13,
	CatalogueVisibilityWide = 14,
	NewFreeDpr2 = 15,
	ExistingAssignmentDpr2 = 16,
	NewNoScientists = 17,
	NewNoLaboratories = 18,
	CatalogueEmpty = 19,
};

struct CalypsoHdRdResearchProfileSpec
{
	int id;
	const char *name;
	const char *research;
	int scientists;
	int laboratories;
	bool existing;
	int assigned;
	const char *neededItem;
	int itemCount;
	bool highlightNewTopics;
};

inline const std::array<CalypsoHdRdResearchProfileSpec, 19>&
calypsoHdRdResearchProfileSpecs()
{
	static const std::array<CalypsoHdRdResearchProfileSpec, 19> specs = {{
		{1, "new-free", "STR_CALYPSO_RD_LAB", 20, 10, false, 0, nullptr, 0, true},
		{2, "new-free-cancel", "STR_CALYPSO_RD_LAB", 20, 10, false, 0, nullptr, 0, true},
		{3, "new-free-escape", "STR_CALYPSO_RD_LAB", 20, 10, false, 0, nullptr, 0, true},
		{4, "new-held-item", "STR_CALYPSO_RD_ITEM", 20, 10, false, 0, "STR_CALYPSO_RD_ITEM", 1, true},
		{5, "new-no-capacity", "STR_CALYPSO_RD_LAB", 0, 0, false, 0, nullptr, 0, true},
		{6, "existing-assignment", "STR_CALYPSO_RD_LAB", 10, 10, true, 5, nullptr, 0, true},
		{7, "existing-escape", "STR_CALYPSO_RD_LAB", 10, 10, true, 5, nullptr, 0, true},
		{8, "existing-cancel", "STR_CALYPSO_RD_LAB", 10, 10, true, 5, nullptr, 0, true},
		{9, "catalogue-visibility", "STR_CALYPSO_RD_LAB", 20, 10, false, 0, nullptr, 0, true},
		{10, "new-free-wide", "STR_CALYPSO_RD_LAB", 20, 10, false, 0, nullptr, 0, true},
		{11, "new-held-item-wide", "STR_CALYPSO_RD_ITEM", 20, 10, false, 0, "STR_CALYPSO_RD_ITEM", 1, true},
		{12, "existing-assignment-wide", "STR_CALYPSO_RD_LAB", 10, 10, true, 5, nullptr, 0, true},
		{13, "existing-cancel-wide", "STR_CALYPSO_RD_LAB", 10, 10, true, 5, nullptr, 0, true},
		{14, "catalogue-visibility-wide", "STR_CALYPSO_RD_LAB", 20, 10, false, 0, nullptr, 0, false},
		{15, "new-free-dpr2", "STR_CALYPSO_RD_LAB", 20, 10, false, 0, nullptr, 0, true},
		{16, "existing-assignment-dpr2", "STR_CALYPSO_RD_LAB", 10, 10, true, 5, nullptr, 0, true},
		{17, "new-no-scientists", "STR_CALYPSO_RD_LAB", 0, 10, false, 0, nullptr, 0, true},
		{18, "new-no-laboratories", "STR_CALYPSO_RD_LAB", 20, 0, false, 0, nullptr, 0, true},
		{19, "catalogue-empty-unsupported", nullptr, 0, 0, false, 0, nullptr, 0, false},
	}};
	return specs;
}

inline const CalypsoHdRdResearchProfileSpec*
calypsoHdRdResearchProfileSpec(int profileId)
{
	for (const auto &spec : calypsoHdRdResearchProfileSpecs())
		if (spec.id == profileId) return &spec;
	return nullptr;
}

inline bool calypsoHdRdResearchProfileValid(int profileId)
{
	return calypsoHdRdResearchProfileSpec(profileId) != nullptr;
}

inline bool calypsoHdRdAddLaboratory(Base *base, Mod *mod)
{
	if (!base || !mod) return false;
	for (const std::string &name : mod->getBaseFacilitiesList())
	{
		const RuleBaseFacility *rule = mod->getBaseFacility(name, false);
		if (!rule || rule->getLaboratories() < 10) continue;
		auto *facility = new BaseFacility(rule, base);
		facility->setBuildTime(0);
		base->getFacilities()->push_back(facility);
		return true;
	}
	return false;
}

inline RdAcceptanceFixture makeRdResearchFixture(Game *game, int profileId)
{
	RdAcceptanceFixture fixture;
	const auto *spec = calypsoHdRdResearchProfileSpec(profileId);
	if (!game || !game->getMod() || !spec) return fixture;
	if (!spec->research) return fixture; // R19 remains explicitly unsupported until an empty catalogue fixture exists.
	Mod *mod = game->getMod();
	fixture.research = mod->getResearch(spec->research, false);
	if (!fixture.research) return RdAcceptanceFixture();
	std::unique_ptr<Base> ownedBase(new Base(mod));
	ownedBase->setName("Batumi");
	ownedBase->setScientists(spec->scientists);
	if (spec->laboratories > 0 && !calypsoHdRdAddLaboratory(ownedBase.get(), mod))
		return RdAcceptanceFixture();
	if (spec->neededItem && spec->itemCount > 0)
	{
		RuleItem *item = mod->getItem(spec->neededItem, false);
		if (!item) return RdAcceptanceFixture();
		ownedBase->getStorageItems()->addItem(item, spec->itemCount);
	}
	if (spec->existing)
	{
		auto *project = new ResearchProject(fixture.research, fixture.research->getCost());
		project->setAssigned(spec->assigned);
		ownedBase->addResearch(project);
	}
	const bool previousHighlight = Options::oxceHighlightNewTopics;
	Options::oxceHighlightNewTopics = spec->highlightNewTopics;
	fixture.restoreOptions = [previousHighlight]() {
		Options::oxceHighlightNewTopics = previousHighlight;
	};
	fixture.base = ownedBase.release();
	return fixture;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
