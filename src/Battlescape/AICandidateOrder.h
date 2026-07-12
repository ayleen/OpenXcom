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

#include "Position.h"

namespace OpenXcom
{

/**
 * Cheap, deterministic rank for a Phase 43.1 movement candidate.
 *
 * Precedence (in priority order):
 *   1. threat    -- lower threat (safer tile) sorts first;
 *   2. occupancy -- lower occupancy (less-crowded tile) sorts first;
 *   3. distanceSq -- smaller squared distance sorts first;
 *   4. position  -- x, then y, then z total deterministic tie-break.
 *
 * Callers map an unknown sparse-field lookup to the field's zero baseline;
 * this avoids starving never-evaluated tiles behind an already-populated
 * top-K. Squared distance is sufficient because it has the same ordering as
 * distance without introducing another floating-point calculation.
 *
 * `occupancy` is a Phase 43.1 comparator-contract extension: the field and the
 * comparator level exist so that future occupancy consumers can plug a real
 * value in, but until then every production initializer passes the placeholder
 * 0, so the ordering is unchanged relative to the threat/distance/position
 * behavior. Position supplies a total deterministic tie-break.
 */
struct AICandidateRank
{
	float threat;
	int occupancy;
	int distanceSq;
	Position position;
};

/**
 * Strict weak ordering for AICandidateRank, suitable for the cheap
 * setupUnitLogic/top-K movement selection. Usable with std::sort /
 * std::priority_queue / ordered containers. `operator()(a, a)` is always false
 * (irreflexive); it is asymmetric and transitive, and two ranks are equivalent
 * (neither less than the other) exactly when every field compares equal.
 * Position is the deterministic total tie-break for distinct positions: any two
 * candidates at distinct tiles compare strictly, so distinct candidate tiles
 * are never equivalent.
 */
struct AICandidateRankLess
{
	bool operator()(const AICandidateRank& lhs, const AICandidateRank& rhs) const
	{
		if (lhs.threat != rhs.threat)
			return lhs.threat < rhs.threat;        // lower threat first
		if (lhs.occupancy != rhs.occupancy)
			return lhs.occupancy < rhs.occupancy;  // lower occupancy first
		if (lhs.distanceSq != rhs.distanceSq)
			return lhs.distanceSq < rhs.distanceSq;
		if (lhs.position.x != rhs.position.x)
			return lhs.position.x < rhs.position.x;
		if (lhs.position.y != rhs.position.y)
			return lhs.position.y < rhs.position.y;
		return lhs.position.z < rhs.position.z;
	}
};

} // namespace OpenXcom
