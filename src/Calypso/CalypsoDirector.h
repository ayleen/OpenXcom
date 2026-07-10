#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Phase 41 (Calypso) -- reusable scripted-scene primitive layer.
 *
 * CalypsoDirector is a lazy singleton (same shape as CalypsoTutorial::get())
 * that owns the COMPLETE upstream battlescape hook surface -- battle start,
 * per-faction turn start, unit death, mission-abort confirmation, and the
 * finish-battle intercept -- plus a set of scene-agnostic primitives any
 * scripted mission needs: a directed (forced) shot through the real projectile
 * pipeline, unit steering, faction handoff, morale pin, a radio-line popup, and
 * per-scene save/load. CalypsoScene is the abstract base class a concrete
 * scripted mission (e.g. the Phase 41 prologue) subclasses.
 *
 * The director is deliberately scene-agnostic: it contains no port, no named
 * actors, no deployment ids, no mission-specific words -- those live in a
 * CalypsoScene subclass and its mod. A second scripted mission plugs in by
 * writing a new CalypsoScene subclass and one registerScene() line; it must not
 * need a new upstream hook site.
 *
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 * See docs/phases/phase-41-tutorial-mission.md sections 41.2 and 41.3.
 */
#ifdef __EMSCRIPTEN__

#include <string>
#include <functional>
#include <map>
#include "../Battlescape/Position.h"

namespace OpenXcom
{

// Forward declarations to keep this header light (matches CalypsoTutorial.h).
namespace YAML { class YamlNodeReader; class YamlNodeWriter; }
class Game;
class State;
class BattlescapeGame;
class BattlescapeState;
class SavedBattleGame;
class BattleUnit;
enum UnitFaction : int;

/**
 * Abstract base for one scripted battlescape scene.
 * One scene scripts one deployment; future story missions subclass this and
 * register a factory with CalypsoDirector. All callbacks default to no-ops so a
 * subclass only overrides what it needs. Pointers passed to callbacks are valid
 * only for the duration of the call -- scenes must NOT retain them; persist unit
 * ids (CalypsoDirector::save/load re-resolves them), never BattleUnit*.
 */
class CalypsoScene
{
public:
	virtual ~CalypsoScene() = default;

	/// Battle just started (fresh) -- the scene's first-frame setup. NOT re-called
	/// on save/load resume (the director rebuilds the scene from the save instead).
	virtual void onBattleStart(BattlescapeGame *) {}
	/// A new player turn has just begun (side already switched, turn incremented).
	virtual void onPlayerTurnStart(BattlescapeGame *) {}
	/// A new hostile turn has just begun.
	virtual void onEnemyTurnStart(BattlescapeGame *) {}
	/// A new neutral turn has just begun (empty by default; most scenes ignore it).
	virtual void onNeutralTurnStart(BattlescapeGame *) {}
	/// Called instead of vanilla AI, once per think() tick, whenever a scripted
	/// scene is active and the acting side is hostile or neutral (the director
	/// suppresses AI entirely for both -- scripted actors have no autonomy).
	/// Return true if the scene queued more work (states pushed this call);
	/// false when the scene has nothing left to do this turn -- the director
	/// then ends the side's turn the vanilla way.
	virtual bool onEnemyTurnIdle(BattlescapeGame *) { return false; }
	/// A unit died this frame. `killer` may be null (bleed-out, environment).
	/// Fires exactly once per actual new death (never for already-dead units).
	virtual void onUnitDied(BattlescapeGame *, BattleUnit *victim, BattleUnit *killer) {}
	/// Mission-abort was confirmed by the player. Return true if the scene
	/// consumes the abort -- it then routes the ending itself via endScene(); the
	/// vanilla finishBattle(true,...) is NOT called.
	virtual bool onAbortRequested(BattlescapeState *) { return false; }
	/// Opt-in: override the abort-confirmation window strings. Populate the three
	/// ids with extraStrings keys and return true; the director translates them.
	virtual bool abortStrings(std::string *title, std::string *ok, std::string *cancel) { return false; }
	/// End state to push instead of the vanilla Debriefing when the scene has set
	/// an outcome. Return null to fall through to the standard debrief. The
	/// concrete scene owns the returned State* (pushed by the director).
	virtual State *makeEndState() { return nullptr; }
	/// Serialize scene-owned state under the director's `calypsoScene.state` node.
	virtual void save(YAML::YamlNodeWriter) const {}
	/// Restore scene-owned state. Unit pointers are NOT valid yet -- re-resolve by id.
	virtual void load(const YAML::YamlNodeReader &) {}
	/// Opt-in: true suppresses every battlescape save/load entry point (pause
	/// menu, quick-save/quick-load, the vanilla per-turn autosave) while this
	/// scene is active. Scenes that roll their own forced autosave (D6) want
	/// this so the player cannot savescum around the scripted outcome.
	virtual bool blocksSaveLoad() const { return false; }
};

/**
 * Owns the upstream hook surface and the scene-agnostic primitives.
 */
class CalypsoDirector
{
public:
	/// Factory that constructs a fresh scene for a deployment id.
	using SceneFactory = std::function<CalypsoScene *()>;

	/// Lazy singleton (function-local static; no static-init ordering hazard).
	static CalypsoDirector &get();

	/// Register a scene factory keyed by the SavedBattleGame mission type
	/// (deployment id). The prologue registers itself; future scenes add a line.
	void registerScene(const std::string &deploymentId, SceneFactory factory);

	/// The active scene, or null when no scripted scene is running. Returns null
	/// while preview-suppressed (scene-preview mode suppresses the director).
	CalypsoScene *active() const;

	/// Scene-preview mode: while set, the director activates no scene (the map
	/// boots inert for inspection). See phase plan 41.1c.
	void setPreviewSuppressed(bool suppressed);

	// ---- battle lifecycle (called from the upstream hooks) -----------------

	/// First-frame battle start. Looks the mission type up in the registry; on a
	/// hit it constructs + activates the scene and fires its onBattleStart. On a
	/// save/load resume the scene is already active (rebuilt by load()) and this
	/// call only caches the fresh battle pointers without re-initialising.
	void onBattleStart(BattlescapeState *bs, BattlescapeGame *bg, SavedBattleGame *save);

	/// End of SavedBattleGame::endTurn -- dispatches onPlayer/onEnemy/onNeutral
	/// turn start to the active scene based on the side that just took over.
	void onEndTurn(SavedBattleGame *save);

	/// A unit died (forwarded from the casualty pipeline). Killer may be null.
	void onUnitDied(BattlescapeGame *bg, BattleUnit *victim, BattleUnit *killer);

	/// Forwards to the active scene's onEnemyTurnIdle (see CalypsoScene). Returns
	/// false (let vanilla AI/end-turn proceed) when no scene is active.
	bool onEnemyTurnIdle(BattlescapeGame *bg);

	/// Abort confirmed by the player. Returns true if the scene consumed it.
	bool onAbortRequested(BattlescapeState *bs);

	/// Opt-in string swap for the abort-confirmation window. Asks the active
	/// scene for three extraStrings ids and translates them via the current
	/// language. Returns false (no swap) when no scene is active or the scene
	/// does not opt in. Called from the AbortMissionState ctor hook.
	bool abortStrings(std::string *title, std::string *ok, std::string *cancel);

	/// Intercept the vanilla finish-battle flow. Returns true if the director
	/// pushed the scene's end state (the caller must `return` and skip Debriefing).
	/// Always tears down the active scene (the battle is over either way).
	bool interceptFinishBattle(BattlescapeState *bs);

	/// Deactivate + delete the active scene. Called from interceptFinishBattle
	/// (and safe to call when no scene is active).
	void endBattleCleanup();

	// ---- primitives (scene-agnostic; scenes call these) --------------------

	/// Force a single real shot from `shooter` at `target`, through the vanilla
	/// projectile pipeline (real projectile, FX, sound, hit, casualty + morale
	/// attribution). `intendMiss` aims at an adjacent tile so the shot visibly
	/// misses the victim. The shooter's TU is granted beforehand so the shot is
	/// always affordable. Returns true unless a precondition failed. Trajectory
	/// validity is NOT pre-checked here -- a blocked line surfaces implicitly as
	/// the projectile impacting the obstruction (see CalypsoDirector.cpp notes).
	bool directedShot(BattlescapeGame *bg, BattleUnit *shooter, BattleUnit *target, bool intendMiss);

	/// Path `unit` toward `waypoint` through the real walking state (no AI).
	/// TU is granted beforehand so the move is always affordable.
	void steerUnit(BattlescapeGame *bg, BattleUnit *unit, Position waypoint);

	/// Hand a unit to the player: convertToFaction(FACTION_PLAYER), select it,
	/// and centre the camera on it. Used by the prologue's Nikos handoff.
	void handoffToPlayer(BattlescapeGame *bg, BattleUnit *unit);

	/// Reset every live unit of `side` to morale 100 (pins panic-driven loss of
	/// control while a scene is directing). Uses BattleUnit::moraleChange(delta).
	void pinMorale(SavedBattleGame *save, UnitFaction side);

	/// Push a transient radio-line popup showing tr(stringId). No-op without an
	/// active scene. Renders through the tutorial DOM overlay (no web-shell change
	/// required for this commit; presentation may be skinned later).
	void radioLine(Game *game, const std::string &stringId);

	/// Record a scene outcome and drive the battle to its end. The outcome is
	/// consumed by interceptFinishBattle (hooked in BattlescapeState::finishBattle),
	/// which pushes the scene's makeEndState() instead of the vanilla debrief.
	void endScene(BattlescapeGame *bg, int outcome);

	/// True while the active scene opts into blocking save/load (see
	/// CalypsoScene::blocksSaveLoad). False (no gate) when no scene is active.
	bool activeSceneBlocksSaveLoad() const;

	/// Write `game`'s current SavedGame directly to `filename` -- no UI, no
	/// SaveGameState. Used for a scene's own rolling anti-savescum autosave
	/// (D6); SavedGame::save() already flushes IDBFS on Emscripten. Scene-
	/// agnostic: the caller supplies the slot name.
	void forceAutosave(Game *game, const std::string &filename) const;

	// ---- persistence (dispatches to the active scene) ----------------------

	/// Serialize the active scene under the given (map) writer: `sceneId` plus a
	/// scene-owned `state` child node. No-op without an active scene.
	void save(YAML::YamlNodeWriter writer) const;

	/// Rebuild the active scene from a `calypsoScene` reader: look the sceneId up
	/// in the registry, construct the scene, mark it active, and call its load().
	void load(const YAML::YamlNodeReader &reader, SavedBattleGame *save);

private:
	CalypsoDirector() = default;

	CalypsoScene *_scene = nullptr;
	std::string _activeDeploymentId;             ///< mission type of the running scene
	std::map<std::string, SceneFactory> _registry;
	bool _previewSuppressed = false;
	int _outcome = -1;                           ///< <0 = no scene outcome pending

	/// Cached for the lifetime of one battle (set in onBattleStart). Pointers
	/// only -- never serialized; re-cached on every battle start / resume.
	BattlescapeState *_battleState = nullptr;
	BattlescapeGame *_battleGame = nullptr;
	SavedBattleGame *_save = nullptr;
};

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
