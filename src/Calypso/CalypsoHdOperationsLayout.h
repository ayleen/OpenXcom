#pragma once
// Shared responsive classifier and fluid layout policy for the R&D operations
// family (re-reviews P1/RR3-001). The layout class comes from the canonical
// logical (CSS) viewport — never from Options::baseXResolution, the physical
// backing size, or DPR. Both the native adapters and the browser reference
// must answer with the same class and the same fluid rects for the same
// viewport.
//
// The fluid policy replaces the old affine reference-canvas stretch: authored
// reference geometry is CSS px and stays CSS px at every viewport in the
// class. Surplus width/height flows into the elastic regions (data area,
// inspector/dock), while fixed-size things — 44px targets, typography, row
// height, bars — keep their authored CSS size. The two reference viewports
// resolve to exactly the approved geometry (zero surplus ⇒ identity).
#include <cstdint>

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoHdOperationsLayoutClass
{
	Unsupported,
	Compact,
	Wide,
};

/// Approved rd-hd-ac-visual-contract breakpoints: wide only at
/// W >= 1280 && H >= 720; compact down to W >= 740 && H >= 360; anything
/// smaller is an explicit HD size failure, not a shrunken canvas.
inline CalypsoHdOperationsLayoutClass classifyCalypsoHdOperationsLayout(
	int logicalWidth, int logicalHeight)
{
	if (logicalWidth >= 1280 && logicalHeight >= 720)
	{
		return CalypsoHdOperationsLayoutClass::Wide;
	}
	if (logicalWidth >= 740 && logicalHeight >= 360)
	{
		return CalypsoHdOperationsLayoutClass::Compact;
	}
	return CalypsoHdOperationsLayoutClass::Unsupported;
}

/// A design-space rectangle (CSS px at the reference viewport).
struct CalypsoHdOperationsRectf
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

inline bool fluidSameEdge(int a, int b)
{
	const int delta = a - b;
	return delta >= -2 && delta <= 2;
}

/// The one fluid transform shared by native adapters and (via golden parity
/// tests) the browser reference. Built from the authored reference geometry;
/// the map functions distribute the width surplus, `mapY` the height surplus.
struct CalypsoHdOperationsFluidPolicy
{
	enum class Anchor
	{
		Geometry, ///< derive from authored geometry (full-width stretch,
		          ///< right-pin near the content edge, else keep)
		Left,     ///< keep authored x/w
		Right,    ///< shift by the whole surplus (right-anchored group)
		Stretch,  ///< width grows by the surplus
	};

	int referenceWidth = 0;
	int referenceHeight = 0;
	int contentLeft = 0;
	int contentWidth = 0;

	static CalypsoHdOperationsFluidPolicy forReference(int referenceWidth,
		int referenceHeight, int contentLeft, int contentWidth)
	{
		CalypsoHdOperationsFluidPolicy policy;
		policy.referenceWidth = referenceWidth;
		policy.referenceHeight = referenceHeight;
		policy.contentLeft = contentLeft;
		policy.contentWidth = contentWidth;
		return policy;
	}

	int widthSurplus(int actualLogicalWidth) const
	{
		const int surplus = actualLogicalWidth - referenceWidth;
		return surplus > 0 ? surplus : 0;
	}

	int heightSurplus(int actualLogicalHeight) const
	{
		const int surplus = actualLogicalHeight - referenceHeight;
		return surplus > 0 ? surplus : 0;
	}

	CalypsoHdOperationsRectf mapX(const CalypsoHdOperationsRectf &rect,
		int actualLogicalWidth, Anchor anchor = Anchor::Geometry) const
	{
		CalypsoHdOperationsRectf out = rect;
		const int dx = widthSurplus(actualLogicalWidth);
		if (dx == 0) return out;
		switch (anchor)
		{
		case Anchor::Left:
			return out;
		case Anchor::Right:
			out.x = rect.x + dx;
			return out;
		case Anchor::Stretch:
			out.w = rect.w + dx;
			return out;
		case Anchor::Geometry:
			break;
		}
		if (fluidSameEdge(rect.x, contentLeft)
			&& fluidSameEdge(rect.x + rect.w, contentLeft + contentWidth))
		{
			out.w = rect.w + dx; // full-content-width band stretches
		}
		else if (fluidSameEdge(rect.x + rect.w, contentLeft + contentWidth))
		{
			out.x = rect.x + dx; // right-pinned panel shifts as a unit
		}
		return out;
	}

	/// Proportionally redistributes a column/slot group across the stretched
	/// data width (weights keep their authored ratios, gaps included).
	void mapXStretchGroup(CalypsoHdOperationsRectf *rects, std::size_t count,
		int actualLogicalWidth) const
	{
		const int dx = widthSurplus(actualLogicalWidth);
		if (dx == 0 || count == 0) return;
		long long total = 0;
		for (std::size_t i = 0; i < count; ++i) total += rects[i].w;
		if (total <= 0) return;
		long long distributed = 0;
		for (std::size_t i = 0; i < count; ++i)
		{
			const long long extra = static_cast<long long>(dx) * rects[i].w / total;
			rects[i].w += static_cast<int>(extra);
			rects[i].x += static_cast<int>(distributed);
			distributed += extra;
		}
		// Rounding remainder extends the LAST rect so the group still ends
		// exactly on its authored right edge.
		const int remainder = dx - static_cast<int>(distributed);
		if (remainder != 0)
		{
			rects[count - 1].w += remainder;
		}
	}

	enum class VerticalRole
	{
		Fixed,       ///< authored CSS position and height (bars, controls)
		Stretch,     ///< elastic height: data area, inspector/dock panel
		BottomShift, ///< bottom-anchored band: footer/dock move down by dy
	};

	CalypsoHdOperationsRectf mapY(const CalypsoHdOperationsRectf &rect,
		int actualLogicalHeight, VerticalRole role) const
	{
		CalypsoHdOperationsRectf out = rect;
		const int dy = heightSurplus(actualLogicalHeight);
		if (dy == 0) return out;
		if (role == VerticalRole::Stretch)
		{
			out.h = rect.h + dy;
		}
		else if (role == VerticalRole::BottomShift)
		{
			out.y = rect.y + dy;
		}
		return out;
	}

	/// Row capacity comes from the actual resolved data viewport — never from
	/// the authored slot count.
	int visibleRows(int resolvedViewportY, int resolvedViewportH,
		int authoredHeaderHeight, int rowHeight) const
	{
		if (rowHeight <= 0) return 0;
		const int dataTop = resolvedViewportY + authoredHeaderHeight;
		const int dataHeight = resolvedViewportY + resolvedViewportH - dataTop;
		if (dataHeight <= 0) return 0;
		return dataHeight / rowHeight;
	}
};

} // namespace Calypso
} // namespace OpenXcom

