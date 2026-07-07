#pragma once
#ifdef __EMSCRIPTEN__
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 40 (Calypso) -- Drill Deck: 3-day combat-training cycle replacing
 * Soldier::trainPhys under Emscripten. See docs/phases/phase-40-drill-deck.md.
 */

namespace OpenXcom
{

class Soldier;

namespace Calypso
{

/// Days in training needed to complete one drill cycle.
constexpr int DRILL_CYCLE_DAYS = 3;

/// Daily tick for one soldier assigned to Drill Deck training. Counts days;
/// every DRILL_CYCLE_DAYS-th healthy day improves the weakest trainable stat
/// by a mission-sized amount (CalypsoTrainingMath.h). The caller keeps the
/// vanilla isFullyTrained()/popup/dropout handling.
void drillDeckDailyTick(Soldier* soldier);

}
}
#endif
