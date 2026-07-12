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

#include "FriendReachableField.h"
#include "TerrainLofNegativeCache.h"
#include "ThreatField.h"
#include <map>

namespace OpenXcom
{

/**
 * Phase 43.1B (Calypso): per-faction turn-cache invalidation state.
 *
 * A pure, dependency-free bookkeeping companion to the Phase 34.9 SquadBlackboard
 * (SavedBattleGame). It records, per UnitFaction, which shared spatial fields are
 * stale and how many terrain mutations have occurred, so later slices (43.1 field
 * builders) can rebuild lazily on first read instead of eagerly every turn.
 *
 * This slice ONLY tracks invalidation -- it does NOT build any field, does NOT read
 * unit/terrain state, and does NOT change any AI decision. Everything here is
 * transient and intentionally NOT serialized: a fresh save loads the caches in their
 * default-invalid state, and they are (re)armed at each faction-turn seam.
 *
 * Dirty policy (per the 43.1 field table):
 *   - threat field, friendReachable aggregate, and terrain-LOF negative cache each
 *     have an independent dirty flag; a single clean method affects only its flag.
 *   - all three are dirtied at faction-turn start (beginTurn) and on every terrain
 *     mutation (onTerrainChanged).
 *   - terrainRevision is a monotonic counter bumped ONLY by terrain mutations; it is
 *     never reset by beginTurn, so a mutation that happened before a turn still
 *     reads as "newer" than any cached field.
 *
 * The live friendReachable accumulator (FriendReachableField, phase 43.1D) is owned
 * here and is wiped by beginTurn and onTerrainChanged (its content is recomputed by
 * the field builder on the next read). markFriendReachableClean only clears the dirty
 * flag -- it must NOT wipe the accumulated field.
 *
 * The live threat accumulator (ThreatField, phase 43.1F) and the live negative-only
 * terrain-LOF cache (TerrainLofNegativeCache, phase 43.1G) are likewise owned here and
 * wiped by beginTurn / onTerrainChanged. markThreatClean and markTerrainLofClean clear
 * only their dirty flags and must NOT wipe their respective content.
 *
 * Default state: invalid (activeTurn == -1) and every field dirty.
 */
struct FactionTurnCache
{
	using PendingThreatSightings = std::map<int, Position>;

	int activeTurn = -1;               // faction turn this cache was last armed for; -1 == invalid
	bool threatDirty = true;           // threat field stale -> rebuild lazily on next query
	bool friendReachableDirty = true;  // friendReachable aggregate stale -> rebuild lazily
	bool terrainLofDirty = true;       // terrain-LOF negative cache stale -> flush on read
	unsigned int terrainRevision = 0;  // monotonically increasing terrain mutation counter

	/// Live friendReachable aggregate accumulator (phase 43.1D). Stamped/un-stamped by
	/// the field builder; wiped on turn start and on terrain mutation (see friendReachableDirty).
	FriendReachableField friendReachable;

	/// Live threat accumulator (phase 43.1F). Stamped by the threat field builder;
	/// wiped on turn start and on terrain mutation (see threatDirty).
	ThreatField threat;

	/// New or updated fair-knowledge sightings that a clean threat producer has not
	/// absorbed yet. The latest tile wins per enemy id, so repeated FOV refreshes do
	/// not grow the queue. This is transient producer input, not serialized state.
	PendingThreatSightings pendingThreatSightings;

	/// Live negative-only terrain-LOF cache (phase 43.1G). Remembers directed
	/// terrain-blocked voxel rays; flushed on turn start and on terrain mutation
	/// (see terrainLofDirty). LOF probes populate and read it; it is wiped by
	/// beginTurn / onTerrainChanged, never by markTerrainLofClean.
	TerrainLofNegativeCache terrainLof;

	/// Mutable access to the friendReachable accumulator (field builders stamp/un-stamp here).
	FriendReachableField& getFriendReachable() { return friendReachable; }
	/// Const access to the friendReachable accumulator.
	const FriendReachableField& getFriendReachable() const { return friendReachable; }

	/// Mutable access to the threat accumulator (field builders stamp here).
	ThreatField& getThreatField() { return threat; }
	/// Const access to the threat accumulator.
	const ThreatField& getThreatField() const { return threat; }

	/// Latest pending fair-known tile for each newly/again spotted enemy.
	const PendingThreatSightings& getPendingThreatSightings() const { return pendingThreatSightings; }

	/// Record producer input only for an armed faction cache. This deliberately does
	/// not mutate the threat field or its dirty flag: the later producer owns the
	/// exact gameplay formula and can consume these events incrementally.
	void recordKnowledgeChanged(int enemyId, const Position& knownTile)
	{
		if (!isValid())
			return;
		pendingThreatSightings[enemyId] = knownTile;
	}

	/// Called after a threat producer has absorbed every queued sighting.
	void clearPendingThreatSightings() { pendingThreatSightings.clear(); }

	/// Mutable access to the terrain-LOF negative cache (field builders remember
	/// directed blocked rays here).
	TerrainLofNegativeCache& getTerrainLofCache() { return terrainLof; }
	/// Const access to the terrain-LOF negative cache.
	const TerrainLofNegativeCache& getTerrainLofCache() const { return terrainLof; }

	/// Arm the cache for the start of a faction turn: record the turn and mark every
	/// terrain-sensitive field dirty. terrainRevision is left untouched (see class note).
	/// The friendReachable accumulator, threat accumulator, and terrain-LOF negative
	/// cache are wiped so they are rebuilt lazily on first read.
	void beginTurn(int turn)
	{
		activeTurn = turn;
		threatDirty = true;
		friendReachableDirty = true;
		terrainLofDirty = true;
		friendReachable.clear();
		threat.clear();
		pendingThreatSightings.clear();
		terrainLof.clear();
	}

	/// True once beginTurn has armed the cache for a real turn; false in the default state.
	bool isValid() const { return activeTurn >= 0; }

	bool isThreatDirty() const { return threatDirty; }
	bool isFriendReachableDirty() const { return friendReachableDirty; }
	bool isTerrainLofDirty() const { return terrainLofDirty; }
	unsigned int getTerrainRevision() const { return terrainRevision; }

	/// Clear only the threat field's dirty flag (a field builder calls this after a rebuild).
	void markThreatClean() { threatDirty = false; }
	/// Clear only the friendReachable aggregate's dirty flag.
	void markFriendReachableClean() { friendReachableDirty = false; }
	/// Clear only the terrain-LOF negative cache's dirty flag.
	void markTerrainLofClean() { terrainLofDirty = false; }

	/// Remove ONLY a single unit's contribution from the live friendReachable
	/// accumulator. Contrast beginTurn / onTerrainChanged, which wipe the whole
	/// field: this leaves every other unit's contribution intact and does NOT
	/// touch any dirty flag (the rest of the cached field is still valid). A
	/// missing id is a harmless no-op. Used by the Phase 43.1E unit-lifecycle
	/// notifications so one unit moving / dying / spawning invalidates only its
	/// own slice while the other units' cached contributions stay valid.
	void invalidateFriendContribution(int unitId)
	{
		friendReachable.removeContribution(unitId);
	}

	/// Terrain mutation (explosion / wall destruction / door state): bump the global revision
	/// and dirty every terrain-sensitive field. Independent of the armed turn. The
	/// friendReachable accumulator, threat accumulator, and terrain-LOF negative cache
	/// are wiped (their content is terrain-keyed and must be rebuilt).
	void onTerrainChanged()
	{
		++terrainRevision;
		threatDirty = true;
		friendReachableDirty = true;
		terrainLofDirty = true;
		friendReachable.clear();
		threat.clear();
		// A full terrain-driven threat rebuild must read authoritative current
		// knowledge, so queued incremental inputs are redundant after this point.
		pendingThreatSightings.clear();
		terrainLof.clear();
	}
};

} // namespace OpenXcom
