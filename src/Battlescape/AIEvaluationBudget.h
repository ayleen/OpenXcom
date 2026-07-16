#pragma once
#include <limits>
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

namespace OpenXcom
{

/**
 * Phase 43.1 (Calypso): deterministic-first work budget for AI candidate evaluation.
 *
 * This is a pure, dependency-free accounting primitive for the 43.1 eval budget. It is created
 * for a single alien activation (or a single eval loop) and tracks how many candidate evaluations
 * have run against a deterministic *count* cap, plus an optional non-deterministic *wall-clock*
 * backstop that can only be checked from outside (the caller supplies elapsed milliseconds).
 *
 * Design constraints (per the 43.1 plan):
 *   - Deterministic reproducibility is owned by the COUNT budget alone (ai.evalBudget). The plan is
 *     explicit that the shipped Calypso config sizes evalBudget so the wall-clock backstop effectively
 *     never fires, and any determinism testing runs with turnBudgetMs = 0.
 *   - The wall-clock budget (ai.turnBudgetMs) is an EMERGENCY DEGRADATION ONLY: it cannot preempt an
 *     in-flight FOV/LOF/pathfinding call (granularity is one candidate), and by design it sacrifices
 *     determinism. It is therefore purely a "should we stop before the NEXT candidate" signal, never a
 *     hard interrupt. It is OFF by default (0 / negative input).
 *   - This primitive reads NO clock. Elapsed time is always supplied externally by the caller, so the
 *     same inputs give the same decision on every machine. No <chrono>, no time functions.
 *
 * This slice does NOT integrate into AIModule / brutalThink; it only exposes the accounting surface so
 * later slices can wire it in behind the ai.sharedFields gate.
 */
class AIEvaluationBudget
{
public:
	/// Build a budget. Non-positive `evalBudget` => count unbounded (limit disabled); non-positive
	/// `turnBudgetMs` => wall-clock backstop OFF. Both are clamped to 0 (the "off/unbounded" sentinel).
	AIEvaluationBudget(int evalBudget = 0, int turnBudgetMs = 0)
	{
		reset(evalBudget, turnBudgetMs);
	}

	/// (Re)configure the budget and clear the evaluation counter. Same clamping as the constructor.
	void reset(int evalBudget = 0, int turnBudgetMs = 0)
	{
		_evalBudget = evalBudget > 0 ? evalBudget : 0;       // 0 == unbounded
		_turnBudgetMs = turnBudgetMs > 0 ? turnBudgetMs : 0; // 0 == backstop off
		_evaluationsUsed = 0;
	}

	/// How many evaluations have been consumed so far this budget window.
	int evaluationsUsed() const { return _evaluationsUsed; }

	/// True when a deterministic count cap is active (ai.evalBudget > 0).
	bool isEvaluationLimitEnabled() const { return _evalBudget > 0; }

	/// True when the non-deterministic wall-clock backstop is armed (ai.turnBudgetMs > 0).
	bool isTimeLimitEnabled() const { return _turnBudgetMs > 0; }

	/// True if another evaluation may run under the COUNT budget. A disabled count limit is always
	/// passable; an enabled one passes only while used < cap. Does NOT consult the time backstop
	/// (that is checked separately by isTimeExpired / shouldStopBeforeNext).
	bool canEvaluate() const
	{
		return !isEvaluationLimitEnabled() || _evaluationsUsed < _evalBudget;
	}

	/// Consume one evaluation slot. Returns false (WITHOUT incrementing) when the count is exhausted;
	/// otherwise increments and returns true. Time is intentionally not considered here.
	bool consumeEvaluation()
	{
		if (!canEvaluate())
			return false;
		// The unlimited mode can, in principle, outlive any configured count cap. Saturate the
		// diagnostic counter instead of invoking signed-overflow undefined behavior.
		if (_evaluationsUsed < std::numeric_limits<int>::max())
			++_evaluationsUsed;
		return true;
	}

	/// Wall-clock expiry under the externally-supplied `elapsedMs`. Returns false when the backstop is
	/// OFF (turnBudgetMs <= 0). Negative `elapsedMs` is treated as "no time elapsed" (never expired).
	/// Exact boundary: elapsedMs >= turnBudgetMs (a positive cap) is expired.
	bool isTimeExpired(int elapsedMs) const
	{
		if (!isTimeLimitEnabled())
			return false;
		if (elapsedMs < 0)
			return false;
		return elapsedMs >= _turnBudgetMs;
	}

	/// Combined stop signal checked before starting the NEXT candidate: true when the count is
	/// exhausted OR the wall-clock backstop has expired. This is the single gate later slices call
	/// between candidates (it cannot interrupt an in-flight evaluation).
	bool shouldStopBeforeNext(int elapsedMs) const
	{
		return !canEvaluate() || isTimeExpired(elapsedMs);
	}

private:
	int _evalBudget = 0;       // deterministic count cap; 0 == unbounded
	int _turnBudgetMs = 0;     // emergency wall-clock backstop (ms); 0 == off
	int _evaluationsUsed = 0;  // monotonic consumption counter for this window
};

} // namespace OpenXcom
