#pragma once
/*
 * Phase 46.4-F33 (Calypso) -- pure reference math for the styled-panel SDF
 * shader (hd_ui_panel.frag), kept here so the exact formula is natively
 * testable (CalypsoHdSdfFalloffTest.cpp) and the GLSL stays a one-line mirror.
 *
 * F33-PARITY-003: the shader computed glow with "1.0 + d / radius"; outside
 * the SDF shape d > 0 made the term > 1 and the clamp pinned the glow to FULL
 * STRENGTH at the edge and beyond -- a hard plateau instead of a soft shadow.
 * The contract below is a monotonic OUTWARD falloff: 1 at the shape edge
 * (d == 0), 0 at/after radius. radius <= 0 disables the glow entirely.
 *
 * Pure, dependency-free, natively unit tested.
 */
#include <algorithm>

namespace OpenXcom
{
namespace Calypso
{

/// Monotonic outward glow falloff over the signed distance `d` (physical px
/// from the shape edge, positive = outside) and the glow `radius` (px).
/// Edge (d == 0) is full strength; half radius is strictly between; radius
/// and beyond are zero; radius <= 0 means no glow.
inline float calypsoHdGlowFalloff(float d, float radius)
{
	if (radius <= 0.0f) return 0.0f;
	return std::max(0.0f, std::min(1.0f, 1.0f - d / radius));
}

} // namespace Calypso
} // namespace OpenXcom
