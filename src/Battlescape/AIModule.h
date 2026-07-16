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
#include "../Engine/Yaml.h"
#include "BattlescapeGame.h"
#include "Position.h"
#include "Pathfinding.h" // Brutal-AI: PathfindingNode, PositionComparator
#include "AIFailureMemory.h"
#include "SpotterCountField.h" // Phase 43.1 (Calypso): per-actor exact spotter-count memo (by-value member)
#include "../Savegame/BattleUnit.h"
#include <vector>
#include <set> // Brutal-AI
#include <array>


namespace OpenXcom
{

class SavedBattleGame;
class BattleUnit;
struct BattleAction;
class BattlescapeState;
class Node;
class FriendReachableField; // Phase 43.1E (Calypso): shared friendReachable field, defined in Savegame/FriendReachableField.h
class ThreatField; // Phase 43.1M (Calypso): shared exact discoverThreat memo, defined in Savegame/ThreatField.h
class TerrainLofNegativeCache; // Phase 43.1I (Calypso): shared negative terrain-LOF cache, defined in Savegame/TerrainLofNegativeCache.h

enum AIMode { AI_PATROL, AI_AMBUSH, AI_COMBAT, AI_ESCAPE };
enum AIAttackWeight : int
{
	/// Base scale of attack weights
	AIW_SCALE = 100,
	AIW_IGNORED = 0,
};

/**
 * This class is used by the BattleUnit AI.
 */
class AIModule
{
private:
	SavedBattleGame *_save;
	BattleUnit *_unit;
	BattleUnit *_aggroTarget;
	int _knownEnemies, _visibleEnemies, _spottingEnemies;
	int _escapeTUs, _ambushTUs;
	bool _weaponPickedUp;
	bool _wantToEndTurn = false; // Brutal-AI (must default false: read via BattleUnit::getWantToEndTurn for all units)
	bool _rifle, _melee, _blaster, _grenade;
	bool _traceAI, _didPsi;
	bool _ranOutOfTUs = false; // Brutal-AI
	int _AIMode, _intelligence, _closestDist;
	Node *_fromNode, *_toNode;
	bool _foundBaseModuleToDestroy;
	int _lastBreachTurn = -1000; // Phase 34.6 (Calypso): transient, NOT saved -- gates wall-breaches to one attempt per 3 turns.
	std::vector<int> _reachable, _reachableWithAttack, _wasHitBy;
	std::vector<PathfindingNode*> _allPathFindingNodes; // Brutal-AI
	std::vector<Position> _pathToEnemyPositions; // Phase 43.0: reusable buffer for getPositionsOnPathTo (targetPosition path), kept valid across the node loop
	std::vector<Position> _pathToPosBuffer; // Phase 43.0: reusable buffer for getPositionsOnPathTo (per-node path, distinct so it never clobbers _pathToEnemyPositions)
	Position _positionAtStartOfTurn; // Brutal-AI
	int _tuCostToReachClosestPositionToBreakLos = 0; // Brutal-AI
	int _energyCostToReachClosestPositionToBreakLos = 0; // Brutal-AI
	int _tuWhenChecking = 0; // Brutal-AI
	bool _allowedToCheckAttack = false; // Brutal-AI
	BattleActionType _reserve;
	UnitFaction _targetFaction;
	UnitFaction _myFaction = FACTION_HOSTILE; // Brutal-AI
	mutable int _committedAttackTargetId = -1; // Phase 43 (H5): target this unit committed to attacking this turn (-1 = none)
	mutable int _committedAttackTurn = -1;     // Phase 43 (H5): the turn number _committedAttackTargetId was recorded for
	// Phase 43.1 (Calypso): per-AIModule exact spotter-count memo. Owned per-actor and NOT in
	// FactionTurnCache because getSpottingUnits()'s result is actor-specific -- it depends on
	// this unit's validTarget()/knowledge profile and on the virtual target geometry handed to
	// canTargetUnit (the `_unit` stand-in placed at a non-occupied pos). Lifetime is a SINGLE
	// think() pass: cleared at the very top of think() so a cached count never survives world
	// changes between immediate rethink passes. Written and read only inside getSpottingUnits()
	// and only when ai.sharedFields is on; with the flag off the member stays empty and unused.
	// Mutable because getSpottingUnits() is const. An unknown tile is NEVER consumed -- the
	// isEvaluated() gate is the only thing that separates a confirmed (incl. zero) count from
	// a never-evaluated tile whose countAt() would optimistically read 0 (see SpotterCountField.h).
	mutable SpotterCountField _spotterCountMemo;
	AIFailureMemory _failureMemory;
	std::string _auditReason, _auditRunnerUp;
	float _auditBestScore = 0.0f, _auditRunnerUpScore = 0.0f;
	std::array<float, 3> _lastScoreTerms{{0, 0, 0}};
	std::array<float, 3> _auditBestTerms{{0, 0, 0}};
	std::array<const char*, 3> _auditTermLabels{{"damage", "hit", "context"}};

	BattleAction _escapeAction, _ambushAction, _attackAction, _patrolAction, _psiAction;

	bool selectPointNearTargetLeeroy(BattleUnit *target, bool canRun);
	int selectNearestTargetLeeroy(bool canRun);
	void meleeActionLeeroy(bool canRun);
	void dont_think(BattleAction *action);
	/// Phase 32: true for an organic civilian when the mod enables smarter civilian AI.
	bool isSmartCivilian() const;
	/// Phase 32: nearest "protector" to head toward — closest recently-spotted soldier, else closest
	/// civilian. Horizontal (2D) distance to match the escape/objective scoring. False if none known.
	bool findNearestProtector(Position& out) const;
	/// Phase 32: position a fleeing civilian should head toward (nearest soldier > civilian cluster > map edge).
	bool findCivilianSafetyTarget(Position& out) const;
	/// Phase 32: true for an armed civilian guard (a smart civilian whose ruleset sets civilianGuard).
	bool isCivilianGuard() const;
	/// Phase 32: nearest distressed civilian a guard can "hear" (panic/low morale/threatened nearby).
	BattleUnit *findDistressedCivilian() const;
	/// Phase 32: where a guard should advance when no alien is perceived (rescue civ > regroup with soldier > stay with crowd).
	bool findGuardObjective(Position& out) const;
	/// Phase 32: fill _patrolAction with a walk toward the guard objective; returns false if none.
	bool setupGuardMove();
	/// Phase 34.4: true for an unengaged (no known enemy) hostile alien when the mod enables
	/// ai.terrorHuntCivilians -- biases patrol node choice toward the civilian-hunt zone.
	bool wantsToHuntCivilians() const;
	/// Phase 34.8 (Calypso): true for an unengaged (no known enemy) hostile alien when the
	/// mod enables ai.hearing -- biases patrol node choice toward the newest in-range noise
	/// zone (quantized to an 8-tile grid). Same faction / known-enemies gates as
	/// wantsToHuntCivilians; used only as a fallback when no civilian-hunt zone (34.4) applies,
	/// so the two behaviours stay disjoint and each remains gated on its own flag.
	bool wantsToInvestigateNoise() const;
	/// Phase 34.6 (Calypso): terrain-tactics candidate-attack generator (floor-drop + wall-breach).
	/// Returns true and fills _attackAction only when the mod's ai.terrainTactics flag is on, the
	/// unit is a non-civilian hostile, no other attack has been chosen yet (_attackAction.type ==
	/// BA_RETHINK), and a fair-channel-known enemy stands on a destructible floor or a wall is
	/// blocking the path to a known objective. With the flag off it is a no-op (byte-identical).
	bool considerTerrainAttack();
	bool candidateAllowed(BattleActionType type, int targetId, const Position& position) const;
	void prepareAIAudit(BattleAction *action);
	/// Phase 43.1E (Calypso): returns the live shared friendReachable field for this
	/// unit's faction when the mod enables ai.sharedFields and a valid faction turn-cache
	/// exists, rebuilding it lazily (dirty -> recompute, clean -> stamp only missing
	/// contributions); returns nullptr otherwise so callers fall back to the legacy
	/// per-unit local map. Never overwrites _ranOutOfTUs (uses a local flag). When
	/// ai.evalBudget > 0 the rebuild is bounded to at most evalBudget actual BFS stamps
	/// per call (the acting unit first, only if its slice is missing; otherwise the full
	/// evalBudget is spent on missing allies), leaving the rest missing as a documented
	/// shared-field approximation that later brutalThinks fill incrementally; evalBudget == 0
	/// is the unbounded byte-identical original full rebuild.
	FriendReachableField* prepareSharedFriendReachable();
	/// Resolve the exact shared enemyReachable accumulator for this actor's
	/// fair-knowledge profile. `forceRebuild` is true after a dirty full clear.
	/// When `allowReset` is false (the read-only mode used by setupEscape) a dirty
	/// field is NOT cleared / marked clean without producing it -- the caller falls
	/// back to the degraded ranking and brutalThink remains the authoritative producer
	/// that may clear / start the rebuild.
	FriendReachableField* prepareSharedEnemyReachable(bool& forceRebuild, bool allowReset = true);
	/// Exact legacy discoverThreat calculation for one base candidate. The
	/// shared and fallback paths both call this helper so feature-off behavior
	/// and footprint/LOF semantics cannot drift.
	float calculateDiscoverThreat(const Position& candidate, const std::map<Position, int, PositionComparator>& enemyReachable);
	/// Return the compatible per-faction threat memo, lazily initialize it, and
	/// conservatively re-stamp evaluated tiles after queued knowledge updates.
	/// Returns nullptr for feature-off, invalid cache, or actor-profile mismatch.
	ThreatField* prepareSharedThreatField(const std::map<Position, int, PositionComparator>& enemyReachable);
	/// Phase 43.1I (Calypso): returns the live shared negative terrain-LOF cache
	/// for this unit's faction when the mod enables ai.sharedFields and a valid
	/// faction turn-cache exists, flushing it lazily (dirty -> clear + mark
	/// clean) before returning the live cache; returns nullptr otherwise so
	/// callers fall back to the original uncached trace. Mirrors the lazy-rebuild
	/// policy of prepareSharedFriendReachable but is a pure read/remember surface
	/// that never recomputes terrain state.
	TerrainLofNegativeCache* prepareSharedTerrainLofCache();
public:
	void beginActivation();
	void recordFailedAttempt(const BattleAction& action);
	/// Read-only view of this unit's AI failure memory (Phase 43 one-retry fix).
	/// Used by BattlescapeGame to decide whether an eligible candidate failure is the
	/// FIRST at the current world revision (before recordFailedAttempt is called).
	const AIFailureMemory& getFailureMemory() const { return _failureMemory; }
	void emitAIAudit(const BattleAction& action) const;
	bool medikit_think(BattleMediKitType healOrStim);
public:
	/// Creates a new AIModule linked to the game and a certain unit.
	AIModule(SavedBattleGame *save, BattleUnit *unit, Node *node);
	/// Cleans up the AIModule.
	~AIModule();
	/// Sets the target faction.
	void setTargetFaction(UnitFaction f);
	/// Resets the unsaved AI state.
	void reset();
	/// Loads the AI Module from YAML.
	void load(const YAML::YamlNodeReader& reader);
	/// Saves the AI Module to YAML.
	void save(YAML::YamlNodeWriter writer) const;
	/// Runs Module functionality every AI cycle.
	void think(BattleAction *action);
	/// Sets the "unit was hit" flag true.
	void setWasHitBy(BattleUnit *attacker);
	/// Sets the "unit picked up a weapon" flag.
	void setWeaponPickedUp();
	/// Gets whether the unit was hit.
	bool getWasHitBy(int attacker) const;
	/// Set start node.
	void setStartNode(Node *node) { _fromNode = node; }
	/// setup a patrol objective.
	void setupPatrol();
	/// setup an ambush objective.
	void setupAmbush();
	/// setup a combat objective.
	void setupAttack();
	/// setup an escape objective.
	void setupEscape();
	/// count how many xcom/civilian units are known to this unit.
	int countKnownTargets() const;
	/// count how many known XCom units are able to see this unit.
	int getSpottingUnits(const Position& pos) const;
	/// Selects the nearest target we can see, and return the number of viable targets.
	int selectNearestTarget();
	/// Selects the closest known xcom unit for ambushing.
	bool selectClosestKnownEnemy();
	/// Selects a random known target.
	bool selectRandomTarget();
	/// Selects the nearest reachable point relative to a target.
	bool selectPointNearTarget(BattleUnit *target, int maxTUs);
	/// Selects a target from a list of units seen by spotter units for out-of-LOS actions
	bool selectSpottedUnitForSniper();
	/// Scores a firing mode action based on distance to target and accuracy.
	int scoreFiringMode(BattleAction *action, BattleUnit *target, bool checkLOF);
	/// re-evaluate our situation, and make a decision from our available options.
	void evaluateAIMode();
	/// Selects a suitable position from which to attack.
	bool findFirePoint();
	/// Decides if we should throw a grenade/launch a missile to this position.
	int explosiveEfficacy(Position targetPos, BattleUnit *attackingUnit, int radius, int diff, bool grenade = false) const;
	bool getNodeOfBestEfficacy(BattleAction *action, int radius);
	/// Attempts to take a melee attack/charge an enemy we can see.
	void meleeAction();
	/// Attempts to fire a waypoint projectile at an enemy we, or one of our teammates sees.
	void wayPointAction();
	/// Attempts to fire at an enemy spotted for us.
	bool sniperAction();
	/// Attempts to fire at an enemy we can see.
	void projectileAction();
	/// Chooses a firing mode for the AI based on expected number of hits per turn
	void extendedFireModeChoice(BattleActionCost& costAuto, BattleActionCost& costSnap, BattleActionCost& costAimed, BattleActionCost& costThrow, bool checkLOF = false);
	/// Attempts to throw a grenade at an enemy (or group of enemies) we can see.
	void grenadeAction();
	/// Performs a psionic attack.
	bool psiAction();
	/// Performs a melee attack action.
	void meleeAttack();

	/// How much given unit is worth as target of attack.
	AIAttackWeight getTargetAttackWeight(BattleUnit* target) const;
	/// Checks to make sure a target is valid, given the parameters
	bool validTarget(BattleUnit* target, bool assessDanger, bool includeCivs) const;

	/// Checks the alien's TU reservation setting.
	BattleActionType getReserveMode();
	/// Assuming we have both a ranged and a melee weapon, we have to select one.
	void selectMeleeOrRanged();
	/// Gets the current targetted unit.
	BattleUnit* getTarget();
	/// Frees up the destination node for another Unit to select
	void freePatrolTarget();

	/// Everything below belongs to Brutal-AI (adapted from Brutal-OXCE by Xilmi, github.com/Xilmi/OpenXcom)
	/// Checks whether anyone on our team can see the target
	bool visibleToAnyFriend(BattleUnit *target) const;
	/// Handles behavior of brutalAI
	void brutalThink(BattleAction *action);
	/// Like selectSpottedUnitForSniper but works for everyone
	bool brutalSelectSpottedUnitForSniper();
	/// look up in _allPathFindingNodes how many time-units we need to get to a specific position
	int tuCostToReachPosition(Position pos, const std::vector<PathfindingNode *>& nodeVector, BattleUnit* actor = NULL, bool forceExactPosition = false, bool energyInsteadOfTU = false);
	/// find the cloest Position to our target we can reach while reserving for a BattleAction
	Position furthestToGoTowards(Position target, BattleActionCost reserve, const std::vector<PathfindingNode *>& nodeVector, bool encircleTileMode = false, Tile *encircleTile = NULL);
	/// find the closest Position that isn't our current position which is on the way to a target
	Position closestToGoTowards(Position target, const std::vector<PathfindingNode *>& nodeVector, Position myPos, bool peakMode = false);
	/// checks if the path to a position is save
	bool isPathToPositionSave(Position target, bool &saveForProxies);
	/// Performs a psionic attack but allow multiple per turn and take success-chance into consideration
	bool brutalPsiAction();
	/// Chooses a firing mode for the AI based on expected damage dealt
	float brutalExtendedFireModeChoice(BattleActionCost &costAuto, BattleActionCost &costSnap, BattleActionCost &costAimed, BattleActionCost &costThrow, BattleActionCost &costHit, bool checkLOF = false, float previousHighScore = 0, float *evaluatedBestScore = nullptr, BattleActionType *evaluatedBestAction = nullptr, std::array<float, 3> *evaluatedTerms = nullptr);
	/// Scores a firing mode action based on distance to target, accuracy and overall Damage dealt, also supports melee-hits
	float brutalScoreFiringMode(BattleAction *action, BattleUnit *target, bool checkLOF, bool reactionCheck = false);
	/// Phase 34.7 (Calypso): the suppression value of one auto-volley from `weapon` -- the
	/// pinning payoff of volume fire, independent of direct hit chance. Returns 0 when the
	/// ai.suppression flag is off, when the weapon has no auto-fire config, or when the
	/// loaded ammo is too scarce to spare on suppression (no margin: needs >= 2 full volleys
	/// worth). Additive-only: callers add this to an auto-shot's base score so volume fire is
	/// preferred over holding fire when direct hit chance is poor but a target is exposed.
	float suppressionVolleyValue(BattleItem* weapon) const;
	/// Phase 34.9 (Calypso): record this hostile unit's declared squad intent from its finalized
	/// action, on the faction blackboard (attack types -> ATTACK on _aggroTarget, BA_WALK toward a
	/// known enemy -> FLANK, desperate escape -> RETREAT). Gated on ai.squadCoordination and
	/// FACTION_HOSTILE; a no-op otherwise. Called at the tail of both the legacy and brutal paths.
	void declareSquadIntentFromAction(const BattleAction* action) const;
	/// Used as multiplier for the throw-action in brutalScoreFiringMode
	float brutalExplosiveEfficacy(Position targetPos, BattleUnit *attackingUnit, int radius, bool grenade = false, bool validOnly = false) const;
	/// An inaccurate simplified check for line of fire from a specific position to a specific target
	bool quickLineOfFire(Position pos, BattleUnit *target, bool beOkayWithFriendOfTarget = false, bool lastLocationMode = false, bool fleeMode = false);
	/// checks whether there is clear sight between two tile-positions
	bool clearSight(Position pos, Position target);
	/// how many time-units would it take to turn to a specific target
	int getTurnCostTowards(Position target, Position from);
	/// overload without from
	int getTurnCostTowards(Position target);
	/// Using weapons like the blaster but actually hitting what we want while avoiding to mow down our allies
	void brutalBlaster();
	/// Attempts to throw a grenade at tiles near potential targets when target itself couldn't be hit
	void brutalGrenadeAction();
	/// Tells the AI of the unit whether it wants to end the turn or not
	void setWantToEndTurn(bool wantToEndTurn);
	/// Asks the unit's AI whether it wants to end the turn or not
	bool getWantToEndTurn();
	/// Attack tiles where units have been seen before but we are not sure where they are
	void blindFire();
	/// Validating the shot of an arcing weapon is way more compliacated than for a throw, that's why there's a separate method
	bool validateArcingShot(BattleAction *action, Tile* originTile = NULL);
	/// check if a unit is targetable according to aiTargetMode
	bool brutalValidTarget(BattleUnit *unit, bool moveMode = false, bool psiMode = false) const;
	/// check the path to an enemy and then subtracts their movement from the cost
	Position closestPositionEnemyCouldReach(BattleUnit *enemy);
	/// returns how far a unit can shoot while extender-accuracy is enabled with the given amount of time-units left
	int maxExtenderRangeWith(BattleUnit *unit, int tus);
	/// Determines a new tile where to look for an enemy who's position is unknown
	int getNewTileIDToLookForEnemy(Position previousPosition, BattleUnit *unit);
	/// Calculates how much TU this unit can have at most considering it's carrying capacity and leg-damage
	int getMaxTU(BattleUnit *unit);
	/// Get the ID of the closest tile which is an entry-point for the player
	int getClosestSpawnTileId();
	/// Tells us whether a unit is an enemy
	bool isEnemy(BattleUnit* unit, bool ignoreSameOriginalFaction = false) const;
	/// Tells us whether a unit is an ally
	bool isAlly(BattleUnit *unit) const;
	/// Checks whether the trajectory of a projectile visits tiles occupied by our allies
	bool projectileMayHarmFriends(Position startPos, Position targetPos);
	/// Checks whether at least one of our allies is in range for a good attack
	bool inRangeOfAnyFriend(Position pos);
	/// Checks whether we should avoid melee-range against a specific enemy
	bool shouldAvoidMeleeRange(BattleUnit *enemy);
	/// Checks whether a unit has any means to fight
	bool isArmed(BattleUnit *unit) const;
	/// Checks whether there's a grenade on the ground and tries to pick it up
	void tryToPickUpGrenade(Tile* tile, BattleAction* action);
	/// returns a score for how much we like to pick up a specific kind of item
	float getItemPickUpScore(BattleItem *item);
	/// Non-cheating-AI needs to be able to determine whether the enemy is doing Triton-shenanigans, where we should prevent exposing ourselves or is exposed enough themselves for us to strike
	bool IsEnemyExposedEnough();
	/// Get the cover-value of a tile
	float getCoverValue(Tile *tile, BattleUnit *bu, int coverQuality = 1);
	/// checks whethere there's any cover in range
	float highestCoverInRange(const std::vector<PathfindingNode *> nodeVector);
	/// runs a very minimalist pathfinding just to see whether the unit could move
	bool isAnyMovementPossible();
	/// returns how much energy the unit can recover each turn
	int getEnergyRecovery(BattleUnit* unit);
	/// returns reachable tile-Ids by a particular unit
	const std::map<Position, int, PositionComparator>& getReachableBy(BattleUnit* unit, bool& ranOutOfTUs, bool forceRecalc = false, bool useMaxTUs = false, bool pruneAirTiles = false);
	/// checks whether it would be possible to see one tile from another
	bool hasTileSight(Position from, Position to);
	/// returns the amount of blaster-waypoints to reach a target-positon
	int requiredWayPointCount(Position to, const std::vector<PathfindingNode*> nodeVector);
	/// returns a vector of all positions we'd have to walk towards a specific location
	/// (writes into caller-supplied `out`, which it clears first)
	void getPositionsOnPathTo(Position target, const std::vector<PathfindingNode*>& nodeVector, std::vector<Position>& out);
	/// returns fear of smoke
	std::map<Position, int, PositionComparator> getSmokeFearMap();
	/// returns how urgent it is to get rid of a grenade
	float grenadeRiddingUrgency();
	/// returns which side of the unit is facing the given position
	UnitSide getSideFacingToPosition(BattleUnit* unit, Position pos);
	/// returns whether the unit wants to run
	bool wantToRun();
	/// Pointer to save so that unit can access it
	SavedBattleGame* getSave() { return _save; };
	/// Determine a good position for indirect peeking
	Position getPeakPosition(bool oneStep = false);
	/// Gives an estimate of a unit's power-level
	float getUnitPower(BattleUnit* unit);
	/// returns a vector of Tiles next to doors
	std::vector<Tile*> getCorpseTiles(const std::vector<PathfindingNode*> nodeVector);
	/// tries to pick up weapon and ammo from current tile if it's an upgrade
	bool improveItemization(float currentItemScore, BattleAction* action);
	/// scores a set of tiles based on how long ago they were seen
	int scoreVisibleTiles(const std::set<Tile*>& tileSet);
	/// prepares a grenade-action to use with validateArcingShot
	BattleAction* grenadeThrowAction(Position pos);
	/// how much damage we can inflict to a given enemy
	float damagePotential(Position pos, BattleUnit* target, int tuTotal, int energyTotal);
	/// checks if a position is visible to the enemy
	bool isPositionVisibleToEnemy(Position pos, bool tileLOSMode = false);
};

struct MoveEvaluation
{
	int remainingTU;
	int remainingEnergy;
	int lastStepCost;
	float walkToDist;
	float attackPotential;
	bool IsDirectPeak;
	int visibleTiles;
	int bestDirection;
};

}
