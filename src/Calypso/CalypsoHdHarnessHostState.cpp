/*
 * Phase 46.4-F33 (Calypso) -- opaque-black engine harness host. See
 * CalypsoHdHarnessHostState.h.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdRuntimeDiagnostics.h"
#include "CalypsoPauseMenu.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoEconomyState.h"
#include "CalypsoContractDeliverState.h"
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
#include "../Basescape/GlobalAlienContainmentState.h"
#include "ContainmentReviewState.h"
#include "../Basescape/BuildFacilitiesState.h"
#include "../Basescape/DismantleFacilityState.h"
#include "../Basescape/PlaceFacilityState.h"
#include "../Basescape/PurchaseState.h"
#include "../Basescape/SellState.h"
#include "../Mod/RuleItem.h"
#include "../Basescape/SackSoldierState.h"
#include "../Basescape/SoldierTransformState.h"
#include "../Basescape/SoldiersState.h"
#include "../Basescape/SoldierArmorState.h"
#include "../Basescape/SoldierAvatarState.h"
#include "../Basescape/SoldierTransformationListState.h"
#include "../Basescape/SoldierTransformationState.h"
#include "../Basescape/SoldierMemorialState.h"
#include "../Basescape/SoldierDiaryOverviewState.h"
#include "../Basescape/SoldierDiaryLightState.h"
#include "../Basescape/SoldierDiaryMissionState.h"
#include "../Basescape/SoldierDiaryPerformanceState.h"
#include "../Basescape/SoldierInfoState.h"
#include "../Basescape/CraftsState.h"
#include "../Basescape/CraftInfoState.h"
#include "../Basescape/CraftPilotsState.h"
#include "../Basescape/CraftPilotSelectState.h"
#include "../Basescape/CraftSoldiersState.h"
#include "../Basescape/CraftArmorState.h"
#include "../Basescape/CraftWeaponsState.h"
#include "../Basescape/CraftEquipmentState.h"
#include "../Basescape/CraftEquipmentLoadState.h"
#include "../Basescape/CraftEquipmentSaveState.h"
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
#include "../Basescape/TechTreeSelectState.h"
#include "../Ufopaedia/ArticleStateCraftWeapon.h"
#include "../Basescape/BasescapeState.h"
#include "../Basescape/BaseInfoState.h"
#include "../Basescape/MonthlyCostsState.h"
#include "../Basescape/StoresState.h"
#include "../Basescape/TransfersState.h"
#include "../Basescape/ItemLocationsState.h"
#include "../Savegame/Transfer.h"
#include "../Basescape/TransferBaseState.h"
#include "../Basescape/TransferItemsState.h"
#include "../Basescape/TransferConfirmState.h"
#include "../Geoscape/CraftErrorState.h"
#include "../Geoscape/LowFuelState.h"
#include "../Geoscape/ConfirmDestinationState.h"
#include "../Geoscape/SelectDestinationState.h"
#include "../Geoscape/GeoscapeCraftState.h"
#include "../Geoscape/InterceptState.h"
#include "../Geoscape/CraftPatrolState.h"
#include "../Savegame/Waypoint.h"
#include "../Geoscape/CraftNotEnoughPilotsState.h"
#include "../Geoscape/DogfightErrorState.h"
#include "../Geoscape/ConfirmLandingState.h"
#include "../Geoscape/ConfirmCydoniaState.h"
#include "../Geoscape/UfoLostState.h"
#include "../Geoscape/AlienBaseState.h"
#include "../Geoscape/MissionDetectedState.h"
#include "../Savegame/MissionSite.h"
#include "../Geoscape/MultipleTargetsState.h"
#include "../Geoscape/TargetInfoState.h"
#include "../Geoscape/UfoTrackerState.h"
#include "../Savegame/Waypoint.h"
#include "../Savegame/AlienBase.h"
#include "../Mod/AlienDeployment.h"
#include "../Geoscape/UfoDetectedState.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Geoscape/GraphsState.h"
#include "../Geoscape/FundingState.h"
#include "../Geoscape/DogfightExperienceState.h"
#include "../Geoscape/MonthlyReportState.h"
#include "../Savegame/GameTime.h"
#include "../Geoscape/ConfirmLandingState.h"
#include "../Geoscape/ConfirmCydoniaState.h"
#include "../Geoscape/SelectDestinationState.h"
#include "../Geoscape/ExtendedGeoscapeLinksState.h"
#include "../Geoscape/SelectMusicTrackState.h"
#include "../Geoscape/Globe.h"
#include "../Mod/RuleResearch.h"
#include "../Mod/RuleManufacture.h"
#include "../Mod/RuleAlienMission.h"
#include "../Savegame/ResearchProject.h"
#include "../Savegame/Production.h"
#include "../Savegame/Ufo.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/AlienMission.h"
#include "../Mod/UfoTrajectory.h"
#include "../Geoscape/TrainingFinishedState.h"
#include "../Geoscape/PsiTrainingState.h"
#include "../Geoscape/TrainingState.h"
#include "../Geoscape/AllocateTrainingState.h"
#include "../Geoscape/AllocatePsiTrainingState.h"
#include "../Geoscape/ProductionCompleteState.h"
#include "../Geoscape/ItemsArrivingState.h"
#include "../Geoscape/GeoscapeEventState.h"
#include "../Ufopaedia/UfopaediaStartState.h"
#include "../Ufopaedia/ArticleStateText.h"
#include "../Ufopaedia/StatsForNerdsState.h"
#include "../Ufopaedia/ArticleStateTFTD.h"
#include "../Mod/ArticleDefinition.h"
#include "../Ufopaedia/UfopaediaSelectState.h"
#include "../Mod/RuleEvent.h"
#include "../Geoscape/ResearchRequiredState.h"
#include "../Geoscape/NewPossibleCraftState.h"
#include "../Geoscape/NewPossibleFacilityState.h"
#include "../Geoscape/NewPossibleManufactureState.h"
#include "../Geoscape/NewPossiblePurchaseState.h"
#include "../Geoscape/NewPossibleResearchState.h"
#include "../Geoscape/ResearchCompleteState.h"
#include "../Battlescape/InventoryState.h"
#include "../Battlescape/InventoryLoadState.h"
#include "../Battlescape/InventorySaveState.h"
#include "../Battlescape/InventoryPersonalState.h"
#include "../Battlescape/ExtendedInventoryLinksState.h"
#include "../Battlescape/CannotReequipState.h"
#include "../Battlescape/AbortMissionState.h"
#include "../Battlescape/BattlescapeState.h"
#include "../Battlescape/BattlescapeGame.h"
#include "../Battlescape/BattlescapeGenerator.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Battlescape/ConfirmEndMissionState.h"
#include "../Battlescape/NoExperienceState.h"
#include "../Battlescape/TurnDiaryState.h"
#include "../Battlescape/AliensCrashState.h"
#include "../Battlescape/DebriefingState.h"
#include "../Battlescape/InfoboxOKState.h"
#include "../Battlescape/InfoboxState.h"
#include "../Battlescape/ExtendedBattlescapeLinksState.h"
#include "../Battlescape/NextTurnState.h"
#include "../Battlescape/BriefingState.h"
#include "../Battlescape/BriefingLightState.h"
#include "../Battlescape/ActionMenuState.h"
#include "../Battlescape/PrimeGrenadeState.h"
#include "../Battlescape/UnitInfoState.h"
#include "../Battlescape/MedikitState.h"
#include "../Battlescape/MiniMapState.h"
#include "../Battlescape/Map.h"
#include "../Battlescape/ScannerState.h"
#include "../Battlescape/SkillMenuState.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/BattleItem.h"
#include "../Battlescape/ExperienceOverviewState.h"
#include "../Battlescape/PromotionsState.h"
#include "../Battlescape/CommendationLateState.h"
#include "../Battlescape/CommendationState.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleInterface.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Mod/RuleCraft.h"
#include "../Mod/RuleSoldierTransformation.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/SoldierDiary.h"
#include "../Mod/RuleCommendations.h"

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

#include "CalypsoMarketState.h"

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
bool prepareOperationsSave(Game *game, bool withGlobe = false)
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
			// A playable globe needs the mod's countries, regions and alien strategy.
			g_harnessSaveLease.fixture = withGlobe ? game->getMod()->newSave(DIFF_BEGINNER) : new SavedGame();
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

// The preview owns the campaign and its mission. Never attach a form to a player's battle.
SavedBattleGame *prepareTacticalBattleData(Game *game)
{
	if (!prepareOperationsSave(game, true)) return nullptr;
	auto *save = game->getSavedGame();
	if (save->getSavedBattle()) return save->getSavedBattle();
	if (save->getBases()->empty()) return nullptr;
	auto &fleet = *save->getBases()->front()->getCrafts();
	const auto transport = std::find_if(fleet.begin(), fleet.end(), [](Craft *craft) {
		return craft && craft->getNumTotalUnits() > 0;
	});
	auto *mod = game->getMod();
	if (transport == fleet.end() || !mod->getDeployment("STR_PORT_TERROR") || !mod->getAlienRace("STR_AQUATOID")) return nullptr;
	auto *battle = new SavedBattleGame(mod, game->getLanguage());
	save->setBattleGame(battle);
	battle->setMissionType("STR_PORT_TERROR");
	BattlescapeGenerator generator(game);
	generator.setCraft(*transport);
	generator.setAlienRace("STR_AQUATOID");
	generator.run();
	return battle;
}

BattlescapeState *prepareTacticalPreview(Game *game)
{
	auto *battle = prepareTacticalBattleData(game);
	if (!battle) return nullptr;
	if (battle->getBattleState()) return battle->getBattleState();
	auto *state = new BattlescapeState();
	battle->setBattleState(state);
	game->pushState(state);
	calypsoHarnessNavigationDestination(calypsoHarnessSession(), state);
	return state;
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
	fixture.research = researchRule(game->getMod(), researchName);
	fixture.manufacture = manufactureRule(game->getMod(), manufactureName);
	if ((needResearch && !fixture.research) || (needManufacture && !fixture.manufacture)
		|| !prepareOperationsSave(game))
		return OperationsFixture();
	// Do not mutate campaign-global tutorial state until the save ownership
	// check above has succeeded.  Rejected requests against a live player save
	// must be completely side-effect free.
	CalypsoTutorial::get().disableForCampaign();

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
	fixture.manufacture = manufactureRule(game->getMod(), "STR_MAELSTROM_TORPEDOES");
	if (!fixture.manufacture || !prepareOperationsSave(game)) return OperationsFixture();
	// The ownership check must precede any tutorial mutation for the same
	// reason as the regular operations fixture above.
	CalypsoTutorial::get().disableForCampaign();

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

/// Gives a harness fixture base completed live-ruleset capacity so behavior
/// proofs satisfy native preconditions (personnel quarters, item storage).
/// Selects real non-lift RuleBaseFacility rules by capacity, builds them
/// completed, and leaves ownership with the base. Returns false when the
/// ruleset cannot provide a requested capacity; callers fail closed.
bool addCompletedFixtureCapacity(Game* game, Base* base, bool needQuarters, bool needStores)
{
	if (!game || !game->getMod() || !base) return false;
	if (!needQuarters && !needStores) return true;
	const RuleBaseFacility* quartersRule = nullptr;
	const RuleBaseFacility* storesRule = nullptr;
	for (const std::string& facilityType : game->getMod()->getBaseFacilitiesList())
	{
		const RuleBaseFacility* candidate = game->getMod()->getBaseFacility(facilityType, false);
		if (!candidate || candidate->isLift()) continue;
		if (needQuarters && !quartersRule && candidate->getPersonnel() > 0) quartersRule = candidate;
		if (needStores && !storesRule && candidate->getStorage() > 0) storesRule = candidate;
	}
	if ((needQuarters && !quartersRule) || (needStores && !storesRule)) return false;
	if (needQuarters)
	{
		BaseFacility* facility = new BaseFacility(quartersRule, base);
		facility->setBuildTime(0);
		base->getFacilities()->push_back(facility);
	}
	if (needStores)
	{
		BaseFacility* facility = new BaseFacility(storesRule, base);
		facility->setBuildTime(0);
		base->getFacilities()->push_back(facility);
	}
	return true;
}

/// F12 transfer fixture: two named GPL bases on one fixture save with
/// deterministic funds. Fixture bases live for the harness process lifetime
/// (F17 defense precedent); no TFTD or proprietary payload is involved.
void ensureTransferFixture(Game* game, Base*& from, Base*& to)
{
	from = nullptr;
	to = nullptr;
	if (!game || !game->getMod()) return;
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
		game->getSavedGame()->setFunds(27550246);
		if (g_harnessSaveLease.fixture == game->getSavedGame()
			&& !game->getSavedGame()->getDebugMode())
			game->getSavedGame()->setDebugMode();
	}
	from = new Base(game->getMod());
	from->setName("Batumi");
	from->setLongitude(0.0);
	from->setLatitude(0.5);
	to = new Base(game->getMod());
	to->setName("Garni");
	to->setLongitude(0.2);
	to->setLatitude(0.72);
	// Completed destination storage so a correctly dispatched transfer click
	// proves a quantity delta instead of a capacity error. Source quantity,
	// base names, and save ownership are untouched.
	if (!addCompletedFixtureCapacity(game, to, false, true))
	{
		delete from;
		delete to;
		from = nullptr;
		to = nullptr;
		return;
	}
	game->getSavedGame()->getBases()->push_back(from);
	game->getSavedGame()->getBases()->push_back(to);
	trackFixtureBase(from);
	trackFixtureBase(to);
	for (const std::string& itemType : game->getMod()->getItemsList())
	{
		const RuleItem* seed = game->getMod()->getItem(itemType, true);
		if (seed && !seed->isAlien())
		{
			from->getStorageItems()->addItem(seed, 4);
			break;
		}
	}
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

bool calypsoHdHarnessTeardownForTarget(const void* target, std::uint64_t generation)
{
	CalypsoHarnessSession& s = calypsoHarnessSession();
	// Same match predicate as calypsoHarnessCloseForTarget: only the still-
	// active preview may tear down. A deferred stale target falls through.
	if (s.activeTarget != target || s.generation != generation)
	{
		// The session is already fully closed with no successor preview (for
		// example the DOM controller closed it first): hide this target's card
		// without touching session state. A live successor preview keeps its
		// own card: never hide for that case.
		if (!s.hostUp && !s.targetUp)
		{
			calypsoHdHarnessDomHide();
		}
		return false;
	}
	// Genuine teardown of the live preview: DOM hide plus the full close
	// cleanup (cursor restore, fixture lease restore/delete, session reset,
	// side-by-side reset) exactly once via the shared close entry point.
	calypsoHdHarnessDomHide();
	calypsoHdHarnessClose();
	return true;
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
		g_harnessSaveLease.fixtureBases.push_back(placeBase);
		return new PlaceFacilityState(placeBase, rule);
	}
	case CalypsoHarnessScenario::F11Purchase:
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
			game->getSavedGame()->setFunds(27550246);
			if (g_harnessSaveLease.fixture == game->getSavedGame()
				&& !game->getSavedGame()->getDebugMode())
				game->getSavedGame()->setDebugMode();
		}
		Base* marketBase = new Base(game->getMod());
		g_harnessSaveLease.fixtureBases.push_back(marketBase);
		// Completed fixture facilities so a successful stepper click is
		// behavior-testable: an empty base has zero quarters/storage, and a
		// real purchase then raises a capacity error that looks like dead
		// input. Rules come from the live ruleset, owned by the fixture base.
		if (!addCompletedFixtureCapacity(game, marketBase, true, true)) return nullptr;
		return new PurchaseState(marketBase, nullptr);
	}
	case CalypsoHarnessScenario::F11Sell:
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
			game->getSavedGame()->setFunds(27550246);
			if (g_harnessSaveLease.fixture == game->getSavedGame()
				&& !game->getSavedGame()->getDebugMode())
				game->getSavedGame()->setDebugMode();
		}
		Base* marketBase = new Base(game->getMod());
		g_harnessSaveLease.fixtureBases.push_back(marketBase);
		for (const std::string& itemType : game->getMod()->getItemsList())
		{
			const RuleItem* seed = game->getMod()->getItem(itemType, true);
			if (seed && !seed->isAlien())
			{
				marketBase->getStorageItems()->addItem(seed, 4);
				break;
			}
		}
		return new SellState(marketBase, nullptr);
	}
	case CalypsoHarnessScenario::F36Economy:
	case CalypsoHarnessScenario::F37ContractDelivery:
	{
		auto *game = getCurrentGame();
		if (!game || !game->getMod() || !prepareOperationsSave(game)) return nullptr;
		if (id == CalypsoHarnessScenario::F36Economy) return new CalypsoEconomyState();
		const auto *economy = game->getSavedGame()->getCalypsoEconomy();
		if (economy)
			for (const auto &c : economy->getContracts())
				if (c.status == Contract::Status::Accepted) return new CalypsoContractDeliverState(c.id);
		return new CalypsoContractDeliverState(-1);
	}
	case CalypsoHarnessScenario::F36Market:
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
		// Purchase-mode picker: the contract captures the Acquisitions title.
		// SavedGame always owns an Economy under emscripten, so refresh()
		// lists live ruleset counterparties plus the Black Market row; the
		// Reference side renders the deterministic GPL fixture rows.
		Base* marketBase = new Base(game->getMod());
		g_harnessSaveLease.fixtureBases.push_back(marketBase);
		return new CalypsoMarketState(marketBase, false);
	}
	case CalypsoHarnessScenario::F02BaseInformation:
	case CalypsoHarnessScenario::F02MonthlyCosts:
	{
		Game *game = getCurrentGame();
		Base *source = nullptr, *destination = nullptr;
		ensureTransferFixture(game, source, destination);
		if (!source || !destination) return nullptr;
		if (id == CalypsoHarnessScenario::F02MonthlyCosts)
			return new MonthlyCostsState(source);
		return new BaseInfoState(source, nullptr);
	}
	case CalypsoHarnessScenario::F12Stores:
	case CalypsoHarnessScenario::F12ActiveTransfers:
	case CalypsoHarnessScenario::F12ItemLocations:
	{
		Game *game = getCurrentGame();
		Base *source = nullptr, *destination = nullptr;
		ensureTransferFixture(game, source, destination);
		if (!source || !destination) return nullptr;
		const auto *stock = source->getStorageItems()->getContents();
		if (stock->empty()) return nullptr;
		auto *item = stock->begin()->first;
		auto *shipment = new Transfer(12);
		shipment->setItems(item, 1);
		destination->getTransfers()->push_back(shipment);
		if (id == CalypsoHarnessScenario::F12ActiveTransfers)
			return new TransfersState(destination);
		if (id == CalypsoHarnessScenario::F12ItemLocations)
			return new ItemLocationsState(item);
		return new StoresState(source);
	}
	case CalypsoHarnessScenario::F12TransferBase:
	{
		Game* game = getCurrentGame();
		Base* from = nullptr;
		Base* to = nullptr;
		ensureTransferFixture(game, from, to);
		if (!from) return nullptr;
		return new TransferBaseState(from, nullptr);
	}
	case CalypsoHarnessScenario::F12TransferItems:
	{
		Game* game = getCurrentGame();
		Base* from = nullptr;
		Base* to = nullptr;
		ensureTransferFixture(game, from, to);
		if (!from || !to) return nullptr;
		return new TransferItemsState(from, to, nullptr);
	}
	case CalypsoHarnessScenario::F04SackSoldier:
		return new CraftErrorState(nullptr, "Dismiss soldier confirmation.");
	case CalypsoHarnessScenario::F18CraftError:
		return new CraftErrorState(nullptr, "Craft cannot complete the assigned operation.");
	case CalypsoHarnessScenario::F18InterceptFleet:
	{
		auto *game = getCurrentGame();
		if (!prepareOperationsSave(game, true)) return nullptr;
		auto *globe = new GeoscapeState();
		game->pushState(globe);
		calypsoHarnessNavigationDestination(calypsoHarnessSession(), globe);
		return new InterceptState(globe->getGlobe(), false);
	}
	case CalypsoHarnessScenario::F18CraftNavigation:
	case CalypsoHarnessScenario::F18CourseMap:
	case CalypsoHarnessScenario::F18CourseConfirm:
	case CalypsoHarnessScenario::F18PatrolArrival:
	case CalypsoHarnessScenario::F18LowFuel:
	case CalypsoHarnessScenario::F18NotEnoughPilots:
	{
		auto *game = getCurrentGame();
		if (!prepareOperationsSave(game, true)) return nullptr;
		const bool fuel = id == CalypsoHarnessScenario::F18LowFuel;
		for (Base *base : *game->getSavedGame()->getBases())
		{
			for (Craft *craft : *base->getCrafts())
			{
				if (id == CalypsoHarnessScenario::F18NotEnoughPilots && craft->getRules()->getPilots() <= 0) continue;
				auto *globe = new GeoscapeState();
				game->pushState(globe);
				calypsoHarnessNavigationDestination(calypsoHarnessSession(), globe);
				if (id == CalypsoHarnessScenario::F18CraftNavigation)
				{
					craft->setDestination(base);
					craft->setStatus("STR_OUT");
					return new GeoscapeCraftState(craft, globe->getGlobe(), nullptr, false);
				}
				if (id == CalypsoHarnessScenario::F18CourseMap)
					return new SelectDestinationState({craft}, globe->getGlobe());
				if (id == CalypsoHarnessScenario::F18CourseConfirm)
				{
					game->pushState(new SelectDestinationState({craft}, globe->getGlobe()));
					return new ConfirmDestinationState({craft}, base);
				}
				if (id == CalypsoHarnessScenario::F18PatrolArrival)
				{
					auto *waypoint = new Waypoint();
					waypoint->setId(18);
					game->getSavedGame()->getWaypoints()->push_back(waypoint);
					craft->setDestination(waypoint);
					return new CraftPatrolState(craft, globe->getGlobe());
				}
				if (!fuel) return new CraftNotEnoughPilotsState(craft);
				craft->setFuel(1);
				craft->setLowFuel(true);
				craft->returnToBase();
				return new LowFuelState(craft, globe);
			}
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F19DogfightError:
	{
		Game *game = getCurrentGame();
		if (!prepareOperationsSave(game, true) || game->getSavedGame()->getBases()->empty()) return nullptr;
		const auto &craft = *game->getSavedGame()->getBases()->front()->getCrafts();
		if (craft.empty()) return nullptr;
		return new DogfightErrorState(craft.front(), game->getLanguage()->getString("STR_UNABLE_TO_ENGAGE_DEPTH"));
	}
	case CalypsoHarnessScenario::F20ConfirmLanding:
	case CalypsoHarnessScenario::F20ConfirmCydonia:
		{
			auto *game = getCurrentGame();
			if (!prepareOperationsSave(game, true)) return nullptr;
			auto *mod = game->getMod();
			for (auto *base : *game->getSavedGame()->getBases())
			{
				if (!base || base->getCrafts()->empty()) continue;
				auto *craft = base->getCrafts()->front();
				if (id == CalypsoHarnessScenario::F20ConfirmCydonia)
				{
					auto *globe = new GeoscapeState();
					game->pushState(globe);
					calypsoHarnessNavigationDestination(calypsoHarnessSession(), globe);
					game->pushState(new SelectDestinationState({craft}, globe->getGlobe()));
					return new ConfirmCydoniaState(craft);
				}
				if (mod->getAlienMissionList().empty() || mod->getAlienRacesList().empty()) return nullptr;
				for (const auto &name : mod->getDeploymentsList())
				{
					auto *rule = mod->getDeployment(name);
					if (!rule || rule->getAlertMessage().empty() || rule->getAlertBackground().empty()) continue;
					auto *site = new MissionSite(mod->getAlienMission(mod->getAlienMissionList().front()), rule, nullptr);
					site->setDetected(true);
					site->setId(1);
					site->setAlienRace(mod->getAlienRacesList().front());
					game->getSavedGame()->getMissionSites()->push_back(site);
					craft->setDestination(site);
					auto *globe = new GeoscapeState();
					game->pushState(globe);
					calypsoHarnessNavigationDestination(calypsoHarnessSession(), globe);
					return new ConfirmLandingState(craft, nullptr, nullptr, 0);
				}
			}
			return nullptr;
		}
	case CalypsoHarnessScenario::F24ResearchRequired:
	case CalypsoHarnessScenario::F24ResearchComplete:
	{
		auto *game = getCurrentGame();
		if (!prepareOperationsSave(game, true) || game->getSavedGame()->getBases()->empty()) return nullptr;
		auto *globe = new GeoscapeState();
		game->pushState(globe);
		calypsoHarnessNavigationDestination(calypsoHarnessSession(), globe);
		if (id == CalypsoHarnessScenario::F24ResearchComplete)
		{
			auto *research = firstResearchRule(game->getMod());
			if (!research) return nullptr;
			return new ResearchCompleteState(research, nullptr, research, game->getSavedGame()->getBases()->front());
		}
		for (const auto &name : game->getMod()->getItemsList())
		{
			auto *item = game->getMod()->getItem(name);
			const auto *ammo = item->getPrimaryCompatibleAmmo();
			if (ammo && !ammo->empty()) return new ResearchRequiredState(item);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F29AutomaticRecovery:
	case CalypsoHarnessScenario::F29Debriefing:
	{
		Game *game = getCurrentGame();
		if (!prepareTacticalBattleData(game)) return nullptr;
		if (id == CalypsoHarnessScenario::F29AutomaticRecovery) return new AliensCrashState();
		return new DebriefingState();
	}
	case CalypsoHarnessScenario::F26InventoryLinks:
	case CalypsoHarnessScenario::F26LoadTemplate:
	case CalypsoHarnessScenario::F26SaveTemplate:
	{
		auto *game = getCurrentGame();
		auto *state = prepareTacticalPreview(game);
		if (!state) return nullptr;
		auto *battle = game->getSavedGame()->getSavedBattle();
		auto *inventory = new InventoryState(false, state, nullptr);
		game->pushState(inventory);
		calypsoHarnessNavigationDestination(calypsoHarnessSession(), inventory);
		if (id == CalypsoHarnessScenario::F26LoadTemplate) return new InventoryLoadState(inventory);
		if (id == CalypsoHarnessScenario::F26SaveTemplate) return new InventorySaveState(inventory);
		return new ExtendedInventoryLinksState(inventory, battle, false, true);
	}
	case CalypsoHarnessScenario::F26PersonalTemplate:
	case CalypsoHarnessScenario::F26Shortages:
	{
		auto *game = getCurrentGame();
		if (!prepareOperationsSave(game, true) || game->getSavedGame()->getBases()->empty()) return nullptr;
		auto *base = game->getSavedGame()->getBases()->front();
		if (id == CalypsoHarnessScenario::F26PersonalTemplate)
			return base->getSoldiers()->empty() ? nullptr : new InventoryPersonalState(base->getSoldiers()->front());
		const auto &stock = *base->getStorageItems()->getContents();
		auto item = std::find_if(stock.begin(), stock.end(), [](const auto &entry) {
			return entry.first && entry.second > 0 && entry.first->getBuyCost() > 0;
		});
		if (item == stock.end() || base->getCrafts()->empty()) return nullptr;
		std::vector<ReequipStat> shortages;
		for (auto *craft : *base->getCrafts())
		{
			shortages.push_back({item->first->getType(), int(shortages.size()) + 2,
				craft->getName(game->getLanguage()), item->first->getListOrder()});
			if (shortages.size() == 2) break;
		}
		return new CannotReequipState(shortages, base);
	}
	case CalypsoHarnessScenario::F27MissionBriefing:
	{
		Game *game = getCurrentGame();
		if (!prepareTacticalPreview(game)) return nullptr;
		return new BriefingState(nullptr, nullptr, true);
	}
	case CalypsoHarnessScenario::F27TargetBriefing:
	{
		Game *game = getCurrentGame();
		if (!prepareOperationsSave(game, true)) return nullptr;
		auto *deployment = game->getMod()->getDeployment("STR_PORT_TERROR", false);
		return deployment ? new BriefingLightState(deployment) : nullptr;
	}
	case CalypsoHarnessScenario::F28AbortMission:
	case CalypsoHarnessScenario::F28ConfirmEnd:
	case CalypsoHarnessScenario::F28TurnTransition:
	case CalypsoHarnessScenario::F28TacticalLinks:
	case CalypsoHarnessScenario::F28TacticalNotice:
	case CalypsoHarnessScenario::F28TacticalAlert:
	case CalypsoHarnessScenario::F30NoExperience:
	case CalypsoHarnessScenario::F25Actions:
	case CalypsoHarnessScenario::F25Timer:
	case CalypsoHarnessScenario::F25UnitInfo:
	case CalypsoHarnessScenario::F25Medikit:
	case CalypsoHarnessScenario::F25Minimap:
	case CalypsoHarnessScenario::F25Scanner:
	case CalypsoHarnessScenario::F25Skills:
	case CalypsoHarnessScenario::F30TurnDiary:
	case CalypsoHarnessScenario::F30ExperienceOverview:
	{
		auto *game = getCurrentGame();
		auto *state = prepareTacticalPreview(game);
		if (!state) return nullptr;
		auto *battle = game->getSavedGame()->getSavedBattle();
		if (id == CalypsoHarnessScenario::F28AbortMission) return new AbortMissionState(battle, state);
		if (id == CalypsoHarnessScenario::F28ConfirmEnd) return new ConfirmEndMissionState(battle, 0, state->getBattleGame());
		if (id == CalypsoHarnessScenario::F28TurnTransition) return new NextTurnState(battle, state);
		if (id == CalypsoHarnessScenario::F28TacticalLinks) return new ExtendedBattlescapeLinksState(state, battle);
		if (id == CalypsoHarnessScenario::F28TacticalNotice) return new InfoboxState(game->getLanguage()->getString("STR_THIS_FEATURE_IS_DISABLED_4"));
		if (id == CalypsoHarnessScenario::F28TacticalAlert) return new InfoboxOKState(game->getLanguage()->getString("STR_MISSION_OVER"));
		if (id == CalypsoHarnessScenario::F25Minimap)
			return new MiniMapState(state->getMap()->getCamera(),battle);
		if (id == CalypsoHarnessScenario::F25Scanner)
		{
			if (!battle->getSelectedUnit()) return nullptr;
			auto *action = state->getBattleGame()->getCurrentAction();
			action->actor = battle->getSelectedUnit();
			return new ScannerState(action);
		}
		if (id == CalypsoHarnessScenario::F25Medikit)
		{
			auto *unit = battle->getSelectedUnit();
			if (!unit || !unit->getTile()) return nullptr;
			for (const auto &name : game->getMod()->getItemsList())
			{
				auto *rule = game->getMod()->getItem(name);
				if (rule->getBattleType() != BT_MEDIKIT || rule->getMediKitType() != BMT_NORMAL) continue;
				auto *kit = battle->createItemForTile(rule, unit->getTile());
				if (!kit) return nullptr;
				auto *action = state->getBattleGame()->getCurrentAction();
				action->actor = unit;
				action->weapon = kit;
				action->type = BA_USE;
				action->updateTU();
				return new MedikitState(unit, action, battle->getTileEngine());
			}
			return nullptr;
		}
		if (id == CalypsoHarnessScenario::F25UnitInfo)
			return battle->getSelectedUnit() ? new UnitInfoState(battle->getSelectedUnit(), state, false, false) : nullptr;
		if (id == CalypsoHarnessScenario::F25Timer)
		{
			for (auto *item : *battle->getItems())
				if (item && item->getRules()->getFuseTimerType() == BFT_SET)
					return new PrimeGrenadeState(nullptr, true, item);
			return nullptr;
		}
		if (id == CalypsoHarnessScenario::F25Actions || id == CalypsoHarnessScenario::F25Skills)
		{
			auto *unit = battle->getSelectedUnit();
			if (!unit) return nullptr;
			auto *action = state->getBattleGame()->getCurrentAction();
			action->actor = unit;
			if (id == CalypsoHarnessScenario::F25Skills)
				return unit->getGeoscapeSoldier() ? new SkillMenuState(action, 0, 0) : nullptr;
			action->weapon = unit->getMainHandWeapon();
			if (!action->weapon && !unit->getInventory()->empty()) action->weapon = unit->getInventory()->front();
			return action->weapon ? new ActionMenuState(action, 0, 0, state->getBattleGame()) : nullptr;
		}
		if (id == CalypsoHarnessScenario::F30TurnDiary) return new TurnDiaryState(battle->getHitLog());
		if (id == CalypsoHarnessScenario::F30ExperienceOverview) return new ExperienceOverviewState(state);
		return new NoExperienceState();
	}
	case CalypsoHarnessScenario::F17UfoLost:
		return new UfoLostState("Contact 17");
	case CalypsoHarnessScenario::F23MonthlyReport:
	{
		auto *game = getCurrentGame();
		if (!prepareOperationsSave(game, true)) return nullptr;
		auto *save = game->getSavedGame();
		save->setTime(GameTime(2, 1, 2, 2040, 0, 0, 0));
		save->addMonth();
		save->monthlyFunding();
		auto *globe = new GeoscapeState();
		game->pushState(globe);
		calypsoHarnessNavigationDestination(calypsoHarnessSession(), globe);
		return new MonthlyReportState(globe->getGlobe());
	}
	case CalypsoHarnessScenario::F23Graphs:
		if (!prepareOperationsSave(getCurrentGame(), true)) return nullptr;
		return new GraphsState();
	case CalypsoHarnessScenario::F30Commendations:
	case CalypsoHarnessScenario::F30FallenCommendations:
	{
		auto *game = getCurrentGame();
		if (!prepareOperationsSave(game, true) || game->getSavedGame()->getBases()->empty()) return nullptr;
		const auto &soldiers = *game->getSavedGame()->getBases()->front()->getSoldiers();
		const auto &rules = game->getMod()->getCommendationsList();
		const auto award = std::find_if(rules.begin(), rules.end(), [](const auto &rule) {
			return rule.second && !rule.second->getCriteria()->empty();
		});
		if (soldiers.empty()) return nullptr;
		auto *soldier = soldiers.front();
		if (award != rules.end())
			soldier->getDiary()->getSoldierCommendations()->push_back(
				new SoldierCommendations(award->first, "noNoun", game->getMod()));
		if (id == CalypsoHarnessScenario::F30FallenCommendations) return new CommendationLateState({soldier});
		return new CommendationState({soldier});
	}
	case CalypsoHarnessScenario::F30Promotions:
	{
		auto *game = getCurrentGame();
		if (!prepareOperationsSave(game, true) || game->getSavedGame()->getBases()->empty()) return nullptr;
		auto &soldiers = *game->getSavedGame()->getBases()->front()->getSoldiers();
		if (soldiers.empty()) return nullptr;
		auto *soldier = soldiers.front();
		soldier->setRank(RANK_SQUADDIE);
		soldier->promoteRank();
		return new PromotionsState();
	}
	case CalypsoHarnessScenario::F19PilotExperience:
	{
		Game *game = getCurrentGame();
		if (!prepareOperationsSave(game, true) || game->getSavedGame()->getBases()->empty()) return nullptr;
		Base *base = game->getSavedGame()->getBases()->front();
		if (!base->getSoldiers()->empty())
		{
			auto *gain = base->getSoldiers()->front()->getDailyDogfightExperienceCache();
			gain->firing = 1;
			gain->reactions = 2;
		}
		return new DogfightExperienceState();
	}
	case CalypsoHarnessScenario::F23Funding:
		if (!prepareOperationsSave(getCurrentGame(), true)) return nullptr;
		return new FundingState();
	case CalypsoHarnessScenario::F17TargetInfo:
	case CalypsoHarnessScenario::F17OverlappingTargets:
	{
		auto *game = getCurrentGame();
		if (!prepareOperationsSave(game, true) || game->getSavedGame()->getBases()->empty()) return nullptr;
		auto *home = game->getSavedGame()->getBases()->front();
		auto *waypoint = new Waypoint();
		waypoint->setId(17);
		game->getSavedGame()->getWaypoints()->push_back(waypoint);
		auto *globe = new GeoscapeState();
		game->pushState(globe);
		calypsoHarnessNavigationDestination(calypsoHarnessSession(), globe);
		if (id == CalypsoHarnessScenario::F17TargetInfo) return new TargetInfoState(waypoint, globe->getGlobe());
		return new MultipleTargetsState({home, waypoint}, {}, globe, false);
	}
	case CalypsoHarnessScenario::F17ContactTracker:
	case CalypsoHarnessScenario::F17AlienBase:
	{
		Game *game = getCurrentGame();
		if (!prepareOperationsSave(game, true)) return nullptr;
		for (const auto &deploymentId : game->getMod()->getDeploymentsList())
		{
			auto *deployment = game->getMod()->getDeployment(deploymentId);
			if (!deployment || !deployment->isAlienBase()) continue;
			auto *base = new AlienBase(deployment, 0);
			base->setId(17);
			base->setLongitude(0.73);
			base->setLatitude(0.72);
			base->setDiscovered(true);
			game->getSavedGame()->getAlienBases()->push_back(base);
			auto *globe = new GeoscapeState();
			game->pushState(globe);
			calypsoHarnessNavigationDestination(calypsoHarnessSession(), globe);
			if (id == CalypsoHarnessScenario::F17ContactTracker) return new UfoTrackerState(globe, globe->getGlobe());
			return new AlienBaseState(base, globe);
		}
		return nullptr;
	}
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
		{
			auto *game = getCurrentGame();
			if (!makeOperationsFixture(game, false, false).base) return nullptr;
			auto *mod = game->getMod();
			if (mod->getAlienMissionList().empty()) return nullptr;
			for (const auto &key : mod->getDeploymentsList())
			{
				auto *rule = mod->getDeployment(key);
				if (!rule || rule->getAlertMessage().empty() || rule->getAlertBackground().empty()) continue;
				auto *site = new MissionSite(mod->getAlienMission(mod->getAlienMissionList().front()), rule, nullptr);
				site->setDetected(true);
				game->getSavedGame()->getMissionSites()->push_back(site);
				auto *globe = new GeoscapeState();
				game->pushState(globe);
				calypsoHarnessNavigationDestination(calypsoHarnessSession(), globe);
				return new MissionDetectedState(site, globe);
			}
			return nullptr;
		}
	case CalypsoHarnessScenario::F22PhysicalRoster:
	case CalypsoHarnessScenario::F22PsionicRoster:
		{
			auto *game = getCurrentGame();
			if (!prepareOperationsSave(game, true)) return nullptr;
			for (auto *base : *game->getSavedGame()->getBases())
			{
				if (!base || base->getSoldiers()->empty()) continue;
				if (id == CalypsoHarnessScenario::F22PsionicRoster) return new AllocatePsiTrainingState(base);
				return new AllocateTrainingState(base);
			}
			return nullptr;
		}
	case CalypsoHarnessScenario::F22PhysicalBases:
	case CalypsoHarnessScenario::F22PsiBases:
		if (!prepareOperationsSave(getCurrentGame(), true)) return nullptr;
		if (id == CalypsoHarnessScenario::F22PsiBases) return new PsiTrainingState();
		return new TrainingState();
	case CalypsoHarnessScenario::F22TrainingFinished:
		{
			auto *game = getCurrentGame();
			if (!prepareOperationsSave(game, true)) return nullptr;
			for (auto *base : *game->getSavedGame()->getBases())
			{
				if (base->getSoldiers()->empty()) continue;
				const auto &soldiers = *base->getSoldiers();
				std::vector<Soldier*> completed(soldiers.begin(), soldiers.begin() + std::min<std::size_t>(3, soldiers.size()));
				return new TrainingFinishedState(base, completed, false);
			}
			return nullptr;
		}
	case CalypsoHarnessScenario::F24ProductionComplete:
		{
			auto *game = getCurrentGame();
			if (!prepareOperationsSave(game, true) || game->getSavedGame()->getBases()->empty()) return nullptr;
			auto *rule = firstManufactureRule(game->getMod());
			if (!rule) return nullptr;
			auto *globe = new GeoscapeState();
			game->pushState(globe);
			calypsoHarnessNavigationDestination(calypsoHarnessSession(), globe);
			return new ProductionCompleteState(game->getSavedGame()->getBases()->front(),
				game->getLanguage()->getString(rule->getName()), globe);
		}
	case CalypsoHarnessScenario::F05SoldierTransform:
		return new CraftErrorState(nullptr, "Soldier transformation is now available.");
	case CalypsoHarnessScenario::F06SoldierMemorial:
		// Campaign-wide ledger over the live save; an empty loss record is
		// the honest empty harness state (no fake rows).
		return new SoldierMemorialState();
	case CalypsoHarnessScenario::F06DiaryOverview:
	{
		// First live diver supplies the ledger; unpushed profile context
		// keeps the OK path's return-to-profile id contract intact.
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getSoldiers()->empty())
			{
				SoldierInfoState* info = new SoldierInfoState(base, 0);
				return new SoldierDiaryOverviewState(base, 0, info);
			}
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F06DiaryLight:
	{
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getSoldiers()->empty())
				return new SoldierDiaryLightState(base->getSoldiers()->at(0));
		}
		if (!game->getSavedGame()->getDeadSoldiers()->empty())
			return new SoldierDiaryLightState(game->getSavedGame()->getDeadSoldiers()->at(0));
		return nullptr;
	}
	case CalypsoHarnessScenario::F06DiaryMission:
	{
		// Only a diver with recorded missions yields a mission record;
		// otherwise the native init would pop immediately (no harness).
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (!base) continue;
			for (Soldier* soldier : *base->getSoldiers())
			{
				if (soldier && soldier->getDiary()
					&& !soldier->getDiary()->getMissionIdList().empty())
					return new SoldierDiaryMissionState(soldier, 0);
			}
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F06DiaryPerformance:
	{
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getSoldiers()->empty())
			{
				SoldierInfoState* info = new SoldierInfoState(base, 0);
				SoldierDiaryOverviewState* overview =
					new SoldierDiaryOverviewState(base, 0, info);
				return new SoldierDiaryPerformanceState(base, 0, overview, DIARY_KILLS);
			}
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F07Crafts:
	{
		// First live base fielding at least one craft; no fixture craft,
		// diver, or pilot is ever invented (honest empty harness state).
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getCrafts()->empty())
				return new CraftsState(base);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F07CraftInfo:
	{
		// First live craft supplies the overview; capability tabs render
		// exactly under their native visibility gates (a Ketos-like
		// zero-capacity craft shows Overview + Weapons only).
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getCrafts()->empty())
				return new CraftInfoState(base, 0);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F07PilotSelect:
	{
		// Candidate rows are the live eligible divers aboard the first
		// live craft; an empty list is the honest empty harness state.
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getCrafts()->empty())
				return new CraftPilotSelectState(base, 0);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F07CraftPilots:
	{
		// Assigned pilots, required seats, and bonus facts all come from
		// the live craft; immediate add/remove recalculates on return.
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getCrafts()->empty())
				return new CraftPilotsState(base, 0);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F08CraftSoldiers:
	{
		// First live base fielding a craft; crew rows, capacity scope, and
		// deployment order all come from live state (an empty roster is the
		// honest empty harness state; no fixture diver is ever invented).
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getCrafts()->empty())
				return new CraftSoldiersState(base, 0);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F08CraftArmor:
	{
		// Per-diver armor rows come from the live base roster; research,
		// compatibility, stock, and capacity gates stay native-owned.
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getCrafts()->empty())
				return new CraftArmorState(base, 0);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F08CraftWeapons:
	{
		// First live craft fielding at least one weapon mount; candidates,
		// exchange, and errors all come from live ruleset, stock, and
		// research state. A mount-less first craft is skipped (the mount-0
		// row would index an empty weapon vector); no craft means no
		// harness, never a fixture.
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (!base || !base->getCrafts()) continue;
			for (std::size_t i = 0; i < base->getCrafts()->size(); ++i)
			{
				Craft* craft = base->getCrafts()->at(i);
				if (craft && craft->getRules() && craft->getRules()->getWeapons() > 0)
					return new CraftWeaponsState(base, i, 0);
			}
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F08CraftEquipment:
	{
		// First live base fielding a craft; categories, quantities, and
		// transfers all come from live ruleset, stock, and research state.
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getCrafts()->empty())
				return new CraftEquipmentState(base, 0);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F08CraftEquipmentLoad:
	{
		// Preset slots come from the live campaign-global namespace; the
		// picker needs its live parent for the apply path, so the target
		// carries an unpushed live parent context (harness-lifetime, same
		// precedent as the diary profile context). Empty slots reject
		// before any mutation, exactly as in production.
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getCrafts()->empty())
				return new CraftEquipmentLoadState(new CraftEquipmentState(base, 0));
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F08CraftEquipmentSave:
	{
		// Same live-parent context as the load picker; the overwrite
		// review arms on the live slot contents.
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getCrafts()->empty())
				return new CraftEquipmentSaveState(new CraftEquipmentState(base, 0));
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F04Soldiers:
	{
		// First live base supplies the roster; sorts, reorder arrows,
		// allocators, memorial, and inventory all come from live state
		// (an empty roster is the honest empty harness state; no
		// fixture diver is ever invented).
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base)
				return new SoldiersState(base);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F05SoldierArmor:
	{
		// First live diver supplies the armor picker; research,
		// compatibility, stock, and craft-space validation stay
		// native-owned.
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getSoldiers()->empty())
				return new SoldierArmorState(base, 0, SA_GEOSCAPE);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F05SoldierAvatar:
	{
		// First live diver supplies the avatar picker; preview, Apply,
		// and Cancel-restore keep their existing handlers.
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getSoldiers()->empty())
				return new SoldierAvatarState(base, 0);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F05TransformSelect:
	{
		// First live diver supplies the project selector; eligibility
		// and the review handoff stay native-owned.
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getSoldiers()->empty())
				return new SoldierTransformState(base, 0);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F05TransformationList:
	{
		// Global project overview over the live campaign; the roster
		// combo write-back context is absent in the harness (production
		// always provides the parent roster), so row activation and OK
		// write back into no roster here — paint, scroll, filters, and
		// selection state are the covered surface.
		Game* game = getCurrentGame();
		if (!game || !game->getSavedGame()) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base)
				return new SoldierTransformationListState(base, nullptr);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F05TransformationReview:
	{
		// First ruleset transformation project reviewed against the
		// first live diver; cost, items, quarters, and stat changes all
		// come from live ruleset and campaign state (no Calypso
		// transformation rows are invented; empty project list is the
		// honest empty harness state).
		Game* game = getCurrentGame();
		if (!game || !game->getMod() || !game->getSavedGame()) return nullptr;
		if (game->getMod()->getSoldierTransformationList().empty()) return nullptr;
		RuleSoldierTransformation* rule = game->getMod()->getSoldierTransformation(
			game->getMod()->getSoldierTransformationList().front(), false);
		if (!rule) return nullptr;
		for (Base* base : *game->getSavedGame()->getBases())
		{
			if (base && !base->getSoldiers()->empty())
				return new SoldierTransformationState(rule, base, base->getSoldiers()->at(0), nullptr);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F12TransferConfirm:
	{
		// Real confirm over a real HD-owned items underlay: the underlay is
		// constructed (configuring its HD adapter) but stays unpushed, living
		// for the harness process lifetime like the F17 fixtures.
		Game* game = getCurrentGame();
		Base* from = nullptr;
		Base* to = nullptr;
		ensureTransferFixture(game, from, to);
		if (!from || !to) return nullptr;
		TransferItemsState* items = new TransferItemsState(from, to, nullptr);
		return new TransferConfirmState(to, items);
	}
	case CalypsoHarnessScenario::F13Containment:
	case CalypsoHarnessScenario::F13ManageContainment:
	case CalypsoHarnessScenario::F13GlobalContainment:
	case CalypsoHarnessScenario::F13ContainmentReview:
	{
		auto *game = getCurrentGame();
		auto fixture = makeOperationsFixture(game, false, false);
		if (!fixture.base) return nullptr;
		for (const auto &name : game->getMod()->getItemsList())
		{
			const auto *rule = game->getMod()->getItem(name, true);
			if (rule->isAlien() && rule->getPrisonType() == 0)
			{
				fixture.base->getStorageItems()->addItem(rule, 2);
				break;
			}
		}
		if (id == CalypsoHarnessScenario::F13GlobalContainment)
			return new GlobalAlienContainmentState(false);
		auto *state = new ManageAlienContainmentState(fixture.base, 0, OPT_GEOSCAPE);
		if (id != CalypsoHarnessScenario::F13ContainmentReview) return state;
		game->pushState(state);
		state->increaseByValue(1);
		return new ContainmentReviewState(*state, ContainmentAction::Remove);
	}
	case CalypsoHarnessScenario::F24UnlockCraft:
	case CalypsoHarnessScenario::F24UnlockFacility:
	case CalypsoHarnessScenario::F24UnlockManufacture:
	case CalypsoHarnessScenario::F24UnlockPurchase:
	case CalypsoHarnessScenario::F24UnlockResearch:
	{
		auto *game = getCurrentGame();
		if (!prepareOperationsSave(game, true) || game->getSavedGame()->getBases()->empty()) return nullptr;
		auto *base = game->getSavedGame()->getBases()->front();
		auto *mod = game->getMod();
		auto *globe = new GeoscapeState();
		game->pushState(globe);
		calypsoHarnessNavigationDestination(calypsoHarnessSession(), globe);
		if (id == CalypsoHarnessScenario::F24UnlockCraft && !mod->getCraftsList().empty())
			return new NewPossibleCraftState(base, {mod->getCraft(mod->getCraftsList().front(), true)});
		if (id == CalypsoHarnessScenario::F24UnlockFacility && !mod->getBaseFacilitiesList().empty())
			return new NewPossibleFacilityState(base, globe->getGlobe(), {mod->getBaseFacility(mod->getBaseFacilitiesList().front(), true)});
		if (id == CalypsoHarnessScenario::F24UnlockManufacture && !mod->getManufactureList().empty())
			return new NewPossibleManufactureState(base, {mod->getManufacture(mod->getManufactureList().front(), true)});
		if (id == CalypsoHarnessScenario::F24UnlockPurchase && !mod->getItemsList().empty())
			return new NewPossiblePurchaseState(base, {mod->getItem(mod->getItemsList().front(), true)});
		if (id == CalypsoHarnessScenario::F24UnlockResearch)
		{
			for (const auto &name : mod->getResearchList())
			{
				auto *rule = mod->getResearch(name);
				if (NewPossibleResearchState::eligible(*game->getSavedGame(), rule))
					return new NewPossibleResearchState(base, {rule});
			}
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F32AdvancedStats:
	{
		auto *game = getCurrentGame();
		if (!prepareOperationsSave(game, true)) return nullptr;
		for (const auto &key : game->getMod()->getUfopaediaList())
		{
			auto *article = game->getMod()->getUfopaediaArticle(key, true);
			if (article->getType() == UFOPAEDIA_TYPE_TFTD_CRAFT && Ufopaedia::isArticleAvailable(game->getSavedGame(), article))
				return new StatsForNerdsState(article->getType(), key, false, false, false);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F32TextArticle:
	{
		auto *game = getCurrentGame();
		if (!prepareOperationsSave(game, true)) return nullptr;
		auto common = std::make_shared<ArticleCommonState>();
		for (const auto &key : game->getMod()->getUfopaediaList())
		{
			auto *article = game->getMod()->getUfopaediaArticle(key, true);
			if (!Ufopaedia::isArticleAvailable(game->getSavedGame(), article)) continue;
			if ((article->getType() == UFOPAEDIA_TYPE_TEXT || article->getType() == UFOPAEDIA_TYPE_TFTD)
				&& common->current_index == ArticleCommonState::invalid)
				common->current_index = common->articleList.size();
			common->articleList.push_back(article);
			common->articleStatusList.push_back(game->getSavedGame()->getUfopediaRuleStatus(article->id) == ArticleDefinition::PEDIA_STATUS_HIDDEN);
		}
		if (common->current_index == ArticleCommonState::invalid) return nullptr;
		auto *article = common->getCurrentArticle();
		if (article->getType() == UFOPAEDIA_TYPE_TEXT)
			return new ArticleStateText(static_cast<ArticleDefinitionText *>(article), common);
		return new ArticleStateTFTD(static_cast<ArticleDefinitionTFTD *>(article), common);
	}
	case CalypsoHarnessScenario::F31ArchiveSections:
	case CalypsoHarnessScenario::F31ArchiveTopics:
	{
		auto *game = getCurrentGame();
		if (!prepareOperationsSave(game, true)) return nullptr;
		if (id == CalypsoHarnessScenario::F31ArchiveSections) return new UfopaediaStartState();
		const auto &categories = game->getMod()->getUfopaediaCategoryList();
		for (const auto &category : categories)
		{
			ArticleDefinitionList articles;
			Ufopaedia::list(game->getSavedGame(), game->getMod(), category, articles);
			if (!articles.empty()) return new UfopaediaSelectState(category, 0, 0);
		}
		return nullptr;
	}
	case CalypsoHarnessScenario::F24StrategicEvent:
	{
		auto *game = getCurrentGame();
		if (!prepareOperationsSave(game, true) || game->getSavedGame()->getBases()->empty()) return nullptr;
		auto *globe = new GeoscapeState();
		game->pushState(globe);
		calypsoHarnessNavigationDestination(calypsoHarnessSession(), globe);
		const auto *events = game->getMod()->getEventList();
		if (!events->empty()) return new GeoscapeEventState(*game->getMod()->getEvent(events->front(), true));
		// The stock campaign has no scripted events. A read-only preview
		// rule supplies translatable narrative and uses the standard event artwork.
		static const RuleEvent preview = [] {
			RuleEvent rule("STR_BARRACUDA");
			rule.load(YAML::YamlRootNodeReader(YAML::YamlString{"description: STR_BARRACUDA_UFOPEDIA"}, "HD event preview"));
			return rule;
		}();
		return new GeoscapeEventState(preview);
	}
	case CalypsoHarnessScenario::F24ItemsArriving:
	{
		Game *game = getCurrentGame();
		if (!prepareOperationsSave(game, true)) return nullptr;
		Base *first = nullptr, *second = nullptr;
		ensureTransferFixture(game, first, second);
		if (!first || !second || first->getStorageItems()->getContents()->empty()) return nullptr;
		const auto *item = first->getStorageItems()->getContents()->begin()->first;
		for (auto *destination : {first, second})
		{
			auto *arrival = new Transfer(0);
			arrival->setItems(item, destination == first ? 4 : 2);
			destination->getTransfers()->push_back(arrival);
		}
		auto *geoscape = new GeoscapeState();
		game->pushState(geoscape);
		calypsoHarnessNavigationDestination(calypsoHarnessSession(), geoscape);
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
	case CalypsoHarnessScenario::F16ExtendedLinks:
	case CalypsoHarnessScenario::F16MusicSelector:
	{
		auto *game = getCurrentGame();
		if (!prepareOperationsSave(game, true)) return nullptr;
		auto *globe = new GeoscapeState();
		game->pushState(globe);
		calypsoHarnessNavigationDestination(calypsoHarnessSession(), globe);
		if (id == CalypsoHarnessScenario::F16ExtendedLinks)
			return new ExtendedGeoscapeLinksState(globe);
		return new SelectMusicTrackState(SMT_GEOSCAPE);
	}
	case CalypsoHarnessScenario::F15TopicSelector:
	{
		auto *game = getCurrentGame();
		auto fixture = makeOperationsFixture(game, true, false);
		if (!fixture.research) return nullptr;
		auto *viewer = new TechTreeViewerState(fixture.research);
		game->pushState(viewer);
		calypsoHarnessNavigationDestination(calypsoHarnessSession(), viewer);
		return new TechTreeSelectState(viewer);
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
		State* host = new CalypsoHdHarnessHostState(id);
		g->pushState(host);
		s.activeHost = host;
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
int calypso_qa_preview_interception()
{
	using namespace OpenXcom;
	using namespace OpenXcom::Calypso;
	if (!EM_ASM_INT({return location.hostname === 'localhost' || location.hostname === '127.0.0.1';})) return 0;
	Game *game = getCurrentGame();
	if (!game || game->getSavedGame() || !game->getTopState() || !prepareOperationsSave(game, true)) return 0;
	SavedGame *save = game->getSavedGame();
	Mod *mod = game->getMod();
	if (save->getBases()->empty() || mod->getUfosList().empty() || mod->getAlienMissionList().empty()) return 0;
	Base *base = save->getBases()->front();
	const auto &fleet = *base->getCrafts();
	auto armed = std::find_if(fleet.begin(), fleet.end(), [](Craft *craft) { return craft && craft->getNumWeapons(true) > 0; });
	if (armed == fleet.end()) return 0;
	const auto *rule = mod->getUfo(mod->getUfosList().front(), false);
	const auto *missionRule = mod->getAlienMission(mod->getAlienMissionList().front(), false);
	const auto *trajectory = mod->getUfoTrajectory(UfoTrajectory::RETALIATION_ASSAULT_RUN, false);
	if (!rule || !missionRule || !trajectory) return 0;
	auto *mission = new AlienMission(*missionRule);
	save->getAlienMissions().push_back(mission);
	auto *ufo = new Ufo(rule, 1);
	ufo->setMissionInfo(mission, trajectory);
	ufo->setId(save->getId("STR_UFO"));
	ufo->setLongitude(base->getLongitude()); ufo->setLatitude(base->getLatitude());
	ufo->setAltitude("STR_VERY_LOW"); ufo->setDetected(true);
	save->getUfos()->push_back(ufo);
	Craft *craft = *armed;
	craft->setDestination(ufo);
	craft->setStatus("STR_OUT");
	CalypsoTutorial::get().disableForCampaign();
	auto *state = new GeoscapeState();
	game->pushState(state);
	state->startHarnessInterception(craft, ufo);
	return 1;
}

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
