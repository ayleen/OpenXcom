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
 * Phase 41 (Calypso) -- the prologue scene "The Port": empty-port landing,
 * bureaucracy-driven ambush, the impossible retreat, "Do not wait for me."
 * A CalypsoScene subclass over the CalypsoDirector primitive layer -- see
 * CalypsoDirector.h for the primitives this scene calls, and
 * docs/phases/phase-41-tutorial-mission.md sections 41.3/41.4 for the design.
 *
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 */
#ifdef __EMSCRIPTEN__

#include <string>
#include <vector>

#include "CalypsoDirector.h"

namespace OpenXcom
{

class BattlescapeGame;
class BattlescapeState;
class SavedBattleGame;
class BattleUnit;

/**
 * The port prologue. Holds no BattleUnit pointers -- only ids (re-resolved
 * from SavedBattleGame on every call and on load). See CalypsoScene for the
 * lifecycle contract; this class only overrides what the script needs.
 */
class CalypsoPrologueScene : public CalypsoScene
{
public:
	/// Scene outcomes -- passed verbatim to CalypsoDirector::endScene(). A
	/// plain enum (not enum class) so it converts implicitly to endScene's int.
	enum Outcome { OutcomeCastOff = 0, OutcomeAllTaken = 1 };

	void onBattleStart(BattlescapeGame *bg) override;
	void onBattleResume(BattlescapeGame *bg) override;
	void onPlayerTurnStart(BattlescapeGame *bg) override;
	void onEnemyTurnStart(BattlescapeGame *bg) override;
	bool onEnemyTurnIdle(BattlescapeGame *bg) override;
	void onUnitDied(BattlescapeGame *bg, BattleUnit *victim, BattleUnit *killer) override;
	bool onAbortRequested(BattlescapeState *bs) override;
	bool onUnexpectedFinish(BattlescapeState *bs, bool abort, int *outcome) override;
	bool abortStrings(std::string *title, std::string *ok, std::string *cancel) override;
	bool abortAvailable() const override;
	bool abortConfirmAvailable(SavedBattleGame *save) const override;
	State *makeEndState() override; // Commit 4: CalypsoPrologueEndState ("six months later")
	void save(YAML::YamlNodeWriter writer) const override;
	void load(const YAML::YamlNodeReader &reader) override;
	bool blocksSaveLoad() const override { return true; } // D3/D6: no player-driven save/load during the prologue

private:
	/// Script phases (design doc mechanical chronicle, docs/tutorial-mission-design.md).
	enum class Ph { Landing, MoveToOffice, Ambushed, Gauntlet, Ended };

	// ---- actor resolution -------------------------------------------------
	bool resolveActors(BattlescapeGame *bg);
	static BattleUnit *findUnit(SavedBattleGame *save, int id);
	std::vector<int> allPlayerIds() const; // leader + both divers (never Nikos)
	int activeHerderId() const;
	/// QA round 1 bug 7: teleport the marksman onto a fixed perch -- see the
	/// .cpp for why (no elevated RMP nodes on this terrain).
	void placeMarksman(BattlescapeGame *bg);
	/// Review round 1 (P1): teleport Nikos / the herders onto their scripted
	/// tiles (SE post, off-path pen). The Assessor is placed separately (see
	/// placeAssessorOnFreeCraftSlot) -- review round 2 finding 1.
	void placeNamedActors(BattlescapeGame *bg);
	/// Review round 2 (P1, finding 1): scan the Nereid's real START_POINT
	/// deployment tiles for one that is free (same predicate
	/// BattlescapeGenerator::canPlaceXCOMUnit uses for real crew placement)
	/// and teleport the Assessor onto it -- replaces the old fixed-tile
	/// ASSESSOR_POST guesswork, which could land on an already-occupied crew
	/// slot. Returns false (no free slot found) -- the caller must go inert.
	bool placeAssessorOnFreeCraftSlot(BattlescapeGame *bg);
	/// Review round 3 (P1): real Pathfinding-cost DIAGNOSTIC on Nikos's
	/// scripted post (log-only; the player is his only mover after the
	/// handoff, so the guarantee is path cost / his per-turn TU). Not a
	/// survival gate -- he is force-killed on any ending regardless.
	void checkNikosPathCost(BattlescapeGame *bg);
	/// Restore additive scripted-unit state after loading an autosave. Old
	/// prologue saves did not serialize these scene flags, so phase is the
	/// compatibility fallback.
	void reconcileScriptedUnitState(BattlescapeGame *bg);
	/// Release the ambusher exactly at the Assessor-death narrative gate.
	void revealMarksman(BattlescapeGame *bg);
	/// Complete Nikos's handoff when player input becomes actionable.
	void focusNikosOnPlayerTurn(BattlescapeGame *bg);

	// ---- turn-idle step machine (onEnemyTurnIdle dispatch) -----------------
	bool stepMoveToOffice(BattlescapeGame *bg);
	bool stepAmbushed(BattlescapeGame *bg);
	bool stepGauntlet(BattlescapeGame *bg);
	void steerActiveHerder(BattlescapeGame *bg);
	int pickGauntletTarget(SavedBattleGame *save) const;

	// ---- endings ------------------------------------------------------------
	/// Detection only -- ARMS a pending ending (never executes it). Safe to call
	/// from inside checkForCasualties (via onUnitDied), where nested kills or
	/// finishBattle would re-enter the casualty loop.
	void checkBranchB(BattlescapeGame *bg);
	/// Switch to a non-hostile extraction-only tail after the player removes the
	/// scripted threat. No further gauntlet actions run; cast-off stays active.
	void enterEvacOnly(BattlescapeGame *bg, bool announceObjective);
	/// Execution -- runs the armed ending (radio, takings, endScene) from a
	/// clean stack: the onEnemyTurnIdle pump or onPlayerTurnStart, the same
	/// contexts vanilla itself calls finishBattle from. Returns true if an
	/// ending was pending and has now been executed.
	bool resolvePendingEnding(BattlescapeGame *bg);
	void killNikosIfAlive(BattlescapeGame *bg);
	void killEveryoneAboard(BattlescapeGame *bg);

	// ---- primitives ---------------------------------------------------------
	/// Real damage() + checkForCasualties() through the actual casualty pipeline
	/// (morale, corpse, tile update) -- the last-resort fallback when a
	/// repeat-fire loop hits SHOT_CAP_PER_TURN, and for scripted off-screen
	/// deaths (Nikos). `attacker` may be null (unattributed, environment-style).
	static void forceKill(BattlescapeGame *bg, BattleUnit *victim, BattleUnit *attacker);
	/// Design amendment #10 ("taken, not corpses", design doc s8 #10): remove
	/// the corpse items of units queued in _pendingTakenIds -- the water
	/// collects the fallen. Runs at player-turn start (clean stack, right
	/// after the Choir turn that dropped them), before the rolling autosave so
	/// a reload never resurrects a collected body.
	void collectTakenBodies(BattlescapeGame *bg);
	void radio(const std::string &stringId, CalypsoRadioLineKind kind = CalypsoRadioLineKind::Narrative) const;

	// ---- state ----------------------------------------------------------------
	Ph _phase = Ph::Landing;
	bool _inert = false;            ///< actor resolution failed at onBattleStart
	bool _endingTriggered = false;  ///< an ending is armed or executed -- the script stops
	int _pendingOutcome = -1;       ///< armed ending awaiting a safe stack (resolvePendingEnding)
	bool _pendingTaking = false;    ///< armed ending is the Branch Б boarding flavor
	bool _evacOnly = false;         ///< Choir neutralized early: no more scripted attacks, cast-off remains available
	bool _marksmanRevealed = false; ///< persisted Assessor-death reveal gate
	bool _nikosHandedOff = false;   ///< persisted ownership (separate from camera focus)
	bool _nikosFocusPending = false; ///< select/centre once on the next player turn

	// actor ids (never pointers -- re-resolved every call)
	int _leaderId = -1;
	std::vector<int> _diverIds;     ///< exactly 2 once resolved
	int _assessorId = -1;
	int _nikosId = -1;
	int _marksmanId = -1;
	std::vector<int> _herderIds;    ///< exactly 2 once resolved

	// D2: randomized first-death beats
	bool _leaderDiesFirst = false;
	bool _firstDeathDone = false;   ///< first post-Assessor gauntlet death has happened
	bool _diverMissFired = false;   ///< the diver pattern's scripted-miss beat already fired

	// gauntlet step machine (reset every onEnemyTurnStart)
	int _gauntletStep = 0;          ///< 0=steer herder, 1=shoot victim, 2=steer Nikos, 3=idle
	int _currentVictimId = -1;      ///< unit being shot this Choir turn (persists across idle pumps)
	int _shotsThisTurn = 0;         ///< repeat-fire counter -> SHOT_CAP_PER_TURN -> forceKill
	int _activeHerderIdx = -1;      ///< index into _herderIds currently being steered

	int _lastNagStage = -1;         ///< last escalationStage() value a nag line fired for

	/// Design amendment #9 (design doc s8 #9): Choir turns bought by killing
	/// herders. Incremented in onUnitDied per dead herder (max 2 herders =
	/// max 2), consumed in stepGauntlet's victim step -- but never against
	/// the FIRST post-Assessor loss (that beat is uncancellable).
	int _herderReprieveTurns = 0;
	/// Design amendment #10: gauntlet victims whose bodies the water has not
	/// collected yet -- processed by collectTakenBodies() at player-turn start.
	std::vector<int> _pendingTakenIds;

	/// The outcome last passed to CalypsoDirector::endScene(). Set right before
	/// that call (both call sites: resolvePendingEnding and onAbortRequested)
	/// so makeEndState() -- called synchronously from within the same endScene
	/// -> finishBattle -> interceptFinishBattle chain -- knows which ending to
	/// stage. Not persisted: never valid across a save/load (the battle is over
	/// by the time it matters, and the scene is torn down right after).
	int _finishedOutcome = -1;
};

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
