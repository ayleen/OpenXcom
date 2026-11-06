#pragma once
#ifdef __EMSCRIPTEN__

// Adapter-side fluid frame (RR3-001): resolves authored reference geometry
// into actual logical rects through the shared policy and projects them into
// surface coordinates once. Native hit owners and HD paint read the same
// rects; DPR enters only at the final raster boundary.

#include "CalypsoHdOperationsLayout.h"
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoViewportRuntime.h"

#include <algorithm>
#include <cstddef>

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoHdOperationsSelectionListGeometry
{
	int scrollBarWidth = 0;
	int minThumbHeight = 0;
	int rowStride = 0;
	int rowOriginY = 0;
	int dataViewportH = 0;
	std::size_t visibleRows = 0;
};

class CalypsoHdOperationsFluidFrame
{
public:
	static CalypsoHdOperationsFluidFrame forReference(int referenceWidth,
		int referenceHeight, int contentLeft, int contentWidth)
	{
		CalypsoHdOperationsFluidFrame frame;
		frame.policy = CalypsoHdOperationsFluidPolicy::forReference(
			referenceWidth, referenceHeight, contentLeft, contentWidth);
		const auto &metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
		// The viewport runtime is the authoritative CSS/logical source used by
		// the classifier and shared shell. Frozen presentation metrics may carry
		// a framebuffer-derived logical extent while a resize transaction is in
		// flight; using that extent here applies the logical projection twice.
		const auto effective = calypsoHdOperationsEffectiveViewport();
		frame.viewW = effective.cssWidth;
		frame.viewH = effective.cssHeight;
		frame.policy = frame.policy.withViewport(frame.viewW, frame.viewH);
		frame.valid = metrics.valid();
		return frame;
	}

	int width() const { return viewW; }
	int height() const { return viewH; }

	/// Resolve a generated collection in CSS/reference space and project each
	/// native-list descriptor field exactly once. In particular, do not feed
	/// the projected viewport back through resolve(): that applies the height
	/// surplus twice at intermediate viewports.
	CalypsoHdOperationsSelectionListGeometry selectionListGeometry(
		const CalypsoHdOperationsRectf &parent,
		const CalypsoHdOperationsRectf &rowSlot,
		const CalypsoHdOperationsRectf &authoredTrack,
		CalypsoHdOperationsFluidPolicy::VerticalRole parentRole =
			CalypsoHdOperationsFluidPolicy::VerticalRole::Stretch,
		CalypsoHdOperationsFluidPolicy::Anchor parentAnchor =
			CalypsoHdOperationsFluidPolicy::Anchor::Geometry) const
	{
		const auto cssParent = cssY(cssX(parent, parentAnchor), parentRole);
		const auto cssRow = cssY(cssX(rowSlot),
			CalypsoHdOperationsFluidPolicy::VerticalRole::Fixed);
		const int headerHeight = std::max(0, rowSlot.y - parent.y);
		const int rowStrideCss = std::max(1, cssRow.h);
		const int visibleRows = policy.visibleRows(
			cssParent.y, cssParent.h, headerHeight, rowStrideCss);
		const auto cssTrack = cssX(
			CalypsoHdOperationsRectf{authoredTrack.x,
				cssParent.y + headerHeight, authoredTrack.w,
				visibleRows * rowStrideCss});
		const auto projectedParent = projectCss(cssParent);
		const auto projectedRow = projectCss(cssRow);
		const auto projectedTrack = projectCss(cssTrack);
		const auto projectedThumb = projectCss(
			CalypsoHdOperationsRectf{0, 0, 1, 44});
		return {
			std::max(1, projectedTrack.w),
			std::max(1, projectedThumb.h),
			std::max(1, projectedRow.h),
			std::max(0, projectedRow.y - projectedParent.y),
			std::max(0, projectedTrack.h),
			static_cast<std::size_t>(std::max(0, visibleRows))};
	}

	/// Resolves one authored rect horizontally (geometry rule or explicit
	/// anchor) into CSS px.
	CalypsoHdOperationsRectf cssX(const CalypsoHdOperationsRectf &rect,
		CalypsoHdOperationsFluidPolicy::Anchor anchor =
			CalypsoHdOperationsFluidPolicy::Anchor::Geometry) const
	{
		return policy.mapX(rect, viewW, anchor);
	}

	/// Resolves one authored rect vertically into CSS px.
	CalypsoHdOperationsRectf cssY(const CalypsoHdOperationsRectf &rect,
		CalypsoHdOperationsFluidPolicy::VerticalRole role) const
	{
		return policy.mapY(rect, viewH, role);
	}

	/// Full resolution: horizontal anchor + vertical role, projected once into
	/// the surface (logical) coordinate space.
	CalypsoHdOperationsRect resolve(const CalypsoHdOperationsRectf &rect,
		CalypsoHdOperationsFluidPolicy::VerticalRole role,
		CalypsoHdOperationsFluidPolicy::Anchor anchor =
			CalypsoHdOperationsFluidPolicy::Anchor::Geometry) const
	{
		return projectCss(cssY(cssX(rect, anchor), role));
	}

	CalypsoHdOperationsFluidPolicy policy;

private:
	CalypsoHdOperationsRect projectCss(
		const CalypsoHdOperationsRectf &css) const
	{
		return calypsoHdOperationsProjectForCurrentPresentation(
			{css.x, css.y, css.w, css.h}, viewW, viewH);
	}

	int viewW = 1;
	int viewH = 1;
	bool valid = false;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
