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
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "CommandCenter/CommandCenterTheme.h"

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

struct CalypsoHdChooserLayout
{
	CalypsoHdOperationsRectf panel;
	int visibleRows = 0;
};

/// Resolve popup placement in one coordinate space. Native callers pass
/// projected logical rects; HD model callers pass CSS rects. Keeping the
/// placement/clamp/visible-window policy shared prevents double projection.
inline CalypsoHdChooserLayout calypsoHdResolveChooserLayout(
	const CalypsoHdOperationsRectf &button,
	const CalypsoHdOperationsRectf &bounds,
	int rowHeight, int insetY, int optionCount, int maxOptions,
	bool preferAbove, int minPanelWidth = 240)
{
	const int stride = std::max(1, rowHeight);
	const int safeInset = std::max(0, insetY);
	const int optionLimit = std::max(1, std::min(std::max(1, optionCount),
		std::max(1, maxOptions)));
	// Keep the popup disjoint from its opener. Button-first dispatch otherwise
	// closes the popup before the painted first option can receive the click.
	const int aboveSpace = std::max(0, button.y - bounds.y);
	const int belowSpace = std::max(0,
		(bounds.y + bounds.h) - (button.y + button.h));
	const auto rowsFor = [stride, safeInset](int space) {
		return std::max(0, (space - safeInset * 2) / stride);
	};
	const int aboveRows = rowsFor(aboveSpace);
	const int belowRows = rowsFor(belowSpace);
	// Use the side with the larger disjoint capacity; the caller's preference
	// breaks ties so a cramped preferred side never needlessly hides rows.
	const bool useAbove = aboveRows > belowRows
		|| (aboveRows == belowRows && preferAbove);
	const int availableRows = useAbove ? aboveRows : belowRows;
	const int visibleRows = std::max(1, std::min(optionLimit,
		std::max(1, availableRows)));
	const int panelH = visibleRows * stride + safeInset * 2;
	// The popup is a content surface, not a copy of the opener's toolbar
	// button.  Localized labels (especially the Russian availability pair)
	// can be substantially wider than the compact 104px opener, while a full
	// viewport-wide panel is excessive for short menus.  Keep a shared
	// content width floor and clamp it to the authored safe bounds.
	const int localizedContentWidth = std::max(1, minPanelWidth);
	const int panelW = std::max(1, std::min(bounds.w,
		std::max(button.w, localizedContentWidth)));
	const int panelX = std::max(bounds.x,
		std::min(button.x, bounds.x + bounds.w - panelW));
	const int aboveY = button.y - panelH;
	const int belowY = button.y + button.h;
	const int panelY = useAbove ? aboveY : belowY;
	return {{panelX, panelY, panelW, panelH}, visibleRows};
}

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
	int originDx = 0;
	int originDy = 0;

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

	/// Preserve the authored inset from the unchanged F01 rail/header when
	/// desktop-fit chrome grows between the canonical viewport endpoints.
	CalypsoHdOperationsFluidPolicy withViewport(int width, int height) const
	{
		auto result = *this;
		const auto scale = [](int w, int h) {
			return std::min({1.0, w / 1280.0, h / 720.0});
		};
		const double current = scale(width, height);
		const double authored = scale(referenceWidth, referenceHeight);
		result.originDx = static_cast<int>(std::lround(CommandCenterTheme::RailWidth * current)
			- std::lround(CommandCenterTheme::RailWidth * authored));
		result.originDy = static_cast<int>(std::lround(CommandCenterTheme::HeaderHeight * current)
			- std::lround(CommandCenterTheme::HeaderHeight * authored));
		return result;
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
		if (rect.x == 0 && rect.w == referenceWidth)
		{
			out.w = actualLogicalWidth;
			return out; // root window, not workspace content
		}
		if (dx == 0 && originDx == 0) return out;
		switch (anchor)
		{
		case Anchor::Left:
			out.x += originDx;
			return out;
		case Anchor::Right:
			out.x += dx;
			return out;
		case Anchor::Stretch:
			out.x += originDx;
			out.w += dx - originDx;
			return out;
		case Anchor::Geometry:
			break;
		}
		if (fluidSameEdge(rect.x, contentLeft)
			&& fluidSameEdge(rect.x + rect.w, contentLeft + contentWidth))
		{
			out.x += originDx;
			out.w += dx - originDx;
		}
		else if (fluidSameEdge(rect.x + rect.w, contentLeft + contentWidth))
		{
			out.x += dx;
		}
		else out.x += originDx;
		return out;
	}

	/// Proportionally redistributes a column/slot group across the stretched
	/// data width (weights keep their authored ratios, gaps included).
	void mapXStretchGroup(CalypsoHdOperationsRectf *rects, std::size_t count,
		int actualLogicalWidth) const
	{
		const int dx = widthSurplus(actualLogicalWidth) - originDx;
		if ((dx == 0 && originDx == 0) || count == 0) return;
		long long total = 0;
		for (std::size_t i = 0; i < count; ++i) total += rects[i].w;
		if (total <= 0) return;
		long long distributed = 0;
		for (std::size_t i = 0; i < count; ++i)
		{
			const long long extra = static_cast<long long>(dx) * rects[i].w / total;
			rects[i].w += static_cast<int>(extra);
			rects[i].x += originDx + static_cast<int>(distributed);
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
		if (rect.y == 0 && rect.h == referenceHeight)
		{
			out.h = actualLogicalHeight;
			return out;
		}
		if (role == VerticalRole::Stretch)
		{
			out.y += originDy;
			out.h += dy - originDy;
		}
		else if (role == VerticalRole::BottomShift) out.y += dy;
		else out.y += originDy;
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

