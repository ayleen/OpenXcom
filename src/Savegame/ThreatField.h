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
#include <map>

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
 *   - stampMax(Position, value) keeps the maximum danger already stamped at the
 *     tile, starting from a baseline of 0. A non-positive stamp does NOT create
 *     a cell (a zero/negative threat is "no threat" and is represented by
 *     absence).
 *   - threatAt(Position) returns 0 when the tile is absent from the map.
 *   - clear() wipes the map, empty()/size() report its state.
 *
 * Contrast with FriendReachableField (43.1D), which aggregates per-unit integer
 * contributions: this field is a single max-keyed accumulator for the owning
 * faction and has no per-unit decomposition.
 */
class ThreatField
{
public:
	using Field = std::map<Position, float, PositionComparator>;

	/// Keep the maximum danger stamped at a tile (baseline 0). A non-positive
	/// stamp does not create a cell -- only a value strictly greater than the
	/// current stored value (or 0 for an absent tile) is recorded.
	void stampMax(const Position& pos, float value)
	{
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

	/// Maximum danger at a tile (0 when the tile has never been stamped with a
	/// positive value).
	float threatAt(const Position& pos) const
	{
		auto it = field.find(pos);
		return it == field.end() ? 0.0f : it->second;
	}

	/// Wipe all stamped danger immediately.
	void clear()
	{
		field.clear();
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

private:
	Field field;
};

} // namespace OpenXcom
