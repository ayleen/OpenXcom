#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- portable HD-metrics viewport model.
 *
 * Single authority for the physical canvas backing-store size, CSS/logical
 * size, safe-area insets, device-pixel ratio, and orientation used by the HD
 * UI overlay. Two producers feed it:
 *
 *   1. the JS bridge observation (calypso_notify_viewport_observation_v1),
 *      authoritative for every VALID field, carrying a validity mask plus a
 *      monotonic revision so a zero value is distinguishable from an absent
 *      one and stale/out-of-order deliveries can be rejected;
 *   2. a once-per-frame backing-store poll, authoritative for the PHYSICAL
 *      canvas dimensions only -- it covers canvas changes that arrive with no
 *      browser event.
 *
 * Raw SDL window probes are deliberately NOT a producer here (see plan
 * "Simplified viewport model"). The model bumps its generation only on an
 * effective value change; duplicate deliveries are no-ops.
 *
 * Header-only and dependency-free (only <algorithm>/<cstdint>/<cmath>): no
 * SDL, browser, engine, YAML, or GL includes, so the native doctest suite
 * exercises the real merge logic. Deliberately NOT wrapped in
 * #ifdef __EMSCRIPTEN__, matching the established Calypso pure-helper
 * convention (CalypsoUiMetrics.h, CalypsoPrologueMath.h, ...). The Emscripten
 * bridge (CalypsoViewportMailbox) owns one instance; native OXCE behavior is
 * left byte-for-byte unchanged because this slice has no native callers.
 */
#include <algorithm>
#include <cstdint>
#include <cmath>

namespace OpenXcom
{
namespace Calypso
{

/// Which fields of an observation carry meaningful data. A field whose bit is
/// false is ignored by the merge (its numeric value is not consulted), which
/// is how a legitimate zero is distinguished from "not reported".
struct CalypsoViewportValidity
{
	bool physicalSize = false;
	bool logicalSize  = false;
	bool safeArea     = false;
	bool dpr          = false;
	bool orientation  = false;
};

/// Coarse orientation. Derived from logical (falling back to physical) size
/// when an observation does not report it explicitly.
enum class CalypsoOrientation { Unknown, Portrait, Landscape };

/// A single observation delivered by the JS bridge. `revision` is monotonic
/// per JS runtime; the model rejects any observation whose revision is not
/// strictly greater than the last accepted one.
struct CalypsoViewportObservation
{
	std::uint64_t revision = 0;
	CalypsoViewportValidity valid;
	int physicalWidth  = 0;
	int physicalHeight = 0;
	int logicalWidth   = 0;
	int logicalHeight  = 0;
	int safeTop    = 0;
	int safeRight  = 0;
	int safeBottom = 0;
	int safeLeft   = 0;
	double dpr = 1.0;
	CalypsoOrientation orientation = CalypsoOrientation::Unknown;
};

/// The merged, current HD viewport state. Every geometric field is clamped
/// non-negative; DPR is clamped to a sane positive range.
struct CalypsoViewportState
{
	int physicalWidth  = 0;
	int physicalHeight = 0;
	int logicalWidth   = 0;
	int logicalHeight  = 0;
	int safeTop    = 0;
	int safeRight  = 0;
	int safeBottom = 0;
	int safeLeft   = 0;
	double dpr = 1.0;
	CalypsoOrientation orientation = CalypsoOrientation::Unknown;
	bool hasPhysicalSize = false;
	bool hasLogicalSize  = false;
};

// --- Small pure helpers ----------------------------------------------------

/// Clamp an int to >= 0 without signed-overflow UB.
inline int calypsoViewportClampNonneg(int v) { return v < 0 ? 0 : v; }

/// Clamp DPR into (0, 8]. Anything <= 0, NaN, or absurdly large collapses to
/// 1.0 (a wrong-but-safe density beats a divide-by-zero or a runaway raster).
inline double calypsoViewportClampDpr(double dpr)
{
	if (!(dpr > 0.0) || dpr > 8.0) return 1.0; // !(>0) also catches NaN
	return dpr;
}

/// Derive orientation from a width/height pair (landscape iff width >= height,
/// with a positive area). Zero-area yields Unknown.
inline CalypsoOrientation calypsoOrientationFromSize(int w, int h)
{
	if (w <= 0 || h <= 0) return CalypsoOrientation::Unknown;
	return (w >= h) ? CalypsoOrientation::Landscape : CalypsoOrientation::Portrait;
}

// --- Model -----------------------------------------------------------------

class CalypsoViewportModel
{
public:
	/// Apply a JS-bridge observation. Authoritative for every VALID field.
	/// Rejects (returns false, no mutation) an observation whose revision is
	/// not strictly greater than the last accepted revision. Otherwise records
	/// the revision and merges valid fields; returns true iff any effective
	/// state field changed (in which case the generation was bumped).
	bool applyObservation(const CalypsoViewportObservation& obs)
	{
		if (_hasObservation && obs.revision <= _lastRevision) return false;
		_lastRevision = obs.revision;
		_hasObservation = true;

		CalypsoViewportState next = _state;
		if (obs.valid.physicalSize)
		{
			next.physicalWidth  = calypsoViewportClampNonneg(obs.physicalWidth);
			next.physicalHeight = calypsoViewportClampNonneg(obs.physicalHeight);
			next.hasPhysicalSize = true;
		}
		if (obs.valid.logicalSize)
		{
			next.logicalWidth  = calypsoViewportClampNonneg(obs.logicalWidth);
			next.logicalHeight = calypsoViewportClampNonneg(obs.logicalHeight);
			next.hasLogicalSize = true;
		}
		if (obs.valid.safeArea)
		{
			next.safeTop    = calypsoViewportClampNonneg(obs.safeTop);
			next.safeRight  = calypsoViewportClampNonneg(obs.safeRight);
			next.safeBottom = calypsoViewportClampNonneg(obs.safeBottom);
			next.safeLeft   = calypsoViewportClampNonneg(obs.safeLeft);
		}
		if (obs.valid.dpr)
		{
			next.dpr = calypsoViewportClampDpr(obs.dpr);
		}
		if (obs.valid.orientation && obs.orientation != CalypsoOrientation::Unknown)
		{
			next.orientation = obs.orientation;
		}
		else
		{
			next.orientation = deriveOrientation(next);
		}
		return commit(next);
	}

	/// Apply a pre-frame backing-store poll. Authoritative for the PHYSICAL
	/// canvas dimensions ONLY; leaves logical size, safe area, DPR, and any
	/// explicitly-reported orientation untouched. Returns true iff the physical
	/// dimensions changed.
	bool applyBackingStorePoll(int physicalWidth, int physicalHeight)
	{
		CalypsoViewportState next = _state;
		next.physicalWidth  = calypsoViewportClampNonneg(physicalWidth);
		next.physicalHeight = calypsoViewportClampNonneg(physicalHeight);
		next.hasPhysicalSize = true;
		// A poll must not resurrect a derived orientation from stale physical
		// dims when the bridge already stated one; only re-derive if the
		// current orientation is Unknown.
		if (_state.orientation == CalypsoOrientation::Unknown)
		{
			next.orientation = deriveOrientation(next);
		}
		return commit(next);
	}

	const CalypsoViewportState& state() const { return _state; }
	std::uint64_t generation() const { return _generation; }
	std::uint64_t lastRevision() const { return _lastRevision; }
	bool hasObservation() const { return _hasObservation; }
	bool hasPhysicalSize() const { return _state.hasPhysicalSize; }

private:
	static CalypsoOrientation deriveOrientation(const CalypsoViewportState& s)
	{
		CalypsoOrientation o = calypsoOrientationFromSize(s.logicalWidth, s.logicalHeight);
		if (o == CalypsoOrientation::Unknown)
		{
			o = calypsoOrientationFromSize(s.physicalWidth, s.physicalHeight);
		}
		return o;
	}

	static bool equalState(const CalypsoViewportState& a, const CalypsoViewportState& b)
	{
		return a.physicalWidth == b.physicalWidth
		    && a.physicalHeight == b.physicalHeight
		    && a.logicalWidth == b.logicalWidth
		    && a.logicalHeight == b.logicalHeight
		    && a.safeTop == b.safeTop && a.safeRight == b.safeRight
		    && a.safeBottom == b.safeBottom && a.safeLeft == b.safeLeft
		    && a.dpr == b.dpr
		    && a.orientation == b.orientation
		    && a.hasPhysicalSize == b.hasPhysicalSize
		    && a.hasLogicalSize == b.hasLogicalSize;
	}

	/// Commit a candidate state. Bumps the generation and returns true only if
	/// the effective state actually changed; duplicates are silent no-ops.
	bool commit(const CalypsoViewportState& next)
	{
		if (equalState(next, _state)) return false;
		_state = next;
		++_generation;
		return true;
	}

	CalypsoViewportState _state;
	std::uint64_t _generation = 0;
	std::uint64_t _lastRevision = 0;
	bool _hasObservation = false;
};

} // namespace Calypso
} // namespace OpenXcom
