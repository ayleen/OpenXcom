#ifdef __EMSCRIPTEN__
/*
 * Phase 41 (Calypso) -- CalypsoPrologueCampaign implementation.
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 *
 * NOTE (Phase 39 gotcha): the `Log` macro cannot be namespace-qualified
 * inside src/Calypso/ files -- it is used bare here.
 */

#include <algorithm>
#include <vector>
#include <emscripten.h>

#include "CalypsoPrologueCampaign.h"
#include "CalypsoPrologueAskState.h"
#include "CalypsoTutorial.h"
#include "CalypsoTutorialPolicy.h"
#include "CalypsoDirector.h"
#include "CalypsoPrologueMath.h"

#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Screen.h"
#include "../Engine/CrossPlatform.h"
#include "../Engine/Logger.h"
#include "../Engine/RNG.h"
#include "../Engine/Yaml.h"
#include "../Mod/Mod.h"
#include "../Mod/AlienDeployment.h" // deployment race -> setAlienRace (launchScriptedBattle)
#include "../Mod/RuleSoldier.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Savegame/ItemContainer.h" // Craft::getItems() (bug 3: sidearm stocking)
#include "../Savegame/Soldier.h"
#include "../Battlescape/BattlescapeGenerator.h"
#include "../Battlescape/BattlescapeGame.h"
#include "../Battlescape/BattlescapeState.h"
#include "../Battlescape/BriefingState.h"
#include "../Battlescape/NextTurnState.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Geoscape/BuildNewBaseState.h"
#include "../Geoscape/BaseNameState.h"
#include "../Basescape/PlaceLiftState.h"

namespace OpenXcom
{
namespace Calypso
{

const std::string PROLOGUE_LEADER_NAME = "K. Sarris";
const std::string PROLOGUE_DIVER1_NAME = "D. Voss";
const std::string PROLOGUE_DIVER2_NAME = "M. Reyes";
const std::string PROLOGUE_AUTOSAVE_FILENAME = "calypso-prologue-auto.sav";
const std::string PROLOGUE_DEPLOYMENT_ID = "STR_CALYPSO_PROLOGUE";

namespace
{
	bool s_finishPending = false;
	GameDifficulty s_pendingFinishDiff = DIFF_BEGINNER;
	bool s_pendingFinishIronman = false;
	int s_pendingFinishOutcome = PROLOGUE_OUTCOME_ALL_TAKEN;

	void deletePrologueAutosaveAndFlush(bool continueCampaign)
	{
		CrossPlatform::deleteFile(Options::getMasterUserFolder() + PROLOGUE_AUTOSAVE_FILENAME);
		EM_ASM({
			var shouldContinue = $0 === 1;
			var retryDelay = 50;
			function flushDeletedAutosave() {
				// SavedGame::save may still be flushing the rolling slot. Wait for
				// it, then persist the current MEMFS state in which the slot is gone.
				if (FS.syncFSRequests > 0) {
					setTimeout(flushDeletedAutosave, retryDelay);
					retryDelay = Math.min(1000, retryDelay * 2);
					return;
				}
				FS.syncfs(false, function(err) {
					if (err) {
						console.error('[calypso] prologue autosave syncfs error', err);
						setTimeout(flushDeletedAutosave, retryDelay);
						retryDelay = Math.min(1000, retryDelay * 2);
						return;
					}
					if (shouldContinue) {
						Module.ccall('calypso_finish_prologue_after_autosave_sync', null, [], []);
					}
				});
			}
			flushDeletedAutosave();
		}, continueCampaign ? 1 : 0);
	}
}

void deletePrologueAutosave()
{
	deletePrologueAutosaveAndFlush(false);
}

namespace
{
	// commit 5: dedicated craft type (calypso-prologue mod), never
	// player-purchasable (no costBuy) -- keeps the prologue's Triton out
	// of the fresh campaign's own Triton pool after the scripted battle.
	static const char *PROLOGUE_CRAFT = "STR_NEREID";

	// Bug 3 fix (QA round 1): the divers deployed unarmed, so the killable
	// Herder (DoD requirement) could never actually be killed. Vanilla
	// STR_DART_PISTOL/STR_DART_POD (xcom2 items.rul) is deliberately the
	// weakest sidearm in the game -- 40/80 accuracy, power 16 -- so the
	// three divers can down a Herder (55 HP, near-zero armor) in a few hits
	// without trivializing the scripted Marksman ambush. One pistol + two
	// clips per soldier, stocked on the craft the same way
	// NewBattleState::initSave stocks base/craft items (Craft::getItems());
	// BattlescapeGenerator::run then copies craft items onto the craft
	// inventory tile and autoEquip() (Options::disableAutoEquip defaults
	// false, and these fresh soldiers have no equipment-layout template)
	// puts one pistol + spare clip in each soldier's hands automatically.
	static const char *PROLOGUE_SIDEARM = "STR_DART_PISTOL";
	static const char *PROLOGUE_SIDEARM_AMMO = "STR_DART_POD";
	static const int PROLOGUE_SIDEARM_COUNT = 3;
	static const int PROLOGUE_SIDEARM_AMMO_COUNT = 6; // 2 clips per soldier

	// Stashed between maybeOfferPrologue (accept) and finishPrologue -- the
	// real campaign is not created until the prologue battle is over (D4).
	GameDifficulty s_stashedDiff = DIFF_BEGINNER;
	bool s_stashedIronman = false; // the New Game screen's ironman toggle

	// One surviving player soldier's name + stats, captured at cast-off
	// (OutcomeCastOff only -- see stashSurvivor). Cleared by finishPrologue.
	struct SurvivorRecord
	{
		std::string name;
		UnitStats stats;
	};
	PrologueSurvivorHandoff<SurvivorRecord> s_survivorHandoff;

	// Shared "GeoscapeState + base-placement" tail, copied verbatim from
	// NewGameState.cpp:180-195. Used by both vanillaNewGameTail() (decline
	// path) and finishPrologue() (D7) so the two don't drift independently.
	void pushGeoscapeAndBaseChain(Game *game, SavedGame *save)
	{
		GeoscapeState *gs = new GeoscapeState;
		game->setState(gs);
		gs->init();

		Base *base = save->getBases()->back();
		if (base->getMarker() != -1)
		{
			// location known already
			base->calculateServices(save);

			// center and rotate 35 degrees down (to see the base location while typoing its name)
			gs->getGlobe()->center(base->getLongitude(), base->getLatitude() + 0.61);

			if (base->getName().empty())
			{
				// fixed location, custom name
				game->pushState(new BaseNameState(base, gs->getGlobe(), true, true));
			}
			else if (Options::customInitialBase)
			{
				// fixed location, fixed name
				game->pushState(new PlaceLiftState(base, gs->getGlobe(), true));
			}
		}
		else
		{
			// custom location, custom name
			game->pushState(new BuildNewBaseState(base, gs->getGlobe(), true));
		}
	}
} // namespace

void resetPrologueHandoff()
{
	s_survivorHandoff.reset();
	s_finishPending = false;
	s_pendingFinishDiff = DIFF_BEGINNER;
	s_pendingFinishIronman = false;
	s_pendingFinishOutcome = PROLOGUE_OUTCOME_ALL_TAKEN;
}

bool maybeOfferPrologue(Game *game, GameDifficulty diff, bool ironman, bool tutorial)
{
	// A new-game confirmation starts a new handoff lifetime whether the player
	// accepts the prologue, declines it, or has tutorial content disabled.
	resetPrologueHandoff();
	if (!game || !game->getMod()) return false;
	const bool deploymentAvailable =
		game->getMod()->getDeployment(PROLOGUE_DEPLOYMENT_ID) != nullptr;
	if (!prologueMissionEnabled(tutorial, deploymentAvailable))
	{
		// The per-campaign tutorial choice is authoritative.  Missing content
		// still behaves exactly like vanilla.
		return false;
	}

	s_stashedDiff = diff;
	s_stashedIronman = ironman;
	// Pop the native NewGameState BEFORE the prompt is pushed: it stays in
	// the stack underneath the invisible modal and its bitmap "Select
	// Difficulty" window would show through under the DOM prologue overlay
	// (2026-08-16). The single call site is NewGameState::btnOkClick, so the
	// top state here is always NewGameState; both prologue outcomes (battle /
	// vanilla tail) push their own states and never touch it again.
	game->popState();
	game->pushState(new CalypsoPrologueAskState());
	return true;
}

GameDifficulty stashedDifficulty()
{
	return s_stashedDiff;
}

void vanillaNewGameTail(Game *game, GameDifficulty diff)
{
	// Defensive for callers that enter the vanilla tail without first passing
	// through maybeOfferPrologue (and for a declined fresh attempt).
	resetPrologueHandoff();
	if (!game) return;
	SavedGame *save = game->getMod()->newSave(diff);
	save->setDifficulty(diff);
	save->setIronman(s_stashedIronman); // honor the New Game screen's toggle (stashed by maybeOfferPrologue)
	game->setSavedGame(save);

	CalypsoTutorial::get().beginCampaign(CalypsoTutorial::get().configuredEnabled());

	pushGeoscapeAndBaseChain(game, save);
}

void launchScriptedBattle(Game *game, const std::string &deploymentId, bool preview)
{
	if (!game) return;
	const Mod *mod = game->getMod();

	if (preview)
	{
		// Suppress BEFORE generation -- BattlescapeState::init (pushed below,
		// preview branch) fires CalypsoDirector::onBattleStart on its first
		// frame, and the director must already see the flag set before it
		// looks the deployment up in its scene registry (plan §41.1c: the
		// preview map boots inert, no script, no kills).
		CalypsoDirector::get().setPreviewSuppressed(true);

		// Bug 6 fix (QA round 1): the preview session is dev-only and should
		// show nothing but the map -- the campaign tutorial's normal
		// battle-start popups (armed via calypso-tutorial's mod triggers)
		// otherwise still fire because CalypsoTutorial is a process-wide
		// singleton with no notion of "this is a preview save". Heavy-handed
		// is fine here: this only ever runs for a throwaway preview session.
		CalypsoTutorial::get().suspendForPrologue();
	}

	// Fresh throwaway SavedGame -- ctor default _monthsPassed=-1 is exactly
	// the marker DebriefingState/CutsceneState use to detect "no real
	// campaign" (D7 anchor: DebriefingState.cpp:881-886); left untouched.
	SavedGame *save = new SavedGame();
	// Review round 1 (P1): the New Game screen's difficulty/ironman choice
	// must survive a page reload mid-prologue. The statics above are
	// process-local; the throwaway save is the only thing the rolling
	// autosave persists -- so carry the choice ON the save (SavedGame::save
	// serializes both fields), and finishPrologue() reads it back from the
	// loaded save rather than from the statics.
	save->setDifficulty(s_stashedDiff);
	save->setIronman(s_stashedIronman);
	// Review round 1 (P2): the generic five-page battlescape tutorial fires
	// during the prologue and contradicts the directed scene (it teaches
	// shooting/kneeling and promises the mission ends when the Choir is
	// neutralized). Suppress it for the scripted battle -- finishPrologue()
	// (and the decline path) restore the configured campaign choice before
	// Geoscape initialization. Review round 2
	// (P1, finding 3): prologue-specific micro-prompts (move/camera/TU
	// before the ambush) are NOT deferred anymore -- CalypsoPrologueScene::
	// onBattleStart fires them itself, through the radio-toast primitive,
	// independently of this flag (see that comment for why reusing
	// CalypsoTutorialState directly would have been the wrong call here).
	if (!preview) // preview already disabled it above
		CalypsoTutorial::get().suspendForPrologue();
	Base *base = new Base(mod);
	YAML::YamlRootNodeReader startingBaseReader(mod->getDefaultStartingBase(), "(prologue starting base template)");
	base->load(startingBaseReader, save, true, true);
	save->getBases()->push_back(base);

	// The template base ships with its own placeholder roster/craft -- strip
	// it, exactly as NewBattleState::initSave does, before adding ours.
	for (auto *soldier : *base->getSoldiers()) delete soldier;
	base->getSoldiers()->clear();
	for (auto *xcraft : *base->getCrafts()) delete xcraft;
	base->getCrafts()->clear();

	Craft *craft = new Craft(mod->getCraft(PROLOGUE_CRAFT, true), base, 1);
	base->getCrafts()->push_back(craft);

	// Exactly three soldiers, Leader created FIRST (lowest BattleUnit id,
	// see CalypsoPrologueScene::resolveActors' leader-by-lowest-id rule).
	static const std::string names[3] = { PROLOGUE_LEADER_NAME, PROLOGUE_DIVER1_NAME, PROLOGUE_DIVER2_NAME };
	const RuleSoldier *ruleSoldier = mod->getSoldier(mod->getSoldiersList().front(), true);
	for (int i = 0; i < 3; ++i)
	{
		int nationality = save->selectSoldierNationalityByLocation(mod, ruleSoldier, nullptr);
		Soldier *soldier = mod->genSoldier(save, ruleSoldier, nationality);
		soldier->setName(names[i]);
		base->getSoldiers()->push_back(soldier);

		int space = craft->getSpaceAvailable();
		if (craft->validateAddingSoldier(space, soldier) == CPE_None)
		{
			soldier->setCraft(craft);
		}
		else
		{
			Log(LOG_ERROR) << "[scripted-battle] " << names[i] << " did not fit on " << PROLOGUE_CRAFT;
		}
	}

	// No research setup (NewBattleState's "make all research discovered"
	// loop): this is a scripted battle with a fixed, mod-defined deployment
	// -- it has no tech tree to stock, and the throwaway save is discarded
	// when the battle ends anyway. It DOES need a sidearm per diver (bug 3
	// above), stocked directly on the craft, same call NewBattleState.cpp
	// uses (Craft::getItems()->addItem).
	craft->getItems()->addItem(mod->getItem(PROLOGUE_SIDEARM, true), PROLOGUE_SIDEARM_COUNT);
	craft->getItems()->addItem(mod->getItem(PROLOGUE_SIDEARM_AMMO, true), PROLOGUE_SIDEARM_AMMO_COUNT);

	game->setSavedGame(save);

	SavedBattleGame *bgame = new SavedBattleGame(game->getMod(), game->getLanguage());
	save->setBattleGame(bgame);
	bgame->setMissionType(deploymentId);

	BattlescapeGenerator bgen(game);
	bgen.setCraft(craft);
	// Race must be passed EXPLICITLY (browser-QA finding): the
	// `_alienRace = ruleDeploy->getRace()` assignment lives in nextStage()
	// (multi-stage missions), NOT in run(); and deployAliens()'s own
	// deployment-race fallback is gated on getMonthsPassed() > -1 -- which
	// deliberately excludes New-Battle-style saves like this one. Without
	// setAlienRace the generator throws "Unknown race:  defined in
	// deployment". Terrain/shade/depth DO default from the deployment in
	// run() itself.
	const AlienDeployment *ruleDeploy = mod->getDeployment(deploymentId);
	if (ruleDeploy && !ruleDeploy->getRace().empty())
	{
		bgen.setAlienRace(ruleDeploy->getRace());
	}
	craft->setSpeed(0);
	bgen.run();

	if (preview)
	{
		// Plan §41.1c: full map reveal + straight into BattlescapeState,
		// skipping BriefingState and InventoryState -- replicates the push
		// sequence BriefingState::btnOkClick runs on OK (verified in
		// BriefingState.cpp), minus the briefing screen itself.
		bgame->setDebugMode(); // revealMap() + _debugMode = true (free camera/zoom)

		Options::baseXResolution = Options::baseXBattlescape;
		Options::baseYResolution = Options::baseYBattlescape;
		game->getScreen()->resetDisplay(false);

		BattlescapeState *bs = new BattlescapeState;
		bs->getBattleGame()->spawnFromPrimedItems();
		game->pushState(bs);
		bgame->setBattleState(bs);
		game->pushState(new NextTurnState(bgame, bs));
		bgame->startFirstTurn();
		return;
	}

	game->pushState(new BriefingState(craft, base));
}

void launchPrologueBattle(Game *game)
{
	// The prompt may remain open while an abandoned run has left process-local
	// state behind. A fresh scripted battle must always start with an empty
	// survivor handoff while preserving the already-stashed difficulty/ironman.
	resetPrologueHandoff();
	launchScriptedBattle(game, PROLOGUE_DEPLOYMENT_ID, false);
}

void stashSurvivor(const std::string &name, const UnitStats &stats)
{
	s_survivorHandoff.stash(SurvivorRecord{ name, stats });
}

void finishPrologue(Game *game, int outcome)
{
	if (!game || s_finishPending) return;

	// Review round 1 (P1): after a page reload + prologue-autosave load the
	// statics are back at their defaults -- the authoritative copy of the
	// New Game choice lives on the throwaway save (see launchScriptedBattle).
	s_pendingFinishDiff = s_stashedDiff;
	s_pendingFinishIronman = s_stashedIronman;
	s_pendingFinishOutcome = outcome;
	if (SavedGame *prologueSave = game->getSavedGame())
	{
		s_pendingFinishDiff = prologueSave->getDifficulty();
		s_pendingFinishIronman = prologueSave->isIronman();
	}

	// The real campaign must not exist until IndexedDB confirms that the rolling
	// prologue slot is gone. A page close during this async boundary therefore
	// leaves either the old prologue (if the flush never completed) or the clean
	// campaign transition (after the callback), never both at once.
	s_finishPending = true;
	deletePrologueAutosaveAndFlush(true);
}

void finishPrologueAfterAutosaveSync(Game *game)
{
	if (!game || !s_finishPending) return;
	s_finishPending = false;

	SavedGame *save = game->getMod()->newSave(s_pendingFinishDiff);
	save->setDifficulty(s_pendingFinishDiff);
	save->setIronman(s_pendingFinishIronman); // honor the New Game screen's toggle
	game->setSavedGame(save);

	// D7: replace the first N default starting-base soldiers with the
	// stashed survivors (name + full stats snapshot at cast-off).
	// OutcomeAllTaken is explicitly gated out even if defensive reset wiring is
	// ever bypassed and stale process-local records somehow remain.
	if (shouldInjectPrologueSurvivors(s_pendingFinishOutcome)
		&& !s_survivorHandoff.empty())
	{
		Base *base = save->getBases()->front();
		auto &soldiers = *base->getSoldiers();
		const auto &survivors = s_survivorHandoff.records();
		size_t n = std::min(survivors.size(), soldiers.size());
		for (size_t i = 0; i < n; ++i)
		{
			soldiers[i]->setName(survivors[i].name);
			*soldiers[i]->getCurrentStatsEditable() = survivors[i].stats;
		}
	}
	resetPrologueHandoff();

	CalypsoTutorial::get().beginCampaign(CalypsoTutorial::get().configuredEnabled());

	pushGeoscapeAndBaseChain(game, save);
}

} // namespace Calypso
} // namespace OpenXcom

extern "C" {

EMSCRIPTEN_KEEPALIVE
void calypso_finish_prologue_after_autosave_sync()
{
	OpenXcom::Calypso::finishPrologueAfterAutosaveSync(OpenXcom::getCurrentGame());
}

}

#endif // __EMSCRIPTEN__
