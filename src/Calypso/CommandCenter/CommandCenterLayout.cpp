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

/// Shared desktop header: base selector at the left, live date/time at the right.
void layoutHeaderContent(CommandCenterLayout& layout, float viewportWidth,
	float headerHeight, float sideInset)
{
	using namespace CommandCenterTheme;

	layout.baseSelector = { sideInset, (headerHeight - 48.0f) / 2.0f, 220.0f, 48.0f };

	const float dateTimeWidth = 176.0f;
	const float contentHeight = 40.0f;
	layout.dateTimeBlock = {viewportWidth - sideInset - dateTimeWidth,
		(headerHeight - contentHeight) / 2.0f, dateTimeWidth, contentHeight};
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


CommandCenterLayout computeMobileLayout(Size2 viewport, const InsetsF& safeInsets)
{
	CommandCenterLayout layout;
	layout.mode = LayoutMode::Mobile;
	layout.root = {0.0f, 0.0f, viewport.width, viewport.height};

	const float contentLeft = safeInsets.left;
	const float contentRight = viewport.width - safeInsets.right;
	const float contentTop = safeInsets.top;
	const float contentBottom = viewport.height - safeInsets.bottom;
	const float contentWidth = contentRight - contentLeft;

	layout.header = {0.0f, contentTop, viewport.width, 48.0f};
	layout.timeline = {contentLeft + 12.0f, contentBottom - 64.0f,
		contentWidth - 24.0f, 56.0f};
	layout.compactCommandGrid = {contentRight - 216.0f,
		layout.header.bottom() + 6.0f, 208.0f, 214.0f};
	layout.navigationRail = layout.compactCommandGrid;
	layout.stage = {contentLeft, layout.header.bottom(),
		layout.compactCommandGrid.x - 8.0f - contentLeft,
		layout.timeline.y - layout.header.bottom()};
	layout.workspace = layout.stage;

	layout.baseSelector = {contentLeft + 8.0f, contentTop + 2.0f, 184.0f, 44.0f};
	layout.dateTimeBlock = {contentRight - 8.0f - 176.0f,
		contentTop + 4.0f, 176.0f, 40.0f};
	layout.zoomControls = {layout.stage.right() - 56.0f,
		layout.stage.bottom() - 100.0f, 44.0f, 88.0f};

	const float commandWidth = 96.0f;
	const float commandHeight = 66.0f;
	for (std::size_t index = 0; index < layout.compactCommands.size(); ++index)
	{
		const float column = static_cast<float>(index % 2);
		const float row = static_cast<float>(index / 2);
		layout.compactCommands[index] = {
			layout.compactCommandGrid.x + column * 104.0f,
			layout.compactCommandGrid.y + row * 74.0f,
			commandWidth, commandHeight};
	}

	const float timeStepsX = layout.timeline.x + 12.0f;
	const float timeStepWidth = (layout.timeline.width - 24.0f) / 6.0f;
	for (std::size_t index = 0; index < layout.compactTimeSteps.size(); ++index)
		layout.compactTimeSteps[index] = {
			timeStepsX + timeStepWidth * static_cast<float>(index),
			layout.timeline.y + 7.0f, timeStepWidth, 48.0f};

	return layout;
}

CommandCenterLayout computeLayout(Size2 viewport, bool inspectorOpen, const InsetsF&)
{
	const float scale = std::min({1.0f, viewport.width / 1280.0f, viewport.height / 720.0f});
	auto layout = computeDesktopLayout(
		Size2{viewport.width / scale, viewport.height / scale}, inspectorOpen);
	layout.scale = scale;
	return layout;
}

} // namespace CommandCenter
} // namespace Calypso
} // namespace OpenXcom
