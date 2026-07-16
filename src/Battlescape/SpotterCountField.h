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
#include <cassert>
#include <cstddef>
#include <map>

namespace OpenXcom
{

/**
 * Phase 43.1T (Calypso): per-faction exact spotter-count memo foundation.
 *
 * A pure, dependency-free, transient memo for the `spotterCount` shared field
 * (phase 43.1). It stores a sparse map of tile positions to an EXACT, non-negative
 * count of units currently spotting (having line-of-sight to) that tile, so a
 * later sniper/evaluation pass can reuse the count without re-walking every
 * potential spotter's visibility.
 *
 * This slice ONLY stores and queries exact counts; it does NOT compute
 * visibility, does NOT read unit/terrain state, and does NOT change any AI
 * decision. Everything here is transient and intentionally NOT serialized.
 *
 * Semantics (evaluated-zero is a real, stored value, distinct from unknown):
 *   - storeExact(Position, count) records the exact non-negative count for a
 *     tile and marks it evaluated. count is a caller precondition: it must be
 *     >= 0 and is stored VERBATIM (no clamping) so the memo never invents a
 *     different number than the producer computed. Re-stamping the same tile
 *     overwrites the previous count in full.
 *   - countAt(Position) returns the stored count for an evaluated tile. For an
 *     unevaluated tile it returns 0 ONLY so the accessor is total (it always
 *     yields an int rather than signalling "unknown"). That 0 is an OPTIMISTIC
 *     placeholder for escape scoring: unknown-as-zero reads as "no spotters /
 *     nobody can see this tile", which is the dangerous side, not the safe
 *     side. It is NOT a claim of known-zero and is NOT decision-safe -- it
 *     MUST NOT be consumed for any scoring or decision without first checking
 *     isEvaluated(). Callers that need to distinguish a confirmed zero spotter
 *     count from a never-evaluated tile MUST consult isEvaluated() first.
 *     This mirrors the ThreatField (43.1F) contract.
 *   - isEvaluated(Position) is the only way to tell evaluated-zero apart from
 *     unknown; it is true iff storeExact has been called for that tile since
 *     the last clear().
 *   - clear() wipes every recorded count; empty()/size() report the number of
 *     tiles with a stored (evaluated) count, including known-zero tiles.
 *
 * Contrast with ThreatField (43.1F), which is a max-accumulator that never
 * lowers a cell and keeps positive-danger separate from the evaluated set:
 * this field keeps ONE exact integer per evaluated tile and overwrites freely,
 * because a fresh spotter sweep replaces the previous count entirely rather
 * than monotonically growing it.
 *
 * This is the standalone foundation only: it is NOT yet wired into AIModule or
 * FactionTurnCache. Integration (ownership, dirty lifecycle, producers) is a
 * later sub-phase.
 */
class SpotterCountField
{
public:
	using CountMap = std::map<Position, int, PositionComparator>;

	/// Record the exact non-negative spotter count for a tile, overwriting any
	/// previous value. Marks the tile evaluated. Precondition: count >= 0,
	/// enforced by a debug assertion; the value is stored VERBATIM (no
	/// clamping) so the memo is always byte-exact and a negative count is a
	/// producer bug, not a value the memo will repair.
	void storeExact(const Position& pos, int count)
	{
		assert(count >= 0);
		counts[pos] = count;
	}

	/// True iff storeExact has been called for this tile since the last clear().
	/// This is the ONLY way to tell an evaluated-zero count apart from an
	/// unevaluated (unknown) tile.
	bool isEvaluated(const Position& pos) const
	{
		return counts.find(pos) != counts.end();
	}

	/// Exact stored count for an evaluated tile. For an unevaluated tile,
	/// returns 0 only so the accessor is total. That 0 is an OPTIMISTIC
	/// placeholder for escape scoring (unknown-as-zero reads as "no
	/// spotters"), NOT a claim of known-zero and NOT decision-safe: it must
	/// not be consumed for a decision without first checking isEvaluated().
	int countAt(const Position& pos) const
	{
		auto it = counts.find(pos);
		return it == counts.end() ? 0 : it->second;
	}

	/// Wipe every recorded count immediately.
	void clear()
	{
		counts.clear();
	}

	/// True when no tile has a stored (evaluated) count.
	bool empty() const
	{
		return counts.empty();
	}

	/// Number of tiles with a stored (evaluated) count, including known-zero.
	std::size_t size() const
	{
		return counts.size();
	}

	/// Read-only sparse view of every evaluated tile and its exact count.
	/// Producers use this to iterate the memoized tiles after a sweep.
	const CountMap& getCounts() const
	{
		return counts;
	}

private:
	CountMap counts;
};

} // namespace OpenXcom
