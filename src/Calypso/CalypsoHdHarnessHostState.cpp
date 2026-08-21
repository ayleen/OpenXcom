/*
 * Phase 46.4-F33 (Calypso) -- opaque-black engine harness host. See
 * CalypsoHdHarnessHostState.h.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdHarnessHostState.h"

#include <SDL.h>
#include <emscripten.h>
#include <string>

#include "../Engine/Game.h"
#include "../Engine/Screen.h"
#include "../Engine/Logger.h"
#include "../Interface/Cursor.h"
#include "../Menu/AbandonGameState.h"
#include "../Basescape/BaseView.h"
#include "../Basescape/DismantleFacilityState.h"
#include "../Basescape/SackSoldierState.h"
#include "../Basescape/SoldierTransformState.h"
#include "../Basescape/SoldierDiaryOverviewState.h"
#include "../Basescape/ManufactureInfoState.h"
#include "../Basescape/ManageAlienContainmentState.h"
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
#include "../Geoscape/MissionDetectedState.h"
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
#include <cstdint>

#include "CalypsoAbandonPopupUi.h" // calypsoHdHarnessSetSideBySide (F33 comparison shift)

namespace OpenXcom
{
namespace Calypso
{

namespace
{

/// One active harness run at a time (repeated opens are no-ops).
CalypsoHarnessSession g_harnessSession;
// F03 harness SavedGame isolation - lease pattern to avoid double-free and dangling pointer
struct HarnessSaveLease
{
    SavedGame *original = nullptr;
    SavedGame *fixture = nullptr;
    std::int64_t originalFunds = 0;
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
		return new CraftErrorState(nullptr, "Unidentified craft detected on radar.");
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
		layoutClass == 1 ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact;
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
		                 : OpenXcom::Calypso::CalypsoLayoutClass::Compact;
	return OpenXcom::Calypso::calypsoHdHarnessReconfigure(layout, sideBySide != 0) ? 1 : 0;
}

} // extern "C"

#endif // __EMSCRIPTEN__
