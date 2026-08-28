#pragma once
/*
 * Phase 46.4 (Calypso) -- pure canvas-backing <-> engine-display coordinate
 * mapping for browser input and the DOM text overlay.
 *
 * Root contract (docs/phases/phase-46.4-geoscape-hd-v2.md, 2026-08-25
 * checkpoint): the browser bridge supplies canvas-BACKING coordinates while
 * the engine SDL/display coordinates may be CSS-logical after exact
 * backing-store ownership (devicePixelRatio != 1). Input must be normalized
 * from the canvas physical extent to the current engine display extent BEFORE
 * Screen scale conversion, and the reverse engine-display -> canvas mapping
 * used by the DOM text overlay must be the exact inverse. At DPR 1 the
 * mapping is identity; invalid/nonpositive extents fail safe to identity.
 *
 * Header-only, allocation-free, no SDL / JavaScript / engine dependency:
 * native doctests exercise the real code
 * (tests/unit_tests/CalypsoCanvasCoordinateMappingTest.cpp).
 */

namespace OpenXcom
{
namespace Calypso
{

/// Normalizes one axis coordinate from the canvas-backing extent onto the
/// current engine display extent. Fails safe to identity when either extent
/// is nonpositive; otherwise scales proportionally:
/// coordinate * displayExtent / canvasExtent.
inline double calypsoCanvasToDisplayCoordinate(double coordinate,
                                               int canvasExtent,
                                               int displayExtent)
{
	if (canvasExtent <= 0 || displayExtent <= 0) return coordinate;
	return coordinate * displayExtent / canvasExtent;
}

/// Exact inverse of calypsoCanvasToDisplayCoordinate: maps one axis
/// coordinate from the engine display extent back onto the canvas-backing
/// extent, with the same identity fallback for nonpositive extents.
inline double calypsoDisplayToCanvasCoordinate(double coordinate,
                                               int displayExtent,
                                               int canvasExtent)
{
	if (displayExtent <= 0 || canvasExtent <= 0) return coordinate;
	return coordinate * canvasExtent / displayExtent;
}

} // namespace Calypso
} // namespace OpenXcom
