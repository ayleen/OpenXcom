/*
 * Command Center -- layout computation (normative spec 2026-08-28).
 * See CommandCenterLayout.h for the contract.
 */
#include "CommandCenterLayout.h"

#include <algorithm>

#include "CommandCenterTheme.h"

namespace OpenXcom
{
namespace Calypso
{
namespace CommandCenter
{

namespace
{

/// Header content grid shared by the desktop and compact modes (spec s.19,
/// s.20): the session selector sits at the left edge; the right group is
/// computed from the right edge with the fixed 329-unit stack
/// (date/time 112 + 16 + divider 1 + 16 + status 128 + 16 + bell 40).
void layoutHeaderContent(CommandCenterLayout& layout, float viewportWidth,
	float headerHeight, float sideInset)
{
	using namespace CommandCenterTheme;

	layout.baseSelector = { sideInset, (headerHeight - 48.0f) / 2.0f, 220.0f, 48.0f };

	const float rightGroupWidth = 112.0f + 16.0f + 1.0f + 16.0f + 128.0f + 16.0f + 40.0f;
	const float rightGroupX = viewportWidth - sideInset - rightGroupWidth;
	const float centerHeight = 40.0f;
	const float contentY = (headerHeight - centerHeight) / 2.0f;

	layout.dateTimeBlock = { rightGroupX, contentY, 112.0f, centerHeight };
	layout.systemStatusBlock = { rightGroupX + 112.0f + 16.0f + 1.0f + 16.0f, contentY, 128.0f, centerHeight };
	layout.notificationButton = { rightGroupX + rightGroupWidth - 40.0f,
		(headerHeight - 40.0f) / 2.0f, 40.0f, 40.0f };
}

/// Zoom cluster anchored inside the stage (spec s.32): left + 16, bottom
/// - 16 - 84; the container is 40x84 with two 40x42 halves.
void layoutZoomControls(CommandCenterLayout& layout)
{
	layout.zoomControls = { layout.stage.x + 16.0f,
		layout.stage.y + layout.stage.height - 16.0f - 84.0f, 40.0f, 84.0f };
}

/// Shared desktop-family grid: header + left rail + padded workspace with
/// stage/timeline stacked and the inspector in the leftover width.
CommandCenterLayout computeDesktopFamily(Size2 viewport, float headerHeight,
	float railWidth, float workspacePad, float workspaceGap, float timelineHeight)
{
	using namespace CommandCenterTheme;

	CommandCenterLayout layout;
	layout.mode = LayoutMode::Desktop;
	layout.root = { 0.0f, 0.0f, viewport.width, viewport.height };
	layout.header = { 0.0f, 0.0f, viewport.width, headerHeight };
	layout.navigationRail = { 0.0f, headerHeight, railWidth, viewport.height - headerHeight };
	layout.workspace = { railWidth, headerHeight,
		viewport.width - railWidth, viewport.height - headerHeight };

	const RectF inner = inset(layout.workspace, workspacePad);

	// Reference inspector: closed on this pass (spec s.74) — callers open it
	// by re-running with the overlay variant below.
	const float stageHeight = inner.height - timelineHeight - workspaceGap;
	layout.stage = { inner.x, inner.y, inner.width, stageHeight };
	layout.timeline = { inner.x, inner.y + stageHeight + workspaceGap,
		inner.width, timelineHeight };
	layout.inspector = {};
	return layout;
}

} // namespace

CommandCenterLayout computeDesktopLayout(Size2 viewport, bool inspectorOpen)
{
	using namespace CommandCenterTheme;

	CommandCenterLayout layout = computeDesktopFamily(viewport,
		HeaderHeight, RailWidth, WorkspacePad, WorkspaceGap, TimelineHeight);

	if (inspectorOpen)
	{
		const RectF inner = inset(layout.workspace, WorkspacePad);
		const float stageWidth = inner.width - InspectorWidth - WorkspaceGap;
		layout.stage = { inner.x, inner.y, stageWidth, layout.stage.height };
		layout.timeline = { inner.x, layout.timeline.y, stageWidth, layout.timeline.height };
		layout.inspector = { inner.x + stageWidth + WorkspaceGap, inner.y,
			InspectorWidth, std::min(540.0f, inner.height) };
	}

	layoutHeaderContent(layout, viewport.width, HeaderHeight, 16.0f);
	layoutZoomControls(layout);
	return layout;
}

CommandCenterLayout computeCompactDesktopLayout(Size2 viewport, bool inspectorOpen)
{
	using namespace CommandCenterTheme;

	CommandCenterLayout layout = computeDesktopFamily(viewport,
		HeaderHeight, 72.0f, 16.0f, 16.0f, TimelineHeight);
	layout.mode = LayoutMode::CompactDesktop;

	// Spec s.49: the inspector floats OVER the stage, width 300, opaque.
	if (inspectorOpen)
	{
		layout.inspector = {
			layout.stage.right() - 16.0f - 300.0f,
			layout.stage.y + 16.0f,
			300.0f,
			std::min(520.0f, layout.stage.height - 32.0f),
		};
	}

	layoutHeaderContent(layout, viewport.width, HeaderHeight, 16.0f);
	layoutZoomControls(layout);
	return layout;
}

CommandCenterLayout computeTabletLayout(Size2 viewport, bool inspectorOpen)
{
	using namespace CommandCenterTheme;

	CommandCenterLayout layout = computeDesktopFamily(viewport,
		64.0f, 64.0f, 12.0f, 12.0f, 92.0f);
	layout.mode = LayoutMode::Tablet;

	// Spec s.50: the inspector is a right drawer overlapping the stage with
	// width min(360, 42% of the viewport width).
	if (inspectorOpen)
	{
		const float drawerWidth = std::min(360.0f, viewport.width * 0.42f);
		layout.inspector = {
			layout.stage.right() - 12.0f - drawerWidth,
			layout.stage.y + 12.0f,
			drawerWidth,
			layout.stage.height - 24.0f,
		};
	}

	layoutHeaderContent(layout, viewport.width, 64.0f, 12.0f);
	layoutZoomControls(layout);
	return layout;
}

CommandCenterLayout computeMobileLayout(Size2 viewport, const InsetsF& safeInsets)
{
	CommandCenterLayout layout;
	layout.mode = LayoutMode::Mobile;

	const float headerHeight = 56.0f;
	const float navigationHeight = 64.0f;
	const float playbackHeight = 56.0f;
	const float objectSheetHeight = 222.0f;
	const float sideInset = 8.0f;
	const float verticalGap = 8.0f;

	const float contentTop = safeInsets.top;
	const float contentBottom = viewport.height - safeInsets.bottom;

	layout.mobileHeader = { 0.0f, contentTop, viewport.width, headerHeight };

	layout.mobileBottomNavigation = { 0.0f, contentBottom - navigationHeight,
		viewport.width, navigationHeight };

	layout.mobilePlayback = { sideInset,
		layout.mobileBottomNavigation.y - verticalGap - playbackHeight,
		viewport.width - sideInset * 2.0f, playbackHeight };

	layout.mobileObjectSheet = { sideInset,
		layout.mobilePlayback.y - verticalGap - objectSheetHeight,
		viewport.width - sideInset * 2.0f, objectSheetHeight };

	layout.mobileGlobeViewport = { 0.0f, layout.mobileHeader.bottom(), viewport.width,
		layout.mobileBottomNavigation.y - layout.mobileHeader.bottom() };

	const float globeSize = clampFloat(viewport.width * 4.0f / 3.0f, 480.0f, 560.0f);

	layout.mobileGlobe = { (viewport.width - globeSize) * 0.5f,
		layout.mobileGlobeViewport.y + 20.0f, globeSize, globeSize };

	return layout;
}

CommandCenterLayout computeLayout(Size2 viewport, bool inspectorOpen)
{
	switch (selectLayoutMode(viewport.width))
	{
		case LayoutMode::Desktop:
			return computeDesktopLayout(viewport, inspectorOpen);
		case LayoutMode::CompactDesktop:
			return computeCompactDesktopLayout(viewport, inspectorOpen);
		case LayoutMode::Tablet:
			return computeTabletLayout(viewport, inspectorOpen);
		case LayoutMode::Mobile:
		default:
			return computeMobileLayout(viewport, InsetsF{});
	}
}

} // namespace CommandCenter
} // namespace Calypso
} // namespace OpenXcom
