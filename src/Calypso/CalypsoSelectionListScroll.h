#pragma once
// Calypso HD selection-list scroll geometry (pure, natively testable).
//
// Single source of truth for the inset track + min-clamped thumb mapping
// shared by TextList/ScrollBar input and the shared selection-list painter.
// All inputs are engine-logical px; DPR is never applied here. Empty and
// non-scrolling lists are safe (thumb height 0, scroll 0).
#include <algorithm>
#include <cstddef>
#include <cmath>

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

/// List-relative design offset backing one absolute design target X. Stored
/// instead of the absolute so later setX/setWidth/scale changes re-project
/// instead of replaying stale absolute coordinates.
inline int calypsoHdArrowRelOffset(int absDesignX, int listDesignX)
{
	return absDesignX - listDesignX;
}

/// Projected native hit rectangle for one HD quantity-arrow target.
/// Inputs are design-space (list-relative X offset plus target w/h); the
/// output is native logical px at the current viewport scale. Rounding
/// mirrors State::applyUiScaling — floor(v*s+0.5) — so native ArrowButton
/// rects coincide with the painted stepper at DPR1 and fractional scales.
/// Negative offsets clamp to the list edge; sizes floor at 1.
struct CalypsoHdArrowProjection
{
	int x = 0;
	int w = 0;
	int h = 0;
};

inline CalypsoHdArrowProjection calypsoHdArrowProjectTarget(
	int listX, int relDesignX, int designW, int designH, double scale)
{
	CalypsoHdArrowProjection out;
	if (!(scale > 0.0))
	{
		return out;
	}
	const int rel = relDesignX < 0 ? 0 : relDesignX;
	out.x = listX + (int)std::floor((double)rel * scale + 0.5);
	out.w = (int)std::floor((double)designW * scale + 0.5);
	out.h = (int)std::floor((double)designH * scale + 0.5);
	if (out.w < 1) out.w = 1;
	if (out.h < 1) out.h = 1;
	return out;
}

/// Hit result for one HD quantity-stepper pointer position: side selects the
/// behavior column (0 = left/increase, 1 = right/decrease), slot selects the
/// visible row. side < 0 means no stepper claims the point.
struct CalypsoHdStepperHit
{
	int side = -1;
	int slot = -1;
};

/// Hit-test a pointer against two projected stepper columns and the visible
/// row window. Columns arrive already projected to native logical px (the
/// same helper the painter and the arrow layout share); rows start at listY
/// with the native stride. Zero-width columns never claim; the header band
/// above listY never selects. Pure and allocation-free.
inline CalypsoHdStepperHit calypsoHdStepperHit(double x, double y,
	int leftX, int leftW, int rightX, int rightW,
	int listY, int rowStride, std::size_t scroll, std::size_t rowCount, std::size_t visibleRows)
{
	CalypsoHdStepperHit out;
	if (rowStride <= 0 || visibleRows == 0 || scroll >= rowCount)
	{
		return out;
	}
	const std::size_t shown = std::min(visibleRows, rowCount - scroll);
	const double relY = y - (double)listY;
	if (!(relY >= 0.0) || relY >= (double)shown * (double)rowStride)
	{
		return out;
	}
	const int slot = (int)(relY / (double)rowStride);
	if (slot < 0 || (std::size_t)slot >= shown)
	{
		return out;
	}
	int side = -1;
	if (leftW > 0 && x >= (double)leftX && x < (double)(leftX + leftW))
	{
		side = 0;
	}
	else if (rightW > 0 && x >= (double)rightX && x < (double)(rightX + rightW))
	{
		side = 1;
	}
	if (side < 0)
	{
		return out;
	}
	out.side = side;
	out.slot = slot;
	return out;
}

/// Press/release pairing for one HD stepper gesture: a click fires only when
/// release lands on the same side and text row as the press. Plain value
/// semantics; the owner (TextList) holds one instance per list.
struct CalypsoHdStepperPress
{
	bool active = false;
	int side = -1;
	std::size_t index = 0;
};

inline void calypsoHdStepperPressBegin(CalypsoHdStepperPress& press, int side, std::size_t index)
{
	press.active = true;
	press.side = side;
	press.index = index;
}

/// Completes a release: always clears the gesture and reports whether it
/// closes a click on the same target. A release with no recorded press, or
/// on a different target, never clicks.
inline bool calypsoHdStepperPressRelease(CalypsoHdStepperPress& press, int side, std::size_t index)
{
	const bool click = press.active && side == press.side && index == press.index;
	press.active = false;
	press.side = -1;
	press.index = 0;
	return click;
}

/// Projected native row stride from a design stride and the live viewport
/// scale. Hit-testing inputs (hover ordinates, stepper slot mapping) arrive
/// in projected/native px, so a design stride must be projected before it
/// can divide them; at scale 1 the design value passes through unchanged.
/// Rounding mirrors applyUiScaling (floor(v*s+0.5)), floored at 1; bad
/// inputs yield 0 so callers fail closed instead of dividing by zero.
inline int calypsoHdProjectedRowStride(int designStride, double scale)
{
	if (designStride <= 0 || !(scale > 0.0))
	{
		return 0;
	}
	const int projected = (int)std::floor((double)designStride * scale + 0.5);
	return projected < 1 ? 1 : projected;
}

/// Visible slot offset for a rows-origin-relative pointer ordinate, given
/// the projected stride and the action scale. Pure form of the hover
/// mapping: the caller adds scroll and clamps as before. May return a
/// negative or past-the-end offset for out-of-window pointers; degenerate
/// stride/scale yields 0 (stay) instead of dividing by zero.
inline int calypsoHdHoverSlotIndex(double relY, int projectedStride, double actionScale)
{
	if (projectedStride <= 0 || !(actionScale > 0.0))
	{
		return 0;
	}
	return (int)std::floor(relY / ((double)projectedStride * actionScale));
}

} // namespace Calypso
} // namespace OpenXcom
