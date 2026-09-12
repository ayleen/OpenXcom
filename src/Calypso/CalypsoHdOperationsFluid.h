#pragma once
#ifdef __EMSCRIPTEN__

// Adapter-side fluid frame (RR3-001): resolves authored reference geometry
// into actual logical rects through the shared policy and projects them into
// surface coordinates once. Native hit owners and HD paint read the same
// rects; DPR enters only at the final raster boundary.

#include "CalypsoHdOperationsLayout.h"
#include "CalypsoHdUiOverlay.h"

#include <algorithm>

namespace OpenXcom
{
namespace Calypso
{

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
		frame.viewW = std::max(1, metrics.logicalWidth);
		frame.viewH = std::max(1, metrics.logicalHeight);
		frame.valid = metrics.valid();
		return frame;
	}

	int width() const { return viewW; }
	int height() const { return viewH; }

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
		const CalypsoHdOperationsRectf css = cssY(cssX(rect, anchor), role);
		return calypsoHdOperationsProjectForCurrentPresentation(
			{css.x, css.y, css.w, css.h}, viewW, viewH);
	}

	CalypsoHdOperationsFluidPolicy policy;

private:
	int viewW = 1;
	int viewH = 1;
	bool valid = false;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
