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
/*
 * Phase 41 (Calypso) -- pure prologue-scene decision math, dependency-free so
 * the native doctest suite can exercise the real formulas
 * (CalypsoPrologueScene.cpp delegates here). No engine, YAML, or GL includes
 * may ever be added to this header.
 */
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <algorithm>

namespace OpenXcom
{
namespace Calypso
{

/// Position + identity of a live unit, as the scene sees it. `aboard` comes
/// from BattleUnit::isInExitArea(START_POINT); it must not be inferred from a
/// coarse craft bounding rectangle because only real deployment tiles count
/// for extraction.
struct UnitPos { int id; int x; int y; bool aboard = false; };

/// Inclusive tile rectangle (the craft's exit/deployment zone).
struct Rect { int x0; int y0; int x1; int y1; };

/// Scene-independent result of a vanilla finish attempt. The Battlescape
/// adapter maps these decisions to director outcomes and engine calls.
enum class UnexpectedFinishAction
{
	FallbackOutcome,
	ConsumeAbort,
	EnterEvacOnly,
	KeepEvacOnly,
	AllTakenOutcome
};

/// The scripted prologue owns its ending and therefore cannot coexist with
/// the vanilla battlescape turn timer. Zero and negative values mean disabled.
inline bool prologueTurnLimitIsSafe(int turnLimit)
{
	return turnLimit <= 0;
}

/// An abort confirmation must be consumed unless cast-off is currently
/// available and at least one live crew member occupies a real START_POINT.
inline bool consumeAbortRequest(bool inert, bool castOffAvailable, bool anyoneAboard)
{
	return inert || !castOffAvailable || !anyoneAboard;
}

/// Pure ordering contract for CalypsoPrologueScene::onUnexpectedFinish.
/// Fallback state outranks abort; an automatic zero-hostiles finish with live
/// crew transitions once into extraction-only; no live crew means all taken.
inline UnexpectedFinishAction decideUnexpectedFinish(bool inert, bool hasPendingOutcome,
	bool abort, bool anyCrewAlive, bool evacOnly)
{
	if (inert || hasPendingOutcome) return UnexpectedFinishAction::FallbackOutcome;
	if (abort) return UnexpectedFinishAction::ConsumeAbort;
	if (anyCrewAlive)
		return evacOnly ? UnexpectedFinishAction::KeepEvacOnly : UnexpectedFinishAction::EnterEvacOnly;
	return UnexpectedFinishAction::AllTakenOutcome;
}

/// Inclusive containment test.
inline bool inRect(int x, int y, const Rect& r)
{
	return x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1;
}

/// Chebyshev (max-axis) tile distance from (x,y) to the nearest tile of r; 0 when inside.
inline int chebyshevToRect(int x, int y, const Rect& r)
{
	int dx = std::max({ r.x0 - x, x - r.x1, 0 });
	int dy = std::max({ r.y0 - y, y - r.y1, 0 });
	return std::max(dx, dy);
}

/// Farthest-from-exitArea unit id, excluding aboard units and nikosId; lowest id breaks ties; -1 if none.
inline int pickGauntletVictim(const std::vector<UnitPos>& alive, const Rect& exitArea, int nikosId)
{
	int bestId = -1;
	int bestDist = -1;
	for (const auto& u : alive)
	{
		if (u.id == nikosId) continue;
		if (u.aboard) continue; // real START_POINT exit tile, never a victim
		int dist = chebyshevToRect(u.x, u.y, exitArea);
		if (dist > bestDist || (dist == bestDist && u.id < bestId))
		{
			bestDist = dist;
			bestId = u.id;
		}
	}
	return bestId;
}

/// Ambush predicate evaluated at Choir-turn start: assessor + at least one squaddie past triggerDist, or unconditional fallback turn.
inline bool ambushShouldFire(int assessorDist, const std::vector<int>& otherDists, int triggerDist, int turn, int fallbackTurn)
{
	if (turn >= fallbackTurn) return true;
	if (assessorDist < triggerDist) return false;
	for (int d : otherDists)
	{
		if (d >= triggerDist) return true;
	}
	return false;
}

/// Escalating radio-line stage for a stalling player turn: -1 before firstNagTurn or from fallbackTurn on, else 0-based stage clamped to 2.
inline int escalationStage(int turn, int firstNagTurn, int fallbackTurn)
{
	if (turn < firstNagTurn || turn >= fallbackTurn) return -1;
	int stage = turn - firstNagTurn;
	if (stage > 2) stage = 2;
	return stage;
}

} // namespace Calypso
} // namespace OpenXcom
