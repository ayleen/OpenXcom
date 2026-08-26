#pragma once
/*
 * Phase 46.4 §16.3 (Calypso) -- pure one-time off-globe hover transition
 * decision for base placement (BuildNewBaseState::hoverRedraw).
 *
 * Owner-confirmed defect: a pointer parked in the starfield outside the
 * Earth disk made the old handler assign non-finite cartToPolar results into
 * its last-sample pair and re-run invalidate() on every 50 ms tick
 * (AreSame(NaN, NaN) never holds), so the globe rebuilt continuously, stale
 * radar rings froze on screen, and recovery depended on poisoned comparisons.
 *
 * Root contract (docs/phases/phase-46.4-geoscape-hd-v2.md §16.3):
 *   - a NON-FINITE sample is a ONE-TIME exit: disable hover once, one
 *     invalidation, refresh the F21 placement readout once;
 *   - further non-finite samples while outside are complete no-ops;
 *   - the first finite sample re-enters exactly once (publish + enable +
 *     one invalidation);
 *   - NaN never reaches the stored last-sample pair;
 *   - finite on-globe behavior is unchanged (publish every tick; one
 *     invalidation per finite move).
 *
 * Header-only, allocation-free, no SDL / JavaScript / engine-state dependency:
 * native doctests exercise the real code
 * (tests/unit_tests/CalypsoGeoscapeHoverTransitionTest.cpp).
 */
#include <cmath>

#include "../fmath.h"

namespace OpenXcom
{
namespace Calypso
{

/// Persistent hover-transition state carried by the base-placement owner.
/// Entry defaults mirror BuildNewBaseState::init(), which enables hover with
/// the globe's zero position before the first pointer sample arrives.
struct CalypsoGlobeHoverTracker
{
	/// Whether rings are believed visible (globe hover enabled).
	bool onGlobe = true;
	/// Whether ANY finite sample has been accepted yet.
	bool hasFiniteSample = false;
	/// Last FINITE sample pair; never overwritten by NaN.
	double lastLon = 0.0;
	double lastLat = 0.0;
};

/// What the caller should do for this timer tick.
struct CalypsoGlobeHoverDecision
{
	/// Publish setNewBaseHoverPos + setNewBaseHover(true) from a finite sample.
	bool publishPosition = false;
	/// One-time exit: setNewBaseHover(false) to clear hover and its rings.
	bool disableHover = false;
	/// Semantic change or transition: request exactly one globe invalidation.
	bool invalidate = false;
	/// One-time exit: reset the F21 placement readout to its pending copy.
	bool refreshOutside = false;
};

/// A hover sample is usable only when BOTH coordinates are finite: NaN and
/// the infinities (which cartToPolar's unprojection can also produce for
/// degenerate inputs) all mean "pointer outside the Earth disk".
inline bool calypsoGlobeSampleFinite(double lon, double lat)
{
	return std::isfinite(lon) && std::isfinite(lat);
}

/// Decides the actions for one base-placement hover tick. Only FINITE
/// samples update the tracker's last-sample pair; a non-finite sample is a
/// state transition, not data.
inline CalypsoGlobeHoverDecision calypsoDecideBaseHover(
	CalypsoGlobeHoverTracker& tracker,
	bool sampleFinite, double sampleLon, double sampleLat)
{
	CalypsoGlobeHoverDecision d;
	if (!sampleFinite)
	{
		if (!tracker.onGlobe)
			return d; /* Already outside: strictly no repeated work. */
		/* One-time off-globe transition: clear rings/readout once. */
		tracker.onGlobe = false;
		d.disableHover = true;
		d.invalidate = true;
		d.refreshOutside = true;
		return d;
	}

	if (!tracker.onGlobe)
	{
		/* One-time re-entry: restore rings exactly once. */
		tracker.onGlobe = true;
		tracker.hasFiniteSample = true;
		tracker.lastLon = sampleLon;
		tracker.lastLat = sampleLat;
		d.publishPosition = true;
		d.invalidate = true;
		return d;
	}

	const bool moved = !tracker.hasFiniteSample
		|| !(AreSame(tracker.lastLat, sampleLat) && AreSame(tracker.lastLon, sampleLon));
	tracker.hasFiniteSample = true;
	tracker.lastLon = sampleLon;
	tracker.lastLat = sampleLat;
	d.publishPosition = true;
	d.invalidate = moved;
	return d;
}

} // namespace Calypso
} // namespace OpenXcom
