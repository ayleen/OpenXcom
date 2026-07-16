/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <cstdint>
#include <limits>

namespace OpenXcom
{
namespace AITuning
{

constexpr int DEFAULT_HEARING_NOISE_BASE = 8;
constexpr int DEFAULT_HEARING_POWER_DIVISOR = 16;
constexpr int DEFAULT_SUPPRESSION_RADIUS = 1;
constexpr int DEFAULT_FOCUS_FIRE_COMMIT_THRESHOLD = 2;
constexpr int DEFAULT_FOCUS_FIRE_SCORE_PERCENT = 50;
constexpr int DEFAULT_BREACH_DETOUR_MULTIPLIER = 2;

inline int clampNonNegative(int value)
{
	return value < 0 ? 0 : value;
}

inline int clampAtLeastOne(int value)
{
	return value < 1 ? 1 : value;
}

inline int clampPercent(int value)
{
	if (value < 1)
		return 1;
	if (value > 100)
		return 100;
	return value;
}

// Phase 43.0: arithmetic shared by the ruleset-backed AI knobs. Rulesets and
// scripts can supply the full int range, so consumer formulas must not rely on
// a narrow, well-behaved value even after their semantic lower-bound clamps.
inline int saturatingInt(std::int64_t value)
{
	if (value > std::numeric_limits<int>::max())
		return std::numeric_limits<int>::max();
	if (value < std::numeric_limits<int>::min())
		return std::numeric_limits<int>::min();
	return static_cast<int>(value);
}

inline int hearingLoudness(int base, int ammoPower, int divisor)
{
	divisor = clampAtLeastOne(divisor);
	return saturatingInt(static_cast<std::int64_t>(base) + static_cast<std::int64_t>(ammoPower) / divisor);
}

inline int applyPercent(int value, int percent)
{
	return saturatingInt(static_cast<std::int64_t>(value) * percent / 100);
}

inline bool isBigDetour(int pathTUs, int straightLine, int multiplier)
{
	if (pathTUs < 0 || straightLine <= 0 || multiplier < 1)
		return false;
	// Every operand is non-negative and int-sized. The maximum product
	// (INT_MAX * INT_MAX * 4) fits in uint64_t, unlike signed int/int64_t.
	const std::uint64_t threshold = static_cast<std::uint64_t>(multiplier)
		* static_cast<std::uint64_t>(straightLine) * 4u;
	return static_cast<std::uint64_t>(pathTUs) > threshold;
}

}
}
