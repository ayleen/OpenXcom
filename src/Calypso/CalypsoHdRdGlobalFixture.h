#pragma once

/*
 * RR3-002 global F14 fixtures. These profiles are harness-only and return
 * unregistered native bases. HostState owns insertion, teardown and options.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdRdFixture.h"

#include <array>
#include <memory>
#include <string>

#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleManufacture.h"
#include "../Mod/RuleResearch.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/Production.h"
#include "../Savegame/ResearchDiary.h"
#include "../Savegame/SavedGame.h"

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoHdRdGlobalProfile : int
{
	GlobalProduction = 200,
	EmptyStaffedBase = 201,
	BasescapeReturn = 202,
	GlobalNavigation = 203,
	GlobalTotals = 204,
	DiaryStableSort = 205,
	DiarySearchRetention = 206,
	DiaryTooltip = 207,
	/// Same staffed global-production fixture entered from Basescape. The
	/// constructor origin is intentionally a separate native profile so the
	/// return destination is exercised, not inferred from a JS label.
	BasescapeGlobalProduction = 208,
};

inline bool calypsoHdRdGlobalProfileValid(int profileId)
{
	return profileId >= 200 && profileId <= 208;
}

inline BaseFacility *calypsoHdRdGlobalFacility(Base *base, Mod *mod,
	const char *name)
{
	if (!base || !mod || !name) return nullptr;
	const RuleBaseFacility *rule = mod->getBaseFacility(name, false);
	if (!rule) return nullptr;
	auto *facility = new BaseFacility(rule, base);
	facility->setBuildTime(0);
	base->getFacilities()->push_back(facility);
	return facility;
}

inline Production *calypsoHdRdGlobalProduction(Base *base,
	RuleManufacture *rule, int engineers, int spent, int amount)
{
	if (!base || !rule) return nullptr;
	auto *production = new Production(rule, amount);
	production->setAssignedEngineers(engineers);
	production->setTimeSpent(spent);
	base->addProduction(production);
	return production;
}

inline Base *calypsoHdRdGlobalBase(Mod *mod, const char *name, int engineers,
	bool workshop)
{
	auto *base = new Base(mod);
	base->setName(name);
	base->setEngineers(engineers);
	if (workshop && !calypsoHdRdGlobalFacility(base, mod, "STR_CALYPSO_RD_WORKSHOP"))
	{
		delete base;
		return nullptr;
	}
	return base;
}

inline bool calypsoHdRdGlobalDiary(Game *game, int profileId)
{
	if (!game || !game->getMod() || !game->getSavedGame()) return false;
	Mod *mod = game->getMod();
	const char *names[] = {
		"STR_CALYPSO_RD_DIARY_ALPHA", "STR_CALYPSO_RD_DIARY_BETA",
		"STR_CALYPSO_RD_DIARY_UNICODE", "STR_CALYPSO_RD_DIARY_LONG",
		"STR_CALYPSO_RD_DIARY_LONG_2", "STR_CALYPSO_RD_DIARY_LONG_3",
		"STR_CALYPSO_RD_DIARY_LONG_4", "STR_CALYPSO_RD_DIARY_LONG_5"};
	const unsigned dates[][3] = {
		{2040, 9, 28}, {2040, 9, 28}, {2040, 8, 14}, {2040, 7, 21},
		{2040, 7, 20}, {2040, 7, 19}, {2040, 7, 18}, {2040, 7, 17}};
	if (profileId == 205)
	{
		names[2] = "STR_DECANTED_CORPSE";
		names[3] = "STR_MAELSTROM_BATTERY";
	}
	const int count = profileId == 206 ? 8 : 4;
	for (int i = 0; i < count; ++i)
	{
		RuleResearch *rule = mod->getResearch(names[i], false);
		if (!rule) return false;
		auto *entry = new ResearchDiaryEntry(rule);
		entry->year = dates[i][0];
		entry->month = dates[i][1];
		entry->day = dates[i][2];
		entry->source.type = BASE;
		entry->source.name = "Batumi";
		entry->source.research = nullptr;
		entry->source.event = nullptr;
		entry->source.mission = nullptr;
		game->getSavedGame()->addResearchDiaryEntry(entry);
	}
	return true;
}

/// Build one of the fixed global fixtures. Additional bases are returned in
/// RdAcceptanceFixture::additionalBases for central host registration.
inline RdAcceptanceFixture makeRdGlobalFixture(Game *game, int profileId)
{
	RdAcceptanceFixture fixture;
	if (!game || !game->getMod() || !game->getSavedGame()
		|| !calypsoHdRdGlobalProfileValid(profileId)) return fixture;
	Mod *mod = game->getMod();
	RuleManufacture *manufacture = mod->getManufacture("STR_MAELSTROM_TORPEDOES", false);
	// Profile 208 is the second real production origin, not a diary variant.
	// Keep the range explicit so the Basescape-origin fixture retains its
	// populated production rows while changing only the native origin flag.
	const bool diary = profileId >= 205 && profileId <= 207;
	if (!diary && !manufacture) return fixture;

	if (diary)
	{
		fixture.base = calypsoHdRdGlobalBase(mod, "Batumi", 0, false);
		if (!fixture.base || !calypsoHdRdGlobalDiary(game, profileId))
		{
			delete fixture.base;
			return RdAcceptanceFixture();
		}
		return fixture;
	}

	std::unique_ptr<Base> first(calypsoHdRdGlobalBase(mod, "Batumi", 2, true));
	if (!first) return RdAcceptanceFixture();
	if (!calypsoHdRdGlobalProduction(first.get(), manufacture, 94, 2640, 40))
		return RdAcceptanceFixture();
	std::unique_ptr<Base> second;
	if (profileId == 201)
	{
		second.reset(calypsoHdRdGlobalBase(mod, "Garni", 2, true));
	}
	else
	{
		second.reset(calypsoHdRdGlobalBase(mod, "Garni", 0, profileId == 204));
		if (second)
			calypsoHdRdGlobalProduction(second.get(), manufacture, 90, 2400, 20);
	}
	if (!second) return RdAcceptanceFixture();
	fixture.base = first.release();
	fixture.additionalBases.push_back(second.release());
	fixture.manufacture = manufacture;
	return fixture;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
