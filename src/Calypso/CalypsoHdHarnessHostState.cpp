/*
 * Phase 46.4-F33 (Calypso) -- opaque-black engine harness host. See
 * CalypsoHdHarnessHostState.h.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdHarnessHostState.h"

#include <SDL.h>
#include <emscripten.h>
#include <string>
#include <vector>

#include "../Engine/Game.h"
#include "../Engine/Screen.h"
#include "../Engine/Logger.h"
#include "../Interface/Cursor.h"
#include "../Menu/AbandonGameState.h"
#include "../Basescape/BaseView.h"
#include "../Basescape/BuildFacilitiesState.h"
#include "../Basescape/BasescapeState.h"
#include "../Basescape/DismantleFacilityState.h"
#include "../Basescape/PlaceFacilityState.h"
#include "../Basescape/PurchaseState.h"
#include "../Basescape/SellState.h"
#include "../Mod/RuleItem.h"
#include "../Basescape/SackSoldierState.h"
#include "../Basescape/SoldierTransformState.h"
#include "../Basescape/SoldierDiaryOverviewState.h"
#include "../Basescape/ManufactureInfoState.h"
#include "../Basescape/ManageAlienContainmentState.h"
#include "../Basescape/TransferBaseState.h"
#include "../Basescape/TransferItemsState.h"
#include "../Basescape/TransferConfirmState.h"
#include "../Geoscape/CraftErrorState.h"
#include "../Geoscape/LowFuelState.h"
#include "../Geoscape/CraftNotEnoughPilotsState.h"
#include "../Geoscape/DogfightErrorState.h"
#include "../Geoscape/ConfirmLandingState.h"
#include "../Geoscape/ConfirmCydoniaState.h"
#include "../Geoscape/ResearchRequiredState.h"
#include "../Geoscape/ResearchCompleteState.h"
#include "../Geoscape/UfoLostState.h"
#include "../Geoscape/UfoDetectedState.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Geoscape/Globe.h"
#include "../Mod/RuleAlienMission.h"
#include "../Savegame/AlienMission.h"
#include "../Savegame/Ufo.h"
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
#include "../Savegame/ItemContainer.h"
#include <cstdint>

#include "CalypsoAbandonPopupUi.h" // calypsoHdHarnessSetSideBySide (F33 comparison shift)
#include "CalypsoMarketState.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{
CalypsoHarnessSession g_harnessSession;

/// One active harness run at a time (repeated opens are no-ops).
struct HarnessSaveLease
{
    SavedGame *original = nullptr;
    SavedGame *fixture = nullptr;
    std::int64_t originalFunds = 0;
    // Fixture bases owned by the harness (not by their target states):
    // every created fixture base is appended and freed only at full close,
    // so a deferred old target never dangles and sequential targets never
    // leak or overwrite each other.
    std::vector<Base*> fixtureBases;
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
	game->getSavedGame()->getBases()->push_back(from);
	game->getSavedGame()->getBases()->push_back(to);
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
	case CalypsoHarnessScenario::F10ManufactureCheck:
		return new CraftErrorState(nullptr, "Manufacture requirements check.");
	case CalypsoHarnessScenario::F13Containment:
		return new CraftErrorState(nullptr, "Alien containment overview.");
	case CalypsoHarnessScenario::F24ItemsArriving:
		return new CraftErrorState(nullptr, "Incoming transfer at base.");
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
		State* host = new CalypsoHdHarnessHostState(id);
		g->pushState(host);
		s.activeHost = host;
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
	// Restore original SavedGame state for F03 fixture isolation - lease pattern, no double-free
	if (g_harnessSaveLease.active) {
		if (Game* g = getCurrentGame()) {
			SavedGame* current = g->getSavedGame();
			if (g_harnessSaveLease.fixture) {
				// Fixture was created - it is current save, just clear it via setSavedGame (which deletes)
				if (current == g_harnessSaveLease.fixture) {
					g->setSavedGame(g_harnessSaveLease.original);
				} else if (current && current != g_harnessSaveLease.original) {
					// Unexpected pointer changed externally - do not delete old lease pointers, just clear lease
					// to avoid dangling. Log and continue.
					Log(LOG_WARNING) << "HarnessSaveLease: unexpected SavedGame pointer change";
				}
				// If we had an original save, restore its funds
				if (g_harnessSaveLease.original) {
					g_harnessSaveLease.original->setFunds(g_harnessSaveLease.originalFunds);
				}
			} else if (g_harnessSaveLease.original) {
				// Existing save was kept - just restore funds if current is still original
				if (current == g_harnessSaveLease.original) {
					current->setFunds(g_harnessSaveLease.originalFunds);
				} else {
					Log(LOG_WARNING) << "HarnessSaveLease: original save pointer changed";
				}
			}
		}
		for (Base* fixtureBase : g_harnessSaveLease.fixtureBases) delete fixtureBase;
		g_harnessSaveLease.fixtureBases.clear();
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
	// BEFORE the deferred target destructor runs. That destructor's teardown
	// is target-scoped, so it stays a no-op against the newly opened preview.
	if (!OpenXcom::Calypso::calypsoHarnessScenarioValid(scenarioId))
	{
		OpenXcom::Calypso::warnUnknownScenario(scenarioId);
		return 0;
	}
	OpenXcom::Game* g = OpenXcom::getCurrentGame();
	OpenXcom::Calypso::CalypsoHarnessSession& s = OpenXcom::Calypso::calypsoHarnessSession();
	if (s.hostUp && g && g->getTopState())
	{
		// The live target is usually buried: settled overlays (tutorial
		// popups, dialogs) pile above the preview, so the top of stack is
		// not necessarily the target. Unwind those overlays first, stopping
		// at the live target, its host, an empty stack, or the bound — never
		// blindly unwind the underlying game.
		const void* targetPtr = s.activeTarget;
		const void* hostPtr = s.activeHost;
		std::uint64_t gen = s.generation;
		int guard = 0;
		while (g->getTopState() && g->getTopState() != targetPtr
			&& g->getTopState() != hostPtr && guard++ < 8)
		{
			g->popState();
		}
		if (g->getTopState() == targetPtr)
		{
			g->popState(); // the harness target (deferred; teardown is target-scoped)
			if (g->getTopState() == hostPtr)
			{
				g->popState(); // its opaque host below it
			}
		}
		else if (g->getTopState() == hostPtr)
		{
			g->popState(); // target already gone; retire its host
		}
		// Identity close resets the session for the fresh open below even
		// when the stack no longer holds the pair; a deferred old-target
		// destructor stays a no-op against the new preview.
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
