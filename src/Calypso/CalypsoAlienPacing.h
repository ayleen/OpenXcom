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

static const int CALYPSO_ALIEN_PACING_ADDITIONAL_STATE_CAP = 3;
static const unsigned int CALYPSO_ALIEN_PACING_RAF_THRESHOLD_MS = 75;

/** Pure production-safe eligibility gate for Emscripten hostile-turn pacing. */
inline bool calypsoAlienPacingEligible(
	bool timerRunning,
	bool noPopups,
	bool hostileSide,
	int battleAlienSpeed,
	bool sharedFields,
	int evalBudget)
{
	return timerRunning
		&& noPopups
		&& hostileSide
		&& battleAlienSpeed == 1
		&& sharedFields
		&& evalBudget > 0;
}

/**
 * Fixed internal number of additional queued-state advances. It has no option,
 * URL or ruleset input: the feature gate and a non-empty queue are mandatory.
 */
inline int calypsoAlienPacingAdditionalStateCount(
	bool timerRunning,
	bool noPopups,
	bool hostileSide,
	int battleAlienSpeed,
	bool sharedFields,
	int evalBudget,
	bool busy)
{
	return busy && calypsoAlienPacingEligible(
		timerRunning, noPopups, hostileSide, battleAlienSpeed, sharedFields, evalBudget)
		? CALYPSO_ALIEN_PACING_ADDITIONAL_STATE_CAP : 0;
}

/**
 * Returns whether immediate scheduling may continue before the 75 ms RAF-request
 * threshold. Crossing it requests RAF; it does not guarantee a presentation at
 * that instant, and fast mode cannot resume until an actual render. Unsigned
 * subtraction intentionally handles SDL tick wraparound. This helper has no AI
 * inputs.
 */
inline bool calypsoAlienPacingBeforeRafThreshold(unsigned int nowMs, unsigned int lastRenderMs)
{
	return nowMs - lastRenderMs < CALYPSO_ALIEN_PACING_RAF_THRESHOLD_MS;
}

} // namespace OpenXcom
