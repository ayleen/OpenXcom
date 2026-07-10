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
