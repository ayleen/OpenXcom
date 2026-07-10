#ifdef __EMSCRIPTEN__
/*
 * Phase 41 (Calypso) -- reusable scripted-scene primitive layer implementation.
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 *
 * The director is scene-agnostic: no port, no actors, no deployment ids. It owns
 * the upstream hook surface (battle start / per-faction turn start / unit death
 * / abort / finish intercept) and the primitives any scripted scene reuses. See
 * CalypsoDirector.h and docs/phases/phase-41-tutorial-mission.md sections 41.2
 * and 41.3.
 *
 * NOTE (Phase 39 gotcha): the `Log` macro cannot be namespace-qualified inside
 * src/Calypso/ files -- it is used bare here.
 */

#include <utility>
#include <string>

#include "CalypsoDirector.h"
#include "CalypsoRadioLineState.h"

#include "../Battlescape/BattlescapeGame.h"   // BattleAction, statePushFront/Back, getSave/getMap
#include "../Battlescape/BattlescapeState.h"  // finishBattle
#include "../Battlescape/ProjectileFlyBState.h"
#include "../Battlescape/UnitTurnBState.h"
#include "../Battlescape/UnitWalkBState.h"
#include "../Battlescape/Pathfinding.h"
#include "../Battlescape/Map.h"               // getCamera() for handoffToPlayer
#include "../Battlescape/Camera.h"            // centerOnPosition()
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/SavedGame.h"             // forceAutosave()
#include "../Savegame/BattleUnit.h"
#include "../Mod/Unit.h"                      // UnitFaction values, UnitStats
#include "../Engine/Game.h"                   // getCurrentGame(), pushState, getLanguage
#include "../Engine/Language.h"               // getString()
#include "../Engine/Logger.h"
#include "../Engine/Yaml.h"

namespace OpenXcom
{

// --------------------------------------------------------------------------- //
// singleton + registration
// --------------------------------------------------------------------------- //

CalypsoDirector &CalypsoDirector::get()
{
	// Function-local static -- first-call construction, no static-init ordering
	// hazard relative to other singletons (matches CalypsoTutorial::get).
	static CalypsoDirector instance;
	return instance;
}

void CalypsoDirector::registerScene(const std::string &deploymentId, SceneFactory factory)
{
	_registry[deploymentId] = std::move(factory);
}

CalypsoScene *CalypsoDirector::active() const
{
	// Null while preview-suppressed so callers (hooks, scenes) skip the director.
	return _previewSuppressed ? nullptr : _scene;
}

void CalypsoDirector::setPreviewSuppressed(bool suppressed)
{
	_previewSuppressed = suppressed;
	if (suppressed) _previewBattle = nullptr;  // fresh preview -- rebind on the next onBattleStart
}

// --------------------------------------------------------------------------- //
// battle lifecycle (called from the upstream hooks)
// --------------------------------------------------------------------------- //

void CalypsoDirector::onBattleStart(BattlescapeState *bs, BattlescapeGame *bg, SavedBattleGame *save)
{
	// A preview suppression sticks for the whole preview battle (every turn,
	// so the coordinate readout keeps working) but must not leak into
	// whatever battle comes after it. onBattleStart fires once per battle
	// (BattlescapeState's _firstInit guard), so "a different save than the
	// one the suppression was bound to" means a new battle has started --
	// clear it. _previewBattle stays null until the preview battle's own
	// first call below binds it, so this can't fire prematurely.
	if (_previewSuppressed && _previewBattle != nullptr && _previewBattle != save)
	{
		_previewSuppressed = false;
		_previewBattle = nullptr;
	}

	// Always cache the fresh battle pointers (valid for one battle's lifetime).
	_battleState = bs;
	_battleGame = bg;
	_save = save;

	// Resume from save: load() already rebuilt + activated the scene; do NOT
	// re-run its first-frame setup (that would re-roll RNG, re-spawn, etc.).
	if (_scene) return;
	if (_previewSuppressed)
	{
		_previewBattle = save;  // bind the suppression to this preview battle
		Log(LOG_INFO) << "[director] battle start (preview) -- scenes suppressed";
		return;                 // scene-preview: director stays inert
	}
	if (!save) return;

	const std::string &mission = save->getMissionType();
	auto it = _registry.find(mission);
	if (it == _registry.end() || !it->second)
	{
		// One line per battle -- cheap, and "scene never activated" vs
		// "scene activated then broke" is the first question every scripted-
		// mission bug report needs answered.
		Log(LOG_INFO) << "[director] battle start '" << mission << "' -- no scene registered (" << _registry.size() << " in registry)";
		return;  // not a scripted battle
	}

	_scene = it->second();
	_activeDeploymentId = mission;
	Log(LOG_INFO) << "[director] scene activated for '" << mission << "'";
	if (_scene) _scene->onBattleStart(bg);
}

void CalypsoDirector::onEndTurn(SavedBattleGame *save)
{
	if (!_scene || !save) return;
	BattlescapeGame *bg = save->getBattleGame();
	if (!bg) return;
	// endTurn() has already resolved the new side (and incremented the turn for
	// the neutral->player transition). Dispatch per side; neutral is an empty
	// default scene callback (most scenes ignore it).
	UnitFaction side = save->getSide();
	if (side == FACTION_PLAYER)        _scene->onPlayerTurnStart(bg);
	else if (side == FACTION_HOSTILE)  _scene->onEnemyTurnStart(bg);
	else if (side == FACTION_NEUTRAL)  _scene->onNeutralTurnStart(bg);
}

void CalypsoDirector::onUnitDied(BattlescapeGame *bg, BattleUnit *victim, BattleUnit *killer)
{
	if (!_scene) return;
	// killer may be null (bleed-out, fire, environment) -- forwarded as-is.
	_scene->onUnitDied(bg, victim, killer);
}

bool CalypsoDirector::onEnemyTurnIdle(BattlescapeGame *bg)
{
	if (!_scene) return false;
	return _scene->onEnemyTurnIdle(bg);
}

bool CalypsoDirector::onAbortRequested(BattlescapeState *bs)
{
	if (!_scene) return false;
	// True = scene owns the ending; it will route via endScene(). The caller
	// (AbortMissionState::btnOkClick) pops itself and skips vanilla finishBattle.
	return _scene->onAbortRequested(bs);
}

bool CalypsoDirector::abortStrings(std::string *title, std::string *ok, std::string *cancel)
{
	if (!_scene) return false;
	std::string t, o, c;
	if (!_scene->abortStrings(&t, &o, &c)) return false;  // scene did not opt in
	Game *g = getCurrentGame();
	if (!g || !g->getLanguage()) return false;
	if (title)  *title  = std::string(g->getLanguage()->getString(t));
	if (ok)     *ok     = std::string(g->getLanguage()->getString(o));
	if (cancel) *cancel = std::string(g->getLanguage()->getString(c));
	return true;
}

bool CalypsoDirector::interceptFinishBattle(BattlescapeState *bs)
{
	(void)bs;
	if (!_scene) return false;  // not a scripted battle -> vanilla debrief

	// A pending outcome means the scene (not the engine) decided the battle is
	// over. Push the scene's end state instead of DebriefingState; if the scene
	// supplied no end state, still consume (suppress the vanilla score screen) --
	// a concrete end state is registered by a later commit.
	bool consume = (_outcome >= 0);
	if (consume)
	{
		State *end = _scene->makeEndState();
		if (end)
		{
			if (Game *g = getCurrentGame()) g->pushState(end);
			else delete end;  // no live game -- don't leak
		}
	}
	// The battle is over either way: tear the scene down so a following battle
	// (or a non-scripted one) starts clean.
	endBattleCleanup();
	return consume;
}

bool CalypsoDirector::interceptUnexpectedFinish(BattlescapeState *bs, bool abort)
{
	if (!_scene || _outcome >= 0) return false;
	int outcome = -1;
	if (!_scene->onUnexpectedFinish(bs, abort, &outcome)) return false;
	if (outcome >= 0)
	{
		// The scene converted vanilla completion into one of its own outcomes.
		// Let this same finishBattle call continue; the late intercept will build
		// the scene end state after the normal battlescape cleanup.
		_outcome = outcome;
		return false;
	}
	return true; // consumed without outcome: keep the scripted battle running
}

void CalypsoDirector::endBattleCleanup()
{
	delete _scene;
	_scene = nullptr;
	_activeDeploymentId.clear();
	_outcome = -1;
	_battleState = nullptr;
	_battleGame = nullptr;
	_save = nullptr;
}

// --------------------------------------------------------------------------- //
// primitives (scene-agnostic; scenes call these)
// --------------------------------------------------------------------------- //

bool CalypsoDirector::directedShot(BattlescapeGame *bg, BattleUnit *shooter, BattleUnit *target, bool intendMiss)
{
	if (!bg || !shooter || !target) return false;
	SavedBattleGame *save = bg->getSave();
	if (!save) return false;

	// Resolve the shooter's main-hand weapon. The caller (scene) guarantees the
	// shooter is a real, armed unit -- the whole point is that the vanilla
	// projectile pipeline runs with a real attacker (no synthetic damage).
	BattleItem *weapon = shooter->getMainHandWeapon();
	if (!weapon) return false;

	// Impact tile: the victim's tile for a kill, or an adjacent tile for a
	// scripted miss (the projectile visibly passes the victim). For the miss we
	// just nudge one tile east if that tile exists; the map author guarantees an
	// open line (phase plan 41.3 / Known Pitfalls: marksman trajectory).
	Position aim = target->getPosition();
	if (intendMiss)
	{
		Position nudge = aim;
		nudge.x += 1;
		if (save->getTile(nudge)) aim = nudge;
	}

	// Grant TU so the shot is always affordable. The director owns this unit's
	// turn budget (it has no AI and no reserved TU). Documented in the header.
	shooter->setTimeUnits(shooter->getBaseStats()->tu);

	// Build a real BattleAction exactly as BattlescapeGame::primaryAction does,
	// but on a LOCAL action so the player's pending _currentAction is untouched.
	// cameraPosition = (0,0,-1) is the engine's own "hidden shot" default that
	// suppresses the camera jump (verified in primaryAction's default ctor).
	BattleAction action;
	action.actor = shooter;
	action.weapon = weapon;
	action.type = BA_SNAPSHOT;
	// ProjectileFlyBState rejects an action whose cost was never populated:
	// BattleActionCost::haveTU() returns false when Time <= 0. AI attacks call
	// updateTU() before queueing the same turn/projectile pair; scripted shots
	// must do so too. The TU refill above guarantees the computed cost fits.
	action.updateTU();
	action.target = aim;
	action.cameraPosition = Position(0, 0, -1);

	// Replicate primaryAction's semantics through the public API. primaryAction
	// does a raw `_states.push_back(fly)` (NO init) + `statePushFront(turn)`
	// (init) -- turn runs first, fly inits only when the turn state pops. With
	// public calls that order matters: statePushFront always inits, and
	// statePushBack inits immediately on an EMPTY queue -- so push the turn
	// state FIRST (queue empty -> init), then the fly state (queue now
	// non-empty -> appended without init). A repeat-fire kill loop is the
	// scene's concern; this primitive fires exactly one shot per call.
	bg->statePushFront(new UnitTurnBState(bg, action));
	bg->statePushBack(new ProjectileFlyBState(bg, action));
	return true;
}

void CalypsoDirector::steerUnit(BattlescapeGame *bg, BattleUnit *unit, Position waypoint)
{
	if (!bg || !unit) return;
	SavedBattleGame *save = bg->getSave();
	if (!save) return;
	Pathfinding *pf = save->getPathfinding();
	if (!pf) return;

	// Afford the walk (director-owned unit, no reserved TU) then compute the path
	// and let the real walking state follow it -- no AI involved. Verified path:
	// Pathfinding::calculate + statePushBack(UnitWalkBState) (phase plan 41.4).
	unit->setTimeUnits(unit->getBaseStats()->tu);
	pf->calculate(unit, waypoint, BAM_NORMAL);

	BattleAction action;
	action.actor = unit;
	action.target = waypoint;
	bg->statePushBack(new UnitWalkBState(bg, action));
}

void CalypsoDirector::handoffToPlayer(BattlescapeGame *bg, BattleUnit *unit)
{
	if (!bg || !unit) return;
	// convertToFaction is the mind-control primitive; it gives the player full
	// control and nothing reverts it at turn end. The unit's _originalFaction is
	// unchanged, so it stays correctly tallied (verified, phase plan 41.4).
	unit->convertToFaction(FACTION_PLAYER);
	if (SavedBattleGame *save = bg->getSave()) save->setSelectedUnit(unit);
	if (Map *map = bg->getMap())
		if (Camera *cam = map->getCamera())
			cam->centerOnPosition(unit->getPosition());
}

void CalypsoDirector::pinMorale(SavedBattleGame *save, UnitFaction side)
{
	if (!save) return;
	// BattleUnit exposes morale as a delta only (moraleChange(int)); pin to 100
	// by applying the delta 100 - current. Clamp is implicit: morale is capped
	// internally by the engine. Pins panic-driven loss of control while a scene
	// is directing (phase plan 41.3 "Morale pinning").
	for (BattleUnit *u : *save->getUnits())
	{
		if (u->getFaction() == side && !u->isOut())
		{
			u->moraleChange(100 - u->getMorale());
		}
	}
}

void CalypsoDirector::radioLine(Game *game, const std::string &stringId)
{
	if (!_scene || !game || stringId.empty()) return;
	// Transient toast (CalypsoRadioLineState) that shows tr(stringId) through
	// the existing tutorial DOM overlay. See CalypsoRadioLineState.h for why
	// CalypsoTutorialState is not reused here.
	game->pushState(new CalypsoRadioLineState(stringId));
}

void CalypsoDirector::endScene(BattlescapeGame *bg, int outcome)
{
	(void)bg;  // the cached _battleState drives the finish; bg kept for API symmetry
	if (!_scene) return;
	_outcome = outcome;
	// Drive the battle-end flow through the single public site
	// (BattlescapeState::finishBattle). interceptFinishBattle -- hooked right
	// before the DebriefingState push inside finishBattle -- consumes the pending
	// outcome and pushes the scene's makeEndState() instead. CAVEAT: this is
	// called from within a scene callback (e.g. onEnemyTurnStart, dispatched at
	// the end of SavedBattleGame::endTurn); finishBattle pops states and pushes
	// the end state mid-state-machine, exactly as BattlescapeGame::autoEndBattle
	// does. A later commit validates this under the real scene; the primitive's
	// contract is just to route the end through finishBattle.
	if (_battleState) _battleState->finishBattle(false, 0);
}

bool CalypsoDirector::activeSceneBlocksSaveLoad() const
{
	return _scene && _scene->blocksSaveLoad();
}

void CalypsoDirector::forceAutosave(Game *game, const std::string &filename) const
{
	if (!game) return;
	SavedGame *save = game->getSavedGame();
	if (!save) return;
	// setName() only affects the label shown in a save list; this slot is
	// never surfaced there (D6 -- written/deleted directly, no SaveGameState).
	save->setName(filename);
	save->save(filename, game->getMod()); // flushes IDBFS itself on Emscripten
}

// --------------------------------------------------------------------------- //
// persistence (dispatches to the active scene)
// --------------------------------------------------------------------------- //

void CalypsoDirector::save(YAML::YamlNodeWriter writer) const
{
	if (!_scene) return;  // no scripted scene -> write nothing (no node)
	writer.setAsMap();
	writer.write("sceneId", _activeDeploymentId);
	// Scene-owned child node (matches the SavedGame.cpp `writer["calypsoTutorial"]`
	// precedent -- by-value writer for a child map).
	_scene->save(writer["state"]);
}

void CalypsoDirector::load(const YAML::YamlNodeReader &reader, SavedBattleGame *save)
{
	// Always start clean -- the singleton is reused across battles/loads.
	endBattleCleanup();

	std::string sceneId = reader["sceneId"].readVal<std::string>(std::string());
	if (sceneId.empty()) return;  // no scene in this save
	auto it = _registry.find(sceneId);
	if (it == _registry.end() || !it->second)
	{
		Log(LOG_WARNING) << "[director] no factory registered for saved sceneId '" << sceneId << "'";
		return;
	}
	_scene = it->second();
	_activeDeploymentId = sceneId;
	_save = save;
	// _battleGame / _battleState are re-cached when BattlescapeState::init fires
	// onBattleStart (which sees the scene already active and skips re-init).
	if (_scene) _scene->load(reader["state"]);
}

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
