#pragma once
// Calypso HD selection-list scroll geometry (pure, natively testable).
//
// Single source of truth for the inset track + min-clamped thumb mapping
// shared by TextList/ScrollBar input and the shared selection-list painter.
// All inputs are engine-logical px; DPR is never applied here. Empty and
// non-scrolling lists are safe (thumb height 0, scroll 0).
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>
namespace OpenXcom
{
namespace Calypso
{

struct CalypsoSelectionListTrack
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

/// Inset full-height track at the list's right edge, matching the painted
/// track (list.right - scrollBarWidth, full list height).
inline CalypsoSelectionListTrack calypsoSelectionListTrackForList(
	int listX, int listY, int listW, int listH, int scrollBarWidth)
{
	if (scrollBarWidth <= 0 || listW <= 0 || listH <= 0)
	{
		return {listX, listY, 0, 0};
	}
	const int w = std::min(scrollBarWidth, listW);
	return {listX + listW - w, listY, w, listH};
}

inline std::size_t calypsoSelectionListMaxScroll(std::size_t total, std::size_t visible)
{
	return total > visible ? total - visible : 0;
}

inline std::size_t calypsoSelectionListClampScroll(std::size_t scroll, std::size_t total, std::size_t visible)
{
	return std::min(scroll, calypsoSelectionListMaxScroll(total, visible));
}

/// Collapse TextList's vanilla physical row map (which repeats a logical row
/// once per wrapped line) to the one-row-per-item map required by the fixed
/// stride HD owner. The input values are logical row indices; preserving their
/// first occurrence keeps this helper independent from TextList storage.
inline std::vector<std::size_t> calypsoSelectionListNormalizeRows(
	const std::vector<std::size_t>& physicalRows)
{
	std::vector<std::size_t> logicalRows;
	for (const std::size_t row : physicalRows)
		if (logicalRows.empty() || logicalRows.back() != row)
			logicalRows.push_back(row);
	return logicalRows;
}

/// Return the first vanilla physical line belonging to a logical row. The
/// end position is an explicit invalid result for stale selections.
inline std::size_t calypsoSelectionListFirstPhysicalRow(
	const std::vector<std::size_t>& physicalRows, std::size_t logicalRow)
{
	for (std::size_t index = 0; index < physicalRows.size(); ++index)
		if (physicalRows[index] == logicalRow) return index;
	return physicalRows.size();
}

/// Min-clamped thumb height for a scrollable list; 0 when there is nothing
/// to scroll (painted thumb is absent, drag maps to scroll 0).
inline int calypsoSelectionListThumbHeight(int trackH, std::size_t total, std::size_t visible, int minThumb)
{
	if (trackH <= 0 || total <= visible || total == 0 || visible == 0)
	{
		return 0;
	}
	long long h = (long long)trackH * (long long)visible / (long long)total;
	if (h < minThumb)
	{
		h = minThumb;
	}
	if (h > trackH)
	{
		h = trackH;
	}
	if (h < 1)
	{
		h = 1;
	}
	return (int)h;
}

/// Thumb offset from the track top for an absolute scroll value.
inline int calypsoSelectionListThumbOffset(int trackH, int thumbH, std::size_t scroll, std::size_t maxScroll)
{
	if (maxScroll == 0 || thumbH <= 0 || trackH <= thumbH)
	{
		return 0;
	}
	const std::size_t s = std::min(scroll, maxScroll);
	const long long travel = (long long)trackH - (long long)thumbH;
	return (int)((travel * (long long)s) / (long long)maxScroll);
}

/// Absolute scroll value for a thumb offset from the track top. Clamps,
/// monotonic in the offset, exact at both travel endpoints.
inline std::size_t calypsoSelectionListScrollForOffset(int offset, int trackH, int thumbH, std::size_t maxScroll)
{
	if (maxScroll == 0)
	{
		return 0;
	}
	const long long travel = (long long)trackH - (long long)thumbH;
	if (travel <= 0)
	{
		return 0;
	}
	long long o = offset;
	if (o < 0)
	{
		o = 0;
	}
	if (o > travel)
	{
		o = travel;
	}
	return (std::size_t)((o * (long long)maxScroll + travel / 2) / travel);
}

/// Maps a pointer offset from the list surface top (engine-logical px, i.e.
/// already divided by the screen scale once) to a native row. `rowOrigin` is
/// the logical offset from the surface top to the first painted row slot; the
/// header and any trailing empty remainder own no row. Out-of-data-viewport or
/// past-the-end pointers yield `hit == false` — never a clamped last row.
struct CalypsoSelectionListRowHit
{
	bool hit = false;
	std::size_t index = 0;
};

inline CalypsoSelectionListRowHit calypsoSelectionListRowAtLogicalY(
	double relativeLogicalY, double rowStride, double rowOrigin,
	std::size_t scroll, std::size_t total, std::size_t visibleCapacity)
{
	CalypsoSelectionListRowHit out;
	if (total == 0 || rowStride <= 0 || visibleCapacity == 0) return out;
	if (!(relativeLogicalY >= rowOrigin)) return out;
	const double dataY = relativeLogicalY - rowOrigin;
	const long long slot = static_cast<long long>(
		std::floor(dataY / rowStride));
	if (slot < 0 || slot >= static_cast<long long>(visibleCapacity)) return out;
	const std::size_t index = scroll + static_cast<std::size_t>(slot);
	if (index >= total) return out;
	out.hit = true;
	out.index = index;
	return out;
}

/// Ordinary (non-HD-seam) TextList row hit mapping. The row is rendered using
/// the list's own width scale, then the screen action scale; keeping this
/// helper pure makes the resized-list regression executable without SDL.
inline std::size_t calypsoTextListRowAtRenderedY(
	double relativeY, int rowHeight, double listScale, double yScale,
	std::size_t scroll)
{
	const double stride = std::max(1.0,
		static_cast<double>(rowHeight) * listScale * yScale);
	if (!(relativeY >= 0.0)) return scroll;
	return scroll + static_cast<std::size_t>(std::floor(relativeY / stride));
}

/// The one descriptor the adapter hands to the native TextList. Projected
/// values are engine-logical px. `dataViewportH` is exactly the painted
/// generated track height — the row origin is applied separately as a Y
/// offset and must never be folded into the height (re-review P2: the
/// invisible input rail once extended 36px past the painted track).
struct CalypsoSelectionListDescriptor
{
	int scrollBarWidth = 0;
	int minThumbHeight = 0;
	int rowStride = 0;
	int rowOriginY = 0;
	int dataViewportH = 0;
	std::size_t visibleRows = 0;
};

inline CalypsoSelectionListDescriptor calypsoSelectionListDescriptorFor(
	int projectedViewportY, int projectedTrackY, int projectedTrackW,
	int projectedTrackH, int projectedRowStride, std::size_t emittedVisibleRows,
	int projectedMinThumb)
{
	CalypsoSelectionListDescriptor out;
	out.scrollBarWidth = std::max(1, projectedTrackW);
	out.minThumbHeight = std::max(1, projectedMinThumb);
	out.rowStride = std::max(1, projectedRowStride);
	out.rowOriginY = std::max(0, projectedTrackY - projectedViewportY);
	out.dataViewportH = std::max(1, projectedTrackH);
	out.visibleRows = emittedVisibleRows;
	return out;
}

} // namespace Calypso
} // namespace OpenXcom
