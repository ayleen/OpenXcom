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

namespace OpenXcom
{

/**
 * Phase 43.1 (Calypso): deterministic, dependency-free directional-hit occupancy clue.
 *
 * Given the tile a unit was just hit on (`victimPos`) and the tile the shot came
 * from (`attackerPos`), produce a single tile on the ray from victim toward
 * attacker that is at most `maxSteps` Chebyshev steps away from the victim. This
 * is a coarse direction hint for occupancy-style reasoning; it is NOT a live
 * attacker-position read.
 *
 * Contract (deliberately narrow):
 *   - NEVER reveals the exact hidden attacker tile, even when the attacker is within
 *     range (Chebyshev span <= maxSteps). The returned tile always stops STRICTLY
 *     short of the attacker: when span <= maxSteps it lands at Chebyshev distance
 *     span - 1 from the victim (span - 1 >= 0), and when span == 1 (an adjacent
 *     attacker) it degrades to the victim tile itself (no usable direction / no clue).
 *   - NEVER reveals the exact attacker tile when the attacker is farther than
 *     maxSteps either. The returned tile lies strictly on the near side: its Chebyshev
 *     distance from the victim is exactly `min(maxSteps, span)` (so <= maxSteps),
 *     and when span > maxSteps it is strictly less than `span`, hence != attacker.
 *   - Pure and deterministic: identical inputs always yield identical output on
 *     every machine. No clock, no RNG, no map state, no allocations. The only
 *     dependency is Position (which the caller already has).
 *
 * Algorithm:
 *   dx/dy/dz = attacker - victim; span = max(|dx|,|dy|,|dz|) (Chebyshev distance).
 *   If maxSteps <= 0 or span == 0, return victim unchanged (no usable direction).
 *   steps = min(maxSteps, span); when steps == span (the attacker is within range)
 *   the clue would otherwise land exactly on the attacker, so step back by one to
 *   steps = span - 1 (an adjacent attacker yields steps == 0 -> the victim tile).
 *   Return victim + delta * steps / span per axis.
 *
 * Truncation is C integer truncation, which (a) is deterministic, and (b) lands on
 * a tile that is on the straight Chebyshev ray toward the attacker. The dominant
 * axis component always equals ±steps exactly, so the returned tile's Chebyshev
 * distance from the victim is exactly `steps` (the bound is tight).
 *
 * Arithmetic policy: Position components are 16-bit, so the delta*steps product is
 * evaluated in `long long` to avoid any signed-overflow undefined behavior even for
 * pathologically large coordinates. The final cast back to int (and then to the
 * Position component type) is safe for any real map extent.
 *
 * IMPORTANT -- caller responsibilities:
 *   - The caller performs map-bounds validation. This helper does not know the map
 *     size and never clamps to the grid; a returned tile may be off-map if the ray
 *     walks past an edge, and the caller must reject/clamp it before use.
 *   - This is a DIRECTION HINT, not live-position knowledge. Feeding it to logic as
 *     "the attacker is at tile X" leaks more than the clue is meant to convey. The
 *     intended consumer treats it as one noisy occupancy observation along a ray.
 *
 * This slice IS integrated: ProjectileFlyBState::hit() calls it (behind the
 * ai.sharedFields gate, via spikeFactionOccupancy) to leave a bounded directional
 * occupancy clue for the victim's faction when a unit is hit. The projection is
 * the same pure, dependency-free helper described above; only its consumer changed.
 *
 * @param victimPos    Tile the hit was taken on.
 * @param attackerPos  Tile the shot originated from.
 * @param maxSteps     Chebyshev-step cap on how far the clue may reach from the
 *                     victim. Non-positive => return victim (no clue). Default 8.
 * @return A deterministic tile on the victim->attacker ray, at most maxSteps
 *         Chebyshev steps from victim; NEVER the exact attacker tile, even when
 *         the attacker is within range (it stops strictly short, see the Contract).
 */
inline Position projectDirectionalHitClue(
	const Position& victimPos,
	const Position& attackerPos,
	int maxSteps = 8)
{
	// Deltas in wide arithmetic so delta*steps cannot overflow.
	const long long dx = static_cast<long long>(attackerPos.x) - static_cast<long long>(victimPos.x);
	const long long dy = static_cast<long long>(attackerPos.y) - static_cast<long long>(victimPos.y);
	const long long dz = static_cast<long long>(attackerPos.z) - static_cast<long long>(victimPos.z);

	const long long ax = dx < 0 ? -dx : dx;
	const long long ay = dy < 0 ? -dy : dy;
	const long long az = dz < 0 ? -dz : dz;

	// span == Chebyshev distance from victim to attacker.
	long long span = ax;
	if (ay > span) span = ay;
	if (az > span) span = az;

	// No usable direction (coincident tiles, or caller disabled the clue).
	if (maxSteps <= 0 || span == 0)
		return victimPos;

	// steps = min(maxSteps, span): clamp the reach so far targets never reveal
	// the exact attacker when the attacker is beyond maxSteps.
	long long steps = static_cast<long long>(maxSteps);
	if (steps > span) steps = span;
	// Stop STRICTLY short of the attacker. When the attacker is within range
	// (steps == span) the clue would otherwise land exactly on the attacker tile;
	// step back by one so it never reveals the hidden attacker. An adjacent
	// attacker (span == 1) yields steps == 0 -> the victim tile (no clue).
	if (steps >= span)
		steps = span - 1;
	if (steps <= 0)
		return victimPos;

	// victim + delta * steps / span, all in long long. The dominant axis yields
	// exactly ±steps, so the result is exactly `steps` Chebyshev steps away.
	const long long rx = static_cast<long long>(victimPos.x) + dx * steps / span;
	const long long ry = static_cast<long long>(victimPos.y) + dy * steps / span;
	const long long rz = static_cast<long long>(victimPos.z) + dz * steps / span;

	return Position(static_cast<int>(rx), static_cast<int>(ry), static_cast<int>(rz));
}

} // namespace OpenXcom
