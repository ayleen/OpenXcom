#pragma once
// F06 personnel diary/memorial shared selection-list shell.
//
// Reuses the shared F04 selection-list shell plumbing (wide/compact switch,
// touch floors, off-screen parking for live-but-unpainted controls, native HD
// list seam, stable-id selection capture/restore, row-text composition) and
// binds it to the five F06 generated selection-list contracts. Rendering stays
// with the shared selection-list renderer, never here.
//
// Memorial stays a separate campaign-wide route sharing only the visual
// language and personnel navigation with the diary shell (contract §7.3);
// no memorial/diary semantic merging happens in this shell.
//
// Parked controls stay fully live input/behavior owners: only their blit is
// suppressed and only their hit area moves (visibility flags and handler
// bindings are never modified), so keyboard paths and native gates behave
// exactly as before while no vanilla pixel can show or intercept pointer.
#ifdef __EMSCRIPTEN__
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "../Engine/Game.h"
#include "../Engine/Surface.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Mod/Mod.h"
#include "../Savegame/Base.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Soldier.h"
#include "CalypsoF04SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF06SoldierMemorial.generated.h"
#include "Generated/CalypsoF06SoldierDiary.generated.h"
#include "Generated/CalypsoF06DiaryLight.generated.h"
#include "Generated/CalypsoF06DiaryMission.generated.h"
#include "Generated/CalypsoF06DiaryPerformance.generated.h"

namespace OpenXcom
{
namespace Calypso
{

inline bool f06HdWideLayout()
{
	return f04HdWideLayout();
}

/// Projects a generated design rect through the live window origin at the
/// current uiScale (F03 chooser projection, shared so paint and widget
/// placement cannot drift). Generic over the F06 generated layout types.
template <typename GeneratedLayout, typename GeneratedRect>
inline CalypsoLogicalRect f06ProjectRect(
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
inline CalypsoLogicalRect f06FindButtonRect(const ButtonRect* row, int count, const char* id)
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

inline const CalypsoF06SoldierMemorialGen::CalypsoF06SoldierMemorialGenLayout* f06MemorialLayout(bool wide)
{
	const auto* generated = CalypsoF06SoldierMemorialGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F06 memorial generated layout is missing");
	return generated;
}

inline const CalypsoF06SoldierDiaryGen::CalypsoF06SoldierDiaryGenLayout* f06DiaryLayout(bool wide)
{
	const auto* generated = CalypsoF06SoldierDiaryGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F06 diary generated layout is missing");
	return generated;
}

inline const CalypsoF06DiaryLightGen::CalypsoF06DiaryLightGenLayout* f06DiaryLightLayout(bool wide)
{
	const auto* generated = CalypsoF06DiaryLightGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F06 combat generated layout is missing");
	return generated;
}

inline const CalypsoF06DiaryMissionGen::CalypsoF06DiaryMissionGenLayout* f06DiaryMissionLayout(bool wide)
{
	const auto* generated = CalypsoF06DiaryMissionGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F06 mission generated layout is missing");
	return generated;
}

inline const CalypsoF06DiaryPerformanceGen::CalypsoF06DiaryPerformanceGenLayout* f06DiaryPerformanceLayout(bool wide)
{
	const auto* generated = CalypsoF06DiaryPerformanceGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F06 performance generated layout is missing");
	return generated;
}

/// Generated action-slot rect by button id (each shell owns exactly its
/// contract's action slot; adapters never invent geometry).
inline CalypsoLogicalRect f06MemorialButtonRect(bool wide, const char* id)
{
	return f06FindButtonRect(CalypsoF06SoldierMemorialGen::kButtonRects[wide ? 0 : 1],
		CalypsoF06SoldierMemorialGen::kButtonCount, id);
}

inline CalypsoLogicalRect f06DiaryButtonRect(bool wide, const char* id)
{
	return f06FindButtonRect(CalypsoF06SoldierDiaryGen::kButtonRects[wide ? 0 : 1],
		CalypsoF06SoldierDiaryGen::kButtonCount, id);
}

inline CalypsoLogicalRect f06DiaryLightButtonRect(bool wide, const char* id)
{
	return f06FindButtonRect(CalypsoF06DiaryLightGen::kButtonRects[wide ? 0 : 1],
		CalypsoF06DiaryLightGen::kButtonCount, id);
}

inline CalypsoLogicalRect f06DiaryMissionButtonRect(bool wide, const char* id)
{
	return f06FindButtonRect(CalypsoF06DiaryMissionGen::kButtonRects[wide ? 0 : 1],
		CalypsoF06DiaryMissionGen::kButtonCount, id);
}

inline CalypsoLogicalRect f06DiaryPerformanceButtonRect(bool wide, const char* id)
{
	return f06FindButtonRect(CalypsoF06DiaryPerformanceGen::kButtonRects[wide ? 0 : 1],
		CalypsoF06DiaryPerformanceGen::kButtonCount, id);
}

/// Configures the native HD selection-list seam AFTER enableUiScaling /
/// recapture, from the actual projected list rect and the generated metrics.
/// Same values re-applied preserve drag capture; real changes reset it.
/// Generic over the F06 generated layout types (same contract fields as the
/// F04 shell seam; kept here so F04 sources stay untouched).
template <typename GeneratedLayout>
inline void f06SeamSelectionList(
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

/// Push one verbatim native Text into the painted rows when it is visible and
/// non-empty. Conditional fields stay absent whenever native hides them
/// (never placeholder values).
inline void f06PushNativeTextRow(const Text* single, std::vector<CalypsoSelectionListRow>& out, bool enabled)
{
	if (!single || !single->getVisible()) return;
	const std::string text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		single->getText());
	if (text.empty()) return;
	CalypsoSelectionListRow entry{};
	entry.text = text;
	entry.enabled = enabled;
	out.push_back(entry);
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
