#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.1.1 (Calypso) -- pure viewport metrics and safe-area foundation.
 *
 * Header-only and dependency-free at the engine layer: only small standard
 * utility headers are used for clamping and the wide generation counter. There
 * are no SDL, browser, engine, YAML, or GL includes, so the native doctest
 * suite exercises the real formulas. Family adapters (46.1.2+), the resolution
 * floor (46.1.5), and the focus/scroll primitives (46.1.4) consume these
 * values. This slice performs no gameplay mutation, performs no per-frame
 * allocation, has no callers, and therefore leaves native OXCE behavior
 * byte-for-byte unchanged.
 *
 * Deliberately NOT wrapped in #ifdef __EMSCRIPTEN__: it is a pure helper with
 * no Emscripten-specific behavior, matching the established Calypso pure-helper
 * convention (CalypsoTrainingMath.h, CalypsoEconomyMath.h, ...) so it stays
 * unit-testable in native builds. Per-family adapters that touch SDL/engine
 * state will carry the whole-file guard when they land in later slices.
 *
 * All geometry is expressed in DPR-independent logical (CSS) pixels. The
 * safe-rectangle arithmetic is carried out in `long long` (see
 * calypsoSafeAxis) so any combination of `int` viewport/inset inputs --
 * including INT_MIN and INT_MAX -- is free of signed-integer-overflow UB.
 */
#include <algorithm>
#include <cstdint>

namespace OpenXcom
{
namespace Calypso
{

// --- Approved constants ----------------------------------------------------

/// Approved compact landscape floor (logical px). The resolution gate (46.1.5)
/// enforces this; the metrics helper only reports against it.
constexpr int CALYPSO_COMPACT_FLOOR_WIDTH  = 740;
constexpr int CALYPSO_COMPACT_FLOOR_HEIGHT = 360;

/// Minimum USABLE safe-area size (after insets) required for the Wide layout
/// class. BOTH dimensions must meet their threshold; otherwise the layout
/// stays Compact. Classification therefore reflects the space actually
/// available to widgets, not the raw framebuffer/viewport size.
constexpr int CALYPSO_WIDE_WIDTH_THRESHOLD  = 1024;
constexpr int CALYPSO_WIDE_HEIGHT_THRESHOLD = 600;

/// Minimum compact touch target (logical px) mandated by the approved contract.
/// Desktop proposals keep their larger approved targets; this is the floor every
/// primary compact control must meet.
constexpr int CALYPSO_MIN_TOUCH_TARGET = 44;

// --- Enums -----------------------------------------------------------------

/// Approved visual language for the screen that owns the current layout.
enum class CalypsoVisualContext { Strategic, Tactical };

/// Responsive layout class, selected from the USABLE safe-area size.
///   Compact -- 740x360 minimum and nearby landscape phones, or any viewport
///              whose safe area does not meet the Wide thresholds.
///   Wide    -- tablet and desktop (safe area >= 1024x600).
///   Portrait -- harness-only explicit request (tall phone composition QA).
///              calypsoClassifySafeArea never produces it; only an explicit
///              harness export can, so classification stays binary and
///              non-participating families treat the value as Compact.
enum class CalypsoLayoutClass { Compact, Wide, Portrait };

// --- Insets ----------------------------------------------------------------

/// Safe-area insets in DPR-independent logical px (notch / OS chrome). Negative
/// values are treated as 0; values that would push the safe rectangle out of
/// the viewport are clamped so the rectangle stays fully contained.
struct CalypsoSafeInsets
{
	int top    = 0;
	int right  = 0;
	int bottom = 0;
	int left   = 0;
};

// --- Metrics snapshot ------------------------------------------------------

/// Shared spacing scale, per layout class. Family adapters may override
/// individual values where an approved contract demands it.
struct CalypsoSpacingMetrics
{
	int small  = 0;
	int medium = 0;
	int large  = 0;
};

/// Immutable layout snapshot. Every geometric field is in DPR-independent
/// logical px.
///
/// Scale metadata describes how the USABLE safe rectangle fits the approved
/// compact reference:
///   scaleX = safeWidth  / 740   (referenceWidth)
///   scaleY = safeHeight / 360   (referenceHeight)
///   scale  = min(scaleX, scaleY)   -- the limiting axis
/// The denominators are nonzero constants, so a zero-area safe rect yields
/// 0.0 (never NaN/inf, never an integer divide-by-zero).
///
/// `rowHeight` is the default list-row content height. It is intentionally
/// distinct from `minTouchTarget`: dense roster rows may be shorter than 44px,
/// while every primary interactive control (including a tappable whole row,
/// when a family makes the row the activation affordance) must independently
/// meet `minTouchTarget`.
struct CalypsoLayoutMetrics
{
	// Effective logical viewport (raw inputs clamped to >= 0).
	int logicalWidth  = 0;
	int logicalHeight = 0;

	// Safe rectangle after insets. Fully contained in the viewport:
	//   0 <= safeX <= logicalWidth,  0 <= safeWidth  <= logicalWidth  - safeX
	//   0 <= safeY <= logicalHeight, 0 <= safeHeight <= logicalHeight - safeY
	int safeX      = 0;
	int safeY      = 0;
	int safeWidth  = 0;
	int safeHeight = 0;

	// Scale metadata, computed from the USABLE safe rectangle (not the raw
	// viewport) against the compact reference.
	int    referenceWidth  = CALYPSO_COMPACT_FLOOR_WIDTH;
	int    referenceHeight = CALYPSO_COMPACT_FLOOR_HEIGHT;
	double scaleX          = 0.0;   // safeWidth  / referenceWidth
	double scaleY          = 0.0;   // safeHeight / referenceHeight
	double scale           = 0.0;   // min(scaleX, scaleY)

	// Classification.
	CalypsoLayoutClass   layoutClass   = CalypsoLayoutClass::Compact;
	CalypsoVisualContext visualContext = CalypsoVisualContext::Strategic;

	// Shared family metrics (per layout class).
	int border           = 0;   // panel border thickness
	int headerHeight     = 0;   // title/context bar height
	int rowHeight        = 0;   // list-row content height (see note above)
	int actionBarHeight  = 0;   // persistent bottom action bar height
	int minTouchTarget   = CALYPSO_MIN_TOUCH_TARGET;  // always 44
	int detailPanelWidth = 0;   // 0 in Compact (stack/overlay); side panel in Wide

	CalypsoSpacingMetrics spacing;
};

/// Safe rectangle projected from CSS-logical coordinates into the engine's
/// current base framebuffer. This is the coordinate space used by State-owned
/// widgets and hit testing.
struct CalypsoBaseSafeRect
{
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
};

// --- Pure helpers ----------------------------------------------------------

/// Saturating increment for a 64-bit counter: returns v+1, or the maximum
/// value if v is already maximal. Never wraps past the maximum back to 0, so a
/// monotonic counter that has become nonzero stays nonzero forever. Exposed as
/// a free function so the boundary behavior is unit-testable directly.
inline std::uint64_t calypsoSaturatingIncrement(std::uint64_t v)
{
	// ~0 yields all-ones == UINT64_MAX without needing <climits>.
	const std::uint64_t maxValue = ~static_cast<std::uint64_t>(0);
	return (v >= maxValue) ? maxValue : (v + static_cast<std::uint64_t>(1));
}

/// Clamp to the nonnegative range.
inline int calypsoClampNonneg(int v) { return v < 0 ? 0 : v; }

inline void calypsoProjectSafeAxis(int safePos, int safeSize, int logicalExtent,
	                                int baseExtent, int* basePos, int* baseSize)
{
	const long long logical = calypsoClampNonneg(logicalExtent);
	const long long base = calypsoClampNonneg(baseExtent);
	if (logical == 0 || base == 0)
	{
		*basePos = 0;
		*baseSize = 0;
		return;
	}
	const long long start = std::min<long long>(logical, calypsoClampNonneg(safePos));
	const long long available = logical - start;
	const long long size = std::min<long long>(available, calypsoClampNonneg(safeSize));
	const long long end = start + size;
	const long long projectedStart = start * base / logical;
	const long long projectedEnd = end * base / logical;
	*basePos = static_cast<int>(projectedStart);
	*baseSize = static_cast<int>(projectedEnd - projectedStart);
}

/// Project a clamped CSS safe rectangle into base-framebuffer coordinates.
/// Integer endpoint projection keeps the result contained and makes the
/// no-inset case exactly equal to the full base framebuffer.
inline CalypsoBaseSafeRect calypsoProjectSafeRect(
	const CalypsoLayoutMetrics& metrics, int baseWidth, int baseHeight)
{
	CalypsoBaseSafeRect result;
	calypsoProjectSafeAxis(metrics.safeX, metrics.safeWidth, metrics.logicalWidth,
		baseWidth, &result.x, &result.width);
	calypsoProjectSafeAxis(metrics.safeY, metrics.safeHeight, metrics.logicalHeight,
		baseHeight, &result.y, &result.height);
	return result;
}

/// Uniformly fit an authored UI canvas inside a projected safe rectangle.
/// A supported device may still expose less temporary space while the virtual
/// keyboard is open, so scales below 1 are intentional and required.
inline double calypsoFitUiScale(const CalypsoBaseSafeRect& safe,
	int designWidth, int designHeight, double factor = 1.0)
{
	if (designWidth <= 0 || designHeight <= 0 || factor <= 0.0) return 0.0;
	const double fitX = static_cast<double>(safe.width) / designWidth;
	const double fitY = static_cast<double>(safe.height) / designHeight;
	const double scale = std::min(fitX, fitY) * factor;
	return scale > 0.0 ? scale : 0.0;
}

/// Compute a fully-contained safe axis. Given a leading inset (left/top), a
/// trailing inset (right/bottom), and the viewport extent (width/height) on
/// that axis, writes the safe origin and size with the invariants
///   0 <= *pos <= extent,  0 <= *size <= extent - *pos.
/// Negative insets are treated as 0; insets that would cross the opposite edge
/// are clamped to the remaining extent. All arithmetic is performed in
/// `long long` and every intermediate is bounded by `extent` (which is itself
/// in [0, INT_MAX]), so any combination of `int` inputs -- including INT_MIN
/// and INT_MAX -- is free of signed-integer-overflow UB, and the casts back to
/// `int` are always in range.
inline void calypsoSafeAxis(int leadingInset, int trailingInset, int extent,
                            int* pos, int* size)
{
	long long e   = (extent > 0) ? static_cast<long long>(extent) : 0;       // [0, INT_MAX]
	long long li  = (leadingInset  < 0) ? 0LL : static_cast<long long>(leadingInset);
	long long p   = (li > e) ? e : li;                                       // [0, e]
	long long rem = e - p;                                                   // [0, e]
	long long ti  = (trailingInset < 0) ? 0LL : static_cast<long long>(trailingInset);
	long long f   = (ti > rem) ? rem : ti;                                   // [0, rem]
	long long s   = rem - f;                                                 // [0, rem]
	*pos  = static_cast<int>(p);   // p in [0, e]   -> int range
	*size = static_cast<int>(s);   // s in [0, rem] -> int range
}

/// Select the layout class from the USABLE safe-area size (after insets).
inline CalypsoLayoutClass calypsoClassifySafeArea(int safeWidth, int safeHeight)
{
	return (safeWidth  >= CALYPSO_WIDE_WIDTH_THRESHOLD &&
	        safeHeight >= CALYPSO_WIDE_HEIGHT_THRESHOLD)
		? CalypsoLayoutClass::Wide
		: CalypsoLayoutClass::Compact;
}

/// Per-class panel border thickness.
inline int calypsoBorderFor(CalypsoLayoutClass c)
{
	return (c == CalypsoLayoutClass::Wide) ? 3 : 2;
}
/// Per-class header / context-bar height.
inline int calypsoHeaderHeightFor(CalypsoLayoutClass c)
{
	return (c == CalypsoLayoutClass::Wide) ? 40 : 28;
}
/// Per-class list-row content height. Does NOT replace the 44px touch floor.
inline int calypsoRowHeightFor(CalypsoLayoutClass c)
{
	return (c == CalypsoLayoutClass::Wide) ? 28 : 22;
}
/// Per-class persistent bottom action-bar height. Compact is sized to the
/// touch-target floor so the bar comfortably hosts 44px primary buttons.
inline int calypsoActionBarHeightFor(CalypsoLayoutClass c)
{
	return (c == CalypsoLayoutClass::Wide) ? 48 : 44;
}
/// Per-class side detail-panel width. Compact returns 0: compact layouts stack
/// or overlay detail instead of reserving a persistent side column.
inline int calypsoDetailPanelWidthFor(CalypsoLayoutClass c)
{
	return (c == CalypsoLayoutClass::Wide) ? 360 : 0;
}
/// Per-class spacing scale.
inline CalypsoSpacingMetrics calypsoSpacingFor(CalypsoLayoutClass c)
{
	return (c == CalypsoLayoutClass::Wide) ? CalypsoSpacingMetrics{6, 12, 20}
	                                      : CalypsoSpacingMetrics{4, 8, 12};
}

/// Recompute the immutable metrics snapshot for the given logical viewport,
/// safe-area insets, and visual context. Pure: no globals, no allocation.
inline CalypsoLayoutMetrics calypsoComputeMetrics(
	int logicalWidth, int logicalHeight,
	const CalypsoSafeInsets& insets,
	CalypsoVisualContext context)
{
	CalypsoLayoutMetrics m;
	m.visualContext = context;

	// Normalize the effective viewport: clamp negatives to 0.
	m.logicalWidth  = calypsoClampNonneg(logicalWidth);
	m.logicalHeight = calypsoClampNonneg(logicalHeight);

	// Safe rectangle: fully contained in the viewport and overflow-safe.
	calypsoSafeAxis(insets.left, insets.right, m.logicalWidth,
	                &m.safeX, &m.safeWidth);
	calypsoSafeAxis(insets.top, insets.bottom, m.logicalHeight,
	                &m.safeY, &m.safeHeight);

	// Class is derived from the USABLE safe-area size, not the raw viewport.
	m.layoutClass = calypsoClassifySafeArea(m.safeWidth, m.safeHeight);

	// Scale metadata: how the usable safe rect fits the compact reference.
	// Denominators are nonzero constants, so a zero-area safe rect yields 0.0
	// (never NaN/inf; never an integer divide-by-zero).
	m.referenceWidth  = CALYPSO_COMPACT_FLOOR_WIDTH;
	m.referenceHeight = CALYPSO_COMPACT_FLOOR_HEIGHT;
	m.scaleX = static_cast<double>(m.safeWidth)  / static_cast<double>(m.referenceWidth);
	m.scaleY = static_cast<double>(m.safeHeight) / static_cast<double>(m.referenceHeight);
	m.scale  = (m.scaleX <= m.scaleY) ? m.scaleX : m.scaleY;

	// Shared family metrics, derived from the class.
	m.border           = calypsoBorderFor(m.layoutClass);
	m.headerHeight     = calypsoHeaderHeightFor(m.layoutClass);
	m.rowHeight        = calypsoRowHeightFor(m.layoutClass);
	m.actionBarHeight  = calypsoActionBarHeightFor(m.layoutClass);
	m.minTouchTarget   = CALYPSO_MIN_TOUCH_TARGET;
	m.detailPanelWidth = calypsoDetailPanelWidthFor(m.layoutClass);
	m.spacing          = calypsoSpacingFor(m.layoutClass);
	return m;
}

// --- Stateful owner with monotonic generation ------------------------------

/// Owns the current layout snapshot and a monotonic generation counter.
///
/// Recomputes only when called (construction, state init, resize, or an
/// explicit content/variant change) -- never per frame, and never allocating
/// during read access.
///
/// The generation is a 64-bit counter. It is 0 before the first recompute and
/// becomes nonzero (>= 1) on the first recompute; every later change advances
/// it via calypsoSaturatingIncrement, so it is strictly nondecreasing and,
/// once nonzero, can never wrap back to 0 (it saturates at UINT64_MAX instead).
/// It bumps ONLY when the effective geometry, visual context, or derived
/// layout class actually changes; re-applying identical inputs is a no-op and
/// leaves the generation (and snapshot) untouched, so resize, focus, scroll,
/// and tutorial-anchor code can reject stale geometry by comparing generations.
class CalypsoViewportMetrics
{
public:
	CalypsoViewportMetrics() = default;

	/// Recompute from new inputs. Returns true iff the effective layout
	/// (viewport, safe rectangle, context, class) changed and the generation
	/// was bumped; false for an identical re-apply.
	bool recompute(int logicalWidth, int logicalHeight,
	               const CalypsoSafeInsets& insets,
	               CalypsoVisualContext context)
	{
		const CalypsoLayoutMetrics next =
			calypsoComputeMetrics(logicalWidth, logicalHeight, insets, context);

		// The very first recompute always establishes a layout (even for a
		// degenerate 0x0 viewport), so generation becomes nonzero. After that,
		// an identical effective layout is a stable no-op.
		if (_set && identicalLayout(_current, next))
			return false;

		_current = next;
		_set = true;
		_generation = calypsoSaturatingIncrement(_generation);  // saturates, never wraps
		return true;
	}

	/// Current immutable snapshot. Read access performs no allocation.
	const CalypsoLayoutMetrics& current() const { return _current; }

	/// Monotonic 64-bit generation. 0 before the first recompute; >= 1
	/// afterwards. Strictly nondecreasing; saturates at the maximum and never
	/// wraps. Bumps only on an effective layout change.
	std::uint64_t generation() const { return _generation; }

	/// True once at least one recompute has established a layout.
	bool hasLayout() const { return _set; }

private:
	/// Two snapshots describe the same effective layout iff every field that
	/// determines rendered geometry is equal. The scale fields (scaleX/scaleY/
	/// scale) and the per-class metrics (border/header/row/action-bar/detail/
	/// spacing) are deterministic functions of the safe rectangle + class, so
	/// equality of the geometry/context/class tuple below already implies them;
	/// the per-class metrics are compared too for explicit, self-documenting
	/// safety against accidental future drift in the per-class constants.
	static bool identicalLayout(const CalypsoLayoutMetrics& a, const CalypsoLayoutMetrics& b)
	{
		return a.logicalWidth  == b.logicalWidth
		    && a.logicalHeight == b.logicalHeight
		    && a.safeX          == b.safeX
		    && a.safeY          == b.safeY
		    && a.safeWidth      == b.safeWidth
		    && a.safeHeight     == b.safeHeight
		    && a.visualContext  == b.visualContext
		    && a.layoutClass    == b.layoutClass
		    && a.border           == b.border
		    && a.headerHeight     == b.headerHeight
		    && a.rowHeight        == b.rowHeight
		    && a.actionBarHeight  == b.actionBarHeight
		    && a.minTouchTarget   == b.minTouchTarget
		    && a.detailPanelWidth == b.detailPanelWidth
		    && a.spacing.small    == b.spacing.small
		    && a.spacing.medium   == b.spacing.medium
		    && a.spacing.large    == b.spacing.large;
	}

	CalypsoLayoutMetrics _current;
	std::uint64_t        _generation = 0;
	bool                 _set = false;
};

} // namespace Calypso
} // namespace OpenXcom
