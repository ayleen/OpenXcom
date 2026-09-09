#pragma once
// F05 personnel appearance/transformation shared selection-list shell.
//
// Reuses the shared F04 selection-list shell plumbing (wide/compact switch,
// touch floors, off-screen parking for live-but-unpainted controls, native HD
// list seam, stable-id selection capture/restore, row-text composition) and
// binds it to the five F05 generated selection-list contracts. Rendering stays
// with the shared selection-list renderer, never here.
//
// Parked controls stay fully live input/behavior owners: only their blit is
// suppressed and only their hit area moves (visibility flags and handler
// bindings are never modified), so keyboard paths and native gates behave
// exactly as before while no vanilla pixel can show or intercept pointer.
#ifdef __EMSCRIPTEN__
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleSoldierTransformation.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Soldier.h"
#include "CalypsoF04SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "Generated/CalypsoF05SoldierArmor.generated.h"
#include "Generated/CalypsoF05SoldierAvatar.generated.h"
#include "Generated/CalypsoF05TransformSelect.generated.h"
#include "Generated/CalypsoF05TransformationList.generated.h"
#include "Generated/CalypsoF05TransformationReview.generated.h"

namespace OpenXcom
{
namespace Calypso
{

inline bool f05HdWideLayout()
{
	return f04HdWideLayout();
}

/// Projects a generated design rect through the live window origin at the
/// current uiScale (F03 chooser projection, shared so paint and widget
/// placement cannot drift). Generic over the F05 generated layout types.
template <typename GeneratedLayout, typename GeneratedRect>
inline CalypsoLogicalRect f05ProjectRect(
	const CalypsoLogicalRect& window,
	const GeneratedLayout& generated,
	const GeneratedRect& rect)
{
	const double uiScale = generated.window.w > 0
		? (double)window.w / (double)generated.window.w : 1.0;
	return {
		window.x + int((rect.x - generated.window.x) * uiScale),
		window.y + int((rect.y - generated.window.y) * uiScale),
		std::max(1, (int)std::llround(rect.w * uiScale)),
		std::max(1, (int)std::llround(rect.h * uiScale))};
}

template <typename ButtonRect>
inline CalypsoLogicalRect f05FindButtonRect(const ButtonRect* row, int count, const char* id)
{
	for (int i = 0; i < count; ++i)
	{
		if (id && row[i].id && std::string(row[i].id) == id)
		{
			const auto& rect = row[i].rect;
			return {rect.x, rect.y, rect.w, rect.h};
		}
	}
	return {};
}

inline const CalypsoF05SoldierArmorGen::CalypsoF05SoldierArmorGenLayout* f05ArmorLayout(bool wide)
{
	const auto* generated = CalypsoF05SoldierArmorGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F05 armor generated layout is missing");
	return generated;
}

inline const CalypsoF05SoldierAvatarGen::CalypsoF05SoldierAvatarGenLayout* f05AvatarLayout(bool wide)
{
	const auto* generated = CalypsoF05SoldierAvatarGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F05 avatar generated layout is missing");
	return generated;
}

inline const CalypsoF05TransformSelectGen::CalypsoF05TransformSelectGenLayout* f05TransformSelectLayout(bool wide)
{
	const auto* generated = CalypsoF05TransformSelectGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F05 transform generated layout is missing");
	return generated;
}

inline const CalypsoF05TransformationListGen::CalypsoF05TransformationListGenLayout* f05TransformationListLayout(bool wide)
{
	const auto* generated = CalypsoF05TransformationListGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F05 overview generated layout is missing");
	return generated;
}

inline const CalypsoF05TransformationReviewGen::CalypsoF05TransformationReviewGenLayout* f05TransformationReviewLayout(bool wide)
{
	const auto* generated = CalypsoF05TransformationReviewGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F05 review generated layout is missing");
	return generated;
}

/// Generated action-slot rect by button id (each shell owns exactly its
/// contract's action slot; adapters never invent geometry).
inline CalypsoLogicalRect f05ArmorButtonRect(bool wide, const char* id)
{
	return f05FindButtonRect(CalypsoF05SoldierArmorGen::kButtonRects[wide ? 0 : 1],
		CalypsoF05SoldierArmorGen::kButtonCount, id);
}

inline CalypsoLogicalRect f05AvatarButtonRect(bool wide, const char* id)
{
	return f05FindButtonRect(CalypsoF05SoldierAvatarGen::kButtonRects[wide ? 0 : 1],
		CalypsoF05SoldierAvatarGen::kButtonCount, id);
}

inline CalypsoLogicalRect f05TransformSelectButtonRect(bool wide, const char* id)
{
	return f05FindButtonRect(CalypsoF05TransformSelectGen::kButtonRects[wide ? 0 : 1],
		CalypsoF05TransformSelectGen::kButtonCount, id);
}

inline CalypsoLogicalRect f05TransformationListButtonRect(bool wide, const char* id)
{
	return f05FindButtonRect(CalypsoF05TransformationListGen::kButtonRects[wide ? 0 : 1],
		CalypsoF05TransformationListGen::kButtonCount, id);
}

inline CalypsoLogicalRect f05TransformationReviewButtonRect(bool wide, const char* id)
{
	return f05FindButtonRect(CalypsoF05TransformationReviewGen::kButtonRects[wide ? 0 : 1],
		CalypsoF05TransformationReviewGen::kButtonCount, id);
}

/// Configures the native HD selection-list seam AFTER enableUiScaling /
/// recapture, from the actual projected list rect and the generated metrics.
/// Same values re-applied preserve drag capture; real changes reset it.
/// Generic over the F05 generated layout types (same contract fields as the
/// F04 shell seam; kept here so F04 sources stay untouched).
template <typename GeneratedLayout>
inline void f05SeamSelectionList(
	TextList* list,
	Surface* window,
	const GeneratedLayout& generated)
{
	if (!list || !window || generated.window.w <= 0) return;
	const double uiScale = (double)window->getWidth() / (double)generated.window.w;
	const int scrollBarWidth = std::max(1, (int)std::llround(generated.scrollBarWidth * uiScale));
	const int minThumbHeight = std::max(1, (int)std::llround(generated.minThumbHeight * uiScale));
	const int rowStride = std::max(1, (int)std::llround(generated.rowHeight * uiScale));
	const size_t visibleRows = generated.visibleRows > 0 ? (size_t)generated.visibleRows : 0;
	list->configureCalypsoHdSelectionList(scrollBarWidth, minThumbHeight, rowStride, visibleRows);
}

/// Exact (uncapped) transformation capacity for the global overview. The
/// native overview deliberately caps its display-oriented calculation; the HD
/// rows disclose exact figures using the same engine sources (funds, base
/// storage, live/dead rosters) without mutating anything. No Calypso
/// transformation rules: external soldier types/projects flow through
/// untouched, identified by stable rule names.
///
/// projectsPossible is -1 when the project needs neither funds nor items
/// (unbounded: the HD row keeps the legacy "+" glyph, now truthful), and the
/// exact non-negative count otherwise.
struct CalypsoF05ExactCapacity
{
	int projectsPossible = 0;
	int eligibleSoldiers = 0;
};

inline CalypsoF05ExactCapacity f05ExactTransformationCapacity(
	const RuleSoldierTransformation* rule, Base* base, Game* game)
{
	CalypsoF05ExactCapacity out{};
	if (!rule || !base || !game || !game->getSavedGame() || !game->getMod())
		return out;
	int projectsPossible = INT_MAX;
	if (rule->getCost() > 0)
	{
		// Funds are 64-bit while the count stays int: saturate the
		// quotient at INT_MAX explicitly, then min two ints (never
		// overflow or narrow blindly).
		const std::int64_t affordable = game->getSavedGame()->getFunds() / rule->getCost();
		const int capped = affordable > INT_MAX ? INT_MAX : (int)affordable;
		projectsPossible = std::min(projectsPossible, capped);
	}
	ItemContainer* items = base->getStorageItems();
	for (const auto& required : rule->getRequiredItems())
	{
		const RuleItem* itemRule = game->getMod()->getItem(required.first);
		if (!itemRule || required.second <= 0)
		{
			projectsPossible = 0;
			break;
		}
		projectsPossible = std::min(projectsPossible, items->getItem(itemRule) / required.second);
	}
	out.projectsPossible = projectsPossible == INT_MAX ? -1 : std::max(0, projectsPossible);
	int eligible = 0;
	for (const Soldier* soldier : *base->getSoldiers())
	{
		if (!soldier)
			continue;
		if (soldier->getCraft() && soldier->getCraft()->getStatus() == "STR_OUT")
			continue;
		if (soldier->isEligibleForTransformation(rule))
			++eligible;
	}
	for (const Soldier* deadMan : *game->getSavedGame()->getDeadSoldiers())
	{
		if (deadMan && deadMan->isEligibleForTransformation(rule))
			++eligible;
	}
	out.eligibleSoldiers = eligible;
	return out;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
