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
 * Lower threat sorts before higher threat. Callers map an unknown sparse-field
 * lookup to the field's zero baseline; this avoids starving never-evaluated
 * tiles behind an already-populated top-K. Squared distance is sufficient
 * because it has the same ordering as distance without introducing another
 * floating-point calculation. Position supplies a total deterministic tie-break.
 */
struct AICandidateRank
{
	float threat;
	int distanceSq;
	Position position;
};

struct AICandidateRankLess
{
	bool operator()(const AICandidateRank& lhs, const AICandidateRank& rhs) const
	{
		if (lhs.threat != rhs.threat)
			return lhs.threat < rhs.threat;
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
