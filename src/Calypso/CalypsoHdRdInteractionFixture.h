#pragma once

/*
 * RR3 interaction/capacity fixtures. Profiles 300-314 are intentionally
 * isolated from the research, production, global, and dependency namespaces.
 * The harness owns registration and teardown of the returned Base.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdRdFixture.h"
#include "CalypsoHdRdGlobalFixture.h"

#include <array>
#include <algorithm>
#include <vector>
#include <cstddef>
#include <memory>
#include <string>

#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Mod/RuleResearch.h"
#include "../Mod/RuleManufacture.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/Production.h"
#include "../Savegame/ResearchProject.h"
#include "../Savegame/SavedGame.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoHdRdInteractionProfileSpec
{
	int id;
	const char *name;
	int rows;
	bool sorting;
	bool highlightNew;
	bool reorder;
	size_t initialOffset;
	size_t initialSelected;
	size_t expectedVisibleRows;
	const char *visualState = nullptr;
};

inline const std::array<CalypsoHdRdInteractionProfileSpec, 20>&
calypsoHdRdInteractionProfileSpecs()
{
	static const std::array<CalypsoHdRdInteractionProfileSpec, 20> specs = {{
		{300, "capacity-0", 0, true, true, false, 0, 0, 0},
		{301, "capacity-1", 1, true, true, false, 0, 0, 0},
		{302, "capacity-64", 64, true, true, false, 0, 0, 0},
		{303, "capacity-65", 65, true, true, false, 0, 0, 0},
		{304, "capacity-300", 300, true, true, false, 0, 0, 0},
		{305, "offset-293", 300, true, true, false, 293, 0, 7},
		{306, "filter-remove", 4, true, true, false, 0, 0, 0},
		{307, "row-open", 4, true, true, false, 0, 0, 0},
		{308, "sort-enabled", 4, true, true, false, 0, 0, 0},
		{309, "sort-disabled", 4, false, true, false, 0, 0, 0},
		{310, "quick-search", 4, true, true, false, 0, 0, 0},
		{311, "wheel-zones", 300, true, true, false, 0, 0, 0},
		{312, "reorder", 4, true, true, true, 0, 0, 0},
		{313, "hold-timer", 4, true, true, false, 0, 0, 0},
		{314, "navigation", 4, true, true, false, 0, 0, 0},
		{315, "reorder-disabled", 4, true, true, false, 0, 0, 0},
		{320, "visual-no-rows", 0, true, true, false, 0, 0, 0, "no-rows"},
		{321, "visual-no-matches", 4, true, true, false, 0, 0, 0, "no-matches"},
		{322, "visual-disabled-feature", 4, false, false, false, 0, 0, 0, "disabled-feature"},
		{323, "visual-route-error", 4, true, true, false, 0, 0, 0, "route-error"},
	}};
	return specs;
}

inline const CalypsoHdRdInteractionProfileSpec*
calypsoHdRdInteractionProfileSpec(int profileId)
{
	for (const auto &spec : calypsoHdRdInteractionProfileSpecs())
		if (spec.id == profileId) return &spec;
	return nullptr;
}

inline const char *calypsoHdRdInteractionVisualState(int profileId)
{
	const auto *spec = calypsoHdRdInteractionProfileSpec(profileId);
	return spec && spec->visualState ? spec->visualState : "";
}

inline bool calypsoHdRdInteractionProfileValid(int profileId)
{
	return calypsoHdRdInteractionProfileSpec(profileId) != nullptr;
}

/// Return one stable GPL rule identity per synthetic capacity row. The first
/// four names retain the small mixed-status fixtures used by I02/I03; the
/// remaining names are distinct so a 300-row run can verify identity/index
/// mapping instead of silently accepting repeated rule names.
inline std::string interactionResearchRuleName(int index)
{
	switch (index)
	{
	case 0: return "STR_CALYPSO_RD_INTERACTION_ROW";
	case 1: return "STR_CALYPSO_RD_INTERACTION_ALPHA";
	case 2: return "STR_CALYPSO_RD_INTERACTION_BETA";
	case 3: return "STR_CALYPSO_RD_INTERACTION_GAMMA";
	default: break;
	}
	std::string suffix = std::to_string(index);
	while (suffix.size() < 3) suffix.insert(suffix.begin(), '0');
	return "STR_CALYPSO_RD_INTERACTION_ROW_" + suffix;
}

inline bool calypsoHdRdInteractionAddLaboratory(Base *base, Mod *mod)
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

inline RdAcceptanceFixture makeRdInteractionFixture(Game *game, int profileId,
	int scenario = 0)
{
	RdAcceptanceFixture fixture;
	const auto *spec = calypsoHdRdInteractionProfileSpec(profileId);
	if (!game || !game->getMod() || !spec) return fixture;
	if (profileId == 310)
		return makeRdGlobalFixture(game, static_cast<int>(CalypsoHdRdGlobalProfile::DiarySearchRetention));
	Mod *mod = game->getMod();
	std::array<RuleResearch*, 300> researchRules = {};
	for (int i = 0; i < spec->rows; ++i)
	{
		const std::string name = interactionResearchRuleName(i);
		researchRules[i] = mod->getResearch(name, false);
		if (!researchRules[i]) return RdAcceptanceFixture();
	}
	fixture.research = researchRules[0];
	if (!fixture.research && !mod->getResearchList().empty())
		fixture.research = mod->getResearch(mod->getResearchList().front(), false);
	std::unique_ptr<Base> ownedBase(new Base(mod));
	ownedBase->setName("Batumi");
	// A true no-rows state must not synthesize the base-group placeholder.
	// GlobalResearchState emits a group whenever scientists are present, even
	// when every project is hidden, so keep the empty fixture's base empty.
	ownedBase->setScientists(profileId == 320 ? 0 : 20);
	if (!calypsoHdRdInteractionAddLaboratory(ownedBase.get(), mod)) return RdAcceptanceFixture();
	if (const RuleBaseFacility *workshop = mod->getBaseFacility("STR_CALYPSO_RD_WORKSHOP", false))
	{
		auto *facility = new BaseFacility(workshop, ownedBase.get());
		facility->setBuildTime(0);
		ownedBase->getFacilities()->push_back(facility);
	}
	for (int index = 0; index < spec->rows; ++index)
	{
		RuleResearch *rule = researchRules[index];
		auto *project = new ResearchProject(rule, rule->getCost());
		project->setAssigned(0);
		ownedBase->addResearch(project);
	}
	const bool productionRoute = (scenario >= 94 && scenario <= 98) || scenario == 101;
	RuleManufacture *manufacture = mod->getManufacture("STR_MAELSTROM_TORPEDOES", false);
	if (!manufacture && !mod->getManufactureList().empty())
		manufacture = mod->getManufacture(mod->getManufactureList().front(), false);
	const bool emptyVisualFixture = profileId == 320 || profileId == 321;
	const bool needsProductionOwner = scenario == 97;
	Production *production = manufacture && productionRoute
		&& (!emptyVisualFixture || needsProductionOwner)
		? new Production(manufacture, 1) : nullptr;
	if (production)
	{
		production->setAssignedEngineers(2);
		ownedBase->addProduction(production);
	}
	const bool oldHighlight = Options::oxceHighlightNewTopics;
	const bool oldReorder = Options::oxceBaseResearchReorder;
	const bool oldSorting = mod->getEnableNewResearchSorting();
	Options::oxceHighlightNewTopics = spec->highlightNew;
	Options::oxceBaseResearchReorder = spec->reorder;
	mod->setEnableNewResearchSorting(spec->sorting);
	if (profileId == 306 || profileId == 308 || profileId == 309)
	{
		// Keep the catalogue mixed so native Show New/Hidden and visibility
		// transitions prove filtering rather than merely exposing a control.
		std::vector<RuleResearch*> availableTopics;
		game->getSavedGame()->getAvailableResearchProjects(availableTopics, mod, ownedBase.get(), true);
		if (availableTopics.size() < 2)
		{
			Options::oxceHighlightNewTopics = oldHighlight;
			Options::oxceBaseResearchReorder = oldReorder;
			mod->setEnableNewResearchSorting(oldSorting);
			return RdAcceptanceFixture();
		}
		// NewResearchListState(profile) opens with the native cost sort. Keep the
		// two status probes in that same order and exclude requirement-only rules
		// which fillProjectList removes before painting; otherwise row zero can
		// be hidden or disappear before L05 receives its real owner action.
		availableTopics.erase(std::remove_if(availableTopics.begin(), availableTopics.end(),
			[](RuleResearch *rule) { return !rule || !rule->getRequirements().empty(); }),
			availableTopics.end());
		std::sort(availableTopics.begin(), availableTopics.end(), [](RuleResearch *a, RuleResearch *b) {
			if (a->getCost() != b->getCost()) return a->getCost() < b->getCost();
			return a->getListOrder() < b->getListOrder();
		});
		game->getSavedGame()->setResearchRuleStatus(
			availableTopics[0]->getName(), RuleResearch::RESEARCH_STATUS_NORMAL);
		game->getSavedGame()->setResearchRuleStatus(
			availableTopics[1]->getName(), RuleResearch::RESEARCH_STATUS_HIDDEN);
	}
	if (profileId == 320 || profileId == 321)
	{
		if (profileId == 320)
			for (const std::string& name : mod->getResearchList())
				game->getSavedGame()->setResearchRuleStatus(
					name, RuleResearch::RESEARCH_STATUS_HIDDEN);
		for (RuleResearch *rule : researchRules)
			if (rule) game->getSavedGame()->setResearchRuleStatus(
				rule->getName(), RuleResearch::RESEARCH_STATUS_HIDDEN);
		for (const std::string& name : mod->getManufactureList())
			game->getSavedGame()->setManufactureRuleStatus(
				name, RuleManufacture::MANU_STATUS_HIDDEN);
	}
	if (profileId == 322)
	{
		Options::oxceHighlightNewTopics = false;
		mod->setEnableNewResearchSorting(false);
	}
	if (scenario == 100 && (profileId == 321 || profileId == 322)
		&& !calypsoHdRdGlobalDiary(game, 206))
	{
		// The visual no-match and disabled-feature profiles must exercise the
		// real diary owner. Profile 320 intentionally remains empty for no-rows.
		Options::oxceHighlightNewTopics = oldHighlight;
		Options::oxceBaseResearchReorder = oldReorder;
		mod->setEnableNewResearchSorting(oldSorting);
		return RdAcceptanceFixture();
	}
	fixture.restoreOptions = [mod, oldHighlight, oldReorder, oldSorting]() {
		Options::oxceHighlightNewTopics = oldHighlight;
		Options::oxceBaseResearchReorder = oldReorder;
		mod->setEnableNewResearchSorting(oldSorting);
	};
	fixture.base = ownedBase.release();
	fixture.manufacture = manufacture;
	fixture.production = production;
	return fixture;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
