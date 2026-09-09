#pragma once
// F04 personnel shared selection-list shell helpers (pure layout/input-seam
// plumbing, natively includable; rendering stays with the shared
// selection-list renderer, never here).
//
// All four F04 list/inspector adapters (roster, profile, bonus, rank) bind
// the ONE shared f04-soldiers contract through these helpers: generated
// layout lookup (fail-closed), native widget placement, the HD list seam,
// 44px touch floors, off-screen parking for live-but-unpainted controls, and
// stable-id selection capture/restore via CalypsoSelectionListState.h.
//
// Parked controls stay fully live input/behavior owners: only their blit is
// suppressed and only their hit area moves (visibility flags and handler
// bindings are never modified), so keyboard paths and native gates behave
// exactly as before while no vanilla pixel can show or intercept pointer.
#ifdef __EMSCRIPTEN__
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/TextList.h"
#include "CalypsoHdUiModel.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSelectionListScroll.h"
#include "CalypsoSelectionListState.h"
#include "CalypsoUiMetrics.h"
#include "Generated/CalypsoF04Soldiers.generated.h"

namespace OpenXcom
{
namespace Calypso
{

inline bool f04HdWideLayout()
{
	return Options::baseXResolution >= 1000;
}

inline const CalypsoF04SoldiersGen::CalypsoF04SoldiersGenLayout* f04GeneratedLayout(bool wide)
{
	const auto* generated = CalypsoF04SoldiersGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F04 personnel generated layout is missing");
	return generated;
}

inline void f04ApplyRect(Surface* surface, int x, int y, int w, int h)
{
	if (!surface) return;
	surface->setX(x);
	surface->setY(y);
	surface->setWidth(w);
	surface->setHeight(h);
}

inline void f04ApplyRect(Surface* surface, const CalypsoLogicalRect& rect)
{
	f04ApplyRect(surface, rect.x, rect.y, rect.w, rect.h);
}

/// Park a live native control off-canvas. See the header contract above.
inline void f04ParkOffscreen(Surface* surface)
{
	if (!surface) return;
	surface->setX(-4096);
	surface->setY(-4096);
}

/// Expand a painted action rect to the shared touch floor, anchored to its
/// right edge and vertically centered (F03 chooser precedent).
inline CalypsoLogicalRect f04TouchRect(CalypsoLogicalRect visual)
{
	const int width = std::max(visual.w, CALYPSO_MIN_TOUCH_TARGET);
	const int height = std::max(visual.h, CALYPSO_MIN_TOUCH_TARGET);
	visual.x -= width - visual.w;
	visual.y -= (height - visual.h) / 2;
	visual.w = width;
	visual.h = height;
	return visual;
}

/// Configures the native HD selection-list seam AFTER enableUiScaling /
/// recapture, from the actual projected list rect and the generated metrics.
/// Same values re-applied preserve drag capture; real changes reset it.
inline void f04SeamSelectionList(
	TextList* list,
	Surface* window,
	const CalypsoF04SoldiersGen::CalypsoF04SoldiersGenLayout& generated)
{
	if (!list || !window || generated.window.w <= 0) return;
	const double uiScale = (double)window->getWidth() / (double)generated.window.w;
	const int scrollBarWidth = std::max(1, (int)std::llround(generated.scrollBarWidth * uiScale));
	const int minThumbHeight = std::max(1, (int)std::llround(generated.minThumbHeight * uiScale));
	const int rowStride = std::max(1, (int)std::llround(generated.rowHeight * uiScale));
	const size_t visibleRows = generated.visibleRows > 0 ? (size_t)generated.visibleRows : 0;
	list->configureCalypsoHdSelectionList(scrollBarWidth, minThumbHeight, rowStride, visibleRows);
}

/// Projects a generated design rect through the live window origin at the
/// current uiScale (F03 chooser projection, shared so paint and widget
/// placement cannot drift).
template <typename GeneratedRect>
inline CalypsoLogicalRect f04ProjectRect(
	const CalypsoLogicalRect& window,
	const CalypsoF04SoldiersGen::CalypsoF04SoldiersGenLayout& generated,
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

/// Generated action-slot rect by button id (the shared shell owns exactly
/// the contract's cancel slot; adapters never invent geometry).
inline CalypsoLogicalRect f04GeneratedButtonRect(bool wide, const char* id)
{
	const auto& buttons = CalypsoF04SoldiersGen::kButtonRects[wide ? 0 : 1];
	for (int i = 0; i < CalypsoF04SoldiersGen::kButtonCount; ++i)
	{
		if (id && buttons[i].id && std::string(buttons[i].id) == id)
		{
			const auto& rect = buttons[i].rect;
			return {rect.x, rect.y, rect.w, rect.h};
		}
	}
	return {};
}

/// Join non-empty native cells with middle dots (roster Name/Rank/Craft and
/// rank name/openings rows). Values are verbatim native strings.
inline std::string f04ComposeRowText(const std::vector<std::string>& cells)
{
	std::string out;
	for (const auto& cell : cells)
	{
		if (cell.empty()) continue;
		if (!out.empty()) out += " \xc2\xb7 ";
		out += cell;
	}
	return out;
}

/// Linearize one native label/value pair (profile stats, bonus summary rows).
inline std::string f04ComposeLabelValue(const std::string& label, const std::string& value)
{
	if (label.empty()) return value;
	if (value.empty()) return label;
	return label + ": " + value;
}

/// Selection snapshot across relayout: stable id + row + scroll, resolved
/// back through calypsoSelectionListRestoreSelection (never row numbers).
struct CalypsoF04ListSelection
{
	std::string id;
	std::size_t index = 0;
	std::size_t scroll = 0;
};

inline CalypsoF04ListSelection f04CaptureListSelection(
	TextList* list, const std::vector<std::string>& ids)
{
	CalypsoF04ListSelection captured{};
	if (!list) return captured;
	captured.index = list->getSelectedRow();
	captured.scroll = list->getScroll();
	if (captured.index < ids.size()) captured.id = ids[captured.index];
	return captured;
}

inline void f04RestoreListSelection(
	TextList* list, const CalypsoF04ListSelection& captured,
	const std::vector<std::string>& ids, std::size_t visibleRows)
{
	if (!list || ids.empty()) return;
	std::vector<std::string_view> views;
	views.reserve(ids.size());
	for (const auto& id : ids) views.push_back(std::string_view(id));
	const CalypsoSelectionListRestored restored = calypsoSelectionListRestoreSelection(
		std::string_view(captured.id), captured.index, views.data(), views.size());
	if (!restored.hasSelection) return;
	if (restored.index != list->getSelectedRow()) list->setSelectedRow(restored.index);
	const std::size_t maxScroll = calypsoSelectionListMaxScroll(ids.size(), visibleRows);
	list->scrollTo(captured.scroll < maxScroll ? captured.scroll : maxScroll);
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
