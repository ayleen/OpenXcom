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
#include <chrono>
#include <climits>
#include <algorithm>
#include <utility>
#include "AIModule.h"
#include "AICandidateOrder.h"
#include "AIEvaluationBudget.h"
#include "../Mod/AITuning.h"
#include "../Savegame/BattleItem.h"
#include "../Savegame/Node.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/SavedGame.h"
#include "TileEngine.h"
#include "BattlescapeState.h"
#include "../Savegame/Tile.h"
#include "Pathfinding.h"
#include "../Engine/RNG.h"
#include "../Engine/Logger.h"
#include "../Engine/Game.h"
#include "../Mod/Armor.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleSoldierBonus.h" // Brutal-AI: getEnergyRecovery
#include "../fmath.h"

namespace OpenXcom
{

// Phase 34.5 (Brutal-AI, adapted from Brutal-OXCE by Xilmi): small tolerance for float compares.
static const double EPSILON = 0.00001;

namespace {
// Phase 43.0: zero-allocation RAII scope timer for AIModule::brutalThink. Constructed on entry,
// destroyed on every scope exit (including the many early returns). Logs elapsed microseconds via
// steady_clock when Options::traceAI is on; produces no log and mutates no state otherwise.
struct AITimingScope
{
	bool _enabled;
	bool _measuring;
	int _unitId;
	std::chrono::steady_clock::time_point _start;
	explicit AITimingScope(int unitId, bool measure = false) :
		_enabled(Options::traceAI), _measuring(_enabled || measure), _unitId(unitId),
		_start(_measuring ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point()) {}
	int elapsedMs() const
	{
		if (!_measuring)
			return 0;
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - _start).count();
		return elapsed > INT_MAX ? INT_MAX : static_cast<int>(elapsed);
	}
	~AITimingScope()
	{
		if (!_enabled)
			return;
		auto end = std::chrono::steady_clock::now();
		auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - _start).count();
		Log(LOG_INFO) << "AI_TIMING brutalThink_us=" << us << " unit=" << _unitId;
	}
	AITimingScope(const AITimingScope&) = delete;
	AITimingScope& operator=(const AITimingScope&) = delete;
};
}

/**
 * Sets up a BattleAIState.
 * @param save Pointer to the battle game.
 * @param unit Pointer to the unit.
 * @param node Pointer to the node the unit originates from.
 */
AIModule::AIModule(SavedBattleGame *save, BattleUnit *unit, Node *node) :
	_save(save), _unit(unit), _aggroTarget(0), _knownEnemies(0), _visibleEnemies(0), _spottingEnemies(0),
	_escapeTUs(0), _ambushTUs(0), _weaponPickedUp(false), _rifle(false), _melee(false), _blaster(false), _grenade(false),
	_didPsi(false), _AIMode(AI_PATROL), _closestDist(100), _fromNode(node), _toNode(0), _foundBaseModuleToDestroy(false)
{
	_traceAI = Options::traceAI;

	_reserve = BA_NONE;
	_intelligence = _unit->getIntelligence();
	_escapeAction = BattleAction();
	_ambushAction = BattleAction();
	_attackAction = BattleAction();
	_patrolAction = BattleAction();
	_psiAction = BattleAction();
	_targetFaction = FACTION_PLAYER;
	if (_unit->getOriginalFaction() == FACTION_NEUTRAL)
	{
		_targetFaction = FACTION_HOSTILE;
	}
}

/**
 * Deletes the BattleAIState.
 */
AIModule::~AIModule()
{

}

void AIModule::beginActivation()
{
	_failureMemory.clear();
	_auditReason.clear();
	_auditRunnerUp.clear();
	_auditBestScore = 0.0f;
	_auditRunnerUpScore = 0.0f;
	_auditBestTerms = {{0, 0, 0}};
	_auditTermLabels = {{"damage", "hit", "context"}};
}

void AIModule::recordFailedAttempt(const BattleAction& action)
{
	if (action.aiFailure == AIFailureReason::NONE || action.type == BA_RETHINK) return;
	AIFailedAttempt failed;
	failed.action = static_cast<int>(action.type);
	failed.targetId = action.aiTargetId;
	failed.position = action.aiAttemptPosition;
	failed.reason = action.aiFailure;
	failed.worldRevision = _save->getBattleGame()->getAIWorldRevision();
	if (_save->getMod()->getAIFailureMemory()) _failureMemory.record(failed);
	if (_traceAI)
	{
		Log(LOG_INFO) << "AI_FAILURE unit=" << _unit->getId() << " action=" << failed.action
			<< " target=" << failed.targetId << " pos=" << failed.position
			<< " reason=" << static_cast<int>(failed.reason) << " revision=" << failed.worldRevision;
	}
}

bool AIModule::candidateAllowed(BattleActionType type, int targetId, const Position& position) const
{
	return !_save->getMod()->getAIFailureMemory()
		|| _failureMemory.allows(static_cast<int>(type), targetId, position,
			_save->getBattleGame()->getAIWorldRevision());
}

void AIModule::prepareAIAudit(BattleAction *action)
{
	action->aiAttemptPosition = action->target;
}

void AIModule::emitAIAudit(const BattleAction& action) const
{
	if (!_traceAI || action.type == BA_RETHINK) return;
	const char *reason = _auditReason.empty()
		? (action.type == BA_WALK ? "move-best-position" : action.type == BA_NONE ? "no-valid-candidate" : "best-valid-action")
		: _auditReason.c_str();
	Log(LOG_INFO) << "AI_DECISION unit=" << _unit->getId() << " reason=" << reason
		<< " action=" << static_cast<int>(action.type) << " target=" << action.aiTargetId
		<< " pos=" << action.target << " score=" << _auditBestScore
		<< " runnerUp=" << (_auditRunnerUp.empty() ? "none" : _auditRunnerUp)
		<< " runnerScore=" << _auditRunnerUpScore
		<< " terms=" << _auditTermLabels[0] << ":" << _auditBestTerms[0]
		<< "," << _auditTermLabels[1] << ":" << _auditBestTerms[1]
		<< "," << _auditTermLabels[2] << ":" << _auditBestTerms[2]
		<< " decisive=score_delta:" << (_auditBestScore - _auditRunnerUpScore);
}

/**
 * Sets the target faction.
 */
void AIModule::setTargetFaction(UnitFaction f)
{
	_targetFaction = f;
}

/**
 * Phase 32 (Calypso): an organic civilian (FACTION_NEUTRAL) gets the smarter-civilian
 * behaviors only when the active mod opts in via `ai: { smartCivilians: true }`. This
 * keeps native OXCE behavior byte-identical when the flag is off and never touches aliens
 * or mind-controlled units (which differ in current vs. original faction).
 */
bool AIModule::isSmartCivilian() const
{
	return _unit->isOrganicCivilian()
		&& _save->getMod()->getAISmartCivilians();
}

/**
 * Phase 32 (Calypso): nearest "protector" to head toward — the closest X-Com soldier the
 * civilians have spotted recently, else the closest other living civilian. Shared by
 * findCivilianSafetyTarget (flee) and findGuardObjective (regroup). Distance is HORIZONTAL
 * (distance2d), matching the 2D escape/objective scoring that consumes `out`, so a soldier a
 * floor above is not mistaken for "nearer" than one on the unit's own level. Returns false
 * (leaving `out` untouched) when neither a spotted soldier nor another civilian is known.
 */
bool AIModule::findNearestProtector(Position& out) const
{
	const Position self = _unit->getPosition();
	BattleUnit* bestSoldier = nullptr;
	BattleUnit* bestCivilian = nullptr;
	int soldierDist = INT_MAX;
	int civilianDist = INT_MAX;
	for (auto* bu : *_save->getUnits())
	{
		if (bu == _unit || bu->isOut())
		{
			continue;
		}
		if (bu->getFaction() == FACTION_PLAYER && bu->getOriginalFaction() == FACTION_PLAYER)
		{
			// only head toward soldiers the civilians have actually spotted recently, so they
			// don't path omnisciently toward an unseen squad across the whole map.
			if (bu->getTurnsSinceSpottedByFaction(FACTION_NEUTRAL) > _intelligence)
			{
				continue;
			}
			int d = Position::distance2d(self, bu->getPosition());
			if (d < soldierDist) { soldierDist = d; bestSoldier = bu; }
		}
		else if (bu->getFaction() == FACTION_NEUTRAL)
		{
			int d = Position::distance2d(self, bu->getPosition());
			if (d < civilianDist) { civilianDist = d; bestCivilian = bu; }
		}
	}
	if (bestSoldier)  { out = bestSoldier->getPosition();  return true; }
	if (bestCivilian) { out = bestCivilian->getPosition(); return true; }
	return false;
}

/**
 * Phase 32 (Calypso): where should a frightened civilian run? Priority:
 *   1. toward the nearest X-Com soldier it has seen recently (within its intelligence
 *      memory) — seek protection;
 *   2. else toward the nearest other living civilian — herd together;
 *   3. else toward the closest map edge — try to leave the terror site.
 * Returns false only if the unit is somehow off-map.
 */
bool AIModule::findCivilianSafetyTarget(Position& out) const
{
	const Position self = _unit->getPosition();
	if (!_save->getTile(self))
	{
		return false;
	}

	if (findNearestProtector(out))
	{
		return true;
	}

	// no allies known: head for the nearest map edge on the unit's level.
	const int sx = _save->getMapSizeX();
	const int sy = _save->getMapSizeY();
	const int toWest = self.x, toEast = sx - 1 - self.x;
	const int toNorth = self.y, toSouth = sy - 1 - self.y;
	const int m = std::min(std::min(toWest, toEast), std::min(toNorth, toSouth));
	out = self;
	if (m == toWest) out.x = 0;
	else if (m == toEast) out.x = sx - 1;
	else if (m == toNorth) out.y = 0;
	else out.y = sy - 1;
	return true;
}

/**
 * Phase 32 (Calypso): an armed civilian guard — a smart civilian whose unit ruleset sets
 * `civilianGuard: true`. Guards carry a built-in weapon and protect other civilians instead
 * of fleeing. Stable (ruleset-derived) so it needs no save/load state of its own.
 */
bool AIModule::isCivilianGuard() const
{
	return isSmartCivilian()
		&& _unit->getUnitRules()
		&& _unit->getUnitRules()->isCivilianGuard();
}

/**
 * Phase 34.4 (Calypso): should this alien bias its patrol toward the civilian-hunt zone
 * instead of a random/highest-value node? Only a genuine alien (both current and original
 * faction HOSTILE -- excludes an MC'd civilian, which should keep behaving like a civilian)
 * with no known enemy (an engaged alien fights; this is purely for the "nothing to do yet"
 * case), gated on the mod flag. The actual civilian-alive check happens in
 * SavedBattleGame::getCivilianHuntZone (false when none are alive).
 */
bool AIModule::wantsToHuntCivilians() const
{
	return _save->getMod()->getAITerrorHuntCivilians()
		&& _unit->getFaction() == FACTION_HOSTILE
		&& _unit->getOriginalFaction() == FACTION_HOSTILE
		&& _knownEnemies == 0;
}

/**
 * Phase 34.8 (Calypso): should this alien bias its patrol toward the newest in-range
 * noise zone instead of a random/highest-value node? Same hostile / no-known-enemy gates
 * as wantsToHuntCivilians (34.4), gated on the mod's ai.hearing flag instead. With the
 * flag off this returns false and every code path it gates stays unreachable, so vanilla
 * OXCE behavior (native AND WASM) is byte-for-byte preserved.
 */
bool AIModule::wantsToInvestigateNoise() const
{
	return _save->getMod()->getAIHearing()
		&& _unit->getFaction() == FACTION_HOSTILE
		&& _unit->getOriginalFaction() == FACTION_HOSTILE
		&& _knownEnemies == 0;
}

/**
 * Phase 34.6 (Calypso): terrain-tactics candidate-attack generator.
 *
 * Scans for one of two terrain attacks against fair-channel-known enemies and, if a viable
 * candidate is found, fills `_attackAction` so the surrounding decision loop dispatches it.
 *
 *   Floor drop -- an enemy known via fair channels (`getTurnsSinceSpottedByFaction <=
 *   _intelligence`, NEVER `_cheating`, NEVER direct map reads) is standing on a destructible
 *   O_FLOOR (`Tile::getMapData(O_FLOOR)` non-null and its armor beatable by the unit's loaded
 *   ammo: `power * ToTile >= armor`, the same `power >= armor` convention used by
 *   `Tile::damage`), and the floor is targetable from the unit's current position
 *   (`TileEngine::canTargetTile`, the precedent at `getNodeOfBestEfficacy`). Score = expected
 *   fall depth (levels the enemy would drop) so deeper drops are preferred; firing happens via
 *   the existing tile-targeting path (no new projectile type).
 *
 *   Wall breach -- only when pathfinding to the nearest known enemy fails or detours > breachDetourMultiplier x
 *   the straight-line distance AND the unit carries a grenade (`_grenade`) whose
 *   `explosiveEfficacy` at the wall position is positive (cleared against friendlies): the unit
 *   throws at the blocking O_OBJECT/wall on the straight line. One breach attempt per unit per
 *   3 turns (transient `_lastBreachTurn`, NOT saved to file).
 *
 * Mission-objective safety: any tile whose `MapData::isBaseModule()` returns true is skipped --
 * we never sabotage objectives outside the existing `destroyBaseFacilities` path (pitfall note
 * in docs/phases/phase-34-advanced-battle-ai.md).
 *
 * With the mod flag off, every code path here is unreachable: the function returns false
 * immediately without touching any state, so vanilla OXCE behavior is byte-for-byte preserved.
 */
bool AIModule::considerTerrainAttack()
{
	// Gate 1: byte-identical off-path.
	if (!_save->getMod()->getAITerrainTactics()) return false;
	// Gate 2: only the hostile AI uses terrain tactics. Civilians stay on the Phase 32 logic;
	// mind-controlled soldiers (current faction != original) are out of scope -- they may be
	// player-controlled again next turn and shouldn't blow up the player's buildings.
	if (_unit->getFaction() != FACTION_HOSTILE || _unit->getOriginalFaction() != FACTION_HOSTILE) return false;
	// Gate 3: don't override an already-chosen attack. Terrain attacks are fallback candidates.
	if (_attackAction.type != BA_RETHINK) return false;

	const Position myPos = _unit->getPosition();

	// === Floor drop ===
	// Pick the highest-scoring fair-known enemy whose standing tile has a destructible floor
	// targetable from our current position. Score = expected fall depth (levels), so a 2-storey
	// drop beats a 1-storey drop; ties broken by horizontal proximity.
	//
	// Section-local gates: floor drop fires a direct shot from the main-hand weapon, so the
	// weapon / ammo / ToTile / terrainPower checks live HERE -- not at the top of the function.
	// A unit carrying only a grenade (no serviceable main-hand weapon for terrain damage) must
	// still be allowed to reach the wall-breach section below.
	BattleItem *weapon = _unit->getMainHandWeapon();
	int terrainPower = 0;
	bool floorDropViable = false;
	if (weapon && _save->canUseWeapon(weapon, _unit, false, BA_SNAPSHOT))
	{
		// Phase 43 (C2 trigger): the floor drop fires a snap shot -- if the unit can't afford it,
		// the candidate would be re-picked verbatim every think with zero TU spent (infinite loop).
		BattleActionCost snapCost(BA_SNAPSHOT, _unit, weapon);
		const BattleItem *ammo = weapon->getAmmoForAction(BA_SNAPSHOT);
		if (ammo && snapCost.haveTU())
		{
			const RuleItem *ammoRule = ammo->getRules();
			const RuleDamageType *dt = ammoRule->getDamageType();
			if (dt && dt->ToTile > 0.0f)
			{
				// Effective terrain damage from a single hit: `power * ToTile` (the multiplier
				// applied by TileEngine at the `ToTile > 0` gate). The destruction predicate is
				// `power >= armor` (Tile::damage / TileEngine::detonate); armor 255 is the
				// indestructible sentinel.
				terrainPower = (int)(ammoRule->getPower() * dt->ToTile);
				if (terrainPower >= 1) floorDropViable = true;
			}
		}
	}

	if (floorDropViable)
	{
		// Phase 43 (C2 trigger, bug B): validate LOF from the weapon MUZZLE, not the eye. The muzzle
		// voxel depends on the aim direction, so it is recomputed per candidate below.
		BattleAction originAction;
		originAction.actor = _unit;
		originAction.weapon = weapon;
		originAction.type = BA_SNAPSHOT;
		int bestScore = 0;
		Position bestTarget = Position(0, 0, 0);
		int bestTargetId = -1;
		BattleItem *bestWeapon = nullptr;
		for (auto* target : *_save->getUnits())
		{
			if (!target || target == _unit || target->isOut()) continue;
			// Fair-knowledge gate -- never `_cheating`, never a direct map read on the live position.
			// isEnemy() is the brutal-AI helper; for legacy dispatch the same lookup-by-faction works.
			if (!isEnemy(target)) continue;
			if (target->getTurnsSinceSpottedByFaction(_unit->getFaction()) > _intelligence) continue;
			// The FAIR known position is the last-spotted tile (Brutal-AI knowledge layer); fall back
			// to the live tile only when the unit is genuinely visible right now (visibleToAnyFriend
			// already encodes the fairness stance -- no omniscience).
			Position enemyPos;
			if (visibleToAnyFriend(target))
			{
				enemyPos = target->getPosition();
			}
			else
			{
				int spottedIdx = target->getTileLastSpotted(_unit->getFaction());
				if (spottedIdx < 0) continue;
				enemyPos = _save->getTileCoords(spottedIdx);
			}
			Tile *enemyTile = _save->getTile(enemyPos);
			if (!enemyTile) continue;
			if (!candidateAllowed(BA_SNAPSHOT, target->getId(), enemyPos)) continue;
			MapData *floorMD = enemyTile->getMapData(O_FLOOR);
			if (!floorMD) continue;
			// Mission-objective safety: never drop a floor flagged as a base module.
			if (floorMD->isBaseModule()) continue;
			// Destructibility predicate (Tile::damage convention: power >= armor; 255 = indestructible).
			if (floorMD->getArmor() >= 255) continue;
			if (terrainPower < floorMD->getArmor()) continue;
			// The floor must be targetable from the unit's weapon muzzle (LOF check).
			originAction.target = enemyPos;
			Position originVoxel = _save->getTileEngine()->getOriginVoxel(originAction, 0);
			Position targetVoxel;
			if (!_save->getTileEngine()->canTargetTile(&originVoxel, enemyTile, O_FLOOR, &targetVoxel, _unit, false))
				continue;
			// Score: expected fall depth. Search downward for the first tile that would catch the
			// enemy; the drop is bounded by the map bottom (z=0) and capped at 4 levels for scoring.
			int fallLevels = 0;
			for (int z = enemyPos.z - 1; z >= 0 && fallLevels < 4; --z)
			{
				Tile *below = _save->getTile(Position(enemyPos.x, enemyPos.y, z));
				if (!below) break;
				++fallLevels;
				if (!below->hasNoFloor(_save)) break; // a floor here catches the enemy
			}
			if (fallLevels < 1) continue; // no drop happens (e.g. ground-level tile) -- skip
			// Base score so any viable drop beats "do nothing"; deeper drops score higher; closer
			// enemies score slightly higher (tie-breaker). Same scale as AIW_SCALE so it slots into
			// the existing scoring cascade without distorting it.
			int score = AIW_SCALE + fallLevels * (AIW_SCALE / 2);
			int horizDist = Position::distance2d(myPos, enemyPos);
			score -= horizDist; // mild proximity bias
			if (score > bestScore)
			{
				bestScore = score;
				bestTarget = enemyPos;
				bestTargetId = target->getId();
				bestWeapon = weapon;
			}
		}

		if (bestScore > 0 && bestWeapon)
		{
			_attackAction.type = BA_SNAPSHOT;
			_attackAction.target = bestTarget;
			_attackAction.weapon = bestWeapon;
			_attackAction.actor = _unit;
			_attackAction.aiTargetId = bestTargetId;
			_attackAction.aiHasFilteredFallback = true;
			_attackAction.updateTU();
			if (_traceAI)
			{
				Log(LOG_INFO) << "Phase 34.6: terrain floor-drop attack at " << bestTarget;
			}
			return true;
		}
	}

	// === Wall breach ===
	// Triggered only when pathing to the nearest fair-known enemy fails or severely detours,
	// the unit carries a grenade ready in the belt, the breach cooldown allows another attempt
	// this turn (one per 3 turns per unit, transient -- NOT saved), and a destructible
	// O_OBJECT / wall sits on the straight line between the unit and the objective. The
	// friendly-fire check reuses explosiveEfficacy (the same convention grenadeAction uses) so
	// an alien never collapses a wall onto its own squad.
	//
	// Independent of the main-hand weapon: the breach is a grenade THROW, so a unit carrying
	// only a grenade (or whose main-hand weapon can't damage terrain) can still breach.
	if (_grenade && (_save->getTurn() - _lastBreachTurn) >= 3)
	{
		BattleItem *grenade = _unit->getGrenadeFromBelt(_save);
		if (grenade)
		{
			// Find the nearest fair-known enemy as the breach objective (same fair-knowledge
			// gates as the floor-drop scan -- visibleToAnyFriend OR getTileLastSpotted).
			Position objectivePos;
			bool haveObjective = false;
			int objectiveTargetId = -1;
			int bestObjDist = INT_MAX;
			for (auto* target : *_save->getUnits())
			{
				if (!target || target == _unit || target->isOut()) continue;
				if (!isEnemy(target)) continue;
				if (target->getTurnsSinceSpottedByFaction(_unit->getFaction()) > _intelligence) continue;
				Position enemyPos;
				if (visibleToAnyFriend(target))
				{
					enemyPos = target->getPosition();
				}
				else
				{
					int spottedIdx = target->getTileLastSpotted(_unit->getFaction());
					if (spottedIdx < 0) continue;
					enemyPos = _save->getTileCoords(spottedIdx);
				}
				int d = Position::distance2d(myPos, enemyPos);
				if (d < bestObjDist) { bestObjDist = d; objectivePos = enemyPos; objectiveTargetId = target->getId(); haveObjective = true; }
			}
			if (haveObjective)
			{
				// Detect a blocking wall via the legacy Pathfinding::calculate + getStartDirection
				// path (the same one setupPatrol uses). tuCostToReachPosition is Brutal-AI-only
				// and depends on _allPathFindingNodes which may not be populated at this insertion
				// point in brutalThink, so we use the engine primitive directly to stay safe in
				// both decision paths. getStartDirection() == -1 means unreachable (path failed).
				_save->getPathfinding()->calculate(_unit, objectivePos, BAM_NORMAL);
				int startDir = _save->getPathfinding()->getStartDirection();
				int pathTUs = _save->getPathfinding()->getTotalTUCost();
				_save->getPathfinding()->abortPath();
				int straightLine = Position::distance2d(myPos, objectivePos);
				bool pathFailed = (startDir == -1);
				// Detour heuristic: typical OXCE tile move cost is ~4 TU; treat a path costing
				// > breachDetourMultiplier x the straight-line expectation as a "blocked approach" worth breaching
				// (Phase 43.0 item 7: the multiplier is now mod-tunable, default 2).
				bool bigDetour = AITuning::isBigDetour(pathTUs, straightLine, _save->getMod()->getAIBreachDetourMultiplier());
				if (pathFailed || bigDetour)
				{
					// Walk the straight line tile-by-tile (2D Bresenham on the unit's z-level)
					// and target the first destructible O_OBJECT or wall blocking the advance.
					int dx = objectivePos.x - myPos.x;
					int dy = objectivePos.y - myPos.y;
					int steps = std::max(std::abs(dx), std::abs(dy));
					if (steps > 0)
					{
						// Cap the walk at the grenade's own throw range (RuleItem::getThrowRange,
						// default 200 = effectively unlimited on standard maps) -- this is a
						// THROW, not a main-hand-weapon shot, so the weapon's getMaxRange would
						// be the wrong cap. Bounded by the 32-tile perf cap below as a hard stop.
						int throwRange = grenade->getRules()->getThrowRange();
						int checkedSteps = 0;
						for (int s = 1; s <= steps; ++s)
						{
							Position p(myPos.x + (dx * s) / steps, myPos.y + (dy * s) / steps, myPos.z);
							if (Position::distance2d(myPos, p) > throwRange) break;
							if (!candidateAllowed(BA_THROW, objectiveTargetId, p)) continue;
							Tile *t = _save->getTile(p);
							if (!t) continue;
							// Try the O_OBJECT slot first (the typical "wall" on TFTD terrains
							// is an O_OBJECT big-wall), then the two wall slots.
							TilePart parts[3] = { O_OBJECT, O_WESTWALL, O_NORTHWALL };
							for (int pi = 0; pi < 3; ++pi)
							{
								MapData *md = t->getMapData(parts[pi]);
								if (!md) continue;
								if (md->isBaseModule()) continue; // mission-objective safety
								if (md->getArmor() >= 255) continue; // indestructible sentinel
								const RuleDamageType *gDt = grenade->getRules()->getDamageType();
								int grenadeTerrainPower = gDt ? (int)(grenade->getRules()->getPower() * gDt->ToTile) : 0;
								if (grenadeTerrainPower < md->getArmor()) continue;
								// Friendly-fire check: same convention as grenadeAction. The
								// radius comes from the grenade's own rules so the efficacy
								// math matches what would actually happen on impact.
								BattleAction probeAction;
								probeAction.type = BA_THROW;
								probeAction.actor = _unit;
								probeAction.weapon = grenade;
								int radius = grenade->getRules()->getExplosionRadius(
									BattleActionAttack::GetBeforeShoot(probeAction));
								if (explosiveEfficacy(p, _unit, radius, _attackAction.diff, true) <= 0) continue;
								// Found a viable breach target.
								_attackAction.type = BA_THROW;
								_attackAction.target = p;
								_attackAction.weapon = grenade;
								_attackAction.actor = _unit;
								_attackAction.aiTargetId = objectiveTargetId;
								_attackAction.aiHasFilteredFallback = true;
								_attackAction.updateTU();
								_attackAction.Time += 4; // grenade prime/swap cost, same as grenadeAction
								_attackAction += _unit->getActionTUs(BA_PRIME, grenade);
								_lastBreachTurn = _save->getTurn();
								if (_traceAI)
								{
									Log(LOG_INFO) << "Phase 34.6: terrain wall-breach attack at " << p;
								}
								return true;
							}
							if (++checkedSteps > 32) break; // perf cap -- 32 tiles max
						}
					}
				}
			}
		}
	}

	return false;
}

/**
 * Phase 32 (Calypso): the nearest civilian "crying for help" that a guard can hear. A civilian
 * is in distress when it is panicking/berserk, its morale has cracked, or a spotted alien is
 * menacing it. Limited to a hearing radius so a guard reacts to nearby screams, not the whole map.
 */
BattleUnit *AIModule::findDistressedCivilian() const
{
	const Position self = _unit->getPosition();
	const int HEAR_RADIUS = 12;   // how far a guard can "hear" a scream (2D tiles)
	const int THREAT_RADIUS = 6;  // an alien this close to a civilian = that civilian is in danger
	// Pre-collect the hostiles the civilians have spotted recently — once — so the per-civilian
	// distress check is O(N_civ × N_spottedHostile) instead of rescanning every hostile inside it.
	std::vector<Position> spottedHostiles;
	for (auto* foe : *_save->getUnits())
	{
		if (foe->isOut() || foe->getFaction() != FACTION_HOSTILE)
		{
			continue;
		}
		if (foe->getTurnsSinceSpottedByFaction(FACTION_NEUTRAL) > _intelligence)
		{
			continue; // the civilians haven't spotted this alien recently
		}
		spottedHostiles.push_back(foe->getPosition());
	}
	BattleUnit *best = nullptr;
	int bestDist = INT_MAX;
	for (auto* civ : *_save->getUnits())
	{
		if (civ == _unit || civ->isOut())
		{
			continue;
		}
		if (civ->getFaction() != FACTION_NEUTRAL || civ->getOriginalFaction() != FACTION_NEUTRAL)
		{
			continue;
		}
		int d = Position::distance2d(self, civ->getPosition());
		if (d > HEAR_RADIUS || d >= bestDist)
		{
			continue; // out of earshot or not closer than the best candidate so far
		}
		bool distressed = civ->getStatus() == STATUS_PANICKING
			|| civ->getStatus() == STATUS_BERSERK
			|| civ->getMorale() < 50;
		if (!distressed)
		{
			// not screaming yet, but is a spotted alien bearing down on it?
			for (const Position& foePos : spottedHostiles)
			{
				if (Position::distance2d(foePos, civ->getPosition()) <= THREAT_RADIUS)
				{
					distressed = true;
					break;
				}
			}
		}
		if (distressed)
		{
			bestDist = d;
			best = civ;
		}
	}
	return best;
}

/**
 * Phase 32 (Calypso): where a guard advances when no alien is in sight. Priority mirrors the
 * brief: (1) rescue the nearest screaming civilian; (2) all quiet — regroup with the nearest
 * aquanaut; (3) failing that, stay among the civilians it protects. Unlike a fleeing civilian,
 * a guard heads toward soldiers proactively (it is their ally, not a frightened bystander).
 */
bool AIModule::findGuardObjective(Position& out) const
{
	if (BattleUnit* victim = findDistressedCivilian())
	{
		out = victim->getPosition();
		return true;
	}
	// all quiet: regroup with the nearest spotted soldier, else stay among the civilians.
	return findNearestProtector(out);
}

/**
 * Phase 32 (Calypso): fill _patrolAction with a step toward the guard's objective. Picks the
 * reachable tile on the unit's level that gets it closest to the objective (skipping fire and
 * known-dangerous tiles). When no tile improves the position, it targets its own tile, which the
 * dispatch in think() collapses to BA_NONE (hold and wait). Returns false only when there is no
 * objective at all, so the caller can fall back to ordinary node patrol.
 */
bool AIModule::setupGuardMove()
{
	Position objective(0, 0, 0);
	if (!findGuardObjective(objective))
	{
		return false;
	}

	const Position self = _unit->getPosition();
	const int curToObjective = Position::distance2d(self, objective);

	int bestScore = 0; // staying put scores 0; only move when a tile is strictly closer
	Position bestTile = self;

	std::vector<Position> search = _save->getTileSearch();
	RNG::shuffle(search);

	for (const auto& off : search)
	{
		Position cand(self.x + off.x, self.y + off.y, self.z);
		if (cand == self)
		{
			continue;
		}
		Tile* tile = _save->getTile(cand);
		if (!tile)
		{
			continue;
		}
		if (std::find(_reachable.begin(), _reachable.end(), _save->getTileIndex(cand)) == _reachable.end())
		{
			continue; // not reachable within the unit's TUs this turn
		}
		int candToObjective = Position::distance2d(cand, objective);
		int score = (curToObjective - candToObjective) * 10;
		if (tile->getFire())
		{
			score -= 40;
		}
		if (tile->getDangerous())
		{
			score -= 100;
		}
		if (score > bestScore)
		{
			// Score by distance only — membership in _reachable already proves the tile is
			// reachable within the unit's TUs this turn, so no per-candidate pathfind is needed.
			bestScore = score;
			bestTile = cand;
		}
	}

	// Pathfind ONCE, to the chosen tile (mirrors the setupPatrol/setupAmbush pattern) — instead of
	// dozens of calculate()/abortPath() runs inside the 121-offset search above. If even the best
	// tile has no clean start direction, hold position this turn rather than walk nowhere.
	if (bestTile != self)
	{
		_save->getPathfinding()->calculate(_unit, bestTile, _patrolAction.getMoveType());
		bool walkable = _save->getPathfinding()->getStartDirection() != -1;
		_save->getPathfinding()->abortPath();
		if (!walkable)
		{
			bestTile = self;
		}
	}

	_patrolAction.actor = _unit;
	_patrolAction.type = BA_WALK;
	_patrolAction.target = bestTile;
	return true;
}

/**
 * Resets the unsaved AI state.
 */
void AIModule::reset()
{
	// these variables are not saved in save() and also not initiated in think()
	_escapeTUs = 0;
	_ambushTUs = 0;
	beginActivation();
}

/**
 * Loads the AI state from a YAML file.
 * @param node YAML node.
 */
void AIModule::load(const YAML::YamlNodeReader& reader)
{
	int fromNodeID = reader["fromNode"].readVal(-1);
	int toNodeID = reader["toNode"].readVal(-1);
	_AIMode = reader["AIMode"].readVal(AI_PATROL);
	reader.tryRead("wasHitBy", _wasHitBy);
	reader.tryRead("weaponPickedUp", _weaponPickedUp);
	reader.tryRead("targetFaction", _targetFaction);

	// TODO: Figure out why AI are sometimes left with junk nodes
	if (fromNodeID >= 0 && (size_t)fromNodeID < _save->getNodes()->size())
	{
		_fromNode = _save->getNodes()->at(fromNodeID);
	}
	if (toNodeID >= 0 && (size_t)toNodeID < _save->getNodes()->size())
	{
		_toNode = _save->getNodes()->at(toNodeID);
	}
}

/**
 * Saves the AI state to a YAML file.
 * @return YAML node.
 */
void AIModule::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.setFlowStyle();
	writer.write("fromNode", _fromNode ? _fromNode->getID() : -1);
	writer.write("toNode", _toNode ? _toNode->getID() : -1);
	writer.write("AIMode", _AIMode);
	writer.write("wasHitBy", _wasHitBy);
	if (_weaponPickedUp)
		writer.write("weaponPickedUp", _weaponPickedUp);
	if (_unit->getOriginalFaction() == FACTION_HOSTILE && _unit->getFaction() == FACTION_NEUTRAL && _targetFaction == FACTION_HOSTILE)
	{
		writer.write("targetFaction", _targetFaction);
	}
}

/**
 * Mindless charge strategy. For mindless units.
 * Consists of running around and charging nearest visible enemy.
 * @param action (possible) AI action to execute after thinking is done.
 */
void AIModule::dont_think(BattleAction *action)
{
	_melee = false;
	action->weapon = _unit->getUtilityWeapon(BT_MELEE);

	if (_traceAI)
	{
		Log(LOG_INFO) << "LEEROY: Unit " << _unit->getId() << " of type " << _unit->getType() << " is Leeroy...";
	}
	if (action->weapon)
	{
		if (action->weapon->getRules()->getBattleType() == BT_MELEE)
		{
			if (_save->canUseWeapon(action->weapon, _unit, false, BA_HIT))
			{
				_melee = true;
			}
		}
		else
		{
			action->weapon = 0;
		}
	}

	bool canRun = _melee && _unit->getArmor()->allowsRunning(false) && _unit->getEnergy() > _unit->getBaseStats()->stamina * 0.4f;
	int visibleEnemiesToAttack = selectNearestTargetLeeroy(canRun);
	if (_traceAI)
	{
		Log(LOG_INFO) << "LEEROY: visibleEnemiesToAttack: " << visibleEnemiesToAttack << " _melee: " << _melee << (canRun ? " run" : "");
	}
	if ((visibleEnemiesToAttack > 0) && _melee)
	{
		if (_traceAI)
		{
			Log(LOG_INFO) << "LEEROY: LEEROYIN' at someone!";
		}
		meleeActionLeeroy(canRun);
		action->type = _attackAction.type;
		action->run = _attackAction.run;
		action->target = _attackAction.target;
		// if this is a firepoint action, set our facing.
		action->finalFacing = _attackAction.finalFacing;
		action->updateTU();
	}
	else
	{
		if (_traceAI)
		{
			Log(LOG_INFO) << "LEEROY: No one to LEEROY!, patrolling...";
		}
		setupPatrol();
		_unit->setCharging(0);
		_reserve = BA_NONE;
		action->type = _patrolAction.type;
		action->target = _patrolAction.target;
	}
}

/**
 * Tries to use self-target medikit if needed and desired (used for AI).
 * @return Was it used?
 */
bool AIModule::medikit_think(BattleMediKitType healOrStim)
{
	// 1. sanity checks, division by zero
	BattleUnit* self = _unit;

	if (self->getBaseStats()->stamina <= 0 || self->getBaseStats()->health <= 0)
	{
		return false;
	}

	// 2. quick unit checks (without RNG)
	int totalWounds = self->getFatalWounds();
	int percentHealthLeft = Clamp((self->getHealth() - self->getStunlevel()) * 100 / self->getBaseStats()->health, 0, 100);
	int percentEnergyLeft = Clamp(self->getEnergy() * 100 / self->getBaseStats()->stamina, 0, 100);

	if (healOrStim == BMT_HEAL)
	{
		if (totalWounds <= 0)
			return false;
	}
	else if (healOrStim == BMT_STIMULANT)
	{
		if (self->getStunlevel() <= 0 && percentEnergyLeft >= 40)
			return false;
	}
	else
	{
		// unsupported medikit type
		return false;
	}

	// 3. quick item checks
	std::vector<BattleItem*> usableMedikits;

	for (auto* item : *self->getInventory())
	{
		const RuleItem* itemRule = item->getRules();
		if (itemRule->getBattleType() == BT_MEDIKIT &&
			(itemRule->getMediKitType() == healOrStim || itemRule->getMediKitType() == BMT_NORMAL) &&
			itemRule->getAllowTargetSelf())
		{
			if (_save->getTurn() < itemRule->getAIUseDelay(_save->getMod()))
			{
				// can't use it yet, too soon
				continue;
			}
			usableMedikits.push_back(item);
		}
	}
	if (usableMedikits.empty())
	{
		// no compatible medikits available
		return false;
	}

	// 4. detailed unit checks (with RNG)
	bool wantsToHeal = false;
	bool wantsToStimStun = false;
	bool wantsToStimEnergy = false;

	if (healOrStim == BMT_HEAL)
	{
		if (totalWounds > 0)
		{
			if (self->getStunlevel() + totalWounds >= self->getHealth())
			{
				// going to die or pass out unless we do something, so do something!
				wantsToHeal = true;
			}
			else
			{
				//  0% health left = 120% chance to heal
				// 15% health left =  60% chance to heal
				// 30% health left =   0% chance to heal (actually 5% chance because of random heal wish)
				int chanceToHeal = 120 - (percentHealthLeft * 4);
				if (chanceToHeal <= 0)
				{
					// 5% for random heal wish (it's not urgent, but you know damage accumulates over time)
					chanceToHeal = 5;
				}
				wantsToHeal = RNG::percent(chanceToHeal);
			}
		}
		if (!wantsToHeal)
		{
			return false;
		}
	}
	else if (healOrStim == BMT_STIMULANT)
	{
		// 1. do we want to decrease stun level?
		if (self->getStunlevel() > 0)
		{
			if (self->getStunlevel() + totalWounds >= self->getHealth())
			{
				// going to die or pass out unless we do something, so do something!
				wantsToStimStun = true;
			}
			else
			{
				//  0% health left = 140% chance to stim
				// 10% health left =  70% chance to stim
				// 20% health left =   0% chance to stim
				int chanceToStim1 = 140 - (percentHealthLeft * 7);
				wantsToStimStun = chanceToStim1 > 0 ? RNG::percent(chanceToStim1) : false;
			}
		}
		// 2. do we want to increase energy?
		if (percentEnergyLeft < 40)
		{
			//  0% energy left = 120% chance to stim
			// 20% energy left =  60% chance to stim
			// 40% energy left =   0% chance to stim
			int chanceToStim2 = 120 - (percentEnergyLeft * 3);
			wantsToStimEnergy = RNG::percent(chanceToStim2);
		}
		if (!wantsToStimStun && !wantsToStimEnergy)
		{
			return false;
		}
	}

	// 5. let's do it
	bool used = false;

	for (auto* medikit : usableMedikits)
	{
		const RuleItem* medikitRule = medikit->getRules();
		{
			if ((wantsToHeal && medikit->getHealQuantity() > 0) ||
				(wantsToStimStun && medikit->getStimulantQuantity() > 0 && medikitRule->getStunRecovery() > 0) ||
				(wantsToStimEnergy && medikit->getStimulantQuantity() > 0 && medikitRule->getEnergyRecovery() > 0))
			{
				BattleAction medikitAction;
				{
					medikitAction.weapon = medikit;
					medikitAction.type = BA_USE;
					medikitAction.actor = self;

					medikitAction.updateTU();

					// yes, hardcoded 4 TUs
					// AI throwing grenades does that for decades and nobody cares, so calm down
					// also, AI pays this cost each time, even if using the same medikit multiple times in a row
					medikitAction.Time += 4; // 4TUs for picking up the medikit

					// sigh, modders...
					//medikitAction.Health = 0;
					//medikitAction.Stun = 0;
				}
				if (!medikitAction.spendTU())
				{
					// not enough TUs, try next item
					continue;
				}
				else
				{
					switch (healOrStim)
					{
					case BMT_HEAL:
						if (_traceAI)
						{
							Log(LOG_INFO) << "  Using medikit (heal). TU*/HP/Stun/Wounds: " <<
								self->getTimeUnits() << "/" << self->getHealth() << "/" << self->getStunlevel() << "/" << totalWounds;
						}
						for (int i = 0; i < BODYPART_MAX; ++i)
						{
							if (self->getFatalWound((UnitBodyPart)i))
							{
								_save->getTileEngine()->medikitUse(&medikitAction, self, BMA_HEAL, (UnitBodyPart)i);
								_save->getTileEngine()->medikitRemoveIfEmpty(&medikitAction);
								used = true;
								break;
							}
						}
						break;
					case BMT_STIMULANT:
						if (_traceAI)
						{
							if (wantsToStimStun)
							{
								Log(LOG_INFO) << "  Using medikit (-stun). TU*/HP/Stun/Wounds: " <<
									self->getTimeUnits() << "/" << self->getHealth() << "/" << self->getStunlevel() << "/" << totalWounds;
							}
							else
							{
								Log(LOG_INFO) << "  Using medikit (+energy). TU*/Energy: " << self->getTimeUnits() << "/" << self->getEnergy();
							}
						}
						_save->getTileEngine()->medikitUse(&medikitAction, self, BMA_STIMULANT, BODYPART_TORSO);
						_save->getTileEngine()->medikitRemoveIfEmpty(&medikitAction);
						used = true;
						break;
					case BMT_PAINKILLER:
					case BMT_NORMAL:
						// not supported
						break;
					}
				}
			}
		}
		if (used)
		{
			// only one use per attempt
			break;
		}
	}

	// 6. if we used something, let's try again
	return used;
}

/**
 * Runs any code the state needs to keep updating every AI cycle.
 * @param action (possible) AI action to execute after thinking is done.
 */
void AIModule::think(BattleAction *action)
{
	// BattleAction is reused across immediate rethink/pickup passes. Execution metadata and
	// audit scratch belong to exactly one pass and must never leak into the next candidate.
	action->aiTargetId = -1;
	action->aiAttemptPosition = Position();
	action->aiFailure = AIFailureReason::NONE;
	action->aiHasFilteredFallback = false;
	_auditReason.clear();
	_auditRunnerUp.clear();
	_auditBestScore = 0.0f;
	_auditRunnerUpScore = 0.0f;
	_auditBestTerms = {{0, 0, 0}};
	_auditTermLabels = {{"damage", "hit", "context"}};
	action->type = BA_RETHINK;
	action->actor = _unit;
	action->weapon = _unit->getMainHandWeapon(false);
	_attackAction.diff = _save->getBattleState()->getGame()->getSavedGame()->getDifficultyCoefficient();
	_attackAction.actor = _unit;
	_attackAction.run = false;
	_attackAction.weapon = action->weapon;
	_attackAction.aiTargetId = -1;
	_attackAction.aiHasFilteredFallback = false;
	_attackAction.number = action->number;
	_escapeAction.number = action->number;
	_knownEnemies = countKnownTargets();
	_visibleEnemies = selectNearestTarget();

	// Phase 34.9 (Calypso): drop this unit's stale intent from the faction blackboard before it
	// (re-)thinks, so the focus-fire / flank reads during its own target scoring below reflect
	// only its squadmates' commitments (per-member self-exclusion). Runs before the brutal
	// dispatch so it covers both paths. Gated + hostile-only; a no-op with the flag off.
	if (_save->getMod()->getAISquadCoordination() && _unit->getFaction() == FACTION_HOSTILE)
	{
		_save->clearSquadMemberIntent(FACTION_HOSTILE, _unit->getId());
	}
	_spottingEnemies = getSpottingUnits(_unit->getPosition());
	_melee = (_unit->getUtilityWeapon(BT_MELEE) != 0);
	_rifle = false;
	_blaster = false;
	_ranOutOfTUs = false; // Brutal-AI: findReachable reports whether the budget bounded the search
	_reachable = _save->getPathfinding()->findReachable(_unit, BattleActionCost(), _ranOutOfTUs);
	bool dummy = false; // Brutal-AI: throwaway out-param for the attack-reachability probes
	_wasHitBy.clear();
	_foundBaseModuleToDestroy = false;

	if (_unit->getCharging() && _unit->getCharging()->isOut())
	{
		_unit->setCharging(0);
	}

	if (_traceAI)
	{
		if (_unit->getFaction() == FACTION_HOSTILE)
		{
			Log(LOG_INFO) << "Unit has " << _visibleEnemies << "/" << _knownEnemies << " known enemies visible, " << _spottingEnemies << " of whom are spotting him. ";
		}
		else
		{
			Log(LOG_INFO) << "Civilian Unit has " << _visibleEnemies << " enemies visible, " << _spottingEnemies << " of whom are spotting him. ";
		}
		std::string AIMode;
		switch (_AIMode)
		{
		case AI_PATROL:
			AIMode = "Patrol";
			break;
		case AI_AMBUSH:
			AIMode = "Ambush";
			break;
		case AI_COMBAT:
			AIMode = "Combat";
			break;
		case AI_ESCAPE:
			AIMode = "Escape";
			break;
		}
		Log(LOG_INFO) << "Currently using " << AIMode << " behaviour";
	}

	if (_unit->isLeeroyJenkins())
	{
		dont_think(action);
		prepareAIAudit(action);
		return;
	}

	if (action->weapon)
	{
		const RuleItem *rule = action->weapon->getRules();
		if (_save->canUseWeapon(action->weapon, _unit, false, BA_NONE)) // Note: ammo is not checked here
		{
			if (rule->getBattleType() == BT_FIREARM)
			{
				if (action->weapon->getCurrentWaypoints() != 0)
				{
					_blaster = true;
					// Brutal-AI computes its own reachability; skip the vanilla attack-BFS for brutal units.
					if (!_unit->isBrutal())
						_reachableWithAttack = _save->getPathfinding()->findReachable(_unit, BattleActionCost(BA_AIMEDSHOT, _unit, action->weapon), dummy);
				}
				else
				{
					_rifle = true;
					if (!_unit->isBrutal())
						_reachableWithAttack = _save->getPathfinding()->findReachable(_unit, BattleActionCost(BA_SNAPSHOT, _unit, action->weapon), dummy);
				}
			}
			else if (rule->getBattleType() == BT_MELEE)
			{
				_melee = true;
				if (!_unit->isBrutal())
					_reachableWithAttack = _save->getPathfinding()->findReachable(_unit, BattleActionCost(BA_HIT, _unit, action->weapon), dummy);
			}
		}
		else
		{
			action->weapon = 0;
		}
	}

	BattleItem *grenadeItem = _unit->getGrenadeFromBelt(_save);
	_grenade = grenadeItem != 0;

	// Phase 34.5: hand hostile units off to the ported Brutal-OXCE decision-making. Placed here
	// (after grenade resolution, before the legacy escape/ambush/attack/patrol cascade) to mirror
	// Xilmi's own dispatch position. Civilians are FACTION_NEUTRAL and never brutal, so the Phase 32
	// hooks below are still reached for them. Adapted from Brutal-OXCE by Xilmi.
	if (_unit->isBrutal())
	{
		brutalThink(action);
		prepareAIAudit(action);
		// Phase 34.9 (Calypso): record the brutal path's finalized action on the squad blackboard.
		declareSquadIntentFromAction(action);
		return;
	}

	// Phase 32: a smart civilian flees as soon as it *sees* an alien, not only once an
	// alien is confirmed to be looking back at it. Guards are excluded from *both* triggers —
	// they protect and never flee, so no escape is ever staged for them (a staged escape plus
	// the spotter odds-multiplier in evaluateAIMode would otherwise give an armed guard a small
	// chance to bolt mid-fight). Non-guards keep the exact vanilla/Phase-32 condition.
	if (!isCivilianGuard()
		&& (_spottingEnemies || (isSmartCivilian() && _visibleEnemies))
		&& !_escapeTUs)
	{
		setupEscape();
	}

	// Phase 32: smart civilians (and guards) never ambush — a frightened civilian flees and a
	// brave-armed one holds its ground and fights; AI_AMBUSH (hide and wait) fits neither. Gating
	// here also skips a wasted findReachable BFS each turn for every civilian (their ambushOdds is
	// zeroed in evaluateAIMode anyway). countKnownTargets now gives civilians _knownEnemies > 0,
	// which would otherwise have routed them through setupAmbush.
	if (_knownEnemies && !_melee && !_ambushTUs && !isSmartCivilian())
	{
		setupAmbush();
	}

	setupAttack();
	setupPatrol();

	if (_psiAction.type != BA_NONE && !_didPsi && _save->getTurn() >= _psiAction.weapon->getRules()->getAIUseDelay(_save->getMod()))
	{
		_didPsi = true;
		action->type = _psiAction.type;
		action->target = _psiAction.target;
		action->number -= 1;
		action->weapon = _psiAction.weapon;
		action->updateTU();
		prepareAIAudit(action);
		return;
	}
	else
	{
		_didPsi = false;
	}

	bool evaluate = false;

	switch (_AIMode)
		{
		case AI_PATROL:
			evaluate = (bool)(_spottingEnemies || _visibleEnemies || _knownEnemies || RNG::percent(10));
			break;
		case AI_AMBUSH:
			evaluate = (!_rifle || !_ambushTUs || _visibleEnemies);
			break;
		case AI_COMBAT:
			evaluate = (_attackAction.type == BA_RETHINK);
			break;
		case AI_ESCAPE:
			evaluate = (!_spottingEnemies || !_knownEnemies);
			break;
			}

	if (_weaponPickedUp)
	{
		evaluate = true;
		_weaponPickedUp = false;
	}
	else if (_spottingEnemies > 2
		|| _unit->getHealth() < 2 * _unit->getBaseStats()->health / 3)
	{
		evaluate = true;
	}


	if (_save->isCheating() && _AIMode != AI_COMBAT)
	{
		evaluate = true;
	}

	if (evaluate)
	{
		evaluateAIMode();
		if (_traceAI)
		{
			std::string AIMode;
			switch (_AIMode)
			{
			case AI_PATROL:
				AIMode = "Patrol";
				break;
			case AI_AMBUSH:
				AIMode = "Ambush";
				break;
			case AI_COMBAT:
				AIMode = "Combat";
				break;
			case AI_ESCAPE:
				AIMode = "Escape";
				break;
			}
			Log(LOG_INFO) << "Re-Evaluated, now using " << AIMode << " behaviour";
		}
	}

	_reserve = BA_NONE;

	switch (_AIMode)
	{
	case AI_ESCAPE:
		_unit->setCharging(0);
		action->type = _escapeAction.type;
		action->target = _escapeAction.target;
		// end this unit's turn.
		action->finalAction = true;
		// ignore new targets.
		action->desperate = true;
		// if armor allow runing then run way from there.
		action->run = _escapeAction.run;
		// spin 180 at the end of your route.
		_unit->setHiding(true);
		break;
	case AI_PATROL:
		_unit->setCharging(0);
		if (action->weapon && action->weapon->getRules()->getBattleType() == BT_FIREARM)
		{
			switch (_unit->getAggression())
			{
			case 0:
				_reserve = BA_AIMEDSHOT;
				break;
			case 1:
				_reserve = BA_AUTOSHOT;
				break;
			case 2:
				_reserve = BA_SNAPSHOT;
				break;
			default:
				break;
			}
		}
		action->type = _patrolAction.type;
		action->target = _patrolAction.target;
		break;
	case AI_COMBAT:
		action->type = _attackAction.type;
		action->target = _attackAction.target;
		// this may have changed to a grenade.
		action->weapon = _attackAction.weapon;
		// Failure-memory identity belongs to the selected candidate. Do not reconstruct it
		// later from _aggroTarget: that pointer can refer to the last candidate evaluated.
		action->aiTargetId = _attackAction.aiTargetId;
		action->aiHasFilteredFallback = _attackAction.aiHasFilteredFallback;
		if (action->weapon && action->type == BA_THROW && action->weapon->getRules()->isGrenadeOrProxy())
		{
			_unit->spendCost(_unit->getActionTUs(BA_PRIME, action->weapon));
			_unit->spendTimeUnits(4);
		}
		// if this is a firepoint action, set our facing.
		action->finalFacing = _attackAction.finalFacing;
		action->updateTU();
		// if this is a "find fire point" action, don't increment the AI counter.
		if (action->type == BA_WALK && _rifle && _unit->getArmor()->allowsMoving()
			// so long as we can take a shot afterwards.
			&& BattleActionCost(BA_SNAPSHOT, _unit, action->weapon).haveTU())
		{
			action->number -= 1;
		}
		else if (action->type == BA_LAUNCH)
		{
			action->waypoints = _attackAction.waypoints;
		}
		else if (action->type == BA_AIMEDSHOT || action->type == BA_AUTOSHOT)
		{
			action->kneel = _unit->getArmor()->allowsKneeling(false);
		}
		break;
	case AI_AMBUSH:
		_unit->setCharging(0);
		action->type = _ambushAction.type;
		action->target = _ambushAction.target;
		// face where we think our target will appear.
		action->finalFacing = _ambushAction.finalFacing;
		// end this unit's turn.
		action->finalAction = true;
		action->kneel = _unit->getArmor()->allowsKneeling(false);
		break;
	default:
		break;
	}

	if (action->type == BA_WALK)
	{
		// if we're moving, we'll have to re-evaluate our escape/ambush position.
		if (action->target != _unit->getPosition())
		{
			_escapeTUs = 0;
			_ambushTUs = 0;
		}
		else
		{
			action->type = BA_NONE;
		}
	}

	// Phase 34.9 (Calypso): record the legacy path's finalized action on the squad blackboard
	// (the brutal path records its own above, before returning).
	declareSquadIntentFromAction(action);
	prepareAIAudit(action);
}


/*
 * sets the "was hit" flag to true.
 */
void AIModule::setWasHitBy(BattleUnit *attacker)
{
	if (attacker->getFaction() != _unit->getFaction() && !getWasHitBy(attacker->getId()))
		_wasHitBy.push_back(attacker->getId());
}

/*
 * Sets the "unit picked up a weapon" flag.
 */
void AIModule::setWeaponPickedUp()
{
	_weaponPickedUp = true;
}

/*
 * Gets whether the unit was hit.
 * @return if it was hit.
 */
bool AIModule::getWasHitBy(int attacker) const
{
	return std::find(_wasHitBy.begin(), _wasHitBy.end(), attacker) != _wasHitBy.end();
}
/*
 * Sets up a patrol action.
 * this is mainly going from node to node, moving about the map.
 * handles node selection, and fills out the _patrolAction with useful data.
 */
void AIModule::setupPatrol()
{
	_patrolAction.clearTU();

	// Phase 32 (Calypso): an armed guard with no attack staged this turn doesn't aimlessly
	// node-patrol — it advances to protect (rescue a screaming civilian, else regroup with the
	// aquanauts). Keyed on "no staged attack" (setupAttack ran just above) rather than "no alien
	// perceived", so a guard whose target is only remembered (LOS broke) or that is out of ammo
	// still advances instead of node-patrolling away. When it CAN attack, evaluateAIMode steers it
	// to combat and this is skipped (_attackAction.type != BA_RETHINK).
	if (isCivilianGuard() && _attackAction.type == BA_RETHINK)
	{
		// Free any patrol node still allocated from an earlier (combat-turn) node-patrol — the
		// guard advances toward its own objective and never reaches that node, so leaving it
		// allocated would lock one node per guard for the rest of the battle (and into saves).
		freePatrolTarget();
		_toNode = 0;
		if (setupGuardMove())
		{
			return;
		}
	}

	// Phase 34.4 (Calypso): an unengaged terror-mission alien biases its patrol-node choice
	// toward the (quantized) civilian-hunt zone instead of picking randomly/highest-value.
	// `huntZoneKnown` gates the two getPatrolNode call sites further down; when it's false
	// (flag off, alien engaged, or no civilians alive) they behave exactly as vanilla.
	Position huntZone;
	bool huntZoneKnown = wantsToHuntCivilians() && _save->getCivilianHuntZone(huntZone);
	// Phase 34.8 (Calypso): FALLBACK bias -- when no civilian-hunt zone applies (34.4 flag
	// off, no civilians alive, or this isn't a terror mission), an unengaged hostile that
	// hears a recent noise biases toward the newest in-range noise zone instead of wandering
	// randomly. The two behaviours are disjoint: noise investigation runs only when the
	// civilian-hunt zone is unknown, so 34.8 never undermines the 34.4 terror-mission race.
	// Reuses the SAME getPatrolNode(scout, unit, fromNode, zoneAnchor) overload 34.4 added --
	// no new BFS, no new reachable-tile walk. `biasZone` is whichever (if any) zone is known.
	Position noiseZone;
	bool noiseZoneKnown = !huntZoneKnown
		&& wantsToInvestigateNoise()
		&& _save->getNewestHearableNoise(_unit->getPosition(), _intelligence, noiseZone);
	const Position &biasZone = huntZoneKnown ? huntZone : noiseZone;
	const bool biasZoneKnown = huntZoneKnown || noiseZoneKnown;

	if (_toNode != 0 && _unit->getPosition() == _toNode->getPosition())
	{
		if (_traceAI)
		{
			Log(LOG_INFO) << "Patrol destination reached!";
		}
		// destination reached
		// head off to next patrol node
		_fromNode = _toNode;
		freePatrolTarget();
		_toNode = 0;
		// take a peek through window before walking to the next node
		int dir = _save->getTileEngine()->faceWindow(_unit->getPosition());
		if (dir != -1 && dir != _unit->getDirection())
		{
			_unit->lookAt(dir);
			while (_unit->getStatus() == STATUS_TURNING)
			{
				_unit->turn();
			}
		}
	}

	if (_fromNode == 0)
	{
		// assume closest node as "from node"
		// on same level to avoid strange things, and the node has to match unit size or it will freeze
		int closest = 1000000;
		for (auto* node : *_save->getNodes())
		{
			if (node->isDummy())
			{
				continue;
			}
			int d = Position::distanceSq(_unit->getPosition(), node->getPosition());
			if (_unit->getPosition().z == node->getPosition().z
				&& d < closest
				&& (!(node->getType() & Node::TYPE_SMALL) || _unit->getArmor()->getSize() == 1))
			{
				_fromNode = node;
				closest = d;
			}
		}
	}
	int triesLeft = 5;

	while (_toNode == 0 && triesLeft)
	{
		triesLeft--;
		// look for a new node to walk towards
		bool scout = true;
		if (_save->getMissionType() != "STR_BASE_DEFENSE")
		{
			// after turn 20 or if the morale is low, everyone moves out the UFO and scout
			// also anyone standing in fire should also probably move
			if (_save->isCheating() || !_fromNode || _fromNode->getRank() == 0 ||
				(_save->getTile(_unit->getPosition()) && _save->getTile(_unit->getPosition())->getFire()))
			{
				scout = true;
			}
			else
			{
				scout = false;
			}
		}

		// in base defense missions, the smaller aliens walk towards target nodes - or if there, shoot objects around them
		else if (_unit->getArmor()->getSize() == 1 && _unit->getOriginalFaction() == FACTION_HOSTILE &&
				_attackAction.weapon &&
				_attackAction.weapon->getRules()->getAccuracySnap() &&
				!_attackAction.weapon->getRules()->getArcingShot() &&
				_attackAction.weapon->getAmmoForAction(BA_SNAPSHOT) &&
				!_attackAction.weapon->getAmmoForAction(BA_SNAPSHOT)->getRules()->getArcingShot() &&
				_attackAction.weapon->getAmmoForAction(BA_SNAPSHOT)->getRules()->getDamageType()->isDirect() &&
				_attackAction.weapon->getAmmoForAction(BA_SNAPSHOT)->getRules()->getDamageType()->ToTile > 0.01f)
		{
			// can i shoot an object?
			if (_fromNode->isTarget() &&
				_save->canUseWeapon(_attackAction.weapon, _unit, false, BA_SNAPSHOT) &&
				_save->getModuleMap()[_fromNode->getPosition().x / 10][_fromNode->getPosition().y / 10].second > 0)
			{
				// scan this room for objects to destroy
				int x = (_unit->getPosition().x/10)*10;
				int y = (_unit->getPosition().y/10)*10;
				for (int i = x; i < x+9; i++)
				for (int j = y; j < y+9; j++)
				{
					MapData *md = _save->getTile(Position(i, j, 1))->getMapData(O_OBJECT);
					if (md && md->isBaseModule())
					{
						_patrolAction.actor = _unit;
						_patrolAction.target = Position(i, j, 1);
						_patrolAction.weapon = _attackAction.weapon;
						_patrolAction.type = BA_SNAPSHOT;
						_patrolAction.updateTU();
						_foundBaseModuleToDestroy = _save->getMod()->getAIDestroyBaseFacilities();
						return;
					}
				}
			}
			else
			{
				// find closest high value target which is not already allocated
				int closest = 1000000;
				BattleUnit* nodeunit = nullptr;
				for (auto* node : *_save->getNodes())
				{
					if (node->isDummy())
					{
						continue;
					}

					nodeunit = _save->getTile(node->getPosition())->getUnit();
					if (nodeunit && nodeunit->getFaction() == _unit->getFaction())
					{
						continue;
					}

					if (node->isTarget() && !node->isAllocated() && _save->getModuleMap()[node->getPosition().x / 10][node->getPosition().y / 10].second > 0)
					{
						int d = Position::distanceSq(_unit->getPosition(), node->getPosition());
						if (!_toNode ||  (d < closest && node != _fromNode))
						{
							_toNode = node;
							closest = d;
						}
					}
				}
			}
		}

		if (_toNode == 0)
		{
			_toNode = biasZoneKnown ? _save->getPatrolNode(scout, _unit, _fromNode, biasZone)
									: _save->getPatrolNode(scout, _unit, _fromNode);
			if (_toNode == 0)
			{
				_toNode = biasZoneKnown ? _save->getPatrolNode(!scout, _unit, _fromNode, biasZone)
										: _save->getPatrolNode(!scout, _unit, _fromNode);
			}
		}

		if (_toNode != 0)
		{
			_save->getPathfinding()->calculate(_unit, _toNode->getPosition(), BAM_NORMAL);
			if (_save->getPathfinding()->getStartDirection() == -1)
			{
				_toNode = 0;
			}
			_save->getPathfinding()->abortPath();
		}
	}

	if (_toNode != 0)
	{
		_toNode->allocateNode();
		_patrolAction.actor = _unit;
		_patrolAction.type = BA_WALK;
		_patrolAction.target = _toNode->getPosition();
	}
	else
	{
		_patrolAction.type = BA_RETHINK;
	}
}

/**
 * Try to set up an ambush action
 * The idea is to check within a 11x11 tile square for a tile which is not seen by our aggroTarget,
 * but that can be reached by him. we then intuit where we will see the target first from our covered
 * position, and set that as our final facing.
 * Fills out the _ambushAction with useful data.
 */
void AIModule::setupAmbush()
{
	_ambushAction.type = BA_RETHINK;
	int bestScore = 0;
	_ambushTUs = 0;
	std::vector<int> path;

	if (selectClosestKnownEnemy())
	{
		const int BASE_SYSTEMATIC_SUCCESS = 100;
		const int COVER_BONUS = 25;
		const int FAST_PASS_THRESHOLD = 80;
		Position origin = _save->getTileEngine()->getSightOriginVoxel(_aggroTarget);

		// we'll use node positions for this, as it gives map makers a good degree of control over how the units will use the environment.
		for (const auto* node : *_save->getNodes())
		{
			if (node->isDummy())
			{
				continue;
			}
			Position pos = node->getPosition();
			Tile *tile = _save->getTile(pos);
			if (tile == 0 || Position::distance2d(pos, _unit->getPosition()) > 10 || pos.z != _unit->getPosition().z || tile->getDangerous() ||
				std::find(_reachableWithAttack.begin(), _reachableWithAttack.end(), _save->getTileIndex(pos))  == _reachableWithAttack.end())
				continue; // just ignore unreachable tiles

			if (_traceAI)
			{
				// colour all the nodes in range purple.
				tile->setPreview(10);
				tile->setMarkerColor(13);
			}

			// make sure we can't be seen here.
			Position target;
			if (!_save->getTileEngine()->canTargetUnit(&origin, tile, &target, _aggroTarget, false, _unit) && !getSpottingUnits(pos))
			{
				_save->getPathfinding()->calculate(_unit, pos, BAM_NORMAL);
				int ambushTUs = _save->getPathfinding()->getTotalTUCost();
				// make sure we can move here
				if (_save->getPathfinding()->getStartDirection() != -1)
				{
					int score = BASE_SYSTEMATIC_SUCCESS;
					score -= ambushTUs;

					// make sure our enemy can reach here too.
					_save->getPathfinding()->calculate(_aggroTarget, pos, BAM_NORMAL);

					if (_save->getPathfinding()->getStartDirection() != -1)
					{
						// ideally we'd like to be behind some cover, like say a window or a low wall.
						if (_save->getTileEngine()->faceWindow(pos) != -1)
						{
							score += COVER_BONUS;
						}
						if (score > bestScore)
						{
							path = _save->getPathfinding()->copyPath();
							bestScore = score;
							_ambushTUs = (pos == _unit->getPosition()) ? 1 : ambushTUs;
							_ambushAction.target = pos;
							if (bestScore > FAST_PASS_THRESHOLD)
							{
								break;
							}
						}
					}
				}
			}
		}

		if (bestScore > 0)
		{
			_ambushAction.type = BA_WALK;
			// i should really make a function for this
			origin = _ambushAction.target.toVoxel() +
				// 4 because -2 is eyes and 2 below that is the rifle (or at least that's my understanding)
				Position(8,8, _unit->getHeight() + _unit->getFloatHeight() - _save->getTile(_ambushAction.target)->getTerrainLevel() - 4);
			Position currentPos = _aggroTarget->getPosition();
			_save->getPathfinding()->setUnit(_aggroTarget);
			size_t tries = path.size();
			// hypothetically walk the target through the path.
			while (tries > 0)
			{
				currentPos = _save->getPathfinding()->getTUCost(currentPos, path.back(), _aggroTarget, 0, BAM_NORMAL).pos;
				path.pop_back();
				Tile *tile = _save->getTile(currentPos);
				Position target;
				// do a virtual fire calculation
				if (_save->getTileEngine()->canTargetUnit(&origin, tile, &target, _unit, false, _aggroTarget))
				{
					// if we can virtually fire at the hypothetical target, we know which way to face.
					_ambushAction.finalFacing = _save->getTileEngine()->getDirectionTo(_ambushAction.target, currentPos);
					break;
				}
				--tries;
			}
			if (_traceAI)
			{
				Log(LOG_INFO) << "Ambush estimation will move to " << _ambushAction.target;
			}
			return;
		}
	}
	if (_traceAI)
	{
		Log(LOG_INFO) << "Ambush estimation failed";
	}
}

/**
 * Try to set up a combat action
 * This will either be a psionic, grenade, or weapon attack,
 * or potentially just moving to get a line of sight to a target.
 * Fills out the _attackAction with useful data.
 */
void AIModule::setupAttack()
{
	_attackAction.type = BA_RETHINK;
	_psiAction.type = BA_NONE;

	bool sniperAttack = false;

	// if enemies are known to us but not necessarily visible, we can attack them with a blaster launcher or psi or a sniper attack.
	if (_knownEnemies)
	{
		if (psiAction())
		{
			// at this point we can save some time with other calculations - the unit WILL make a psionic attack this turn.
			return;
		}
		if (_blaster)
		{
			wayPointAction();
		}
		else if (_unit->getUnitRules()) // xcom soldiers (under mind control) lack unit rules!
		{
			// don't always act on spotter information unless modder says so
			if (RNG::percent(_unit->getUnitRules()->getSniperPercentage()))
			{
				sniperAttack = sniperAction();
			}
		}
	}

	// if we CAN see someone, that makes them a viable target for "regular" attacks.
	// This is skipped if sniperAction has already chosen an attack action
	if (!sniperAttack && selectNearestTarget())
	{
		// if we have both types of weapon, make a determination on which to use.
		if (_melee && _rifle)
		{
			selectMeleeOrRanged();
		}
		if (_grenade)
		{
			grenadeAction();
		}
		if (_melee)
		{
			meleeAction();
		}
		if (_rifle)
		{
			projectileAction();
		}
	}

	// Phase 34.6 (Calypso): terrain tactics (floor-drop / wall-breach) -- gated internally on
	// the mod flag, the unit being a non-civilian hostile, and no other attack having been
	// chosen. A no-op when the flag is off, so the legacy path stays byte-identical.
	if (_attackAction.type == BA_RETHINK)
	{
		considerTerrainAttack();
	}

	if (_attackAction.type != BA_RETHINK)
	{
		if (_traceAI)
		{
			if (_attackAction.type != BA_WALK)
			{
				Log(LOG_INFO) << "Attack estimation desires to shoot at " << _attackAction.target;
			}
			else
			{
				Log(LOG_INFO) << "Attack estimation desires to move to " << _attackAction.target;
			}
		}
		return;
	}
	else if (_spottingEnemies || _unit->getAggression() < RNG::generate(0, 3))
	{
		// if enemies can see us, or if we're feeling lucky, we can try to spot the enemy.
		if (findFirePoint())
		{
			if (_traceAI)
			{
				Log(LOG_INFO) << "Attack estimation desires to move to " << _attackAction.target;
			}
			return;
		}
	}
	if (_traceAI)
	{
		Log(LOG_INFO) << "Attack estimation failed";
	}
}

/**
 * Attempts to find cover, and move toward it.
 * The idea is to check within a 11x11 tile square for a tile which is not seen by our aggroTarget.
 * If there is no such tile, we run away from the target.
 * Fills out the _escapeAction with useful data.
 */
void AIModule::setupEscape()
{
	int unitsSpottingMe = getSpottingUnits(_unit->getPosition());
	int currentTilePreference = 15;
	int tries = -1;
	bool coverFound = false;
	selectNearestTarget();
	// Phase 32: a civilian fleeing a remembered-but-unseen alien still needs a direction to flee
	// FROM. selectNearestTarget() clears _aggroTarget when no enemy is currently visible (civilians
	// now reach setupEscape on memory-only threats via countKnownTargets), so fall back to the
	// nearest remembered enemy — otherwise the away-from-alien term below contributes nothing and
	// the escape is steered only by the safety bias (which can point past/at the alien).
	if (!_aggroTarget && isSmartCivilian())
	{
		selectClosestKnownEnemy();
	}
	_escapeTUs = 0;

	// Phase 32: a fleeing civilian biases its escape tile toward safety (nearest seen
	// soldier > civilian cluster > map edge), on top of the away-from-alien / cover terms.
	bool haveSafety = false;
	Position safetyTarget(0, 0, 0);
	const Position selfPos = _unit->getPosition();
	int curToSafety = 0;
	if (isSmartCivilian())
	{
		haveSafety = findCivilianSafetyTarget(safetyTarget);
		// The edge fallback returns self when an ally-less civilian is already on its nearest map
		// edge. Then curToSafety would be 0 and the safety term becomes -candToSafety*8 — penalizing
		// ANY step away, which damps (sometimes cancels) the genuine flee bias. Drop it in that case.
		if (haveSafety && safetyTarget == selfPos)
		{
			haveSafety = false;
		}
		if (haveSafety)
		{
			curToSafety = Position::distance2d(selfPos, safetyTarget);
		}
	}

	// Phase 34.9 (Calypso): a badly wounded hostile (health < 1/3 of max) biases its escape toward
	// the nearest friendly cluster centroid -- mirrors the Phase 32 civilian safety bias above
	// (same shape, same weight 8). Gated + hostile-only; the centroid is the live squadmates'
	// average position (the blackboard "knows member positions"). Off => haveCluster false =>
	// the bias term below is skipped => byte-identical.
	bool haveCluster = false;
	Position clusterTarget(0, 0, 0);
	int curToCluster = 0;
	if (_save->getMod()->getAISquadCoordination()
		&& _unit->getFaction() == FACTION_HOSTILE
		&& _unit->getHealth() * 3 < _unit->getBaseStats()->health)
	{
		haveCluster = _save->getFriendlyClusterCentroid(FACTION_HOSTILE, _unit, clusterTarget);
		// If the centroid is the unit's own tile the term would just penalize any step (mirrors
		// the Phase 32 self==safety guard) -- drop it.
		if (haveCluster && clusterTarget == selfPos)
		{
			haveCluster = false;
		}
		if (haveCluster)
		{
			curToCluster = Position::distance2d(selfPos, clusterTarget);
		}
	}

	int dist = _aggroTarget ? Position::distance2d(_unit->getPosition(), _aggroTarget->getPosition()) : 0;

	int bestTileScore = -100000;
	int score = -100000;
	Position bestTile(0, 0, 0);
	bool run = false;

	Tile *tile = 0;

	// weights of various factors in choosing a tile to which to withdraw
	const int EXPOSURE_PENALTY = 10;
	const int FIRE_PENALTY = 40;
	const int BASE_SYSTEMATIC_SUCCESS = 100;
	const int BASE_DESPERATE_SUCCESS = 110;
	const int FAST_PASS_THRESHOLD = 100; // a score that's good enough to quit the while loop early; it's subjective, hand-tuned and may need tweaking

	std::vector<Position> randomTileSearch = _save->getTileSearch();
	RNG::shuffle(randomTileSearch);

	while (tries < 150 && !coverFound)
	{
		_escapeAction.target = _unit->getPosition(); // start looking in a direction away from the enemy
		_escapeAction.run = _unit->getArmor()->allowsRunning(false) && (tries & 1); // every odd try, i.e. roughly 50%

		if (!_save->getTile(_escapeAction.target))
		{
			_escapeAction.target = _unit->getPosition(); // cornered at the edge of the map perhaps?
		}

		score = 0;

		if (tries == -1)
		{
			// you know, maybe we should just stay where we are and not risk reaction fire...
			// or maybe continue to wherever we were running to and not risk looking stupid
			if (_save->getTile(_unit->lastCover) != 0)
			{
				_escapeAction.target = _unit->lastCover;
			}
		}
		else if (tries < 121)
		{
			// looking for cover
			_escapeAction.target.x += randomTileSearch[tries].x;
			_escapeAction.target.y += randomTileSearch[tries].y;
			score = BASE_SYSTEMATIC_SUCCESS;
			if (_escapeAction.target == _unit->getPosition())
			{
				if (unitsSpottingMe > 0)
				{
					// maybe don't stay in the same spot? move or something if there's any point to it?
					_escapeAction.target.x += RNG::generate(-20,20);
					_escapeAction.target.y += RNG::generate(-20,20);
				}
				else
				{
					score += currentTilePreference;
				}
			}
		}
		else
		{
			if (tries == 121)
			{
				if (_traceAI)
				{
					Log(LOG_INFO) << "best score after systematic search was: " << bestTileScore;
				}
			}

			score = BASE_DESPERATE_SUCCESS; // ruuuuuuun
			_escapeAction.target = _unit->getPosition();
			_escapeAction.target.x += RNG::generate(-10,10);
			_escapeAction.target.y += RNG::generate(-10,10);
			_escapeAction.target.z = _unit->getPosition().z + RNG::generate(-1,1);
			if (_escapeAction.target.z < 0)
			{
				_escapeAction.target.z = 0;
			}
			else if (_escapeAction.target.z >= _save->getMapSizeZ())
			{
				_escapeAction.target.z = _unit->getPosition().z;
			}
		}

		tries++;

		// THINK, DAMN YOU
		tile = _save->getTile(_escapeAction.target);
		int distanceFromTarget = _aggroTarget ? Position::distance2d(_aggroTarget->getPosition(), _escapeAction.target) : 0;
		if (dist >= distanceFromTarget)
		{
			score -= (distanceFromTarget - dist) * 10;
		}
		else
		{
			score += (distanceFromTarget - dist) * 10;
		}
		// Phase 32: reward candidate tiles that bring the civilian closer to safety.
		if (haveSafety)
		{
			int candToSafety = Position::distance2d(_escapeAction.target, safetyTarget);
			score += (curToSafety - candToSafety) * 8;
		}
		// Phase 34.9: reward tiles that bring a wounded alien closer to its squad cluster.
		if (haveCluster)
		{
			int candToCluster = Position::distance2d(_escapeAction.target, clusterTarget);
			score += (curToCluster - candToCluster) * 8;
		}
		int spotters = 0;
		if (!tile)
		{
			score = -100001; // no you can't quit the battlefield by running off the map.
		}
		else
		{
			spotters = getSpottingUnits(_escapeAction.target);
			if (std::find(_reachable.begin(), _reachable.end(), _save->getTileIndex(_escapeAction.target))  == _reachable.end())
				continue; // just ignore unreachable tiles

			if (_spottingEnemies || spotters)
			{
				if (_spottingEnemies <= spotters)
				{
					score -= (1 + spotters - _spottingEnemies) * EXPOSURE_PENALTY; // that's for giving away our position
				}
				else
				{
					score += (_spottingEnemies - spotters) * EXPOSURE_PENALTY;
				}
			}
			if (tile->getFire())
			{
				score -= FIRE_PENALTY;
			}
			if (tile->getDangerous())
			{
				score -= BASE_SYSTEMATIC_SUCCESS;
			}

			if (_traceAI)
			{
				tile->setMarkerColor(score < 0 ? 3 : (score < FAST_PASS_THRESHOLD/2 ? 8 : (score < FAST_PASS_THRESHOLD ? 9 : 5)));
				tile->setPreview(10);
				tile->setTUMarker(score);
			}

		}

		if (tile && score > bestTileScore)
		{
			// calculate TUs to tile; we could be getting this from findReachable() somehow but that would break something for sure...
			_save->getPathfinding()->calculate(_unit, _escapeAction.target, _escapeAction.getMoveType());
			if (_escapeAction.target == _unit->getPosition() || _save->getPathfinding()->getStartDirection() != -1)
			{
				bestTileScore = score;
				bestTile = _escapeAction.target;
				run = _escapeAction.run;
				_escapeTUs = _save->getPathfinding()->getTotalTUCost();
				if (_escapeAction.target == _unit->getPosition())
				{
					_escapeTUs = 1;
				}
				if (_traceAI)
				{
					tile->setMarkerColor(score < 0 ? 7 : (score < FAST_PASS_THRESHOLD/2 ? 10 : (score < FAST_PASS_THRESHOLD ? 4 : 5)));
					tile->setPreview(10);
					tile->setTUMarker(score);
				}
			}
			_save->getPathfinding()->abortPath();
			if (bestTileScore > FAST_PASS_THRESHOLD) coverFound = true; // good enough, gogogo
		}
	}
	_escapeAction.target = bestTile;
	_escapeAction.run = run;
	if (_traceAI)
	{
		_save->getTile(_escapeAction.target)->setMarkerColor(13);
	}

	if (bestTileScore <= -100000)
	{
		if (_traceAI)
		{
			Log(LOG_INFO) << "Escape estimation failed.";
		}
		_escapeAction.type = BA_RETHINK; // do something, just don't look dumbstruck :P
		return;
	}
	else
	{
		if (_traceAI)
		{
			Log(LOG_INFO) << "Escape estimation completed after " << tries << " tries, " << Position::distance2d(_unit->getPosition(), bestTile) << " squares or so away.";
		}
		_escapeAction.type = BA_WALK;
	}
}

/**
 * Counts how many targets, both xcom and civilian are known to this unit
 * @return how many targets are known to us.
 */
int AIModule::countKnownTargets() const
{
	int knownEnemies = 0;

	// Phase 32: smart civilians also accumulate remembered enemies (within their
	// intelligence window) so they keep fleeing/fighting a known alien after LOS breaks.
	if (_unit->getFaction() == FACTION_HOSTILE || isSmartCivilian())
	{
		for (auto* bu : *_save->getUnits())
		{
			if (validTarget(bu, true, true))
			{
				++knownEnemies;
			}
		}
	}
	return knownEnemies;
}

/*
 * counts how many enemies (xcom only) are spotting any given position.
 * @param pos the Position to check for spotters.
 * @return spotters.
 */
int AIModule::getSpottingUnits(const Position& pos) const
{
	// if we don't actually occupy the position being checked, we need to do a virtual LOF check.
	bool checking = pos != _unit->getPosition();
	int tally = 0;
	for (auto* bu : *_save->getUnits())
	{
		if (validTarget(bu, false, false))
		{
			int dist = Position::distance2d(pos, bu->getPosition());
			if (dist > 20) continue;
			Position originVoxel = _save->getTileEngine()->getSightOriginVoxel(bu);
			originVoxel.z -= 2;
			Position targetVoxel;
			if (checking)
			{
				if (_save->getTileEngine()->canTargetUnit(&originVoxel, _save->getTile(pos), &targetVoxel, bu, false, _unit))
				{
					tally++;
				}
			}
			else
			{
				if (_save->getTileEngine()->canTargetUnit(&originVoxel, _save->getTile(pos), &targetVoxel, bu, false))
				{
					tally++;
				}
			}
		}
	}
	return tally;
}

/**
 * Selects the nearest known living target we can see/reach and returns the number of visible enemies.
 * This function includes civilians as viable targets.
 * @return viable targets.
 */
int AIModule::selectNearestTarget()
{
	int tally = 0;
	_closestDist= 100;
	_aggroTarget = 0;
	Position target;
	for (auto* bu : *_save->getUnits())
	{
		if (validTarget(bu, true, true) &&
			_save->getTileEngine()->visible(_unit, bu->getTile()))
		{
			tally++;
			int dist = Position::distance2d(_unit->getPosition(), bu->getPosition());
			if (dist < _closestDist)
			{
				bool valid = false;
				if (_rifle || !_melee)
				{
					BattleAction action;
					action.actor = _unit;
					action.weapon = _attackAction.weapon;
					action.target = bu->getPosition();
					Position origin = _save->getTileEngine()->getOriginVoxel(action, 0);
					valid = _save->getTileEngine()->canTargetUnit(&origin, bu->getTile(), &target, _unit, false);
				}
				else
				{
					if (selectPointNearTarget(bu, _unit->getTimeUnits()))
					{
						int dir = _save->getTileEngine()->getDirectionTo(_attackAction.target, bu->getPosition());
						valid = _save->getTileEngine()->validMeleeRange(_attackAction.target, dir, _unit, bu, 0);
					}
				}
				if (valid)
				{
					_closestDist = dist;
					_aggroTarget = bu;
				}
			}
		}
	}
	if (_aggroTarget)
	{
		return tally;
	}

	return 0;
}

/**
 * Selects the nearest known living target we can see/reach and returns the number of visible enemies.
 * This function includes civilians as viable targets.
 * Note: Differs from selectNearestTarget() in calling selectPointNearTargetLeeroy().
 * @return viable targets.
 */
int AIModule::selectNearestTargetLeeroy(bool canRun)
{
	int tally = 0;
	_closestDist = 100;
	_aggroTarget = 0;
	for (auto* bu : *_save->getUnits())
	{
		if (validTarget(bu, true, true) &&
			_save->getTileEngine()->visible(_unit, bu->getTile()))
		{
			tally++;
			int dist = Position::distance2d(_unit->getPosition(), bu->getPosition());
			if (dist < _closestDist)
			{
				bool valid = false;
				if (selectPointNearTargetLeeroy(bu, canRun))
				{
					int dir = _save->getTileEngine()->getDirectionTo(_attackAction.target, bu->getPosition());
					valid = _save->getTileEngine()->validMeleeRange(_attackAction.target, dir, _unit, bu, 0);
				}
				if (valid)
				{
					_closestDist = dist;
					_aggroTarget = bu;
				}
			}
		}
	}
	if (_aggroTarget)
	{
		return tally;
	}

	return 0;
}

/**
 * Selects the nearest known living Xcom unit.
 * used for ambush calculations
 * @return if we found one.
 */
bool AIModule::selectClosestKnownEnemy()
{
	_aggroTarget = 0;
	int minDist = 255;
	for (auto* bu : *_save->getUnits())
	{
		if (validTarget(bu, true, false))
		{
			int dist = Position::distance2d(bu->getPosition(), _unit->getPosition());
			if (dist < minDist)
			{
				minDist = dist;
				_aggroTarget = bu;
			}
		}
	}
	return _aggroTarget != 0;
}

/**
 * Selects a random known living Xcom or civilian unit.
 * @return if we found one.
 */
bool AIModule::selectRandomTarget()
{
	int farthest = -100;
	_aggroTarget = 0;

	for (auto* bu : *_save->getUnits())
	{
		if (validTarget(bu, true, true))
		{
			int dist = RNG::generate(0,20) - Position::distance2d(_unit->getPosition(), bu->getPosition());
			if (dist > farthest)
			{
				farthest = dist;
				_aggroTarget = bu;
			}
		}
	}
	return _aggroTarget != 0;
}

/**
 * Selects a point near enough to our target to perform a melee attack.
 * @param target Pointer to a target.
 * @param maxTUs Maximum time units the path to the target can cost.
 * @return True if a point was found.
 */
bool AIModule::selectPointNearTarget(BattleUnit *target, int maxTUs)
{
	int size = _unit->getArmor()->getSize();
	int sizeTarget = target->getArmor()->getSize();
	int dirTarget = target->getDirection();
	float dodgeChanceDiff = target->getArmor()->getMeleeDodge(target) * target->getArmor()->getMeleeDodgeBackPenalty() * _attackAction.diff / 160.0f;
	bool returnValue = false;
	int distance = 1000;
	for (int z = -1; z <= 1; ++z)
	{
		for (int x = -size; x <= sizeTarget; ++x)
		{
			for (int y = -size; y <= sizeTarget; ++y)
			{
				if (x || y) // skip the unit itself
				{
					Position checkPath = target->getPosition() + Position (x, y, z);
					if (_save->getTile(checkPath) == 0 || std::find(_reachable.begin(), _reachable.end(), _save->getTileIndex(checkPath))  == _reachable.end())
						continue;
					int dir = _save->getTileEngine()->getDirectionTo(checkPath, target->getPosition());
					bool valid = _save->getTileEngine()->validMeleeRange(checkPath, dir, _unit, target, 0);
					bool fitHere = _save->setUnitPosition(_unit, checkPath, true);

					if (valid && fitHere && !_save->getTile(checkPath)->getDangerous())
					{
						_save->getPathfinding()->calculate(_unit, checkPath, BAM_NORMAL, 0, maxTUs);

						//for 100% dodge diff and on 4th difficulty it will allow aliens to move 10 squares around to made attack from behind.
						int distanceCurrent = _save->getPathfinding()->getPath().size() - dodgeChanceDiff * _save->getTileEngine()->getArcDirection(dir - 4, dirTarget);
						if (_save->getPathfinding()->getStartDirection() != -1 && distanceCurrent < distance)
						{
							_attackAction.target = checkPath;
							returnValue = true;
							distance = distanceCurrent;
						}
						_save->getPathfinding()->abortPath();
					}
				}
			}
		}
	}
	return returnValue;
}

/**
 * Selects a point near enough to our target to perform a melee attack.
 * Note: Differs from selectPointNearTarget() in that it doesn't consider:
 *  - remaining TUs (charge even if not enough TUs to attack)
 *  - dangerous tiles (grenades? pfff!)
 *  - melee dodge (not intelligent enough to attack from behind)
 * @param target Pointer to a target.
 * @return True if a point was found.
 */
bool AIModule::selectPointNearTargetLeeroy(BattleUnit *target, bool canRun)
{
	int size = _unit->getArmor()->getSize();
	int targetsize = target->getArmor()->getSize();
	bool returnValue = false;
	unsigned int distance = 1000;
	for (int z = -1; z <= 1; ++z)
	{
		for (int x = -size; x <= targetsize; ++x)
		{
			for (int y = -size; y <= targetsize; ++y)
			{
				if (x || y) // skip the unit itself
				{
					Position checkPath = target->getPosition() + Position(x, y, z);
					if (_save->getTile(checkPath) == 0)
						continue;
					int dir = _save->getTileEngine()->getDirectionTo(checkPath, target->getPosition());
					bool valid = _save->getTileEngine()->validMeleeRange(checkPath, dir, _unit, target, 0);
					bool fitHere = _save->setUnitPosition(_unit, checkPath, true);

					if (valid && fitHere)
					{
						_save->getPathfinding()->calculate(_unit, checkPath, canRun ? BAM_RUN : BAM_NORMAL, 0, 100000); // disregard unit's TUs.
						if (_save->getPathfinding()->getStartDirection() != -1 && _save->getPathfinding()->getPath().size() < distance)
						{
							_attackAction.target = checkPath;
							returnValue = true;
							distance = _save->getPathfinding()->getPath().size();
						}
						_save->getPathfinding()->abortPath();
					}
				}
			}
		}
	}
	return returnValue;
}

/**
 * Selects a target from a list of units seen by spotter units for out-of-LOS actions and populates _attackAction with the relevant data
 * @return True if we have a target selected
 */
bool AIModule::selectSpottedUnitForSniper()
{
	_aggroTarget = 0;

	// Create a list of spotted targets and the type of attack we'd like to use on each
	std::vector<std::pair<BattleUnit*, BattleAction>> spottedTargets;

	// Get the TU costs for each available attack type
	BattleActionCost costAuto(BA_AUTOSHOT, _attackAction.actor, _attackAction.weapon);
	BattleActionCost costSnap(BA_SNAPSHOT, _attackAction.actor, _attackAction.weapon);
	BattleActionCost costAimed(BA_AIMEDSHOT, _attackAction.actor, _attackAction.weapon);

	BattleActionCost costThrow;
	// Only want to check throwing if we have a grenade, the default constructor (line above) conveniently returns false from haveTU()
	if (_grenade)
	{
		// We know we have a grenade, now we need to know if we have the TUs to throw it
		costThrow.type = BA_THROW;
		costThrow.actor = _attackAction.actor;
		costThrow.weapon = _unit->getGrenadeFromBelt(_save);
		costThrow.updateTU();
		costThrow.Time += 4; // Vanilla TUs for AI picking up grenade from belt
		costThrow += _attackAction.actor->getActionTUs(BA_PRIME, costThrow.weapon);
	}

	for (auto* bu : *_save->getUnits())
	{
		if (validTarget(bu, true, true) && bu->getTurnsLeftSpottedForSnipersByFaction(_unit->getFaction()))
		{
			// Determine which firing mode to use based on how many hits we expect per turn and the unit's intelligence/aggression
			_aggroTarget = bu;
			_attackAction.type = BA_RETHINK;
			_attackAction.target = bu->getPosition();
			extendedFireModeChoice(costAuto, costSnap, costAimed, costThrow, true);

			BattleAction chosenAction = _attackAction;
			if (chosenAction.type == BA_THROW)
				chosenAction.weapon = costThrow.weapon;

			if (_attackAction.type != BA_RETHINK)
			{
				std::pair<BattleUnit*, BattleAction> spottedTarget;
				spottedTarget = std::make_pair(bu, chosenAction);
				spottedTargets.push_back(spottedTarget);
			}
		}
	}

	int numberOfTargets = static_cast<int>(spottedTargets.size());

	if (numberOfTargets) // Now that we have a list of valid targets, pick one and return.
	{
		int pick = RNG::generate(0, numberOfTargets - 1);
		_aggroTarget = spottedTargets.at(pick).first;
		_attackAction.target = _aggroTarget->getPosition();
		_attackAction.type = spottedTargets.at(pick).second.type;
		_attackAction.weapon = spottedTargets.at(pick).second.weapon;
	}
	else // We didn't find a suitable target
	{
		// Make sure we reset anything we might have changed while testing for targets
		_aggroTarget = 0;
		_attackAction.type = BA_RETHINK;
		_attackAction.weapon = _unit->getMainHandWeapon(false);
	}

	return _aggroTarget != 0;
}

/**
 * Scores a firing mode for a particular target based on a accuracy / TUs ratio
 * @param action Pointer to the BattleAction determining the firing mode
 * @param target Pointer to the BattleUnit we're trying to target
 * @param checkLOF Set to true if you want to check for a valid line of fire
 * @return The calculated score
 */
int AIModule::scoreFiringMode(BattleAction *action, BattleUnit *target, bool checkLOF)
{
	// Sanity check first, if the passed action has no type or weapon, return 0.
	if (!action->type || !action->weapon)
	{
		return 0;
	}
	auto* weapon = action->weapon->getRules();

	// Get base accuracy for the action
	int accuracy = BattleUnit::getFiringAccuracy(BattleActionAttack::GetBeforeShoot(*action), _save->getMod());
	int distanceSq = _unit->distance3dToUnitSq(target);
	int distance = (int)std::ceil(sqrt(float(distanceSq)));

	{
		int upperLimit, lowerLimit;
		int dropoff = weapon->calculateLimits(upperLimit, lowerLimit, _save->getDepth(), action->type);

		if (distance > upperLimit)
		{
			accuracy -= (distance - upperLimit) * dropoff;
		}
		else if (distance < lowerLimit)
		{
			accuracy -= (lowerLimit - distance) * dropoff;
		}
	}

	bool outOfRange = action->type == BA_THROW
		? weapon->isOutOfThrowRange(distanceSq, _save->getDepth())
		: weapon->isOutOfRange(distanceSq);

	if (outOfRange)
	{
		accuracy = 0;
	}

	int numberOfShots = 1;
	if (action->type == BA_AIMEDSHOT)
	{
		numberOfShots = weapon->getConfigAimed()->shots;
	}
	else if (action->type == BA_SNAPSHOT)
	{
		numberOfShots = weapon->getConfigSnap()->shots;
	}
	else if (action->type == BA_AUTOSHOT)
	{
		numberOfShots = weapon->getConfigAuto()->shots;
	}

	int tuCost = _unit->getActionTUs(action->type, action->weapon).Time;
	// Need to include TU cost of getting grenade from belt + priming if we're checking throwing
	if (action->type == BA_THROW && _grenade)
	{
		// FIXME: why not just use action->weapon ?
		auto* grenadeItem = _unit->getGrenadeFromBelt(_save);
		tuCost = _unit->getActionTUs(action->type, grenadeItem).Time;
		tuCost += 4;
		tuCost += _unit->getActionTUs(BA_PRIME, grenadeItem).Time;
	}
	int tuTotal = _unit->getBaseStats()->tu;

	// Return a score of zero if this firing mode doesn't exist for this weapon
	if (!tuCost)
	{
		return 0;
	}

	if (checkLOF)
	{
		Position origin = _save->getTileEngine()->getOriginVoxel((*action), 0);
		Position targetPosition;

		if (action->weapon->getArcingShot(action->type) || action->type == BA_THROW)
		{
			targetPosition = target->getPosition().toVoxel() + Position (8,8, (2 + -target->getTile()->getTerrainLevel()));
			if (!_save->getTileEngine()->validateThrow((*action), origin, targetPosition, _save->getDepth()))
			{
				return 0;
			}
		}
		else
		{
			if (!_save->getTileEngine()->canTargetUnit(&origin, target->getTile(), &targetPosition, _unit, false, target))
			{
				return 0;
			}
		}
	}

	return accuracy * numberOfShots * tuTotal / tuCost;
}

/**
 * Selects an AI mode based on a number of factors, some RNG and the results of the rest of the determinations.
 */
void AIModule::evaluateAIMode()
{
	if ((_unit->getCharging() && _attackAction.type != BA_RETHINK))
	{
		_AIMode = AI_COMBAT;
		return;
	}
	// don't try to run away as often if we're a melee type, and really don't try to run away if we have a viable melee target, or we still have 50% or more TUs remaining.
	int escapeOdds = 15;
	if (_melee)
	{
		escapeOdds = 12;
	}
	if (_unit->getFaction() == FACTION_HOSTILE && (_unit->getTimeUnits() > _unit->getBaseStats()->tu / 2 || _unit->getCharging()))
	{
		escapeOdds = 5;
	}
	int ambushOdds = 12;
	int combatOdds = 20;
	// we're less likely to patrol if we see enemies.
	int patrolOdds = _visibleEnemies ? 15 : 30;

	// the enemy sees us, we should take retreat into consideration, and forget about patrolling for now.
	if (_spottingEnemies)
	{
		patrolOdds = 0;
		// Phase 32 (Calypso): a guard never escapes (escapeOdds is forced to 0 below), so skip
		// staging an escape route for it — it would only burn a findReachable BFS each spotted turn.
		if (_escapeTUs == 0 && !isCivilianGuard())
		{
			setupEscape();
		}
	}

	// melee/blaster units shouldn't consider ambush
	if (!_rifle || _ambushTUs == 0)
	{
		ambushOdds = 0;
		if (_melee)
		{
			combatOdds *= 1.3;
		}
	}

	// if we KNOW there are enemies around...
	if (_knownEnemies)
	{
		if (_knownEnemies == 1)
		{
			combatOdds *= 1.2;
		}

		// Phase 32 (Calypso): guards never escape (escapeOdds forced to 0 below) — don't stage one.
		if (_escapeTUs == 0 && !isCivilianGuard())
		{
			if (selectClosestKnownEnemy())
			{
				setupEscape();
			}
			else
			{
				escapeOdds = 0;
			}
		}
	}
	else if (_unit->getFaction() == FACTION_HOSTILE)
	{
		combatOdds = 0;
		escapeOdds = 0;
	}

	// take our current mode into consideration
	switch (_AIMode)
	{
	case AI_PATROL:
		patrolOdds *= 1.1;
		break;
	case AI_AMBUSH:
		ambushOdds *= 1.1;
		break;
	case AI_COMBAT:
		combatOdds *= 1.1;
		break;
	case AI_ESCAPE:
		escapeOdds *= 1.1;
		break;
	}

	// take our overall health into consideration
	if (_unit->getHealth() < _unit->getBaseStats()->health / 3)
	{
		escapeOdds *= 1.7;
		combatOdds *= 0.6;
		ambushOdds *= 0.75;
	}
	else if (_unit->getHealth() < 2 * (_unit->getBaseStats()->health / 3))
	{
		escapeOdds *= 1.4;
		combatOdds *= 0.8;
		ambushOdds *= 0.8;
	}
	else if (_unit->getHealth() < _unit->getBaseStats()->health)
	{
		escapeOdds *= 1.1;
	}

	// take our aggression into consideration
	switch (_unit->getAggression())
	{
	case 0:
		escapeOdds *= 1.4;
		combatOdds *= 0.7;
		break;
	case 1:
		ambushOdds *= 1.1;
		break;
	case 2:
		combatOdds *= 1.4;
		escapeOdds *= 0.7;
		break;
	default:
		combatOdds *= Clamp(1.2 + (_unit->getAggression() / 10.0), 0.1, 2.0);
		escapeOdds *= Clamp(0.9 - (_unit->getAggression() / 10.0), 0.1, 2.0);
		break;
	}

	if (_AIMode == AI_COMBAT)
	{
		ambushOdds *= 1.5;
	}

	// factor in the spotters.
	if (_spottingEnemies)
	{
		escapeOdds = 10 * escapeOdds * (_spottingEnemies + 10) / 100;
		combatOdds = 5 * combatOdds * (_spottingEnemies + 20) / 100;
	}
	else
	{
		escapeOdds /= 2;
	}

	// factor in visible enemies.
	if (_visibleEnemies)
	{
		combatOdds = 10 * combatOdds * (_visibleEnemies + 10) /100;
		if (_closestDist < 5)
		{
			ambushOdds = 0;
		}
	}
	// make sure we have an ambush lined up, or don't even consider it.
	if (_ambushTUs)
	{
		ambushOdds *= 1.7;
	}
	else
	{
		ambushOdds = 0;
	}

	// factor in mission type
	if (_save->getMissionType() == "STR_BASE_DEFENSE")
	{
		escapeOdds *= 0.75;
		ambushOdds *= 0.6;
	}

	// no weapons, not psychic? don't pick combat or ambush
	if (!_melee && !_rifle && !_blaster && !_grenade && _unit->getBaseStats()->psiSkill == 0)
	{
		combatOdds = 0;
		ambushOdds = 0;
	}

	// Phase 32 (Calypso): a guard protects, it does not flee. When it perceives an alien (and
	// has a weapon in hand) combat dominates; otherwise it advances to its objective via the
	// patrol move staged in setupPatrol() (rescue a crying civilian / regroup with the aquanauts).
	if (isCivilianGuard())
	{
		// A guard protects, it never flees: escapeOdds and ambushOdds are forced to 0 in every
		// case (the escape route is never even staged in think(), so any non-zero escapeOdds here
		// would only waste a turn on an empty escape action). It either stands and fights or
		// advances to its objective (the protective move staged in setupPatrol()/setupGuardMove()).
		// Key the choice on whether setupAttack actually STAGED an attack (a target it can hit this
		// turn), not merely on "an enemy is perceived/remembered": _knownEnemies persists for the
		// whole intelligence window after LOS breaks, so the old test forced AI_COMBAT → a bare
		// BA_RETHINK → the guard stood idle for several turns instead of advancing to protect. A
		// disarmed/out-of-ammo guard likewise has no staged attack → it advances rather than stalls.
		patrolOdds = 0;
		ambushOdds = 0;
		escapeOdds = 0;
		if (_attackAction.type != BA_RETHINK)
		{
			combatOdds = std::max(combatOdds, 1000);
		}
		else
		{
			// nothing it can shoot this turn: advance to rescue a crying civilian, else regroup.
			combatOdds = 0;
			patrolOdds = std::max(patrolOdds, 1000);
		}
	}
	// Phase 32: a smart civilian that perceives any alien should run for safety instead of
	// wandering. Only the brave-and-armed (aggression >= 2) hold their ground and fight.
	else if (isSmartCivilian() && (_visibleEnemies || _knownEnemies || _spottingEnemies))
	{
		patrolOdds = 0;
		bool armedBrave = (_rifle || _melee || _blaster || _grenade) && _unit->getAggression() >= 2;
		if (!armedBrave)
		{
			// timid / unarmed civilian: drop combat and let the forced escape dominate.
			combatOdds = 0;
			ambushOdds = 0;
			if (_escapeTUs == 0)
			{
				setupEscape();
			}
			if (_escapeTUs != 0)
			{
				escapeOdds = std::max(escapeOdds, 1000);
			}
		}
		// armedBrave: leave the ordinary (small) escapeOdds intact so combat dominates and the
		// "brave-and-armed hold their ground and fight" behavior actually happens (forcing
		// escapeOdds to 1000 here would make a brave civilian flee ~95% of the time).
	}

	// generate a random number to represent our decision.
	int decision = RNG::generate(1, std::max(1, patrolOdds + ambushOdds + escapeOdds + combatOdds));

	if (decision > escapeOdds)
	{
		if (decision > escapeOdds + ambushOdds)
		{
			if (decision > escapeOdds + ambushOdds + combatOdds)
			{
				_AIMode = AI_PATROL;
			}
			else
			{
				_AIMode = AI_COMBAT;
			}
		}
		else
		{
			_AIMode = AI_AMBUSH;
		}
	}
	else
	{
		_AIMode = AI_ESCAPE;
	}

	// if the aliens are cheating, or the unit is charging, enforce combat as a priority.
	if ((_unit->getFaction() == FACTION_HOSTILE && _save->isCheating()) || _unit->getCharging() != 0)
	{
		_AIMode = AI_COMBAT;
	}


	// enforce the validity of our decision, and try fallback behaviour according to priority.
	if (_AIMode == AI_COMBAT)
	{
		auto* xtile = _save->getTile(_attackAction.target);
		bool throwingGrenadeOrProxy = _attackAction.type == BA_THROW && _attackAction.weapon && _attackAction.weapon->getRules()->isGrenadeOrProxy();
		if (xtile && (xtile->getUnit() || throwingGrenadeOrProxy)) // https://openxcom.org/forum/index.php?topic=12145.0
		{
			if (_attackAction.type != BA_RETHINK)
			{
				return;
			}
			if (findFirePoint())
			{
				return;
			}
		}
		else if (selectRandomTarget() && findFirePoint())
		{
			return;
		}
		_AIMode = AI_PATROL;
	}

	if (_AIMode == AI_PATROL)
	{
		if (_toNode || _foundBaseModuleToDestroy)
		{
			return;
		}
		// base defense mission protocol: patrol action becomes an attack action when base modules are sighted
		if (_patrolAction.type == BA_SNAPSHOT)
		{
			return;
		}
		// Phase 32 (Calypso): a guard's "patrol" is its protective advance — a BA_WALK toward its
		// objective staged in setupGuardMove() (which never assigns a patrol _toNode), or a
		// hold-in-place when it is already closest. It must dispatch as patrol; never cascade a
		// guard into ambush/escape (that would freeze it on an empty escape action and, because
		// AI_ESCAPE re-evaluates every quiet turn, lock it there permanently).
		if (isCivilianGuard())
		{
			return;
		}
		_AIMode = AI_AMBUSH;
	}

	if (_AIMode == AI_AMBUSH)
	{
		if (_ambushTUs != 0)
		{
			return;
		}
		_AIMode = AI_ESCAPE;
	}
}

/**
 * Find a position where we can see our target, and move there.
 * check the 11x11 grid for a position nearby where we can potentially target him.
 * @return True if a possible position was found.
 */
bool AIModule::findFirePoint()
{
	if (!selectClosestKnownEnemy())
		return false;
	std::vector<Position> randomTileSearch = _save->getTileSearch(); // copy!
	RNG::shuffle(randomTileSearch);
	Position target;
	const int BASE_SYSTEMATIC_SUCCESS = 100;
	const int FAST_PASS_THRESHOLD = 125;
	bool waitIfOutsideWeaponRange = _unit->getGeoscapeSoldier() ? false : _unit->getUnitRules()->waitIfOutsideWeaponRange();
	bool extendedFireModeChoiceEnabled = _save->getMod()->getAIExtendedFireModeChoice();
	int bestScore = 0;
	_attackAction.type = BA_RETHINK;
	for (const auto& randomPosition : randomTileSearch)
	{
		Position pos = _unit->getPosition() + randomPosition;
		Tile *tile = _save->getTile(pos);
		if (tile == 0  ||
			std::find(_reachableWithAttack.begin(), _reachableWithAttack.end(), _save->getTileIndex(pos))  == _reachableWithAttack.end())
			continue;
		int score = 0;
		// i should really make a function for this
		Position origin = pos.toVoxel() +
			// 4 because -2 is eyes and 2 below that is the rifle (or at least that's my understanding)
			Position(8,8, _unit->getHeight() + _unit->getFloatHeight() - tile->getTerrainLevel() - 4);

		if (_save->getTileEngine()->canTargetUnit(&origin, _aggroTarget->getTile(), &target, _unit, false))
		{
			_save->getPathfinding()->calculate(_unit, pos, BAM_NORMAL);
			// can move here
			if (_save->getPathfinding()->getStartDirection() != -1)
			{
				score = BASE_SYSTEMATIC_SUCCESS - getSpottingUnits(pos) * 10;
				score += _unit->getTimeUnits() - _save->getPathfinding()->getTotalTUCost();
				if (!_aggroTarget->checkViewSector(pos))
				{
					score += 10;
				}

				// Extended behavior: if we have a limited-range weapon, bump up the score for getting closer to the target, down for further
				if (!waitIfOutsideWeaponRange && extendedFireModeChoiceEnabled)
				{
					int distanceToTargetSq = _unit->distance3dToUnitSq(_aggroTarget);
					int distanceToTarget = (int)std::ceil(sqrt(float(distanceToTargetSq)));
					if (_attackAction.weapon && _attackAction.weapon->getRules()->isOutOfRange(distanceToTargetSq)) // make sure we can get the ruleset before checking the range
					{
						int proposedDistance = Position::distance2d(pos, _aggroTarget->getPosition());
						proposedDistance = std::max(proposedDistance, 1);
						score = score * distanceToTarget / proposedDistance;
					}
				}

				if (score > bestScore)
				{
					bestScore = score;
					_attackAction.target = pos;
					_attackAction.finalFacing = _save->getTileEngine()->getDirectionTo(pos, _aggroTarget->getPosition());
					if (score > FAST_PASS_THRESHOLD)
					{
						break;
					}
				}
			}
		}
	}

	if (bestScore > 70)
	{
		_attackAction.type = BA_WALK;
		if (_traceAI)
		{
			Log(LOG_INFO) << "Firepoint found at " << _attackAction.target << ", with a score of: " << bestScore;
		}
		return true;
	}
	if (_traceAI)
	{
		Log(LOG_INFO) << "Firepoint failed, best estimation was: " << _attackAction.target << ", with a score of: " << bestScore;
	}

	return false;
}

/**
 * Decides if it worth our while to create an explosion here.
 * Return value in same range as number affected targets but not equal exactly to that value.
 * @param targetPos The target's position.
 * @param attackingUnit The attacking unit.
 * @param radius How big the explosion will be.
 * @param diff Game difficulty.
 * @param grenade Is the explosion coming from a grenade?
 * @return Value greater than zero if it is worthwhile creating an explosion in the target position. Bigger value better target.
 */
int AIModule::explosiveEfficacy(Position targetPos, BattleUnit *attackingUnit, int radius, int diff, bool grenade) const
{
	Tile *targetTile = _save->getTile(targetPos);

	// don't throw grenades at flying enemies.
	if (grenade && targetPos.z > 0 && targetTile->hasNoFloor(_save))
	{
		return false;
	}

	if (diff == -1)
	{
		diff = _save->getBattleState()->getGame()->getSavedGame()->getDifficultyCoefficient();
	}
	int distance = Position::distance2d(attackingUnit->getPosition(), targetPos);
	int injurylevel = attackingUnit->getBaseStats()->health - attackingUnit->getHealth();
	int desperation = (100 - attackingUnit->getMorale()) / 10;
	int enemiesAffected = 0;
	// if we're below 1/3 health, let's assume things are dire, and increase desperation.
	if (injurylevel > (attackingUnit->getBaseStats()->health / 3) * 2)
		desperation += 3;

	int efficacy = AIW_SCALE * desperation;

	// don't go kamikaze unless we're already doomed.
	if (abs(attackingUnit->getPosition().z - targetPos.z) <= Options::battleExplosionHeight && distance <= radius)
	{
		efficacy -= AIW_SCALE * 4;
	}

	// allow difficulty to have its influence
	efficacy += AIW_SCALE * diff/2;

	// account for the unit we're targetting
	BattleUnit *target = targetTile->getUnit();
	if (target && !targetTile->getDangerous())
	{
		++enemiesAffected;
		efficacy += getTargetAttackWeight(target);
	}

	for (auto* bu : *_save->getUnits())
	{
			// don't grenade dead guys
		if (!bu->isOut() &&
			// don't count ourself twice
			bu != attackingUnit &&
			// don't count the target twice
			bu != target &&
			// don't count units that probably won't be affected cause they're out of range
			abs(bu->getPosition().z - targetPos.z) <= Options::battleExplosionHeight &&
			Position::distance2d(bu->getPosition(), targetPos) <= radius)
		{
			if (bu->getTile()->getDangerous())
			{
				// don't count people who were already grenaded this turn
				continue;
			}

			auto weight = getTargetAttackWeight(bu);

			if (weight == 0)
			{
				// AI do not know anything about this unit
				continue;
			}

			// trace a line from the grenade origin to the unit we're checking against
			Position voxelPosA = Position (targetPos.toVoxel() + TileEngine::voxelTileCenter);
			Position voxelPosB = Position (bu->getPosition().toVoxel() + TileEngine::voxelTileCenter);
			std::vector<Position> traj;
			int collidesWith = _save->getTileEngine()->calculateLineVoxel(voxelPosA, voxelPosB, false, &traj, target, bu);

			if (collidesWith == V_UNIT && traj.front().toTile() == bu->getPosition())
			{
				if (bu->getFaction() == _targetFaction)
				{
					++enemiesAffected;
				}

				efficacy += weight;
			}
		}
	}
	// don't throw grenades at single targets, unless morale is in the danger zone
	// or we're halfway towards panicking while bleeding to death.
	if (grenade && desperation < 6 && enemiesAffected < 2)
	{
		return 0;
	}

	if (enemiesAffected >= 10)
	{
		// Ignore loses if we can kill lot of enemies.
		return enemiesAffected;
	}
	else if (efficacy > 0)
	{
		// We kill more enemies than allies. Scale back to number of targets, can round down to zero
		return efficacy / AIW_SCALE;
	}
	else
	{
		return 0;
	}
}

/**
 * Attempts to take a melee attack/charge an enemy we can see.
 * Melee targetting: we can see an enemy, we can move to it so we're charging blindly toward an enemy.
 */
void AIModule::meleeAction()
{
	BattleActionCost attackCost(BA_HIT, _unit, _unit->getUtilityWeapon(BT_MELEE));
	if (!attackCost.haveTU())
	{
		// cannot make a melee attack - consider some other behaviour, like running away, or standing motionless.
		return;
	}
	if (_aggroTarget != 0 && !_aggroTarget->isOut())
	{
		if (_save->getTileEngine()->validMeleeRange(_unit, _aggroTarget, _save->getTileEngine()->getDirectionTo(_unit->getPosition(), _aggroTarget->getPosition())))
		{
			meleeAttack();
			return;
		}
	}
	int chargeReserve = std::min(_unit->getTimeUnits() - attackCost.Time, 2 * (_unit->getEnergy() - attackCost.Energy));
	int distance = (chargeReserve / 4) + 1;
	_aggroTarget = 0;
	for (auto* bu : *_save->getUnits())
	{
		int newDistance = Position::distance2d(_unit->getPosition(), bu->getPosition());
		if (newDistance > 20 ||
			!validTarget(bu, true, true))
			continue;
		//pick closest living unit that we can move to
		if ((newDistance < distance || newDistance == 1) && !bu->isOut())
		{
			if (newDistance == 1 || selectPointNearTarget(bu, chargeReserve))
			{
				_aggroTarget = bu;
				_attackAction.type = BA_WALK;
				_unit->setCharging(_aggroTarget);
				distance = newDistance;
			}

		}
	}
	if (_aggroTarget != 0)
	{
		if (_save->getTileEngine()->validMeleeRange(_unit, _aggroTarget, _save->getTileEngine()->getDirectionTo(_unit->getPosition(), _aggroTarget->getPosition())))
		{
			meleeAttack();
		}
	}
	if (_traceAI && _aggroTarget) { Log(LOG_INFO) << "AIModule::meleeAction:" << " [target]: " << (_aggroTarget->getId()) << " at: "  << _attackAction.target; }
	if (_traceAI && _aggroTarget) { Log(LOG_INFO) << "CHARGE!"; }
}

/**
 * Attempts to take a melee attack/charge an enemy we can see.
 * Melee targetting: we can see an enemy, we can move to it so we're charging blindly toward an enemy.
 * Note: Differs from meleeAction() in calling selectPointNearTargetLeeroy() and ignoring some more checks.
 */
void AIModule::meleeActionLeeroy(bool canRun)
{
	if (_aggroTarget != 0 && !_aggroTarget->isOut())
	{
		if (_save->getTileEngine()->validMeleeRange(_unit, _aggroTarget, _save->getTileEngine()->getDirectionTo(_unit->getPosition(), _aggroTarget->getPosition())))
		{
			meleeAttack();
			return;
		}
	}
	int distance = 1000;
	_aggroTarget = 0;
	for (auto* bu : *_save->getUnits())
	{
		int newDistance = Position::distance2d(_unit->getPosition(), bu->getPosition());
		if (!validTarget(bu, true, true))
			continue;
		//pick closest living unit
		if ((newDistance < distance || newDistance == 1) && !bu->isOut())
		{
			if (newDistance == 1 || selectPointNearTargetLeeroy(bu, canRun))
			{
				_aggroTarget = bu;
				_attackAction.type = BA_WALK;
				_attackAction.run = canRun;
				_unit->setCharging(_aggroTarget);
				distance = newDistance;
			}

		}
	}
	if (_aggroTarget != 0)
	{
		if (_save->getTileEngine()->validMeleeRange(_unit, _aggroTarget, _save->getTileEngine()->getDirectionTo(_unit->getPosition(), _aggroTarget->getPosition())))
		{
			meleeAttack();
		}
	}
	if (_traceAI && _aggroTarget) { Log(LOG_INFO) << "AIModule::meleeAction:" << " [target]: " << (_aggroTarget->getId()) << " at: " << _attackAction.target; }
	if (_traceAI && _aggroTarget) { Log(LOG_INFO) << "CHARGE!"; }
}

/**
 * Attempts to fire a waypoint projectile at an enemy we, or one of our teammates sees.
 *
 * Waypoint targeting: pick from any units currently spotted by our allies.
 */
void AIModule::wayPointAction()
{
	BattleActionCost attackCost(BA_LAUNCH, _unit, _attackAction.weapon);
	if (!attackCost.haveTU())
	{
		// cannot make a launcher attack - consider some other behaviour, like running away, or standing motionless.
		return;
	}
	_aggroTarget = 0;
	for (auto* bu : *_save->getUnits())
	{
		if (_aggroTarget != 0) break; // loop finished
		if (!validTarget(bu, true, true))
		{
			continue;
		}
		_save->getPathfinding()->calculate(_unit, bu->getPosition(), BAM_MISSILE, bu, -1);
		BattleItem* ammo = _attackAction.weapon->getAmmoForAction(BA_LAUNCH);
		if (_save->getPathfinding()->getStartDirection() != -1 &&
			explosiveEfficacy(bu->getPosition(), _unit, ammo->getRules()->getExplosionRadius({ BA_LAUNCH, _unit, _attackAction.weapon, ammo }), _attackAction.diff))
		{
			_aggroTarget = bu;
		}
		_save->getPathfinding()->abortPath();
	}

	if (_aggroTarget != 0)
	{
		_attackAction.type = BA_LAUNCH;
		_attackAction.updateTU();
		if (!_attackAction.haveTU())
		{
			_attackAction.type = BA_RETHINK;
			return;
		}
		_attackAction.waypoints.clear();

		int PathDirection;
		int CollidesWith;
		int maxWaypoints = _attackAction.weapon->getCurrentWaypoints();
		if (maxWaypoints == -1)
		{
			maxWaypoints = 6 + (_attackAction.diff * 2);
		}
		Position LastWayPoint = _unit->getPosition();
		Position LastPosition = _unit->getPosition();
		Position CurrentPosition = _unit->getPosition();
		Position DirectionVector;

		_save->getPathfinding()->calculate(_unit, _aggroTarget->getPosition(), BAM_MISSILE, _aggroTarget, -1);
		PathDirection = _save->getPathfinding()->dequeuePath();
		while (PathDirection != -1 && (int)_attackAction.waypoints.size() < maxWaypoints)
		{
			LastPosition = CurrentPosition;
			_save->getPathfinding()->directionToVector(PathDirection, &DirectionVector);
			CurrentPosition = CurrentPosition + DirectionVector;
			Position voxelPosA ((CurrentPosition.x * 16)+8, (CurrentPosition.y * 16)+8, (CurrentPosition.z * 24)+16);
			Position voxelPosb ((LastWayPoint.x * 16)+8, (LastWayPoint.y * 16)+8, (LastWayPoint.z * 24)+16);
			CollidesWith = _save->getTileEngine()->calculateLineVoxel(voxelPosA, voxelPosb, false, nullptr, _unit);
			if (CollidesWith > V_EMPTY && CollidesWith < V_UNIT)
			{
				_attackAction.waypoints.push_back(LastPosition);
				LastWayPoint = LastPosition;
			}
			else if (CollidesWith == V_UNIT)
			{
				BattleUnit* target = _save->getTile(CurrentPosition)->getOverlappingUnit(_save);
				if (target == _aggroTarget)
				{
					_attackAction.waypoints.push_back(CurrentPosition);
					LastWayPoint = CurrentPosition;
				}
			}
			PathDirection = _save->getPathfinding()->dequeuePath();
		}
		_attackAction.target = _attackAction.waypoints.front();
		if (LastWayPoint != _aggroTarget->getPosition())
		{
			_attackAction.type = BA_RETHINK;
		}
	}
}

/**
 * Attempts to fire at an enemy spotted for us.
 *
 */
bool AIModule::sniperAction()
{
	if (_traceAI) { Log(LOG_INFO) << "Attempting sniper action..."; }

	if (selectSpottedUnitForSniper())
	{
		_visibleEnemies = std::max(_visibleEnemies, 1); // Make sure we count at least our target as visible, otherwise we might not shoot!

		if (_traceAI) { Log(LOG_INFO) << "Target for sniper found at (" << _attackAction.target.x << "," << _attackAction.target.y << "," << _attackAction.target.z << ")."; }
		return true;
	}

	if (_traceAI) { Log(LOG_INFO) << "No valid target found or not enough TUs for sniper action."; }
	return false;
}

/**
 * Attempts to fire at an enemy we can see.
 *
 * Regular targeting: we can see an enemy, we have a gun, let's try to shoot.
 */
void AIModule::projectileAction()
{
	_attackAction.target = _aggroTarget->getPosition();
	auto testEffect = [&](BattleActionCost& cost)
	{
		if (cost.haveTU())
		{
			BattleActionAttack attack = BattleActionAttack::GetBeforeShoot(cost);
			if (attack.damage_item == nullptr)
			{
				cost.clearTU();
			}
			else
			{
				int radius = attack.damage_item->getRules()->getExplosionRadius(attack);
				if (radius != 0 && explosiveEfficacy(_attackAction.target, _unit, radius, _attackAction.diff) == 0)
				{
					cost.clearTU();
				}
			}
		}
	};

	int distance = Position::distance2d(_unit->getPosition(), _attackAction.target);
	_attackAction.type = BA_RETHINK;

	BattleActionCost costAuto(BA_AUTOSHOT, _attackAction.actor, _attackAction.weapon);
	BattleActionCost costSnap(BA_SNAPSHOT, _attackAction.actor, _attackAction.weapon);
	BattleActionCost costAimed(BA_AIMEDSHOT, _attackAction.actor, _attackAction.weapon);

	testEffect(costAuto);
	testEffect(costSnap);
	testEffect(costAimed);

	// Is the unit willingly waiting outside of weapon's range (e.g. ninja camouflaged in ambush)?
	bool waitIfOutsideWeaponRange = _unit->getGeoscapeSoldier() ? false : _unit->getUnitRules()->waitIfOutsideWeaponRange();

	// Do we want to use the extended firing mode scoring?
	bool extendedFireModeChoiceEnabled = _save->getMod()->getAIExtendedFireModeChoice();
	if (!waitIfOutsideWeaponRange && extendedFireModeChoiceEnabled)
	{
		// Note: this will also check for the weapon's max range
		BattleActionCost costThrow; // Not actually checked here, just passed to extendedFireModeChoice as a necessary argument
		extendedFireModeChoice(costAuto, costSnap, costAimed, costThrow, false);
		return;
	}

	// Do we want to check if the weapon is in range?
	bool aiRespectsMaxRange = _save->getMod()->getAIRespectMaxRange();
	if (!waitIfOutsideWeaponRange && aiRespectsMaxRange)
	{
		// If we want to check and it's not in range, perhaps we should re-think shooting
		int distanceSq = _unit->distance3dToPositionSq(_attackAction.target);
		if (_attackAction.weapon->getRules()->isOutOfRange(distanceSq))
		{
			return;
		}
	}

	// vanilla
	if (distance < 4)
	{
		if (costAuto.haveTU())
		{
			_attackAction.type = BA_AUTOSHOT;
			return;
		}
		if (!costSnap.haveTU())
		{
			if (costAimed.haveTU())
			{
				_attackAction.type = BA_AIMEDSHOT;
			}
			return;
		}
		_attackAction.type = BA_SNAPSHOT;
		return;
	}


	if (distance > 12)
	{
		if (costAimed.haveTU())
		{
			_attackAction.type = BA_AIMEDSHOT;
			return;
		}
		if (distance < 20 && costSnap.haveTU())
		{
			_attackAction.type = BA_SNAPSHOT;
			return;
		}
	}

	if (costSnap.haveTU())
	{
		_attackAction.type = BA_SNAPSHOT;
		return;
	}
	if (costAimed.haveTU())
	{
		_attackAction.type = BA_AIMEDSHOT;
		return;
	}
	if (costAuto.haveTU())
	{
		_attackAction.type = BA_AUTOSHOT;
	}
}

void AIModule::extendedFireModeChoice(BattleActionCost& costAuto, BattleActionCost& costSnap, BattleActionCost& costAimed, BattleActionCost& costThrow, bool checkLOF)
{
	std::vector<BattleActionType> attackOptions = { };
	if (costAimed.haveTU())
	{
		attackOptions.push_back(BA_AIMEDSHOT);
	}
	if (costAuto.haveTU())
	{
		attackOptions.push_back(BA_AUTOSHOT);
	}
	if (costSnap.haveTU())
	{
		attackOptions.push_back(BA_SNAPSHOT);
	}
	if (costThrow.haveTU())
	{
		attackOptions.push_back(BA_THROW);
	}

	BattleActionType chosenAction = BA_RETHINK;
	BattleAction testAction = _attackAction;
	int score = 0;
	for (auto& i : attackOptions)
	{
		testAction.type = i;
		if (i == BA_THROW)
		{
			if (_grenade)
			{
				testAction.weapon = _unit->getGrenadeFromBelt(_save);
			}
			else
			{
				continue;
			}
		}
		else
		{
			testAction.weapon = _attackAction.weapon;
		}
		int newScore = scoreFiringMode(&testAction, _aggroTarget, checkLOF);

		// Add a random factor to the firing mode score based on intelligence
		// An intelligence value of 10 will decrease this random factor to 0
		// Default values for and intelligence value of 0 will make this a 50% to 150% roll
		int intelligenceModifier = _save->getMod()->getAIFireChoiceIntelCoeff() * std::max(10 - _unit->getIntelligence(), 0);
		newScore = newScore * (100 + RNG::generate(-intelligenceModifier, intelligenceModifier)) / 100;

		// More aggressive units get a modifier to the score for autoshots
		// Aggression = 0 lowers the score, aggro = 1 is no modifier, aggro > 1 bumps up the score by 5% (configurable) for each increment over 1
		if (i == BA_AUTOSHOT)
		{
			newScore = newScore * (100 + (_unit->getAggression() - 1) * _save->getMod()->getAIFireChoiceAggroCoeff()) / 100;

			// Phase 34.7 (Calypso): suppression-aware auto-fire (legacy path). Add the volley's
			// pinning value so volume fire is preferred over holding fire when direct hit chance
			// is poor but the target is exposed. `newScore > 0` is the exposed-target proxy here:
			// scoreFiringMode returns > 0 only when the LOF check passed and accuracy is nonzero,
			// so a no-line-of-fire shot (score 0) does NOT pick up a suppression bonus. Additive +
			// gated inside suppressionVolleyValue; flag off => +0 => byte-identical.
			if (newScore > 0)
			{
				const int suppressVal = (int)suppressionVolleyValue(_attackAction.weapon);
				newScore += suppressVal;
				// Phase 34.9 (Calypso): prefer suppressing a target a teammate has declared intent
				// to FLANK -- pin it so the flanker can close (the F.E.A.R.-style flank/suppress
				// loop). Doubles the suppression value on that target. Additive + gated; +0 when
				// off, when suppression is off (suppressVal 0), or when no teammate is flanking it.
				if (suppressVal > 0 && _save->getMod()->getAISquadCoordination() && _aggroTarget
					&& _save->getSquadHasFlankIntent(_unit->getFaction(), _aggroTarget->getId()))
				{
					newScore += suppressVal;
				}
			}
		}

		if (newScore > score)
		{
			score = newScore;
			chosenAction = i;
		}

		if (_traceAI)
		{
			Log(LOG_INFO) << "Evaluate option " << (int)i << ", score = " << newScore;
		}
	}

	_attackAction.type = chosenAction;
}

/**
 * Evaluates whether to throw a grenade at an enemy (or group of enemies) we can see.
 */
void AIModule::grenadeAction()
{
	// do we have a grenade on our belt?
	BattleItem *grenade = _unit->getGrenadeFromBelt(_save);
	BattleAction action;
	action.weapon = grenade;
	action.type = BA_THROW;
	action.actor = _unit;

	action.updateTU();
	action.Time += 4; // 4TUs for picking up the grenade
	action += _unit->getActionTUs(BA_PRIME, grenade);
	// do we have enough TUs to prime and throw the grenade?
	if (action.haveTU())
	{
		int radius = grenade->getRules()->getExplosionRadius(BattleActionAttack::GetBeforeShoot(action));
		if (explosiveEfficacy(_aggroTarget->getPosition(), _unit, radius, _attackAction.diff, true))
		{
			action.target = _aggroTarget->getPosition();
		}
		else if (!getNodeOfBestEfficacy(&action, radius))
		{
			return;
		}
		std::vector<std::pair<Position, int>> shifts;
		if (grenade->getRules()->getBattleType() == BT_PROXIMITYGRENADE)
		{
			// let's try to not throw the proxy below xcom's feet, otherwise they'll just throw it straight back :)
			if (action.target.x < _save->getMapSizeX() - 1) shifts.push_back(std::make_pair(Position(1, 0, 0), _unit->distance3dToPositionSq(action.target + Position(1, 0, 0))));
			if (action.target.y < _save->getMapSizeY() - 1) shifts.push_back(std::make_pair(Position(0, 1, 0), _unit->distance3dToPositionSq(action.target + Position(0, 1, 0))));
			if (action.target.x > 0) shifts.push_back(std::make_pair(Position(-1, 0, 0), _unit->distance3dToPositionSq(action.target + Position(-1, 0, 0))));
			if (action.target.y > 0) shifts.push_back(std::make_pair(Position(0, -1, 0), _unit->distance3dToPositionSq(action.target + Position(0, -1, 0))));
			//RNG::shuffle(shifts);
			std::sort(shifts.begin(), shifts.end(), [](auto& left, auto& right) {
				return left.second < right.second;
			});
			// PS: if someone wants to calculate a better target spot (based on multiple enemies, RNG, day of the week or position of the stars), be my guest
		}
		else
		{
			// normal grenade
			shifts.push_back(std::make_pair(Position(0, 0, 0), 0));
		}
		Position originVoxel = _save->getTileEngine()->getOriginVoxel(action, 0);
		for (auto& shift : shifts)
		{
			Position targetTile = action.target + shift.first;
			Position targetVoxel = targetTile.toVoxel() + Position(8,8, (2 + -_save->getTile(targetTile)->getTerrainLevel()));
			// are we within range?
			if (_save->getTileEngine()->validateThrow(action, originVoxel, targetVoxel, _save->getDepth()))
			{
				_attackAction.weapon = grenade;
				_attackAction.target = targetTile;
				_attackAction.type = BA_THROW;
				_rifle = false;
				_melee = false;
				break;
			}
		}
	}
}

/**
 * Attempts a psionic attack on an enemy we "know of".
 *
 * Psionic targetting: pick from any of the "exposed" units.
 * Exposed means they have been previously spotted, and are therefore "known" to the AI,
 * regardless of whether we can see them or not, because we're psychic.
 * @return True if a psionic attack is performed.
 */
bool AIModule::psiAction()
{
	BattleItem *item = _unit->getUtilityWeapon(BT_PSIAMP);
	if (!item)
	{
		return false;
	}

	const int costLength = 3;
	BattleActionCost cost[costLength] =
	{
		BattleActionCost(BA_USE, _unit, item),
		BattleActionCost(BA_PANIC, _unit, item),
		BattleActionCost(BA_MINDCONTROL, _unit, item),
	};
	bool have = false;
	for (int j = 0; j < costLength; ++j)
	{
		if (cost[j].Time > 0)
		{
			cost[j].Time += _escapeTUs;
			cost[j].Energy += _escapeTUs / 2;
			have |= cost[j].haveTU();
		}
	}
	bool LOSRequired = item->getRules()->isLOSRequired();

	_aggroTarget = 0;
		// don't let mind controlled soldiers mind control other soldiers.
	if (_unit->getOriginalFaction() == _unit->getFaction()
		// and we have the required 25 TUs and can still make it to cover
		&& have
		// and we didn't already do a psi action this round
		&& !_didPsi)
	{
		int weightToAttack = 0;
		BattleActionType typeToAttack = BA_NONE;

		for (auto* bu : *_save->getUnits())
		{
			// don't target tanks
			if (bu->getArmor()->getSize() == 1 &&
				validTarget(bu, true, false) &&
				// they must be player units
				bu->getOriginalFaction() != _unit->getFaction() &&
				(!LOSRequired ||
				std::find(_unit->getVisibleUnits()->begin(), _unit->getVisibleUnits()->end(), bu) != _unit->getVisibleUnits()->end()))
			{
				BattleUnit *victim = bu;
				if (item->getRules()->isOutOfRange(_unit->distance3dToUnitSq(victim)))
				{
					continue;
				}
				for (int j = 0; j < costLength; ++j)
				{
					// can't use this attack.
					if (!cost[j].haveTU())
					{
						continue;
					}

					int weightToAttackMe = _save->getTileEngine()->psiAttackCalculate({ cost[j].type, _unit, item, item }, victim);

					// low chance we hit this target.
					if (weightToAttackMe < 0)
					{
						continue;
					}

					// different bonus per attack.
					if (cost[j].type == BA_MINDCONTROL)
					{
						// target cannot be mind controlled
						if (victim->getUnitRules() && !victim->getUnitRules()->canBeMindControlled()) continue;

						int controlOdds = 40;
						int morale = victim->getMorale();
						int bravery = victim->reduceByBravery(10);
						if (bravery > 6)
							controlOdds -= 15;
						if (bravery < 4)
							controlOdds += 15;
						if (morale >= 40)
						{
							if (morale - 10 * bravery < 50)
								controlOdds -= 15;
						}
						else
						{
							controlOdds += 15;
						}
						if (!morale)
						{
							controlOdds = 100;
						}
						if (RNG::percent(controlOdds))
						{
							weightToAttackMe += 60;
						}
						else
						{
							continue;
						}
					}
					else if (cost[j].type == BA_USE)
					{
						if (RNG::percent(80 - _attackAction.diff * 10)) // Star gods have mercy on us.
						{
							continue;
						}
						BattleActionAttack attack = BattleActionAttack{ BA_USE, _unit, item, item };
						int radius = item->getRules()->getExplosionRadius(attack);
						if (radius > 0)
						{
							int efficity = explosiveEfficacy(victim->getPosition(), _unit, radius, _attackAction.diff);
							if (efficity)
							{
								weightToAttackMe += 2 * efficity * _intelligence; //bonus for boom boom.
							}
							else
							{
								continue;
							}
						}
						else
						{
							weightToAttackMe += item->getRules()->getPowerBonus(attack);
						}
					}
					else if (cost[j].type == BA_PANIC)
					{
						// target cannot be panicked
						if (victim->getUnitRules() && !victim->getUnitRules()->canPanic()) continue;

						weightToAttackMe += 40;
					}

					if (weightToAttackMe > weightToAttack)
					{
						typeToAttack = cost[j].type;
						weightToAttack = weightToAttackMe;
						_aggroTarget = victim;
					}
				}
			}
		}

		if (!_aggroTarget || !weightToAttack) return false;

		if (_visibleEnemies && _attackAction.weapon)
		{
			BattleActionType actions[] = {
				BA_AIMEDSHOT,
				BA_AUTOSHOT,
				BA_SNAPSHOT,
				BA_HIT,
			};
			for (BattleActionType action : actions)
			{
				auto* ammo = _attackAction.weapon->getAmmoForAction(action);
				if (!ammo)
				{
					continue;
				}

				int weightPower = ammo->getRules()->getPowerBonus({ action, _attackAction.actor, _attackAction.weapon, ammo });
				if (action == BA_HIT)
				{
					// prefer psi over melee
					weightPower /= 2;
				}
				else
				{
					// prefer machine guns
					weightPower *= _attackAction.weapon->getActionConf(action)->shots;
				}
				if (weightPower >= weightToAttack)
				{
					return false;
				}
			}
		}
		else if (RNG::generate(35, 155) >= weightToAttack)
		{
			return false;
		}

		if (_traceAI)
		{
			Log(LOG_INFO) << "making a psionic attack this turn";
		}

		_psiAction.type = typeToAttack;
		_psiAction.target = _aggroTarget->getPosition();
		_psiAction.weapon = item;
		return true;
	}
	return false;
}

/**
 * Performs a melee attack action.
 */
void AIModule::meleeAttack()
{
	_unit->lookAt(_aggroTarget->getPosition() + Position(_unit->getArmor()->getSize()-1, _unit->getArmor()->getSize()-1, 0), false);
	while (_unit->getStatus() == STATUS_TURNING)
		_unit->turn();
	if (_traceAI) { Log(LOG_INFO) << "Attack unit: " << _aggroTarget->getId(); }
	_attackAction.target = _aggroTarget->getPosition();
	_attackAction.type = BA_HIT;
	_attackAction.weapon = _unit->getUtilityWeapon(BT_MELEE);
}


/**
 *
 * @param target
 * @return
 */
AIAttackWeight AIModule::getTargetAttackWeight(BattleUnit* target) const
{
	AIAttackWeight weight = AIW_IGNORED;

	if (target->getFaction() == _unit->getFaction())
	{
		// friendly target have negative weight, used for AoE attacks.
		weight = target->getAITargetWeightAsFriendly(_save->getMod());
	}
	else if (
		_intelligence < target->getTurnsSinceSpottedByFaction(_unit->getFaction()) &&
		(!_unit->isSniper() || !target->getTurnsLeftSpottedForSnipersByFaction(_unit->getFaction())))
	{
		// ignore units that we don't "know" about...
		// ... unless we are a sniper and the spotters know about them
		weight = AIW_IGNORED;
	}
	else if (target->getFaction() == FACTION_HOSTILE || _unit->getFaction() == FACTION_HOSTILE)
	{
		if (target->getFaction() == _targetFaction)
		{
			// enemy unit, full weight
			weight = target->getAITargetWeightAsHostile(_save->getMod());
		}
		else
		{
			// if its not xcom unit then its civilian, less value that xcom
			weight = target->getAITargetWeightAsHostileCivilians(_save->getMod());
		}
	}
	else if (target->getFaction() == FACTION_NEUTRAL || _unit->getFaction() == FACTION_NEUTRAL)
	{
		// if its not alien then its xcom or civilian, humans do not shoot each other, usually...
		weight = target->getAITargetWeightAsNeutral(_save->getMod());
	}

	weight = (AIAttackWeight)ModScript::scriptFunc2<ModScript::AiCalculateTargetWeight>(
		_unit->getArmor(),
		weight, weight,
		_unit, target, _save
	);

	// Phase 34.9 (Calypso): pin-and-flank (legacy path). There is no distinct "flanking move" seam
	// in either AI path (grep 'flank' -> 0 hits; the brutal AI uses cover-quality scoring, the
	// legacy AI none), so the plan's "a unit choosing a flanking move prefers pinned targets" is
	// realized as a target preference: an enemy pinned by suppression fire (34.7) is the one to
	// press, so it gets a small weight bonus in selection. Guarded on weight > AIW_IGNORED so it
	// only nudges already-known targets (never resurrects an ignored one). Hostile-only, like every
	// sibling 34.9 hook: getTargetAttackWeight is reachable by civilian AI (Phase 32), which must
	// stay on Phase 32 logic and never enter squad logic. Gated; +0 off => identical.
	if (_save->getMod()->getAISquadCoordination()
		&& _unit->getFaction() == FACTION_HOSTILE
		&& weight > AIW_IGNORED
		&& target->getFaction() != _unit->getFaction()
		&& target->isPinned())
	{
		weight = (AIAttackWeight)(weight + AIW_SCALE / 2);
	}

	// Phase 34.9 (Calypso): soft focus-fire cap (legacy path). A target that >= focusFireCommitThreshold
	// squadmates have already committed to is down-weighted so a fresh target outscores it and fire
	// spreads -- but the floor (just above the threat threshold) keeps it a VALID target, so if it is
	// the only option it is still engaged ("unless no alternative exists"). The reduction never
	// increases the weight (weight*keepPercent/100 < weight for positive weights, keepPercent<=100).
	// Gated; the board reads empty when off, so the value is unchanged => byte-identical. Enemy
	// targets only (friendly-AoE weights are non-positive and already below the floor).
	if (_save->getMod()->getAISquadCoordination()
		&& target->getFaction() != _unit->getFaction())
	{
		const AIAttackWeight floor = (AIAttackWeight)(_save->getMod()->getAITargetWeightThreatThreshold() + 1);
		// Phase 43.0 item 7: commit count + reduction percent are now mod-tunable (defaults 2 / 50).
		const int commitThreshold = _save->getMod()->getAIFocusFireCommitThreshold();
		const int keepPercent = _save->getMod()->getAIFocusFireScorePercent();
		if (weight > floor
			&& _save->getSquadAssignedAttackers(_unit->getFaction(), target->getId()) >= commitThreshold)
		{
			weight = std::max(floor, (AIAttackWeight)AITuning::applyPercent(static_cast<int>(weight), keepPercent));
		}
	}

	return weight;
}

/**
 * Validates a target.
 * @param target the target we want to validate.
 * @param assessDanger do we care if this unit was previously targetted with a grenade?
 * @param includeCivs do we include civilians in the threat assessment?
 * @return whether this target is someone we would like to kill.
 */
bool AIModule::validTarget(BattleUnit *target, bool assessDanger, bool includeCivs) const
{
	// ignore units that:
	// 1. are dead/unconscious
	// 2. are dangerous (they have been grenaded)
	// 3. are hostile/neutral units marked as ignored by the AI
	if (target->isOut() ||
		(assessDanger && target->getTile()->getDangerous()) ||
		(target->getFaction() != FACTION_PLAYER && target->isIgnoredByAI()))
	{
		return false;
	}

	if (includeCivs)
	{
		return  getTargetAttackWeight(target) > AIW_IGNORED;
	}
	else
	{
		return  getTargetAttackWeight(target) > _save->getMod()->getAITargetWeightThreatThreshold();
	}
}

/**
 * Checks the alien's reservation setting.
 * @return the reserve setting.
 */
BattleActionType AIModule::getReserveMode()
{
	return _reserve;
}

/**
 * We have a dichotomy on our hands: we have a ranged weapon and melee capability.
 * let's make a determination on which one we'll be using this round.
 */
void AIModule::selectMeleeOrRanged()
{
	BattleItem *range = _attackAction.weapon;
	BattleItem *melee = _unit->getUtilityWeapon(BT_MELEE);

	if (!melee || !melee->haveAnyAmmo())
	{
		// no idea how we got here, but melee is definitely out of the question.
		_melee = false;
		return;
	}
	if (!range || !range->haveAnyAmmo())
	{
		_rifle = false;
		return;
	}

	const RuleItem *meleeRule = melee->getRules();

	int meleeOdds = 10;

	int dmg = _aggroTarget->reduceByResistance(meleeRule->getPowerBonus(BattleActionAttack::GetBeforeShoot(BA_HIT, _unit, melee)), meleeRule->getDamageType()->ResistType);

	if (dmg > 50)
	{
		meleeOdds += (dmg - 50) / 2;
	}
	if ( _visibleEnemies > 1 )
	{
		meleeOdds -= 20 * (_visibleEnemies - 1);
	}

	if (meleeOdds > 0 && _unit->getHealth() >= 2 * _unit->getBaseStats()->health / 3)
	{
		if (_unit->getAggression() == 0)
		{
			meleeOdds -= 20;
		}
		else if (_unit->getAggression() > 1)
		{
			meleeOdds += 10 * _unit->getAggression();
		}

		if (RNG::percent(meleeOdds))
		{
			_rifle = false;
			_attackAction.weapon = melee;
			bool ranOutOfTUsMelee = false; // Brutal-AI: findReachable out-param (unused here)
			_reachableWithAttack = _save->getPathfinding()->findReachable(_unit, BattleActionCost(BA_HIT, _unit, melee), ranOutOfTUsMelee);
			return;
		}
	}
	_melee = false;
}

/**
 * Checks nearby nodes to see if they'd make good grenade targets
 * @param action contains our details one weapon and user, and we set the target for it here.
 * @return if we found a viable node or not.
 */
bool AIModule::getNodeOfBestEfficacy(BattleAction *action, int radius)
{
	int bestScore = 2;
	Position originVoxel = _save->getTileEngine()->getSightOriginVoxel(_unit);
	Position targetVoxel;
	for (const auto* node : *_save->getNodes())
	{
		if (node->isDummy())
		{
			continue;
		}
		int dist = Position::distance2d(node->getPosition(), _unit->getPosition());
		if (dist <= 20 && dist > radius &&
			_save->getTileEngine()->canTargetTile(&originVoxel, _save->getTile(node->getPosition()), O_FLOOR, &targetVoxel, _unit, false))
		{
			int nodePoints = 0;
			for (auto* bu : *_save->getUnits())
			{
				dist = Position::distance2d(node->getPosition(), bu->getPosition());
				if (!bu->isOut() && dist < radius)
				{
					Position targetOriginVoxel = _save->getTileEngine()->getSightOriginVoxel(bu);
					if (_save->getTileEngine()->canTargetTile(&targetOriginVoxel, _save->getTile(node->getPosition()), O_FLOOR, &targetVoxel, bu, false))
					{
						if ((_unit->getFaction() == FACTION_HOSTILE && bu->getFaction() != FACTION_HOSTILE) ||
							(_unit->getFaction() == FACTION_NEUTRAL && bu->getFaction() == FACTION_HOSTILE))
						{
							if (bu->getTurnsSinceSpottedByFaction(_unit->getFaction()) <= _intelligence)
							{
								nodePoints++;
							}
						}
						else
						{
							nodePoints -= 2;
						}
					}
				}
			}
			if (nodePoints > bestScore)
			{
				bestScore = nodePoints;
				action->target = node->getPosition();
			}
		}
	}
	return bestScore > 2;
}

BattleUnit* AIModule::getTarget()
{
	return _aggroTarget;
}

void AIModule::freePatrolTarget()
{
	if (_toNode)
	{
		_toNode->freeNode();
	}
}


// ===== Brutal-AI ported methods (adapted from Brutal-OXCE by Xilmi, github.com/Xilmi/OpenXcom) =====

bool AIModule::visibleToAnyFriend(BattleUnit* target) const
{
	// The player is at a disadvantage as per the engine and can't directly target units he can't see. So autoplay must be aware of this disadvantage.
	if (_unit->getFaction() == FACTION_PLAYER)
		return target->getVisible();
	else
		return target->getTurnsSinceSeen(_unit->getFaction()) == 0;
}

void AIModule::brutalThink(BattleAction* action)
{
	const bool measureEvalBudget = _save->getMod()->getAISharedFields()
		&& _save->getMod()->getAITurnBudgetMs() > 0;
	AITimingScope _aiTiming(_unit->getId(), measureEvalBudget);
	// Step 1: Check whether we wait for someone else on our team to move first
	int myReachable = getReachableBy(_unit, _ranOutOfTUs, true).size();
	float myDist = 0;
	bool IAmMindControlled = false;
	if (_unit->getFaction() != _unit->getOriginalFaction())
		IAmMindControlled = true;
	Position myPos = _unit->getPosition();
	Tile* myTile = _save->getTile(myPos);

	// Detect if I am blocking a door
	bool iAmBlockingDoor = _save->getTileEngine()->isNextToDoor(myTile);

	// Only calculate distances if I'm not in a doorway.
	// If I am in a doorway, priority is strictly based on position, not distance/reachability.
	if (!iAmBlockingDoor)
	{
		for (BattleUnit* enemy : *(_save->getUnits()))
		{
			if (enemy->getMainHandWeapon() == NULL || enemy->isOut() || enemy->getFaction() == _unit->getFaction())
				continue;
			Position enemyPos = enemy->getPosition();
			if (!_unit->isCheatOnMovement())
			{
				enemyPos = _save->getTileCoords(enemy->getTileLastSpotted(_unit->getFaction()));
			}
			if (_unit->hasVisibleUnit(enemy))
			{
				myDist = 0;
				break;
			}
			myDist += Position::distance(myPos, enemyPos);
		}
	}

	for (BattleUnit* ally : *(_save->getUnits()))
	{
		if (ally == _unit)
			continue;
		if (ally->isOut())
			continue;
		if (ally->getFaction() != _unit->getFaction())
			continue;
		if (!ally->reselectAllowed() || !ally->isSelectable(_unit->getFaction(), false, false))
			continue;
		if (!ally->isAIControlled())
			continue;

		// --- FIX START: Doorway Priority Logic ---
		bool allyBlockingDoor = _save->getTileEngine()->isNextToDoor(ally->getTile());

		if (iAmBlockingDoor)
		{
			// If I am in a doorway, I have absolute priority to clear the choke point.
			// I will NOT wait for any ally, regardless of their reachability or distance.
			// We skip checking this ally and proceed to our move.
			continue;
		}

		if (allyBlockingDoor)
		{
			// If I am NOT in a doorway, but an ally IS, they must move first.
			// We pass the turn immediately without calculating reachability.
			action->type = BA_WAIT;
			action->number -= 1;
			_save->getBattleGame()->setNextUnitToSelect(ally);
			if (Options::traceAI)
			{
				Log(LOG_INFO) << "#" << _unit->getId() << " waits for " << ally->getId() << " because ally is in a doorway.";
			}
			return;
		}
		// --- FIX END ---

		int allyReachable = 0;
		bool allyRanOutOfTUs = false;
		float allyDist = 0;
		bool allyIsMindControlled = false;
		if (ally->getFaction() != ally->getOriginalFaction())
			allyIsMindControlled = true;

		// We no longer need the door check inside this calculation block
		for (BattleUnit* enemy : *(_save->getUnits()))
		{
			if (enemy->getMainHandWeapon() == NULL || enemy->isOut() || enemy->getFaction() == _unit->getFaction())
				continue;
			Position enemyPos = enemy->getPosition();
			if (!_unit->isCheatOnMovement())
			{
				enemyPos = _save->getTileCoords(enemy->getTileLastSpotted(ally->getFaction()));
			}
			if (ally->hasVisibleUnit(enemy))
			{
				allyDist = 0;
				break;
			}
			allyDist += Position::distance(ally->getPosition(), enemyPos);
		}

		allyReachable = getReachableBy(ally, allyRanOutOfTUs).size();
		if (_ranOutOfTUs == false)
		{
			if (myReachable < allyReachable)
			{
				action->type = BA_WAIT;
				action->number -= 1;
				_save->getBattleGame()->setNextUnitToSelect(ally);
				// if (Options::traceAI)
				//{
				//	Log(LOG_INFO) << "#" << _unit->getId() << " with myReachable: " << myReachable << " and " << myDist << " wants " << ally->getId() << " with allyReachable: " << allyReachable << " and " << allyDist << " to move next.";
				// }
				return;
			}
		}
		else if (_ranOutOfTUs == true && allyRanOutOfTUs == true)
		{
			if (myDist > allyDist)
			{
				action->type = BA_WAIT;
				action->number -= 1;
				_save->getBattleGame()->setNextUnitToSelect(ally);
				// if (Options::traceAI)
				//{
				//	Log(LOG_INFO) << "#" << _unit->getId() << " with myReachable: " << myReachable << " and " << myDist << " wants " << ally->getId() << " with allyReachable: " << allyReachable << " and " << allyDist << " to move next.";
				// }
				return;
			}
		}
	}

	// Create reachabiliy and turncost-list for the entire map
	if (_traceAI)
	{
		Log(LOG_INFO) << "#" << _unit->getId() << "--" << _unit->getType() << " TU: " << _unit->getTimeUnits() << "/" << _unit->getBaseStats()->tu << " Position: " << myPos << " Direction: " << _unit->getDirection() << " Turn: " << _save->getTurn();
	}

	if (_tuWhenChecking != _unit->getTimeUnits())
	{
		_tuCostToReachClosestPositionToBreakLos = -1;
		_energyCostToReachClosestPositionToBreakLos = -1;
	}

	bool IAmPureMelee = _melee && !_blaster && !_rifle && !_grenade;
	if (_unit->getMainHandWeapon() && _unit->getMainHandWeapon()->getRules()->getBattleType() == BT_MELEE)
		IAmPureMelee = true;
	if (IAmPureMelee)
		_attackAction.weapon = _unit->getUtilityWeapon(BT_MELEE);

	bool dummy = false;
	BattleActionMove bam = BAM_NORMAL;
	if (Options::strafe && wantToRun())
		bam = BAM_RUN;
	_allPathFindingNodes = _save->getPathfinding()->findReachablePathFindingNodes(_unit, BattleActionCost(), dummy, true, NULL, NULL, false, false, bam);
	BattleUnit* unitToFaceTo = NULL;

	float shortestDist = FLT_MAX;
	int shortestWalkingPath = INT_MAX;
	BattleUnit* unitToWalkTo = NULL;
	bool amInLoSToFurthestReachable = false;
	bool contact = _unit->getTurnsSinceSeen(_targetFaction) == 0;

	Position furthestPositionEnemyCanReach = myPos;
	float closestDistanceofFurthestPosition = FLT_MAX;
	bool immobile = false;
	// Check if I'm a turret. In this case I can skip everything about walking
	if (!_unit->getArmor()->allowsMoving() || _unit->getEnergy() == 0)
	{
		immobile = true;
		_allowedToCheckAttack = true;
	}
	float targetDistanceTofurthestReach = FLT_MAX;
	std::map<Position, int, PositionComparator> enemyReachable;
	std::map<Position, int, PositionComparator> friendReachable;
	std::map<Position, int, PositionComparator> bestFriendReachable;
	bool immobileEnemies = false;

	Position visibleFromPosition = myPos;
	bool visibleToEnemy = false;
	bool enemyFarAwayFromStart = false;
	float damagePotentialFromCurrentPosition = 0;
	// Phase 43.1E (Calypso): when a shared friendReachable field is available we skip the
	// legacy per-ally local-map merge below (the field already holds every ally's contribution);
	// when it is null we run the original local-map code byte-for-byte. Enemy handling is unchanged.
	FriendReachableField* sharedField = prepareSharedFriendReachable();
	bool forceEnemyReachability = false;
	if (_save->getMod()->getAISharedFields())
	{
		const FactionTurnCache* threatCache = _save->getFactionTurnCache(_unit->getFaction());
		forceEnemyReachability = threatCache != nullptr && threatCache->isValid() && threatCache->isThreatDirty();
	}
	for (BattleUnit* target : *(_save->getUnits()))
	{
		if (target->isOut())
			continue;
		if (isAlly(target))
		{
			if (sharedField == nullptr)
			{
				if (target != _unit)
				{
					_save->getPathfinding()->setIgnoreFriends(true);
					for (const auto& reachablePosOfTarget : getReachableBy(target, _ranOutOfTUs, false, true))
					{
						friendReachable[reachablePosOfTarget.first] += reachablePosOfTarget.second;
						bestFriendReachable[reachablePosOfTarget.first] = std::max(bestFriendReachable[reachablePosOfTarget.first], reachablePosOfTarget.second);
					}
					_save->getPathfinding()->setIgnoreFriends(false);
				}
			}
		}
		Position targetPosition = target->getPosition();
		if (!_unit->isCheatOnMovement() && isEnemy(target))
		{
			if (target->getTileLastSpotted(_unit->getFaction()) == -1)
			{
				const int spawnIndex = getClosestSpawnTileId();
				target->setTileLastSpotted(spawnIndex, _unit->getFaction());
				if (spawnIndex >= 0)
					_save->notifyFactionTurnKnowledgeChanged(_unit->getFaction(), target, _save->getTileCoords(spawnIndex));
			}
			if (target->getTileLastSpotted(_unit->getFaction()) == -1)
				continue;
		}
		// Seems redundant but isn't. This is necessary because we also don't want to attack the units that we have mind-controlled
		if (!isEnemy(target))
			continue;
		if (brutalValidTarget(target))
			damagePotentialFromCurrentPosition = std::max(damagePotential(myPos, target, _unit->getTimeUnits(), _unit->getEnergy()), damagePotentialFromCurrentPosition);
		for (BattleUnit* visble : *target->getVisibleUnits())
		{
			if (visble == _unit)
			{
				visibleToEnemy = true;
				visibleFromPosition = target->getPosition();
				break;
			}
		}
		if (!target->getArmor()->allowsMoving() || target->getBaseStats()->stamina == 0)
			immobileEnemies = true;
		int turnsLastSeen = 0;
		if (!_unit->isCheatOnMovement() && !visibleToAnyFriend(target))
		{
			turnsLastSeen = target->getTurnsSinceSeen(_unit->getFaction());
			targetPosition = _save->getTileCoords(target->getTileLastSpotted(_unit->getFaction()));
			Tile* targetTile = _save->getTile(targetPosition);
			bool tileChecked = false;
			if (targetTile == 0) continue; // Calypso: guard against null tile (OOB last-spotted position)
			if (targetTile->getLastExplored(_unit->getFaction()) == _save->getTurn() && targetTile->getSmoke() == 0)
				tileChecked = true;
			else if (targetTile->getUnit() && targetTile->getUnit()->getFaction() == _unit->getFaction())
				tileChecked = true;
			else
			{
				for (BattleUnit* ally : *(_save->getUnits()))
				{
					if (ally->isOut())
						continue;
					if (ally->getFaction() != _unit->getFaction())
						continue;
					float avgSmoke = (targetTile->getSmoke() + ally->getTile()->getSmoke()) / 2.0;
					float minViewDistance = _save->getMod()->getMaxViewDistance() / (1.0 + avgSmoke / 3.0);
					if (targetTile->getShade() > _save->getMod()->getMaxDarknessToSeeUnits() && targetTile->getFire() == 0)
						minViewDistance = std::min((float)ally->getMaxViewDistanceAtDark(target), minViewDistance);
					if (targetTile->getLastExplored(_unit->getFaction()) == _save->getTurn() && Position::distance(targetPosition, ally->getPosition()) <= minViewDistance)
					{
						tileChecked = true;
						break;
					}
				}
			}
			if (tileChecked && targetTile->getUnit() && targetTile->getUnit()->getFaction() == _targetFaction && !visibleToAnyFriend(targetTile->getUnit()))
				tileChecked = false;
			// if (_traceAI)
			//	Log(LOG_INFO) << "Assuming unit at " << target->getPosition() << " to be at " << targetPosition << " checked: " << tileChecked << " target-tile last explored: " << targetTile->getLastExplored(_unit->getFaction()) << " current turn: " << _save->getTurn() << " smoke: " << targetTile->getSmoke() << " turns since seen: " << target->getTurnsSinceSeen(_unit->getFaction());
			if (tileChecked)
			{
				int newIndex = getNewTileIDToLookForEnemy(targetPosition, target);
				if (_traceAI)
				{
					Log(LOG_INFO) << "Target " << target->getPosition() << " is no longer where it is suspected at: " << targetPosition << " Guess for new position is: " << _save->getTileCoords(newIndex);
					//_save->getTile(newIndex)->setMarkerColor(_unit->getId());
					//_save->getTile(newIndex)->setPreview(10);
					//_save->getTile(newIndex)->setTUMarker(target->getId());
				}
				target->setTileLastSpotted(newIndex, _unit->getFaction());
				if (newIndex >= 0)
					_save->notifyFactionTurnKnowledgeChanged(_unit->getFaction(), target, _save->getTileCoords(newIndex));
				// We clear it for blind-shot in this case, as it makes no sense to still try and shoot there
				target->setTileLastSpotted(-1, _unit->getFaction(), true);
				if (newIndex == -1)
					continue;
			}
		}
		bool isFarAwayFromStart = true;
		if (!target->hasPanickedLastTurn())
		{
			_save->getPathfinding()->setIgnoreFriends(true);
			for (const auto& reachablePosOfTarget : getReachableBy(target, _ranOutOfTUs, forceEnemyReachability, true, false))
			{
				Tile* checkStartTile = _save->getTile(reachablePosOfTarget.first);
				if (checkStartTile->getFloorSpecialTileType() == START_POINT)
					isFarAwayFromStart = false;
				enemyReachable[reachablePosOfTarget.first] += reachablePosOfTarget.second;
			}
			_save->getPathfinding()->setIgnoreFriends(false);
		}
		BattleUnit* LoFCheckUnitForPath = NULL;
		if (_unit->isCheatOnMovement())
			LoFCheckUnitForPath = target;
		int currentWalkPath = tuCostToReachPosition(targetPosition, _allPathFindingNodes) + turnsLastSeen * getMaxTU(_unit);
		Position posUnitCouldReach = closestPositionEnemyCouldReach(target);
		float distToPosUnitCouldReach = Position::distance(myPos, posUnitCouldReach);
		if (distToPosUnitCouldReach < closestDistanceofFurthestPosition)
		{
			furthestPositionEnemyCanReach = posUnitCouldReach;
			closestDistanceofFurthestPosition = distToPosUnitCouldReach;
			targetDistanceTofurthestReach = Position::distance(posUnitCouldReach, targetPosition);
		}
		if (currentWalkPath < shortestWalkingPath)
		{
			shortestWalkingPath = currentWalkPath;
			unitToWalkTo = target;
			enemyFarAwayFromStart = isFarAwayFromStart;
		}
	}
	int myMaxTU = getMaxTU(_unit);
	//Log(LOG_INFO) << "friendReachable[myPos]: " << friendReachable[myPos]
	//			  << " myMaxTU: " << myMaxTU;
	int weaponRange = maxExtenderRangeWith(_unit, getMaxTU(_unit));
	bool sweepMode = _unit->isLeeroyJenkins() || immobile;
	_unit->setCharging(nullptr);

	// Phase 1: Check if you can attack anything from where you currently are
	_attackAction.type = BA_RETHINK;
	_psiAction.type = BA_NONE;
	bool checkedAttack = false;
	if (_unit->getTimeUnits() == getMaxTU(_unit))
	{
		_positionAtStartOfTurn = myPos;
		if (damagePotentialFromCurrentPosition == 0 && !immobile)
			_allowedToCheckAttack = false;
	}

	if (_allowedToCheckAttack || _blaster || _unit->getUtilityWeapon(BT_PSIAMP) != nullptr || IAmPureMelee)
	{
		checkedAttack = true;
		if (brutalPsiAction())
		{
			if (_psiAction.type != BA_NONE)
			{
				action->type = _psiAction.type;
				action->target = _psiAction.target;
				action->number -= 1;
				action->weapon = _psiAction.weapon;
				action->updateTU();
				return;
			}
		}
		brutalBlaster();
		if (_allowedToCheckAttack)
		{
			if (_attackAction.type == BA_RETHINK)
				brutalSelectSpottedUnitForSniper();
			if (_attackAction.type == BA_RETHINK && _grenade)
				brutalGrenadeAction();
			// Phase 34.6 (Calypso): terrain tactics (floor-drop / wall-breach) -- additive
			// candidate slot, gated internally on ai.terrainTactics + hostile faction + no
			// other attack chosen. A no-op when the flag is off, so brutalThink stays
			// byte-identical to the 34.5 port in that case.
			if (_attackAction.type == BA_RETHINK)
				considerTerrainAttack();
		}
		if (_attackAction.type != BA_RETHINK)
		{
			action->type = _attackAction.type;
			action->target = _attackAction.target;
			action->weapon = _attackAction.weapon;
			action->aiTargetId = _attackAction.aiTargetId;
			action->aiHasFilteredFallback = _attackAction.aiHasFilteredFallback;
			action->number -= 1;
			if (action->weapon && action->type == BA_THROW && action->weapon->getRules()->getBattleType() == BT_GRENADE && !action->weapon->isFuseEnabled())
			{
				_unit->spendCost(_unit->getActionTUs(BA_PRIME, action->weapon));
				action->weapon->setFuseTimer(0); // don't just spend the TUs for nothing! If we already circumvent the API anyways, we might as well actually prime the damn thing!
				_unit->spendTimeUnits(action->weapon->getMoveToCost(_save->getMod()->getInventoryLeftHand()));
				_save->getBattleGame()->markAIWorldChanged();
			}
			action->updateTU();
			if (_traceAI)
			{
				if (action->type != BA_WALK)
					Log(LOG_INFO) << "Should attack " << action->target
								  << " with " << action->weapon->getRules()->getName();
			}
			if (action->type == BA_LAUNCH)
			{
				action->waypoints = _attackAction.waypoints;
			}
			else if (action->type == BA_AIMEDSHOT || action->type == BA_AUTOSHOT)
			{
				if (_unit->getTimeUnits() >= _unit->getKneelDownCost() + action->Time + (_tuCostToReachClosestPositionToBreakLos > 0 ? (_tuCostToReachClosestPositionToBreakLos + _unit->getKneelUpCost()) : 0))
					action->kneel = _unit->getArmor()->allowsKneeling(_unit->getType() == "SOLDIER") && !_unit->isFloating();
			}
			return;
		}
		else
		{
			if (_traceAI)
				Log(LOG_INFO) << "Could not find a proper target to attack.";
		}
	}
	_allowedToCheckAttack = false;
	
	Position peakPosition = myPos;
	bool iHaveLof = false;
	Position targetPosition = myPos;
	BattleAction originAction;
	originAction.actor = _unit;
	originAction.weapon = action->weapon;
	if (unitToWalkTo)
	{
		targetPosition = unitToWalkTo->getPosition();
		if (!_unit->isCheatOnMovement())
			targetPosition = _save->getTileCoords(unitToWalkTo->getTileLastSpotted(_unit->getFaction()));
		Tile* tileOfTarget = _save->getTile(targetPosition);
		originAction.target = unitToWalkTo->getPosition();
		Position origin = _save->getTileEngine()->getOriginVoxel(originAction, myTile);
		if (targetPosition == unitToWalkTo->getPosition())
		{
			iHaveLof = _save->getTileEngine()->canTargetUnit(&origin, unitToWalkTo->getTile(), nullptr, _unit, false);
			if (iHaveLof && Options::battleRealisticAccuracy)
			{
				if (_save->getTileEngine()->checkVoxelExposure(&origin, unitToWalkTo->getTile(), _unit) < EPSILON)
					iHaveLof = false;
			}
		}
		iHaveLof = iHaveLof || clearSight(myPos, targetPosition);
	}
	if (!_unit->isCheatOnMovement() && visibleToEnemy && _visibleEnemies == 0)
	{
		if (_save->getTileEngine()->getDirectionTo(myPos, visibleFromPosition) != _unit->getDirection())
		{
			action->type = BA_TURN;
			action->target = visibleFromPosition;
			if (_traceAI)
			{
				Log(LOG_INFO) << "Want to look at position: " << visibleFromPosition;
			}
			return;
		}
	}

	// Check if I'm a turret. In this case I can skip everything about walking
	if (immobile && _tuWhenChecking == _unit->getTimeUnits())
	{
		if (_traceAI)
			Log(LOG_INFO) << "I'm either not allowed to move or have 0 energy. So I'll just end my turn.";
		action->type = BA_NONE;
		setWantToEndTurn(true);
		return;
	}

	BattleActionCost snapCost = BattleActionCost(BA_SNAPSHOT, _unit, action->weapon);
	BattleActionCost hitCost = BattleActionCost(BA_HIT, _unit, action->weapon);
	BattleActionCost costSnap(BA_SNAPSHOT, _unit, action->weapon);

	// When I'm mind-controlled I should definitely be reckless
	if (IAmMindControlled)
	{
		sweepMode = true;
		if (_traceAI)
			Log(LOG_INFO) << "I'm mind-controlled.";
	}

	if (_traceAI)
	{
		if (unitToWalkTo)
		{
			Log(LOG_INFO) << "unit with closest walking-distance " << unitToWalkTo->getId() << " " << unitToWalkTo->getPosition() << " dist: " << shortestWalkingPath << " Lof: " << iHaveLof;
			if (!_unit->isCheatOnMovement())
			{
				Position targetPosition = _save->getTileCoords(unitToWalkTo->getTileLastSpotted(_unit->getFaction()));
				Log(LOG_INFO) << "Since I'm not cheating I think " << unitToWalkTo->getId() << " at " << unitToWalkTo->getPosition() << " is at " << targetPosition;
			}
		}
	}
	float bestAttackScore = 0;
	Position bestAttackPosition = myPos;
	float bestGreatCoverScore = 0;
	Position bestGreatCoverPosition = myPos;
	float bestGoodCoverScore = 0;
	Position bestGoodCoverPosition = myPos;
	float bestOkayCoverScore = 0;
	Position bestOkayCoverPosition = myPos;
	float bestDirectPeakScore = 0;
	Position bestDirectPeakPosition = myPos;
	float bestIndirectPeakScore = 0;
	Position bestIndirectPeakPosition = myPos;
	float bestFallbackScore = 0;
	Position bestFallbackPosition = myPos;
	bool saveDistance = true;
	for (auto& reachable : enemyReachable)
	{
		if (reachable.second > 0 && hasTileSight(myPos, reachable.first))
		{
			saveDistance = false;
			break;
		}
	}
	if (!_unit->getVisibleUnits()->empty() || contact)
		saveDistance = false;
	if (_traceAI)
		Log(LOG_INFO) << "I have last been seen: " << _unit->getTurnsSinceSeen(_targetFaction);
	if (_traceAI && immobileEnemies)
		Log(LOG_INFO) << "Immobile enemies detected. Taking cover takes precedent over attacking.";
	bool wantToPrime = false;
	int primeCost = 0;
	if (true /*CALYPSO: match Brutal default*/ && _grenade && !_unit->getGrenadeFromBelt(_save)->isFuseEnabled() && !IAmMindControlled && !_unit->getGrenadeFromBelt(_save)->getRules()->getExplodeInventory(_save->getMod()))
	{
		BattleItem* grenade = _unit->getGrenadeFromBelt(_save);
		
		primeCost = _unit->getActionTUs(BA_PRIME, grenade).Time + grenade->getMoveToCost(_save->getMod()->getInventoryLeftHand());
		if (saveDistance)
		{
			if (primeCost <= _unit->getTimeUnits())
			{
				_unit->spendTimeUnits(grenade->getMoveToCost(_save->getMod()->getInventoryLeftHand()));
				_unit->spendCost(_unit->getActionTUs(BA_PRIME, grenade));
				grenade->setFuseTimer(0); // don't just spend the TUs for nothing! If we already circumvent the API anyways, we might as well actually prime the damn thing!
				_save->getBattleGame()->markAIWorldChanged();
				if (_traceAI)
					Log(LOG_INFO) << "I spent " << primeCost << " time-units on priming a grenade.";
				action->type = BA_RETHINK;
				action->number -= 1;
				return;
			}
		}
		else
		{
			wantToPrime = true;
		}
	}
	float myWeaponScore = getItemPickUpScore(_unit->getMainHandWeapon(true, false));
	if (saveDistance)
		improveItemization(myWeaponScore, action);
	if (_traceAI)
		Log(LOG_INFO) << "iHaveLof : " << iHaveLof << " sweep - mode : " << sweepMode << " could be found : " << amInLoSToFurthestReachable << " energy - recovery : " << getEnergyRecovery(_unit) << " wantToPrime: " << wantToPrime << " saveDistance: " << saveDistance << " contact: " << contact << " damagePotentialFromCurrentPosition: " << damagePotentialFromCurrentPosition;
	bool winnerWasSpecialDoorCase = false;
	bool shouldHaveLofAfterMove = false;
	bool shouldEndTurnAfterMove = false;
	int peakDirection = _unit->getDirection();
	int lastStepCost = 0;
	int attackTU = snapCost.Time;
	int attackENE = snapCost.Energy;
	if (IAmPureMelee)
	{ // We want to go in anyways, regardless of whether we still can attack or not
		attackTU = hitCost.Time;
		attackENE = hitCost.Energy;
	}
	Position travelTarget = myPos;
	int selectedMoveTargetId = unitToWalkTo ? unitToWalkTo->getId() : -1;
	bool enemyHasHighGround = false;
	std::unordered_map<int, MoveEvaluation> moveMap;
	if (unitToWalkTo != NULL)
	{
		const int moveTargetId = selectedMoveTargetId;
		Position attackDirection = targetPosition;
		BattleActionCost reserved = BattleActionCost(_unit);
		Position travelTarget = furthestToGoTowards(targetPosition, reserved, _allPathFindingNodes);
		std::vector<PathfindingNode*> targetNodes = _save->getPathfinding()->findReachablePathFindingNodes(_unit, BattleActionCost(), dummy, true, NULL, &travelTarget, false, false, bam);
		if (_traceAI)
		{
			Log(LOG_INFO) << "travelTarget: " << travelTarget << " targetPositon: " << targetPosition << " sweep-mode: " << sweepMode << " furthest-enemy: " << furthestPositionEnemyCanReach << " targetDistanceTofurthestReach: " << targetDistanceTofurthestReach << " peakPosition: " << peakPosition;
		}
		float myTuDistFromTarget = tuCostToReachPosition(_positionAtStartOfTurn, targetNodes, NULL, true);
		float myWalkToDist = myMaxTU + myTuDistFromTarget;
		std::vector<Tile*> corpseTiles = getCorpseTiles(_allPathFindingNodes);
		float visiblePathFromMyPos = 0;
		bool pathThroughLift = false;
		getPositionsOnPathTo(targetPosition, _allPathFindingNodes, _pathToEnemyPositions);
		for (auto pathPos : _pathToEnemyPositions)
		{
			Tile* pathTile = _save->getTile(pathPos);
			if (pathTile->getMapData(O_FLOOR) && pathTile->getMapData(O_FLOOR)->isGravLift())
				pathThroughLift = true;
			if (hasTileSight(myPos, pathPos))
				visiblePathFromMyPos += 1;
		}
		if (pathThroughLift && targetPosition.z > myPos.z && !IAmMindControlled)
			enemyHasHighGround = true;
		ThreatField* sharedThreatField = prepareSharedThreatField(enemyReachable);
		const bool useDeterministicEvalBudget = _save->getMod()->getAISharedFields();
		std::vector<PathfindingNode*> orderedMovementCandidates;
		const std::vector<PathfindingNode*>* movementCandidates = &_allPathFindingNodes;
		if (useDeterministicEvalBudget)
		{
			typedef std::pair<AICandidateRank, PathfindingNode*> RankedMovementCandidate;
			std::vector<RankedMovementCandidate> rankedCandidates;
			rankedCandidates.reserve(_allPathFindingNodes.size());
			for (PathfindingNode* candidate : _allPathFindingNodes)
			{
				const Position pos = candidate->getPosition();
				rankedCandidates.push_back(RankedMovementCandidate(
					AICandidateRank{sharedThreatField ? sharedThreatField->threatAt(pos) : 0.0f,
						Position::distanceSq(pos, targetPosition), pos}, candidate));
			}
			std::sort(rankedCandidates.begin(), rankedCandidates.end(),
				[](const RankedMovementCandidate& lhs, const RankedMovementCandidate& rhs)
				{
					return AICandidateRankLess()(lhs.first, rhs.first);
				});
			orderedMovementCandidates.reserve(rankedCandidates.size());
			for (const RankedMovementCandidate& candidate : rankedCandidates)
				orderedMovementCandidates.push_back(candidate.second);
			movementCandidates = &orderedMovementCandidates;
		}
		AIEvaluationBudget movementEvalBudget(
			useDeterministicEvalBudget ? _save->getMod()->getAIEvalBudget() : 0,
			useDeterministicEvalBudget ? _save->getMod()->getAITurnBudgetMs() : 0);

		for (auto pu : *movementCandidates)
		{
			Position pos = pu->getPosition();
			if (!candidateAllowed(BA_WALK, moveTargetId, pos)) continue;
			Tile* tile = _save->getTile(pos);
			if (tile == NULL)
				continue;
			if (tile->hasNoFloor() && _unit->getMovementType() != MT_FLY)
				continue;
			if (pu->getTUCost(false).time > _unit->getTimeUnits() || pu->getTUCost(false).energy > _unit->getEnergy())
				continue;
			MoveEvaluation me{};
			bool saveForProxies = true;
			bool inDoors = false;
			Tile* tileAbove = _save->getAboveTile(tile);
			if (tileAbove && !tileAbove->hasNoFloor())
				inDoors = true;
			Tile* tileBelow = _save->getBelowTile(tile);
			if (_save->getMod()->getAIPerformanceOptimization() && tile->hasNoFloor() && !inDoors && tileBelow && tileBelow->hasNoFloor())
				continue;
			isPathToPositionSave(pos, saveForProxies);
			if (!saveForProxies)
				continue;
			if (useDeterministicEvalBudget)
			{
				const int elapsedMs = movementEvalBudget.isTimeLimitEnabled() ? _aiTiming.elapsedMs() : 0;
				const bool countExhausted = !movementEvalBudget.canEvaluate();
				if (movementEvalBudget.shouldStopBeforeNext(elapsedMs))
				{
					if (!countExhausted && movementEvalBudget.isTimeExpired(elapsedMs) && _traceAI)
					{
						Log(LOG_INFO) << "AI_EVAL_BUDGET_TIME_EXPIRED elapsed_ms=" << elapsedMs
							<< " evaluations_used=" << movementEvalBudget.evaluationsUsed()
							<< " unit=" << _unit->getId();
					}
					break;
				}
				movementEvalBudget.consumeEvaluation();
			}
			float closestEnemyDistValid = FLT_MAX;
			float closestEnemyDistAssumed = FLT_MAX;
			float targetDist = Position::distance(pos, targetPosition);
			float cuddleAvoidModifier = 1;
			bool avoidMeleeRange = false;
			bool lineOfFire = false;
			bool lineOfFireBeforeFriendCheck = false;
			float closestAnyOneDist = FLT_MAX;
			float exposureMod = 1.0;
			int currLastStepCost = 0;
			Position ref;
			float viewDistance = _save->getMod()->getMaxViewDistance();
			int maxSmoke = myTile->getSmoke();
			int remainingTimeUnits = _unit->getTimeUnits() - pu->getTUCost(false).time;
			me.remainingTU = remainingTimeUnits;
			int remainingEnergy = _unit->getTimeUnits() - pu->getTUCost(false).energy;
			me.remainingEnergy = remainingEnergy;
			int bestPeakDirectionFromPos = _unit->getDirection();
			bool enemyShouldBeVisible = false;
			Position currentAttackDirection = targetPosition;
			if (unitToWalkTo)
			{
				viewDistance = _unit->getMaxViewDistanceAtDay(unitToWalkTo);
				if (tile->getShade() > _save->getMod()->getMaxDarknessToSeeUnits() && tile->getFire() == 0)
					viewDistance = _unit->getMaxViewDistanceAtDark(unitToWalkTo);
				maxSmoke = std::max(unitToWalkTo->getTile()->getSmoke(), std::max(maxSmoke, tile->getSmoke()));
			}
			viewDistance = std::min(viewDistance, (float)(_save->getMod()->getMaxViewDistance() / (1.0 + maxSmoke / 3.0)));
			float highestDamage = 0;
			bool specialDoorCase = false;
			for (BattleUnit* unit : *(_save->getUnits()))
			{
				Position unitPosition = unit->getPosition();
				if (unit->isOut())
					continue;
				if (!_unit->isCheatOnMovement() && unit->getFaction() != _unit->getFaction())
					unitPosition = _save->getTileCoords(unit->getTileLastSpotted(_unit->getFaction()));
				float unitDist = Position::distance(pos, unitPosition);
				if (isAlly(unit) && unit != _unit && unitPosition.z == pos.z && !IAmMindControlled)
				{
					if (unitDist < 5)
					{
						if (quickLineOfFire(pos, unit))
							cuddleAvoidModifier += (1 - unitDist * 0.2);
					}
				}
				if (unitDist < closestAnyOneDist && unit != _unit)
					closestAnyOneDist = unitDist;
				if (isAlly(unit))
					continue;
				if (!_unit->isCheatOnMovement() && unit->getTileLastSpotted(_unit->getFaction()) == -1)
					continue;
				if (hasTileSight(pos, unitPosition))
				{
					lineOfFireBeforeFriendCheck = true;
					bestPeakDirectionFromPos = _save->getTileEngine()->getDirectionTo(pos, unitPosition);
					me.bestDirection = bestPeakDirectionFromPos;
					if (Position::distance(pos, unitPosition) <= viewDistance)
					{
						if (unitPosition == unit->getPosition())
						{
							if (quickLineOfFire(pos, unit))
								enemyShouldBeVisible = true;
						}
						else if (clearSight(pos, targetPosition))
							enemyShouldBeVisible = true;
					}
				}
				if (unitDist < closestEnemyDistAssumed)
					closestEnemyDistAssumed = unitDist;
				if (shouldAvoidMeleeRange(unit) && unitDist < 2)
				{
					avoidMeleeRange = true;
				}
				if (_unit->aiCheatMode() < 0 && unitDist > viewDistance)
					continue;
				if (brutalValidTarget(unit, true))
				{
					if (unitDist < closestEnemyDistValid)
						closestEnemyDistValid = unitDist;
					float currentDamagePotential = damagePotential(pos, unit, remainingTimeUnits, remainingEnergy);
					if (currentDamagePotential > highestDamage)
					{
						highestDamage = currentDamagePotential;
						currentAttackDirection = unitPosition;
					}
					if (!IAmPureMelee)
					{
						if (!lineOfFire)
						{
							originAction.target = unit->getPosition();
							Position origin = _save->getTileEngine()->getOriginVoxel(originAction, tile);
							if (originAction.weapon && originAction.weapon->getArcingShot(BA_SNAPSHOT))
								lineOfFire = validateArcingShot(&originAction, tile);
							else
								lineOfFire = _save->getTileEngine()->canTargetUnit(&origin, unit->getTile(), nullptr, _unit, false);
							BattleAction* throwAction = grenadeThrowAction(originAction.target);
							if (throwAction && !lineOfFire &&!_save->getTile(originAction.target)->getDangerous())
								lineOfFire = validateArcingShot(throwAction, tile);
							if (lineOfFire && Options::battleRealisticAccuracy)
							{
								exposureMod = std::max(exposureMod, (float)_save->getTileEngine()->checkVoxelExposure(&origin, unit->getTile(), _unit));
								if (exposureMod < EPSILON)
									lineOfFire = false;
							}
							if (!_unit->isCheatOnMovement() && !lineOfFire)
								lineOfFire = clearSight(pos, unitPosition);
							if (lineOfFire)
							{
								lineOfFireBeforeFriendCheck = true;
								if (projectileMayHarmFriends(pos, unitPosition))
									lineOfFire = false;
							}
						}
					}
				}
			}
			bool haveTUToAttack = false;
			bool outOfRangeForShortRangeWeapon = false;
			if (weaponRange < closestEnemyDistAssumed)
				outOfRangeForShortRangeWeapon = true;
			if (!lineOfFire && (pos != myPos))
			{
				if (IAmPureMelee || _unit->isCheatOnMovement())
				{
					if ((brutalValidTarget(unitToWalkTo, true) || _unit->isCheatOnMovement()) && (_save->getTileEngine()->validMeleeRange(pos, _save->getTileEngine()->getDirectionTo(pos, targetPosition), _unit, unitToWalkTo, NULL) && (_melee || quickLineOfFire(pos, unitToWalkTo, false, !_unit->isCheatOnMovement()))))
					{
						lineOfFire = true;
					}
				}
			}
			bool shouldHaveBeenAbleToAttack = pos == myPos && _tuWhenChecking == _unit->getTimeUnits();

			bool realLineOfFire = lineOfFire;
			//! Special case: Our target is at a door and the tile we want to go to is too and they have a distance of 1. That means the target is blocking door from other side. So we go there and open it!
			if (!lineOfFire)
			{
				for (int x = 0; x < _unit->getArmor()->getSize(); ++x)
				{
					for (int y = 0; y < _unit->getArmor()->getSize(); ++y)
					{
						Position checkPos = pos;
						checkPos += Position(x, y, 0);
						Tile* targetTile = _save->getTile(checkPos);
						if (_save->getTileEngine()->isNextToDoor(targetTile) && targetDist < 1 + _unit->getArmor()->getSize() && targetPosition.z == checkPos.z)
						{
							Tile* targetTile = _save->getTile(targetPosition);
							if (_save->getTileEngine()->isNextToDoor(targetTile) || IAmPureMelee)
							{
								shouldHaveBeenAbleToAttack = false;
								lineOfFire = true;
								realLineOfFire = false;
								attackTU += 8;
								specialDoorCase = true;
								remainingTimeUnits -= 8;
							}
						}
					}
				}
			}
			if (pu->getTUCost(false).time <= _unit->getTimeUnits() - attackTU && pu->getTUCost(false).energy <= _unit->getEnergy() - attackENE)
				haveTUToAttack = true;
			float attackScore = 0;
			float greatCoverScore = 0;
			float goodCoverScore = 0;
			float okayCoverScore = 0;
			float directPeakScore = 0;
			float indirectPeakScore = 0;
			float fallbackScore = 0;
			int crossEnemyVision = 0;
			bool pathInvolvesFalling = false;
			getPositionsOnPathTo(pos, _allPathFindingNodes, _pathToPosBuffer);
			for (auto pathPos : _pathToPosBuffer)
			{
				if (_save->getTile(pathPos)->hasNoFloor() && _unit->getMovementType() != MT_FLY)
				{
					pathInvolvesFalling = true;
				}
				if (!IAmPureMelee && !sweepMode)
				{
					for (BattleUnit* bu : *(_save->getUnits()))
					{
						if (!isEnemy(bu) || bu->isOut())
							continue;
						if (bu->getReactionScore() < (remainingTimeUnits * _unit->getBaseStats()->reactions) / (double)_unit->getBaseStats()->tu)
							continue;
						if (Position::distance(pathPos, bu->getPosition()) > viewDistance)
							continue;
						if (Position::distance(pathPos, bu->getPosition()) > maxExtenderRangeWith(bu, bu->getTimeUnits()))
							continue;
						for (Tile* buVisible : *bu->getVisibleTiles())
						{
							if (buVisible->getPosition() == pathPos)
								crossEnemyVision++;
						}
					}
				}
			}
			if (!_blaster && lineOfFire && haveTUToAttack && !shouldHaveBeenAbleToAttack && (highestDamage > 0 || specialDoorCase) && !enemyHasHighGround)
			{
				if ((maxExtenderRangeWith(_unit, _unit->getTimeUnits() - pu->getTUCost(false).time) >= closestEnemyDistValid || specialDoorCase) || IAmPureMelee)
				{
					if (crossEnemyVision > 1 || (crossEnemyVision > 0 && contact))
						highestDamage = std::min(highestDamage, 1.0f);
					if (specialDoorCase)
						highestDamage = 1;
					attackScore = remainingTimeUnits * highestDamage;
					me.attackPotential = highestDamage;
					if (Options::battleRealisticAccuracy && !specialDoorCase)
					{
						attackScore *= exposureMod;
						me.attackPotential *= exposureMod;
					}
					me.bestDirection = _save->getTileEngine()->getDirectionTo(pos, currentAttackDirection);
					if (pu->getPrevNode() && !isPositionVisibleToEnemy(pu->getPrevNode()->getPosition()))
						currLastStepCost = pu->getTUCost(false).time - pu->getPrevNode()->getTUCost(false).time;
				}
			}
			me.lastStepCost = currLastStepCost;
			float tuDistFromTarget = tuCostToReachPosition(pos, targetNodes, NULL, true);
			float walkToDist = myMaxTU + tuDistFromTarget;
			me.walkToDist = walkToDist;
			float visiblePath = 0;
			//only add visiblePath-bonus for positions closer to target than our current position as otherwise we are unnecessarily prolong the path
			if (tuDistFromTarget < myTuDistFromTarget)
			{
				for (auto pathPos : _pathToEnemyPositions)
				{
					if (hasTileSight(pos, pathPos))
						visiblePath += 1;
				}
			}
			if (!sweepMode && crossEnemyVision <= 1 && !enemyHasHighGround && haveTUToAttack)
			{
				if (myPos != pos && enemyShouldBeVisible && !outOfRangeForShortRangeWeapon)
				{
					directPeakScore = remainingTimeUnits;
					me.IsDirectPeak = true;
				}
				else if (!pathInvolvesFalling && !_unit->isCheatOnMovement() && (myMaxTU == _unit->getTimeUnits() || _save->getTileEngine()->isNextToDoor(myTile)))
				{
					bool viable = !tile->hasNoFloor();
					if (pos.x == myPos.x && pos.y == myPos.y)
					{
						viable = true;
					}
					else
					{
						for (Position pathToEnemyPos : _pathToEnemyPositions)
						{
							if (pos == pathToEnemyPos)
							{
								viable = true;
								break;
							}
						}
					}
					if (viable)
					{
						int highestVisibleTiles = 0;
						if (!_save->getMod()->getAIPerformanceOptimization())
						{
							for (int i = 0; i < 8; i++)
							{
								float currentVisibleTiles = scoreVisibleTiles(_save->getTileEngine()->visibleTilesFrom(_unit, pos, i, true));
								if (currentVisibleTiles > highestVisibleTiles)
								{
									highestVisibleTiles = currentVisibleTiles;
									bestPeakDirectionFromPos = i;
								}
							}
							if (me.attackPotential == 0)
							{
								me.bestDirection = bestPeakDirectionFromPos;
							}
						}
						if (!(bestPeakDirectionFromPos == _unit->getDirection() || pos == myPos))
						{
							if (_save->getMod()->getAIPerformanceOptimization())
							{
								indirectPeakScore = visiblePath;
								me.visibleTiles = visiblePath;
							}
							else
							{
								indirectPeakScore = highestVisibleTiles;
								if (visiblePath > 0)
									highestVisibleTiles *= 2;
								me.visibleTiles = highestVisibleTiles;
								indirectPeakScore *= remainingTimeUnits;
							}
						}
					}
				}
			}
			float discoverThreat = 0;
			bool validCover = true;
			bool isNode = false;
			if (_save->getMod()->getAIPerformanceOptimization() && validCover)
			{
				if (tile->hasNoFloor() && !inDoors)
				{
					if (tileBelow && tileBelow->hasNoFloor())
						validCover = false;
				}
				for (const auto* node : *_save->getNodes())
				{
					if (node->getPosition() == pos)
					{
						isNode = true;
						break;
					}
				}
				if (!isNode && getCoverValue(tile, _unit, 3) == 0)
					validCover = false;
			}
			const bool sharedThreatConfirmsDanger = sharedThreatField && sharedThreatField->confirmsDangerAt(pos);
			if (_unit->getMainHandWeapon() != NULL && outOfRangeForShortRangeWeapon
				&& (sharedThreatConfirmsDanger || isPositionVisibleToEnemy(pos, true)))
				validCover = false;
			if (!sweepMode && validCover)
			{
				if (sharedThreatField && sharedThreatField->isEvaluated(pos))
				{
					discoverThreat = sharedThreatField->threatAt(pos);
				}
				else
				{
					discoverThreat = calculateDiscoverThreat(pos, enemyReachable);
					if (sharedThreatField)
						sharedThreatField->stampMax(pos, discoverThreat);
				}
				if (discoverThreat == 0)
				{
					if (!_save->getTileEngine()->isNextToDoor(tile) || contact)
						greatCoverScore = 100 / walkToDist;
					else
						goodCoverScore = 100 / walkToDist;
				}
				else if (discoverThreat > 0)
				{
					float tieBreaker = 1 / walkToDist;
					if (!outOfRangeForShortRangeWeapon && !IAmPureMelee)
						tieBreaker *= -1;
					if (!_save->getTileEngine()->isNextToDoor(tile) || contact)
						goodCoverScore = 100 / discoverThreat + tieBreaker;
					else
						okayCoverScore = 100 / discoverThreat + tieBreaker;
				}
				else if (!lineOfFireBeforeFriendCheck)
				{
					okayCoverScore = 100 / walkToDist;
				}
				if (discoverThreat == 0)
				{
					float highestPickupScore = 0;
					if (!tile->getInventory()->empty())
					{
						for (BattleItem* item : *tile->getInventory())
						{
							float pickUpScore = getItemPickUpScore(item);
							if (pickUpScore > myWeaponScore && pickUpScore > highestPickupScore)
							{
								highestPickupScore = pickUpScore;
							}
						}
					}
					if (highestPickupScore > 0)
					{
						if (greatCoverScore > 0)
							greatCoverScore += highestPickupScore - myWeaponScore;
						if (goodCoverScore > 0)
							goodCoverScore += highestPickupScore - myWeaponScore;
						if (okayCoverScore > 0)
							okayCoverScore += highestPickupScore - myWeaponScore;
					}
				}
			}
			if ((discoverThreat == 0 || immobileEnemies) && !contact && !IAmPureMelee && !tile->getDangerous() && !tile->getFire() && !_save->getTileEngine()->isNextToDoor(tile) && (pu->getTUCost(false).time < _tuCostToReachClosestPositionToBreakLos || _tuWhenChecking != _unit->getTimeUnits()))
			{
				_tuCostToReachClosestPositionToBreakLos = pu->getTUCost(false).time;
				_energyCostToReachClosestPositionToBreakLos = pu->getTUCost(false).energy;
				_tuWhenChecking = _unit->getTimeUnits();
			}
			fallbackScore = 100 / walkToDist;
			greatCoverScore /= cuddleAvoidModifier;
			goodCoverScore /= cuddleAvoidModifier;
			okayCoverScore /= cuddleAvoidModifier;
			fallbackScore /= cuddleAvoidModifier;
			if (tile->getDangerous() || (tile->getFire() && _unit->avoidsFire()))
			{
				if (IAmMindControlled && !(tile->getFloorSpecialTileType() == START_POINT && _unit->getOriginalFaction() == FACTION_PLAYER))
				{
					greatCoverScore *= 10;
					goodCoverScore *= 10;
					okayCoverScore *= 10;
					fallbackScore *= 10;
				}
				else
				{
					attackScore /= 2;
					if (_unit->getTile()->getDangerous() || (_unit->getTile()->getFire() && _unit->avoidsFire()))
					{
						greatCoverScore /= 10;
						goodCoverScore /= 10;
						okayCoverScore /= 10;
						fallbackScore /= 10;
					}
					else
					{
						greatCoverScore = 0;
						goodCoverScore = 0;
						okayCoverScore = 0;
						fallbackScore = 0;
					}
				}
			}
			float avoidDivider = 1.0f;
			for (auto corpseTile : corpseTiles)
			{
				if (hasTileSight(pos, corpseTile->getPosition()))
					avoidDivider += 1.0f;
			}
			if (tile->getMapData(O_FLOOR) && tile->getMapData(O_FLOOR)->isGravLift())
			{
				avoidDivider += 1.0f;
			}
			greatCoverScore /= avoidDivider;
			goodCoverScore /= avoidDivider;
			okayCoverScore /= avoidDivider;

			float bonus = 100;
			if (inDoors)
			{
				if(contact)
					bonus += tileAbove->getMapData(O_FLOOR)->getArmor();
				else
					bonus += std::max(20.0, tileAbove->getMapData(O_FLOOR)->getArmor() / 5.0);
			}
			greatCoverScore *= bonus;
			goodCoverScore *= bonus;
			okayCoverScore *= bonus;
			// Avoid tiles from which the player can take me with them when retreating
			if (IAmMindControlled && tile->getFloorSpecialTileType() == START_POINT && _unit->getOriginalFaction() == FACTION_PLAYER)
			{
				greatCoverScore /= 10;
				goodCoverScore /= 10;
				okayCoverScore /= 10;
				fallbackScore /= 10;
			}
			if (!tile->getInventory()->empty() && _unit->getFaction() == _unit->getOriginalFaction())
			{
				for (BattleItem* bi : *tile->getInventory())
				{
					if (bi->getUnit() && bi->getUnit()->getFaction() == _unit->getFaction())
					{
						greatCoverScore /= 2;
						goodCoverScore /= 2;
						okayCoverScore /= 2;
					}
				}
			}
			if (avoidMeleeRange)
			{
				attackScore /= 2;
				directPeakScore /= 10;
				indirectPeakScore /= 10;
			}
			moveMap[_save->getTileIndex(pos)] = me;
			if (attackScore > bestAttackScore)
			{
				bestAttackScore = attackScore;
				bestAttackPosition = pos;
				shouldHaveLofAfterMove = realLineOfFire;
				winnerWasSpecialDoorCase = specialDoorCase;
				lastStepCost = currLastStepCost;
				attackDirection = currentAttackDirection;
			}
			if (greatCoverScore > bestGreatCoverScore)
			{
				bestGreatCoverScore = greatCoverScore;
				bestGreatCoverPosition = pos;
			}
			if (goodCoverScore > bestGoodCoverScore)
			{
				bestGoodCoverScore = goodCoverScore;
				bestGoodCoverPosition = pos;
			}
			if (okayCoverScore > bestOkayCoverScore)
			{
				bestOkayCoverScore = okayCoverScore;
				bestOkayCoverPosition = pos;
			}
			if (directPeakScore > bestDirectPeakScore)
			{
				bestDirectPeakScore = directPeakScore;
				bestDirectPeakPosition = pos;
				peakDirection = bestPeakDirectionFromPos;
			}
			if (indirectPeakScore > bestIndirectPeakScore)
			{
				bestIndirectPeakScore = indirectPeakScore;
				bestIndirectPeakPosition = pos;
				peakDirection = bestPeakDirectionFromPos;
			}
			if (fallbackScore > bestFallbackScore)
			{
				bestFallbackScore = fallbackScore;
				bestFallbackPosition = pos;
			}
			//if (_traceAI)
			//{
			//	tile->setMarkerColor(_unit->getId()%100);
			//	tile->setPreview(10);
			//	tile->setTUMarker(highestDamage);
			//}
		}
		if (_traceAI)
		{
			if (bestAttackScore > 0)
			{
				Log(LOG_INFO) << "bestAttackPosition: " << bestAttackPosition << " score: " << bestAttackScore;
			}
			if (bestDirectPeakScore > 0)
			{
				Log(LOG_INFO) << "bestDirectPeakPosition: " << bestDirectPeakPosition << " score: " << bestDirectPeakScore;
			}
			if (bestIndirectPeakScore > 0)
			{
				Log(LOG_INFO) << "bestIndirectPeakScore: " << bestIndirectPeakPosition << " score: " << bestIndirectPeakScore;
			}
			if (bestGreatCoverScore > 0)
			{
				Log(LOG_INFO) << "bestGreatCoverPosition: " << bestGreatCoverPosition << " score: " << bestGreatCoverScore;
			}
			if (bestGoodCoverScore > 0)
			{
				Log(LOG_INFO) << "bestGoodCoverPosition: " << bestGoodCoverPosition << " score: " << bestGoodCoverScore;
			}
			if (bestOkayCoverScore > 0)
			{
				Log(LOG_INFO) << "bestOkayCoverPosition: " << bestOkayCoverPosition << " score: " << bestOkayCoverScore;
			}
			if (bestFallbackScore > 0)
			{
				Log(LOG_INFO) << "bestFallbackPosition: " << bestFallbackPosition << " score: " << bestFallbackScore;
			}
		}
		if (bestAttackPosition == myPos)
		{
			attackTU += getTurnCostTowards(attackDirection);
		}
	}
	bool haveTUToAttack = false;
	int moveTU = tuCostToReachPosition(bestAttackPosition, _allPathFindingNodes);
	if (lastStepCost != 0)
		_tuCostToReachClosestPositionToBreakLos = lastStepCost;
	if (_tuCostToReachClosestPositionToBreakLos != -1)
	{
		attackTU += _tuCostToReachClosestPositionToBreakLos;
	}
	if (moveTU <= _unit->getTimeUnits() - attackTU)
		haveTUToAttack = true;
	if (bestAttackScore > 0 && !haveTUToAttack && bestGreatCoverScore > 0)
	{
		shouldHaveLofAfterMove = iHaveLof;
		if (_traceAI)
			Log(LOG_INFO) << "Attack dismissed due to lack of TU to go back to hiding-spot afterwards. Attack + Hide: " << attackTU << " move: " << moveTU << " current: " << _unit->getTimeUnits();
	}
	else if (bestAttackScore > 0)
	{
		haveTUToAttack = true;
		_tuCostToReachClosestPositionToBreakLos = -1;
		_energyCostToReachClosestPositionToBreakLos = -1;
	}
	int newVisibleTilesDirect = 0;
	int newVisibleTilesInDirect = 0;
	bool indirectPeek = false;
	newVisibleTilesDirect += scoreVisibleTiles(_save->getTileEngine()->visibleTilesFrom(_unit, bestDirectPeakPosition, peakDirection, true));
	newVisibleTilesInDirect += scoreVisibleTiles(_save->getTileEngine()->visibleTilesFrom(_unit, bestIndirectPeakPosition, peakDirection, true));
	if (_traceAI)
	{
		Log(LOG_INFO) << "New visible tiles from " << bestDirectPeakPosition << ": " << newVisibleTilesDirect;
		Log(LOG_INFO) << "New visible tiles from " << bestIndirectPeakPosition << ": " << newVisibleTilesInDirect;
	}
	float bestPeekPreserveScore = -1.0f;
	Position peekPreserveCompromise = _unit->getPosition();
	if (!contact && bestAttackScore <= 0)
	{
		int maxScout = 0;
		int minScout = INT_MAX;
		float avgScout = 0;
		int maxRemainingTU = 0;
		int minRemainingTU = getMaxTU(_unit);
		float avgRemainingTU = 0;
		for (auto& move : moveMap)
		{
			maxScout = std::max(maxScout, move.second.visibleTiles);
			minScout = std::min(minScout, move.second.visibleTiles);
			avgScout += move.second.visibleTiles;
			maxRemainingTU = std::max(maxRemainingTU, move.second.remainingTU);
			minRemainingTU = std::min(minRemainingTU, move.second.remainingTU);
			avgRemainingTU += move.second.remainingTU;
		}
		avgScout /= moveMap.size();
		avgRemainingTU /= moveMap.size();

		for (auto& move : moveMap)
		{
			float scoutNorm = 1.0f;
			if (maxScout != minScout)
				scoutNorm = (move.second.visibleTiles - minScout) / float(maxScout - minScout);

			float tuNorm = 1.0f;
			if (maxRemainingTU != minRemainingTU)
				tuNorm = (move.second.remainingTU - minRemainingTU) / float(maxRemainingTU - minRemainingTU);

			float peekPreserveScore = scoutNorm * tuNorm;
			//if (_traceAI && move.second.visibleTiles > 0)
			//{
			//	Tile* tile = _save->getTile(_save->getTileCoords(move.first));
			//	tile->setMarkerColor(_unit->getId()%100);
			//	tile->setPreview(10);
			//	tile->setTUMarker(move.second.visibleTiles);
			//}
			if (peekPreserveScore > bestPeekPreserveScore && move.second.visibleTiles > 0)
			{
				bestPeekPreserveScore = peekPreserveScore;
				peekPreserveCompromise = _save->getTileCoords(move.first);
				peakDirection = move.second.bestDirection;
			}
		}
	}
	if (bestAttackScore > 0 && haveTUToAttack)
	{
		_auditReason = "move-attack-position";
		_auditBestScore = bestAttackScore;
		_allowedToCheckAttack = true;
		travelTarget = bestAttackPosition;
	}
	else if (bestDirectPeakScore > 0 && newVisibleTilesDirect > 0 && haveTUToAttack)
	{
		_auditReason = "move-direct-peak";
		_auditBestScore = bestDirectPeakScore;
		travelTarget = bestDirectPeakPosition;
	}
	else if (sweepMode && bestFallbackScore > 0)
	{
		_auditReason = "move-sweep-fallback";
		_auditBestScore = bestFallbackScore;
		travelTarget = bestFallbackPosition;
		shouldEndTurnAfterMove = true;
	}
	else if (bestPeekPreserveScore > 0 && (sharedField != nullptr ? sharedField->maxAtExcluding(peekPreserveCompromise, _unit->getId()) : bestFriendReachable[peekPreserveCompromise]) <= [&]() {
		const std::map<Position, int, PositionComparator>& selfReach = getReachableBy(_unit, _ranOutOfTUs, false, true);
		auto srIt = selfReach.find(peekPreserveCompromise);
		return srIt != selfReach.end() ? srIt->second : 0;
	}())
	{
		_auditReason = "move-peek-preserve";
		_auditBestScore = bestPeekPreserveScore;
		if (_traceAI)
			Log(LOG_INFO) << "peekPreserveCompromise: " << peekPreserveCompromise << " score: " << bestPeekPreserveScore;
		travelTarget = peekPreserveCompromise;
		indirectPeek = true;
	}
	else if (bestGreatCoverScore > 0)
	{
		_auditReason = "move-great-cover";
		_auditBestScore = bestGreatCoverScore;
		travelTarget = bestGreatCoverPosition;
		if (!wantToPrime)
			shouldEndTurnAfterMove = true;
	}
	else if (bestGoodCoverScore > 0)
	{
		_auditReason = "move-good-cover";
		_auditBestScore = bestGoodCoverScore;
		travelTarget = bestGoodCoverPosition;
		shouldEndTurnAfterMove = true;
	}
	else if (bestOkayCoverScore > 0)
	{
		_auditReason = "move-okay-cover";
		_auditBestScore = bestOkayCoverScore;
		travelTarget = bestOkayCoverPosition;
		shouldEndTurnAfterMove = true;
	}
	else if (bestFallbackScore > 0)
	{
		_auditReason = "move-fallback";
		_auditBestScore = bestFallbackScore;
		travelTarget = bestFallbackPosition;
		shouldEndTurnAfterMove = true;
	}
	_auditBestTerms = {{_auditBestScore, bestAttackScore,
		std::max(bestGreatCoverScore, std::max(bestGoodCoverScore, bestOkayCoverScore))}};
	_auditTermLabels = {{"chosen", "attack", "cover"}};
	if (_traceAI)
	{
		auto orderedNext = [&](bool valid, const char *label, float score, const Position& pos)
		{
			if (!valid || !_auditRunnerUp.empty() || pos == travelTarget) return;
			_auditRunnerUp = std::string("ordered-next:") + label + "@"
				+ std::to_string(pos.x) + "," + std::to_string(pos.y) + "," + std::to_string(pos.z);
			_auditRunnerUpScore = score;
		};
		orderedNext(bestAttackScore > 0 && haveTUToAttack, "attack", bestAttackScore, bestAttackPosition);
		orderedNext(bestDirectPeakScore > 0 && newVisibleTilesDirect > 0 && haveTUToAttack,
			"direct-peak", bestDirectPeakScore, bestDirectPeakPosition);
		orderedNext(sweepMode && bestFallbackScore > 0, "sweep-fallback", bestFallbackScore, bestFallbackPosition);
		// The peek branch's friend-reachability predicate is short-circuited when an earlier
		// bucket wins. Do not rerun it for tracing; label the existing scored bucket honestly.
		orderedNext(bestPeekPreserveScore > 0, "peek-unvalidated", bestPeekPreserveScore, peekPreserveCompromise);
		orderedNext(bestGreatCoverScore > 0, "great-cover", bestGreatCoverScore, bestGreatCoverPosition);
		orderedNext(bestGoodCoverScore > 0, "good-cover", bestGoodCoverScore, bestGoodCoverPosition);
		orderedNext(bestOkayCoverScore > 0, "okay-cover", bestOkayCoverScore, bestOkayCoverPosition);
		orderedNext(bestFallbackScore > 0, "fallback", bestFallbackScore, bestFallbackPosition);
		if (_auditRunnerUp.empty()) _auditRunnerUp = "ordered-next:none";
	}

	// Phase 43 (H2): revive the civilian-hunt (34.4) / hearing (34.8) biases on the brutal path.
	// brutalThink synthesises a spawn-tile "last seen" for never-spotted enemies (getClosestSpawnTileId
	// above), so travelTarget is rarely myPos even with no genuinely known enemy -- it advances on the
	// X-Com spawn. The hunt/noise helpers gate on _knownEnemies == 0, so when a zone is known there is
	// no real target and the bias must OVERRIDE that synthetic-spawn advance (same intent as legacy
	// setupPatrol). Fair-knowledge only, no cheat.
	{
		Position huntZone;
		bool huntZoneKnown = wantsToHuntCivilians() && _save->getCivilianHuntZone(huntZone);
		Position noiseZone;
		bool noiseZoneKnown = !huntZoneKnown && wantsToInvestigateNoise()
			&& _save->getNewestHearableNoise(_unit->getPosition(), _intelligence, noiseZone);
		if (huntZoneKnown || noiseZoneKnown)
		{
			const Position& biasZone = huntZoneKnown ? huntZone : noiseZone;
			BattleActionCost biasReserve = BattleActionCost(_unit);
			Position biased = furthestToGoTowards(biasZone, biasReserve, _allPathFindingNodes);
			// Only override the cascade's travelTarget when the bias yields a real reachable step;
			// otherwise keep what the cascade chose rather than forcing the unit to idle.
			if (biased != myPos && candidateAllowed(BA_WALK, -1, biased))
			{
				if (_traceAI && travelTarget != myPos)
				{
					_auditRunnerUp = std::string("ordered-next:") + (_auditReason.empty() ? "cascade" : _auditReason)
						+ "@" + std::to_string(travelTarget.x) + "," + std::to_string(travelTarget.y)
						+ "," + std::to_string(travelTarget.z);
					_auditRunnerUpScore = _auditBestScore;
				}
				travelTarget = biased;
				selectedMoveTargetId = -1;
				_auditReason = huntZoneKnown ? "move-hunt-zone" : "move-noise-zone";
				if (_traceAI)
					Log(LOG_INFO) << "Phase 43 (H2): brutal unit biasing toward " << (huntZoneKnown ? "hunt" : "noise") << " zone " << biasZone;
			}
		}
	}

	if (travelTarget == myPos && saveDistance)
	{
		if (wantToPrime)
		{
			BattleItem* grenade = _unit->getGrenadeFromBelt(_save);
			primeCost = _unit->getActionTUs(BA_PRIME, grenade).Time + grenade->getMoveToCost(_save->getMod()->getInventoryLeftHand());
			if (primeCost <= _unit->getTimeUnits())
			{
				_unit->spendTimeUnits(grenade->getMoveToCost(_save->getMod()->getInventoryLeftHand()));
				_unit->spendCost(_unit->getActionTUs(BA_PRIME, grenade));
				grenade->setFuseTimer(0); // don't just spend the TUs for nothing! If we already circumvent the API anyways, we might as well actually prime the damn thing!
				_save->getBattleGame()->markAIWorldChanged();
				if (_traceAI)
					Log(LOG_INFO) << "I spent " << primeCost << " time-units on priming a grenade.";
				action->type = BA_RETHINK;
				action->number -= 1;
				return;
			}
		}
		improveItemization(myWeaponScore, action);
	}

	if (_traceAI)
	{
		Log(LOG_INFO) << "Brutal-AI wants to go from "
					  << myPos
					  << " to travel-target: " << travelTarget << " Remaining TUs: " << _unit->getTimeUnits() << " TU-cost: " << tuCostToReachPosition(travelTarget, _allPathFindingNodes);
		Log(LOG_INFO) << "My range is: "<<maxExtenderRangeWith(_unit, _unit->getTimeUnits()) <<" IAmPureMelee: " << IAmPureMelee;
		if (_tuCostToReachClosestPositionToBreakLos != -1)
			Log(LOG_INFO) << "I need to preserve " << _tuCostToReachClosestPositionToBreakLos << " to hide.";
	}
	if (travelTarget != myPos)
	{
		BattleActionCost reserved = BattleActionCost(_unit);
		action->target = furthestToGoTowards(travelTarget, reserved, _allPathFindingNodes);
		action->type = BA_WALK;
		action->run = wantToRun();
		action->aiTargetId = selectedMoveTargetId;
		action->aiHasFilteredFallback = true;
	} else
	{
		tryToPickUpGrenade(_unit->getTile(), action);
		action->target = myPos;
		if (!checkedAttack)
			action->type = BA_RETHINK;
		else
		{
			action->number -= 1;
			action->type = BA_NONE;
		}
	}
	
	if (_traceAI)
	{
		Log(LOG_INFO) << "Brutal-AI final goto-position from "
					  << myPos
					  << " to " << action->target;
	}
	shortestDist = 255;
	shouldHaveLofAfterMove |= winnerWasSpecialDoorCase;
	for (BattleUnit *target : *(_save->getUnits()))
	{
		if (!isEnemy(target, true) || target->isOut())
			continue;
		if (!_unit->isCheatOnMovement() && target->getTileLastSpotted(_unit->getFaction()) == -1)
			continue;
		Position targetPosition = target->getPosition();
		if (!_unit->isCheatOnMovement())
			targetPosition = _save->getTileCoords(target->getTileLastSpotted(_unit->getFaction()));
		bool haveLof = shouldHaveLofAfterMove;
		std::vector<Position> _trajectory;
		_trajectory.clear();
		if (hasTileSight(action->target, targetPosition))
			haveLof = true;
		if (!_unit->isCheatOnMovement())
			haveLof = haveLof || clearSight(action->target, targetPosition);
		if (!haveLof)
		{
			originAction.target = target->getPosition();
			Position origin = _save->getTileEngine()->getOriginVoxel(originAction, myTile);
			haveLof = _save->getTileEngine()->canTargetUnit(&origin, target->getTile(), nullptr, _unit, false);
			if (haveLof && Options::battleRealisticAccuracy)
			{
				if (_save->getTileEngine()->checkVoxelExposure(&origin, target->getTile(), _unit) < EPSILON)
					haveLof = false;
			}
		}
		if (!haveLof)
			continue;
		float currentDist = Position::distance(action->target, targetPosition);
		if (currentDist < shortestDist)
		{
			shortestDist = currentDist;
			unitToFaceTo = target;
		}
	}
	if (_traceAI && unitToFaceTo)
		Log(LOG_INFO) << "unit with closest distance after moving " << unitToFaceTo->getId() << " " << unitToFaceTo->getPosition() << " dist: " << shortestDist;
	action->finalFacing = -1;
	if (unitToFaceTo != NULL && shouldHaveLofAfterMove)
	{
		Position targetPosition = unitToFaceTo->getPosition();
		if (!_unit->isCheatOnMovement())
			targetPosition = _save->getTileCoords(unitToFaceTo->getTileLastSpotted(_unit->getFaction()));
		action->finalFacing = _save->getTileEngine()->getDirectionTo(action->target, targetPosition);
		if (_traceAI)
		{
			Log(LOG_INFO) << "Should face towards " << targetPosition << " which is " << action->finalFacing << " should have Lof after move: " << shouldHaveLofAfterMove << " winnerWasSpecialDoorCase: " << winnerWasSpecialDoorCase;
		}
		if (winnerWasSpecialDoorCase && travelTarget == myPos)
		{
			if (action->finalFacing == _unit->getDirection())
				action->type = BA_NONE;
			else
				action->type = BA_TURN;
			action->target = targetPosition;
		}
	}
	else
	{
		Tile* lookAtTile = NULL;
		if (unitToWalkTo != NULL)
		{
			Position targetPosition = unitToWalkTo->getPosition();
			if (!_unit->isCheatOnMovement())
				targetPosition = _save->getTileCoords(unitToWalkTo->getTileLastSpotted(_unit->getFaction()));
			if (_traceAI)
				Log(LOG_INFO) << "Should look at path towards " << targetPosition;
			std::vector<PathfindingNode*> myNodes = _save->getPathfinding()->findReachablePathFindingNodes(_unit, BattleActionCost(), dummy, true, NULL, &action->target, false, false, bam);
			lookAtTile = _save->getTile(closestToGoTowards(targetPosition, myNodes, action->target));
			if (lookAtTile && lookAtTile->getPosition() != action->target)
			{
				action->finalFacing = _save->getTileEngine()->getDirectionTo(action->target, lookAtTile->getPosition());
				if (_traceAI)
					Log(LOG_INFO) << "Facing corrected towards " << lookAtTile->getPosition() << " which is " << action->finalFacing;
			}
		}
	}
	if (indirectPeek)
	{
		action->finalFacing = peakDirection;
		if (_traceAI)
			Log(LOG_INFO) << "Overruling facing towards direction that reveals most tiles: " << action->finalFacing;
	}
	if (!_unit->getVisibleUnits()->empty() || contact || _save->getTileEngine()->isNextToDoor(myTile))
		shouldEndTurnAfterMove = false;
	if (shouldEndTurnAfterMove)
		_unit->setWantToEndTurn(true);
}

/**
 * Selects a target from a list of units seen by any unit for out-of-LOS actions and populates _attackAction with the relevant data
 * @return True if we have a target selected
 */
bool AIModule::brutalSelectSpottedUnitForSniper()
{
	_aggroTarget = 0;
	// Create a list of spotted targets and the type of attack we'd like to use on each
	std::vector<std::pair<BattleUnit *, BattleAction> > spottedTargets;

	BattleActionCost costThrow;
	// We know we have a grenade, now we need to know if we have the TUs to throw it
	costThrow.type = BA_THROW;
	costThrow.actor = _attackAction.actor;
	costThrow.weapon = _unit->getGrenadeFromBelt(_save);
	costThrow.updateTU();
	if (costThrow.weapon && !costThrow.weapon->isFuseEnabled())
	{
		costThrow.Time += costThrow.weapon->getMoveToCost(_save->getMod()->getInventoryLeftHand()); // Vanilla TUs for AI picking up grenade from belt
		costThrow += _attackAction.actor->getActionTUs(BA_PRIME, costThrow.weapon);
	}

	std::vector<BattleItem *> weapons;
	if (_attackAction.actor->getRightHandWeapon())
		weapons.push_back(_attackAction.actor->getRightHandWeapon());
	if (_attackAction.actor->getLeftHandWeapon())
		weapons.push_back(_attackAction.actor->getLeftHandWeapon());
	if (_attackAction.actor->getUtilityWeapon(BT_MELEE))
		weapons.push_back(_attackAction.actor->getUtilityWeapon(BT_MELEE));
	if (_attackAction.actor->getSpecialWeapon(BT_FIREARM))
		weapons.push_back(_attackAction.actor->getSpecialWeapon(BT_FIREARM));
	if (_grenade && _attackAction.actor->getGrenadeFromBelt(_save))
		weapons.push_back(_attackAction.actor->getGrenadeFromBelt(_save));

	float bestScore = 0;
	BattleAction chosenAction = _attackAction;
	BattleUnit* chosenTarget = _aggroTarget;
	for (std::vector<BattleUnit *>::const_iterator i = _save->getUnits()->begin(); i != _save->getUnits()->end(); ++i)
	{
		if (brutalValidTarget(*i))
		{
			for (BattleItem *weapon : weapons)
			{
				// Get the TU costs for each available attack type
				_attackAction.weapon = weapon;
				// Reset the others
				_aggroTarget = (*i);
				_attackAction.type = BA_RETHINK;
				_attackAction.target = (*i)->getPosition();
				BattleActionCost costAuto(BA_AUTOSHOT, _attackAction.actor,weapon);
				BattleActionCost costSnap(BA_SNAPSHOT, _attackAction.actor, weapon);
				BattleActionCost costAimed(BA_AIMEDSHOT, _attackAction.actor, weapon);
				BattleActionCost costHit(BA_HIT, _attackAction.actor, weapon);
				// M4 (Calypso): costThrow is declared once OUTSIDE this target×weapon loop, so
				// adding the break-LOS cost to it here compounded every iteration (phantom TU for
				// late candidates). Add it to a fresh per-candidate copy instead; the per-shot
				// costs below are already re-made each iteration so they stay correct.
				BattleActionCost costThrowIter = costThrow;
				if (_tuCostToReachClosestPositionToBreakLos > 0)
				{
					costThrowIter.Time += _tuCostToReachClosestPositionToBreakLos;
					costThrowIter.Energy += _energyCostToReachClosestPositionToBreakLos;
					costAuto.Time += _tuCostToReachClosestPositionToBreakLos;
					costAuto.Energy += _energyCostToReachClosestPositionToBreakLos;
					costSnap.Time += _tuCostToReachClosestPositionToBreakLos;
					costSnap.Energy += _energyCostToReachClosestPositionToBreakLos;
					costAimed.Time += _tuCostToReachClosestPositionToBreakLos;
					costAimed.Energy += _energyCostToReachClosestPositionToBreakLos;
					costHit.Time += _tuCostToReachClosestPositionToBreakLos;
					costHit.Energy += _energyCostToReachClosestPositionToBreakLos;
				}
				float evaluatedScore = 0.0f;
				BattleActionType evaluatedAction = BA_RETHINK;
				std::array<float, 3> evaluatedTerms{{0, 0, 0}};
				float score = brutalExtendedFireModeChoice(costAuto, costSnap, costAimed, costThrowIter, costHit, true, bestScore, &evaluatedScore, &evaluatedAction, &evaluatedTerms);
				// Phase 34.9 (Calypso): soft focus-fire cap (ported path). The plan's anchor
				// "brutalValidTarget" is a bool validity filter; the actual target RANKING happens
				// here, so the down-weight lands on this score. A target >= focusFireCommitThreshold
				// squadmates already committed to is reduced by focusFireScorePercent/100.0f so fire
				// spreads; a single dogpiled target still competes (score stays > 0 => still chosen
				// if it is the only viable one). Gated; empty board when off => no change => byte-identical.
				if (score > 0.0f && _save->getMod()->getAISquadCoordination()
					&& _save->getSquadAssignedAttackers(_unit->getFaction(), (*i)->getId()) >= _save->getMod()->getAIFocusFireCommitThreshold())
				{
					// Phase 43.0 item 7: commit count + reduction percent are now mod-tunable (defaults 2 / 50 => 0.5f).
					score *= _save->getMod()->getAIFocusFireScorePercent() / 100.0f;
					evaluatedScore *= _save->getMod()->getAIFocusFireScorePercent() / 100.0f;
				}
				// Phase 34.9 (Calypso): pin-and-flank (ported path) -- press a target pinned by
				// suppression (34.7). No distinct flanking-move seam exists, so the preference is
				// applied here at target ranking. Gated; unchanged when off (byte-identical).
				// Phase 43 (H4): only boost a pinned target if this candidate actually produced a firing
				// action. brutalExtendedFireModeChoice leaves _attackAction.type == BA_RETHINK (and returns
				// the incoming bestScore unchanged) when there is no viable fire mode; multiplying that
				// pass-through by 1.25 would let an unreachable pinned unit beat a real attack and be chosen
				// with a BA_RETHINK action (the valid attack silently discarded).
				if (score > 0.0f && _attackAction.type != BA_RETHINK && _save->getMod()->getAISquadCoordination() && (*i)->isPinned())
				{
					score *= 1.25f;
					evaluatedScore *= 1.25f;
				}
				if (score > bestScore)
				{
					_auditRunnerUp = bestScore > 0.0f
						? (std::to_string(static_cast<int>(chosenAction.type)) + ":" + std::to_string(chosenTarget ? chosenTarget->getId() : -1))
						: "none";
					_auditRunnerUpScore = _auditBestScore;
					_auditReason = "highest-fire-score";
					_auditBestScore = score;
					_auditBestTerms = evaluatedTerms;
					bestScore = score;
					chosenAction = _attackAction;
					chosenTarget = _aggroTarget;
				}
				else if (evaluatedScore > _auditRunnerUpScore)
				{
					_auditRunnerUp = std::to_string(static_cast<int>(evaluatedAction)) + ":" + std::to_string((*i)->getId());
					_auditRunnerUpScore = evaluatedScore;
				}
			}
		}
	}
	_aggroTarget = chosenTarget;
	_attackAction.type = chosenAction.type;
	_attackAction.weapon = chosenAction.weapon;
	_attackAction.target = chosenAction.target;
	_attackAction.aiTargetId = chosenAction.aiTargetId;
	_attackAction.aiHasFilteredFallback = chosenAction.aiHasFilteredFallback;

	if (bestScore == 0)
	{
		_aggroTarget = 0;
		_attackAction.type = BA_RETHINK;
		_attackAction.weapon = _unit->getMainHandWeapon(false);
	}
	return _aggroTarget != 0;
}

int AIModule::tuCostToReachPosition(Position pos, const std::vector<PathfindingNode*>& nodeVector, BattleUnit* actor, bool forceExactPosition, bool energyInsteadOfTU)
{
	float closestDistToTarget = 3;
	int tuCostToClosestNode = 10000;
	Tile *posTile = _save->getTile(pos);
	if (!posTile)
		return tuCostToClosestNode;
	if (actor == NULL)
		actor = _unit;
	for (auto pn : nodeVector)
	{
		if (pos == pn->getPosition())
			return pn->getTUCost(false).time;
		if (forceExactPosition)
			continue;
		Tile *tile = _save->getTile(pn->getPosition());
		if (pos.z != pn->getPosition().z)
			continue;
		if (!posTile->hasNoFloor() && tile->hasNoFloor() && actor->getMovementType() != MT_FLY)
			continue;
		float currDist = Position::distance(pos, pn->getPosition());
		if (currDist < closestDistToTarget)
		{
			if (hasTileSight(pn->getPosition(), pos))
			{
				closestDistToTarget = currDist;
				if (energyInsteadOfTU)
					tuCostToClosestNode = pn->getTUCost(false).energy;
				else
					tuCostToClosestNode = pn->getTUCost(false).time;
			}
		}
	}
	return tuCostToClosestNode;
}

Position AIModule::furthestToGoTowards(Position target, BattleActionCost reserved, const std::vector<PathfindingNode *>& nodeVector, bool encircleTileMode, Tile *encircleTile)
{
	//consider time-units we already spent
	reserved.Time = _unit->getTimeUnits() - reserved.Time;
	reserved.Energy = _unit->getEnergy();
	//We need to consider the cost of standing up
	if (_unit->isKneeled())
	{
		reserved.Time -= _unit->getKneelUpCost();
	}
	PathfindingNode *targetNode = NULL;
	int closestDistToTarget = 255;
	for (auto pn : nodeVector)
	{
		if (target == pn->getPosition())
		{
			targetNode = pn;
			break;
		}
		// If we want to get close to the target it must be on the same layer
		if (target.z != pn->getPosition().z)
		{
			if (target.z > pn->getPosition().z)
			{
				Tile *targetTile = _save->getTile(target);
				Tile *tileAbovePathNode = _save->getAboveTile(_save->getTile(pn->getPosition()));
				if (!targetTile->hasNoFloor() && !tileAbovePathNode->hasNoFloor())
					continue;
			}
			if (target.z < pn->getPosition().z)
			{
				Tile *tileAbovetargetTile = _save->getAboveTile(_save->getTile(target));
				Tile *pathNodeTile = _save->getTile(pn->getPosition());
				if (!tileAbovetargetTile->hasNoFloor() && !pathNodeTile->hasNoFloor())
					continue;
			}
		}
		int currDist = Position::distance(target, pn->getPosition());
		if (currDist < closestDistToTarget)
		{
			closestDistToTarget = currDist;
			targetNode = pn;
		}
	}
	if (targetNode != NULL)
	{
		if (encircleTileMode)
		{
			PathfindingNode *furthestNodeThatWasDangerous = targetNode;
			while (targetNode->getPrevNode() != NULL)
			{
				bool nodeIsDangerous = false;
				Tile *tile = _save->getTile(targetNode->getPosition());
				for (BattleUnit *unit : *(_save->getUnits()))
				{
					if (unit->isOut())
						continue;
					if (isAlly(unit))
						continue;
					if ((_unit->isCheatOnMovement() || brutalValidTarget(unit, true)) && hasTileSight(unit->getPosition(), tile->getPosition()))
						nodeIsDangerous = true;
					if (!_unit->isCheatOnMovement() && unit->getTileLastSpotted(_unit->getFaction()) >= 0)
					{
						Position unitPos = _save->getTileCoords(unit->getTileLastSpotted(_unit->getFaction()));
						nodeIsDangerous = quickLineOfFire(targetNode->getPosition(), unit, false, !_unit->isCheatOnMovement());
						nodeIsDangerous = nodeIsDangerous || clearSight(targetNode->getPosition(), unitPos);
					}
					if (nodeIsDangerous)
						break;
				}
				if (nodeIsDangerous)
					furthestNodeThatWasDangerous = targetNode;
				targetNode = targetNode->getPrevNode();
			}
			if (furthestNodeThatWasDangerous->getPrevNode() != NULL)
				return furthestNodeThatWasDangerous->getPrevNode()->getPosition();
		}
		else
		{
			bool haveLosToEncircleTile = true;
			if (encircleTile != NULL && _unit->getTimeUnits() == _unit->getBaseStats()->tu && targetNode->getTUCost(false).time <= 8)
				haveLosToEncircleTile = false;
			while ((targetNode->getTUCost(false).time > reserved.Time || targetNode->getTUCost(false).energy > reserved.Energy || (haveLosToEncircleTile && encircleTile != NULL)) && targetNode->getPrevNode() != NULL)
			{
				targetNode = targetNode->getPrevNode();
				if (encircleTile != NULL)
				{
					if (clearSight(targetNode->getPosition(), encircleTile->getPosition()) && (_unit->getTimeUnits() < _unit->getBaseStats()->tu || targetNode->getTUCost(false).time > 8))
						haveLosToEncircleTile = true;
					else
						haveLosToEncircleTile = false;
				}
			}
			return targetNode->getPosition();
		}
	}
	return _unit->getPosition();
}

Position AIModule::closestToGoTowards(Position target, const std::vector<PathfindingNode *>& nodeVector, Position myPos, bool peakMode)
{
	PathfindingNode *targetNode = NULL;
	float closestDistToTarget = 255;
	for (auto pn : nodeVector)
	{
		if (target == pn->getPosition())
		{
			targetNode = pn;
			break;
		}
		// If we want to get close to the target it must be on the same layer
		if (target.z != pn->getPosition().z)
		{
			if (target.z > pn->getPosition().z)
			{
				Tile *targetTile = _save->getTile(target);
				Tile *tileAbovePathNode = _save->getAboveTile(_save->getTile(pn->getPosition()));
				if (!targetTile->hasNoFloor() && !tileAbovePathNode->hasNoFloor())
					continue;
			}
			if (target.z < pn->getPosition().z)
			{
				Tile *tileAbovetargetTile = _save->getAboveTile(_save->getTile(target));
				Tile *pathNodeTile = _save->getTile(pn->getPosition());
				if (!tileAbovetargetTile->hasNoFloor() && !pathNodeTile->hasNoFloor())
					continue;
			}
		}
		float currDist = Position::distance(target, pn->getPosition());
		if (currDist < closestDistToTarget)
		{
			closestDistToTarget = currDist;
			targetNode = pn;
		}
	}
	if (targetNode != NULL)
	{
		while (targetNode->getPrevNode() != NULL && targetNode->getPrevNode()->getPosition() != myPos)
		{
			if (peakMode && hasTileSight(myPos, targetNode->getPrevNode()->getPosition()))
			{
				//if (_traceAI)
				//{
				//	Tile* tile = _save->getTile(targetNode->getPosition());
				//	tile->setMarkerColor(_unit->getId());
				//	tile->setPreview(10);
				//	tile->setTUMarker(_unit->getId() % 1000);
				//}
				return targetNode->getPosition();
			}
			targetNode = targetNode->getPrevNode();
		}
		return targetNode->getPosition();
	}
	return myPos;
}

bool AIModule::isPathToPositionSave(Position target, bool &saveForProxies)
{
	PathfindingNode *targetNode = NULL;
	for (auto pn : _allPathFindingNodes)
	{
		if (target == pn->getPosition())
		{
			targetNode = pn;
			break;
		}
	}
	bool save = true;
	if (targetNode != NULL)
	{
		while (targetNode->getPrevNode() != NULL)
		{
			Tile *tile = _save->getTile(targetNode->getPosition());
			if (_unit->isAvoidMines())
			{
				for (int x = -1; x <= 1; ++x)
					for (int y = -1; y <= 1; ++y)
					{
						for (int x2 = 0; x2 < _unit->getArmor()->getSize(); ++x2)
							for (int y2 = 0; y2 < _unit->getArmor()->getSize(); ++y2)
							{
								Position posToCheck = tile->getPosition();
								posToCheck.x += x + x2;
								posToCheck.y += y + y2;
								Tile* tileToCheck = _save->getTile(posToCheck);
								if (tileToCheck)
								{
									for (BattleItem* item : *(tileToCheck->getInventory()))
									{
										if (item->isFuseEnabled() && item->getRules()->getDamageType()->RandomType != DRT_NONE && !item->getRules()->isHiddenOnMinimap())
										{
											bool willBeHit = false;
											if (tileToCheck != tile || tileToCheck == tile)
												if (_save->getTileEngine()->horizontalBlockage(tileToCheck, tile, DT_HE) >= item->getRules()->getPower())
													willBeHit = false;
												else
													willBeHit = true;
											else
												willBeHit = true;
											if (willBeHit)
											{
												float damage = item->getRules()->getPower();
												damage *= _unit->getArmor()->getDamageModifier(item->getRules()->getDamageType()->ResistType);
												float damageRange = 1.0 + _save->getMod()->DAMAGE_RANGE / 100.0;
												damage = (damage * damageRange - _unit->getArmor()->getUnderArmor()) / 2.0f;
												damage *= _unit->getArmor()->getSize() * _unit->getArmor()->getSize(); //take into account that large units get hit multiple times
												if (damage * 2.0 > _unit->getHealth() - _unit->getStunlevel())
													saveForProxies = false;
											}
										}
									}
								}
							}
					}
			}
			// If we can't see the previous node despite being on the same level, the only plausible reason is there's a closed door. And if there's a closed door, we'd pop out. So any proxies we've seen before would not be triggered and the path is safe up until the door.
			if (targetNode->getPosition().z == targetNode->getPrevNode()->getPosition().z && !hasTileSight(targetNode->getPosition(), targetNode->getPrevNode()->getPosition()))
				saveForProxies = true;
			targetNode = targetNode->getPrevNode();
		}
	}
	return save;
}

bool AIModule::brutalPsiAction()
{
	BattleItem *item = _unit->getUtilityWeapon(BT_PSIAMP);
	if (!item)
	{
		return false;
	}

	const int costLength = 3;
	BattleActionCost cost[costLength] =
		{
			BattleActionCost(BA_USE, _unit, item),
			BattleActionCost(BA_PANIC, _unit, item),
			BattleActionCost(BA_MINDCONTROL, _unit, item),
		};
	bool have = false;
	for (int j = 0; j < costLength; ++j)
	{
		if (_tuCostToReachClosestPositionToBreakLos > 0)
		{
			cost[j].Time += _tuCostToReachClosestPositionToBreakLos;
			cost[j].Energy += _energyCostToReachClosestPositionToBreakLos;
		}
		if (cost[j].Time > 0)
		{
			cost[j].Time;
			cost[j].Energy;
			have |= cost[j].haveTU();
		}
	}
	bool LOSRequired = item->getRules()->isLOSRequired();

	_aggroTarget = 0;
	BattleUnit *bestPsiTarget = NULL;
	float highestPsiScore = 0;

	if (have)
	{
		BattleActionType typeToAttack = BA_NONE;
		for (std::vector<BattleUnit *>::const_iterator i = _save->getUnits()->begin(); i != _save->getUnits()->end(); ++i)
		{
			// don't target tanks
			if ((*i)->getArmor()->getSize() == 1 &&
				// civilians must be armed to be considered psi-targets
				((*i)->getMainHandWeapon() || (*i)->getFaction() != FACTION_NEUTRAL) &&
				(!LOSRequired ||
				 std::find(_unit->getVisibleUnits()->begin(), _unit->getVisibleUnits()->end(), *i) != _unit->getVisibleUnits()->end()) &&
				brutalValidTarget(*i, true, true)
				)
			{
				BattleUnit *victim = (*i);
				if (item->getRules()->isOutOfRange(_unit->distance3dToUnitSq(victim)))
				{
					continue;
				}
				// No need to use psi against units that are already panicking or mind-controlled (MindControllerId is also set for panic)
				if (victim->getStatus() == STATUS_PANICKING || victim->getStatus() == STATUS_BERSERK)
				{
					continue;
				}
				for (int j = 0; j < costLength; ++j)
				{
					// can't use this attack.
					if (!cost[j].haveTU())
					{
						continue;
					}
					float psiActionScore = _save->getTileEngine()->psiAttackCalculate({cost[j].type, _unit, item, item}, victim);

					// low chance we hit this target.
					if (psiActionScore < 0)
					{
						continue;
					}
					// when we rolled a 55 or higher on our test-attempt, we are guaranteed to hit which maximizes the successMod's impact on the final score
					psiActionScore = std::min(psiActionScore, 55.0f) / 55.0f;

					// different bonus per attack.
					if (cost[j].type == BA_MINDCONTROL)
					{
						// target cannot be mind controlled
						if (victim->getUnitRules() && !victim->getUnitRules()->canBeMindControlled())
							continue;
					}
					else if (cost[j].type == BA_PANIC)
					{
						// target cannot be panicked
						if (victim->getUnitRules() && !victim->getUnitRules()->canPanic())
							continue;
						psiActionScore *= std::min(victim->getMorale(), 110 - victim->getBaseStats()->bravery) / 100.0;
					}
					if (psiActionScore > highestPsiScore)
					{
						highestPsiScore = psiActionScore;
						bestPsiTarget = victim;
						typeToAttack = cost[j].type;
					}
				}
			}
		}
		if (bestPsiTarget != NULL)
		{
			_aggroTarget = bestPsiTarget;
			_psiAction.type = typeToAttack;
		}
		if (!_aggroTarget)
			return false;

		if (_traceAI)
		{
			Log(LOG_INFO) << "making a psionic attack against " << _aggroTarget->getId();
		}
		_psiAction.target = _aggroTarget->getPosition();
		_psiAction.weapon = item;
		return true;
	}
	return false;
}

float AIModule::brutalExtendedFireModeChoice(BattleActionCost &costAuto, BattleActionCost &costSnap, BattleActionCost &costAimed, BattleActionCost &costThrow, BattleActionCost &costHit, bool checkLOF, float previousHighScore, float *evaluatedBestScore, BattleActionType *evaluatedBestAction, std::array<float, 3> *evaluatedTerms)
{
	std::vector<BattleActionType> attackOptions = {};
	if (costAimed.haveTU())
	{
		attackOptions.push_back(BA_AIMEDSHOT);
	}
	if (costAuto.haveTU())
	{
		attackOptions.push_back(BA_AUTOSHOT);
	}
	if (costSnap.haveTU())
	{
		attackOptions.push_back(BA_SNAPSHOT);
	}
	if (costThrow.haveTU())
	{
		attackOptions.push_back(BA_THROW);
	}
	if (costHit.haveTU())
	{
		attackOptions.push_back(BA_HIT);
	}

	BattleActionType chosenAction = BA_RETHINK;
	BattleAction testAction = _attackAction;
	BattleAction chosenBattleAction = _attackAction;
	float score = previousHighScore;
	float candidateBest = 0.0f;
	BattleActionType candidateBestAction = BA_RETHINK;
	std::array<float, 3> candidateBestTerms{{0, 0, 0}};
	Position originPosition = _unit->getPosition();
	//first check our actions from the current tile
	for (auto &i : attackOptions)
	{
		testAction.type = i;
		if (!candidateAllowed(i, _aggroTarget ? _aggroTarget->getId() : -1, testAction.target))
		{
			if (_traceAI)
			{
				Log(LOG_INFO) << "AI_FAILURE_BLOCK action=" << static_cast<int>(i)
					<< " target=" << (_aggroTarget ? _aggroTarget->getId() : -1)
					<< " pos=" << testAction.target;
			}
			continue;
		}
		float newScore = brutalScoreFiringMode(&testAction, _aggroTarget, checkLOF);
		if (newScore > candidateBest)
		{
			candidateBest = newScore;
			candidateBestAction = i;
			candidateBestTerms = _lastScoreTerms;
		}

		if (newScore > score)
		{
			score = newScore;
			chosenBattleAction.type = i;
			chosenBattleAction.weapon = _attackAction.weapon;
			chosenBattleAction.aiTargetId = _aggroTarget ? _aggroTarget->getId() : -1;
			chosenBattleAction.aiHasFilteredFallback = true;
		}
	}
	_attackAction = chosenBattleAction;
	if (evaluatedBestScore) *evaluatedBestScore = candidateBest;
	if (evaluatedBestAction) *evaluatedBestAction = candidateBestAction;
	if (evaluatedTerms) *evaluatedTerms = candidateBestTerms;
	return score;
}

/**
 * Phase 34.7 (Calypso): the suppression value of one auto-volley from `weapon`. This is the
 * "AI usage" half of the suppression slice -- when direct hit chance is poor but a target is
 * exposed, volume fire still has pinning value (morale/energy drain on near-misses), so the
 * AI should take the auto-shot instead of holding fire.
 *
 * The value is the per-near-miss knob sum (ai.suppressionMorale + ai.suppressionEnergy)
 * scaled by the volley's shot count -- an auto-volley that throws 3 bullets can pin via 3
 * near-misses. Returns 0 (no bonus) when:
 *   - ai.suppression is off (the gate; flag off => byte-identical, the caller adds 0),
 *   - the weapon has no auto-fire config (no volley to value),
 *   - ammo margin is insufficient: the shooter must be able to spare this volley AND still
 *     have at least one full spare volley left (`ammoQty >= 2 * autoShots`). This stops the
 *     AI from burning its last rounds on speculative suppression.
 * The magnitude is deliberately small relative to a high-damage direct hit's score, so the
 * bonus only tips the balance for marginal (low-accuracy) shots -- exactly the design intent.
 */
float AIModule::suppressionVolleyValue(BattleItem* weapon) const
{
	// Gated by the master switch; flag off => +0 => byte-identical.
	if (!_save->getMod()->getAISuppression()) return 0.0f;
	if (!weapon) return 0.0f;
	const auto* conf = weapon->getRules()->getConfigAuto();
	const int autoShots = conf ? conf->shots : 0;
	if (autoShots <= 0) return 0.0f;
	// Ammo margin: keep at least one full spare volley. getAmmoForAction(BA_AUTOSHOT) returns
	// the loaded ammo clip (or null) without spending it.
	BattleItem* ammo = weapon->getAmmoForAction(BA_AUTOSHOT);
	if (!ammo || ammo->getAmmoQuantity() < 2 * autoShots) return 0.0f;
	return float(_save->getMod()->getAISuppressionMorale() + _save->getMod()->getAISuppressionEnergy()) * float(autoShots);
}

/**
 * Phase 34.9 (Calypso): record this hostile's declared squad intent from its finalized action on
 * the faction blackboard. Called at the tail of both the legacy dispatch and the ported brutal
 * path. Classification from the action type: a direct attack (incl. a suppressing auto-volley)
 * on _aggroTarget is ATTACK; a walk while a known enemy is selected is FLANK (repositioning /
 * closing on it -- there is no distinct "flanking helper" in either path, so the move-while-
 * -targeting is the faithful flank signal); a desperate escape is RETREAT. GATED: no-op unless
 * ai.squadCoordination is on and the unit is FACTION_HOSTILE (squad coordination is a hostile-AI
 * behavior per the DoD -- civilians stay on Phase 32 logic), so the flag-off path is byte-identical.
 */
void AIModule::declareSquadIntentFromAction(const BattleAction* action) const
{
	if (!_save->getMod()->getAISquadCoordination()) return;
	if (_unit->getFaction() != FACTION_HOSTILE) return;
	const BattleActionType t = action->type;
	const bool isAttack = (t == BA_SNAPSHOT || t == BA_AUTOSHOT || t == BA_AIMEDSHOT ||
	                       t == BA_HIT || t == BA_THROW || t == BA_LAUNCH);
	const int turn = _save->getTurn();
	SquadIntent intent = SquadIntent::NONE;
	int targetId = -1;
	if (isAttack && _aggroTarget)
	{
		intent = SquadIntent::ATTACK;
		targetId = _aggroTarget->getId();
		// Phase 43 (H5a): remember this turn's committed attack so a later idle (BA_NONE) think in
		// the same activation does not blank the attacker off the board.
		_committedAttackTargetId = targetId;
		_committedAttackTurn = turn;
	}
	else if (t == BA_WALK && _aggroTarget && isEnemy(_aggroTarget))
	{
		// Phase 43 (H5b): only declare FLANK against a real enemy -- on the brutal path _aggroTarget
		// during a move can be a stale legacy selectNearestTarget() pick (including a civilian).
		intent = SquadIntent::FLANK;
		targetId = _aggroTarget->getId();
	}
	else if (action->desperate)
	{
		intent = SquadIntent::RETREAT;
	}
	else if (_committedAttackTurn == turn && _committedAttackTargetId >= 0)
	{
		// Phase 43 (H5a): this think produced no fresh intent, but the unit already committed to an
		// attack earlier this turn -- re-declare it so squadmates still read the real attacker.
		intent = SquadIntent::ATTACK;
		targetId = _committedAttackTargetId;
	}
	if (intent != SquadIntent::NONE)
	{
		_save->declareSquadIntent(FACTION_HOSTILE, _unit->getId(), intent, targetId);
	}
}

/**
 * Scores a firing mode for a particular target based on a damage / TUs ratio
 * @param action Pointer to the BattleAction determining the firing mode
 * @param target Pointer to the BattleUnit we're trying to target
 * @param checkLOF Set to true if you want to check for a valid line of fire
 * @return The calculated score
 */
float AIModule::brutalScoreFiringMode(BattleAction* action, BattleUnit* target, bool checkLOF, bool reactionCheck)
{
	// Sanity check first, if the passed action has no type or weapon, return 0.
	if (!action->type || !action->weapon)
	{
		return 0;
	}

	// Get base accuracy for the action
	float accuracy = BattleUnit::getFiringAccuracy(BattleActionAttack::GetBeforeShoot(*action), _save->getBattleGame()->getMod());

	Position originPosition = _unit->getPosition();
	int distanceSq = Position::distanceSq(originPosition, target->getPosition());
	if (!checkLOF)
		distanceSq = Position::distanceSq(originPosition, _save->getTileCoords(target->getTileLastSpotted(_unit->getFaction(), true)));
	float distance = Position::distance(originPosition, target->getPosition());

	int tuTotal = _unit->getTimeUnits();
	int energyTotal = _unit->getEnergy();
	float dangerMod = 1;
	float explosionMod = 1;

	if (Options::battleUFOExtenderAccuracy && action->type != BA_THROW)
	{
		int upperLimit;
		if (action->type == BA_AIMEDSHOT)
		{
			upperLimit = action->weapon->getRules()->getAimRange();
		}
		else if (action->type == BA_AUTOSHOT)
		{
			upperLimit = action->weapon->getRules()->getAutoRange();
		}
		else
		{
			upperLimit = action->weapon->getRules()->getSnapRange();
		}
		int lowerLimit = action->weapon->getRules()->getMinRange();

		if (distance > upperLimit)
		{
			accuracy -= (distance - upperLimit) * action->weapon->getRules()->getDropoff();
		}
		else if (distance < lowerLimit)
		{
			accuracy -= (lowerLimit - distance) * action->weapon->getRules()->getDropoff();
		}
	}
	if (action->weapon->getRules()->getNoLOSAccuracyPenalty(const_cast<Mod*>(_save->getMod())) != -1)
	{
		Tile* targetTile = target->getTile();
		bool shouldHaveLos = true;
		if (targetTile)
		{
			int viewDistance = _unit->getMaxViewDistanceAtDay(target);
			if (target->getTile()->getShade() > _save->getMod()->getMaxDarknessToSeeUnits() && target->getTile()->getFire() == 0)
				viewDistance = _unit->getMaxViewDistanceAtDark(target);
			float minViewDistance = _save->getMod()->getMaxViewDistance() / (1.0 + targetTile->getSmoke() / 3.0);
			viewDistance = std::min(viewDistance, (int)minViewDistance);
			if (Position::distance(originPosition, target->getPosition()) > viewDistance)
				shouldHaveLos = false;
		}
		else
			shouldHaveLos = false;
		if (!shouldHaveLos)
			accuracy *= action->weapon->getRules()->getNoLOSAccuracyPenalty(const_cast<Mod*>(_save->getMod())) / 100.0;
	}

	if (action->type != BA_THROW && action->weapon->getRules()->isOutOfRange(distanceSq))
		accuracy = 0;
	if (action->type == BA_HIT)
	{
		Position attackVexel = originPosition.toVoxel();
		attackVexel += Position(8, 8, 0) * _unit->getArmor()->getSize();
		int arc = _save->getTileEngine()->getArcDirection(_save->getTileEngine()->getDirectionTo(target->getPositionVexels(), attackVexel), target->getDirection());
		float penalty = 1.0f - arc * target->getArmor()->getMeleeDodgeBackPenalty() / 4.0f;
		if (target->getArmor()->getMeleeDodge(target) * penalty < accuracy)
			accuracy -= target->getArmor()->getMeleeDodge(target) * penalty;
		else
			accuracy = 0.01;
		// We can definitely assume we'll be facing the target
		int directionToLook = _save->getTileEngine()->getDirectionTo(originPosition, target->getPosition());
		if (checkLOF)
		{
			if (!_save->getTileEngine()->validMeleeRange(originPosition, directionToLook, _unit, target, 0))
				accuracy = 0;
		}
	}
	else if (shouldAvoidMeleeRange(target) && distance < 2)
	{
		accuracy = 0;
	}

	float numberOfShots = 1;
	if (action->type == BA_AIMEDSHOT)
	{
		numberOfShots = action->weapon->getRules()->getConfigAimed()->shots;
	}
	else if (action->type == BA_SNAPSHOT)
	{
		numberOfShots = action->weapon->getRules()->getConfigSnap()->shots;
	}
	else if (action->type == BA_AUTOSHOT)
	{
		numberOfShots = action->weapon->getRules()->getConfigAuto()->shots;
	}
	else if (action->type == BA_HIT)
	{
		numberOfShots = action->weapon->getRules()->getConfigMelee()->shots;
	}

	int tuCost = _unit->getActionTUs(action->type, action->weapon).Time;
	int energyCost = _unit->getActionTUs(action->type, action->weapon).Energy;
	// Return a score of zero if this firing mode doesn't exist for this weapon
	if (!tuCost)
		return 0;
	// Need to include TU cost of getting grenade from belt + priming if we're checking throwing
	float damage = 0;
	RuleDamageType damageType = *action->weapon->getRules()->getDamageType();
	if (action->type == BA_THROW && action->weapon == _unit->getGrenadeFromBelt(_save))
	{
		if (!_grenade)
			return 0;
		if (target->getTile()->getDangerous())
			return 0;
		if (!_unit->getGrenadeFromBelt(_save)->isFuseEnabled())
		{
			tuCost += action->weapon->getMoveToCost(_save->getMod()->getInventoryLeftHand());
			tuCost += _unit->getActionTUs(BA_PRIME, _unit->getGrenadeFromBelt(_save)).Time;
		}
		// We don't have several shots but we can hit several targets at once
		BattleItem *grenade = action->weapon;
		auto radius = grenade->getRules()->getExplosionRadius(BattleActionAttack::GetBeforeShoot(*action));
		if (checkLOF)
			explosionMod = brutalExplosiveEfficacy(target->getPosition(), _unit, radius, true);
		else
			explosionMod = brutalExplosiveEfficacy(_save->getTileCoords(target->getTileLastSpotted(_unit->getFaction(), true)), _unit, radius, true);
		explosionMod *= grenadeRiddingUrgency();
	}
	else
	{
		if (action->type == BA_THROW && action->weapon != _unit->getGrenadeFromBelt(_save))
			return 0;
		auto ammo = action->weapon->getAmmoForAction(action->type);
		if (ammo)
		{
			damage = ammo->getRules()->getPower();
			int radius = ammo->getRules()->getExplosionRadius({action->type, _unit, _attackAction.weapon, ammo});
			if (radius > 0)
				explosionMod *= brutalExplosiveEfficacy(target->getPosition(), _unit, radius, false);
			if (ammo->getRules()->getShotgunPellets() > 0)
				numberOfShots *= ammo->getRules()->getShotgunPellets();
			damageType = *ammo->getRules()->getDamageType();
		}
		else
			return 0;
	}
	// I had to make it mutually exclusive from ammo-damage because that way I wouldn't have power from lasers twice. This seems okay for vanilla but might be wrong for other stuff.
	if (action->weapon->getRules()->getPowerBonus(BattleActionAttack::GetBeforeShoot(*action)))
		damage = action->weapon->getRules()->getPowerBonus(BattleActionAttack::GetBeforeShoot(*action));
	float relevantArmor = 0;
	if (action->type == BA_THROW)
	{
		relevantArmor = target->getArmor()->getUnderArmor();
	}
	else
	{
		UnitSide side = getSideFacingToPosition(target, originPosition);
		if (side == SIDE_FRONT || side == SIDE_RIGHT || side == SIDE_LEFT || side == SIDE_REAR || side == SIDE_UNDER)
			relevantArmor = target->getArmor()->getArmor(side);
		else if (side == SIDE_LEFT_FRONT)
			relevantArmor = (target->getArmor()->getArmor(SIDE_LEFT) + target->getArmor()->getArmor(SIDE_FRONT)) / 2.0;
		else if (side == SIDE_RIGHT_FRONT)
			relevantArmor = (target->getArmor()->getArmor(SIDE_RIGHT) + target->getArmor()->getArmor(SIDE_FRONT)) / 2.0;
		else if (side == SIDE_LEFT_REAR)
			relevantArmor = (target->getArmor()->getArmor(SIDE_LEFT) + target->getArmor()->getArmor(SIDE_REAR)) / 2.0;
		else if (side == SIDE_RIGHT_REAR)
			relevantArmor = (target->getArmor()->getArmor(SIDE_RIGHT) + target->getArmor()->getArmor(SIDE_REAR)) / 2.0;
	}
	float damageRange = 1.0 + _save->getMod()->DAMAGE_RANGE / 100.0;
	damage *= target->getArmor()->getDamageModifier(damageType.ResistType);
	float armorPreDamage = damageType.getArmorPreFinalDamage(damage);
	damage = (damage * damageRange - relevantArmor) / 2.0f;
	if (reactionCheck)
		damage = std::max(1.0f, damage);
	else
		damage = std::max(0.0f, damage);
	if (damage <= 0 && armorPreDamage <= 0)
		return 0;
	float damageTypeMod = 0;
	if (damage > 0)
	{
		damageTypeMod += damageType.getHealthFinalDamage(damage) / damage;
		damageTypeMod += damageType.getWoundFinalDamage(damage) / damage;
		damageTypeMod += damageType.getStunFinalDamage(damage) / (2 * damage);
		if (damageType.getArmorFinalDamage(damage) > 0)
			damageTypeMod += damageType.getArmorFinalDamage(damage) / (3 * damage);
		if (damageType.getMoraleFinalDamage(damage) > 0)
			damageTypeMod += damageType.getMoraleFinalDamage(damage) / (5 * damage);
		if (damageType.getEnergyFinalDamage(damage) > 0)
			damageTypeMod += damageType.getEnergyFinalDamage(damage) / (10 * damage);
		if (damageType.getManaFinalDamage(damage) > 0)
			damageTypeMod += damageType.getManaFinalDamage(damage) / (10 * damage);
		if (damageType.getTimeFinalDamage(damage) > 0)
			damageTypeMod += damageType.getTimeFinalDamage(damage) / (10 * damage);
		if (target->getTile() && target->getTile()->getDangerous())
			damage /= 2.0f;
	}
	else
	{
		damageTypeMod = 1.0 / 3.0;
	}

	float attacks = static_cast<float>(tuTotal) / tuCost;
	if (energyCost > 0)
		attacks = std::min(attacks, static_cast<float>(energyTotal) / energyCost);
	numberOfShots *= attacks;
	if (numberOfShots < 1)
		return 0;

	accuracy /= 100.0;

	// Apply a modifier for higher/lower hit-chance when closer/further from the target. But not for melee-attacks.
	if (action->type != BA_HIT && !Options::battleRealisticAccuracy)
	{
		if (accuracy > 0)
			accuracy += std::max(1 - accuracy, 0.0f) / distance;
		accuracy = std::min(1.0f, accuracy);
	}

	Position origin = _save->getTileEngine()->getOriginVoxel((*action), nullptr);
	Position targetPosition;
	double targetQuality = 1;
	if (action->type != BA_HIT) //Melee-attacks have their own validity check. This additional check can cause false negatives!
	{
		if (checkLOF)
		{
			if (action->weapon->getArcingShot(action->type) || action->type == BA_THROW)
			{
				if (!validateArcingShot(action, nullptr))
				{
					return 0;
				}
			}
			else
			{
				if (!_save->getTileEngine()->canTargetUnit(&origin, target->getTile(), nullptr, _unit, false))
					return 0;
				if (Options::battleRealisticAccuracy)
				{
					targetQuality = _save->getTileEngine()->checkVoxelExposure(&origin, target->getTile(), _unit);
					if (targetQuality < EPSILON)
						return 0;
				}
			}
		}
		else
		{
			if (action->weapon->getArcingShot(action->type) || action->type == BA_THROW)
			{
				if (!validateArcingShot(action, nullptr))
				{
					return 0;
				}
			}
			else
			{
				if (!clearSight(originPosition, targetPosition) || !quickLineOfFire(originPosition, target, true, true))
				{
					return 0;
				}
			}
		}
	}
	//if (_traceAI)
	//{
	//	Log(LOG_INFO) << action->weapon->getRules()->getName() << " attack-type: " << (int)action->type
	//				  << " No LOS-Penalty: "<< action->weapon->getRules()->getNoLOSAccuracyPenalty(const_cast<Mod*>(_save->getMod()))
	//				  << " damage: " << damage + armorPreDamage << " armor: " << relevantArmor << " damage-mod: " << target->getArmor()->getDamageModifier(action->weapon->getRules()->getDamageType()->ResistType)
	//				  << " accuracy : " << accuracy << " numberOfShots : " << numberOfShots << " tuCost : " << tuCost << " tuTotal: " << tuTotal
	//				  << " from: " << originPosition << " to: "<<action->target
	//				  << " distance: " << distance << " dangerMod: " << dangerMod << " explosionMod: " << explosionMod << " grenade ridding urgency: " << grenadeRiddingUrgency()
	//				  << " targetQuality: " << targetQuality
	//				  << " damageTypeMod: " << damageTypeMod
	//				  << " score: " << damage * accuracy * numberOfShots * dangerMod * explosionMod * targetQuality;
	//}
	// Phase 34.7 (Calypso): suppression-aware auto-fire (ported path). Add the volley's
	// pinning value to auto-shots so volume fire is preferred over holding fire when direct
	// hit chance is poor but the target is exposed (we reached here past the LOF /
	// targetQuality / accuracy gates above). The bonus is small relative to a high-damage
	// direct hit, so it only tips marginal (low-accuracy) shots -- the design intent. The
	// accuracy>0 guard stops an out-of-range shot (accuracy 0) from scoring on suppression
	// alone (those bullets never reach the target, so no near-miss). Additive + gated inside
	// suppressionVolleyValue; flag off => +0, byte-identical.
	float suppressionBonus = (action->type == BA_AUTOSHOT && accuracy > 0.0f) ? suppressionVolleyValue(action->weapon) : 0.0f;
	// Phase 34.9 (Calypso): prefer suppressing a target a teammate has declared intent to FLANK --
	// pin it so the flanker can close (the F.E.A.R.-style flank/suppress loop). Doubles the
	// suppression bonus on that target. Gated; +0 when off, suppression off, or no teammate flanking.
	if (suppressionBonus > 0.0f && _save->getMod()->getAISquadCoordination() && target
		&& _save->getSquadHasFlankIntent(_unit->getFaction(), target->getId()))
	{
		suppressionBonus *= 2.0f;
	}
	_lastScoreTerms = {{damage + armorPreDamage, accuracy * numberOfShots,
		static_cast<float>(dangerMod * explosionMod * targetQuality * damageTypeMod + suppressionBonus)}};
	return (damage + armorPreDamage) * accuracy * numberOfShots * dangerMod * explosionMod * targetQuality * damageTypeMod + suppressionBonus;
}

/**
 * Decides if it worth our while to create an explosion here.
 * @param targetPos The target's position.
 * @param attackingUnit The attacking unit.
 * @param radius How big the explosion will be.
 * @param diff Game difficulty.
 * @param grenade Is the explosion coming from a grenade?
 * @return Value greater than zero if it is worthwhile creating an explosion in the target position. Bigger value better target.
 */
float AIModule::brutalExplosiveEfficacy(Position targetPos, BattleUnit *attackingUnit, int radius, bool grenade, bool validOnly) const
{
	Tile *targetTile = _save->getTile(targetPos);
	if (grenade && targetTile->getDangerous())
		return 0;

	// don't throw grenades at flying enemies.
	if (grenade && targetPos.z > 0 && targetTile->hasNoFloor(_save))
	{
		return 0;
	}

	int distance = Position::distance2d(attackingUnit->getPosition(), targetPos);
	float enemiesAffected = 0;

	// don't go kamikaze unless we're already doomed.
	if (abs(attackingUnit->getPosition().z - targetPos.z) <= Options::battleExplosionHeight && distance <= radius)
	{
		if (_unit->getFaction() == _unit->getOriginalFaction())
		{
			enemiesAffected -= float(radius - distance / 2.0) / float(radius);
		}
		else
			enemiesAffected += float(radius - distance / 2.0) / float(radius);
	}

	// account for the unit we're targetting
	BattleUnit *target = targetTile->getUnit();
	if (target)
	{
		if (isEnemy(target) && (brutalValidTarget(target) || !validOnly))
			enemiesAffected++;
		else if (isAlly(target))
			enemiesAffected--;
	}

	for (std::vector<BattleUnit *>::iterator i = _save->getUnits()->begin(); i != _save->getUnits()->end(); ++i)
	{
		// don't grenade dead guys
		if (!(*i)->isOut() &&
			// don't count ourself twice
			(*i) != attackingUnit &&
			// don't count the target twice
			(*i) != target &&
			// don't count units that probably won't be affected cause they're out of range
			abs((*i)->getPosition().z - targetPos.z) <= Options::battleExplosionHeight &&
			Position::distance2d((*i)->getPosition(), targetPos) <= radius)
		{
			// don't count people who were already grenaded this turn
			if ((*i)->getTile()->getDangerous())
				continue;

			// trace a line from the grenade origin to the unit we're checking against
			Position voxelPosA = Position(targetPos.toVoxel() + TileEngine::voxelTileCenter);
			Position voxelPosB = Position((*i)->getPosition().toVoxel() + TileEngine::voxelTileCenter);
			std::vector<Position> traj;
			int collidesWith = _save->getTileEngine()->calculateLineVoxel(voxelPosA, voxelPosB, false, &traj, target, *i);

			float dist = Position::distance2d(targetPos, (*i)->getPosition());
			float distMod = float(radius - dist / 2.0) / float(radius);
			if (collidesWith == V_UNIT && traj.front().toTile() == (*i)->getPosition())
			{
				if (isEnemy(*i) && (brutalValidTarget((*i)) || !validOnly))
				{
					enemiesAffected += distMod;
				}
				else if (isAlly(*i))
					enemiesAffected--;
			}
		}
	}
	//if (_traceAI)
	//{
	//	Log(LOG_INFO) << "Explosion at " << targetPos << " will hit " << enemiesAffected;
	//}
	return enemiesAffected;
}

/**
 * Returns whether we think we'd have a line of fire from a particular positon towards a particular target
 * @param pos Positon to check
 * @param target target to check whether we'd have a line of fire
 * @return whether it's likely there would be a line of fire
 */
bool AIModule::quickLineOfFire(Position pos, BattleUnit* target, bool beOkayWithFriendOfTarget, bool lastLocationMode, bool fleeMode) {
	Tile *tile = _save->getTile(pos);
	Position originVoxel = pos.toVoxel() + TileEngine::voxelTileCenter;
	originVoxel.z -= tile->getTerrainLevel();
	Position targetPosition = target->getPosition();
	if (lastLocationMode)
	{
		if (target->getTileLastSpotted(_unit->getFaction()) == -1)
			return false;
		targetPosition = _save->getTileCoords(target->getTileLastSpotted(_unit->getFaction()));
	}
	BattleUnit *unitToIgnore = _unit;
	if (tile->getUnit() && isAlly(tile->getUnit()))
		unitToIgnore = tile->getUnit();
	// In fleeMode we don't ignore ourselves because otherwise we think we can take cover behind ourselves
	if (fleeMode && pos != _unit->getPosition())
		unitToIgnore = NULL;
	// Phase 43.1I (Calypso): shared negative terrain-LOF cache for this faction.
	// Obtained once; a null pointer (feature off / no cache) makes every ray
	// fall through to the original uncached trace, byte-for-byte.
	TerrainLofNegativeCache* terrainLofCache = prepareSharedTerrainLofCache();
	for (int x = 0; x < target->getArmor()->getSize(); ++x)
		for (int y = 0; y < target->getArmor()->getSize(); ++y)
		{
			Position targetVoxel = targetPosition;
			targetVoxel += Position(x, y, 0);
			Tile *targetTile = _save->getTile(targetVoxel);
			if (!targetTile)
				return false;
			targetVoxel = targetVoxel.toVoxel();
			targetVoxel += TileEngine::voxelTileCenter;
			targetVoxel.z -= targetTile->getTerrainLevel();
			std::vector<Position> trajectory;
			// Negative cache short-circuit: if the directed ray origin->targetVoxel
			// is already known terrain-blocked, skip it without re-walking the line.
			if (terrainLofCache && terrainLofCache->isKnownBlocked(originVoxel, targetVoxel))
			{
				continue;
			}
			int lofTest = _save->getTileEngine()->calculateLineVoxel(originVoxel, targetVoxel, false, &trajectory, unitToIgnore, NULL, false);
			// Remember only confirmed terrain-blocked rays (V_FLOOR..V_OBJECT).
			// Never cache V_UNIT / V_EMPTY / V_OUTOFBOUNDS -- the cache is negative-only.
			if (terrainLofCache && lofTest >= V_FLOOR && lofTest <= V_OBJECT)
			{
				terrainLofCache->rememberBlocked(originVoxel, targetVoxel, trajectory.at(0));
			}
			if (lofTest == V_UNIT)
			{
				if (targetVoxel.toTile() == trajectory.begin()->toTile())
					return true;
				if (beOkayWithFriendOfTarget && _save->getTile(trajectory.begin()->toTile())->getUnit() && _save->getTile(trajectory.begin()->toTile())->getUnit()->getFaction() == target->getFaction())
					return true;
			}
		}
	return false;
}

/**
 * Returns whether there's clear sight between two positions
 * @param pos Positon to check
 * @param target target to check towards
 * @return whether there is clear sight
 */
bool AIModule::clearSight(Position pos, Position target)
{
	Tile *tile = _save->getTile(pos);
	Tile *targetTile = _save->getTile(target);
	Position originVoxel = pos.toVoxel() + TileEngine::voxelTileCenter;
	originVoxel.z -= tile->getTerrainLevel();
	Position targetVoxel = target.toVoxel() + TileEngine::voxelTileCenter;
	targetVoxel.z -= targetTile->getTerrainLevel();
	// Phase 43.1I (Calypso): shared negative terrain-LOF cache for this faction.
	// Obtained once; a null pointer (feature off / no cache) falls through to the
	// original uncached trace, byte-for-byte.
	TerrainLofNegativeCache* terrainLofCache = prepareSharedTerrainLofCache();
	// Negative cache short-circuit: a known terrain-blocked ray has no clear sight.
	if (terrainLofCache && terrainLofCache->isKnownBlocked(originVoxel, targetVoxel))
	{
		return false;
	}
	std::vector<Position> trajectory;
	int lofTest = _save->getTileEngine()->calculateLineVoxel(originVoxel, targetVoxel, false, &trajectory, _unit, NULL, false);
	// Remember only confirmed terrain-blocked rays (V_FLOOR..V_OBJECT). Never cache
	// V_EMPTY / V_UNIT / V_OUTOFBOUNDS -- the cache is negative-only.
	if (terrainLofCache && lofTest >= V_FLOOR && lofTest <= V_OBJECT)
	{
		terrainLofCache->rememberBlocked(originVoxel, targetVoxel, trajectory.at(0));
	}
	// Original return: true only for V_EMPTY, byte-for-byte.
	if (lofTest == V_EMPTY)
		return true;
	return false;
}

/**
 * Returns the amount of TUs required to turn into a specific direction
 * @param target Positon to consider how many TUs it takes to turn towards
 * @return amount of TUs required to turn in that direction
 */
int AIModule::getTurnCostTowards(Position target, Position from)
{
	int currDir = _unit->getFaceDirection();
	int wantDir = _save->getTileEngine()->getDirectionTo(from, target);
	int turnSteps = std::abs(currDir - wantDir);
	if (turnSteps > 4)
		turnSteps = 8 - turnSteps;
	return turnSteps *= _unit->getArmor()->getTurnCost();
}

int AIModule::getTurnCostTowards(Position target)
{
	Position from = _unit->getPosition();
	return getTurnCostTowards(target, from);
}

/**
 * Fires a waypoint projectile at an enemy we, or one of our teammates sees.
 *
 * Waypoint targeting: pick from any units currently spotted by our allies.
 */
void AIModule::brutalBlaster()
{
	if (_unit->getSpecialWeapon(BT_FIREARM))
	{
		if (_unit->getSpecialWeapon(BT_FIREARM)->getCurrentWaypoints() != 0)
		{
			if (!_blaster)
				_attackAction.weapon = _unit->getSpecialWeapon(BT_FIREARM);
			_blaster = true;
		}
	}
	if (!_blaster)
		return;
	BattleActionCost attackCost(BA_LAUNCH, _unit, _attackAction.weapon);
	if (_tuCostToReachClosestPositionToBreakLos > 0)
	{
		attackCost.Time += _tuCostToReachClosestPositionToBreakLos;
		attackCost.Energy += _energyCostToReachClosestPositionToBreakLos;
	}
	int maxWaypoints = _attackAction.weapon->getCurrentWaypoints();
	if (maxWaypoints == -1)
	{
		maxWaypoints = INT_MAX;
	}
	if (!attackCost.haveTU())
	{
		// cannot make a launcher attack - consider some other behaviour, like running away, or standing motionless.
		return;
	}
	_aggroTarget = 0;
	float highestScore = 0;
	for (std::vector<BattleUnit *>::const_iterator i = _save->getUnits()->begin(); i != _save->getUnits()->end() && _aggroTarget == 0; ++i)
	{
		if ((*i)->isOut() || !brutalValidTarget(*i, true, true))
			continue;
		bool dummy = false;
		std::vector<PathfindingNode *> path = _save->getPathfinding()->findReachablePathFindingNodes(_unit, BattleActionCost(), dummy, true, *i);
		bool havePath = false;
		for (auto node : path)
		{
			if (node->getPosition() == (*i)->getPosition())
			{
				havePath = true;
			}
		}
		if (havePath)
		{
			if (requiredWayPointCount((*i)->getPosition(), path) <= maxWaypoints)
			{
				auto ammo = _attackAction.weapon->getAmmoForAction(BA_LAUNCH);
				float score = brutalExplosiveEfficacy((*i)->getPosition(), _unit, ammo->getRules()->getExplosionRadius({BA_LAUNCH, _unit, _attackAction.weapon, ammo}), false);
				if (score > highestScore)
				{
					highestScore = score;
					_aggroTarget = *i;
				}
			}
		}
		_save->getPathfinding()->abortPath();
	}
	//consider blind-blastering too
	bool blindMode = false;
	Position blindTarget;
	if (_aggroTarget == 0 && _unit->aiCheatMode() >= 0)
	{
		for (std::vector<BattleUnit *>::const_iterator i = _save->getUnits()->begin(); i != _save->getUnits()->end() && _aggroTarget == 0; ++i)
		{
			if ((*i)->getTileLastSpotted(_unit->getFaction(), true) == -1)
				continue;
			if (!(*i)->isOut() && isEnemy((*i), true) && !brutalValidTarget(*i, true, true) && (*i)->getTurnsSinceSeen(_unit->getFaction()) < 2)
			{
				Position targetPos = _save->getTileCoords((*i)->getTileLastSpotted(_unit->getFaction(), true));
				bool dummy = false;
				std::vector<PathfindingNode*> path = _save->getPathfinding()->findReachablePathFindingNodes(_unit, BattleActionCost(), dummy, true, *i);
				bool havePath = false;
				for (auto node : path)
				{
					if (node->getPosition() == targetPos)
					{
						havePath = true;
					}
				}
				if (havePath)
				{
					if (requiredWayPointCount(targetPos, path) <= maxWaypoints)
					{
						auto ammo = _attackAction.weapon->getAmmoForAction(BA_LAUNCH);
						float score = brutalExplosiveEfficacy(targetPos, _unit, ammo->getRules()->getExplosionRadius({BA_LAUNCH, _unit, _attackAction.weapon, ammo}), false);
						// for blind-fire an efficacy of 0 is good enough
						if (score >= highestScore)
						{
							highestScore = score;
							_aggroTarget = *i;
							blindMode = true;
							blindTarget = targetPos;
							if (_traceAI)
							{
								Log(LOG_INFO) << "Blindfire with blaster at " << blindTarget << " would have a score of " << score;
							}
						}
					}
				}
				_save->getPathfinding()->abortPath();
			}
		}
	}

	if (_aggroTarget != 0)
	{
		bool dummy = false;
		std::vector<PathfindingNode *> missilePaths = _save->getPathfinding()->findReachablePathFindingNodes(_unit, BattleActionCost(), dummy, true, _aggroTarget);
		_attackAction.type = BA_LAUNCH;
		_attackAction.updateTU();
		if (!_attackAction.haveTU())
		{
			_attackAction.type = BA_RETHINK;
			return;
		}
		_attackAction.waypoints.clear();
		int PathDirection = 0;
		int CollidesWith = 0;
		PathfindingNode *targetNode = NULL;
		Position target = _aggroTarget->getPosition();
		if (blindMode)
			target = blindTarget;
		if (!true /*CALYPSO: match Brutal default*/ && _save->getTile(target) && _save->getTile(target)->getFloorSpecialTileType() == START_POINT)
		{
			if (_traceAI)
				Log(LOG_INFO) << "Launching blaster-bomb at "<<target<<" aborted out of pity.";
			return;
		}
		float closestDistToTarget = 255;
		for (auto pn : missilePaths)
		{
			if (target == pn->getPosition())
			{
				targetNode = pn;
				break;
			}
		}

		if (targetNode != NULL)
		{
			Tile *tile = _save->getTile(target);
			int lastDirection = -1;
			while (targetNode->getPrevNode() != NULL)
			{
				if (targetNode->getPrevNode() != NULL)
				{
					int direction = _save->getTileEngine()->getDirectionTo(targetNode->getPosition(), targetNode->getPrevNode()->getPosition());
					Position wpPosition = targetNode->getPosition();
					Tile* wpTile = _save->getTile(wpPosition);
					if (_attackAction.weapon->getCurrentWaypoints() == -1  && wpTile->getMapData(O_OBJECT))
					{
						Tile* aboveTile = _save->getAboveTile(wpTile);
						if (aboveTile && !aboveTile->getMapData(O_OBJECT) && aboveTile->hasNoFloor())
							wpPosition.z++;
					}
					bool zChange = false;
					if (wpPosition.z != targetNode->getPrevNode()->getPosition().z)
						zChange = true;
					bool losBreak = false;
					if (!_attackAction.waypoints.empty() && !hasTileSight(targetNode->getPrevNode()->getPosition(), _attackAction.waypoints.front()))
						losBreak = true;
					//If we have unlimited way-points for our blaster, we might as well put a way-point on every single node along the path
					if (_attackAction.weapon->getCurrentWaypoints() == -1)
					{
						_attackAction.waypoints.push_front(wpPosition);
					}
					else if (direction != lastDirection || zChange || losBreak)
					{
						_attackAction.waypoints.push_front(wpPosition);
					}
					lastDirection = direction;
				}
				targetNode = targetNode->getPrevNode();
			}
			if (_attackAction.waypoints.size() < maxWaypoints)
				_attackAction.waypoints.push_back(target);
			//if (_traceAI)
			//{
			//	int iStep = 0;
			//	for (Position pos : _attackAction.waypoints)
			//	{
			//		iStep++;
			//		Tile* wpTile = _save->getTile(pos);
			//		wpTile->setMarkerColor(_unit->getId());
			//		wpTile->setPreview(10);
			//		wpTile->setTUMarker(iStep);
			//	}
			//}
			_attackAction.target = _attackAction.waypoints.front();
			if (_attackAction.waypoints.size() > maxWaypoints)
				_attackAction.type = BA_RETHINK;
			else if (blindMode)
				_aggroTarget->setTileLastSpotted(-1, _unit->getFaction(), true);
		}
		else
		{
			_attackAction.type = BA_RETHINK;
		}
		return;
	}
}

/**
 * Evaluates whether to throw a grenade at an enemy or a tile nearby.
 */
void AIModule::brutalGrenadeAction()
{
	// do we have a grenade on our belt?
	BattleItem* grenade = _unit->getGrenadeFromBelt(_save);
	BattleAction action;
	action.weapon = grenade;
	action.type = BA_THROW;
	action.actor = _unit;

	action.updateTU();
	// Xilmi: Take into account that we might already have primed the grenade before
	if (!action.weapon->isFuseEnabled())
	{
		action.Time += 4; // 4TUs for picking up the grenade
		action += _unit->getActionTUs(BA_PRIME, grenade);
	}
	auto radius = grenade->getRules()->getExplosionRadius(BattleActionAttack::GetBeforeShoot(action));
	Position bestReachablePosition;
	float bestScore = 0;
	BattleUnit *bestTarget = nullptr;
	int actionTimeBefore = action.Time;
	for (BattleUnit *target : *(_save->getUnits()))
	{
		if (target->isOut())
			continue;
		if (!brutalValidTarget(target, true))
			continue;
		// We don't want to nade someone who's already been naded
		if (target->getTile() && target->getTile()->getDangerous())
			continue;
		for (int x = 0; x < _save->getMapSizeX(); ++x)
		{
			for (int y = 0; y < _save->getMapSizeY(); ++y)
			{
				Position currentPosition(x, y, target->getPosition().z);
				int dist = Position::distance2d(currentPosition, target->getPosition());
				if (dist <= radius)
				{
					if (!candidateAllowed(BA_THROW, target->getId(), currentPosition)) continue;
					// take into account we might have to turn towards our target
					action.Time = actionTimeBefore;
					action.Time += getTurnCostTowards(currentPosition);
					// do we have enough TUs to prime and throw the grenade?
					if (action.haveTU())
					{
						action.target = currentPosition;
						if (!validateArcingShot(&action))
							continue;
						float currentEfficacy = brutalExplosiveEfficacy(currentPosition, _unit, radius, true, true);
						if (currentEfficacy > bestScore)
						{
							bestReachablePosition = currentPosition;
							bestScore = currentEfficacy;
							bestTarget = target;
						}
					}
				}
			}
		}
	}
	if (_unit->aiCheatMode() > 0 && bestScore == 0 && grenadeRiddingUrgency() > 1)
	{
		for (BattleUnit* target : *(_save->getUnits()))
		{
			if (target->isOut())
				continue;
			if (!isEnemy(target))
				continue;
			if (target->getTurnsSinceSeen(_unit->getFaction()) > 1)
				continue;
			Position pos = _save->getTileCoords(target->getTileLastSpotted(_unit->getFaction(), true));
			Tile* tile = _save->getTile(pos);
			if (!tile)
				continue;
			if (tile->getDangerous())
				continue;
			action.Time = actionTimeBefore;
			action.Time += getTurnCostTowards(pos);
			if (!action.haveTU())
				continue;
			action.target = pos;
			if (!candidateAllowed(BA_THROW, target->getId(), pos))
				continue;
			if (!validateArcingShot(&action))
				continue;
			if (brutalExplosiveEfficacy(pos, _unit, radius, true, true) < 0)
				continue;
			float score = Position::distance(pos, _unit->getPosition());
			if (score > bestScore)
			{
				bestScore = score;
				bestReachablePosition = pos;
				bestTarget = target;
			}
		}
	}
	if (bestScore > 0)
	{
		_aggroTarget = bestTarget;
		if (_aggroTarget)
			_aggroTarget->setTileLastSpotted(-1, _unit->getFaction(), true);
		_attackAction.weapon = grenade;
		_attackAction.target = bestReachablePosition;
		_attackAction.type = BA_THROW;
		_attackAction.aiTargetId = bestTarget ? bestTarget->getId() : -1;
		_attackAction.aiHasFilteredFallback = true;
		_rifle = false;
		_melee = false;
		if (_traceAI)
			Log(LOG_INFO) << "brutalGrenadeAction: Throw grenade at " << bestReachablePosition << " score: " << bestScore;
	}
}

/**
 * Changes whether the AI wants to end their turn
 * @param wantToEndTurn
 */
void AIModule::setWantToEndTurn(bool wantToEndTurn)
{
	_wantToEndTurn = wantToEndTurn;
}

/**
 * Returns whether the AI wants to end their turn
 */
bool AIModule::getWantToEndTurn()
{
	// CALYPSO: Xilmi folds the just-woke-from-stun "skip turn" behaviour into this getter; our
	// handleAI keeps its own explicit stun check, so returning only the flag keeps non-brutal
	// (brutalAI off) behaviour byte-identical -- the flag is only ever set by brutalThink.
	return _wantToEndTurn;
}

/**
 * Fires at locations that we've spotted enemies before
 */
void AIModule::blindFire()
{
	// Create a list of spotted targets and the type of attack we'd like to use on each
	std::vector<std::pair<BattleUnit *, BattleAction> > spottedTargets;

	BattleActionCost costThrow;
	// We know we have a grenade, now we need to know if we have the TUs to throw it
	costThrow.type = BA_THROW;
	costThrow.actor = _attackAction.actor;
	costThrow.weapon = _unit->getGrenadeFromBelt(_save);
	costThrow.updateTU();
	if (costThrow.weapon && !costThrow.weapon->isFuseEnabled())
	{
		costThrow.Time += costThrow.weapon->getMoveToCost(_save->getMod()->getInventoryLeftHand()); // Vanilla TUs for AI picking up grenade from belt
		costThrow += _attackAction.actor->getActionTUs(BA_PRIME, costThrow.weapon);
	}

	std::vector<BattleItem *> weapons;
	if (_attackAction.actor->getRightHandWeapon())
		weapons.push_back(_attackAction.actor->getRightHandWeapon());
	if (_attackAction.actor->getLeftHandWeapon())
		weapons.push_back(_attackAction.actor->getLeftHandWeapon());
	if (_attackAction.actor->getUtilityWeapon(BT_MELEE))
		weapons.push_back(_attackAction.actor->getUtilityWeapon(BT_MELEE));
	if (_attackAction.actor->getSpecialWeapon(BT_FIREARM))
		weapons.push_back(_attackAction.actor->getSpecialWeapon(BT_FIREARM));
	if (_attackAction.actor->getGrenadeFromBelt(_save))
		weapons.push_back(_attackAction.actor->getGrenadeFromBelt(_save));

	for (std::vector<BattleUnit *>::const_iterator i = _save->getUnits()->begin(); i != _save->getUnits()->end(); ++i)
	{
		if ((*i)->getTileLastSpotted(_unit->getFaction(), true) == -1)
			continue;
		if (!(*i)->isOut() && isEnemy((* i), true) && !brutalValidTarget(*i, true))
		{
			// Determine which firing mode to use based on how many hits we expect per turn and the unit's intelligence/aggression
			_aggroTarget = (*i);
			_attackAction.type = BA_RETHINK;
			_attackAction.target = _save->getTileCoords((*i)->getTileLastSpotted(_unit->getFaction(), true));
			for (BattleItem *weapon : weapons)
			{
				// Get the TU costs for each available attack type
				_attackAction.weapon = weapon;
				BattleActionCost costAuto(BA_AUTOSHOT, _attackAction.actor, weapon);
				BattleActionCost costSnap(BA_SNAPSHOT, _attackAction.actor, weapon);
				BattleActionCost costAimed(BA_AIMEDSHOT, _attackAction.actor, weapon);
				BattleActionCost costHit(BA_HIT, _attackAction.actor, weapon);
				brutalExtendedFireModeChoice(costAuto, costSnap, costAimed, costThrow, costHit, false);

				BattleAction chosenAction = _attackAction;
				if (_attackAction.type != BA_RETHINK)
				{
					std::pair<BattleUnit *, BattleAction> spottedTarget;
					spottedTarget = std::make_pair((*i), chosenAction);
					spottedTargets.push_back(spottedTarget);
				}
			}
		}
	}

	int numberOfTargets = static_cast<int>(spottedTargets.size());

	if (numberOfTargets) // Now that we have a list of valid targets, pick one and return.
	{
		float clostestDist = 255;
		for (auto& targetAction : spottedTargets)
		{
			float dist = Position::distance(targetAction.first->getPosition(), _unit->getPosition());
			Tile *targetTile = _save->getTile(targetAction.first->getPosition());
			// deprioritize naded targets but don't ignore them completely
			if (targetTile->getDangerous())
				dist *= 5;
			if (dist < clostestDist)
			{
				clostestDist = dist;
				_aggroTarget = targetAction.first;
				_attackAction.type = targetAction.second.type;
				_attackAction.weapon = targetAction.second.weapon;
				_attackAction.target = _save->getTileCoords(_aggroTarget->getTileLastSpotted(_unit->getFaction(), true));
				_attackAction.aiTargetId = targetAction.second.aiTargetId;
				_attackAction.aiHasFilteredFallback = targetAction.second.aiHasFilteredFallback;
			}
		}
		if (_aggroTarget)
		{
			if (_traceAI)
				Log(LOG_INFO) << "Blindfire at " << _attackAction.target;
			// we blindFire only once per target, so doing so clears up the remembered position:
			_aggroTarget->setTileLastSpotted(-1, _unit->getFaction(), true);
		}
	}
	else // We didn't find a suitable target
	{
		// Make sure we reset anything we might have changed while testing for targets
		_aggroTarget = 0;
		_attackAction.type = BA_RETHINK;
		_attackAction.weapon = _unit->getMainHandWeapon(false);
	}
}

bool AIModule::validateArcingShot(BattleAction *action, Tile* originTile)
{
	action->actor = _unit;
	if (originTile == NULL)
		originTile = _unit->getTile();
	Position origin = _save->getTileEngine()->getOriginVoxel((*action), originTile);
	Tile *targetTile = _save->getTile(action->target);
	if (!targetTile)
		return false;
	Position targetVoxel;
	std::vector<Position> targets;
	double curvature;
	targetVoxel = action->target.toVoxel() + Position(8, 8, (1 + -targetTile->getTerrainLevel()));
	targets.clear();
	bool forced = false;

	if (action->type == BA_THROW)
	{
		targets.push_back(targetVoxel);
	}
	else
	{
		BattleUnit *tu = targetTile->getOverlappingUnit(_save);
		if (tu && tu->getVisible())
		{                                          // unit
			targetVoxel.z += tu->getFloatHeight(); // ground level is the base
			targets.push_back(targetVoxel + Position(0, 0, tu->getHeight() / 2 + 1));
			targets.push_back(targetVoxel + Position(0, 0, 2));
			targets.push_back(targetVoxel + Position(0, 0, tu->getHeight() - 1));
		}
		else if (targetTile->getMapData(O_OBJECT) != 0)
		{
			targetVoxel = action->target.toVoxel() + Position(8, 8, 0);
			targets.push_back(targetVoxel + Position(0, 0, 13));
			targets.push_back(targetVoxel + Position(0, 0, 8));
			targets.push_back(targetVoxel + Position(0, 0, 23));
			targets.push_back(targetVoxel + Position(0, 0, 2));
		}
		else if (targetTile->getMapData(O_NORTHWALL) != 0)
		{
			targetVoxel = action->target.toVoxel() + Position(8, 0, 0);
			targets.push_back(targetVoxel + Position(0, 0, 13));
			targets.push_back(targetVoxel + Position(0, 0, 8));
			targets.push_back(targetVoxel + Position(0, 0, 20));
			targets.push_back(targetVoxel + Position(0, 0, 3));
		}
		else if (targetTile->getMapData(O_WESTWALL) != 0)
		{
			targetVoxel = action->target.toVoxel() + Position(0, 8, 0);
			targets.push_back(targetVoxel + Position(0, 0, 13));
			targets.push_back(targetVoxel + Position(0, 0, 8));
			targets.push_back(targetVoxel + Position(0, 0, 20));
			targets.push_back(targetVoxel + Position(0, 0, 2));
		}
		else if (targetTile->getMapData(O_FLOOR) != 0)
		{
			targets.push_back(targetVoxel);
		}
	}
	int test = V_OUTOFBOUNDS;
	for (std::vector<Position>::iterator i = targets.begin(); i != targets.end(); ++i)
	{
		targetVoxel = *i;
		if (_save->getTileEngine()->validateThrow((*action), origin, targetVoxel, _save->getDepth(), &curvature, &test, forced))
		{
			return true;
		}
	}
	return false;
}

bool AIModule::brutalValidTarget(BattleUnit *unit, bool moveMode, bool psiMode) const
{
	if (unit == NULL)
		return false;
	if (unit->isOut() || unit->isIgnoredByAI() ||
		isAlly(unit))
	{
		return false;
	}
	int targetMode = _unit->aiCheatMode();
	if (psiMode)
		targetMode = std::max(targetMode, 0);
	bool iAmMindControlled = false;
	if (_unit->getOriginalFaction() != _unit->getFaction())
		iAmMindControlled = true;
	if (targetMode < 0 && !moveMode)
	{
		if (_unit->hasVisibleUnit(unit))
			return isEnemy(unit, iAmMindControlled);
		else
			return false;
	}
	else if (targetMode < 2 || moveMode)
	{
		if (visibleToAnyFriend(unit) || targetMode >= 2)
			return isEnemy(unit, iAmMindControlled);
		else
			return false;
	}
	return isEnemy(unit, iAmMindControlled);
}

Position AIModule::closestPositionEnemyCouldReach(BattleUnit *enemy)
{
	if (!_unit->isCheatOnMovement() && enemy->getTileLastSpotted(_unit->getFaction()) == -1)
		return _unit->getPosition();
	PathfindingNode *targetNode = NULL;
	int tu = 0;
	for (auto pn : _allPathFindingNodes)
	{
		Position enemyPositon = _save->getTileCoords(enemy->getTileLastSpotted(_unit->getFaction()));
		if (_unit->isCheatOnMovement())
			enemyPositon = enemy->getPosition();
		if (enemyPositon == pn->getPosition())
		{
			targetNode = pn;
			tu = pn->getTUCost(false).time;
			break;
		}
	}
	tu -= getMaxTU(enemy);
	if (targetNode != NULL)
	{
		while (targetNode->getPrevNode() != NULL)
		{
			if (targetNode->getTUCost(false).time < tu)
				return targetNode->getPosition();
			targetNode = targetNode->getPrevNode();
		}
	}
	return _unit->getPosition();
}

int AIModule::maxExtenderRangeWith(BattleUnit *unit, int tus)
{
	BattleItem *weapon = unit->getMainHandWeapon();
	if (weapon == NULL)
		return 0;
	if (!Options::battleUFOExtenderAccuracy)
	{
		if (weapon->getRules()->getBattleType() == BT_MELEE)
			return 1;
		return weapon->getRules()->getMaxRange();
	}
	int highestRangeAvailableWithTUs = 0;
	if (weapon->getRules()->getCostAimed().Time > 0 && unit->getActionTUs(BA_AIMEDSHOT, weapon).Time < tus)
		highestRangeAvailableWithTUs = weapon->getRules()->getAimRange();
	if (weapon->getRules()->getCostSnap().Time > 0 && unit->getActionTUs(BA_SNAPSHOT, weapon).Time < tus)
		highestRangeAvailableWithTUs = std::max(highestRangeAvailableWithTUs, weapon->getRules()->getSnapRange());
	if (weapon->getRules()->getCostAuto().Time > 0 && unit->getActionTUs(BA_AUTOSHOT, weapon).Time < tus)
		highestRangeAvailableWithTUs = std::max(highestRangeAvailableWithTUs, weapon->getRules()->getAutoRange());
	if (weapon->getRules()->getBattleType() == BT_GRENADE && weapon->getRules()->getCostThrow().Time > 0 && unit->getActionTUs(BA_THROW, weapon).Time < tus)
		highestRangeAvailableWithTUs = std::max(highestRangeAvailableWithTUs, weapon->getRules()->getThrowRange());
	if (weapon->getRules()->getCostMelee().Time > 0 && unit->getActionTUs(BA_HIT, weapon).Time < tus)
		highestRangeAvailableWithTUs = std::max(highestRangeAvailableWithTUs, 1);
	highestRangeAvailableWithTUs = std::min(highestRangeAvailableWithTUs, weapon->getRules()->getMaxRange());
	return highestRangeAvailableWithTUs;
}

int AIModule::getNewTileIDToLookForEnemy(Position previousPosition, BattleUnit* unit)
{
	Tile *TileToCheckNext = NULL;
	int LowestTuCost = INT_MAX;
	bool dummy;
	std::vector<PathfindingNode*> reachable = _save->getPathfinding()->findReachablePathFindingNodes(unit, BattleActionCost(), dummy, true, NULL, &previousPosition);
	for (auto pn : reachable)
	{
		Tile *tile = _save->getTile(pn->getPosition());
		int lastExplored = tile->getLastExplored(_unit->getFaction());
		if (lastExplored == _save->getTurn() && tile->getUnit() != unit)
			continue;
		if (pn->getTUCost(false).time > unit->getTurnsSinceSeen(_unit->getFaction()) * getMaxTU(unit))
			continue;
		int TUCost = pn->getTUCost(false).time + lastExplored * getMaxTU(unit);
		if (TUCost < LowestTuCost)
		{
			LowestTuCost = TUCost;
			TileToCheckNext = tile;
		}
	}
	if (TileToCheckNext)
	{
		return _save->getTileIndex(TileToCheckNext->getPosition());
	}
	return -1;
}

int AIModule::getMaxTU(BattleUnit *unit)
{
	int maxTU = 0;
	if (!unit->isOut())
	{
		// Add to previous turn TU, if regen is less than normal unit need couple of turns to regen full bar
		maxTU = unit->getBaseStats()->tu;
		float encumbrance = (float)unit->getBaseStats()->strength / (float)unit->getCarriedWeight();
		if (encumbrance < 1)
		{
			maxTU = int(encumbrance * maxTU);
		}
		// Each fatal wound to the left or right leg reduces the soldier's TUs by 10%.
		maxTU -= (maxTU * ((unit->getFatalWound(BODYPART_LEFTLEG) + unit->getFatalWound(BODYPART_LEFTLEG)) * 10)) / 100;
	}
	return maxTU;
}

int AIModule::getClosestSpawnTileId()
{
	int id = -1;
	for (auto pn : _allPathFindingNodes)
	{
		Position tilePositon = pn->getPosition();
		bool alreadyTaken = false;
		for (BattleUnit* target : *(_save->getUnits()))
		{
			if (target->isOut())
				continue;
			if (_save->getTileCoords(target->getTileLastSpotted(_unit->getFaction())) == tilePositon)
			{
				alreadyTaken = true;
				break;
			}
		}
		if (alreadyTaken)
			continue;
		Tile *tile = _save->getTile(tilePositon);
		if (tile->getFloorSpecialTileType() == START_POINT)
		{
			if (_traceAI)
			{
				Log(LOG_INFO) << "Assuming a target to be at " << tilePositon;
				//tile->setMarkerColor(4);
				//tile->setPreview(10);
				//tile->setTUMarker(4);
			}
			return _save->getTileIndex(tilePositon);
		}
	}
	return id;
}

bool AIModule::isEnemy(BattleUnit* unit, bool ignoreSameOriginalFaction) const
{
	if (!unit)
		return false;
	if (_unit == unit)
		return false;
	if (unit->isIgnoredByAI())
		return false;
	UnitFaction faction = unit->getFaction();
	bool unitIsMindControlled = false;
	if (unit->getFaction() != unit->getOriginalFaction())
		unitIsMindControlled = true;
	if (ignoreSameOriginalFaction || unitIsMindControlled)
		faction = unit->getOriginalFaction();
	if (_unit->getFaction() == FACTION_HOSTILE)
	{
		if (faction == FACTION_PLAYER || faction == FACTION_NEUTRAL)
			return true;
	}
	else if (_unit->getFaction() == FACTION_NEUTRAL)
	{
		if (faction == FACTION_HOSTILE)
			return true;
	}
	else if (_unit->getFaction() == FACTION_PLAYER)
	{
		if (faction == FACTION_HOSTILE)
			return true;
	}
	return false;
}

bool AIModule::isAlly(BattleUnit *unit) const
{
	if (!unit)
		return false;
	UnitFaction faction = unit->getFaction();
	if (_unit->getFaction() == FACTION_HOSTILE)
	{
		if (faction == FACTION_PLAYER || faction == FACTION_NEUTRAL)
			return false;
	}
	else if (_unit->getFaction() == FACTION_NEUTRAL)
	{
		if (faction == FACTION_HOSTILE)
			return false;
	}
	else if (_unit->getFaction() == FACTION_PLAYER)
	{
		if (faction == FACTION_HOSTILE)
			return false;
	}
	return true;
}

bool AIModule::projectileMayHarmFriends(Position startPos, Position targetPos)
{
	float distance = Position::distance(startPos, targetPos);
	Position posToCheck = targetPos;
	std::vector<Position> trajectory;
	trajectory.clear();
	int tst = _save->getTileEngine()->calculateLineTile(startPos, posToCheck, trajectory);
	// Reveal all tiles along line of vision. Note: needed due to width of bresenham stroke.
	for (std::vector<Position>::iterator i = trajectory.begin(); i != trajectory.end(); ++i)
	{
		Position posVisited = (*i);
		if (posVisited == startPos)
			continue;
		Tile *tile = _save->getTile(posVisited);
		if (!tile)
			continue;
		if (tile && tile->getUnit() && isAlly(tile->getUnit()) && !tile->getUnit()->isOut() && tile->getUnit() != _unit)
		{
			return true;
		}
	}
	return false;
}

bool AIModule::inRangeOfAnyFriend(Position pos)
{
	for (BattleUnit* ally : *(_save->getUnits()))
	{
		if (ally->isOut())
			continue;
		if (ally->getFaction() != _unit->getFaction())
			continue;
		if(maxExtenderRangeWith(ally, getMaxTU(ally)) > Position::distance(ally->getPosition(), pos))
			return true;
	}
	return false;
}

bool AIModule::shouldAvoidMeleeRange(BattleUnit *enemy)
{
	if (maxExtenderRangeWith(_unit, getMaxTU(_unit)) == 1)
		return false;
	if (_save->getMod()->getEnableCloseQuartersCombat() && !_unit->getArmor()->getIgnoresMeleeThreat() && enemy->getArmor()->getCreatesMeleeThreat())
		return true;
	return false;
}

bool AIModule::isArmed(BattleUnit *unit) const
{
	if (unit->getMainHandWeapon())
		return true;
	if (unit->getGrenadeFromBelt(_save))
		return true;
	if (unit->getUtilityWeapon(BT_PSIAMP))
		return true;
	if (unit->getSpecialWeapon(BT_MELEE))
		return true;
	if (unit->getSpecialWeapon(BT_FIREARM))
		return true;
	return false;
}

void AIModule::tryToPickUpGrenade(Tile *tile, BattleAction *action)
{
	if (!_unit->hasInventory())
		return;
	for (BattleItem *item : *(tile->getInventory()))
	{
		if (item->isFuseEnabled() && item->getRules()->isInventoryItem())
		{
			if (_save->getBattleGame()->takeItemFromGround(item, action) == 0)
				if (_traceAI)
					Log(LOG_INFO) << "Picked up " << item->getRules()->getName() << " from " << tile->getPosition();
		}
	}
}

float AIModule::getItemPickUpScore(BattleItem* item)
{
	if (!_unit->isBrutal())
		return item->getRules()->getAttraction();
	if (!_save->canUseWeapon(item, _unit, false, BA_SNAPSHOT))
		return 0;
	float score = 0;
	bool valid = false;
	if (item->haveAnyAmmo() || item == _unit->getMainHandWeapon(true, false))
	{
		if (item->getRules()->getBattleType() == BT_FIREARM || item->getRules()->getBattleType() == BT_GRENADE || item->getRules()->getBattleType() == BT_MELEE)
			valid = true;
	} 
	if (item->getRules()->getBattleType() == BT_AMMO)
	{
		for (const auto *bi : *_unit->getInventory())
		{
			if (bi->getRules()->getBattleType() == BT_FIREARM)
			{
				if (bi->getRules()->getSlotForAmmo(item->getRules()) != -1)
					valid = true;
			}
		}
	}
	if (!valid)
		return 0;
	score = item->getRules()->getSellCost();
	int mainHandWeight = 0;
	if (_unit->getMainHandWeapon())
		mainHandWeight = _unit->getMainHandWeapon()->getRules()->getWeight();
	float encumbrance = (float)_unit->getBaseStats()->strength / (float)(_unit->getCarriedWeight() - mainHandWeight + item->getRules()->getWeight());
	if (encumbrance < 1)
		score *= encumbrance;
	if (_traceAI)
		Log(LOG_INFO) << "Pickup-score for " << item->getRules()->getName() << ": " << score;
	return score;
}

bool AIModule::IsEnemyExposedEnough()
{
	bool dummy = false;
	Position startPosition = _unit->getPosition();

	if (getClosestSpawnTileId() >= 0)
		startPosition = _save->getTileCoords(getClosestSpawnTileId());
	else
		return true;
	if (_traceAI)
	{
		Log(LOG_INFO) << "startPos: " << startPosition;
	}
	std::vector<PathfindingNode *> enemySimulationNodes = _save->getPathfinding()->findReachablePathFindingNodes(_unit, BattleActionCost(), dummy, true, NULL, &startPosition);
	for (BattleUnit *enemy : *(_save->getUnits()))
	{
		if (!isEnemy(enemy))
			continue;
		if (enemy->isOut())
			continue;
		if (visibleToAnyFriend(enemy))
			return true;
		Position currentAssumedPosition = _save->getTileCoords(enemy->getTileLastSpotted(_unit->getFaction()));
		int turnsSinceSeen = enemy->getTurnsSinceSeen(_unit->getFaction());
		if (_unit->isCheatOnMovement())
		{
			currentAssumedPosition = enemy->getPosition();
			turnsSinceSeen = 0;
		}
		else if (enemy->getTileLastSpotted(_unit->getFaction()) == -1)
			return false;
		turnsSinceSeen = std::max(turnsSinceSeen, 1);
		int requiredTUFromStart = turnsSinceSeen * getMaxTU(enemy);
		int neededTUToStart = tuCostToReachPosition(currentAssumedPosition, enemySimulationNodes, enemy);
		bool inSmoke = false;
		if (_save->getTile(currentAssumedPosition) && _save->getTile(currentAssumedPosition)->getSmoke() > 0)
			inSmoke = true;
		if (_traceAI)
		{
			Log(LOG_INFO) << enemy->getId() << ", seen " << enemy->getTurnsSinceSeen(_unit->getFaction()) << " turns ago, needs to be at least " << requiredTUFromStart << " TUs from the starting-location. We assume they should need " << neededTUToStart << " in smoke: "<<inSmoke;
		}
		//If I'm in smoke myself, I have the same advantage as the enemy and thus don't care whether they are in smoke
		if (_unit->getTile()->getSmoke() > 0)
			inSmoke = false;
		if (requiredTUFromStart < neededTUToStart && !inSmoke)
			return true;
	}
	return false;
}

float AIModule::getCoverValue(Tile* tile, BattleUnit* bu, int coverQuality)
{
	if (tile == NULL)
		return 0;
	if (coverQuality == 0)
	{
		if (_save->getAboveTile(tile) && _save->getAboveTile(tile)->hasNoFloor())
			return 0;
	}
	if (coverQuality < 3 && _save->getTileEngine()->isNextToDoor(tile))
		return 0;
	float cover = 0;
	Tile* tileFrom = tile;
	int peakOver = tile->getTerrainLevel() * -1 + bu->getHeight() - 24;
	if (peakOver > 0)
		tileFrom = _save->getAboveTile(tile);
	if (tileFrom == NULL)
		tileFrom = tile;
	for (int direction = 0; direction <= 7; ++direction)
	{
		Position posInDirection = tileFrom->getPosition();
		switch (direction)
		{
		case 0:
			posInDirection += Position(0, -1, 0);
			break;
		case 1:
			posInDirection += Position(1, -1, 0);
			break;
		case 2:
			posInDirection += Position(1, 0, 0);
			break;
		case 3:
			posInDirection += Position(1, 1, 0);
			break;
		case 4:
			posInDirection += Position(0, 1, 0);
			break;
		case 5:
			posInDirection += Position(-1, 1, 0);
			break;
		case 6:
			posInDirection += Position(-1, 0, 0);
			break;
		case 7:
			posInDirection += Position(-1, -1, 0);
			break;
		}
		Tile *tileInDirection = _save->getTile(posInDirection);
		if (tileInDirection)
		{
			float totalEnemies = 0;
			float enemiesInThisDirection = 0;
			float trueDirection = 0;
			for (BattleUnit *enemy : *(_save->getUnits()))
			{
				if (!enemy->isOut() && isEnemy(enemy))
				{
					if (!_unit->isCheatOnMovement() && enemy->getTileLastSpotted(_unit->getFaction()) == -1)
						continue;
					Position pos = _save->getTileCoords(enemy->getTileLastSpotted(_unit->getFaction()));
					if (_unit->isCheatOnMovement())
						pos = enemy->getPosition();
					int enemyDir = _save->getTileEngine()->getDirectionTo(tile->getPosition(), pos);
					float dist = Position::distance(tile->getPosition(), pos);
					if (direction == enemyDir)
					{
						enemiesInThisDirection += 1.0 / dist;
						trueDirection += 1.0 / dist;
					}
					if (direction == enemyDir - 1 || (direction == 0 && enemyDir == 7))
						enemiesInThisDirection += 0.5 / dist;
					if (direction == enemyDir + 1 || (direction == 7 && enemyDir == 0))
						enemiesInThisDirection += 0.5 / dist;
					totalEnemies += 2.0 / dist;
				}
			}
			float dirCoverMod = enemiesInThisDirection / totalEnemies;
			std::vector<Position> traj;
			float coverFromDir = 0;
			coverFromDir += _save->getTileEngine()->horizontalBlockage(tileInDirection, tileFrom, DT_NONE) / 255.0;
			if (coverFromDir >= 1 || coverQuality > 3)
				coverFromDir += _save->getTileEngine()->horizontalBlockage(tileInDirection, tileFrom, DT_HE) / 255.0;
			if (coverFromDir > 0)
				cover += coverFromDir * dirCoverMod;
			else if (coverQuality == 1 && enemiesInThisDirection > 0)
				return 0;
			else if (coverQuality == 2 && trueDirection > 0)
				return 0;
		}
	}
	return cover;
}

float AIModule::highestCoverInRange(const std::vector<PathfindingNode *> nodeVector)
{
	float highestCover = 0;
	for (auto pn : nodeVector)
	{
		if (pn->getTUCost(false).time > getMaxTU(_unit) || pn->getTUCost(false).energy > _unit->getBaseStats()->stamina)
			continue;
		Tile *tile = _save->getTile(pn->getPosition());
		float cover = getCoverValue(tile, _unit, 3);
		if (cover > highestCover)
		{
			highestCover = cover;
		}
	}
	return highestCover;
}

bool AIModule::isAnyMovementPossible()
{
	bool dummy = true;
	BattleActionMove bam = BAM_NORMAL;
	if (Options::strafe && wantToRun())
		bam = BAM_RUN;
	if (_save->getPathfinding()->findReachablePathFindingNodes(_unit, BattleActionCost(), dummy, false, NULL, NULL, true, false, bam).size() > 1)
		return true;
	return false;
}

int AIModule::getEnergyRecovery(BattleUnit* unit)
{
	int recovery = 0;
	if (unit->getGeoscapeSoldier())
	{
		for (const auto* bonusRule : *(unit->getGeoscapeSoldier()->getBonuses(nullptr)))
		{
			recovery += bonusRule->getEnergyRecovery(unit);
		}
	}
	recovery = _unit->getArmor()->getEnergyRecovery(unit, recovery);
	return recovery;
}

/**
 * Phase 43.1M (Calypso): exact extraction of brutalThink's legacy
 * discoverThreat loop. Both the shared memo and the feature-off fallback call
 * this function, preserving the original distance-before-footprint-offset and
 * currThreat-before-LOF ordering.
 */
float AIModule::calculateDiscoverThreat(const Position& candidate, const std::map<Position, int, PositionComparator>& enemyReachable)
{
	return ThreatField::calculateThreatAt(candidate, enemyReachable, _unit->getArmor()->getSize(),
		[this](const Position& from, const Position& to) { return hasTileSight(from, to); });
}

/**
 * Phase 43.1M (Calypso): prepare the exact per-faction discoverThreat memo.
 * Actor footprint/height affect the legacy LOF probe, so the first user binds
 * a profile for this turn and incompatible actors retain the legacy path.
 * Queued new/updated knowledge never lowers danger: already evaluated tiles
 * are recomputed from the current fair-known aggregate and stampMax keeps the
 * conservative maximum.
 */
ThreatField* AIModule::prepareSharedThreatField(const std::map<Position, int, PositionComparator>& enemyReachable)
{
	if (!_save->getMod()->getAISharedFields() || !_unit->isAIControlled())
		return nullptr;
	FactionTurnCache* cache = _save->getFactionTurnCache(_unit->getFaction());
	if (cache == nullptr || !cache->isValid())
		return nullptr;

	const int footprint = _unit->getArmor()->getSize();
	const int height = _unit->getHeight();
	const bool movementCheat = _unit->isCheatOnMovement();
	ThreatField& field = cache->getThreatField();
	if (cache->isThreatDirty())
	{
		field.clear();
		cache->setThreatProfile(footprint, height, movementCheat);
		cache->markThreatClean();
		// The current aggregate already reflects all authoritative knowledge, and
		// there are no evaluated tiles to update after the clear.
		cache->clearPendingThreatSightings();
		return &field;
	}
	if (!cache->matchesThreatProfile(footprint, height, movementCheat))
		return nullptr;

	if (!cache->getPendingThreatSightings().empty())
	{
		const std::vector<Position> evaluated(
			field.getEvaluatedPositions().begin(), field.getEvaluatedPositions().end());
		for (const Position& pos : evaluated)
			field.stampMax(pos, calculateDiscoverThreat(pos, enemyReachable));
		cache->clearPendingThreatSightings();
	}
	return &field;
}

/**
 * Phase 43.1E (Calypso): shared friendReachable integration.
 *
 * Returns the live friendReachable accumulator for this unit's faction when the
 * mod enables ai.sharedFields and a valid faction turn-cache exists; otherwise
 * returns nullptr so the brutalThink legacy per-ally local map is used.
 *
 * Rebuild policy (lazy, mirroring FactionTurnCache's dirty flag):
 *   - Dirty cache: clear the field, then scan every living unit of exactly this
 *     faction, compute its reachability (ignoreFriends on, forceRecalc + useMaxTUs),
 *     and replaceContribution(id, result); then markFriendReachableClean.
 *   - Clean cache: same scan, but only units missing a contribution are recomputed
 *     (forceRecalc=false, useMaxTUs=true); already-stamped units are left intact.
 * A local ran-out flag is used so _ranOutOfTUs is never overwritten.
 */
FriendReachableField* AIModule::prepareSharedFriendReachable()
{
	if (!_save->getMod()->getAISharedFields())
		return nullptr;
	FactionTurnCache* cache = _save->getFactionTurnCache(_unit->getFaction());
	if (cache == nullptr || !cache->isValid())
		return nullptr;
	FriendReachableField& field = cache->getFriendReachable();
	const bool dirty = cache->isFriendReachableDirty();
	if (dirty)
	{
		field.clear();
	}
	for (BattleUnit* ally : *(_save->getUnits()))
	{
		if (ally->getFaction() != _unit->getFaction())
			continue;
		if (ally->isOut())
			continue;
		if (!dirty && field.hasContribution(ally->getId()))
			continue;
		bool localRanOut = false;
		_save->getPathfinding()->setIgnoreFriends(true);
		const std::map<Position, int, PositionComparator>& result = getReachableBy(ally, localRanOut, dirty, true);
		field.replaceContribution(ally->getId(), result);
		_save->getPathfinding()->setIgnoreFriends(false);
	}
	if (dirty)
	{
		cache->markFriendReachableClean();
	}
	return &field;
}

/**
 * Phase 43.1I (Calypso): returns the live shared negative terrain-LOF cache for
 * this unit's faction, or nullptr when the feature is off / no cache applies.
 *
 * Mirrors the gating of prepareSharedFriendReachable / the TileEngine prefilter:
 *   - the mod must enable ai.sharedFields;
 *   - the calling unit must be AI-controlled (it defines the faction whose
 *     cache we are consulting);
 *   - the faction's turn cache must exist and be valid.
 * When the per-faction terrain-LOF slice is dirty we flush it once and mark it
 * clean (lazy rebuild, matching the other per-faction fields); otherwise we
 * return the live cache as-is. A null return means "feature off / no cache",
 * and every caller must fall back to the original uncached trace (byte-identical
 * behavior). The cache is a pure remember/query surface -- this helper never
 * walks a voxel line or changes an AI decision.
 */
TerrainLofNegativeCache* AIModule::prepareSharedTerrainLofCache()
{
	if (!_save->getMod()->getAISharedFields())
		return nullptr;
	if (!_unit->isAIControlled())
		return nullptr;
	FactionTurnCache* cache = _save->getFactionTurnCache(_unit->getFaction());
	if (cache == nullptr || !cache->isValid())
		return nullptr;
	if (cache->isTerrainLofDirty())
	{
		cache->getTerrainLofCache().clear();
		cache->markTerrainLofClean();
	}
	return &cache->getTerrainLofCache();
}

const std::map<Position, int, PositionComparator>& AIModule::getReachableBy(BattleUnit* unit, bool& ranOutOfTUs, bool forceRecalc, bool useMaxTUs, bool pruneAirTiles)
{
	std::map<Position, int, PositionComparator> tuAtPositionMap;
	Position startPosition = _save->getTileCoords(unit->getTileLastSpotted(_unit->getFaction()));
	if (_unit->isCheatOnMovement() || unit->getFaction() == _unit->getFaction())
		startPosition = unit->getPosition();
	if (startPosition == TileEngine::invalid)
	{
		static const std::map<Position, int, PositionComparator> s_empty;
		return s_empty;
	}
	if (unit->getPositionOfUpdate() == startPosition && unit->wasMaxTusOfUpdate() == useMaxTUs && !forceRecalc)
	{
		ranOutOfTUs = unit->getRanOutOfTUs();
		return unit->getReachablePositions();
	}
	std::vector<PathfindingNode*> reachable = _save->getPathfinding()->findReachablePathFindingNodes(unit, BattleActionCost(), ranOutOfTUs, false, NULL, &startPosition, false, useMaxTUs);
	int TUs = unit->getTimeUnits();
	if (useMaxTUs)
		TUs = getMaxTU(unit);
	for (std::vector<PathfindingNode*>::const_iterator it = reachable.begin(); it != reachable.end(); ++it)
	{
		if (pruneAirTiles && _save->getTile((*it)->getPosition())->hasNoFloor())
			continue;
		tuAtPositionMap[(*it)->getPosition()] = TUs - (*it)->getTUCost(false).time;
		//if (_traceAI && unit->getFaction() == _unit->getFaction())
		//{
		//	Tile* tile = _save->getTile((*it)->getPosition());
		//	tile->setMarkerColor(unit->getId());
		//	tile->setPreview(10);
		//	tile->setTUMarker(TUs - (*it)->getTUCost(false).time);
		//}
	}
	unit->setPositionOfUpdate(startPosition, useMaxTUs);
	unit->setReachablePositions(tuAtPositionMap);
	unit->setRanOutOfTUs(ranOutOfTUs);
	return unit->getReachablePositions();
}

std::map<Position, int, PositionComparator> AIModule::getSmokeFearMap()
{
	std::map<Position, int, PositionComparator> smokeFearMap;
	for (int i = 0; i < _save->getMapSizeXYZ(); i++)
	{
		Tile* tile = _save->getTile(i);
		if (tile && tile->getSmoke() > 0)
		{
			smokeFearMap[tile->getPosition()] = tile->getSmoke();
		}
	}
	return smokeFearMap;
}

bool AIModule::hasTileSight(Position from, Position to)
{
	if (from == to)
		return true;
	if (_save->getTileEngine()->hasEntry(from, to))
	{
		return _save->getTileEngine()->getVisibilityCache(from, to);
	}
	Tile* tile = _save->getTile(from);
	if (!tile)
		return false;
	bool result = true;
	std::vector<Position> trajectory;
	trajectory.clear();
	if (tile->getTerrainLevel() * -1 + _unit->getHeight() - 24 > 0)
		from.z += 1;
	tile = _save->getTile(to);
	if (!tile)
		return false;
	if (tile->getTerrainLevel() * -1 + _unit->getHeight() - 24 > 0)
		to.z += 1;
	if (_save->getTileEngine()->calculateLineTile(from, to, trajectory) > 0)
		result = false;
	_save->getTileEngine()->setVisibilityCache(from, to, result);
	// Set visibility cache for each position in the trajectory
	if (result)
	{
		for (const Position& position : trajectory)
			_save->getTileEngine()->setVisibilityCache(position, to, result);
	}
	return result;
}

int AIModule::requiredWayPointCount(Position to, const std::vector<PathfindingNode*> nodeVector)
{
	PathfindingNode* targetNode = NULL;
	for (auto pn : nodeVector)
	{
		if (to == pn->getPosition())
		{
			targetNode = pn;
			break;
		}
	}
	int lastDirection = -1;
	int directionChanges = 1;
	PathfindingNode* lastWPNode = targetNode;
	if (targetNode != NULL)
	{
		while (targetNode->getPrevNode() != NULL)
		{
			if (targetNode->getPrevNode() != NULL)
			{
				int direction = _save->getTileEngine()->getDirectionTo(targetNode->getPosition(), targetNode->getPrevNode()->getPosition());
				bool zChange = false;
				bool losBreak = false;
				if (targetNode->getPosition().z != targetNode->getPrevNode()->getPosition().z)
					zChange = true;
				if (!hasTileSight(targetNode->getPrevNode()->getPosition(), lastWPNode->getPosition()))
					losBreak = true;
				if (direction != lastDirection || zChange || losBreak)
				{
					++directionChanges;
					lastWPNode = targetNode;
				}
				lastDirection = direction;
			}
			targetNode = targetNode->getPrevNode();
		}
	}
	if (_traceAI)
		Log(LOG_INFO) << "need " << directionChanges << " waypoints to launch blaster at "<<to;
	return directionChanges;
}

void AIModule::getPositionsOnPathTo(Position target, const std::vector<PathfindingNode*>& nodeVector, std::vector<Position>& out)
{
	out.clear();
	PathfindingNode* targetNode = NULL;
	for (auto pn : nodeVector)
	{
		if (target == pn->getPosition())
		{
			targetNode = pn;
			break;
		}
	}
	if (targetNode != NULL)
	{
		while (targetNode->getPrevNode() != NULL)
		{
			out.push_back(targetNode->getPosition());
			//if (_traceAI)
			//{
			//	Tile* tile = _save->getTile(_save->getTileIndex(targetNode->getPosition()));
			//	tile->setMarkerColor(_unit->getId());
			//	tile->setPreview(10);
			//	tile->setTUMarker(_unit->getId() % 100);
			//}
			targetNode = targetNode->getPrevNode();
		}
	}
}

float AIModule::grenadeRiddingUrgency()
{
	if (_grenade && _unit->getGrenadeFromBelt(_save) && _unit->getGrenadeFromBelt(_save)->isFuseEnabled())
	{
		BattleAction action;
		action.weapon = _unit->getGrenadeFromBelt(_save);
		action.type = BA_THROW;
		action.actor = _unit;
		int explosionRadius = action.weapon->getRules()->getExplosionRadius(BattleActionAttack::GetBeforeShoot(action));
		return 1 + -1 * brutalExplosiveEfficacy(_unit->getPosition(), _unit, explosionRadius, true, true);
	}
	return 1;
}

UnitSide AIModule::getSideFacingToPosition(BattleUnit* unit, Position pos)
{
	if (unit->isOut())
		return SIDE_UNDER;

	int direction = unit->getDirection();
	int directionTo = _save->getTileEngine()->getDirectionTo(unit->getPosition(), pos);
	int relativeDirection = (directionTo - direction + 8) % 8;

	if (relativeDirection == 0)
		return SIDE_FRONT;
	else if (relativeDirection == 1)
		return SIDE_LEFT_FRONT;
	else if (relativeDirection == 2)
		return SIDE_LEFT;
	else if (relativeDirection == 3)
		return SIDE_LEFT_REAR;
	else if (relativeDirection == 4)
		return SIDE_REAR;
	else if (relativeDirection == 5)
		return SIDE_RIGHT_REAR;
	else if (relativeDirection == 6)
		return SIDE_RIGHT;
	else if (relativeDirection == 7)
		return SIDE_RIGHT_FRONT;

	return SIDE_UNDER;
}

bool AIModule::wantToRun()
{
	if (!Options::strafe || !_unit->getArmor()->allowsRunning())
		return false;
	if (_unit->getTimeUnits() > 0 && (float) _unit->getEnergy() / _unit->getTimeUnits() > (float)_unit->getArmor()->getMoveCostRun().EnergyPercent / _unit->getArmor()->getMoveCostRun().TimePercent)
	{
		if (_traceAI)
			Log(LOG_INFO) << "Wants to run since energy is decent: " << (float)_unit->getEnergy() / _unit->getTimeUnits() << " / " << (float)_unit->getArmor()->getMoveCostRun().EnergyPercent / _unit->getArmor()->getMoveCostRun().TimePercent;
		return true;
	}
	return false;
}

Position AIModule::getPeakPosition(bool oneStep)
{
	for (PathfindingNode* pn : _allPathFindingNodes)
	{
		Tile* tile = _save->getTile(pn->getPosition());
		if (tile->getLastExplored(_unit->getFaction()) < _save->getTurn())
		{
			return pn->getPosition();
		}
		if (oneStep && pn->getPrevNode() != nullptr && pn->getPrevNode()->getPosition() != _unit->getPosition())
			break;
	}
	return _unit->getPosition();
}

float AIModule::getUnitPower(BattleUnit* unit)
{
	return getMaxTU(unit);
}

std::vector<Tile*> AIModule::getCorpseTiles(const std::vector<PathfindingNode*> nodeVector)
{
	std::vector<Tile*> doorVector;
	for (auto node : nodeVector)
	{
		Tile* tile = _save->getTile(node->getPosition());
		for (auto item : *(tile->getInventory()))
		{
			if (item->getUnit())
				doorVector.push_back(tile);
		}
	}
	return doorVector;
}

bool AIModule::improveItemization(float currentItemScore, BattleAction* action)
{
	if (!_unit->hasInventory())
		return false;
	bool pickedSomethingUp = false;
	Tile* myTile = _unit->getTile();
	Position myPos = _unit->getPosition();
	if (!myTile->getInventory()->empty())
	{
		float highestPickupScore = 0;
		BattleItem* bestItem = nullptr;
		for (BattleItem* item : *myTile->getInventory())
		{
			float pickUpScore = getItemPickUpScore(item);
			if (pickUpScore > currentItemScore && pickUpScore > highestPickupScore)
			{
				highestPickupScore = pickUpScore;
				bestItem = item;
			}
		}
		if (bestItem)
		{
			if (_unit->getMainHandWeapon())
			{
				BattleActionCost cost{action->actor};
				cost.Time += 2;
				if (cost.spendTU())
				{
					if (_traceAI)
						Log(LOG_INFO) << "Dropping " << _unit->getMainHandWeapon()->getRules()->getName() << " to " << myTile->getPosition() << " to replace it with " << bestItem->getRules()->getName();
					_save->getBattleGame()->dropItem(myPos, _unit->getMainHandWeapon());
				}
			}
			if (_save->getBattleGame()->takeItemFromGround(bestItem, action) == 0)
			{
				pickedSomethingUp = true;
				if (_traceAI)
					Log(LOG_INFO) << "Picked up " << bestItem->getRules()->getName() << " from " << myTile->getPosition();
			}
		}
		bool additionalPickup = false;
		do
		{
			additionalPickup = false;
			BattleItem* itemToPickup = nullptr;
			for (BattleItem* item : *myTile->getInventory())
			{
				if (item->getRules()->getWeight() + _unit->getCarriedWeight() > _unit->getBaseStats()->strength)
					continue;
				bool IsUsefull = false;
				if (item->getRules()->getBattleType() == BT_AMMO && _unit->getMainHandWeapon())
				{
					if (_unit->getMainHandWeapon()->getRules()->getSlotForAmmo(item->getRules()) != -1)
						IsUsefull = true;
				}
				if (item->getRules()->getBattleType() == BT_GRENADE)
					IsUsefull = true;
				if (IsUsefull)
				{
					itemToPickup = item;
					break;
				}
			}
			if (itemToPickup)
			{
				int takeResult = _save->getBattleGame()->takeItemFromGround(itemToPickup, action);
				if (takeResult == 0)
				{
					pickedSomethingUp = true;
					if (_traceAI)
						Log(LOG_INFO) << "Picked up " << itemToPickup->getRules()->getName() << " from " << myTile->getPosition();
					additionalPickup = true;
				}
			}
		} while (additionalPickup);
	}
	if (pickedSomethingUp) _save->getBattleGame()->markAIWorldChanged();
	return pickedSomethingUp;
}

int AIModule::scoreVisibleTiles(const std::set<Tile*>& tileSet)
{
	int totalScore = 0;
	for (Tile* tile : tileSet)
	{
		totalScore += _save->getTurn() - tile->getLastExplored(_unit->getFaction());
	}
	return totalScore;
}

BattleAction* AIModule::grenadeThrowAction(Position pos)
{
	BattleItem* grenade = _unit->getGrenadeFromBelt(_save);
	if (grenade == NULL || !grenade->isFuseEnabled())
		return NULL;
	BattleAction* action = new BattleAction();
	action->weapon = grenade;
	action->type = BA_THROW;
	action->actor = _unit;
	action->target = pos;
	return action;
}

float AIModule::damagePotential(Position pos, BattleUnit* target, int tuTotal, int energyTotal)
{
	float overallMaxDamage = 0;
	std::vector<BattleItem*> weapons;
	if (_unit->getRightHandWeapon())
		weapons.push_back(_unit->getRightHandWeapon());
	if (_unit->getLeftHandWeapon())
		weapons.push_back(_unit->getLeftHandWeapon());
	if (_unit->getUtilityWeapon(BT_MELEE))
		weapons.push_back(_unit->getUtilityWeapon(BT_MELEE));
	if (_unit->getSpecialWeapon(BT_FIREARM))
		weapons.push_back(_unit->getSpecialWeapon(BT_FIREARM));
	if (_grenade && _unit->getGrenadeFromBelt(_save))
		weapons.push_back(_unit->getGrenadeFromBelt(_save));

	std::vector<BattleActionType> actionTypes;
	actionTypes.push_back(BA_AIMEDSHOT);
	actionTypes.push_back(BA_SNAPSHOT);
	actionTypes.push_back(BA_AUTOSHOT);
	actionTypes.push_back(BA_THROW);
	actionTypes.push_back(BA_HIT);

	int distanceSq = Position::distanceSq(pos, target->getPosition());
	float distance = Position::distance(pos, target->getPosition());

	for (auto weapon : weapons)
	{
		float maxFinalDamageForThisWeapon = 0; // Tracks the best *calculated* damage for the current weapon

		for (BattleActionType bat : actionTypes)
		{
			float explosionMod = 1.0f;
			float numberOfShots = 1;
			float currentActionRawPower = 0; // Raw power specifically for THIS action (bat)
			int tuCost = _unit->getActionTUs(bat, weapon).Time;
			int energyCost = _unit->getActionTUs(bat, weapon).Energy;
			if (bat == BA_THROW && weapon == _unit->getGrenadeFromBelt(_save))
			{
				if (!_grenade)
					continue;
				if (target->getTile()->getDangerous())
					continue;
				if (!_unit->getGrenadeFromBelt(_save)->isFuseEnabled())
				{
					tuCost += weapon->getMoveToCost(_save->getMod()->getInventoryLeftHand());
					tuCost += _unit->getActionTUs(BA_PRIME, _unit->getGrenadeFromBelt(_save)).Time;
				}
				// We don't have several shots but we can hit several targets at once
				auto radius = weapon->getRules()->getExplosionRadius(BattleActionAttack::GetBeforeShoot(bat, _unit, weapon));
				explosionMod = brutalExplosiveEfficacy(target->getPosition(), _unit, radius, true);
				explosionMod *= grenadeRiddingUrgency();
			}
			else
			{
				if (bat == BA_THROW && weapon != _unit->getGrenadeFromBelt(_save))
					continue;
				auto ammo = weapon->getAmmoForAction(bat);
				if (ammo)
				{
					currentActionRawPower = ammo->getRules()->getPower();
					int radius = ammo->getRules()->getExplosionRadius({bat, _unit, _attackAction.weapon, ammo});
					if (radius > 0)
						explosionMod *= brutalExplosiveEfficacy(target->getPosition(), _unit, radius, false);
					if (ammo->getRules()->getShotgunPellets() > 0)
						numberOfShots *= ammo->getRules()->getShotgunPellets();
				}
				else
					continue;
			}

			// Get base accuracy for the action
			float accuracy = BattleUnit::getFiringAccuracy(BattleActionAttack::GetBeforeShoot(bat, _unit, weapon), _save->getBattleGame()->getMod());

			if (Options::battleUFOExtenderAccuracy && bat != BA_THROW)
			{
				int upperLimit;
				if (bat == BA_AIMEDSHOT)
				{
					upperLimit = weapon->getRules()->getAimRange();
				}
				else if (bat == BA_AUTOSHOT)
				{
					upperLimit = weapon->getRules()->getAutoRange();
				}
				else
				{
					upperLimit = weapon->getRules()->getSnapRange();
				}
				int lowerLimit = weapon->getRules()->getMinRange();

				if (distance > upperLimit)
				{
					accuracy -= (distance - upperLimit) * weapon->getRules()->getDropoff();
				}
				else if (distance < lowerLimit)
				{
					accuracy -= (lowerLimit - distance) * weapon->getRules()->getDropoff();
				}
			}
			if (weapon->getRules()->getNoLOSAccuracyPenalty(const_cast<Mod*>(_save->getMod())) != -1)
			{
				Tile* targetTile = target->getTile();
				bool shouldHaveLos = true;
				if (targetTile)
				{
					int viewDistance = _unit->getMaxViewDistanceAtDay(target);
					if (target->getTile()->getShade() > _save->getMod()->getMaxDarknessToSeeUnits() && target->getTile()->getFire() == 0)
						viewDistance = _unit->getMaxViewDistanceAtDark(target);
					float minViewDistance = _save->getMod()->getMaxViewDistance() / (1.0 + targetTile->getSmoke() / 3.0);
					viewDistance = std::min(viewDistance, (int)minViewDistance);
					if (Position::distance(pos, target->getPosition()) > viewDistance)
						shouldHaveLos = false;
				}
				else
					shouldHaveLos = false;
				if (!shouldHaveLos)
					accuracy *= weapon->getRules()->getNoLOSAccuracyPenalty(const_cast<Mod*>(_save->getMod())) / 100.0;
			}

			if (bat != BA_THROW && weapon->getRules()->isOutOfRange(distanceSq))
				accuracy = 0;
			if (bat == BA_HIT)
			{
				Position attackVexel = pos.toVoxel();
				attackVexel += Position(8, 8, 0) * _unit->getArmor()->getSize();
				int arc = _save->getTileEngine()->getArcDirection(_save->getTileEngine()->getDirectionTo(target->getPositionVexels(), attackVexel), target->getDirection());
				float penalty = 1.0f - arc * target->getArmor()->getMeleeDodgeBackPenalty() / 4.0f;
				if (target->getArmor()->getMeleeDodge(target) * penalty < accuracy)
					accuracy -= target->getArmor()->getMeleeDodge(target) * penalty;
				else
					accuracy = 0.01;
				// We can definitely assume we'll be facing the target
				int directionToLook = _save->getTileEngine()->getDirectionTo(pos, target->getPosition());
				if (!_save->getTileEngine()->validMeleeRange(pos, directionToLook, _unit, target, 0))
				{
					accuracy = 0;
				}
			}
			else if (shouldAvoidMeleeRange(target) && distance < 2)
			{
				accuracy = 0;
			}

			if (bat == BA_AIMEDSHOT)
			{
				numberOfShots = weapon->getRules()->getConfigAimed()->shots;
			}
			else if (bat == BA_SNAPSHOT)
			{
				numberOfShots = weapon->getRules()->getConfigSnap()->shots;
			}
			else if (bat == BA_AUTOSHOT)
			{
				numberOfShots = weapon->getRules()->getConfigAuto()->shots;
			}
			else if (bat == BA_HIT)
			{
				numberOfShots = weapon->getRules()->getConfigMelee()->shots;
			}

			// check next firing mode if current one doesn't exist for this weapon
			if (!tuCost)
				continue;

			int possibleActions = tuTotal / tuCost;

			if (energyCost > 0)
				possibleActions = std::min(possibleActions, energyTotal / energyCost);

			if (bat == BA_THROW)
			{
				possibleActions = std::min(possibleActions, 1);
			}

			// If we don't have enough TU/Energy for even a single action, skip.
			if (possibleActions < 1)
				continue;

			auto ammo = weapon->getAmmoForAction(bat);
			if (ammo)
			{
				currentActionRawPower = std::max(currentActionRawPower, (float)ammo->getRules()->getPower());
			}
			// Add power bonus for this specific action
			currentActionRawPower = std::max(currentActionRawPower, (float)weapon->getRules()->getPowerBonus(BattleActionAttack::GetBeforeShoot(bat, _unit, weapon)));

			if (currentActionRawPower <= 0) // If this specific action has no power, it won't contribute damage
			{
				continue;
			}

			float relevantArmor = 0;
			if (bat == BA_THROW) // BA_THROW uses underarmor
				relevantArmor = target->getArmor()->getUnderArmor();
			else
			{
				UnitSide side = getSideFacingToPosition(target, pos);
				// Simplified armor logic for brevity
				if (side == SIDE_FRONT || side == SIDE_RIGHT || side == SIDE_LEFT || side == SIDE_REAR || side == SIDE_UNDER)
					relevantArmor = target->getArmor()->getArmor(side);
				else if (side == SIDE_LEFT_FRONT)
					relevantArmor = (target->getArmor()->getArmor(SIDE_LEFT) + target->getArmor()->getArmor(SIDE_FRONT)) / 2.0f;
				else if (side == SIDE_RIGHT_FRONT)
					relevantArmor = (target->getArmor()->getArmor(SIDE_RIGHT) + target->getArmor()->getArmor(SIDE_FRONT)) / 2.0f;
				else if (side == SIDE_LEFT_REAR)
					relevantArmor = (target->getArmor()->getArmor(SIDE_LEFT) + target->getArmor()->getArmor(SIDE_REAR)) / 2.0f;
				else if (side == SIDE_RIGHT_REAR)
					relevantArmor = (target->getArmor()->getArmor(SIDE_RIGHT) + target->getArmor()->getArmor(SIDE_REAR)) / 2.0f;
			}

			// Start with the raw power of THIS specific action
			float damageForCalc = currentActionRawPower;

			// Apply resistance modifier
			damageForCalc *= target->getArmor()->getDamageModifier(weapon->getRules()->getDamageType()->ResistType);
			float armorPreDamage = weapon->getRules()->getDamageType()->getArmorPreFinalDamage(damageForCalc);

			float damageRangeFactor = 1.0f + _save->getMod()->DAMAGE_RANGE / 100.0f;

			accuracy /= 100.0;
			// Apply a modifier for higher/lower hit-chance when closer/further from the target. But not for melee-attacks.
			if (bat != BA_HIT && !Options::battleRealisticAccuracy)
			{
				if (accuracy > 0)
					accuracy += std::max(1 - accuracy, 0.0f) / distance;
				accuracy = std::min(1.0f, accuracy);
			}
			// Calculate final damage for *this action* using its own (modified) power
			float damagePerExecution = (damageForCalc * damageRangeFactor - relevantArmor) / 2.0f;
			damagePerExecution *= accuracy * numberOfShots * explosionMod;
			damagePerExecution = std::max(0.0f, damagePerExecution + armorPreDamage / 3.0f);

			// --- FIX 2: Kill Efficiency Calculation ---

			float hp = target->getHealth();

			// Step A: Calculate how many actions it takes to actually kill this specific target
			// We use a small epsilon or ceil to ensure we don't under-estimate.
			int actionsNeededToKill = 999;
			if (damagePerExecution > 0)
			{
				actionsNeededToKill = (int)std::ceil(hp / damagePerExecution);
			}

			// Step B: Determine what happens to the CURRENT target
			int actionsUsedOnTarget = std::min(possibleActions, actionsNeededToKill);

			float damageToTarget = damagePerExecution * actionsUsedOnTarget;

			// Soft Cap: Value damage up to HP highly, overkill loosely.
			if (damageToTarget > hp)
			{
				float overkill = damageToTarget - hp;
				damageToTarget = hp + (overkill * 0.1f);
			}

			// Step C: Calculate Remaining Potential (The "Free TUs" Bonus)
			// If we killed the target and have actions left, those are valuable!
			float remainingPotential = 0;

			if (possibleActions > actionsUsedOnTarget)
			{
				int actionsRemaining = possibleActions - actionsUsedOnTarget;

				// If this is a grenade, we usually don't have infinite grenades to throw,
				// so remaining potential for this specific weapon is 0.
				if (bat != BA_THROW)
				{
					// We assume the remaining actions could be used on OTHER targets
					// with similar armor/resistance (heuristic).
					remainingPotential = actionsRemaining * damagePerExecution;
				}
			}

			// Final Score = The Value of the Kill + The Value of the leftover TUs
			float finalScore = damageToTarget + remainingPotential;

			maxFinalDamageForThisWeapon = std::max(maxFinalDamageForThisWeapon, finalScore);
		}
		// After checking all actions for this weapon, update the overall maximum damage
		overallMaxDamage = std::max(overallMaxDamage, maxFinalDamageForThisWeapon);
	}
	return overallMaxDamage;
}

bool AIModule::isPositionVisibleToEnemy(Position pos, bool tileLOSMode)
{
	for (BattleUnit* bu : *(_save->getUnits()))
	{
		if (!isEnemy(bu) || bu->isOut())
			continue;
		for (int x = 0; x < _unit->getArmor()->getSize(); ++x)
		{
			for (int y = 0; y < _unit->getArmor()->getSize(); ++y)
			{
				Position checkPos = pos;
				checkPos += Position(x, y, 0);
				if (tileLOSMode)
				{
					for (int x2 = 0; x2 < bu->getArmor()->getSize(); ++x2)
					{
						for (int y2 = 0; y2 < bu->getArmor()->getSize(); ++y2)
						{
							Position checkPos2 = bu->getPosition();
							checkPos2 += Position(x2, y2, 0);
							if (hasTileSight(checkPos2, checkPos))
								return true;
						}
					}
					continue;
				}
				for (Tile* buVisible : *bu->getVisibleTiles())
				{
					if (buVisible->getPosition() == checkPos)
						return true;
				}
			}
		}
	}
	return false;
}

}
