/*
 * Phase 46.4-F33 (Calypso) -- opaque-black engine harness host. See
 * CalypsoHdHarnessHostState.h.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdRuntimeDiagnostics.h"
#include "CalypsoPauseMenu.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoErrorPopupUi.h"
#include "../Menu/ErrorMessageState.h"
#include "CalypsoTutorial.h"

#include <SDL.h>
#include <emscripten.h>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cmath>
#include <sstream>
#include <string>
#include <typeinfo>

#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/RNG.h"
#include "../Engine/Screen.h"
#include "../Engine/Logger.h"
#include "../Interface/Cursor.h"
#include "../Menu/AbandonGameState.h"
#include "../Basescape/BaseView.h"
#include "../Basescape/ManageAlienContainmentState.h"
#include "../Basescape/BuildFacilitiesState.h"
#include "../Basescape/DismantleFacilityState.h"
#include "../Basescape/PlaceFacilityState.h"
#include "../Basescape/SackSoldierState.h"
#include "../Basescape/SoldierTransformState.h"
#include "../Basescape/SoldierDiaryOverviewState.h"
#include "../Basescape/ResearchState.h"
#include "../Basescape/NewResearchListState.h"
#include "../Basescape/ResearchInfoState.h"
#include "../Basescape/ManufactureState.h"
#include "../Basescape/NewManufactureListState.h"
#include "../Basescape/ManufactureStartState.h"
#include "../Basescape/ManufactureInfoState.h"
#include "../Basescape/ManufactureDependenciesTreeState.h"
#include "../Basescape/GlobalResearchState.h"
#include "../Basescape/GlobalResearchDiaryState.h"
#include "../Basescape/GlobalManufactureState.h"
#include "../Basescape/TechTreeViewerState.h"
#include "../Ufopaedia/ArticleStateCraftWeapon.h"
#include "../Basescape/BasescapeState.h"
#include "../Geoscape/ConfirmLandingState.h"
#include "../Geoscape/CraftErrorState.h"
#include "../Geoscape/ConfirmCydoniaState.h"
#include "../Geoscape/ResearchRequiredState.h"
#include "../Geoscape/ResearchCompleteState.h"
#include "../Geoscape/UfoLostState.h"
#include "../Geoscape/UfoDetectedState.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Geoscape/Globe.h"
#include "../Mod/RuleResearch.h"
#include "../Mod/RuleManufacture.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleAlienMission.h"
#include "../Savegame/ResearchProject.h"
#include "../Savegame/Production.h"
#include "../Savegame/Ufo.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/AlienMission.h"
#include "../Mod/UfoTrajectory.h"
#include "../Geoscape/TrainingFinishedState.h"
#include "../Geoscape/ProductionCompleteState.h"
#include "../Geoscape/ItemsArrivingState.h"
#include "../Battlescape/AbortMissionState.h"
#include "../Battlescape/ConfirmEndMissionState.h"
#include "../Battlescape/NoExperienceState.h" 
#include "../Mod/Mod.h"
#include "../Mod/RuleInterface.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/SavedGame.h"

#include "CalypsoAbandonPopupUi.h" // calypsoHdHarnessSetSideBySide (F33 comparison shift)
#include "CalypsoHdOperationsRenderer.h"
#include "CalypsoCanvasCoordinateMapping.h"
#include "CalypsoViewportRuntime.h"
#include "CalypsoF09ResearchUi.h"
#include "CalypsoHdRdResearchFixture.h"
#include "CalypsoHdRdProductionFixture.h"
#include "CalypsoHdRdGlobalFixture.h"
#include "CalypsoHdRdDependenciesFixture.h"
#include "CalypsoHdRdInteractionFixture.h"
#include "CalypsoHdRdVisualStateFixture.h"
#include "CalypsoHdRdNavigationFixture.h"
#include "CalypsoRdErrorForm.h"
#include "CalypsoStrategicNavigation.h"
#include "CalypsoHarnessSaveOwnership.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{
CalypsoHarnessSession g_harnessSession;

struct HarnessSaveLease
{
    SavedGame *original = nullptr;
    SavedGame *fixture = nullptr;
    std::int64_t originalFunds = 0;
    // Fixture bases owned by the harness (not by their target states): the
    // place-facility target never owns its base, so the lease frees it.
    Base *fixtureBase = nullptr;
    std::vector<Base*> fixtureBases;
    bool fixtureBaseInSave = false;
    bool active = false;
};
static HarnessSaveLease g_harnessSaveLease;
std::function<void()> g_rdRestoreOptions;
std::string g_rdRestoreLanguage;
bool g_rdRestoreLanguageSet = false;
bool g_harnessCursorCaptured = false;
bool g_harnessCursorVisible = true;
bool g_harnessCursorHidden = false;
std::string g_harnessSnapshotLease;
bool g_harnessSnapshotLeaseValid = false;

void hideHarnessCursor(Game* game)
{
	if (!game || !game->getCursor() || g_harnessCursorCaptured) return;
	Cursor* cursor = game->getCursor();
	g_harnessCursorVisible = cursor->getVisible();
	g_harnessCursorHidden = cursor->getHidden();
	g_harnessCursorCaptured = true;
	cursor->setVisible(false);
	cursor->setHidden(true);
}

void restoreHarnessCursor(Game* game)
{
	if (!g_harnessCursorCaptured) return;
	if (game && game->getCursor())
	{
		Cursor* cursor = game->getCursor();
		cursor->setVisible(g_harnessCursorVisible);
		cursor->setHidden(g_harnessCursorHidden);
	}
	g_harnessCursorCaptured = false;
}
bool prepareOperationsSave(Game *game)
{
	if (!game || !game->getMod()) return false;
	// Fixture routes may only mutate a save owned by this harness.  Reusing a
	// player campaign and restoring a few fields is not isolation: diary rows,
	// research status, and other mutable records can survive teardown.  A live
	// save is therefore rejected until the caller has opened an explicit
	// harness-owned replacement save.
	if (!g_harnessSaveLease.active)
	{
		if (game->getSavedGame())
			return false;
		else
		{
			g_harnessSaveLease.fixture = new SavedGame();
			game->setSavedGame(g_harnessSaveLease.fixture);
		}
		g_harnessSaveLease.active = true;
	}
	SavedGame *save = game->getSavedGame();
	if (!Calypso::calypsoHarnessSaveOwned(g_harnessSaveLease.active,
		g_harnessSaveLease.fixture, save))
		return false;
	save->setFunds(27550246);
	const int profile = calypsoHarnessSession().rdActiveProfile;
	const bool dependencyHiddenRowsProfile = profile == 222 || profile == 223;
	if (save == g_harnessSaveLease.fixture
		&& !dependencyHiddenRowsProfile
		&& !save->getDebugMode())
		save->setDebugMode();
	return true;
}

RuleResearch *firstResearchRule(Mod *mod)
{
	if (!mod) return nullptr;
	for (const std::string &name : mod->getResearchList())
	{
		if (RuleResearch *rule = mod->getResearch(name, false)) return rule;
	}
	return nullptr;
}

RuleManufacture *firstManufactureRule(Mod *mod)
{
	if (!mod) return nullptr;
	for (const std::string &name : mod->getManufactureList())
	{
		if (RuleManufacture *rule = mod->getManufacture(name, false)) return rule;
	}
	return nullptr;
}

RuleResearch *researchRule(Mod *mod, const char *name)
{
	if (!mod) return nullptr;
	if (name)
	{
		if (RuleResearch *rule = mod->getResearch(name, false)) return rule;
	}
	return firstResearchRule(mod);
}

RuleManufacture *manufactureRule(Mod *mod, const char *name)
{
	if (!mod) return nullptr;
	if (name)
	{
		if (RuleManufacture *rule = mod->getManufacture(name, false)) return rule;
	}
	return firstManufactureRule(mod);
}

void trackFixtureBase(Base *base)
{
	if (!base) return;
	if (!g_harnessSaveLease.fixtureBase) g_harnessSaveLease.fixtureBase = base;
	g_harnessSaveLease.fixtureBases.push_back(base);
	g_harnessSaveLease.fixtureBaseInSave = true;
}

struct OperationsFixture
{
	Base *base = nullptr;
	RuleResearch *research = nullptr;
	RuleManufacture *manufacture = nullptr;
	Production *production = nullptr;
};

Production *addProductionFixture(Base *base, RuleManufacture *rule, int engineers,
	int amount, int timeSpent)
{
	if (!base || !rule) return nullptr;
	auto *production = new Production(rule, amount);
	production->setAssignedEngineers(engineers);
	production->setTimeSpent(timeSpent);
	base->addProduction(production);
	return production;
}

bool addLaboratoryFixture(Base *base, Mod *mod)
{
	if (!base || !mod) return false;
	for (const std::string& name : mod->getBaseFacilitiesList())
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

bool addWorkshopFixture(Base *base, Mod *mod)
{
	if (!base || !mod) return false;
	for (const std::string& name : mod->getBaseFacilitiesList())
	{
		const RuleBaseFacility *rule = mod->getBaseFacility(name, false);
		if (!rule || rule->getWorkshops() < 10) continue;
		auto *facility = new BaseFacility(rule, base);
		facility->setBuildTime(0);
		base->getFacilities()->push_back(facility);
		return true;
	}
	return false;
}

OperationsFixture makeOperationsFixture(Game *game, bool needResearch, bool needManufacture,
	const char *researchName = nullptr, const char *manufactureName = nullptr,
	bool activeResearch = false, bool activeManufacture = false)
{
	OperationsFixture fixture;
	if (!game || !game->getMod()) return fixture;
	const int profile = calypsoHarnessSession().rdActiveProfile;
	const bool visualStateProfile = calypsoHdRdVisualStateProfileValid(profile);
	const int ownerProfile = calypsoHdRdNavigationProfileValid(profile)
		? calypsoHdRdNavigationUnderlyingProfile(profile) : profile;
	if (profile != 0 && !visualStateProfile)
	{
		if (!prepareOperationsSave(game)) return fixture;
		if (calypsoHdRdNavigationProfileValid(profile))
			calypsoHdRdNavigationPrepareProfile(profile);
		RdAcceptanceFixture owned = ownerProfile < 100
			? makeRdResearchFixture(game, ownerProfile)
			: ownerProfile < 200 ? makeRdProductionFixture(game, ownerProfile)
			: ownerProfile < 220 ? makeRdGlobalFixture(game, ownerProfile)
			: calypsoHdRdInteractionProfileValid(ownerProfile)
					? makeRdInteractionFixture(game, ownerProfile,
						calypsoHarnessSession().rdActiveScenario)
				: makeRdDependenciesFixture(game, ownerProfile);
		if (!owned.base)
		{
			if (owned.restoreOptions) owned.restoreOptions();
			return fixture;
		}
		g_rdRestoreOptions = std::move(owned.restoreOptions);
		fixture.base = owned.base;
		fixture.research = owned.research;
		fixture.manufacture = owned.manufacture;
		fixture.production = owned.production;
		game->getSavedGame()->getBases()->push_back(fixture.base);
		trackFixtureBase(fixture.base);
		for (Base* additional : owned.additionalBases)
		{
			game->getSavedGame()->getBases()->push_back(additional);
			trackFixtureBase(additional);
		}
		CalypsoTutorial::get().disableForCampaign();
		return fixture;
	}
	CalypsoTutorial::get().disableForCampaign();
	fixture.research = researchRule(game->getMod(), researchName);
	fixture.manufacture = manufactureRule(game->getMod(), manufactureName);
	if ((needResearch && !fixture.research) || (needManufacture && !fixture.manufacture)
		|| !prepareOperationsSave(game))
		return OperationsFixture();

	fixture.base = new Base(game->getMod());
	fixture.base->setName("Batumi");
	if (needResearch)
	{
		fixture.base->setScientists(0);
		if (activeResearch && fixture.research)
		{
			// The canonical global-research fixture assigns 100 scientists.
			// Give it ten ten-place laboratories so native free capacity remains
			// zero, matching the generated Reference summary instead of exposing
			// an impossible negative laboratory balance.
			for (int i = 0; i < 10; ++i)
				if (!addLaboratoryFixture(fixture.base, game->getMod())) break;
			auto *project = new ResearchProject(fixture.research, fixture.research->getCost());
			project->setAssigned(100);
			fixture.base->addResearch(project);
		}
	}
	if (needManufacture)
	{
		fixture.base->setEngineers(2);
		if (activeManufacture && fixture.manufacture)
			fixture.production = addProductionFixture(
				fixture.base, fixture.manufacture, 94, 40, 13600);
	}
	if (manufactureName && std::string(manufactureName) == "STR_MANTIS_PROJECTOR")
	{
		if (RuleItem *ichor = game->getMod()->getItem("STR_ICHOR", false))
			fixture.base->getStorageItems()->addItem(ichor, 1188);
	}
	game->getSavedGame()->getBases()->push_back(fixture.base);
	trackFixtureBase(fixture.base);
	return fixture;
}

OperationsFixture makeGlobalManufactureFixture(Game *game)
{
	OperationsFixture fixture;
	if (!game || !game->getMod()) return fixture;
	CalypsoTutorial::get().disableForCampaign();
	fixture.manufacture = manufactureRule(game->getMod(), "STR_MAELSTROM_TORPEDOES");
	if (!fixture.manufacture || !prepareOperationsSave(game)) return OperationsFixture();

	Base *batumi = new Base(game->getMod());
	batumi->setName("Batumi");
	batumi->setEngineers(2);
	addProductionFixture(batumi, fixture.manufacture, 94, 40, 13600);
	Base *garni = new Base(game->getMod());
	garni->setName("Garni");
	addProductionFixture(garni, fixture.manufacture, 90, 20, 2400);
	game->getSavedGame()->getBases()->push_back(batumi);
	game->getSavedGame()->getBases()->push_back(garni);
	trackFixtureBase(batumi);
	trackFixtureBase(garni);
	fixture.base = batumi;
	return fixture;
}

/// Rebuild the minimum live campaign state needed after N06 deliberately
/// replaces SavedGame.  Game::setSavedGame() destroys the old object, so no
/// Base or project from the old save may be retained.  This copy keeps the
/// intended R&D owner alive while changing the save identity, and gives the
/// Geoscape a real base so its normal no-bases game-over path is not entered.
Base *makeNavigationReplacementBase(Game *game, const Base *sourceBase)
{
	if (!game || !game->getMod()) return nullptr;
	Mod *mod = game->getMod();
	Base *replacementBase = new Base(mod);
	if (sourceBase)
	{
		replacementBase->setName(sourceBase->getName());
		replacementBase->setLongitude(sourceBase->getLongitude());
		replacementBase->setLatitude(sourceBase->getLatitude());
		replacementBase->setId(sourceBase->getId());
		replacementBase->setScientists(sourceBase->getScientists());
		replacementBase->setEngineers(sourceBase->getEngineers());
	}
	else
	{
		replacementBase->setName("Batumi");
		replacementBase->setLongitude(0.0);
		replacementBase->setLatitude(0.5);
		replacementBase->setScientists(10);
		replacementBase->setEngineers(2);
	}

	// The source Base's facilities belong to the deleted save.  Recreate only
	// the GPL rule-backed capacity that the live owner needs, never its pointers.
	const bool hadResearch = sourceBase && !sourceBase->getResearch().empty();
	const bool hadProduction = sourceBase && !sourceBase->getProductions().empty();
	if (!addLaboratoryFixture(replacementBase, mod)
		&& hadResearch)
	{
		delete replacementBase;
		return nullptr;
	}
	if (hadProduction && !addWorkshopFixture(replacementBase, mod))
	{
		delete replacementBase;
		return nullptr;
	}

	// Preserve one active research/production record when present.  The rule
	// objects are owned by Mod and are looked up again by name; only the save
	// records are recreated.
	if (hadResearch)
	{
		const ResearchProject *source = sourceBase->getResearch().front();
		const RuleResearch *sourceRule = source ? source->getRules() : nullptr;
		RuleResearch *rule = sourceRule
			? mod->getResearch(sourceRule->getName(), false) : firstResearchRule(mod);
		if (rule)
		{
			auto *project = new ResearchProject(rule,
				source ? source->getCost() : rule->getCost());
			if (source)
			{
				project->setAssigned(source->getAssigned());
				project->setSpent(source->getSpent());
			}
			replacementBase->addResearch(project);
		}
	}
	if (hadProduction)
	{
		const Production *source = sourceBase->getProductions().front();
		const RuleManufacture *sourceRule = source ? source->getRules() : nullptr;
		RuleManufacture *rule = sourceRule
			? mod->getManufacture(sourceRule->getName(), false) : firstManufactureRule(mod);
		if (rule)
		{
			auto *production = new Production(rule, source ? source->getAmountTotal() : 0);
			if (source)
			{
				production->setAssignedEngineers(source->getAssignedEngineers());
				production->setTimeSpent(source->getTimeSpent());
				production->setInfiniteAmount(source->getInfiniteAmount());
				production->setSellItems(source->getSellItems());
				production->setFallback(source->isFallback());
			}
			replacementBase->addProduction(production);
		}
	}
	return replacementBase;
}

bool populateDiaryFixture(Game *game)
{
	if (!game || !game->getMod()) return false;
	// Check ownership before even changing tutorial state: the preview route
	// must be side-effect free when a real campaign is loaded.
	if (!prepareOperationsSave(game)) return false;
	CalypsoTutorial::get().disableForCampaign();
	if (!game->getSavedGame()->getResearchDiary().empty()) return true;
	const unsigned dates[][3] = {
		{2040, 9, 28}, {2040, 9, 28}, {2040, 8, 14}, {2040, 7, 21}
	};
	const std::vector<std::string> &names = game->getMod()->getResearchList();
	std::vector<RuleResearch *> rules;
	rules.reserve(4);
	for (const std::string &name : names)
	{
		if (RuleResearch *rule = game->getMod()->getResearch(name, false))
			rules.push_back(rule);
		if (rules.size() == 4) break;
	}
	if (rules.size() != 4) return false;
	SavedGame *save = game->getSavedGame();
	for (int i = 0; i < 4; ++i)
	{
		auto *entry = new ResearchDiaryEntry(rules[i]);
		entry->year = dates[i][0];
		entry->month = dates[i][1];
		entry->day = dates[i][2];
		entry->source.type = DiscoverySourceType::BASE;
		entry->source.name = "Batumi";
		entry->source.research = nullptr;
		entry->source.event = nullptr;
		entry->source.mission = nullptr;
		save->addResearchDiaryEntry(entry);
	}
	return true;
}

std::string dependencyItem(const RuleManufacture *rule)
{
	if (!rule) return std::string();
	if (!rule->getRequiredItems().empty())
		return rule->getRequiredItems().begin()->first->getType();
	if (!rule->getProducedItems().empty())
		return rule->getProducedItems().begin()->first->getType();
	return rule->getName();
}

/// Build the real nested owner used by N05. The owner target remains the
/// harness' active target; this child is pushed only after that target is live
/// so the navigation loop must unwind an actual native state before it can
/// finalize the owner. Error forms opt into the shared rail explicitly.
State *makeNavigationNestedOwner(Game *game, State *owner, int profile,
	bool &required)
{
	const auto *spec = calypsoHdRdNavigationProfileSpec(profile);
	required = spec != nullptr && spec->nested;
	if (!game || !owner || !spec || !spec->nested) return nullptr;
	if (profile == static_cast<int>(CalypsoHdRdNavigationProfile::NestedNewResearch)
		|| profile == static_cast<int>(CalypsoHdRdNavigationProfile::NestedNewProduction))
	{
		return new TechTreeViewerState(firstResearchRule(game->getMod()),
			firstManufactureRule(game->getMod()));
	}
	// The GPL R&D fragment owns the Basescape error interface and BACK17.SCR;
	// do not fall back to the sparse standard errorMessages interface.
	const auto *errorInterface = game->getMod()->getInterface("basescape");
	const auto *errorColor = errorInterface
		? errorInterface->getElement("errorMessage") : nullptr;
	const auto *errorPalette = errorInterface
		? errorInterface->getElement("errorPalette") : nullptr;
	if (!errorColor || !errorPalette) return nullptr;
	const auto form = calypsoRdErrorForm(owner, "CALYPSO R&D", "Nested native error owner.",
		"Return to the previous screen.", "OK");
	return new ErrorMessageState("Nested native error owner.", owner->getPalette(),
		static_cast<Uint8>(errorColor->color), "BACK17.SCR", errorPalette->color,
		0, owner, form);
}

std::string snapshotEscape(const std::string& value)
{
	std::string escaped;
	escaped.reserve(value.size() + 2);
	for (unsigned char ch : value)
	{
		switch (ch)
		{
		case '"': escaped += "\\\""; break;
		case '\\': escaped += "\\\\"; break;
		case '\b': escaped += "\\b"; break;
		case '\f': escaped += "\\f"; break;
		case '\n': escaped += "\\n"; break;
		case '\r': escaped += "\\r"; break;
		case '\t': escaped += "\\t"; break;
		default:
			if (ch < 0x20)
			{
				static const char hex[] = "0123456789abcdef";
				escaped += "\\u00";
				escaped += hex[(ch >> 4) & 0xf];
				escaped += hex[ch & 0xf];
			}
			else escaped += static_cast<char>(ch);
			break;
		}
	}
	return escaped;
}

void appendSnapshotString(std::ostringstream& out, const char* key,
	const std::string& value, bool& first)
{
	if (!first) out << ',';
	first = false;
	out << '"' << key << "\":\"" << snapshotEscape(value) << '"';
}

std::string visualColorHex(std::uint32_t value)
{
	static const char digits[] = "0123456789ABCDEF";
	std::string result(8, '0');
	for (int i = 7; i >= 0; --i)
	{
		result[static_cast<std::size_t>(i)] = digits[value & 0xFu];
		value >>= 4;
	}
	return result;
}

void appendVisualRect(std::ostringstream& out, const char* key,
	const CalypsoHdOperationsRect& rect, bool& first)
{
	if (!rect.valid()) return;
	if (!first) out << ',';
	first = false;
	out << '"' << key << "\":{\"x\":" << rect.x << ",\"y\":" << rect.y
		<< ",\"width\":" << rect.w << ",\"height\":" << rect.h << '}';
}

void appendVisualAction(std::ostringstream& out,
	const CalypsoHdOperationsAction& action,
	const CalypsoHdOperationsStyle& style, bool framePresented, bool& first)
{
	if (!first) out << ',';
	first = false;
	out << '{';
	bool fieldFirst = true;
	appendSnapshotString(out, "id", action.id, fieldFirst);
	appendSnapshotString(out, "tone", action.tone, fieldFirst);
	appendVisualRect(out, "visible", action.visible, fieldFirst);
	appendVisualRect(out, "hit", action.hit, fieldFirst);
	if (!fieldFirst) out << ',';
	fieldFirst = false;
	out << "\"colors\":{";
	bool colorFirst = true;
	const auto palette = calypsoHdOperationsActionPalette(style, action.tone, action.state);
	appendSnapshotString(out, "fill", visualColorHex(palette.fill), colorFirst);
	appendSnapshotString(out, "border", visualColorHex(palette.border), colorFirst);
	appendSnapshotString(out, "text", visualColorHex(palette.text), colorFirst);
	out << '}';
	if (!fieldFirst) out << ',';
	out << "\"state\":{";
	bool stateFirst = true;
	out << "\"visible\":" << (action.state.visible ? "true" : "false")
		<< ",\"hovered\":" << (action.state.hovered ? "true" : "false")
		<< ",\"focused\":" << (action.state.focused ? "true" : "false")
		<< ",\"selected\":" << (action.state.selected ? "true" : "false")
		<< ",\"disabled\":" << (action.state.disabled ? "true" : "false") << '}';
	if (!fieldFirst) out << ',';
	out << "\"observed\":" << (framePresented && action.visible.valid() ? "true" : "false")
		<< ",\"paintPath\":\"collectAction\"}";
}

struct CalypsoHdLocaleStressNameSpec
{
	const char* id;
	const char* original;
};

bool calypsoHdLocaleStressModelContains(
	const CalypsoHdOperationsModel& model, const std::string& value)
{
	const auto rowContains = [&value](const CalypsoHdOperationsRow& row) {
		for (const auto& text : row.values)
			if (text == value) return true;
		for (const auto& cell : row.cells)
			if (cell.value == value) return true;
		return false;
	};
	const auto collectionContains = [&rowContains](const CalypsoHdOperationsCollection& collection) {
		const std::size_t start = std::min(collection.scrollOffset, collection.rows.size());
		const std::size_t end = std::min(collection.rows.size(), start + collection.visibleRows);
		for (std::size_t i = start; i < end; ++i)
			if (rowContains(collection.rows[i])) return true;
		return false;
	};
	if (collectionContains(model.collection)) return true;
	for (const auto& region : model.regions)
	{
		if (region.label == value || region.previewContent == value) return true;
		for (const auto& field : region.fields)
			if (field.label == value || field.value == value) return true;
		if (collectionContains(region.collection)) return true;
		for (const auto& action : region.actions)
			if (action.label == value) return true;
	}
	if (model.detail.identity.title == value || model.detail.identity.subtitle == value) return true;
	for (const auto& field : model.summaryFields)
		if (field.label == value || field.value == value) return true;
	for (const auto& metric : model.detail.metrics)
		if (metric.label == value || metric.value == value) return true;
	const auto actionContains = [&value](const CalypsoHdOperationsAction& action) {
		return action.label == value;
	};
	for (const auto& action : model.toolbarActions)
		if (actionContains(action)) return true;
	for (const auto& action : model.footerActions)
		if (actionContains(action)) return true;
	for (const auto& action : model.detail.actions)
		if (actionContains(action)) return true;
	for (const auto& control : model.controls)
	{
		if (control.label == value || control.displayValue == value) return true;
		for (const auto& option : control.options)
			if (option == value) return true;
	}
	for (const auto& action : model.navigation.actions)
		if (actionContains(action)) return true;
	return false;
}

void appendLocaleStressEvidence(std::ostringstream& out,
	const CalypsoHdOperationsModel& model, const SavedGame* save,
	const Language* language)
{
	// The originals are the ordinary en-US values in calypso-test-master.rul.
	// Keep this table limited to fixture-owned rule identities: a translated
	// string alone is never accepted as locale evidence.
	static const CalypsoHdLocaleStressNameSpec names[] = {
		{"STR_MAELSTROM_BATTERY", "MAELSTROM BATTERY"},
		{"STR_DECANTED_CORPSE", "DECANTED CORPSE"},
		{"STR_MAELSTROM_TORPEDOES", "MAELSTROM TORPEDOES"},
		{"STR_MANTIS_PROJECTOR", "MANTIS PROJECTOR"},
		{"STR_CALYPSO_RD_ITEM", "CALYPSO R&D ITEM"},
		// ICHOR is the real required item owner in the dependency route.  It
		// has no baseline language entry, so L08 supplies its expanded label
		// through the same harness-only overlay as the named rule identities.
		{"STR_ICHOR", "ICHOR"},
	};
	out << ",\"localeStress\":{\"names\":[";
	bool first = true;
	for (const auto& spec : names)
	{
		if (!language) continue;
		const std::string expanded = language->getString(spec.id);
		if (expanded.empty() || expanded.size() < std::string(spec.original).size() * 2
			|| !calypsoHdLocaleStressModelContains(model, expanded))
			continue;
		if (!first) out << ',';
		first = false;
		out << '{';
		bool fieldFirst = true;
		appendSnapshotString(out, "id", spec.id, fieldFirst);
		appendSnapshotString(out, "original", spec.original, fieldFirst);
		appendSnapshotString(out, "expanded", expanded, fieldFirst);
		out << '}';
	}
	out << "],\"funds\":" << (save ? save->getFunds() : 0) << ",\"infinity\":";
	bool infinity = false;
	const auto inspectAction = [&infinity](const CalypsoHdOperationsAction& action) {
		if (action.id == "infinity" && action.widget != nullptr && action.state.visible
			&& action.label.find("∞") != std::string::npos)
			infinity = true;
	};
	for (const auto& region : model.regions)
		for (const auto& action : region.actions) inspectAction(action);
	out << (infinity ? "true" : "false") << '}';
}

void appendVisualEvidence(std::ostringstream& out,
	const CalypsoHdOperationsModel& model, const SavedGame* save,
	const Language* language, bool fontsReady)
{
	out << ",\"visual\":{";
	bool first = true;
	appendSnapshotString(out, "locale", Options::language, first);
	appendSnapshotString(out, "fixtureState",
		calypsoHdRdInteractionVisualState(calypsoHarnessSession().rdActiveProfile), first);
	if (!first) out << ',';
	first = false;
	out << "\"fontCoverage\":\"" << (fontsReady ? "complete" : "incomplete") << '\"';
	if (!first) out << ',';
	first = false;
	out << "\"actions\":[";
	bool actionFirst = true;
	for (const auto& action : model.toolbarActions) appendVisualAction(out, action, model.style, fontsReady, actionFirst);
	for (const auto& action : model.footerActions) appendVisualAction(out, action, model.style, fontsReady, actionFirst);
	for (const auto& action : model.detail.actions) appendVisualAction(out, action, model.style, fontsReady, actionFirst);
	for (const auto& region : model.regions)
		for (const auto& action : region.actions) appendVisualAction(out, action, model.style, fontsReady, actionFirst);
	for (const auto& action : model.navigation.actions) appendVisualAction(out, action, model.style, fontsReady, actionFirst);
	out << "]";
	appendLocaleStressEvidence(out, model, save, language);
	out << '}';
}

void appendResearchSnapshot(std::ostringstream& out, const Base* base)
{
	out << "\"research\":[";
	const auto& projects = base->getResearch();
	for (std::size_t i = 0; i < projects.size(); ++i)
	{
		if (i) out << ',';
		const ResearchProject* project = projects[i];
		out << '{';
		bool first = true;
		appendSnapshotString(out, "id",
			project && project->getRules() ? project->getRules()->getName() : "", first);
		if (!first) out << ',';
		out << "\"assigned\":" << (project ? project->getAssigned() : 0)
			<< ",\"spent\":" << (project ? project->getSpent() : 0)
			<< ",\"cost\":" << (project ? project->getCost() : 0) << '}';
	}
	out << ']';
}

void appendProductionSnapshot(std::ostringstream& out, const Base* base)
{
	out << "\"production\":[";
	const auto& productions = base->getProductions();
	for (std::size_t i = 0; i < productions.size(); ++i)
	{
		if (i) out << ',';
		const Production* production = productions[i];
		out << '{';
		bool first = true;
		appendSnapshotString(out, "id",
			production && production->getRules() ? production->getRules()->getName() : "", first);
		if (!first) out << ',';
		out << "\"assignedEngineers\":"
			<< (production ? production->getAssignedEngineers() : 0)
			<< ",\"amountTotal\":" << (production ? production->getAmountTotal() : 0)
			<< ",\"amountProduced\":" << (production ? production->getAmountProduced() : 0)
			<< ",\"infinite\":"
			<< (production && production->getInfiniteAmount() ? "true" : "false")
			<< ",\"sellItems\":"
		<< (production && production->getSellItems() ? "true" : "false")
		<< ",\"fallback\":"
		<< (production && production->isFallback() ? "true" : "false") << '}';
	}
	out << ']';
}

void appendStorageSnapshot(std::ostringstream& out, const Base* base)
{
	out << "\"storage\":[";
	const ItemContainer* storage = base->getStorageItems();
	const auto* contents = storage ? storage->getContents() : nullptr;
	bool first = true;
	if (contents)
	{
		for (const auto& entry : *contents)
		{
			if (!first) out << ',';
			first = false;
			out << '{';
			bool fieldFirst = true;
			appendSnapshotString(out, "id", entry.first ? entry.first->getType() : "", fieldFirst);
			if (!fieldFirst) out << ',';
			out << "\"count\":" << entry.second << '}';
		}
	}
	out << ']';
}

std::string calypsoHdHarnessOperationsSnapshot(const char* capturePhase = "live")
{
	const CalypsoHarnessSession& session = calypsoHarnessSession();
	if (!calypsoHarnessHostUp(session)) return {};
	Game* game = getCurrentGame();
	if (!game) return {};
	std::ostringstream out;
	out << '{';
	bool first = true;
	appendSnapshotString(out, "capturePhase", capturePhase, first);
	if (!first) out << ',';
	first = false;
	out << "\"harnessGeneration\":" << session.generation
		<< ",\"rdProfile\":" << session.rdActiveProfile;
	appendSnapshotString(out, "rngSeed", std::to_string(RNG::getSeed()), first);
	const State* capturedTop = game->getTopState();
	if (capturedTop)
	{
		const std::string stateName = typeid(*capturedTop).name();
		appendSnapshotString(out, "topState", stateName, first);
		appendSnapshotString(out, "currentTopState", stateName, first);
	}
	if (!first) out << ',';
	out << "\"savedGame\":{";
	SavedGame* save = game->getSavedGame();
	out << "\"funds\":" << (save ? save->getFunds() : 0);
	out << ",\"bases\":[";
	if (save && save->getBases())
	{
		const auto* bases = save->getBases();
		for (std::size_t i = 0; i < bases->size(); ++i)
		{
			if (i) out << ',';
			const Base* base = (*bases)[i];
			out << '{';
			bool baseFirst = true;
			appendSnapshotString(out, "name", base ? base->getName() : "", baseFirst);
			if (!baseFirst) out << ',';
			out << "\"scientists\":" << (base ? base->getScientists() : 0)
				<< ",\"engineers\":" << (base ? base->getEngineers() : 0)
				<< ",\"allocatedScientists\":"
				<< (base ? base->getAllocatedScientists() : 0)
				<< ",\"allocatedEngineers\":"
				<< (base ? base->getAllocatedEngineers() : 0) << ',';
			out << "\"availableWorkshops\":" << (base ? base->getAvailableWorkshops() : 0) << ',';
			out << "\"usedWorkshops\":" << (base ? base->getUsedWorkshops() : 0) << ',';
			out << "\"freeWorkshops\":" << (base ? base->getFreeWorkshops() : 0) << ',';
			out << "\"availableLaboratories\":" << (base ? base->getAvailableLaboratories() : 0) << ',';
			out << "\"usedLaboratories\":" << (base ? base->getUsedLaboratories() : 0) << ',';
			if (base) appendResearchSnapshot(out, base);
			else out << "\"research\":[]";
			out << ',';
			if (base) appendProductionSnapshot(out, base);
			else out << "\"production\":[]";
			out << ',';
			if (base) appendStorageSnapshot(out, base);
			else out << "\"storage\":[]";
			out << '}';
		}
	}
	out << "]}";
	const CalypsoHdOperationsRenderer* renderer = calypsoHdOperationsActiveRenderer();
	// Nested owners (notably F15 TechTreeViewerState) can be rendered by the
	// exact top-state adapter after another contributor updated the diagnostic
	// active-renderer pointer. Never serialize that stale contributor: resolve
	// the registered adapter by the live top state and accept it only when it is
	// an operations renderer.
	if (!renderer || renderer->topState() != game->getTopState())
	{
		const auto *adapter = CalypsoHdUiOverlay::instance().adapterForState(
			game->getTopState());
		renderer = dynamic_cast<const CalypsoHdOperationsRenderer*>(adapter);
	}
	if (renderer && renderer->topState() == game->getTopState())
	{
		out << ",\"operationsModel\":"
			<< calypsoHdOperationsModelSnapshotJson(renderer->model());
		appendVisualEvidence(out, renderer->model(), game->getSavedGame(), game->getLanguage(),
			renderer->physicalReady() && renderer->completeFrameReady());
	}
	else
	{
		// ErrorMessageState is not an operations renderer, but an explicit R&D
		// error popup may own the same real shared rail. Publish only that
		// state-owned chrome for input diagnostics; never synthesize actions or
		// treat the popup as an ordinary operations screen.
		const auto *adapter = CalypsoHdUiOverlay::instance().adapterForState(
			game->getTopState());
		const auto *error = dynamic_cast<const CalypsoErrorPopupUi*>(adapter);
		const auto *chrome = error ? error->strategicChrome() : nullptr;
		if (chrome)
		{
			CalypsoHdOperationsModel model;
			model.ownerState = game->getTopState();
			model.presentation = "nested-error";
			model.provenance = "CalypsoErrorPopupUi::strategicChrome";
			model.familyId = 34;
			chrome->populateModel(model);
			out << ",\"operationsModel\":"
				<< calypsoHdOperationsModelSnapshotJson(model)
				<< ",\"visual\":null";
		}
		else out << ",\"operationsModel\":null,\"visual\":null";
	}
	out << ",\"navigationDiagnostics\":"
		<< calypsoHdNavigationSnapshotJson();
	out << '}';
	return out.str();
}

} // namespace

CalypsoHarnessSession& calypsoHarnessSession()
{
	return g_harnessSession;
}

bool calypsoHdHarnessAllowsUnregisteredNativeArticle(const State *state)
{
	if (!state) return false;
	const CalypsoHarnessSession& session = calypsoHarnessSession();
	if (!calypsoHarnessHostUp(session) || !calypsoHarnessTargetUp(session)
		|| session.rdActiveProfile != 203
		|| session.rdActiveScenario != static_cast<int>(CalypsoHarnessScenario::F14GlobalProduction))
		return false;
	Game *game = getCurrentGame();
	if (!game || game->getTopState() != state) return false;
	const auto& overlay = CalypsoHdUiOverlay::instance();
	if (overlay.hasRegisteredState(state)) return false;
	return dynamic_cast<const ArticleStateCraftWeapon*>(state) != nullptr;
}

const char *calypsoRdLocaleStressRule(const std::string &language)
{
	if (language == "en-US") return "calypso-rd-locale-en-US";
	if (language == "en-GB") return "calypso-rd-locale-en-GB";
	if (language == "ru") return "calypso-rd-locale-ru";
	return nullptr;
}

void calypsoHdHarnessDomShow()
{
	EM_ASM({ if (globalThis.__calypsoHdHarnessShow) globalThis.__calypsoHdHarnessShow(); });
}

void calypsoHdHarnessDomHide()
{
	EM_ASM({ if (globalThis.__calypsoHdHarnessHide) globalThis.__calypsoHdHarnessHide(); });
}

State* calypsoHarnessCreateTarget(CalypsoHarnessScenario id)
{
	CalypsoHarnessSession& session = calypsoHarnessSession();
	session.rdActiveScenario = static_cast<int>(id);
	const int profile = calypsoHarnessTakeRdProfile(session);
	const int scenario = static_cast<int>(id);
	if (profile == 305 && scenario == 91)
		CalypsoF09ResearchUi::queueHarnessQueueListSetup(0, 293);
	if ((profile >= 306 && profile <= 309 || profile == 9 || profile == 10 || profile == 14)
		&& scenario == 92)
		CalypsoF09ResearchUi::queueHarnessCatalogueListSetup(0);
	if (profile != 0 && !((profile < 100 && scenario >= 91 && scenario <= 93)
		|| (profile >= 100 && profile < 200 && scenario >= 94 && scenario <= 98)
		|| (profile >= 200 && profile < 220 && scenario >= 99 && scenario <= 101)
		|| (profile >= 220 && profile <= 225 && scenario == 98)
		|| (profile >= 300 && profile < 350
			&& ((scenario >= 91 && scenario <= 93) || scenario == 100))
		|| (profile >= 320 && profile <= 323 && scenario >= 91 && scenario <= 101)
		|| (profile >= 324 && profile <= 331 && scenario >= 91 && scenario <= 101)
		|| (profile >= 350 && profile <= 359 && (scenario == 93 || scenario == 97))))
	{
		Log(LOG_ERROR) << "R&D acceptance profile does not support this harness route";
		return nullptr;
	}
	// Locale requests are consumed only after the previous target has been
	// torn down.  This keeps a queued switch from restoring the requested
	// language while the old target is being released.
	const std::string requestedLanguage = calypsoHarnessTakeRdLanguage(session);
	if (!requestedLanguage.empty())
	{
		if (!g_rdRestoreLanguageSet)
		{
			g_rdRestoreLanguage = Options::language;
			g_rdRestoreLanguageSet = true;
		}
		Options::language = requestedLanguage;
		if (Game* game = getCurrentGame())
		{
			game->loadLanguages();
			// The ordinary fixture rule fragments intentionally keep canonical
			// labels short.  L08 overlays only the real rule identities after the
			// normal locale load, so every route paints the expanded value while
			// baseline captures retain their approved text.
			if (session.rdLocaleStress)
			{
				if (const char *stressRule = calypsoRdLocaleStressRule(requestedLanguage))
					game->getLanguage()->loadRule(game->getMod()->getExtraStrings(), stressRule);
			}
		}
		if (Options::language != requestedLanguage)
		{
			if (Game* game = getCurrentGame())
			{
				Options::language = g_rdRestoreLanguage;
				game->loadLanguages();
			}
			g_rdRestoreLanguage.clear();
			g_rdRestoreLanguageSet = false;
			return nullptr;
		}
	}
	switch (id)
	{
	case CalypsoHarnessScenario::F33Abandon:
		return new AbandonGameState(OPT_GEOSCAPE);
	case CalypsoHarnessScenario::F03Dismantle:
	{
		Game* game = getCurrentGame();
		if (!game || !game->getMod()) return nullptr;
		// Find suitable facility rule BEFORE capturing SavedGame lease (avoid leaving modified save on failure)
		const RuleBaseFacility* rule = nullptr;
		for (const std::string& name : game->getMod()->getBaseFacilitiesList())
		{
			const RuleBaseFacility* candidate = game->getMod()->getBaseFacility(name, false);
			if (candidate && !candidate->isLift() && candidate->getRefundValue() >= 0
				&& candidate->getBuildCostItems().empty())
			{
				rule = candidate;
				break;
			}
		}
		if (!rule) return nullptr;
		// Capture SavedGame lease only after successful rule lookup
		if (!g_harnessSaveLease.active) {
			if (game->getSavedGame()) {
				// Existing save: keep same object, just save funds
				g_harnessSaveLease.original = game->getSavedGame();
				g_harnessSaveLease.originalFunds = game->getSavedGame()->getFunds();
				g_harnessSaveLease.fixture = nullptr;
			} else {
				// No save: create fixture and track it
				SavedGame* fixture = new SavedGame();
				game->setSavedGame(fixture);
				g_harnessSaveLease.original = nullptr;
				g_harnessSaveLease.fixture = fixture;
				g_harnessSaveLease.originalFunds = 0;
			}
			g_harnessSaveLease.active = true;
		}
		// Temporarily set funds for fixture (only if we have a save)
		if (game->getSavedGame()) game->getSavedGame()->setFunds(6800000);

		Base* base = new Base(game->getMod());
		BaseFacility* facility = new BaseFacility(rule, base);
		facility->setBuildTime(rule->getBuildTime());
		base->getFacilities()->push_back(facility);
		BaseView* view = new BaseView(192, 192, 0, 8);
		view->setBase(base);
		auto* state = new DismantleFacilityState(base, view, facility);
		state->calypsoOwnHarnessFixture();
		return state;
	}
	case CalypsoHarnessScenario::F03BuildFacilities:
	{
		Game* game = getCurrentGame();
		if (!game || !game->getMod()) return nullptr;
		if (!g_harnessSaveLease.active) {
			if (game->getSavedGame()) {
				g_harnessSaveLease.original = game->getSavedGame();
				g_harnessSaveLease.originalFunds = game->getSavedGame()->getFunds();
				g_harnessSaveLease.fixture = nullptr;
			} else {
				SavedGame* fixture = new SavedGame();
				game->setSavedGame(fixture);
				g_harnessSaveLease.original = nullptr;
				g_harnessSaveLease.fixture = fixture;
				g_harnessSaveLease.originalFunds = 0;
			}
			g_harnessSaveLease.active = true;
		}
		if (game->getSavedGame())
		{
			game->getSavedGame()->setFunds(6800000);
			// Deterministic chooser rows: on a fixture-only save every
			// requirement-gated facility lists enabled in mod order, so the
			// contract rows and the engine rows agree. A live campaign save
			// is never touched: debug mode stays off there.
			if (g_harnessSaveLease.fixture == game->getSavedGame()
				&& !game->getSavedGame()->getDebugMode())
				game->getSavedGame()->setDebugMode();
		}
		Base* base = new Base(game->getMod());
		// The fixture globe intentionally lives for the harness process
		// lifetime (F21 defense precedent); the covered state owns the base.
		const int sw = 320, sh = 200;
		Globe* globe = new Globe(game, (sw - 64) / 2, sh / 2, sw - 64, sh, 0, 0);
		BasescapeState* covered = new BasescapeState(base, globe);
		auto* state = new BuildFacilitiesState(base, covered);
		state->calypsoOwnHarnessFixture();
		return state;
	}
	case CalypsoHarnessScenario::F03PlaceFacility:
	{
		Game* game = getCurrentGame();
		if (!game || !game->getMod()) return nullptr;
		if (!g_harnessSaveLease.active) {
			if (game->getSavedGame()) {
				g_harnessSaveLease.original = game->getSavedGame();
				g_harnessSaveLease.originalFunds = game->getSavedGame()->getFunds();
				g_harnessSaveLease.fixture = nullptr;
			} else {
				SavedGame* fixture = new SavedGame();
				game->setSavedGame(fixture);
				g_harnessSaveLease.original = nullptr;
				g_harnessSaveLease.fixture = fixture;
				g_harnessSaveLease.originalFunds = 0;
			}
			g_harnessSaveLease.active = true;
		}
		if (game->getSavedGame()) game->getSavedGame()->setFunds(6800000);
		const RuleBaseFacility* rule = game->getMod()->getBaseFacility("STR_LIVING_QUARTERS", false);
		if (!rule)
		{
			for (const std::string& name : game->getMod()->getBaseFacilitiesList())
			{
				const RuleBaseFacility* candidate = game->getMod()->getBaseFacility(name, false);
				if (candidate && !candidate->isLift())
				{
					rule = candidate;
					break;
				}
			}
		}
		if (!rule) return nullptr;
		// Real native owners for the placement slice; the lease frees the
		// base because PlaceFacilityState never owns it.
		Base* placeBase = new Base(game->getMod());
		g_harnessSaveLease.fixtureBase = placeBase;
		return new PlaceFacilityState(placeBase, rule);
	}
	case CalypsoHarnessScenario::F04SackSoldier:
		return new CraftErrorState(nullptr, "Dismiss soldier confirmation.");
	case CalypsoHarnessScenario::F18CraftError:
		return new CraftErrorState(nullptr, "Craft cannot complete the assigned operation.");
	case CalypsoHarnessScenario::F18LowFuel:
		return new CraftErrorState(nullptr, "Craft is low on fuel and returning to base.");
	case CalypsoHarnessScenario::F18NotEnoughPilots:
		return new CraftErrorState(nullptr, "Not enough pilots for this craft.");
	case CalypsoHarnessScenario::F19DogfightError:
		return new CraftErrorState(nullptr, "Craft cannot engage the target now.");
	case CalypsoHarnessScenario::F20ConfirmLanding:
		return new CraftErrorState(nullptr, "Confirm landing at selected site?");
	case CalypsoHarnessScenario::F20ConfirmCydonia:
		return new CraftErrorState(nullptr, "Confirm final mission?");
	case CalypsoHarnessScenario::F24ResearchRequired:
		return new CraftErrorState(nullptr, "Additional research required.");
	case CalypsoHarnessScenario::F24ResearchComplete:
		return new CraftErrorState(nullptr, "Research completed successfully.");
	case CalypsoHarnessScenario::F28AbortMission:
		return new AbortMissionState(nullptr, nullptr);
	case CalypsoHarnessScenario::F28ConfirmEnd:
		return new CraftErrorState(nullptr, "Confirm end of mission?");
	case CalypsoHarnessScenario::F17UfoLost:
		return new CraftErrorState(nullptr, "Contact with UFO has been lost.");
	case CalypsoHarnessScenario::F17UfoDetected:
	{
		// The S01 board renders real contact data, so this fixture builds a
		// REAL UfoDetectedState (the old CraftErrorState stand-in could not
		// provide it). Deterministic: first mod USO rule, one fixture base,
		// fixed coordinates; the unpushed GeoscapeState and fixture campaign
		// objects intentionally live for the harness process lifetime (F21
		// defense precedent).
		Game* game = getCurrentGame();
		if (!game || !game->getMod() || game->getMod()->getUfosList().empty())
			return nullptr;
		SavedGame* save = game->getSavedGame();
		if (!save)
		{
			save = new SavedGame();
			game->setSavedGame(save);
		}
		Base* base = new Base(game->getMod());
		base->setLongitude(0.0);
		base->setLatitude(0.5);
		save->getBases()->push_back(base);
		RuleUfo* rule = game->getMod()->getUfo(game->getMod()->getUfosList().front(), false);
		if (!rule) return nullptr;
		Ufo* ufo = new Ufo(rule, 1);
		// The vanilla ctor dereferences the mission (hyperwave rows), so the
		// fixture binds a live mission like the F21 defense fixture does.
		const RuleAlienMission* missionRule = nullptr;
		for (const std::string& name : game->getMod()->getAlienMissionList())
		{
			missionRule = game->getMod()->getAlienMission(name, false);
			if (missionRule) break;
		}
		if (missionRule)
		{
			ufo->setMissionInfo(new AlienMission(*missionRule),
				game->getMod()->getUfoTrajectory(UfoTrajectory::RETALIATION_ASSAULT_RUN, true));
		}
		// OpenXcom latitude increases southward. Keep the fixture distinctly
		// south-east of the base so an inverted vertical projection is visible.
		ufo->setLongitude(0.2);
		ufo->setLatitude(0.72);
		// The vanilla ctor fires the "ufo.detected" tutorial step; disarm the
		// campaign tutorial so it cannot push itself above the harness target.
		CalypsoTutorial::get().disableForCampaign();
		GeoscapeState* geoscape = new GeoscapeState();
		return new UfoDetectedState(ufo, geoscape, true, false);
	}
	case CalypsoHarnessScenario::F17MissionDetected:
		return new CraftErrorState(nullptr, "Alien mission detected nearby.");
	case CalypsoHarnessScenario::F22TrainingFinished:
		return new CraftErrorState(nullptr, "Training program has finished.");
	case CalypsoHarnessScenario::F30NoExperience:
		return new NoExperienceState();
	case CalypsoHarnessScenario::F24ProductionComplete:
		return new CraftErrorState(nullptr, "Manufacturing project completed.");
	case CalypsoHarnessScenario::F05SoldierTransform:
		return new CraftErrorState(nullptr, "Soldier transformation is now available.");
	case CalypsoHarnessScenario::F06SoldierDiary:
		return new CraftErrorState(nullptr, "New diary entry has been recorded.");
	case CalypsoHarnessScenario::F12TransferConfirm:
		return new CraftErrorState(nullptr, "Confirm transfer of selected items?");
	case CalypsoHarnessScenario::F13Containment:
	{
		OperationsFixture fixture = makeOperationsFixture(getCurrentGame(), false, false);
		return fixture.base ? new ManageAlienContainmentState(fixture.base, 0, OPT_GEOSCAPE) : nullptr;
	}
	case CalypsoHarnessScenario::F24ItemsArriving:
	{
		Game *game = getCurrentGame();
		if (!game || !game->getMod() || !prepareOperationsSave(game)) return nullptr;
		// ItemsArrivingState reads the campaign transfer queues during
		// construction; the empty deterministic save is a valid no-arrivals
		// fixture and keeps the native route available.
		GeoscapeState *geoscape = new GeoscapeState();
		return new ItemsArrivingState(geoscape);
	}
	case CalypsoHarnessScenario::F09ResearchQueue:
	{
		OperationsFixture fixture = makeOperationsFixture(getCurrentGame(), true, false,
			"STR_MAELSTROM_BATTERY", nullptr, true, false);
		return fixture.base ? new ResearchState(fixture.base) : nullptr;
	}
	case CalypsoHarnessScenario::F09ResearchCatalogue:
	{
		OperationsFixture fixture = makeOperationsFixture(getCurrentGame(), false, false);
		return fixture.base ? new NewResearchListState(fixture.base, true) : nullptr;
	}
	case CalypsoHarnessScenario::F09ResearchStaffing:
	{
		OperationsFixture fixture = makeOperationsFixture(getCurrentGame(), true, false,
			"STR_MAELSTROM_BATTERY", nullptr, false, false);
		if (fixture.base && calypsoHarnessSession().rdActiveProfile == 0)
		{
			fixture.base->setScientists(10);
			if (!addLaboratoryFixture(fixture.base, getCurrentGame()->getMod()))
				return nullptr;
		}
		return fixture.base ? new ResearchInfoState(fixture.base, fixture.research) : nullptr;
	}
	case CalypsoHarnessScenario::F10ProductionQueue:
	{
		OperationsFixture fixture = makeOperationsFixture(getCurrentGame(), false, true,
			nullptr, "STR_MAELSTROM_TORPEDOES", false, true);
		return fixture.base ? new ManufactureState(fixture.base) : nullptr;
	}
	case CalypsoHarnessScenario::F10ProductionCatalogue:
	{
		OperationsFixture fixture = makeOperationsFixture(getCurrentGame(), false, true,
			nullptr, "STR_MAELSTROM_TORPEDOES", false, false);
		return fixture.base ? new NewManufactureListState(fixture.base) : nullptr;
	}
	case CalypsoHarnessScenario::F10ProductionRequirements:
	{
		OperationsFixture fixture = makeOperationsFixture(getCurrentGame(), false, true,
			nullptr, "STR_MANTIS_PROJECTOR", false, false);
		return fixture.base ? new ManufactureStartState(fixture.base, fixture.manufacture) : nullptr;
	}
	case CalypsoHarnessScenario::F10ProductionControls:
	{
		OperationsFixture fixture = makeOperationsFixture(getCurrentGame(), false, true,
			nullptr, "STR_MAELSTROM_TORPEDOES", false, true);
		if (!fixture.base || !fixture.manufacture) return nullptr;
		// Navigation profile 107 is a real new-production owner: the native
		// constructor creates its unstarted Production from the rule. Existing
		// profiles provide a live Production record and use the modifying ctor.
		return fixture.production
			? new ManufactureInfoState(fixture.base, fixture.production)
			: new ManufactureInfoState(fixture.base, fixture.manufacture);
	}
	case CalypsoHarnessScenario::F10ProductionDependencies:
	{
		OperationsFixture fixture = makeOperationsFixture(getCurrentGame(), false, true,
			nullptr, "STR_MAELSTROM_TORPEDOES", false, false);
		return fixture.base ? new ManufactureDependenciesTreeState(dependencyItem(fixture.manufacture)) : nullptr;
	}
	case CalypsoHarnessScenario::F14GlobalResearch:
	{
		OperationsFixture fixture = makeOperationsFixture(getCurrentGame(), true, false,
			"STR_DECANTED_CORPSE", nullptr, true, false);
		return fixture.base ? new GlobalResearchState(true) : nullptr;
	}
	case CalypsoHarnessScenario::F14ResearchDiary:
	{
		const bool ready = profile >= 200
			? makeOperationsFixture(getCurrentGame(), false, false).base != nullptr
			: populateDiaryFixture(getCurrentGame());
		return ready ? new GlobalResearchDiaryState() : nullptr;
	}
	case CalypsoHarnessScenario::F14GlobalProduction:
	{
		OperationsFixture fixture = profile >= 200
			? makeOperationsFixture(getCurrentGame(), false, true)
			: makeGlobalManufactureFixture(getCurrentGame());
		const bool openedFromBasescape = profile == 208;
		return fixture.base ? new GlobalManufactureState(openedFromBasescape) : nullptr;
	}
	case CalypsoHarnessScenario::F15TechTree:
	{
		OperationsFixture fixture = makeOperationsFixture(getCurrentGame(), false, false,
			"STR_DECANTED_CORPSE", nullptr, false, false);
		return fixture.research ? new TechTreeViewerState(fixture.research, nullptr) : nullptr;
	}
	default:
		break;
	}
	if (State* f21 = calypsoF21HarnessCreateTarget(id)) return f21;
	if (State* screen = calypsoHdScreenHarnessCreateTarget(id)) return screen;
	return nullptr;
}

CalypsoHdHarnessHostState::CalypsoHdHarnessHostState(CalypsoHarnessScenario scenario)
	: _scenario(scenario)
{
	_screen = true; // opaque: the blit walk stops here, above every lower state
}

void CalypsoHdHarnessHostState::init()
{
	State::init();
}

void CalypsoHdHarnessHostState::think()
{
	// Native operations states close through their normal handlers and need
	// not know about this diagnostic host. Reaching the opaque host itself
	// proves the target (and every nested child) has left the state stack.
	Game* game = getCurrentGame();
	CalypsoHarnessSession& session = calypsoHarnessSession();
	if (game && calypsoHarnessHostUp(session) && !calypsoHarnessTargetUp(session)
		&& session.pendingSwitch.scenario
		&& game->getTopState() == this)
	{
		const auto next = calypsoHarnessTakeSwitch(session);
		calypsoHdHarnessClose();
		game->popState();
		if (next.scenario != 0)
		{
			calypsoHdHarnessOpen(static_cast<CalypsoHarnessScenario>(next.scenario),
				next.layout, next.sideBySide);
			calypsoHarnessSetMotionDisabled(calypsoHarnessSession(), next.motionDisabled);
			calypsoHarnessSetMotionHold(calypsoHarnessSession(), next.motionHoldPct);
		}
		return;
	}
	if (calypsoHarnessReturnedToHost(calypsoHarnessSession(), this,
		game ? game->getTopState() : nullptr))
		calypsoHdHarnessClose();
	// The host only thinks while it IS the top state -- i.e. the target preview
	// has closed and calypsoHdHarnessClose() cleared the session. Pop the host
	// (it pops itself from its own think, the same pattern states use for
	// Escape/cancel), leaving the previous game state intact.
	if (!calypsoHarnessHostUp(session))
	{
		// Some legacy targets close only the pure session in their destructor.
		// Destruction has finished before host think, so owned fixtures are now safe to release.
		if (g_harnessSaveLease.active || g_rdRestoreOptions) calypsoHdHarnessClose();
		const auto next = calypsoHarnessTakeSwitch(calypsoHarnessSession());
		if (Game* g = getCurrentGame())
		{
			g->popState();
			if (next.scenario != 0)
			{
				calypsoHdHarnessOpen(static_cast<CalypsoHarnessScenario>(next.scenario),
					next.layout, next.sideBySide);
				calypsoHarnessSetMotionDisabled(calypsoHarnessSession(), next.motionDisabled);
				calypsoHarnessSetMotionHold(calypsoHarnessSession(), next.motionHoldPct);
			}
		}
		return;
	}
	State::think();
}

void CalypsoHdHarnessHostState::blit()
{
	// Structural opaque black: filled directly into the logical screen surface,
	// never gated on the target's physical adapter readiness (F33-PARITY-002).
	if (Game* g = getCurrentGame())
	{
		if (SDL_Surface* screen = g->getScreen()->getSurface())
		{
			SDL_FillRect(screen, nullptr,
				SDL_MapRGBA(screen->format, 0, 0, 0, 255));
		}
	}
	// No visible widgets on the host; nothing further to blit.
}

bool calypsoHdHarnessOpen(CalypsoHarnessScenario id, CalypsoLayoutClass layout,
	bool sideBySide)
{
	CalypsoHarnessSession& s = calypsoHarnessSession();
	g_harnessSnapshotLease.clear();
	g_harnessSnapshotLeaseValid = false;
	CalypsoF09ResearchUi::clearHarnessQueueListSetup();
	if (!calypsoHarnessRequestOpen(s))
	{
		Log(LOG_WARNING) << "CalypsoHdHarnessHostState: already open; ignoring repeated request";
		return false;
	}
	calypsoHarnessSetRequestedLayout(s, layout);

	// Side-by-side comparison shifts the dialog into the left half (the DOM
	// reference card occupies the right); overlay/reference modes keep the
	// centered contract placement. Must be set BEFORE the target is
	// constructed -- its configure() reads the flag.
	if (id == CalypsoHarnessScenario::F33Abandon)
	{
		calypsoHdHarnessSetSideBySide(sideBySide);
	}
	// Phase 46.F21: side-by-side is session state for every family adapter.
	s.sideBySide = sideBySide;

	if (Game* g = getCurrentGame())
	{
		g->pushState(new CalypsoHdHarnessHostState(id));
		State* target = calypsoHarnessCreateTarget(id);
		if (target)
		{
			bool nestedRequired = false;
			State *nested = makeNavigationNestedOwner(g, target, s.rdActiveProfile,
				nestedRequired);
			if (nestedRequired && !nested)
			{
				Log(LOG_ERROR) << "R&D navigation fixture requires a nested native owner";
				delete target;
				calypsoHdHarnessClose();
				calypsoHdHarnessSetSideBySide(false);
				return false;
			}
			const bool globalProductionOrigin =
				id == CalypsoHarnessScenario::F14GlobalProduction
				&& s.rdActiveProfile >= 200 && s.rdActiveProfile <= 219;
			if (calypsoHdRdNavigationProfileValid(s.rdActiveProfile)
				|| globalProductionOrigin)
			{
				// GlobalManufactureState(true) is normally opened from a real
				// BasescapeState, whose handler pops that parent before it pushes
				// ManufactureState.  Keep both origins as real stack owners; a
				// target-only flag would make profile 208 pop the opaque host.
				State *destination = new GeoscapeState();
				g->pushState(destination);
				if (s.rdActiveProfile == 208)
				{
					SavedGame *save = g->getSavedGame();
					Base *base = save && !save->getBases()->empty()
						? save->getBases()->front() : nullptr;
					if (!base)
					{
						Log(LOG_ERROR) << "G03 Basescape origin fixture has no base";
						g->popState();
						calypsoHdHarnessClose();
						return false;
					}
					Globe *globe = new Globe(g, 128, 100, 256, 200, 0, 0);
					State *baseState = new BasescapeState(base, globe);
					g->pushState(baseState);
					calypsoHarnessNavigationDestination(s, baseState);
				}
				else
				{
					calypsoHarnessNavigationDestination(s, destination);
				}
			}
			g->pushState(target);
			calypsoHarnessMarkTargetUp(s, target);
			if (nested)
				g->pushState(nested);
			hideHarnessCursor(g);
			return true;
		}
		// Unknown/empty target: restore fixture/options; the host pops itself.
		calypsoHdHarnessClose();
		calypsoHdHarnessSetSideBySide(false); // never leave the shift behind
		return false;
	}

	calypsoHarnessClose(s); // no live game: roll the session back
	calypsoHdHarnessSetSideBySide(false); // never leave the shift behind
	return false;
}

void calypsoHdHarnessClose()
{
	// Preserve one diagnostic read after the target has closed. The ordinary
	// teardown below restores the campaign save immediately, so capturing first
	// is the only way to prove the native before/after state at the close edge.
	CalypsoHarnessSession& session = calypsoHarnessSession();
	if (calypsoHarnessHasLiveTarget(session))
	{
		g_harnessSnapshotLease = calypsoHdHarnessOperationsSnapshot("target-close");
		g_harnessSnapshotLeaseValid = !g_harnessSnapshotLease.empty();
		if (session.navigationDestinationActive)
		{
			// Keep the real destination and fixture lease alive for the browser's
			// post-action snapshot. Host cleanup happens at the next switch or
			// explicit close once the destination has been observed.
			calypsoHarnessNavigationTargetClosed(session);
			return;
		}
	}
	CalypsoF09ResearchUi::clearHarnessQueueListSetup();
	if (g_rdRestoreOptions)
	{
		auto restore = std::move(g_rdRestoreOptions);
		g_rdRestoreOptions = {};
		restore();
	}
	if (g_rdRestoreLanguageSet)
	{
		if (Game* g = getCurrentGame())
		{
			Options::language = g_rdRestoreLanguage;
			g->loadLanguages();
		}
		g_rdRestoreLanguage.clear();
		g_rdRestoreLanguageSet = false;
	}
	restoreHarnessCursor(getCurrentGame());
	// Restore original SavedGame state for harness fixtures without deleting
	// campaign-owned data. Operation fixtures are removed from an existing
	// save before the lease frees them; a fixture save owns its own base.
	if (g_harnessSaveLease.active)
	{
		bool fixtureBaseOwnedBySave = false;
		if (Game *g = getCurrentGame())
		{
			SavedGame *current = g->getSavedGame();
			if (g_harnessSaveLease.fixture)
			{
				if (current == g_harnessSaveLease.fixture)
				{
					fixtureBaseOwnedBySave = g_harnessSaveLease.fixtureBaseInSave;
					g->setSavedGame(g_harnessSaveLease.original);
				}
				else if (current && current != g_harnessSaveLease.original)
				{
					Log(LOG_WARNING) << "HarnessSaveLease: unexpected SavedGame pointer change";
				}
				if (g_harnessSaveLease.original)
					g_harnessSaveLease.original->setFunds(g_harnessSaveLease.originalFunds);
			}
			else if (g_harnessSaveLease.original)
			{
				if (current == g_harnessSaveLease.original)
				{
					if (!g_harnessSaveLease.fixtureBases.empty())
					{
						auto *bases = current->getBases();
						for (Base *fixtureBase : g_harnessSaveLease.fixtureBases)
						{
							auto it = std::find(bases->begin(), bases->end(), fixtureBase);
							if (it != bases->end()) bases->erase(it);
						}
					}
					else if (g_harnessSaveLease.fixtureBase)
					{
						auto *bases = current->getBases();
						auto it = std::find(bases->begin(), bases->end(), g_harnessSaveLease.fixtureBase);
						if (it != bases->end()) bases->erase(it);
					}
					current->setFunds(g_harnessSaveLease.originalFunds);
				}
				else
				{
					Log(LOG_WARNING) << "HarnessSaveLease: original save pointer changed";
				}
			}
		}
		if (!fixtureBaseOwnedBySave)
		{
			if (!g_harnessSaveLease.fixtureBases.empty())
			{
				for (Base *fixtureBase : g_harnessSaveLease.fixtureBases) delete fixtureBase;
			}
			else
			{
				delete g_harnessSaveLease.fixtureBase;
			}
		}
		g_harnessSaveLease = HarnessSaveLease();
	}
	const bool switching = calypsoHarnessSession().pendingSwitch.scenario != 0;
	const bool preserveLocaleStress = switching
		&& !calypsoHarnessSession().rdPendingLanguage.empty();
	if (!switching)
	{
		calypsoHarnessSession().rdPendingLanguage.clear();
		calypsoHdRuntimeDiagnostics().textAuditEnabled = false;
	}
	// The L02 letterbox probe is presentation-only and must never survive the
	// harness lifecycle. Clear it after the close-edge snapshot has been taken,
	// before the host/session is released and ordinary gameplay resumes.
	CalypsoHdUiOverlay::instance().clearHarnessContentOffset();
	calypsoHarnessClose(calypsoHarnessSession());
	if (preserveLocaleStress)
		calypsoHarnessSession().rdLocaleStress = true;
	// Clear the F33 side-by-side comparison shift so ordinary gameplay never
	// inherits harness presentation (the flag is F33-adapter file state).
	calypsoHdHarnessSetSideBySide(false);
}

bool calypsoHdHarnessReplaceSavedGameForNavigation()
{
	CalypsoHarnessSession& session = calypsoHarnessSession();
	if (!session.navigationDestinationActive || session.targetUp
		|| !g_harnessSaveLease.active || !g_harnessSaveLease.fixture)
		return false;
	Game *game = getCurrentGame();
	if (!game || game->getSavedGame() != g_harnessSaveLease.fixture)
		return false;
	// Game::setSavedGame deletes the old object. Capture only value state before
	// clearing lease-owned pointers, then rebuild a live Base owned by the new
	// save; retaining an old Base pointer here would be use-after-free.
	SavedGame *oldSave = game->getSavedGame();
	const Base *sourceBase = oldSave && !oldSave->getBases()->empty()
		? oldSave->getBases()->front() : nullptr;
	Base *replacementBase = makeNavigationReplacementBase(game, sourceBase);
	if (!replacementBase)
	{
		Log(LOG_ERROR) << "HarnessSaveLease: replacement save could not create a live base";
		return false;
	}
	g_harnessSaveLease.fixtureBase = nullptr;
	g_harnessSaveLease.fixtureBases.clear();
	g_harnessSaveLease.fixtureBaseInSave = false;
	SavedGame *replacement = new SavedGame();
	replacement->setFunds(oldSave->getFunds());
	replacement->getBases()->push_back(replacementBase);
	g_harnessSaveLease.fixture = replacement;
	g_harnessSaveLease.fixtureBase = replacementBase;
	g_harnessSaveLease.fixtureBases.push_back(replacementBase);
	g_harnessSaveLease.fixtureBaseInSave = true;
	game->setSavedGame(replacement);
	return true;
}

bool calypsoHdHarnessReconfigure(CalypsoLayoutClass layout, bool sideBySide)
{
	CalypsoHarnessSession& s = calypsoHarnessSession();
	if (!calypsoHarnessReconfigure(s, layout)) return false;
	calypsoHdHarnessSetSideBySide(sideBySide);
	s.sideBySide = sideBySide;
	// The active target owns the physical adapter and its resize hook is
	// the canonical way to re-capture the selected design-space rectangles
	// (generic since 46.F21; every harness target overrides resize).
	if (Game* g = getCurrentGame())
	{
		if (State* target = g->getTopState())
		{
			int dx = 0;
			int dy = 0;
			target->resize(dx, dy);
		}
	}
	return true;
}

void warnUnknownScenario(int scenarioId)
{
	Log(LOG_WARNING) << "calypso_hd_harness_open: unknown scenario id " << scenarioId;
	calypsoReportHdRouteError("hd-harness", "Unknown scenario id " + std::to_string(scenarioId));
}

} // namespace Calypso
} // namespace OpenXcom

// --- Generic harness exports -------------------------------------------------

extern "C" {

EMSCRIPTEN_KEEPALIVE
int calypso_hd_harness_open(int scenarioId, int layoutClass, int sideBySide)
{
	if (!OpenXcom::Calypso::calypsoHarnessScenarioValid(scenarioId))
	{
		OpenXcom::Calypso::warnUnknownScenario(scenarioId);
		return 0;
	}
	const OpenXcom::Calypso::CalypsoLayoutClass layout =
		layoutClass == 1 ? OpenXcom::Calypso::CalypsoLayoutClass::Wide
		: layoutClass == 2 ? OpenXcom::Calypso::CalypsoLayoutClass::Portrait
		                   : OpenXcom::Calypso::CalypsoLayoutClass::Compact;
	return OpenXcom::Calypso::calypsoHdHarnessOpen(
		static_cast<OpenXcom::Calypso::CalypsoHarnessScenario>(scenarioId), layout,
		sideBySide != 0) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int calypso_hd_harness_switch(int scenarioId, int layoutClass, int sideBySide)
{
	using OpenXcom::Calypso::CalypsoLayoutClass;
	using OpenXcom::Calypso::CalypsoHarnessScenario;
	if (!OpenXcom::Calypso::calypsoHarnessScenarioValid(scenarioId))
	{
		OpenXcom::Calypso::warnUnknownScenario(scenarioId);
		return 0;
	}
	const CalypsoLayoutClass layout =
		layoutClass == 1 ? CalypsoLayoutClass::Wide
		: layoutClass == 2 ? CalypsoLayoutClass::Portrait : CalypsoLayoutClass::Compact;
	OpenXcom::Game* g = OpenXcom::getCurrentGame();
	auto& s = OpenXcom::Calypso::calypsoHarnessSession();
	if (s.hostUp)
	{
		if (!g || !OpenXcom::Calypso::calypsoHarnessQueueSwitch(
			s, scenarioId, layout, sideBySide != 0, g->getTopState())) return 0;
		// A navigation target leaves a real destination state above the host.
		// Remove that destination stack before the host restores the fixture and
		// constructs the queued replacement.
		OpenXcom::Calypso::calypsoHdHarnessDomHide();
		while (g->getTopState()
			&& dynamic_cast<OpenXcom::Calypso::CalypsoHdHarnessHostState*>(g->getTopState()) == nullptr)
			g->popState();
		return 1;
	}
	return OpenXcom::Calypso::calypsoHdHarnessOpen(
		static_cast<CalypsoHarnessScenario>(scenarioId), layout,
		sideBySide != 0) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
void calypso_hd_harness_set_motion_pct(int pct)
{
	OpenXcom::Calypso::calypsoHarnessSetMotionHold(
		OpenXcom::Calypso::calypsoHarnessSession(), pct);
}

EMSCRIPTEN_KEEPALIVE
void calypso_hd_harness_set_motion(int enabled)
{
	OpenXcom::Calypso::calypsoHarnessSetMotionDisabled(
		OpenXcom::Calypso::calypsoHarnessSession(), enabled == 0);
}

EMSCRIPTEN_KEEPALIVE
void calypso_hd_harness_close()
{
	OpenXcom::Calypso::calypsoHdHarnessClose();
}

EMSCRIPTEN_KEEPALIVE
int calypso_hd_harness_reconfigure(int layoutClass, int sideBySide)
{
	const OpenXcom::Calypso::CalypsoLayoutClass layout =
		layoutClass == 1 ? OpenXcom::Calypso::CalypsoLayoutClass::Wide
		: layoutClass == 2 ? OpenXcom::Calypso::CalypsoLayoutClass::Portrait
		                   : OpenXcom::Calypso::CalypsoLayoutClass::Compact;
	return OpenXcom::Calypso::calypsoHdHarnessReconfigure(layout, sideBySide != 0) ? 1 : 0;
}

/// Queue a bounded fixture for the next target without mutating the live one.
EMSCRIPTEN_KEEPALIVE
int calypso_harness_rd_profile(int profile)
{
	// Named acceptance factories must never seed an existing player campaign.
	if (profile != 0 && OpenXcom::Calypso::g_harnessSaveLease.original) return 0;
	const bool known = profile == 0
		|| OpenXcom::Calypso::calypsoHdRdResearchProfileValid(profile)
		|| OpenXcom::Calypso::calypsoHdRdProductionProfileValid(profile)
		|| OpenXcom::Calypso::calypsoHdRdGlobalProfileValid(profile)
		|| OpenXcom::Calypso::calypsoHdRdDependenciesProfileValid(profile)
		|| OpenXcom::Calypso::calypsoHdRdInteractionProfileValid(profile)
		|| OpenXcom::Calypso::calypsoHdRdVisualStateProfileValid(profile)
		|| OpenXcom::Calypso::calypsoHdRdNavigationProfileValid(profile);
	return OpenXcom::Calypso::calypsoHarnessQueueRdProfile(
		OpenXcom::Calypso::calypsoHarnessSession(), profile, known) ? 1 : 0;
}

/// Queue one real GPL language for the next target. The old target remains in
/// its original locale until the switch tears it down; the new target applies
/// the request during construction and the ordinary close path restores the
/// saved gameplay locale.
EMSCRIPTEN_KEEPALIVE
int calypso_harness_rd_set_language(const char* language)
{
	using namespace OpenXcom;
	using namespace OpenXcom::Calypso;
	if (!calypsoHarnessHostUp(calypsoHarnessSession()) || !language || !*language)
		return 0;
	const std::string requested(language);
	if (!Language::isSupported(requested)) return 0;
	CalypsoHarnessSession& session = calypsoHarnessSession();
	if (!calypsoHarnessQueueRdLanguage(session, requested, true)) return 0;
	session.rdLocaleStress = true;
	return 1;
}

/// Apply the F01 QA shell identity used by V01. This is intentionally a
/// harness-only presentation override: it is consumed by the F01 screen
/// fixture and never changes a live save or production renderer state. The
/// active target is resized in place so the override survives teardown and
/// switch bookkeeping without changing common harness ownership.
EMSCRIPTEN_KEEPALIVE
int calypso_harness_rd_f01_shell_identity(const char* base, const char* time,
	const char* date)
{
	using namespace OpenXcom::Calypso;
	CalypsoHarnessSession& session = calypsoHarnessSession();
	// The screen target can be between host-stack transitions while its
	// resize hook is still the authoritative model rebuild path. Guard the
	// override by the actual generated F01 model, not by a transient stack bit.
	if (!session.hostUp || !calypsoHdScreenHarnessF01Active()
		|| !base || !*base || !time || !*time || !date || !*date)
		return 0;
	session.rdF01ShellBase = base;
	session.rdF01ShellTime = time;
	session.rdF01ShellDate = date;
	if (OpenXcom::Game* game = OpenXcom::getCurrentGame())
	{
		if (OpenXcom::State* target = game->getTopState())
		{
			int dx = 0;
			int dy = 0;
			target->resize(dx, dy);
		}
	}
	return 1;
}

/// Apply a physical content offset for the L02 pointer/paint parity probe.
/// This is intentionally harness-only: the production viewport model remains
/// zero-offset. Update the frozen metrics before asking the active target to
/// rebuild so paint, geometry snapshots, and native input share one frame.
EMSCRIPTEN_KEEPALIVE
int calypso_harness_rd_set_content_offset(int physicalX, int physicalY)
{
	using namespace OpenXcom::Calypso;
	CalypsoHarnessSession& session = calypsoHarnessSession();
	if (!calypsoHarnessHostUp(session) || !calypsoHarnessTargetUp(session)
		|| physicalX < 0 || physicalY < 0
		|| physicalX > 256 || physicalY > 256)
		return 0;
	CalypsoHdUiOverlay::instance().setHarnessContentOffset(physicalX, physicalY);
	if (OpenXcom::Game* game = OpenXcom::getCurrentGame())
	{
		if (OpenXcom::State* target = game->getTopState())
		{
			int dx = 0;
			int dy = 0;
			target->resize(dx, dy);
		}
	}
	return 1;
}

/// Read-only live harness evidence, or the explicitly marked close-edge lease
/// retained until the next harness open. Ordinary gameplay is never sampled.
EMSCRIPTEN_KEEPALIVE
const char *calypso_harness_operations_snapshot()
{
	static std::string snapshot;
	const auto& session = OpenXcom::Calypso::calypsoHarnessSession();
	snapshot = (!session.targetUp && OpenXcom::Calypso::g_harnessSnapshotLeaseValid)
		? std::string() : OpenXcom::Calypso::calypsoHdHarnessOperationsSnapshot();
	if (snapshot.empty() && OpenXcom::Calypso::g_harnessSnapshotLeaseValid)
	{
		// A real strategic destination may already be rendering after the HD
		// target closes. Prefer that live renderer payload: the lease is the
		// close-edge owner evidence, not the destination's current model. Keep
		// the lease under an explicit field so conservation checks can still
		// inspect the state immediately before teardown.
		if (session.navigationDestinationActive)
		{
			const std::string live = OpenXcom::Calypso::calypsoHdHarnessOperationsSnapshot(
				"destination");
			if (!live.empty())
			{
				snapshot = live;
				if (!snapshot.empty() && snapshot.back() == '}') snapshot.pop_back();
				snapshot += ",\"closeEdge\":"
					+ OpenXcom::Calypso::g_harnessSnapshotLease;
				snapshot += ",\"liveNavigationDiagnostics\":"
					+ OpenXcom::Calypso::calypsoHdNavigationSnapshotJson() + "}";
				return snapshot.c_str();
			}
		}
		const std::string currentTopState = [&]() {
			if (OpenXcom::Game* game = OpenXcom::getCurrentGame())
				if (const OpenXcom::State* top = game->getTopState())
					return std::string(typeid(*top).name());
			return std::string();
		}();
		const std::string& cached = OpenXcom::Calypso::g_harnessSnapshotLease;
		const std::string marker = "{\"currentTopStateAtRead\":\""
			+ OpenXcom::Calypso::snapshotEscape(currentTopState)
			+ "\",";
		// The lease is the target's close-edge operations model. Keep it as
		// the authoritative UI payload, but append a separate live diagnostics
		// object because navigation can finalize after the target is gone.
		snapshot = marker + cached.substr(1);
		if (!snapshot.empty() && snapshot.back() == '}') snapshot.pop_back();
		snapshot += ",\"liveNavigationDiagnostics\":"
			+ OpenXcom::Calypso::calypsoHdNavigationSnapshotJson() + "}";
	}
	return snapshot.c_str();
}

// Read-only input evidence for interaction acceptance. The browser bridge owns
// the software Cursor; SDL_GetMouseState can remain stale after a synthetic
// warp, so report the rendered cursor projected through the current screen
// scale and black bands. Keep SDL only for the button mask.
EMSCRIPTEN_KEEPALIVE
const char *calypso_harness_pointer_position()
{
	static std::string position;
	int x = 0;
	int y = 0;
	if (OpenXcom::Game *game = OpenXcom::getCurrentGame())
	{
		if (OpenXcom::Cursor *cursor = game->getCursor())
		{
			OpenXcom::Screen *screen = game->getScreen();
			const double sx = screen && screen->getXScale() > 0.0 ? screen->getXScale() : 1.0;
			const double sy = screen && screen->getYScale() > 0.0 ? screen->getYScale() : 1.0;
			const int left = screen ? screen->getCursorLeftBlackBand() : 0;
			const int top = screen ? screen->getCursorTopBlackBand() : 0;
			const auto &metrics = OpenXcom::Calypso::CalypsoHdUiOverlay::instance().frozenMetrics();
			const auto &runtime = OpenXcom::Calypso::calypsoViewportRuntime();
			const int canvasW = runtime.hasPhysicalSize() ? runtime.physicalWidth() : (screen ? screen->getWidth() : 0);
			const int canvasH = runtime.hasPhysicalSize() ? runtime.physicalHeight() : (screen ? screen->getHeight() : 0);
			const int displayW = screen ? screen->getWidth() : 0;
			const int displayH = screen ? screen->getHeight() : 0;
			const int offsetX = metrics.valid()
				? static_cast<int>(std::lround(OpenXcom::Calypso::calypsoCanvasToDisplayCoordinate(
					metrics.contentOffsetX, canvasW, displayW))) : 0;
			const int offsetY = metrics.valid()
				? static_cast<int>(std::lround(OpenXcom::Calypso::calypsoCanvasToDisplayCoordinate(
					metrics.contentOffsetY, canvasH, displayH))) : 0;
			x = static_cast<int>(cursor->getX() * sx) + left + offsetX;
			y = static_cast<int>(cursor->getY() * sy) + top + offsetY;
		}
	}
	const Uint32 buttons = SDL_GetMouseState(nullptr, nullptr);
	position = "{\"x\":" + std::to_string(x)
		+ ",\"y\":" + std::to_string(y)
		+ ",\"buttons\":" + std::to_string(buttons) + '}';
	return position.c_str();
}

EMSCRIPTEN_KEEPALIVE
int calypso_harness_rd_fault(int mode)
{
 using namespace OpenXcom::Calypso;
 if (!calypsoHarnessHostUp(calypsoHarnessSession()) || mode < 0 || mode > 2) return 0;
 calypsoHdRuntimeDiagnostics().harnessFault = mode;
 return 1;
}

EMSCRIPTEN_KEEPALIVE
const char* calypso_harness_rd_diagnostics()
{
 using namespace OpenXcom; using namespace OpenXcom::Calypso;
 static std::string result;
 if (!calypsoHarnessHostUp(calypsoHarnessSession())) return "{}";
 auto& overlay = CalypsoHdUiOverlay::instance();
 const auto& d = calypsoHdRuntimeDiagnostics();
 Game* game = getCurrentGame();
 const State* top = game ? game->getTopState() : nullptr;
 const auto* error = dynamic_cast<const ErrorMessageState*>(top);
 std::ostringstream out;
 out << "{\"frameId\":" << overlay.frameId()
 << ",\"renderedFrames\":" << d.renderedFrames
 << ",\"lastDrawCount\":" << d.lastDrawCount
 << ",\"surfaceAllocations\":" << d.surfaceAllocations
 << ",\"registeredVanillaBlits\":" << d.registeredVanillaBlits
 << ",\"routeFailures\":" << d.routeFailures
 << ",\"imageUploads\":" << overlay.imageUploadCount()
 << ",\"imageDecodes\":" << overlay.imageDecodeCount()
 << ",\"registered\":" << (top && overlay.hasRegisteredState(top) ? "true" : "false")
 << ",\"logicalSuppressed\":" << (top && overlay.logicalStateSuppressed(top,overlay.frameId()) ? "true" : "false")
 << ",\"errorPopupHd\":" << (error && error->calypsoHdActive() ? "true" : "false")
 << ",\"topState\":\"" << (top ? snapshotEscape(typeid(*top).name()) : "") << "\"}";
 result = out.str(); return result.c_str();
}

/// Read-only G04 article evidence. This deliberately inspects the native
/// ArticleStateCraftWeapon owner rather than treating a top-state name or
/// screenshot path as proof that the article body and image were present.
EMSCRIPTEN_KEEPALIVE
const char* calypso_harness_article_snapshot()
{
	using namespace OpenXcom;
	using namespace OpenXcom::Calypso;
	static std::string result;
	Game *game = getCurrentGame();
	const auto *article = game
		? dynamic_cast<const ArticleStateCraftWeapon*>(game->getTopState()) : nullptr;
	if (!article)
	{
		result = "{}";
		return result.c_str();
	}
	std::ostringstream out;
	out << "{\"id\":\"" << snapshotEscape(article->getId())
		<< "\",\"title\":\"" << snapshotEscape(article->calypsoArticleTitle())
		<< "\",\"body\":\"" << snapshotEscape(article->calypsoArticleBody())
		<< "\",\"imageId\":\"" << snapshotEscape(article->calypsoArticleImageId())
		<< "\",\"imageReady\":"
		<< (article->calypsoArticleImageReady() ? "true" : "false") << '}';
	result = out.str();
	return result.c_str();
}

} // extern "C"

#endif // __EMSCRIPTEN__
