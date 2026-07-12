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
 * Cheap, deterministic rank for a Phase 43.1 escape movement candidate.
 *
 * This is a dependency-free POD: only built-in types plus Position, no engine/
 * SDL includes beyond Position.h, so it can be unit-tested in isolation (see
 * AIEscapeCandidateOrderTest.cpp). Lower rank sorts before higher rank. The
 * ordering is, in priority order:
 *   1. threat              -- lower threat (safer tile) sorts first;
 *   2. aggro target        -- only compared when an aggro target exists; a
 *                            candidate carrying an aggro target sorts before
 *                            one without, and among two that both carry one the
 *                            candidate farther from the target (larger
 *                            distanceSqFromTarget) sorts first (escape-aligned);
 *   3. distanceSqFromActor -- smaller squared movement distance sorts first;
 *   4. position            -- x, then y, then z total deterministic tie-break.
 *
 * Target absence is represented explicitly by `hasAggroTarget` so callers never
 * feed a sentinel into `distanceSqFromTarget` arithmetic: when the flag is false
 * the comparator ignores `distanceSqFromTarget` entirely. Squared distance is
 * sufficient because it shares distance's ordering without a floating-point
 * sqrt. Position is the final total deterministic tie-break: any two
 * candidates at distinct tiles compare strictly (exactly one is less than the
 * other), while field-identical ranks -- same position included -- are
 * equivalent. Position is the final deterministic tie-break for candidates at
 * distinct tiles; duplicate candidates at the same tile may remain equivalent,
 * as required by a strict weak ordering.
 */
struct AIEscapeCandidateRank
{
	float threat;
	bool hasAggroTarget;
	int distanceSqFromTarget;   // meaningful only when hasAggroTarget == true
	int distanceSqFromActor;
	Position position;
};

/**
 * Strict weak ordering for AIEscapeCandidateRank, suitable for the cheap
 * setupEscape top-K selection. Usable with std::sort / std::priority_queue /
 * ordered containers. `operator()(a, a)` is always false (irreflexive); it is
 * asymmetric and transitive, and two ranks are equivalent (neither less than
 * the other) exactly when every field compares equal. Position is the
 * deterministic total tie-break for distinct positions: any two candidates at
 * distinct tiles compare strictly, so distinct candidate tiles are never
 * equivalent.
 *
 * The aggro-target level is gated on `hasAggroTarget`: when it differs the
 * target-known candidate sorts first; when both are true the candidate with the
 * larger `distanceSqFromTarget` (farther from the fled target) sorts first; when
 * both are false `distanceSqFromTarget` is never read, so a stale/sentinel value
 * cannot perturb the order.
 */
struct AIEscapeCandidateRankLess
{
	bool operator()(const AIEscapeCandidateRank& lhs, const AIEscapeCandidateRank& rhs) const
	{
		if (lhs.threat != rhs.threat)
			return lhs.threat < rhs.threat;                                  // lower threat first
		if (lhs.hasAggroTarget != rhs.hasAggroTarget)
			return lhs.hasAggroTarget;                                        // target-known candidate first
		if (lhs.hasAggroTarget)                                               // both carry a target
		{
			if (lhs.distanceSqFromTarget != rhs.distanceSqFromTarget)
				return lhs.distanceSqFromTarget > rhs.distanceSqFromTarget;   // farther from target first (escape-aligned)
		}
		if (lhs.distanceSqFromActor != rhs.distanceSqFromActor)
			return lhs.distanceSqFromActor < rhs.distanceSqFromActor;        // cheaper movement first
		if (lhs.position.x != rhs.position.x)
			return lhs.position.x < rhs.position.x;
		if (lhs.position.y != rhs.position.y)
			return lhs.position.y < rhs.position.y;
		return lhs.position.z < rhs.position.z;
	}
};

} // namespace OpenXcom
