#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- portable HD UI overlay model.
 *
 * The dependency-free heart of the shared physical-resolution overlay:
 *   - the frozen per-frame presentation metrics and the logical->physical
 *     edge mapping / clipping used by every family adapter;
 *   - the atomic-subgroup readiness machine (Ready/Warming/Unavailable/Failed
 *     with worst-wins aggregation);
 *   - the frame-scoped logical-claim identity and claim set that let a widget
 *     skip exactly the visual the overlay has taken over this frame;
 *   - the deterministic ordering key (stage -> groupOrder -> stableId ->
 *     instanceKey -> subgroupOrder -> itemOrder) plus complete-tuple collision
 *     detection.
 *
 * No SDL, browser, engine, GL, or allocation-heavy dependency (only
 * <algorithm>/<cstdint>/<cmath>/<vector> for the small helpers), so the native
 * doctest suite exercises the real math and state machine. Not wrapped in
 * #ifdef __EMSCRIPTEN__, matching the Calypso pure-helper convention; it has
 * no native callers and therefore leaves native OXCE behavior unchanged.
 */
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <vector>

namespace OpenXcom
{
namespace Calypso
{
/// Reusable SDF silhouettes for styled panels. RoundedRect preserves the
/// existing family behavior; F33 uses OpposingCutRect for its command frame
/// and WarningTriangle for the small amber caution glyph.
enum class CalypsoHdPanelShape { RoundedRect = 0, OpposingCutRect = 1, WarningTriangle = 2 };


/// Optional styling for a Panel item, rendered by the hd_ui_panel SDF shader.
/// All px values are DESIGN-space (the same space as `rect`) and scale with
/// the logical->physical mapping. A style with `styled == false` (default)
/// keeps the plain tinted-quad path. Colours are packed 0xRRGGBBAA.
struct CalypsoHdPanelStyle
{
	bool styled = false;
	CalypsoHdPanelShape shape = CalypsoHdPanelShape::RoundedRect;
	float radiusPx = 0.0f;          // rounded-corner radius
	float cutCornerPx = 0.0f;       // opposing cut size (top-left/bottom-right)
	float borderWidthPx = 0.0f;     // ring thickness at the shape edge
	std::uint32_t borderColorRgba = 0;
	std::uint32_t fillTopRgba = 0;  // gradient stop at the grad direction origin
	std::uint32_t fillBottomRgba = 0;
	float gradDirX = 0.26f;         // gradient direction (normalized-ish);
	float gradDirY = 1.0f;          // default ~165deg-like downward drift
	std::uint32_t glowRgba = 0;     // soft outer falloff colour (alpha = strength)
	float glowRadiusPx = 0.0f;      // 0 => no glow
};

// --- Presentation metrics + logical/physical mapping -----------------------

/// A logical (layout-grid) rectangle. Edges, not width*scale, are mapped so
/// adjacent rectangles tile without seams or overlaps at physical resolution.
struct CalypsoLogicalRect
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

/// A physical (device-pixel) rectangle produced by the mapping.
struct CalypsoPhysRect
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;

	bool empty() const { return w <= 0 || h <= 0; }
};

/// The ONE frozen presentation snapshot per presentable frame. Nothing after
/// Screen::finalizePresentationMetrics() may remix these values; every claimed
/// destination derives from them. Calypso presents a stretched canvas, so
/// scaleX and scaleY are independent; content offsets support an
/// aspect-preserving (letterboxed) presentation should one ever be added.
struct CalypsoHdPresentationMetrics
{
	int logicalWidth   = 0;
	int logicalHeight  = 0;
	int physicalWidth  = 0;
	int physicalHeight = 0;
	double scaleX = 1.0;
	double scaleY = 1.0;
	int contentOffsetX = 0;
	int contentOffsetY = 0;
	double dpr = 1.0;
	std::uint64_t generation = 0;

	bool valid() const
	{
		return logicalWidth > 0 && logicalHeight > 0
		    && physicalWidth > 0 && physicalHeight > 0;
	}
};

/// Round to nearest int, ties toward +infinity (floor(v + 0.5)). The tie
/// direction is unimportant; what matters for edge mapping is that it is
/// monotonic non-decreasing, so abutting logical edges never cross after
/// rounding. Overflow-guarded to the int range so a degenerate scale can
/// never produce UB.
inline int calypsoHdRoundToInt(double v)
{
	if (!(v == v)) return 0; // NaN
	double r = std::floor(v + 0.5);
	if (r <= -2147483648.0) return -2147483647 - 1;
	if (r >= 2147483647.0) return 2147483647;
	return static_cast<int>(r);
}

/// Keep a physical text projection coupled to the actual edge-mapped opening
/// extent of its destination rectangle. Using the ideal floating-point motion
/// factor is insufficient because independent edge rounding can make the
/// animated physical box one pixel smaller than that factor predicts.
inline double calypsoHdMotionProjectionScale(
	double projectionScale, int restingPhysicalExtent, int animatedPhysicalExtent)
{
	if (restingPhysicalExtent <= 0 || animatedPhysicalExtent <= 0)
		return projectionScale;
	return projectionScale * animatedPhysicalExtent / restingPhysicalExtent;
}

/// Build stretched-canvas metrics: scaleX = physW/logW, scaleY = physH/logH,
/// zero content offset. Returns an invalid (all-default-scale) snapshot when
/// any dimension is non-positive, so callers can gate on valid().
inline CalypsoHdPresentationMetrics calypsoMakeStretchMetrics(
	int logicalWidth, int logicalHeight,
	int physicalWidth, int physicalHeight,
	double dpr, std::uint64_t generation)
{
	CalypsoHdPresentationMetrics m;
	m.logicalWidth   = logicalWidth  < 0 ? 0 : logicalWidth;
	m.logicalHeight  = logicalHeight < 0 ? 0 : logicalHeight;
	m.physicalWidth  = physicalWidth  < 0 ? 0 : physicalWidth;
	m.physicalHeight = physicalHeight < 0 ? 0 : physicalHeight;
	m.dpr = (dpr > 0.0 && dpr <= 8.0) ? dpr : 1.0;
	m.generation = generation;
	if (m.logicalWidth > 0)  m.scaleX = static_cast<double>(m.physicalWidth)  / m.logicalWidth;
	if (m.logicalHeight > 0) m.scaleY = static_cast<double>(m.physicalHeight) / m.logicalHeight;
	return m;
}

/// Saturate a 64-bit value to the int range (no signed-overflow UB).
inline int calypsoSatI64ToInt(long long v)
{
	if (v <= -2147483648LL) return -2147483647 - 1;
	if (v >=  2147483647LL) return  2147483647;
	return static_cast<int>(v);
}

/// Map a logical rectangle to physical device pixels by mapping each edge
/// independently and differencing, so that abutting logical rectangles stay
/// gap-free after rounding. Content offsets are applied in physical space.
/// All post-round arithmetic (offset add, edge difference) is done in 64-bit
/// and saturated back to int, so even a degenerate scale/offset can never
/// produce signed-overflow UB -- the helper's documented overflow-safety
/// guarantee is proven by the INT_MIN/INT_MAX doctest cases.
inline CalypsoPhysRect calypsoMapLogicalRect(
	const CalypsoLogicalRect& r, const CalypsoHdPresentationMetrics& m)
{
	const double left   = r.x;
	const double right  = static_cast<double>(r.x) + r.w;
	const double top    = r.y;
	const double bottom = static_cast<double>(r.y) + r.h;
	const long long ox = m.contentOffsetX;
	const long long oy = m.contentOffsetY;
	const long long px0 = static_cast<long long>(calypsoHdRoundToInt(left   * m.scaleX)) + ox;
	const long long px1 = static_cast<long long>(calypsoHdRoundToInt(right  * m.scaleX)) + ox;
	const long long py0 = static_cast<long long>(calypsoHdRoundToInt(top    * m.scaleY)) + oy;
	const long long py1 = static_cast<long long>(calypsoHdRoundToInt(bottom * m.scaleY)) + oy;
	CalypsoPhysRect out;
	out.x = calypsoSatI64ToInt(px0);
	out.y = calypsoSatI64ToInt(py0);
	out.w = calypsoSatI64ToInt(px1 - px0);
	out.h = calypsoSatI64ToInt(py1 - py0);
	return out;
}

/// Pack 8-bit RGBA channels into the canonical 0xRRGGBBAA word the HD text
/// rasteriser unpacks (R=v>>24, G=v>>16, B=v>>8, A=v). Use this for every
/// theme colour literal so the byte order can never silently drift to ARGB.
constexpr std::uint32_t calypsoRgba(std::uint8_t r, std::uint8_t g,
	std::uint8_t b, std::uint8_t a = 255)
{
	return (static_cast<std::uint32_t>(r) << 24)
	     | (static_cast<std::uint32_t>(g) << 16)
	     | (static_cast<std::uint32_t>(b) << 8)
	     |  static_cast<std::uint32_t>(a);
}

/// Intersect two physical rectangles. Returns false (and leaves `out`
/// unspecified) if the intersection is empty.
inline bool calypsoClipPhysRect(
	const CalypsoPhysRect& r, const CalypsoPhysRect& clip, CalypsoPhysRect& out)
{
	const long long rx0 = r.x,    rx1 = static_cast<long long>(r.x) + r.w;
	const long long ry0 = r.y,    ry1 = static_cast<long long>(r.y) + r.h;
	const long long cx0 = clip.x, cx1 = static_cast<long long>(clip.x) + clip.w;
	const long long cy0 = clip.y, cy1 = static_cast<long long>(clip.y) + clip.h;
	const long long ix0 = std::max(rx0, cx0);
	const long long iy0 = std::max(ry0, cy0);
	const long long ix1 = std::min(rx1, cx1);
	const long long iy1 = std::min(ry1, cy1);
	if (ix1 <= ix0 || iy1 <= iy0) return false;
	out.x = static_cast<int>(ix0);
	out.y = static_cast<int>(iy0);
	out.w = static_cast<int>(ix1 - ix0);
	out.h = static_cast<int>(iy1 - iy0);
	return true;
}

// --- Readiness machine -----------------------------------------------------

/// Readiness of a single item or of an atomic subgroup. Only a fully `Ready`
/// subgroup submits physical items and creates claims; anything else renders
/// the complete logical fallback for that subgroup.
///   Warming     -- caches still filling; retry next frame.
///   Unavailable -- structurally cannot go physical this checkpoint (e.g.
///                  contains an unsupported TOK_NL_SMALL run, or a missing
///                  font); render logical, do not spin.
///   Failed      -- an operation failed (raster/upload); render logical.
enum class CalypsoHdReadiness { Ready, Warming, Unavailable, Failed };

/// Worst-wins precedence for aggregating item readiness into a subgroup.
/// Failed > Unavailable > Warming > Ready. An empty item list is Unavailable
/// (a subgroup with nothing to show never claims).
inline int calypsoReadinessRank(CalypsoHdReadiness r)
{
	switch (r)
	{
	case CalypsoHdReadiness::Ready:       return 0;
	case CalypsoHdReadiness::Warming:     return 1;
	case CalypsoHdReadiness::Unavailable: return 2;
	case CalypsoHdReadiness::Failed:      return 3;
	}
	return 3;
}

inline CalypsoHdReadiness calypsoAggregateReadiness(const std::vector<CalypsoHdReadiness>& items)
{
	if (items.empty()) return CalypsoHdReadiness::Unavailable;
	CalypsoHdReadiness worst = CalypsoHdReadiness::Ready;
	for (CalypsoHdReadiness r : items)
	{
		if (calypsoReadinessRank(r) > calypsoReadinessRank(worst)) worst = r;
	}
	return worst;
}

inline bool calypsoSubgroupSubmits(CalypsoHdReadiness aggregate)
{
	return aggregate == CalypsoHdReadiness::Ready;
}

// --- Frame-scoped logical claims -------------------------------------------

/// Complete owner/instance identity of a single logical visual the overlay has
/// taken over. Every field participates in identity so two simultaneous popups
/// of the same type (distinct instanceKey) never alias each other's claims.
struct CalypsoHdClaimId
{
	std::uint32_t familyId        = 0;
	std::uint32_t stableId        = 0;
	std::uint64_t instanceKey     = 0;
	std::uint32_t stableSubgroupId = 0;
	std::uint32_t stableVisualId  = 0;

	bool operator==(const CalypsoHdClaimId& o) const
	{
		return familyId == o.familyId
		    && stableId == o.stableId
		    && instanceKey == o.instanceKey
		    && stableSubgroupId == o.stableSubgroupId
		    && stableVisualId == o.stableVisualId;
	}
	bool operator!=(const CalypsoHdClaimId& o) const { return !(*this == o); }

	/// Strict-weak ordering, lexicographic over the identity tuple.
	bool operator<(const CalypsoHdClaimId& o) const
	{
		if (familyId != o.familyId) return familyId < o.familyId;
		if (stableId != o.stableId) return stableId < o.stableId;
		if (instanceKey != o.instanceKey) return instanceKey < o.instanceKey;
		if (stableSubgroupId != o.stableSubgroupId) return stableSubgroupId < o.stableSubgroupId;
		return stableVisualId < o.stableVisualId;
	}
};

/// Frame-scoped claim registry. Claims start empty each frame (beginFrame),
/// are recreated only by Ready subgroups (add), and a query only reports a
/// visual as claimed when the query's frame id matches the current frame --
/// so a stale claim from a previous frame can never suppress a logical visual.
class CalypsoHdClaimSet
{
public:
	/// Open a new frame: clears all claims and records the frame id.
	void beginFrame(std::uint64_t frameId)
	{
		_frameId = frameId;
		_claims.clear();
	}

	/// Register a claim for the current frame. Idempotent per id.
	void add(const CalypsoHdClaimId& id)
	{
		// Small N (visuals in the top state); linear insert keeps it allocation
		// -light and ordered for deterministic iteration/debugging.
		auto it = std::lower_bound(_claims.begin(), _claims.end(), id);
		if (it == _claims.end() || !(*it == id)) _claims.insert(it, id);
	}

	/// True iff `id` is claimed AND `frameId` is the current frame. A mismatched
	/// frame id always returns false (fail-safe to logical rendering).
	bool claimsLogical(const CalypsoHdClaimId& id, std::uint64_t frameId) const
	{
		if (frameId != _frameId) return false;
		return std::binary_search(_claims.begin(), _claims.end(), id);
	}

	/// Discard all claims for the current frame (post-claim failure path).
	void clear() { _claims.clear(); }

	std::uint64_t frameId() const { return _frameId; }
	std::size_t size() const { return _claims.size(); }
	bool empty() const { return _claims.empty(); }

private:
	std::vector<CalypsoHdClaimId> _claims;
	std::uint64_t _frameId = 0;
};

// --- Deterministic ordering + collision detection --------------------------

/// The three post-composite overlay stages, in draw order. HD UI draws above
/// the legacy composite; diagnostics (FPS) above HD UI; the pointer last.
enum class CalypsoHdStage { HdUi = 0, Diagnostics = 1, Pointer = 2 };

/// Complete ordering tuple for one submitted item. Ordering is purely by these
/// keys -- never by submission/registration order. Two items sharing the whole
/// tuple are a submission error (calypsoOrderKeyCollides), not resolved by
/// insertion order.
struct CalypsoHdOrderKey
{
	int stage = 0;            // CalypsoHdStage as int
	int groupOrder = 0;       // signed; adapters space these out
	std::uint32_t stableId = 0;
	std::uint64_t instanceKey = 0;
	int subgroupOrder = 0;
	std::uint32_t subgroupId = 0;
	int itemOrder = 0;
	std::uint32_t itemId = 0;

	bool operator==(const CalypsoHdOrderKey& o) const
	{
		return stage == o.stage && groupOrder == o.groupOrder
		    && stableId == o.stableId && instanceKey == o.instanceKey
		    && subgroupOrder == o.subgroupOrder && subgroupId == o.subgroupId
		    && itemOrder == o.itemOrder && itemId == o.itemId;
	}
};

/// Strict-weak lexicographic ordering over the full tuple.
inline bool calypsoOrderKeyLess(const CalypsoHdOrderKey& a, const CalypsoHdOrderKey& b)
{
	if (a.stage != b.stage) return a.stage < b.stage;
	if (a.groupOrder != b.groupOrder) return a.groupOrder < b.groupOrder;
	if (a.stableId != b.stableId) return a.stableId < b.stableId;
	if (a.instanceKey != b.instanceKey) return a.instanceKey < b.instanceKey;
	if (a.subgroupOrder != b.subgroupOrder) return a.subgroupOrder < b.subgroupOrder;
	if (a.subgroupId != b.subgroupId) return a.subgroupId < b.subgroupId;
	if (a.itemOrder != b.itemOrder) return a.itemOrder < b.itemOrder;
	return a.itemId < b.itemId;
}

/// True iff two order keys are identical over the complete tuple (a collision
/// that must be rejected at submission).
inline bool calypsoOrderKeyCollides(const CalypsoHdOrderKey& a, const CalypsoHdOrderKey& b)
{
	return a == b;
}

/// Sort a vector of order keys deterministically and report whether any
/// complete-tuple collision exists. On collision the caller must reject the
/// submission rather than pick a winner by insertion order.
inline bool calypsoSortAndDetectCollision(std::vector<CalypsoHdOrderKey>& keys)
{
	std::sort(keys.begin(), keys.end(), calypsoOrderKeyLess);
	for (std::size_t i = 1; i < keys.size(); ++i)
	{
		if (keys[i - 1] == keys[i]) return true; // collision
	}
	return false;
}

} // namespace Calypso
} // namespace OpenXcom
