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
#include <set>

namespace OpenXcom
{

/**
 * Phase 43.1F (Calypso): per-faction threat accumulator foundation.
 *
 * A pure, dependency-free, transient accumulator for the `threat` shared field
 * (phase 43.1). It stores a sparse map of tile positions to a float danger
 * value and maintains the maximum danger stamped at each tile. The semantics
 * deliberately mirror the current brutalThink `discoverThreat = max` so that
 * conservative, incremental new/updated sightings can be layered in without
 * ever removing old danger: a later, smaller sighting never lowers an existing
 * cell, which matches the existing AI's "keep the worst known danger" behavior.
 *
 * This slice ONLY accumulates maximum danger; it does NOT build any field, does
 * NOT read unit/terrain state, does NOT apply any falloff/LOF/knowledge hooks,
 * and does NOT change any AI decision. Everything here is transient and
 * intentionally NOT serialized.
 *
 * Semantics:
 *   - stampMax(Position, value) marks the tile evaluated and keeps the maximum
 *     danger already stamped there, starting from a baseline of 0. A
 *     non-positive stamp does NOT create a danger cell, but remains
 *     distinguishable from a tile that has never been evaluated.
 *   - threatAt(Position) returns 0 when the tile is absent from the map.
 *   - clear() wipes danger and evaluation state; empty()/size() report only
 *     the sparse positive-danger map.
 *
 * Contrast with FriendReachableField (43.1D), which aggregates per-unit integer
 * contributions: this field is a single max-keyed accumulator for the owning
 * faction and has no per-unit decomposition.
 */
class ThreatField
{
public:
	using Field = std::map<Position, float, PositionComparator>;
	using Reachability = std::map<Position, int, PositionComparator>;
	using EvaluatedPositions = std::set<Position, PositionComparator>;

	/// Exact pure form of brutalThink's legacy discoverThreat loop. Distance is
	/// intentionally measured from the base candidate before applying footprint
	/// offsets; visibility is supplied by the caller so production can use the
	/// existing hasTileSight path and unit tests can exercise the math directly.
	template<typename VisibilityFn>
	static float calculateThreatAt(const Position& candidate, const Reachability& enemyReachable,
		int footprintSize, VisibilityFn visible)
	{
		float discoverThreat = 0.0f;
		for (const auto& reachable : enemyReachable)
		{
			for (int x = 0; x < footprintSize; ++x)
			{
				for (int y = 0; y < footprintSize; ++y)
				{
					Position compPos = candidate;
					const float currThreat = reachable.second / (Position::distance(reachable.first, compPos) + 1);
					compPos.x += x;
					compPos.y += y;
					if (currThreat > discoverThreat && visible(compPos, reachable.first))
						discoverThreat = currThreat;
				}
			}
		}
		return std::max(0.0f, discoverThreat);
	}

	/// Keep the maximum danger stamped at a tile (baseline 0). A non-positive
	/// stamp does not create a cell -- only a value strictly greater than the
	/// current stored value (or 0 for an absent tile) is recorded.
	void stampMax(const Position& pos, float value)
	{
		evaluated.insert(pos);
		auto it = field.find(pos);
		if (it == field.end())
		{
			if (value > 0.0f)
				field[pos] = value;
		}
		else if (value > it->second)
		{
			it->second = value;
		}
	}

	/// Mark a tile evaluated without assigning positive danger. Useful when a
	/// lazy producer has completed a probe whose exact result is zero.
	void markEvaluated(const Position& pos)
	{
		evaluated.insert(pos);
	}

	/// True after a producer explicitly evaluated or stamped this tile. False
	/// means unknown; it must never be interpreted as known-safe.
	bool isEvaluated(const Position& pos) const
	{
		return evaluated.find(pos) != evaluated.end();
	}

	/// Maximum danger at a tile (0 when the tile has never been stamped with a
	/// positive value).
	float threatAt(const Position& pos) const
	{
		auto it = field.find(pos);
		return it == field.end() ? 0.0f : it->second;
	}

	/// Conservative prefilter: only a completed strictly-positive evaluation can
	/// confirm danger. Unknown and evaluated-zero both return false so callers
	/// must run their original full visibility check.
	bool confirmsDangerAt(const Position& pos) const
	{
		return isEvaluated(pos) && threatAt(pos) > 0.0f;
	}

	/// Wipe all stamped danger immediately.
	void clear()
	{
		field.clear();
		evaluated.clear();
	}

	/// True when no tile carries any stamped (positive) danger.
	bool empty() const
	{
		return field.empty();
	}

	/// Number of tiles currently carrying stamped danger.
	std::size_t size() const
	{
		return field.size();
	}

	/// Number of tiles with a completed producer evaluation, including known-zero
	/// tiles that have no entry in the positive danger map.
	std::size_t evaluatedSize() const
	{
		return evaluated.size();
	}

	/// Stable ordered view of evaluated positions. Producers use this to
	/// conservatively re-stamp already memoized tiles after new knowledge.
	const EvaluatedPositions& getEvaluatedPositions() const
	{
		return evaluated;
	}

private:
	Field field;
	EvaluatedPositions evaluated;
};

} // namespace OpenXcom
