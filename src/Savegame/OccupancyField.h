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
#include "../Battlescape/Position.h"
#include <algorithm>
#include <cstddef>
#include <map>

namespace OpenXcom
{

/**
 * Phase 43.1 (Calypso): occupancy field foundation.
 *
 * A pure, dependency-free, transient sparse field of integer occupancy weights
 * keyed by tile Position, backing the planned `occupancy` shared field (phase
 * 43.1 occupancy slice). Only NON-ZERO cells are ever stored, so the map is both
 * the value store and the "occupied set". This slice ONLY stores, spikes, and
 * decays/spreads integer occupancy; it does NOT read unit/terrain state, does
 * NOT wire any producer/consumer, does NOT change any AI decision, does NOT own
 * any cache, and is intentionally NOT serialized here (serialization/ownership
 * is a later slice). It pulls in no YAML, no Mod, no SDL -- just Position and
 * the STL -- so it builds and unit-tests without the engine.
 *
 * ----------------------------------------------------------------------
 * INTEGRATION PRECONDITION -- read before wiring any producer/consumer.
 * spike() does NOT bounds-check `pos`: it takes no map dimensions and will
 * stamp a value at an off-map or otherwise illegal tile. PRODUCERS (callers
 * of spike) MUST validate every Position against the live map extent BEFORE
 * spiking, so CONSUMERS (code that iterates getCells()) never observe an
 * out-of-range cell. decayAndSpread defends itself by dropping off-map
 * sources, but that is a safety net, NOT a licence to feed it dirty data.
 * ----------------------------------------------------------------------
 *
 * Semantics (deterministic, platform-independent integer math):
 *   - spike(pos, amount, maxValue): STRICTLY MONOTONIC. A non-positive `amount`
 *     OR a non-positive `maxValue` is a NO-OP (no cell is created, lowered, or
 *     erased). Otherwise `amount` is ADDED to the current value in wide
 *     (long long) arithmetic and clamped DOWN to `maxValue`, then FLOORED at
 *     the EXISTING value -- so a positive spike can NEVER lower a cell, even
 *     when a caller passes a `maxValue` smaller than the value already stored
 *     there (a tightened cap never retroactively shrinks an existing cell). The
 *     stored result is therefore always >= max(1, existing) (amount > 0 and
 *     maxValue > 0), so the non-zero-only invariant holds with no erase path.
 *   - decayAndSpread(mapSizeX, mapSizeY, mapSizeZ, retainPercent, spreadPercent,
 *     maxValue): one mass-conserving (modulo the retain drop) diffusion pass
 *     over a SNAPSHOT of the current cells. Both percentages are clamped to
 *     [0,100] before any arithmetic.
 *       * Invalid / non-positive map dimensions are a SAFE CLEAR: a field under
 *         a nonsensical map is wiped (and the function returns) rather than left
 *         carrying stale out-of-range data. This is the chosen, documented
 *         invalid-bounds policy and is unit-tested.
 *       * A non-positive `maxValue` is likewise a SAFE CLEAR: it would clamp
 *         every diffused value to <= 0 (and thus store nothing), so the field is
 *         explicitly wiped up front and the diffusion loop is skipped. This is
 *         unit-tested for both zero and negative caps.
 *       * For each in-bounds source cell (a cell that itself falls outside the
 *         current mapSize is dropped -- it is off-map and cannot legally hold
 *         occupancy): retained = value*retainPercent/100; spread =
 *         retained*spreadPercent/100; the center keeps (retained - spread), or
 *         all of `retained` when it has zero in-bounds neighbours; `spread` is
 *         divided across the valid in-bounds 8-neighbours on the SAME z --
 *         quotient = spread/count equally to every neighbour, the integer
 *         remainder = spread%count handed one-by-one to the FIRST neighbours in
 *         PositionComparator order (so the split is total and reproducible).
 *       * Overlapping contributions from several sources are accumulated in a
 *         wide (long long) temporary per target cell; each final value is then
 *         clamped DOWN to `maxValue`, and zero results are NOT stored (so they
 *         effectively erase the cell).
 *       * The source map is read once (the snapshot) while every write targets
 *         the separate temporary, which guarantees NO double-decay: a cell that
 *         receives spread in this pass never re-spreads the boosted amount in
 *         the same pass, and the output never contains an off-map cell.
 *
 * Contrast with ThreatField (43.1F, a float max-accumulator that never lowers a
 * cell) and FriendReachableField (43.1D, a per-unit integer sum): this field
 * keeps a single plain integer per occupied tile that spikes additively and
 * decays by exact integer diffusion, so it both grows and shrinks.
 */
class OccupancyField
{
public:
	using CellMap = std::map<Position, int, PositionComparator>;

	/// Strictly monotonic add: adds `amount` to the cell at `pos` in wide
	/// (long long) arithmetic and clamps DOWN to `maxValue`, but FLOORS the
	/// result at the EXISTING value so a positive spike can NEVER lower a cell
	/// (a tightened cap does not retroactively shrink a stored value). A
	/// non-positive `amount` OR `maxValue` is a NO-OP (no create / lower /
	/// erase). Does NOT bounds-check `pos` -- see the INTEGRATION PRECONDITION
	/// in the class doc.
	void spike(const Position& pos, int amount, int maxValue)
	{
		// No-op on any non-positive input: a bad amount cannot create/zero a
		// cell and a bad cap cannot lower or erase an existing one.
		if (amount <= 0 || maxValue <= 0)
			return;
		const int existing = valueAt(pos);
		long long candidate = static_cast<long long>(existing) + amount;
		if (candidate > maxValue)
			candidate = maxValue;
		// Monotonicity: never let a tightened cap pull the stored value DOWN.
		if (candidate < existing)
			candidate = existing;
		// candidate >= max(1, existing) here (amount > 0, maxValue > 0), so the
		// non-zero-only invariant holds with no erase path.
		cells[pos] = static_cast<int>(candidate);
	}

	/// One decay-and-spread pass over a snapshot of the current cells. See the
	/// class header for the exact integer-diffusion contract and the
	/// invalid-dims clear policy.
	void decayAndSpread(int mapSizeX, int mapSizeY, int mapSizeZ,
		int retainPercent, int spreadPercent, int maxValue)
	{
		if (mapSizeX <= 0 || mapSizeY <= 0 || mapSizeZ <= 0)
		{
			// Invalid map: wipe rather than carry stale out-of-range occupancy.
			cells.clear();
			return;
		}
		if (maxValue <= 0)
		{
			// A non-positive cap clamps every diffused value to <= 0 (so nothing
			// would be stored); clear up front so the contract is obvious and the
			// diffusion loop never runs against a nonsensical cap.
			cells.clear();
			return;
		}
		const int retain = std::min(100, std::max(0, retainPercent));
		const int spreadPct = std::min(100, std::max(0, spreadPercent));

		// Wide accumulator per target cell: overlapping sources may sum here
		// before the single clamp-to-maxValue pass at the end.
		std::map<Position, long long, PositionComparator> next;

		for (const auto& kv : cells)            // snapshot: read-only over `cells`
		{
			const Position pos = kv.first;
			const int value = kv.second;
			if (value <= 0)
				continue;                        // non-zero-only invariant; defensive
			if (pos.x < 0 || pos.x >= mapSizeX ||
				pos.y < 0 || pos.y >= mapSizeY ||
				pos.z < 0 || pos.z >= mapSizeZ)
				continue;                        // off-map source dropped

			// Multiply in long long to stay overflow-free and platform-independent;
			// retained <= value and spread <= retained, so both fit back in int.
			const int retained = static_cast<int>(static_cast<long long>(value) * retain / 100);
			const int spread = static_cast<int>(static_cast<long long>(retained) * spreadPct / 100);

			// Gather the valid in-bounds 8-neighbours on the same z, then sort
			// them with PositionComparator so the remainder split is deterministic.
			Position neighbours[8];
			int count = 0;
			for (int dy = -1; dy <= 1; ++dy)
			{
				for (int dx = -1; dx <= 1; ++dx)
				{
					if (dx == 0 && dy == 0)
						continue;
					const Position n(pos.x + dx, pos.y + dy, pos.z);
					// z == pos.z is already in [0, mapSizeZ) because pos is in-bounds.
					if (n.x >= 0 && n.x < mapSizeX && n.y >= 0 && n.y < mapSizeY)
						neighbours[count++] = n;
				}
			}
			std::sort(neighbours, neighbours + count, PositionComparator());

			if (count == 0)
			{
				// Nobody to spread to: the center keeps all of `retained`.
				next[pos] += retained;
			}
			else
			{
				next[pos] += retained - spread;  // spread >= 0 and spread <= retained
				const int quotient = spread / count;
				const int remainder = spread % count;
				for (int i = 0; i < count; ++i)
					next[neighbours[i]] += quotient + (i < remainder ? 1 : 0);
			}
		}

		// Clamp each accumulated cell DOWN to maxValue and drop zeros.
		CellMap result;
		for (const auto& kv : next)
		{
			long long v = kv.second;
			if (v > maxValue)
				v = maxValue;
			if (v > 0)
				result[kv.first] = static_cast<int>(v);
		}
		cells.swap(result);
	}

	/// Wipe every stored cell immediately.
	void clear()
	{
		cells.clear();
	}

	/// True when no cell currently carries a (non-zero) occupancy value.
	bool empty() const
	{
		return cells.empty();
	}

	/// Number of cells currently carrying a (non-zero) occupancy value.
	std::size_t size() const
	{
		return cells.size();
	}

	/// Occupancy value at `pos` (0 when the cell is absent -- the non-zero-only
	/// invariant means a 0 return is exactly "unoccupied", not "unknown").
	int valueAt(const Position& pos) const
	{
		auto it = cells.find(pos);
		return it == cells.end() ? 0 : it->second;
	}

	/// Read-only sparse view of every occupied cell for producer/consumer passes
	/// that need to iterate the whole field.
	const CellMap& getCells() const
	{
		return cells;
	}

private:
	CellMap cells;
};

} // namespace OpenXcom
