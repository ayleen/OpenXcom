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
 * F21 (Calypso, Phase 46.F21): deterministic harness fixtures for the
 * new-base flow scenarios (site selection / merged transaction / first-base
 * naming). Defense and destruction scenarios follow in the next F21 slice
 * (they need Ufo/GeoscapeState fixtures).
 *
 * Whole file Emscripten-only.
 */
#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <string>

#include "../Engine/Game.h"
#include "../Engine/Screen.h"
#include "../Geoscape/BuildNewBaseState.h"
#include "../Geoscape/BaseNameState.h"
#include "../Geoscape/ConfirmNewBaseState.h"
#include "../Geoscape/Globe.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleRegion.h"
#include "../Savegame/Base.h"
#include "../Savegame/Region.h"
#include "../Savegame/SavedGame.h"

#include "CalypsoHdHarnessHostState.h"
#include "CalypsoUiMetrics.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

/// Deterministic fixture shared by the new-base scenarios: a saved game with
/// known funds (never mutating a live campaign -- the harness only ever runs
/// from the QA page over a fresh boot) and one provisional base.
struct CalypsoF21Fixture
{
	SavedGame* save = nullptr;
	Globe* globe = nullptr;
	Base* base = nullptr;
};

CalypsoF21Fixture calypsoF21HarnessFixture()
{
	CalypsoF21Fixture f;
	Game* g = getCurrentGame();
	if (!g || !g->getMod()) return f;

	f.save = g->getSavedGame();
	if (!f.save)
	{
		f.save = new SavedGame();
		g->setSavedGame(f.save);
	}
	f.save->setFunds(6800000); // representative fixture funds
	if (f.save->getRegions()->empty())
	{
		for (const std::string& regionName : g->getMod()->getRegionsList())
		{
			RuleRegion* rule = g->getMod()->getRegion(regionName, false);
			if (rule && !rule->getLonMin().empty())
				f.save->getRegions()->push_back(new Region(rule));
		}
	}

	const int sw = 320, sh = 200; // logical canvas the states position against
	f.globe = new Globe(g, (sw - 64) / 2, sh / 2, sw - 64, sh, 0, 0);
	f.base = new Base(g->getMod());
	f.base->setLongitude(0.0);
	f.base->setLatitude(0.5);
	return f;
}

} // namespace

State* calypsoF21HarnessCreateTarget(CalypsoHarnessScenario id)
{
	switch (id)
	{
	case CalypsoHarnessScenario::F21Site:
	{
		const CalypsoF21Fixture f = calypsoF21HarnessFixture();
		if (!f.base) return nullptr;
		// Additional-base variant: the Cancel action is visible.
		return new BuildNewBaseState(f.base, f.globe, false);
	}
	case CalypsoHarnessScenario::F21Transaction:
	{
		const CalypsoF21Fixture f = calypsoF21HarnessFixture();
		if (!f.base) return nullptr;
		return new ConfirmNewBaseState(f.base, f.globe);
	}
	case CalypsoHarnessScenario::F21Name:
	{
		const CalypsoF21Fixture f = calypsoF21HarnessFixture();
		if (!f.base) return nullptr;
		return new BaseNameState(f.base, f.globe, true, true);
	}
	default:
		return nullptr;
	}
}

} // namespace Calypso
} // namespace OpenXcom

extern "C" {

EMSCRIPTEN_KEEPALIVE
void calypso_hd_harness_f21_site()
{
	OpenXcom::Calypso::calypsoHdHarnessOpen(
		OpenXcom::Calypso::CalypsoHarnessScenario::F21Site,
		OpenXcom::Calypso::CalypsoLayoutClass::Wide);
}

EMSCRIPTEN_KEEPALIVE
void calypso_hd_harness_f21_transaction()
{
	OpenXcom::Calypso::calypsoHdHarnessOpen(
		OpenXcom::Calypso::CalypsoHarnessScenario::F21Transaction,
		OpenXcom::Calypso::CalypsoLayoutClass::Wide);
}

EMSCRIPTEN_KEEPALIVE
void calypso_hd_harness_f21_name()
{
	OpenXcom::Calypso::calypsoHdHarnessOpen(
		OpenXcom::Calypso::CalypsoHarnessScenario::F21Name,
		OpenXcom::Calypso::CalypsoLayoutClass::Wide);
}

} // extern "C"

#endif // __EMSCRIPTEN__
