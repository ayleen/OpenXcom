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
#include <cstddef>
#include <set>
#include <utility>

namespace OpenXcom
{

/**
 * Phase 43.1G (Calypso): per-faction negative-only terrain line-of-fire (LOF)
 * cache foundation.
 *
 * A pure, dependency-free, transient cache for the `terrainLof` negative set
 * (phase 43.1). It stores directed voxel-line endpoint pairs -- a from Position
 * and a to Position -- that are known to be blocked by terrain, so a later LOF
 * probe between the same directed endpoints can short-circuit without re-walking
 * the voxel ray. The cache is deliberately NEGATIVE-ONLY: it records only
 * confirmed "no line of fire" results. There is no positive/cleared entry and
 * deliberately no rememberClear / positive API -- the only writers are
 * rememberBlocked() (insert) and the lifecycle clears (beginTurn /
 * onTerrainChanged); absence from the set is simply "unknown", never "clear".
 *
 * This slice ONLY records and queries directed endpoint pairs; it does NOT
 * compute any voxel line, does NOT read terrain state, and does NOT change any
 * AI decision. Everything here is transient and intentionally NOT serialized.
 *
 * Directionality: the pair (from, to) is DIRECTED and asymmetry is NOT assumed.
 * rememberBlocked(A, B) records only the A->B ray; the reverse B->A ray is a
 * distinct entry that must be remembered separately. The comparator orders by
 * the from endpoint first, then the to endpoint, using the existing
 * PositionComparator for each so ordering is deterministic and independent of
 * std::pair's default lexicographic tie-break quirks across toolchains.
 *
 * Semantics:
 *   - rememberBlocked(from, to) inserts the directed pair; inserting the same
 *     directed pair again is a no-op (dedupe -- std::set stores each once).
 *   - isKnownBlocked(from, to) returns true iff that exact directed pair has
 *     been remembered; the reverse pair is NOT consulted.
 *   - clear() wipes every remembered pair; empty()/size() report its state.
 *
 * Contrast with ThreatField (43.1F) and FriendReachableField (43.1D), which
 * accumulate signed/positive values: this cache is a pure presence set of
 * directed blocked rays and carries no value payload.
 */
class TerrainLofNegativeCache
{
public:
	/// A directed voxel-line endpoint pair (origin -> target).
	using BlockedPair = std::pair<Position, Position>;

	/// Deterministic strict-weak ordering over directed pairs: order by the from
	/// endpoint using PositionComparator, then by the to endpoint. Asymmetry is
	/// preserved (from and to are NOT interchangeable); this only makes the set
	/// ordering deterministic and well-defined.
	struct DirectedPairComparator
	{
		bool operator()(const BlockedPair& a, const BlockedPair& b) const
		{
			PositionComparator by;
			if (by(a.first, b.first))
				return true;
			if (by(b.first, a.first))
				return false;
			// from endpoints are equal -> order by to endpoint.
			return by(a.second, b.second);
		}
	};

	using Cache = std::set<BlockedPair, DirectedPairComparator>;

	/// Remember that the directed ray from -> to is blocked by terrain. Repeating
	/// the same directed pair is a harmless no-op (dedupe at the set level).
	void rememberBlocked(const Position& from, const Position& to)
	{
		cache.insert(BlockedPair(from, to));
	}

	/// True iff the exact directed pair from -> to has been remembered as
	/// blocked. The reverse to -> from ray is NOT consulted (asymmetry).
	bool isKnownBlocked(const Position& from, const Position& to) const
	{
		return cache.find(BlockedPair(from, to)) != cache.end();
	}

	/// Wipe every remembered blocked pair immediately.
	void clear()
	{
		cache.clear();
	}

	/// True when no directed blocked pair is stored.
	bool empty() const
	{
		return cache.empty();
	}

	/// Number of directed blocked pairs currently stored.
	std::size_t size() const
	{
		return cache.size();
	}

private:
	Cache cache;
};

} // namespace OpenXcom
