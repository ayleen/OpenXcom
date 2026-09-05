#pragma once
/*
 * Command Center -- layout computation (normative spec 2026-08-28, s.5/s.13-15/s.49-52/s.32).
 *
 * The single source of every Command Center rectangle. Renderers and input
 * routing must consume these rects; duplicating coordinates elsewhere is
 * forbidden by the spec (s.64, s.83.2).
 *
 * Pure C++ (no SDL, no GL, no engine state): natively unit-testable and
 * shared by the Emscripten production renderer.
 */
#include "CommandCenterTypes.h"
#include <array>

namespace OpenXcom
{
namespace Calypso
{
namespace CommandCenter
{

enum class LayoutMode
{
	Desktop,
	CompactDesktop,
	Mobile,
};

/// Header-internal and stage-anchored rects that complete the desktop grid.
struct CommandCenterLayout
{
	LayoutMode mode = LayoutMode::Desktop;
	// Uniform layout-unit to CSS-pixel scale; small windows keep the desktop grid.
	float scale = 1.0f;

	RectF root;
	RectF header;
	RectF navigationRail;
	RectF workspace;

	RectF stage;
	RectF timeline;
	RectF inspector;

	RectF baseSelector;
	RectF dateTimeBlock;
	RectF systemStatusBlock;
	RectF notificationButton;

	RectF zoomControls;

	RectF compactCommandGrid;
	std::array<RectF, 6> compactCommands{};
	std::array<RectF, 6> compactTimeSteps{};
};


/// Desktop grid (spec s.15). With the inspector closed the stage and the
/// timeline expand across the freed width (spec s.74).
CommandCenterLayout computeDesktopLayout(Size2 viewport, bool inspectorOpen);

/// Compact desktop (spec s.49): narrower rail and pads; the inspector floats
/// over the stage's right edge with an opaque panel.
CommandCenterLayout computeCompactDesktopLayout(Size2 viewport, bool inspectorOpen);


/// Compact landscape below 1024 CSS px: modern technical header, clipped
/// left globe stage, 2x3 command grid and one horizontal six-speed rail.
CommandCenterLayout computeMobileLayout(Size2 viewport, const InsetsF& safeInsets);

/// Gameplay uses the desktop grid everywhere, uniformly fitted below 1280x720.
/// Compact implementations remain dormant while responsive switching is disabled.
CommandCenterLayout computeLayout(Size2 viewport, bool inspectorOpen, const InsetsF& safeInsets);

} // namespace CommandCenter
} // namespace Calypso
} // namespace OpenXcom
