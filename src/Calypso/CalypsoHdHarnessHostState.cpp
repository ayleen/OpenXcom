/*
 * Phase 46.4-F33 (Calypso) -- opaque-black engine harness host. See
 * CalypsoHdHarnessHostState.h.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdHarnessHostState.h"

#include <SDL.h>
#include <emscripten.h>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <string>

#include "../Engine/Game.h"
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
#include "../Mod/RuleBaseFacility.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/SavedGame.h"

#include "CalypsoAbandonPopupUi.h" // calypsoHdHarnessSetSideBySide (F33 comparison shift)

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
bool g_harnessCursorCaptured = false;
bool g_harnessCursorVisible = true;
bool g_harnessCursorHidden = false;

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
	if (!g_harnessSaveLease.active)
	{
		if (game->getSavedGame())
		{
			g_harnessSaveLease.original = game->getSavedGame();
			g_harnessSaveLease.originalFunds = g_harnessSaveLease.original->getFunds();
		}
		else
		{
			g_harnessSaveLease.fixture = new SavedGame();
			game->setSavedGame(g_harnessSaveLease.fixture);
		}
		g_harnessSaveLease.active = true;
	}
	SavedGame *save = game->getSavedGame();
	if (!save) return false;
	save->setFunds(27550246);
	if (save == g_harnessSaveLease.fixture && !save->getDebugMode())
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

OperationsFixture makeOperationsFixture(Game *game, bool needResearch, bool needManufacture,
	const char *researchName = nullptr, const char *manufactureName = nullptr,
	bool activeResearch = false, bool activeManufacture = false)
{
	OperationsFixture fixture;
	if (!game || !game->getMod()) return fixture;
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
bool populateDiaryFixture(Game *game)
{
	if (!game || !game->getMod()) return false;
	if (game->getSavedGame() && !game->getSavedGame()->getResearchDiary().empty())
		return prepareOperationsSave(game);
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
	if (!prepareOperationsSave(game)) return false;
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

} // namespace

CalypsoHarnessSession& calypsoHarnessSession()
{
	return g_harnessSession;
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
			"STR_DECANTED_CORPSE", nullptr, true, false);
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
		return fixture.production
			? new ManufactureInfoState(fixture.base, fixture.production) : nullptr;
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
		return populateDiaryFixture(getCurrentGame()) ? new GlobalResearchDiaryState() : nullptr;
	}
	case CalypsoHarnessScenario::F14GlobalProduction:
	{
		OperationsFixture fixture = makeGlobalManufactureFixture(getCurrentGame());
		return fixture.base ? new GlobalManufactureState(true) : nullptr;
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
	// The host only thinks while it IS the top state -- i.e. the target preview
	// has closed and calypsoHdHarnessClose() cleared the session. Pop the host
	// (it pops itself from its own think, the same pattern states use for
	// Escape/cancel), leaving the previous game state intact.
	if (!calypsoHarnessHostUp(calypsoHarnessSession()))
	{
		if (Game* g = getCurrentGame())
		{
			g->popState();
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
			g->pushState(target);
			calypsoHarnessTargetUp(s, target);
			hideHarnessCursor(g);
			return true;
		}
		// Unknown/empty target: roll the session back; the host pops itself.
		calypsoHarnessClose(s);
		calypsoHdHarnessSetSideBySide(false); // never leave the shift behind
		return false;
	}

	calypsoHarnessClose(s); // no live game: roll the session back
	calypsoHdHarnessSetSideBySide(false); // never leave the shift behind
	return false;
}

void calypsoHdHarnessClose()
{
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
	calypsoHarnessClose(calypsoHarnessSession());
	// Clear the F33 side-by-side comparison shift so ordinary gameplay never
	// inherits harness presentation (the flag is F33-adapter file state).
	calypsoHdHarnessSetSideBySide(false);
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
	// Multi-family catalogs (46.F21) switch the previewed scenario while the
	// harness is live. A plain open is rejected in that state ("already
	// open"), so tear the current pair down first: pop the target, pop the
	// opaque host (its think-self-pop is bypassed), and reset the session
	// BEFORE the deferred target destructor runs (its calypsoHdHarnessClose
	// is idempotent against an already-closed session).
	if (!OpenXcom::Calypso::calypsoHarnessScenarioValid(scenarioId))
	{
		OpenXcom::Calypso::warnUnknownScenario(scenarioId);
		return 0;
	}
	OpenXcom::Game* g = OpenXcom::getCurrentGame();
	OpenXcom::Calypso::CalypsoHarnessSession& s = OpenXcom::Calypso::calypsoHarnessSession();
	if (s.hostUp && g && g->getTopState())
	{
		// Use target-scoped close to avoid closing a newly opened harness if old target destructor runs later
		OpenXcom::State* top = g->getTopState();
		// The top should be the target; capture its identity before popping
		const void* targetPtr = top;
		std::uint64_t gen = s.generation;
		g->popState(); // the harness target
		g->popState(); // the opaque host below it
		OpenXcom::Calypso::calypsoHarnessCloseForTarget(s, targetPtr, gen);
		OpenXcom::Calypso::calypsoHdHarnessSetSideBySide(false);
	}
	const CalypsoLayoutClass layout =
		layoutClass == 1 ? CalypsoLayoutClass::Wide
		: layoutClass == 2 ? CalypsoLayoutClass::Portrait : CalypsoLayoutClass::Compact;
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

} // extern "C"

#endif // __EMSCRIPTEN__
