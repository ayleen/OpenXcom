#pragma once
/*
 * Calypso shared native HD tabbed-management renderer.
 *
 * One archetype-owned physical implementation for every tabbed-management
 * screen (Forces/submarine roster, submarine detail, crew, armor, weapons,
 * equipment, presets, pilots). Generated tabbed contracts own copy,
 * geometry, and rest-state tokens. State adapters provide only localized
 * text, live widgets, interaction state, and projected rectangles. This
 * renderer is the single physical implementation of the archetype: no
 * screen-specific painter, geometry, or copy may live here or in callers.
 */
#include <cstdint>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoHdInteractionState.h"
#include "CalypsoHdUiModel.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/TextList.h"

namespace OpenXcom
{
class Mod;
class Surface;
class Text;
class TextButton;
class TextList;

namespace Calypso
{

/// One painted footer/toolbar/detail action bound to a live native button.
/// The native widget stays the behavior/input owner; this model only carries
/// what the shared shell paints.
struct CalypsoTabbedAction
{
	TextButton* widget = nullptr;
	TextButton* peer = nullptr;
	std::string text;
	CalypsoLogicalRect rect;
	CalypsoActionTone tone = CalypsoActionTone::Safe;
	std::uint32_t restFill = 0;
	std::uint32_t restBorder = 0;
	std::uint32_t textColor = 0;
};

/// One painted section tab. Tabs with a live native owner carry its widget
/// (blit-suppressed, positioned hit area); display-only tabs (no existing
/// native handler) paint selected/disabled state with a null widget and
/// commit nothing.
struct CalypsoTabbedTab
{
	std::string id;
	std::string label;
	bool selected = false;
	bool enabled = true;
	TextButton* widget = nullptr;
	CalypsoLogicalRect rect;
};

/// One painted header summary fact (label + value composed by the adapter
/// from live native texts).
struct CalypsoTabbedSummaryField
{
	std::string text;
	CalypsoLogicalRect rect;
};

/// One painted toolbar control slot. The native control (combo, toggle,
/// text field, or button) stays the behavior/input owner; the painted
/// label mirrors its live text.
struct CalypsoTabbedControl
{
	std::string id;
	std::string kind;
	std::string text;
	Surface* widget = nullptr;
	CalypsoLogicalRect rect;
};

/// One painted collection column header (live native header text where the
/// state owns headers, otherwise empty and skipped). The stable column id
/// drives structural roles: the "open" column paints the row-activation
/// affordance marker instead of text.
struct CalypsoTabbedColumn
{
	std::string id;
	std::string label;
	CalypsoLogicalRect rect;
};

/// One painted detail metric (label + value composed by the adapter from
/// live native texts).
struct CalypsoTabbedMetric
{
	std::string text;
	CalypsoLogicalRect rect;
};

/// Selected-item detail panel. Present only when the contract declares a
/// detail object; metrics and actions bind live native state, never fixture
/// copy beyond generated fallback labels.
struct CalypsoTabbedDetail
{
	bool present = false;
	std::string titleText;
	std::string subtitleText;
	CalypsoLogicalRect panel{};
	CalypsoLogicalRect titleRect{};
	CalypsoLogicalRect subtitleRect{};
	std::vector<CalypsoTabbedMetric> metrics;
	std::vector<CalypsoTabbedAction> actions;
};

/// One chooser row: the readable native label plus the native availability
/// verdict. The native list remains the behavior/input owner; this model
/// only carries what the shared shell paints. When per-column cells are
/// present and match the header count, the renderer paints aligned cells;
/// otherwise it falls back to the composed single-line text.
struct CalypsoTabbedRow
{
	std::string text;
	std::vector<std::string> cells;
	bool enabled = true;
};

/// One painted grid tile (mount/weapon cards). The native control bound to
/// the tile stays the behavior/input owner; the painted label mirrors its
/// live text. Tiles never scroll: the fixture count always fits the
/// template-owned tile capacity.
struct CalypsoTabbedTile
{
	CalypsoLogicalRect rect;
	CalypsoLogicalRect labelRect;
	std::string label;
	bool enabled = true;
	Surface* widget = nullptr;
};

/// Reusable tabbed-management model: full-viewport shell (header, summary
/// rail, tab bar, toolbar, collection viewport, detail panel, footer) over
/// native rows. Production paints no scrim/backdrop; the isolated harness
/// host owns the opaque backing.
struct CalypsoTabbedModel
{
	std::uint32_t familyId = 0;
	const void* instance = nullptr;
	Mod* mod = nullptr;
	bool wide = false;
	int designWidth = 0;
	int designHeight = 0;

	CalypsoLogicalRect window;
	CalypsoLogicalRect title;
	CalypsoLogicalRect summaryBar;
	CalypsoLogicalRect tabBar;
	CalypsoLogicalRect toolbarBar;
	CalypsoLogicalRect collectionViewport;
	CalypsoLogicalRect detailPanel;
	CalypsoLogicalRect footer;
	/// Design-space row slots at the native stride, in visible order.
	std::vector<CalypsoLogicalRect> rowSlots;

	Surface* windowWidget = nullptr;
	Text* titleWidget = nullptr;
	TextList* listWidget = nullptr;

	std::string titleText;
	std::vector<CalypsoTabbedTab> tabs;
	std::vector<CalypsoTabbedSummaryField> summary;
	std::vector<CalypsoTabbedControl> controls;
	std::vector<CalypsoTabbedAction> toolbar;
	std::vector<CalypsoTabbedColumn> columns;
	/// All native rows in collection order; the renderer paints the scrolled
	/// window.
	std::vector<CalypsoTabbedRow> rows;
	std::size_t scrollOffset = 0;
	std::size_t selectedRow = 0;
	bool hasSelection = false;
	/// Grid tiles painted inside the collection viewport instead of list
	/// rows (mount cards). Empty for list/table collections.
	std::vector<CalypsoTabbedTile> tiles;

	CalypsoTabbedDetail detail;
	std::vector<CalypsoTabbedAction> actions;

	int rowHeight = 1;
	int visibleRows = 1;
	int scrollBarWidth = 0;
	/// Projected min thumb height (shared 44px touch floor scaled); the
	/// legacy scrollbar path uses this instead of a hardcoded painter
	/// constant.
	int minThumbHeight = 0;
	/// Native inset track/thumb in the list's logical space, shared with
	/// TextList/ScrollBar input. When present the painter uses them exactly.
	bool hasNativeScrollGeometry = false;
	CalypsoLogicalRect nativeTrack{};
	CalypsoLogicalRect nativeThumb{};
	bool nativeThumbVisible = false;
	float cutCornerPx = 0.0f;
	std::uint32_t panelFillTop = 0;
	std::uint32_t panelFillBottom = 0;
	std::uint32_t frameColor = 0;
	std::uint32_t selectedTabColor = 0;
	std::uint32_t dividerColor = 0;
	std::uint32_t footerDotColor = 0;
	std::uint32_t textColor = 0;
	std::uint32_t mutedTextColor = 0;
	std::uint32_t selectionColor = 0;
	std::uint32_t scrollTrackColor = 0;
	std::uint32_t scrollThumbColor = 0;

	double uiScale = 1.0;
	double visualScale = 1.0;
	double projectionScaleX = 1.0;
	double projectionScaleY = 1.0;
	int titleDesignHeight = 1;
	int motionDurationMs = 0;
	double motionScaleFrom = 1.0;
};

void calypsoCollectTabbedManagement(
	CalypsoHdFrameBuilder& builder,
	const CalypsoTabbedModel& model,
	CalypsoSmallConfirmationMotion& motion);

/// Map a contract button tone to the shared interaction tone. Rest visuals
/// always come from the contract fill/border words; only hover/focus/press
/// resolve through the theme token families, which know Safe/Primary/
/// Destructive alone.
inline CalypsoActionTone calypsoTabbedToneForContract(const std::string& tone)
{
	if (tone == "danger") return CalypsoActionTone::Destructive;
	if (tone == "primary") return CalypsoActionTone::Primary;
	return CalypsoActionTone::Safe;
}

/// Shared tabbed geometry plumbing (archetype-generic; rendering stays in
/// the collector above). Adapters position live native widgets in design
/// space and project paint rects through the live window origin at the
/// current uiScale, so paint and widget placement cannot drift.

inline bool calypsoTabbedWideLayout()
{
	return Options::baseXResolution >= 1000;
}

/// Projects a generated design rect through the live window origin at the
/// current uiScale. Generic over the generated tabbed layout types.
template <typename GeneratedLayout, typename GeneratedRect>
inline CalypsoLogicalRect calypsoTabbedProjectRect(
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

/// Named-rect lookup by stable id over one generated rect array.
template <typename NamedRect>
inline CalypsoLogicalRect calypsoTabbedFindRect(const NamedRect* row, int count, const char* id)
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

/// Configures the native HD selection-list seam AFTER enableUiScaling /
/// recapture, from the generated collection metrics. Same values re-applied
/// preserve drag capture; real changes reset it. The minimum thumb reuses
/// the shared 44px touch floor (not a second geometry convention).
template <typename GeneratedLayout>
inline void calypsoTabbedSeamList(
	TextList* list,
	Surface* window,
	const GeneratedLayout& generated)
{
	if (!list || !window || generated.window.w <= 0) return;
	const double uiScale = (double)window->getWidth() / (double)generated.window.w;
	const int scrollBarWidth = std::max(1, (int)std::llround(generated.scrollBarWidth * uiScale));
	const int minThumbHeight = std::max(1, (int)std::llround(CALYPSO_MIN_TOUCH_TARGET * uiScale));
	const int rowStride = std::max(1, (int)std::llround(generated.rowHeight * uiScale));
	const size_t visibleRows = generated.visibleRows > 0 ? (size_t)generated.visibleRows : 0;
	list->configureCalypsoHdSelectionList(scrollBarWidth, minThumbHeight, rowStride, visibleRows);
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
