#ifdef __EMSCRIPTEN__
#ifndef OPENXCOM_CALYPSOCHECKLIST_H
#define OPENXCOM_CALYPSOCHECKLIST_H

/*
 * Phase 39 (Calypso): tutorial task-checklist model. Whole file Emscripten-only.
 * Items are parsed onto Mod from the `tutorial: checklist:` ruleset block; the
 * done/visible state is recomputed from live game state (no new save fields).
 */

#include <string>

namespace OpenXcom
{

class Game;

struct CalypsoChecklistItem {
	std::string id;
	std::string label;      // extraStrings id
	std::string check;      // researched|facilityBuilt|itemInStores|stepShown
	std::string checkArg;
	std::string afterStep;  // "" = always visible
};

namespace CalypsoChecklist {
	/// True when the item's done-condition currently holds.
	bool isDone(const Game* game, const CalypsoChecklistItem& item);
	/// True when the item should appear in the panel at all.
	bool isVisible(const Game* game, const CalypsoChecklistItem& item);
}

} // namespace OpenXcom

#endif // OPENXCOM_CALYPSOCHECKLIST_H
#endif // __EMSCRIPTEN__
