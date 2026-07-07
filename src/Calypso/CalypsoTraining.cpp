#ifdef __EMSCRIPTEN__
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 40 (Calypso) -- Drill Deck daily tick. Formula lives in
 * CalypsoTrainingMath.h (pure, doctest-covered); this file only adapts
 * Soldier/RNG to it.
 */
#include "CalypsoTraining.h"
#include "CalypsoTrainingMath.h"
#include "../Engine/RNG.h"
#include "../Mod/RuleSoldier.h"
#include "../Savegame/Soldier.h"

namespace OpenXcom
{
namespace Calypso
{

void drillDeckDailyTick(Soldier* soldier)
{
	// no P.T. for the wounded (parity with Soldier::trainPhys)
	if (!soldier->hasFullHealth())
		return;

	int days = soldier->getCalypsoDrillDays() + 1;
	if (days < DRILL_CYCLE_DAYS)
	{
		soldier->setCalypsoDrillDays(days);
		return;
	}
	soldier->setCalypsoDrillDays(0);

	UnitStats* cur = soldier->getCurrentStatsEditable();
	UnitStats caps = soldier->getRules()->getTrainingStatCaps();
	int current[DRILL_STAT_COUNT] = { cur->tu, cur->stamina, cur->health, cur->strength, cur->firing, cur->throwing, cur->melee };
	int capArr[DRILL_STAT_COUNT] = { caps.tu, caps.stamina, caps.health, caps.strength, caps.firing, caps.throwing, caps.melee };

	int pick = drillPickWeakest(current, capArr);
	if (pick < 0)
		return; // fully trained; GeoscapeState handles the popup

	int lo = 0, hi = 0;
	drillGainBounds(pick, current[pick], capArr[pick], &lo, &hi);
	int next = drillApply(current[pick], capArr[pick], RNG::generate(lo, hi));
	switch (pick)
	{
		case DRILL_TU:       cur->tu = next; break;
		case DRILL_STAMINA:  cur->stamina = next; break;
		case DRILL_HEALTH:   cur->health = next; break;
		case DRILL_STRENGTH: cur->strength = next; break;
		case DRILL_FIRING:   cur->firing = next; break;
		case DRILL_THROWING: cur->throwing = next; break;
		case DRILL_MELEE:    cur->melee = next; break;
		default:            break; // unreachable — drillPickWeakest returns [0..DRILL_STAT_COUNT-1]
	}
}

}
}
#endif
