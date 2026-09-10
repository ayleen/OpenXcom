#pragma once
/*
 * Shared, screen-identity-neutral operations runtime model.
 *
 * Adapters populate this value model from generated contract geometry and
 * localized/native widget state.  It owns no gameplay and does not name a
 * route.  The native-pure helpers are intentionally usable by unit tests.
 */
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "CalypsoHdTextRasterKey.h"

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoHdOperationsArchetype
{
	OperationsWorkspace,
	WideDetail
};

enum class CalypsoHdOperationsControlKind
{
	Stepper,
	Toggle
};

enum class CalypsoHdOperationsRegionKind
{
	Fields,
	Preview,
	Collection,
	Actions
};

struct CalypsoHdOperationsRect
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;

	bool valid() const { return w > 0 && h > 0; }
};

/// One composed design-to-logical projection for both overlay paint and the
/// hidden native input owner. Offsets are already expressed in logical pixels.
inline CalypsoHdOperationsRect calypsoHdOperationsProjectRect(
	const CalypsoHdOperationsRect& rect, int designWidth, int designHeight,
	int logicalWidth, int logicalHeight, int logicalOffsetX, int logicalOffsetY)
{
	if (!rect.valid() || designWidth <= 0 || designHeight <= 0
		|| logicalWidth <= 0 || logicalHeight <= 0)
		return rect;
	const double sx = static_cast<double>(logicalWidth) / designWidth;
	const double sy = static_cast<double>(logicalHeight) / designHeight;
	const int left = static_cast<int>(std::llround(rect.x * sx)) - logicalOffsetX;
	const int right = static_cast<int>(std::llround((rect.x + rect.w) * sx))
		- logicalOffsetX;
	const int top = static_cast<int>(std::llround(rect.y * sy)) - logicalOffsetY;
	const int bottom = static_cast<int>(std::llround((rect.y + rect.h) * sy))
		- logicalOffsetY;
	return {left, top, std::max(1, right - left), std::max(1, bottom - top)};
}

enum class CalypsoHdOperationsContentRole
{
	Unknown,
	Name,
	Category,
	Status,
	Count,
	Quantity,
	Money,
	Time,
	Date,
	Type
};

/// The generated profile describes these roles as strings.  Keeping the
/// conversion here lets native adapters remain screen-identity-neutral while
/// still giving the renderer typed font/alignment metadata.
inline CalypsoHdOperationsContentRole calypsoHdOperationsContentRole(
	const std::string& role, const std::string& id = {})
{
	const std::string& value = role.empty() ? id : role;
	if (value == "name") return CalypsoHdOperationsContentRole::Name;
	if (value == "category") return CalypsoHdOperationsContentRole::Category;
	if (value == "status") return CalypsoHdOperationsContentRole::Status;
	if (value == "count") return CalypsoHdOperationsContentRole::Count;
	if (value == "quantity") return CalypsoHdOperationsContentRole::Quantity;
	if (value == "money") return CalypsoHdOperationsContentRole::Money;
	if (value == "time") return CalypsoHdOperationsContentRole::Time;
	if (value == "date") return CalypsoHdOperationsContentRole::Date;
	if (value == "type") return CalypsoHdOperationsContentRole::Type;
	return CalypsoHdOperationsContentRole::Unknown;
}

struct CalypsoHdOperationsState
{
	bool visible = true;
	bool focused = false;
	bool selected = false;
	bool disabled = false;
};

struct CalypsoHdOperationsCell
{
	std::string value;
	std::string contentRole;
	CalypsoHdOperationsState state;
};

struct CalypsoHdOperationsAction
{
	std::string id;
	std::string label;
	std::string component;
	std::string slotRole;
	std::string coordinateSpace;
	std::string tone = "normal";
	CalypsoHdOperationsRect visible;
	CalypsoHdOperationsRect hit;
	int focusOrder = 0;
	int zOrder = 1;
	CalypsoHdOperationsState state;
	// The native widget remains the input/behaviour owner in live mode.
	const void* widget = nullptr;
};

struct CalypsoHdOperationsSummaryField
{
	std::string id;
	std::string label;
	std::string value;
	CalypsoHdOperationsRect rect;
	CalypsoHdOperationsState state;
	const void* widget = nullptr;
};

struct CalypsoHdOperationsControl
{
	std::string id;
	std::string label;
	CalypsoHdOperationsControlKind kind = CalypsoHdOperationsControlKind::Toggle;
	std::vector<std::string> options;
	int selectedOption = 0;
	bool toggled = false;
	std::string displayValue;
	CalypsoHdOperationsRect rect;
	CalypsoHdOperationsRect valueRect;
	CalypsoHdOperationsAction decrement;
	CalypsoHdOperationsAction increment;
	CalypsoHdOperationsState state;
	const void* widget = nullptr;
};

struct CalypsoHdOperationsColumn
{
	std::string id;
	std::string label;
	CalypsoHdOperationsRect rect;
	CalypsoHdOperationsState state;
	/// Generated profile role (name/count/money/...). Empty means infer from id.
	std::string contentRole;
};

enum class CalypsoHdOperationsRowKind
{
	Item,
	GroupHeader,
	Empty
};

struct CalypsoHdOperationsRow
{
	std::string id;
	std::vector<std::string> values;
	CalypsoHdOperationsRect rect;
	CalypsoHdOperationsState state;
	const void* widget = nullptr;
	/// Optional typed cells. Legacy adapters may continue publishing values.
	std::vector<CalypsoHdOperationsCell> cells;
	CalypsoHdOperationsRowKind kind = CalypsoHdOperationsRowKind::Item;
};


struct CalypsoHdOperationsScroll
{
	std::size_t offset = 0;
	std::size_t count = 0;
	std::size_t visibleRows = 0;
	CalypsoHdOperationsRect viewport;
	CalypsoHdOperationsRect track;
	CalypsoHdOperationsRect thumb;
};

struct CalypsoHdOperationsCollection
{
	std::string heading;
	std::string meta;
	std::string emptyTitle;
	std::string emptyBody;
	std::string emptyKind = "empty";
	std::vector<CalypsoHdOperationsColumn> columns;
	std::vector<CalypsoHdOperationsRow> rows;
	std::vector<CalypsoHdOperationsRect> rowSlots;
	std::size_t selectedIndex = 0;
	std::size_t scrollOffset = 0;
	std::size_t visibleRows = 0;
	int rowHeight = 0;
	CalypsoHdOperationsRect viewport;
	CalypsoHdOperationsRect scrollTrack;
	CalypsoHdOperationsRect scrollThumb;
	/// New contract-shaped metadata. Legacy fields above remain adapter-compatible.
	std::size_t count = 0;
	CalypsoHdOperationsScroll scroll;
};

inline bool calypsoHdOperationsRowSelected(
	const CalypsoHdOperationsCollection& collection, std::size_t index)
{
	if (index >= collection.rows.size()) return false;
	const auto& row = collection.rows[index];
	return row.kind == CalypsoHdOperationsRowKind::Item
		&& !row.state.disabled
		&& (index == collection.selectedIndex || row.state.selected);
}


struct CalypsoHdOperationsMetric
{
	std::string id;
	std::string label;
	std::string value;
	CalypsoHdOperationsRect rect;
	CalypsoHdOperationsState state;
};

struct CalypsoHdOperationsIdentity
{
	std::string id;
	std::string label;
	std::string title;
	std::string subtitle;
	CalypsoHdOperationsRect rect;
	CalypsoHdOperationsRect titleRect;
	CalypsoHdOperationsRect subtitleRect;
	CalypsoHdOperationsState state;
};

struct CalypsoHdOperationsDetail
{
	std::string id;
	CalypsoHdOperationsIdentity identity;
	std::vector<CalypsoHdOperationsMetric> metrics;
	std::vector<CalypsoHdOperationsAction> actions;
	CalypsoHdOperationsRect panel;
	CalypsoHdOperationsState state;
};

struct CalypsoHdOperationsRegion
{
	std::string id;
	std::string label;
	CalypsoHdOperationsRegionKind kind = CalypsoHdOperationsRegionKind::Fields;
	std::string previewContent;
	CalypsoHdOperationsRect previewRect;
	CalypsoHdOperationsRect rect;
	CalypsoHdOperationsRect labelRect;
	CalypsoHdOperationsCollection collection;
	std::vector<CalypsoHdOperationsMetric> fields;
	std::vector<CalypsoHdOperationsAction> actions;
	CalypsoHdOperationsState state;
};

/// Every rectangle here is copied from generated contract data by an adapter.
/// The renderer derives no route-specific coordinates.
struct CalypsoHdOperationsGeometry
{
	int designWidth = 0;
	int designHeight = 0;
	CalypsoHdOperationsRect window;
	CalypsoHdOperationsRect screenHeader;
	CalypsoHdOperationsRect headerArt;
	CalypsoHdOperationsRect status;
	CalypsoHdOperationsRect title;
	CalypsoHdOperationsRect summaryBar;
	CalypsoHdOperationsRect toolbarBar;
	CalypsoHdOperationsRect controlBar;
	CalypsoHdOperationsRect collectionViewport;
	CalypsoHdOperationsRect detailPanel;
	CalypsoHdOperationsRect footer;
	CalypsoHdOperationsRect detailIdentity;
	CalypsoHdOperationsRect detailIdentityTitle;
	CalypsoHdOperationsRect detailIdentitySubtitle;
	std::vector<CalypsoHdOperationsRect> collectionColumns;
	std::vector<CalypsoHdOperationsRect> collectionRows;
	CalypsoHdOperationsRect collectionScrollTrack;
	CalypsoHdOperationsRect collectionScrollThumb;
	std::vector<CalypsoHdOperationsRect> detailMetrics;
	std::vector<CalypsoHdOperationsRect> detailActions;
	std::vector<CalypsoHdOperationsRect> footerActions;
	std::vector<CalypsoHdOperationsRegion> regions;
};

struct CalypsoHdOperationsReadiness
{
	bool contractReady = false;
	bool fontsReady = false;
	bool uploadsReady = false;
	bool retryable = false;
};

struct CalypsoHdOperationsStyle
{
	std::uint32_t panelFillTop = 0x071725E8u;
	std::uint32_t panelFillBottom = 0x050F19F2u;
	std::uint32_t regionFill = 0x071725E8u;
	std::uint32_t frame = 0x16384DFFu;
	std::uint32_t divider = 0x102E40FFu;
	std::uint32_t text = 0xE5F0F3FFu;
	std::uint32_t mutedText = 0x84A0AEFFu;
	std::uint32_t selection = 0x163C38FFu;
	std::uint32_t disabled = 0x55756D99u;
	std::uint32_t disabledText = 0xA9D8C7FFu;
	std::uint32_t accent = 0x74FFB0FFu;
	std::uint32_t warning = 0xF2B84BFFu;
	std::uint32_t danger = 0xF25F5CFFu;
	std::uint32_t textOnAccent = 0x071013FFu;
	float cornerRadiusPx = 12.0f;
	float cutCornerPx = 14.0f;
};

struct CalypsoHdOperationsModel
{
	CalypsoHdOperationsArchetype archetype = CalypsoHdOperationsArchetype::OperationsWorkspace;
	/// Generated descriptor metadata; identities are intentionally opaque to
	/// the shared renderer and are never used to select a layout branch.
	std::string presentation;
	std::string profileId;
	std::string profileVersion;
	std::string provenance;
	/// Contract family identity is supplied by the adapter; zero is never ready.
	std::uint32_t familyId = 0;
	const void* ownerState = nullptr;
	std::string title;
	std::vector<CalypsoHdOperationsSummaryField> summaryFields;
	std::vector<CalypsoHdOperationsControl> controls;
	std::vector<CalypsoHdOperationsAction> toolbarActions;
	CalypsoHdOperationsCollection collection;
	std::string visualShell;
	std::string headerArtId;
	std::string baseCaption;
	std::string baseName;
	std::string sectionLabel;
	std::string clockTime;
	std::string clockDate;
	CalypsoHdOperationsDetail detail;
	std::vector<CalypsoHdOperationsRegion> regions;
	std::vector<CalypsoHdOperationsAction> footerActions;
	/// Native presentation owners that must remain hidden without becoming
	/// rendered controls in this model.
	std::vector<const void*> suppressedWidgets;
	CalypsoHdOperationsGeometry geometry;
	CalypsoHdOperationsReadiness readiness;
	CalypsoTtfSourceDescriptor headingFont;
	CalypsoTtfSourceDescriptor bodyFont;
	CalypsoTtfSourceDescriptor monoFont;
	CalypsoHdOperationsStyle style;
};
/// Project a generated design-space font size into the canvas backing store.
/// The physical/design-height ratio includes both logical layout projection and
/// DPR, matching the rect edge mapping used by the HD overlay.
inline int calypsoHdOperationsPhysicalFontPx(
	int designFontPx, int designHeight, int physicalHeight)
{
	if (designFontPx <= 0 || designHeight <= 0 || physicalHeight <= 0) return 1;
	const std::int64_t numerator =
		static_cast<std::int64_t>(designFontPx) * physicalHeight;
	const std::int64_t rounded =
		(numerator + static_cast<std::int64_t>(designHeight) / 2) / designHeight;
	return static_cast<int>(std::max<std::int64_t>(
		1, std::min<std::int64_t>(rounded, 2147483647)));
}


inline bool calypsoHdOperationsHasScrollMetadata(
	const CalypsoHdOperationsCollection& collection)
{
	const auto& scroll = collection.scroll;
	return scroll.offset != 0 || scroll.count != 0 || scroll.visibleRows != 0
		|| scroll.viewport.valid() || scroll.track.valid() || scroll.thumb.valid();
}

inline std::size_t calypsoHdOperationsCollectionCount(
	const CalypsoHdOperationsCollection& collection)
{
	if (collection.count != 0) return collection.count;
	if (calypsoHdOperationsHasScrollMetadata(collection)
		&& collection.scroll.count != 0)
		return collection.scroll.count;
	return collection.rows.size();
}

inline std::size_t calypsoHdOperationsVisibleRows(
	const CalypsoHdOperationsCollection& collection)
{
	if (collection.visibleRows != 0) return collection.visibleRows;
	if (calypsoHdOperationsHasScrollMetadata(collection)
		&& collection.scroll.visibleRows != 0)
		return collection.scroll.visibleRows;
	return collection.rowSlots.size();
}

inline std::size_t calypsoHdOperationsScrollOffset(
	const CalypsoHdOperationsCollection& collection)
{
	return calypsoHdOperationsHasScrollMetadata(collection)
		? collection.scroll.offset : collection.scrollOffset;
}

inline CalypsoHdOperationsRect calypsoHdOperationsScrollTrack(
	const CalypsoHdOperationsCollection& collection,
	const CalypsoHdOperationsRect& fallback = {})
{
	if (collection.scroll.track.valid()) return collection.scroll.track;
	return collection.scrollTrack.valid() ? collection.scrollTrack : fallback;
}

inline CalypsoHdOperationsRect calypsoHdOperationsScrollThumb(
	const CalypsoHdOperationsCollection& collection,
	const CalypsoHdOperationsRect& fallback = {})
{
	if (collection.scroll.thumb.valid()) return collection.scroll.thumb;
	return collection.scrollThumb.valid() ? collection.scrollThumb : fallback;
}

inline std::size_t calypsoHdOperationsMaxScroll(
	const CalypsoHdOperationsCollection& collection)
{
	const std::size_t visible = calypsoHdOperationsVisibleRows(collection);
	const std::size_t count = calypsoHdOperationsCollectionCount(collection);
	return visible == 0 || count <= visible ? 0 : count - visible;
}

/// Resolve a visible slot only from generated geometry. Missing or invalid
/// slots deliberately fail closed rather than reusing a runtime row rectangle.
inline CalypsoHdOperationsRect calypsoHdOperationsRowSlot(
	const CalypsoHdOperationsCollection& collection, std::size_t slot)
{
	if (slot >= calypsoHdOperationsVisibleRows(collection)
		|| slot >= collection.rowSlots.size())
		return {};
	const auto result = collection.rowSlots[slot];
	return result.valid() ? result : CalypsoHdOperationsRect{};
}

inline std::optional<std::size_t> calypsoHdOperationsRowIndexForSlot(
	const CalypsoHdOperationsCollection& collection, std::size_t slot)
{
	const auto rowSlot = calypsoHdOperationsRowSlot(collection, slot);
	if (!rowSlot.valid()) return std::nullopt;
	const std::size_t offset = calypsoHdOperationsScrollOffset(collection);
	if (offset > collection.rows.size() || slot > collection.rows.size() - offset)
		return std::nullopt;
	const std::size_t index = offset + slot;
	return index < calypsoHdOperationsCollectionCount(collection)
		? std::optional<std::size_t>(index) : std::nullopt;
}

inline CalypsoHdOperationsRect calypsoHdOperationsScrollHitRail(
	const CalypsoHdOperationsCollection& collection,
	const CalypsoHdOperationsRect& fallback = {})
{
	const auto track = calypsoHdOperationsScrollTrack(collection, fallback);
	if (!track.valid() || calypsoHdOperationsMaxScroll(collection) == 0) return {};
	constexpr int minimumWidth = 44;
	const int width = std::max(minimumWidth, track.w);
	return {track.x - (width - track.w) / 2, track.y, width, track.h};
}

inline CalypsoHdOperationsRect calypsoHdOperationsRowHitRect(
	const CalypsoHdOperationsCollection& collection, std::size_t slot)
{
	auto row = calypsoHdOperationsRowSlot(collection, slot);
	if (!row.valid()) return {};
	const auto rail = calypsoHdOperationsScrollHitRail(collection);
	if (rail.valid() && row.x < rail.x + rail.w && rail.x < row.x + row.w)
	{
		const int right = std::min(row.x + row.w, rail.x);
		row.w = std::max(0, right - row.x);
	}
	return row.valid() ? row : CalypsoHdOperationsRect{};
}
struct CalypsoHdOperationsActionPalette
{
	std::uint32_t fill = 0;
	std::uint32_t border = 0;
	std::uint32_t text = 0;
};

inline CalypsoHdOperationsActionPalette calypsoHdOperationsActionPalette(
	const CalypsoHdOperationsStyle& style, const std::string& tone,
	const CalypsoHdOperationsState& state)
{
	CalypsoHdOperationsActionPalette palette;
	if (state.disabled)
		return {style.disabled, style.frame, style.disabledText};
	if (tone == "safe")
		palette = {style.panelFillTop, style.accent, style.text};
	else if (tone == "primary")
		palette = {style.accent, style.accent, style.textOnAccent};
	else if (tone == "warning")
		palette = {style.warning, style.warning, style.textOnAccent};
	else if (tone == "danger")
		palette = {style.danger, style.danger, style.text};
	else
		palette = {style.panelFillTop, style.frame, style.text};
	if (state.selected && tone != "primary" && tone != "warning"
		&& tone != "danger")
		palette.fill = style.selection;
	if (state.focused) palette.border = style.accent;
	return palette;
}

inline CalypsoHdOperationsRect calypsoHdOperationsVisibleScrollThumb(
	const CalypsoHdOperationsCollection& collection,
	const CalypsoHdOperationsRect& fallbackTrack = {},
	const CalypsoHdOperationsRect& fallbackThumb = {})
{
	const auto track = calypsoHdOperationsScrollTrack(collection, fallbackTrack);
	const auto generatedThumb = calypsoHdOperationsScrollThumb(collection, fallbackThumb);
	const std::size_t visible = calypsoHdOperationsVisibleRows(collection);
	const std::size_t count = calypsoHdOperationsCollectionCount(collection);
	if (visible == 0 || count <= visible || !track.valid()
		|| calypsoHdOperationsScrollOffset(collection) > count - visible)
		return {};
	const std::size_t maxScroll = count - visible;
	const std::size_t offset = calypsoHdOperationsScrollOffset(collection);
	constexpr int minimumHeight = 44;
	const auto proportionalHeight = static_cast<int>(
		(static_cast<long long>(track.h) * visible) / count);
	const int thumbHeight = std::min(track.h,
		std::max(minimumHeight, proportionalHeight));
	const int travel = track.h - thumbHeight;
	const int y = track.y + static_cast<int>(
		(static_cast<long long>(travel) * offset) / maxScroll);
	return {generatedThumb.valid() ? generatedThumb.x : track.x, y,
		generatedThumb.valid() ? generatedThumb.w : track.w, thumbHeight};
}


/// Explicit validation helper retained for callers that need to sanitize a
/// local model snapshot. Renderers never call it: native selection/scroll are
/// published exactly as read from their owner.
inline void calypsoHdOperationsClampSelectionAndScroll(
	CalypsoHdOperationsCollection& collection)
{
	if (collection.rows.empty()) collection.selectedIndex = 0;
	else if (collection.selectedIndex >= collection.rows.size())
		collection.selectedIndex = collection.rows.size() - 1;
	collection.scrollOffset = std::min(collection.scrollOffset,
		calypsoHdOperationsMaxScroll(collection));
	if (!collection.rows.empty() && collection.visibleRows > 0)
	{
		if (collection.selectedIndex < collection.scrollOffset)
			collection.scrollOffset = collection.selectedIndex;
		const std::size_t lastVisible = collection.scrollOffset + collection.visibleRows;
		if (collection.selectedIndex >= lastVisible)
			collection.scrollOffset = collection.selectedIndex + 1 - collection.visibleRows;
		collection.scrollOffset = std::min(collection.scrollOffset,
			calypsoHdOperationsMaxScroll(collection));
	}
}

/// Clamp selection and scroll without changing row order or gameplay state.
inline void calypsoHdOperationsClampSelectionAndScroll(CalypsoHdOperationsModel& model)
{
	calypsoHdOperationsClampSelectionAndScroll(model.collection);
}

inline bool calypsoHdOperationsActionVisible(const CalypsoHdOperationsAction& action)
{
	return action.state.visible && !action.id.empty() && action.visible.valid();
}

inline bool calypsoHdOperationsHitTargetValid(const CalypsoHdOperationsAction& action)
{
	return !action.state.visible || (action.hit.valid() && action.hit.w >= 44 && action.hit.h >= 44);
}

inline bool calypsoHdOperationsActionReady(const CalypsoHdOperationsAction& action)
{
	return !action.state.visible
		|| (action.widget != nullptr && !action.label.empty()
			&& calypsoHdOperationsActionVisible(action)
			&& calypsoHdOperationsHitTargetValid(action));
}
inline CalypsoHdOperationsRect calypsoHdOperationsCollectionViewport(
	const CalypsoHdOperationsCollection& collection,
	const CalypsoHdOperationsRect& fallback = {})
{
	return collection.scroll.viewport.valid() ? collection.scroll.viewport
		: (collection.viewport.valid() ? collection.viewport : fallback);
}


inline bool calypsoHdOperationsModelReady(const CalypsoHdOperationsModel& model)
{
	const auto& g = model.geometry;
	if (model.familyId == 0 || !model.readiness.contractReady || !model.readiness.fontsReady
		|| !model.readiness.uploadsReady || model.title.empty()
		|| g.designWidth <= 0 || g.designHeight <= 0 || !g.window.valid()
		|| !g.title.valid() || !g.footer.valid())
		return false;
	if ((model.archetype == CalypsoHdOperationsArchetype::OperationsWorkspace
			|| model.archetype == CalypsoHdOperationsArchetype::WideDetail)
		&& (model.visualShell != "base-operations" || model.headerArtId.empty()
			|| model.baseCaption.empty() || !g.screenHeader.valid()
			|| !g.headerArt.valid()))
		return false;
	if (model.summaryFields.size() > 4 || model.controls.size() > 3
		|| model.toolbarActions.size() > 4 || model.collection.columns.size() > 8
		|| model.detail.metrics.size() > 4 || model.detail.actions.size() > 12
		|| model.footerActions.size() > 3 || model.regions.size() > 4)
		return false;
	for (const auto& control : model.controls)
	{
		if (control.options.size() > 32) return false;
		if (control.state.visible && (control.widget == nullptr || !control.rect.valid()))
			return false;
		if (control.state.visible
			&& control.kind == CalypsoHdOperationsControlKind::Stepper
			&& (!control.valueRect.valid()
				|| !control.decrement.state.visible
				|| !control.increment.state.visible
				|| !calypsoHdOperationsActionReady(control.decrement)
				|| !calypsoHdOperationsActionReady(control.increment)))
			return false;
	}
	const auto& mainRowSlots = g.collectionRows.empty()
		? model.collection.rowSlots : g.collectionRows;
	const std::size_t rowCount = calypsoHdOperationsCollectionCount(model.collection);
	const std::size_t visibleMainRows = std::min(
		calypsoHdOperationsVisibleRows(model.collection), rowCount);
	if (rowCount != model.collection.rows.size()) return false;
	for (const auto& row : model.collection.rows)
	{
		if ((row.values.size() > model.collection.columns.size()
				&& row.cells.size() > model.collection.columns.size())
			|| row.widget == nullptr)
			return false;
	}
	if (mainRowSlots.size() < visibleMainRows) return false;
	for (std::size_t i = 0; i < visibleMainRows; ++i)
		if (!mainRowSlots[i].valid()) return false;
	for (const auto& region : model.regions)
	{
		if (region.fields.size() > 3 || region.actions.size() > 12)
			return false;
		switch (region.kind)
		{
		case CalypsoHdOperationsRegionKind::Fields:
			if (region.fields.empty()) return false;
			for (const auto& field : region.fields)
				if (!field.rect.valid()) return false;
			break;
		case CalypsoHdOperationsRegionKind::Preview:
			if (region.previewContent.empty() || !region.previewRect.valid())
				return false;
			break;
		case CalypsoHdOperationsRegionKind::Actions:
			if (region.actions.empty()) return false;
			for (const auto& action : region.actions)
				if (!calypsoHdOperationsActionReady(action)) return false;
			break;
		case CalypsoHdOperationsRegionKind::Collection:
		{
			const auto& collection = region.collection;
			const std::size_t count = calypsoHdOperationsCollectionCount(collection);
			const std::size_t visible = calypsoHdOperationsVisibleRows(collection);
			if (collection.columns.empty() || collection.columns.size() > 8
				|| collection.rowSlots.size() > 64 || visible == 0
				|| collection.rowHeight <= 0
				|| !calypsoHdOperationsCollectionViewport(collection, region.rect).valid()
				|| count != collection.rows.size())
				return false;
			for (const auto& row : collection.rows)
			{
				if (row.widget == nullptr
					|| (row.values.size() > collection.columns.size()
						&& row.cells.size() > collection.columns.size()))
					return false;
			}
			const std::size_t visibleRows = std::min(visible, count);
			if (collection.rowSlots.size() < visibleRows) return false;
			for (std::size_t i = 0; i < visibleRows; ++i)
				if (!collection.rowSlots[i].valid()) return false;
			break;
		}
		}
	}
	if (model.archetype == CalypsoHdOperationsArchetype::OperationsWorkspace)
	{
		if (!g.summaryBar.valid() || !g.toolbarBar.valid() || !g.collectionViewport.valid()
			|| !g.detailPanel.valid() || model.collection.columns.empty()
			|| calypsoHdOperationsVisibleRows(model.collection) == 0
			|| model.collection.rowHeight <= 0
			|| (model.collection.rows.empty()
				&& (model.collection.emptyTitle.empty()
					|| model.collection.emptyBody.empty()))
			|| model.detail.identity.id.empty() || model.detail.identity.label.empty()
			|| model.detail.identity.title.empty())
			return false;
	}
	else
	{
		if (!g.status.valid() || !g.controlBar.valid() || model.regions.empty()) return false;
		for (const auto& region : model.regions)
			if (!region.rect.valid()) return false;
	}
	for (const auto& action : model.toolbarActions)
		if (!calypsoHdOperationsActionReady(action)) return false;
	for (const auto& action : model.detail.actions)
		if (!calypsoHdOperationsActionReady(action)) return false;
	for (const auto& action : model.footerActions)
		if (!calypsoHdOperationsActionReady(action)) return false;
	for (const auto& region : model.regions)
		for (const auto& action : region.actions)
			if (!calypsoHdOperationsActionReady(action)) return false;
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
