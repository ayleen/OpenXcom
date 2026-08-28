#pragma once
/*
 * Phase 46.4 Stage 13.2/13.3 (Calypso) -- loopback-only deterministic QA
 * presentation controls for the Geoscape HD capture matrix.
 *
 * The Stage 13.3 visual-acceptance rows (day, night, clouds, debug geometry,
 * reduced motion) must compare strictly, but the live decorative clocks
 * (`SDL_GetTicks()` star twinkle, shader `u_time` cloud drift) and the
 * campaign-time sun direction make repeated captures non-repeatable, and the
 * only debug-geometry owner is saved-game debug mode whose enablement is a
 * campaign mutation. This header is the single pure control state those QA
 * rows flip; it owns NO campaign, simulation, input, or ruleset state.
 *
 * Rules pinned here:
 *   - every field defaults to production (`Live` inputs, live clock, debug
 *     geometry off), and reset restores exactly that struct;
 *   - a frozen or reduced decorative clock replaces ONLY the millisecond
 *     source of the existing expressions -- never their math;
 *   - reduced motion freezes decoration at the documented zero-instant and
 *     wins over an explicit freeze when both are somehow set;
 *   - day/night sun selection is a pure function of the camera center in the
 *     fixed world frame used by Globe::getSunDirectionWorld();
 *   - the debug-geometry decision can only ADD presentation while the save is
 *     non-debug; the saved-game mode stays the sole canonical owner.
 *
 * Pure, dependency-free, natively unit tested
 * (tests/unit_tests/CalypsoGeoscapeQaPresentationTest.cpp).
 */
#include <cmath>

namespace OpenXcom
{
namespace Calypso
{

/// Which value feeds the globe shader's `u_sunDir` uniform.
///
///   Live = 0     campaign time via Globe::getSunDirectionWorld() (production)
///   Daylight = 1 observer-facing world normal: visible hemisphere fully lit
///   Night = 2    negated normal: visible hemisphere dark with city lights
enum class GeoscapeQaSunMode : int
{
	Live = 0,
	Daylight = 1,
	Night = 2,
};

/// Which texture input the globe shader samples as `u_clouds`.
///
///   Live = 0   mod clouds texture (production binding)
///   Hidden = 1 fully transparent input: shader alpha density yields zero
enum class GeoscapeQaCloudMode : int
{
	Live = 0,
	Hidden = 1,
};

struct GeoscapeQaPresentationState
{
	bool frozenClock = false;
	double frozenSeconds = 0.0;
	bool reducedMotion = false;
	GeoscapeQaSunMode sunMode = GeoscapeQaSunMode::Live;
	GeoscapeQaCloudMode cloudMode = GeoscapeQaCloudMode::Live;
	bool debugGeometryForced = false;
};

/// Restore exact production defaults.
inline void calypsoGeoscapeQaResetPresentation(GeoscapeQaPresentationState& s)
{
	s = GeoscapeQaPresentationState();
}

/// Process-wide control state consumed by the Geoscape globe paths. Only the
/// loopback-only `calypso_qa_globe_*` Emscripten exports write it.
inline GeoscapeQaPresentationState& calypsoGeoscapeQaPresentation()
{
	static GeoscapeQaPresentationState state;
	return state;
}

/// Reduced-motion frozen instant for both decorative clocks. Zero matches the
/// Stage 8 motion model's reduced-motion sample (cloud offset 0.0, seeded
/// resting star phases) so native and browser evidence describe one state.
constexpr double GEOSCAPE_QA_REDUCED_MOTION_SECONDS = 0.0;

/// Effective decorative presentation seconds for star twinkle and cloud
/// drift. With defaults this returns `liveSeconds` unchanged, keeping every
/// production expression bit-identical. A capture freeze pins the clock;
/// reduced motion pins it to the zero-instant regardless of any freeze value.
inline double calypsoGeoscapeQaPresentationSeconds(const GeoscapeQaPresentationState& s,
	double liveSeconds)
{
	if (!s.frozenClock && !s.reducedMotion) return liveSeconds;
	if (s.reducedMotion) return GEOSCAPE_QA_REDUCED_MOTION_SECONDS;
	return s.frozenSeconds < 0.0 ? 0.0 : s.frozenSeconds;
}

/// World-frame direction (Y = north pole, X = +90 deg lon east, Z = prime
/// meridian) of the surface point at (lon, lat) -- the same frame and angle
/// construction Globe::getSunDirectionWorld() documents.
struct GeoscapeQaVec3
{
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

inline GeoscapeQaVec3 calypsoGeoscapeQaWorldDirection(double lonRadians, double latRadians)
{
	return {std::cos(latRadians) * std::sin(lonRadians),
		std::sin(latRadians),
		std::cos(latRadians) * std::cos(lonRadians)};
}

/// Deterministic day/night sun direction at the current camera center:
/// Daylight points from the observed hemisphere (fully lit), Night is its
/// exact negation (fully dark with night lights). Live callers never reach
/// this helper.
inline GeoscapeQaVec3 calypsoGeoscapeQaSunDirection(GeoscapeQaSunMode mode,
	double cameraLonRadians, double cameraLatRadians)
{
	const GeoscapeQaVec3 view =
		calypsoGeoscapeQaWorldDirection(cameraLonRadians, cameraLatRadians);
	if (mode == GeoscapeQaSunMode::Night) return {-view.x, -view.y, -view.z};
	return view;
}

/// Effective debug-geometry visibility for one frame. The saved-game debug
/// mode remains the sole canonical owner: with the switch off the decision
/// equals it exactly, and forced-on can only add presentation while the save
/// is non-debug -- never suppress canonical output or mutate save state.
inline bool calypsoGeoscapeQaDebugGeometry(const GeoscapeQaPresentationState& s,
	bool savedGameDebugMode)
{
	return savedGameDebugMode || s.debugGeometryForced;
}

} // namespace Calypso
} // namespace OpenXcom
