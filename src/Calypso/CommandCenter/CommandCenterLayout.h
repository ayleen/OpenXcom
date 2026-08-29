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
	Tablet,
	Mobile,
};

/// Header-internal and stage-anchored rects that complete the desktop grid.
struct CommandCenterLayout
{
	LayoutMode mode = LayoutMode::Desktop;

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

	RectF mobileHeader;
	RectF mobileGlobeViewport;
	RectF mobileGlobe;
	RectF mobileObjectSheet;
	RectF mobilePlayback;
	RectF mobileBottomNavigation;
};

/// The only place the responsive mode is decided (spec s.5): by LOGICAL
/// viewport width, never physical pixels.
inline LayoutMode selectLayoutMode(float logicalWidth)
{
	if (logicalWidth >= 1280.0f) return LayoutMode::Desktop;
	if (logicalWidth >= 1024.0f) return LayoutMode::CompactDesktop;
	if (logicalWidth >= 768.0f) return LayoutMode::Tablet;
	return LayoutMode::Mobile;
}

/// Desktop grid (spec s.15). With the inspector closed the stage and the
/// timeline expand across the freed width (spec s.74).
CommandCenterLayout computeDesktopLayout(Size2 viewport, bool inspectorOpen);

/// Compact desktop (spec s.49): narrower rail and pads; the inspector floats
/// over the stage's right edge with an opaque panel.
CommandCenterLayout computeCompactDesktopLayout(Size2 viewport, bool inspectorOpen);

/// Tablet (spec s.50): icon-only rail; the inspector opens as a right drawer
/// min(360, 42% viewport width) overlapping the stage.
CommandCenterLayout computeTabletLayout(Size2 viewport, bool inspectorOpen);

/// Mobile (spec s.52): dedicated components; the globe is intentionally
/// cropped inside the mobile viewport.
CommandCenterLayout computeMobileLayout(Size2 viewport, const InsetsF& safeInsets);

/// Convenience: dispatch on the mode selected from the viewport width.
CommandCenterLayout computeLayout(Size2 viewport, bool inspectorOpen);

} // namespace CommandCenter
} // namespace Calypso
} // namespace OpenXcom
