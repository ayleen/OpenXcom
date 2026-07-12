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
 * Phase 43.1D (Calypso): per-unit reachability aggregation field.
 *
 * A pure, dependency-free accumulator for the `friendReachable` shared field
 * (phase 43.1). Each own unit stamps a Contribution -- a sparse map of tile
 * positions to integer reachability weights -- and the field maintains a
 * running aggregate sum across all units plus the per-unit slices needed to
 * later un-stamp exactly one unit's contribution when it moves, dies, or
 * changes faction.
 *
 * This slice ONLY accumulates and decomposes contributions; it does NOT build
 * any BFS, does NOT read unit/terrain state, and does NOT change any AI
 * decision. Everything here is transient and intentionally NOT serialized.
 *
 * Semantics (no overflow policy invented -- plain signed int arithmetic per
 * the 43.1 field table):
 *   - replaceContribution first un-stamps the unit's previous contribution,
 *     then stores the new one even if empty. Each stored value is added into
 *     the aggregate sum; an aggregate key whose result becomes zero is erased
 *     so sumAt() returns 0 cleanly for uncontested tiles.
 *   - removeContribution subtracts the unit's exact old values and erases any
 *     aggregate key that reaches zero.
 *   - maxAtExcluding scans the stored per-unit maps for a position, skips the
 *     excluded unit id, and returns the maximum per-unit value there (starting
 *     at 0, so a position contributed only by negatives yields 0 -- matching
 *     std::max(default-zero, value) semantics).
 */
class FriendReachableField
{
public:
	using Contribution = std::map<Position, int, PositionComparator>;

	/// Wipe all per-unit slices and the aggregate immediately.
	void clear()
	{
		byUnit.clear();
		aggregateSum.clear();
	}

	/// Replace (or newly insert) a unit's contribution. The previous
	/// contribution for the same id is un-stamped first; the new contribution
	/// is stored verbatim even when empty, so hasContribution() stays true for
	/// a unit that has moved to contribute nothing.
	void replaceContribution(int unitId, const Contribution& contribution)
	{
		// 1. Un-stamp the existing contribution for this unit if any.
		auto oldIt = byUnit.find(unitId);
		if (oldIt != byUnit.end())
		{
			subtractFromAggregate(oldIt->second);
			byUnit.erase(oldIt);
		}
		// 2. Store the new contribution (even if empty) and stamp it.
		byUnit[unitId] = contribution;
		addToAggregate(contribution);
	}

	/// Remove a unit's contribution entirely. Returns false (no-op) when the
	/// unit has no stored contribution; true after the exact old values are
	/// subtracted and any aggregate key that reached zero is erased.
	bool removeContribution(int unitId)
	{
		auto it = byUnit.find(unitId);
		if (it == byUnit.end())
			return false;
		subtractFromAggregate(it->second);
		byUnit.erase(it);
		return true;
	}

	/// True when a contribution is stored for this unit id (even an empty one).
	bool hasContribution(int unitId) const
	{
		return byUnit.find(unitId) != byUnit.end();
	}

	/// Number of units with a stored contribution.
	std::size_t unitCount() const { return byUnit.size(); }

	/// True when there are no stored contributions and no aggregate entries.
	bool empty() const
	{
		return byUnit.empty() && aggregateSum.empty();
	}

	/// Aggregate reachability weight at a tile (0 when no unit contributes).
	int sumAt(const Position& pos) const
	{
		auto it = aggregateSum.find(pos);
		return it == aggregateSum.end() ? 0 : it->second;
	}

	/// Read-only sparse aggregate view for consumers that need to iterate every
	/// contributed tile (for example the shared enemy-reachability producer).
	const Contribution& getAggregate() const { return aggregateSum; }

	/// Maximum per-unit reachability weight at a tile, ignoring excludeUnitId.
	/// Starts at 0 so a position contributed only by negative weights yields 0.
	int maxAtExcluding(const Position& pos, int excludeUnitId) const
	{
		int best = 0;
		for (const auto& kv : byUnit)
		{
			if (kv.first == excludeUnitId)
				continue;
			auto vit = kv.second.find(pos);
			if (vit != kv.second.end())
				best = std::max(best, vit->second);
		}
		return best;
	}

private:
	std::map<int, Contribution> byUnit;
	Contribution aggregateSum;

	void addToAggregate(const Contribution& contribution)
	{
		for (const auto& kv : contribution)
		{
			auto it = aggregateSum.find(kv.first);
			if (it == aggregateSum.end())
			{
				if (kv.second != 0)
					aggregateSum[kv.first] = kv.second;
			}
			else
			{
				it->second += kv.second;
				if (it->second == 0)
					aggregateSum.erase(it);
			}
		}
	}

	void subtractFromAggregate(const Contribution& contribution)
	{
		for (const auto& kv : contribution)
		{
			auto it = aggregateSum.find(kv.first);
			if (it == aggregateSum.end())
			{
				// A previously cancelled (to-zero) key was intentionally erased, so
				// it is absent even though it holds a balance of zero. Re-create the
				// negative residual only for a nonzero weight; keep zero absent.
				if (kv.second != 0)
					aggregateSum[kv.first] = -kv.second;
			}
			else
			{
				it->second -= kv.second;
				if (it->second == 0)
					aggregateSum.erase(it);
			}
		}
	}
};

} // namespace OpenXcom
