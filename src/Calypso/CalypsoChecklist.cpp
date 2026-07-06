#ifdef __EMSCRIPTEN__
#include "CalypsoChecklist.h"
#include "CalypsoTutorial.h"
#include <emscripten.h>
#include "../Engine/Game.h"
#include "../Engine/Logger.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Savegame/ItemContainer.h"

namespace OpenXcom
{

namespace CalypsoChecklist
{

bool isDone(const Game* game, const CalypsoChecklistItem& item)
{
	if (!game) return false;
	const SavedGame* save = game->getSavedGame();
	if (!save) return false;
	if (item.check == "researched")
	{
		return save->isResearched(item.checkArg);
	}
	else if (item.check == "facilityBuilt")
	{
		for (Base* base : *save->getBases())
			for (BaseFacility* fac : *base->getFacilities())
				if (fac->getRules()->getType() == item.checkArg && fac->getBuildTime() == 0)
					return true;
		return false;
	}
	else if (item.check == "itemInStores")
	{
		for (Base* base : *save->getBases())
			if (base->getStorageItems()->getItem(item.checkArg) > 0)
				return true;
		return false;
	}
	else if (item.check == "stepShown")
	{
		return CalypsoTutorial::get().wasShown(item.checkArg);
	}
	return false; // unknown check (already warned at parse time)
}

bool isVisible(const Game* game, const CalypsoChecklistItem& item)
{
	(void)game;
	return item.afterStep.empty() || CalypsoTutorial::get().wasShown(item.afterStep);
}

} // namespace CalypsoChecklist

} // namespace OpenXcom

/* ---- Debug export: clones the calypso_tutorial_dump style. ---- */
extern "C" {

EMSCRIPTEN_KEEPALIVE
void calypso_checklist_dump()
{
	OpenXcom::Game* g = OpenXcom::getCurrentGame();
	if (!g) { OpenXcom::Log(OpenXcom::LOG_INFO) << "[checklist] no game"; return; }
	const auto& items = g->getMod()->getCalypsoChecklist();
	OpenXcom::Log(OpenXcom::LOG_INFO) << "[checklist] " << items.size() << " items";
	for (const auto& it : items)
	{
		OpenXcom::Log(OpenXcom::LOG_INFO) << "[checklist] id='" << it.id
			<< "' visible=" << (OpenXcom::CalypsoChecklist::isVisible(g, it) ? "1" : "0")
			<< " done=" << (OpenXcom::CalypsoChecklist::isDone(g, it) ? "1" : "0");
	}
}

} /* extern "C" */

#endif // __EMSCRIPTEN__
