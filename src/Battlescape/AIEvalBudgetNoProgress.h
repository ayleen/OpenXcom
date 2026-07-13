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

namespace OpenXcom
{

/**
 * Phase 43.1 (Calypso): no-progress resolution for the brutalThink deterministic
 * evaluation budgets.
 *
 * When a deterministic per-unit count budget (ai.evalBudget > 0, gated on
 * ai.sharedFields) is TRUNCATED, brutalThink can finish with no committable
 * action: the candidate cascade left travelTarget == myPos (every evaluated
 * candidate scored zero -- e.g. the top-K ranked tiles were all dangerous or
 * uncovered) and there is no attack / turn / pickup to fall back to.
 *
 * Two count caps feed this decision: the Phase-1 target-enemy scan (top-K ranked
 * live enemies by AITargetRank) and the movement-candidate scan. Either being
 * truncated means work was skipped -- e.g. some live enemies were never fully
 * scored, or some candidate move tiles were never evaluated. Emitting the legacy
 * BA_RETHINK there is unsafe: BattlescapeGame re-dispatches brutalThink on the
 * SAME unit, which (nothing having changed) re-emits BA_RETHINK -- an AI-turn
 * soft-lock. PORT/Superhuman with sharedFields=1, evalBudget=8, turnBudgetMs=0
 * reproduced a >180s hang this way.
 *
 * This is a pure, header-only, dependency-free decision: given whether the unit
 * opened an attack this activation (checkedAttack) and whether a deterministic
 * count budget actually truncated (deterministicBudgetTruncated = Phase-1 target
 * OR movement count truncation), it returns true iff the activation must be
 * terminated (BA_NONE + number--) instead of emitting a no-progress BA_RETHINK.
 *
 * Byte-identical guarantee: returns false (== legacy BA_RETHINK) whenever NO
 * deterministic budget truncated -- i.e. always for ai.evalBudget == 0 /
 * ai.sharedFields off / budgets that comfortably covered every candidate. Only a
 * genuine truncation flips the decision, matching the 43.1 "emergency degradation
 * only" discipline. The non-deterministic wall-clock time backstop never feeds
 * this flag (reproducibility is owned by the count caps alone).
 */
inline bool aiEvalBudgetShouldEndActivation(bool checkedAttack, bool deterministicBudgetTruncated)
{
	// Legacy path: a unit that never opened an attack still gets BA_RETHINK so the
	// engine can re-think. Only when a deterministic count budget actually
	// truncated do we force a terminating end-activation rather than an endless
	// BA_RETHINK loop.
	if (!checkedAttack && !deterministicBudgetTruncated)
		return false;
	return true;
}

} // namespace OpenXcom
