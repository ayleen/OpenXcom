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
 * Phase 40 (Calypso) -- Drill Deck: 3-day combat-training cycle replacing
 * Soldier::trainPhys under Emscripten. See docs/phases/phase-40-drill-deck.md.
 */
#ifdef __EMSCRIPTEN__

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
