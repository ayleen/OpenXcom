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
#include "OccupancyField.h"
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
 *   - threat field, friend/enemy reachable aggregates, and terrain-LOF negative cache each
 *     have an independent dirty flag; a single clean method affects only its flag.
 *   - all four are dirtied at faction-turn start (beginTurn) and on every terrain
 *     mutation (onTerrainChanged).
 *   - terrainRevision is a monotonic counter bumped ONLY by terrain mutations; it is
 *     never reset by beginTurn, so a mutation that happened before a turn still
 *     reads as "newer" than any cached field.
 *
 * The live friendReachable and enemyReachable accumulators (FriendReachableField,
 * phases 43.1D/43.1Q) are owned here and wiped by beginTurn and onTerrainChanged.
 * Their clean methods only clear dirty flags and must NOT wipe accumulated fields.
 *
 * The live threat accumulator (ThreatField, phase 43.1F) and the live negative-only
 * terrain-LOF cache (TerrainLofNegativeCache, phase 43.1G) are likewise owned here and
 * wiped by beginTurn / onTerrainChanged. markThreatClean and markTerrainLofClean clear
 * only their dirty flags and must NOT wipe their respective content.
 *
 * The live occupancy accumulator (OccupancyField, phase 43.1 occupancy slice) is owned
 * here BY VALUE together with its `occupancyLastAdvancedTurn` arm marker. UNLIKE every
 * other owned field, occupancy is NEVER cleared or mutated by beginTurn, onTerrainChanged,
 * the knowledge / unit-lifecycle paths, or any dirty policy: it is advanced explicitly by
 * advanceOccupancyToTurn and replaced explicitly by setOccupancyState. The marker starts
 * at -1 (disarmed) and is preserved across turn / terrain / knowledge boundaries.
 *
 * Default state: invalid (activeTurn == -1) and every field dirty.
 */
struct FactionTurnCache
{
	using PendingThreatSightings = std::map<int, Position>;

	int activeTurn = -1;               // faction turn this cache was last armed for; -1 == invalid
	bool threatDirty = true;           // threat field stale -> rebuild lazily on next query
	bool friendReachableDirty = true;  // friendReachable aggregate stale -> rebuild lazily
	bool enemyReachableDirty = true;   // enemyReachable aggregate stale -> rebuild lazily
	bool terrainLofDirty = true;       // terrain-LOF negative cache stale -> flush on read
	unsigned int terrainRevision = 0;  // monotonically increasing terrain mutation counter
	bool threatProfileValid = false;   // whether the shared threat memo has an actor geometry profile
	int threatProfileSize = 0;         // armor footprint used by the exact legacy-equivalent probe
	int threatProfileHeight = 0;       // eye-height input used by AIModule::hasTileSight
	bool threatProfileMovementCheat = false; // fair-known vs live-position reachability mode
	bool enemyReachableProfileValid = false;
	bool enemyReachableProfileMovementCheat = false;

	/// Live friendReachable aggregate accumulator (phase 43.1D). Stamped/un-stamped by
	/// the field builder; wiped on turn start and on terrain mutation (see friendReachableDirty).
	FriendReachableField friendReachable;

	/// Per-enemy reachability slices and their exact aggregate sum. The existing
	/// accumulator is intentionally reused: its accounting is faction-agnostic.
	FriendReachableField enemyReachable;

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

	/// Live occupancy accumulator (phase 43.1 occupancy slice). Owned by value and
	/// SPIKED by a later occupancy producer. Unlike the threat / reachable / terrain-LOF
	/// fields, this field is NEVER cleared or mutated by beginTurn, onTerrainChanged, or
	/// any knowledge / unit-lifecycle / dirty path: it advances ONLY via
	/// advanceOccupancyToTurn (exact integer decay+spread catch-up) and is replaced ONLY
	/// via the setOccupancyState load seam. Its arm marker starts at -1 (disarmed) and is
	/// likewise preserved across every other cache transition.
	OccupancyField occupancy;
	/// Last faction turn the occupancy field was advanced to; -1 == disarmed (the field
	/// has never been caught up to a real turn yet, so the first advance arms the marker
	/// without decaying whatever cells already exist).
	int occupancyLastAdvancedTurn = -1;

	/// Mutable access to the friendReachable accumulator (field builders stamp/un-stamp here).
	FriendReachableField& getFriendReachable() { return friendReachable; }
	/// Const access to the friendReachable accumulator.
	const FriendReachableField& getFriendReachable() const { return friendReachable; }
	/// Mutable access to the shared enemyReachable accumulator.
	FriendReachableField& getEnemyReachable() { return enemyReachable; }
	/// Const access to the shared enemyReachable accumulator.
	const FriendReachableField& getEnemyReachable() const { return enemyReachable; }

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
		invalidateEnemyContribution(enemyId);
	}

	/// Called after a threat producer has absorbed every queued sighting.
	void clearPendingThreatSightings() { pendingThreatSightings.clear(); }

	/// Bind a freshly-cleared threat memo to the actor-dependent geometry inputs
	/// used by the legacy discoverThreat loop.
	void setThreatProfile(int size, int height, bool movementCheat)
	{
		threatProfileValid = true;
		threatProfileSize = size;
		threatProfileHeight = height;
		threatProfileMovementCheat = movementCheat;
	}

	/// Exact reuse is safe only for actors whose legacy threat probe uses the
	/// same footprint, eye height, and fair/cheat reachability mode.
	bool matchesThreatProfile(int size, int height, bool movementCheat) const
	{
		return threatProfileValid
			&& threatProfileSize == size
			&& threatProfileHeight == height
			&& threatProfileMovementCheat == movementCheat;
	}

	void clearThreatProfile()
	{
		threatProfileValid = false;
		threatProfileSize = 0;
		threatProfileHeight = 0;
		threatProfileMovementCheat = false;
	}

	void setEnemyReachableProfile(bool movementCheat)
	{
		enemyReachableProfileValid = true;
		enemyReachableProfileMovementCheat = movementCheat;
	}

	bool matchesEnemyReachableProfile(bool movementCheat) const
	{
		return enemyReachableProfileValid
			&& enemyReachableProfileMovementCheat == movementCheat;
	}

	void clearEnemyReachableProfile()
	{
		enemyReachableProfileValid = false;
		enemyReachableProfileMovementCheat = false;
	}

	/// Mutable access to the terrain-LOF negative cache (field builders remember
	/// directed blocked rays here).
	TerrainLofNegativeCache& getTerrainLofCache() { return terrainLof; }
	/// Const access to the terrain-LOF negative cache.
	const TerrainLofNegativeCache& getTerrainLofCache() const { return terrainLof; }

	/// Mutable access to the occupancy accumulator (a producer spikes cells here; the
	/// catch-up advance and the load seam mutate it directly).
	OccupancyField& getOccupancyField() { return occupancy; }
	/// Const access to the occupancy accumulator.
	const OccupancyField& getOccupancyField() const { return occupancy; }
	/// Last faction turn the occupancy field was advanced to (-1 == disarmed / never).
	int getOccupancyLastAdvancedTurn() const { return occupancyLastAdvancedTurn; }

	/// Arm the cache for the start of a faction turn: record the turn and mark every
	/// terrain-sensitive field dirty. terrainRevision is left untouched (see class note).
	/// The friendReachable accumulator, threat accumulator, and terrain-LOF negative
	/// cache are wiped so they are rebuilt lazily on first read. The occupancy field and
	/// its occupancyLastAdvancedTurn marker are PRESERVED (occupancy advances on its own
	/// explicit cadence via advanceOccupancyToTurn, independent of the armed turn).
	void beginTurn(int turn)
	{
		activeTurn = turn;
		threatDirty = true;
		friendReachableDirty = true;
		enemyReachableDirty = true;
		terrainLofDirty = true;
		friendReachable.clear();
		enemyReachable.clear();
		clearEnemyReachableProfile();
		threat.clear();
		clearThreatProfile();
		pendingThreatSightings.clear();
		terrainLof.clear();
	}

	/// True once beginTurn has armed the cache for a real turn; false in the default state.
	bool isValid() const { return activeTurn >= 0; }

	bool isThreatDirty() const { return threatDirty; }
	bool isFriendReachableDirty() const { return friendReachableDirty; }
	bool isEnemyReachableDirty() const { return enemyReachableDirty; }
	bool isTerrainLofDirty() const { return terrainLofDirty; }
	unsigned int getTerrainRevision() const { return terrainRevision; }

	/// Clear only the threat field's dirty flag (a field builder calls this after a rebuild).
	void markThreatClean() { threatDirty = false; }
	/// Clear only the friendReachable aggregate's dirty flag.
	void markFriendReachableClean() { friendReachableDirty = false; }
	/// Clear only the enemyReachable aggregate's dirty flag.
	void markEnemyReachableClean() { enemyReachableDirty = false; }
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

	/// Remove one enemy's reachability slice without disturbing other enemies.
	void invalidateEnemyContribution(int unitId)
	{
		enemyReachable.removeContribution(unitId);
	}

	/// Terrain mutation (explosion / wall destruction / door state): bump the global revision
	/// and dirty every terrain-sensitive field. Independent of the armed turn. The
	/// friend/enemy reachable accumulators, threat accumulator, and terrain-LOF negative cache
	/// are wiped (their content is terrain-keyed and must be rebuilt). The occupancy field and
	/// its occupancyLastAdvancedTurn marker are PRESERVED (occupancy is terrain-agnostic and
	/// advances on its own explicit cadence via advanceOccupancyToTurn).
	void onTerrainChanged()
	{
		++terrainRevision;
		threatDirty = true;
		friendReachableDirty = true;
		enemyReachableDirty = true;
		terrainLofDirty = true;
		friendReachable.clear();
		enemyReachable.clear();
		clearEnemyReachableProfile();
		threat.clear();
		clearThreatProfile();
		// A full terrain-driven threat rebuild must read authoritative current
		// knowledge, so queued incremental inputs are redundant after this point.
		pendingThreatSightings.clear();
		terrainLof.clear();
		// NOTE: occupancy and occupancyLastAdvancedTurn are intentionally NOT touched --
		// they survive terrain changes (see the class doc and advanceOccupancyToTurn).
	}

	/// Advance the occupancy field's decay/spread cadence up to `currentTurn`, applying
	/// EXACTLY ONE OccupancyField::decayAndSpread pass per missed integer turn. This is
	/// the ONLY path (besides setOccupancyState) that mutates the occupancy field, and it
	/// is independent of beginTurn / onTerrainChanged / knowledge / unit-lifecycle.
	///
	/// Contract:
	///   - currentTurn < 0: NO-OP (a not-yet-real turn never advances the field).
	///   - occupancyLastAdvancedTurn == -1 (disarmed): ARM the marker to currentTurn
	///     WITHOUT decaying the existing cells (the first catch-up stamps the baseline).
	///   - currentTurn <= occupancyLastAdvancedTurn (already armed): NO-OP (same turn or
	///     a backward step never re-decays).
	///   - otherwise: run (currentTurn - occupancyLastAdvancedTurn) decay+spread passes
	///     in order and set the marker to currentTurn.
	///
	/// The decay+spread pass delegates to OccupancyField::decayAndSpread, so its
	/// invalid-dims / non-positive-maxValue SAFE-CLEAR policies apply unchanged. The map
	/// dimensions and percentages are forwarded verbatim to every pass.
	void advanceOccupancyToTurn(int currentTurn, int mapSizeX, int mapSizeY, int mapSizeZ,
		int retainPercent, int spreadPercent, int maxValue = 1000)
	{
		if (currentTurn < 0)
			return;                         // a not-yet-real turn never advances the field
		if (occupancyLastAdvancedTurn == -1)
		{
			// First arm: stamp the baseline marker without decaying existing cells.
			occupancyLastAdvancedTurn = currentTurn;
			return;
		}
		if (currentTurn <= occupancyLastAdvancedTurn)
			return;                         // same or backward turn: never re-decay
		const int missed = currentTurn - occupancyLastAdvancedTurn;
		for (int i = 0; i < missed; ++i)
			occupancy.decayAndSpread(mapSizeX, mapSizeY, mapSizeZ,
				retainPercent, spreadPercent, maxValue);
		occupancyLastAdvancedTurn = currentTurn;
	}

	/// Load seam: replace the occupancy field's entire state deterministically. This is a
	/// DATA-ONLY replacement (no YAML, no Mod, no engine state): it clears the live field
	/// and replays a validated subset of the supplied sparse map. Used by a later
	/// serialization slice to restore an occupancy snapshot; safe to call with an empty
	/// map (the field ends up cleared) and safe to call before the marker has been armed.
	///
	/// Replacement policy (applied in order):
	///   - occupancyLastAdvancedTurn = max(lastTurn, -1)  (clamp: -1 is the disarmed floor).
	///   - Wipe every existing cell.
	///   - For every entry in `cells`: drop it UNLESS its value is strictly positive AND
	///     its Position is strictly in-bounds for (mapSizeX, mapSizeY, mapSizeZ). Each
	///     surviving entry is then SPIKED into the cleared field with `maxValue` as the
	///     cap, which clamps it DOWN to maxValue. A non-positive maxValue therefore stores
	///     nothing (spike is a no-op under a non-positive cap), and an out-of-range /
	///     non-positive map dimension rejects every entry, leaving an empty field.
	/// Repeated entries for the same Position cannot occur (cells is a std::map), so the
	/// result is fully deterministic.
	void setOccupancyState(int lastTurn, const OccupancyField::CellMap& cells,
		int mapSizeX, int mapSizeY, int mapSizeZ, int maxValue = 1000)
	{
		occupancyLastAdvancedTurn = (lastTurn < -1) ? -1 : lastTurn;
		occupancy.clear();
		for (const auto& kv : cells)
		{
			const int value = kv.second;
			if (value <= 0)
				continue;                   // only strictly-positive spikes are replayed
			const Position& pos = kv.first;
			if (pos.x < 0 || pos.x >= mapSizeX ||
				pos.y < 0 || pos.y >= mapSizeY ||
				pos.z < 0 || pos.z >= mapSizeZ)
				continue;                   // off-map serialized cell dropped
			// After the clear, spike clamps (existing=0)+value down to maxValue, so the
			// stored value is exactly min(value, maxValue) for maxValue > 0; for a
			// non-positive maxValue spike is a no-op and nothing is stored.
			occupancy.spike(pos, value, maxValue);
		}
	}
};

} // namespace OpenXcom
