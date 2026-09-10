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

/// Maps display-space pointer Y to the native row index. `rowStride` is
/// already projected into display pixels; callers must not apply screen scale
/// a second time.
inline std::size_t calypsoSelectionListRowAtDisplayY(
	double relativeY, int rowStride, std::size_t scroll, std::size_t total)
{
	if (total == 0 || rowStride <= 0) return 0;
	const long long offset = static_cast<long long>(
		std::floor(std::max(0.0, relativeY) / static_cast<double>(rowStride)));
	const std::size_t index = scroll + static_cast<std::size_t>(offset);
	return std::min(index, total - 1);
}

} // namespace Calypso
} // namespace OpenXcom
