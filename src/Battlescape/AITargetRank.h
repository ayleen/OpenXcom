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
 */

namespace OpenXcom
{

/**
 * Cheap, deterministic rank for an AI target candidate.
 *
 * This is a dependency-free POD: only built-in types, no engine/SDL includes,
 * so it can be unit-tested in isolation (see AITargetRankTest.cpp). Lower rank
 * sorts before higher rank. The ordering is, in priority order:
 *   1. engaged    -- targets already in combat (engaged == true) sort first;
 *   2. known      -- targets with fair-known position data sort first;
 *   3. distanceSq -- closer targets (smaller squared distance) sort first;
 *   4. unitId     -- final total deterministic tie-break (ascending).
 *
 * unitId is unique per battle unit, so the four-level comparison yields a strict
 * total order: no two distinct targets ever compare equivalent. Squared distance
 * is sufficient because it has the same ordering as distance without introducing
 * a floating-point calculation.
 */
struct AITargetRank
{
	bool engaged;
	bool known;
	int distanceSq;
	int unitId;
};

/**
 * Strict weak ordering (in fact strict total, given unitId uniqueness) for
 * AITargetRank. Usable with std::sort / std::priority_queue / ordered containers.
 * `operator()(a, a)` is always false (irreflexive); for distinct a, b exactly
 * one of `(*this)(a, b)` / `(*this)(b, a)` is true.
 */
struct AITargetRankLess
{
	bool operator()(const AITargetRank& lhs, const AITargetRank& rhs) const
	{
		if (lhs.engaged != rhs.engaged)
			return lhs.engaged;                      // engaged (true) first
		if (lhs.known != rhs.known)
			return lhs.known;                        // known (true) first
		if (lhs.distanceSq != rhs.distanceSq)
			return lhs.distanceSq < rhs.distanceSq;  // closer first
		return lhs.unitId < rhs.unitId;              // total deterministic tie-break
	}
};

} // namespace OpenXcom
