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
 * Default state: invalid (activeTurn == -1) and every field dirty.
 */
struct FactionTurnCache
{
	int activeTurn = -1;               // faction turn this cache was last armed for; -1 == invalid
	bool threatDirty = true;           // threat field stale -> rebuild lazily on next query
	bool friendReachableDirty = true;  // friendReachable aggregate stale -> rebuild lazily
	bool terrainLofDirty = true;       // terrain-LOF negative cache stale -> flush on read
	unsigned int terrainRevision = 0;  // monotonically increasing terrain mutation counter

	/// Live friendReachable aggregate accumulator (phase 43.1D). Stamped/un-stamped by
	/// the field builder; wiped on turn start and on terrain mutation (see friendReachableDirty).
	FriendReachableField friendReachable;

	/// Mutable access to the friendReachable accumulator (field builders stamp/un-stamp here).
	FriendReachableField& getFriendReachable() { return friendReachable; }
	/// Const access to the friendReachable accumulator.
	const FriendReachableField& getFriendReachable() const { return friendReachable; }

	/// Arm the cache for the start of a faction turn: record the turn and mark every
	/// terrain-sensitive field dirty. terrainRevision is left untouched (see class note).
	/// The friendReachable accumulator is wiped so it is rebuilt lazily on first read.
	void beginTurn(int turn)
	{
		activeTurn = turn;
		threatDirty = true;
		friendReachableDirty = true;
		terrainLofDirty = true;
		friendReachable.clear();
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

	/// Terrain mutation (explosion / wall destruction / door state): bump the global revision
	/// and dirty every terrain-sensitive field. Independent of the armed turn. The
	/// friendReachable accumulator is wiped (its underlying BFS memo is terrain-keyed and
	/// must be rebuilt).
	void onTerrainChanged()
	{
		++terrainRevision;
		threatDirty = true;
		friendReachableDirty = true;
		terrainLofDirty = true;
		friendReachable.clear();
	}
};

} // namespace OpenXcom
