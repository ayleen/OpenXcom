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
#include "CalypsoDirector.h"

#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Screen.h"
#include "../Engine/CrossPlatform.h"
#include "../Engine/Logger.h"
#include "../Engine/RNG.h"
#include "../Engine/Yaml.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleSoldier.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
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

namespace
{
	static const char *PROLOGUE_DEPLOYMENT = "STR_CALYPSO_PROLOGUE";
	static const char *PROLOGUE_CRAFT = "STR_TRITON";

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
	std::vector<SurvivorRecord> s_survivorStash;

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

bool maybeOfferPrologue(Game *game, GameDifficulty diff, bool ironman)
{
	if (!game || !game->getMod()) return false;
	if (Options::calypsoPrologueSeen) return false;
	if (game->getMod()->getDeployment(PROLOGUE_DEPLOYMENT) == nullptr)
	{
		// Mod content not loaded yet (pre-commit-5, or a build without the
		// calypso-prologue mod) -- behave exactly like vanilla.
		return false;
	}

	s_stashedDiff = diff;
	s_stashedIronman = ironman;
	game->pushState(new CalypsoPrologueAskState());
	return true;
}

GameDifficulty stashedDifficulty()
{
	return s_stashedDiff;
}

void vanillaNewGameTail(Game *game, GameDifficulty diff)
{
	if (!game) return;
	SavedGame *save = game->getMod()->newSave(diff);
	save->setDifficulty(diff);
	save->setIronman(s_stashedIronman); // honor the New Game screen's toggle (stashed by maybeOfferPrologue)
	game->setSavedGame(save);

	CalypsoTutorial::get().resetCampaign();
	CalypsoTutorial::get().requestAsk(); // Phase 39: first-run enable prompt

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
	}

	// Fresh throwaway SavedGame -- ctor default _monthsPassed=-1 is exactly
	// the marker DebriefingState/CutsceneState use to detect "no real
	// campaign" (D7 anchor: DebriefingState.cpp:881-886); left untouched.
	SavedGame *save = new SavedGame();
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

	// No item/research setup (NewBattleState's "generate items" + "make all
	// research discovered" loops): this is a scripted battle with a fixed,
	// mod-defined deployment -- it has no base inventory or tech tree to
	// stock, and the throwaway save is discarded when the battle ends anyway.

	game->setSavedGame(save);

	SavedBattleGame *bgame = new SavedBattleGame(game->getMod(), game->getLanguage());
	save->setBattleGame(bgame);
	bgame->setMissionType(deploymentId);

	BattlescapeGenerator bgen(game);
	bgen.setCraft(craft);
	// No setTerrain/setAlienRace/setAlienItemlevel/setWorldShade: all four
	// default from the deployment itself (verified, BattlescapeGenerator.cpp:
	// _terrain==0 picks from ruleDeploy->getTerrains(), _alienRace defaults
	// from ruleDeploy->getRace(), shade from ruleDeploy->getShade()/min/
	// maxShade, depth via the internal setDepth(ruleDeploy,...) call at the
	// top of run()) -- exactly like an alien-base/mission-site battle
	// launched off a deployment, no NewBattleState UI involved.
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
	launchScriptedBattle(game, PROLOGUE_DEPLOYMENT, false);
}

void stashSurvivor(const std::string &name, const UnitStats &stats)
{
	s_survivorStash.push_back(SurvivorRecord{ name, stats });
}

void finishPrologue(Game *game, int outcome)
{
	(void)outcome; // survivor injection is driven by stash contents, not the raw outcome value
	if (!game) return;

	SavedGame *save = game->getMod()->newSave(s_stashedDiff);
	save->setDifficulty(s_stashedDiff);
	save->setIronman(s_stashedIronman); // honor the New Game screen's toggle
	game->setSavedGame(save);

	// D7: replace the first N default starting-base soldiers with the
	// stashed survivors (name + full stats snapshot at cast-off).
	// OutcomeAllTaken leaves the stash empty -> default roster untouched.
	if (!s_survivorStash.empty())
	{
		Base *base = save->getBases()->front();
		auto &soldiers = *base->getSoldiers();
		size_t n = std::min(s_survivorStash.size(), soldiers.size());
		for (size_t i = 0; i < n; ++i)
		{
			soldiers[i]->setName(s_survivorStash[i].name);
			*soldiers[i]->getCurrentStatsEditable() = s_survivorStash[i].stats;
		}
	}
	s_survivorStash.clear();

	// D6: this was the only save that ever existed for the throwaway
	// prologue campaign -- delete it before the real campaign starts, so
	// there is nothing left to reload back into the finished mission.
	// Unlike DeleteGameState (native path, no explicit flush), we flush
	// explicitly here: this delete is anti-savescum-critical, and the next
	// natural syncfs might not happen before the tab closes.
	CrossPlatform::deleteFile(Options::getMasterUserFolder() + PROLOGUE_AUTOSAVE_FILENAME);
	EM_ASM(({ FS.syncfs(false, function(err) { if (err) console.error('[calypso] syncfs error', err); }); }));

	CalypsoTutorial::get().resetCampaign();
	CalypsoTutorial::get().requestAsk();

	pushGeoscapeAndBaseChain(game, save);
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
