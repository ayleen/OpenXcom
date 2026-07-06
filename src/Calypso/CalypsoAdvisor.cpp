#ifdef __EMSCRIPTEN__
#include "CalypsoAdvisor.h"
#include "CalypsoTutorial.h"
#include <emscripten.h>
#include "../Engine/Game.h"
#include "../Engine/Logger.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Savegame/Craft.h"
#include "../Savegame/CraftWeapon.h"
#include "../Mod/RuleCraftWeapon.h"

namespace OpenXcom
{

CalypsoAdvisor& CalypsoAdvisor::get()
{
	static CalypsoAdvisor instance;
	return instance;
}

bool CalypsoAdvisor::evaluate(const Game* game, const CalypsoAdvisorRule& r) const
{
	const SavedGame* save = game->getSavedGame();
	if (!save) return false;
	if (r.check == "idleScientists")
	{
		for (Base* b : *save->getBases()) if (b->getScientists() > 0) return true;
		return false;
	}
	else if (r.check == "idleEngineers")
	{
		for (Base* b : *save->getBases()) if (b->getEngineers() > 0) return true;
		return false;
	}
	else if (r.check == "noFacility")
	{
		// Any base having the facility at ANY build stage satisfies intent.
		for (Base* b : *save->getBases())
			for (BaseFacility* f : *b->getFacilities())
				if (f->getRules()->getType() == r.checkArg) return false;
		return true;
	}
	else if (r.check == "craftWeaponEquipped")
	{
		for (Base* b : *save->getBases())
			for (Craft* c : *b->getCrafts())
				for (CraftWeapon* w : *c->getWeapons())
					if (w && w->getRules()->getType() == r.checkArg) return true;
		return false;
	}
	else if (r.check == "singleBase")
	{
		return save->getBases()->size() == 1;
	}
	else if (r.check == "facilityBuilt")
	{
		for (Base* b : *save->getBases())
			for (BaseFacility* f : *b->getFacilities())
				if (f->getRules()->getType() == r.checkArg && f->getBuildTime() == 0) return true;
		return false;
	}
	return false; // unknown check (warned at parse time)
}

void CalypsoAdvisor::daily(Game* game)
{
	// FIRST LINE: zero cost when disabled.
	if (!CalypsoTutorial::get().isActive(game)) return;
	const SavedGame* save = game->getSavedGame();
	if (!save) return;
	for (const auto& r : game->getMod()->getCalypsoAdvisors())
	{
		if (save->getMonthsPassed() < r.afterMonth) { _streak[r.id] = 0; continue; }
		if (!evaluate(game, r)) { _streak[r.id] = 0; continue; }
		if (++_streak[r.id] < r.graceDays + 1) continue;
		CalypsoTutorial::get().fire(game, "advisor", r.id);
		// fire() dedups via the step's shown-set entry; re-fires are no-ops.
	}
}

void CalypsoAdvisor::dump() const
{
	Game* g = getCurrentGame();
	if (!g) { Log(LOG_INFO) << "[advisor] no game"; return; }
	const SavedGame* save = g->getSavedGame();
	const auto& rules = g->getMod()->getCalypsoAdvisors();
	Log(LOG_INFO) << "[advisor] " << rules.size() << " rules, monthsPassed="
	              << (save ? save->getMonthsPassed() : -1);
	for (const auto& r : rules)
	{
		auto it = _streak.find(r.id);
		int streak = (it == _streak.end()) ? 0 : it->second;
		bool gate = save && save->getMonthsPassed() >= r.afterMonth;
		bool ev = evaluate(g, r);
		Log(LOG_INFO) << "[advisor] id='" << r.id << "' check='" << r.check
		              << "' monthGate=" << (gate ? "1" : "0")
		              << " eval=" << (ev ? "1" : "0")
		              << " streak=" << streak << "/" << (r.graceDays + 1);
	}
}

} // namespace OpenXcom

/* ---- Debug exports (clone the calypso_tutorial_* style). ---- */
extern "C" {

EMSCRIPTEN_KEEPALIVE
void calypso_advisor_tick()
{
	if (OpenXcom::Game* g = OpenXcom::getCurrentGame())
		OpenXcom::CalypsoAdvisor::get().daily(g);
}

EMSCRIPTEN_KEEPALIVE
void calypso_advisor_dump()
{
	OpenXcom::CalypsoAdvisor::get().dump();
}

} /* extern "C" */

#endif // __EMSCRIPTEN__
