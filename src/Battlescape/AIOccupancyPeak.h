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

#include "Position.h"
#include "../Savegame/OccupancyField.h"

namespace OpenXcom
{

/**
 * Phase 43.1 (Calypso): deterministic occupancy-peak selector.
 *
 * Given a read-only OccupancyField and an actor position, pick the single
 * "peak" tile -- the cell carrying the strongest occupancy -- under a fully
 * deterministic total tie-break. This is the pure read-side counterpart to the
 * OccupancyField accumulator (phase 43.1 occupancy slice) and is intended as
 * the foundation a later slice can wire behind the AI gates for "move toward
 * the densest known enemy area" reasoning.
 *
 * Contract (deliberately narrow):
 *   - READ-ONLY. The field is taken by const reference and is never mutated;
 *     `getCells()` is iterated, nothing is spiked, decayed, or erased.
 *   - Returns false and leaves `out` UNCHANGED when the field is empty or
 *     carries no positive cell. (OccupancyField's non-zero-only invariant means
 *     every stored cell is already positive, so the non-positive branch is a
 *     defensive no-op that keeps the contract honest regardless of how the
 *     field was populated.)
 *   - Otherwise selects the cell with the HIGHEST occupancy value; on ties the
 *     cell whose squared 3D distance to the actor is SMALLER wins; on a final
 *     distance tie the cell that sorts FIRST in PositionComparator order wins
 *     (x, then y, then z ascending). Three levels make the result a unique,
 *     reproducible peak for any input.
 *   - Pure and deterministic: identical inputs always yield identical output on
 *     every machine. No clock, no RNG, no map state, no heap allocation beyond
 *     what iterating the field's std::map already does. The only dependencies
 *     are Position and OccupancyField (which itself depends only on Position +
 *     the STL), so it builds and unit-tests without the engine.
 *   - Determinism is INDEPENDENT of insertion order: OccupancyField stores its
 *     cells in a std::map<Position, int, PositionComparator>, which iterates in
 *     a fixed PositionComparator order regardless of the order in which spikes
 *     were applied, and the selection walks that single stable order with a
 *     strict comparison -- so spiking the same set of cells in any order
 *     produces the same peak.
 *
 * Arithmetic policy: distance uses Position::distanceSq (x*x + y*y + z*z), the
 * same integer metric the rest of the AI rankers use. Map tiles are small
 * non-negative coordinates in practice, so the squares stay inside the int
 * range exactly like every other Position::distanceSq call site in the engine.
 *
 * IMPORTANT -- caller responsibilities:
 *   - The caller still owns producer-side map-bounds validation (see the
 *     INTEGRATION PRECONDITION in OccupancyField.h). This selector trusts the
 *     field's contents and performs no bounds check of its own; it is a pure
 *     consumer.
 *   - This is a FOUNDATION slice. It does NOT integrate into AIModule /
 *     brutalThink, does NOT change any AI decision, and pulls in no Mod/SDL.
 *
 * @param field     Read-only occupancy field to scan.
 * @param actorPos  Reference tile for the distance tie-break (typically the
 *                  unit whose turn is being decided). The peak itself never
 *                  depends on whether actorPos is on the map.
 * @param out       [out] Receives the selected peak tile on success.
 * @return true when a peak was selected (field had >= 1 positive cell); false
 *         when the field was empty / had no positive cell, in which case `out`
 *         is left exactly as the caller passed it.
 */
inline bool selectOccupancyPeak(
	const OccupancyField& field,
	const Position& actorPos,
	Position& out)
{
	const OccupancyField::CellMap& cells = field.getCells();

	bool found = false;
	int bestValue = 0;
	int bestDistSq = 0;
	Position bestPos;            // default (0,0,0); overwritten before any read
	PositionComparator order;    // final deterministic tie-break

	for (const auto& kv : cells)            // PositionComparator-ascending snapshot
	{
		const int value = kv.second;
		if (value <= 0)                     // defensive: invariant keeps only >0 cells
			continue;

		const Position& pos = kv.first;
		const int distSq = Position::distanceSq(pos, actorPos);

		bool replace = false;
		if (!found)
		{
			replace = true;                 // first positive cell seeds the best
		}
		else if (value != bestValue)
		{
			replace = value > bestValue;    // 1. higher occupancy wins
		}
		else if (distSq != bestDistSq)
		{
			replace = distSq < bestDistSq;  // 2. closer to actor wins
		}
		else
		{
			replace = order(pos, bestPos);  // 3. first in PositionComparator order wins
		}

		if (replace)
		{
			found = true;
			bestValue = value;
			bestDistSq = distSq;
			bestPos = pos;
		}
	}

	if (!found)
		return false;                       // empty / no positive cell: leave out untouched

	out = bestPos;
	return true;
}

} // namespace OpenXcom
