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
 * base-lifecycle family: site selection / merged transaction / first-base
 * naming / base defense / partial destruction review.
 *
 * Whole file Emscripten-only.
 */
#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <string>

#include "../Engine/Game.h"
#include "../Engine/Screen.h"
#include "../Geoscape/BaseDefenseState.h"
#include "../Geoscape/BaseDestroyedState.h"
#include "../Geoscape/BaseNameState.h"
#include "../Geoscape/BuildNewBaseState.h"
#include "../Geoscape/ConfirmNewBaseState.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Geoscape/Globe.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleAlienMission.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Mod/RuleInterface.h"
#include "../Mod/RuleRegion.h"
#include "../Mod/RuleUfo.h"
#include "../Mod/UfoTrajectory.h"
#include "../Savegame/AlienMission.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/Region.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Ufo.h"

#include "CalypsoHdHarnessHostState.h"
#include "CalypsoUiMetrics.h"
#include "../Menu/ErrorMessageState.h"

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

/// Deterministic attacking UFO for the defense/destruction fixtures: the
/// first mod rule whose missile power is 0 (so BaseDefenseState keeps its
/// Start/Skip buttons and nothing auto-runs), carrying a live mission so the
/// OK route stays dereference-safe.
Ufo* calypsoF21HarnessUfo()
{
	Game* g = getCurrentGame();
	if (!g || !g->getMod()) return nullptr;
	RuleUfo* rule = nullptr;
	for (const std::string& name : g->getMod()->getUfosList())
	{
		RuleUfo* candidate = g->getMod()->getUfo(name, false);
		if (candidate && candidate->getMissilePower() == 0)
		{
			rule = candidate;
			break;
		}
	}
	if (!rule) return nullptr;
	Ufo* ufo = new Ufo(rule, 1);
	const RuleAlienMission* missionRule = nullptr;
	for (const std::string& name : g->getMod()->getAlienMissionList())
	{
		missionRule = g->getMod()->getAlienMission(name, false);
		if (missionRule) break;
	}
	if (missionRule)
	{
		ufo->setMissionInfo(new AlienMission(*missionRule),
			g->getMod()->getUfoTrajectory(UfoTrajectory::RETALIATION_ASSAULT_RUN, true));
	}
	return ufo;
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
	case CalypsoHarnessScenario::F21Defense:
	{
		const CalypsoF21Fixture f = calypsoF21HarnessFixture();
		if (!f.base) return nullptr;
		f.base->setName("ATLANTIS POST");
		// A live (non-pushed) GeoscapeState keeps the OK route safe to click:
		// with an ungarrisoned fixture base it routes into the undefended-base
		// destruction popup instead of a battle.
		GeoscapeState* geoscape = new GeoscapeState();
		Ufo* ufo = calypsoF21HarnessUfo();
		if (!ufo) { delete geoscape; return nullptr; }
		ufo->setLongitude(f.base->getLongitude());
		ufo->setLatitude(f.base->getLatitude());
		return new BaseDefenseState(f.base, ufo, geoscape, false);
	}
	case CalypsoHarnessScenario::F21Destruction:
	{
		const CalypsoF21Fixture f = calypsoF21HarnessFixture();
		if (!f.base) return nullptr;
		f.base->setName("ATLANTIS POST");
		Game* g = getCurrentGame();
		if (!g || !g->getMod()) return nullptr;
		Ufo* ufo = calypsoF21HarnessUfo();
		if (!ufo) return nullptr;
		ufo->setLongitude(f.base->getLongitude());
		ufo->setLatitude(f.base->getLatitude());
		// Missile-damage variant: populate the destroyed-facilities cache the
		// same way Base::damageFacilities would, so the review list is non-empty.
		auto* cache = f.base->getDestroyedFacilitiesCache();
		int added = 0;
		for (const std::string& name : g->getMod()->getBaseFacilitiesList())
		{
			RuleBaseFacility* rule = g->getMod()->getBaseFacility(name, false);
			if (!rule || rule->isLift()) continue;
			(*cache)[rule] += 1;
			if (++added >= 3) break;
		}
		return new BaseDestroyedState(f.base, ufo, true, true);
	}
	case CalypsoHarnessScenario::F21SiteError:
	{
		Game* g = getCurrentGame();
		if (!g || !g->getMod() || !g->getLanguage() || !g->getScreen()) return nullptr;
		const auto* geoscape = g->getMod()->getInterface("geoscape");
		if (!geoscape) return nullptr;
		const auto* genericWindow = geoscape->getElement("genericWindow");
		const auto* palette = geoscape->getElement("palette");
		if (!genericWindow || !palette) return nullptr;
		return new ErrorMessageState(
			g->getLanguage()->getString("STR_XCOM_BASE_CANNOT_BE_BUILT"),
			g->getScreen()->getPalette(), genericWindow->color, "BACK01.SCR",
			palette->color, 0, nullptr,
			ErrorMessageHdForm{
				g->getLanguage()->getString("STR_CAL_F34_PROTOCOL_ERROR"),
				g->getLanguage()->getString("STR_CAL_ERROR_OPERATIONAL_WARNING"),
				{
					g->getLanguage()->getString("STR_CAL_NEW_BASE_LAND_ERROR_LINE_1"),
					g->getLanguage()->getString("STR_CAL_NEW_BASE_LAND_ERROR_LINE_2")
				},
				g->getLanguage()->getString("STR_CAL_ACKNOWLEDGE")
			});
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

extern "C" {

EMSCRIPTEN_KEEPALIVE
void calypso_hd_harness_f21_defense()
{
	OpenXcom::Calypso::calypsoHdHarnessOpen(
		OpenXcom::Calypso::CalypsoHarnessScenario::F21Defense,
		OpenXcom::Calypso::CalypsoLayoutClass::Wide);
}

EMSCRIPTEN_KEEPALIVE
void calypso_hd_harness_f21_destruction()
{
	OpenXcom::Calypso::calypsoHdHarnessOpen(
		OpenXcom::Calypso::CalypsoHarnessScenario::F21Destruction,
		OpenXcom::Calypso::CalypsoLayoutClass::Wide);
}

} // extern "C"

#endif // __EMSCRIPTEN__
