#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 40 (Calypso) -- pure Drill Deck training math, dependency-free so
 * the native doctest suite can exercise the real formulas
 * (CalypsoTraining.cpp delegates here). No engine, YAML, or GL includes may
 * ever be added to this header.
 */

namespace OpenXcom
{
namespace Calypso
{

/// Drill-trainable stats, fixed order. The order is also the tie-break
/// priority in drillPickWeakest (lower index wins). Bravery, reactions and
/// Resonance (psi) are deliberately NOT drill-trainable: bravery is
/// combat-fear-driven, Resonance belongs to the Resonance Lab.
enum DrillStat
{
	DRILL_TU = 0,
	DRILL_STAMINA,
	DRILL_HEALTH,
	DRILL_STRENGTH,
	DRILL_FIRING,
	DRILL_THROWING,
	DRILL_MELEE,
	DRILL_STAT_COUNT
};

/// Index of the stat with the largest (cap - current) gap; ties resolve to
/// the lowest index; -1 when every stat is at/above its cap (fully trained).
inline int drillPickWeakest(const int current[DRILL_STAT_COUNT], const int caps[DRILL_STAT_COUNT])
{
	int best = -1;
	int bestGap = 0;
	for (int i = 0; i < DRILL_STAT_COUNT; ++i)
	{
		int gap = caps[i] - current[i];
		if (gap > bestGap)
		{
			bestGap = gap;
			best = i;
		}
	}
	return best;
}

/// RNG bounds for one drill improvement of stat `i` ("mission-sized"):
/// skill stats (firing/throwing/melee) use the busy-mission primary roll
/// (BattleUnit::improveStat with exp>10 -> RNG(2,6)); body stats use the
/// post-mission secondary formula RNG(0, gap/10 + 2), stamina gap/15 + 2.
inline void drillGainBounds(int i, int current, int cap, int* lo, int* hi)
{
	if (i == DRILL_FIRING || i == DRILL_THROWING || i == DRILL_MELEE)
	{
		*lo = 2;
		*hi = 6;
		return;
	}
	int gap = cap - current;
	if (gap < 0) gap = 0;
	*lo = 0;
	*hi = (i == DRILL_STAMINA) ? gap / 15 + 2 : gap / 10 + 2;
}

/// Apply a rolled gain: a completed 3-day cycle always grants at least +1,
/// clamped to the training cap.
inline int drillApply(int current, int cap, int roll)
{
	int gain = (roll < 1) ? 1 : roll;
	return (current + gain > cap) ? cap : current + gain;
}

}
}
