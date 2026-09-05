#pragma once
/*
 * Phase 46.4 Stage 8/9 closure (Calypso) -- generation-invalidated live model
 * snapshot cache.
 *
 * The live strategic renderer previously rebuilt the full runtime model in
 * completeFrameReady, retryableReadiness, collectLogicalSuppression, and
 * collect -- several vector/string-building passes per frame for one logical
 * state. This cache owns ONE snapshot per consumer state and rebuilds it only
 * when a keyed input actually changed.
 *
 * Pure and engine-independent: native doctests exercise the invalidation
 * matrix; the Emscripten renderer supplies the key from live state. No SDL,
 * no GL, no allocations on the reuse path.
 */
#include <cstddef>
#include <cstdint>

namespace OpenXcom
{
namespace Calypso
{

/// Every input the live strategic-shell snapshot depends on. Text fields are
/// monotonic content generations from the owning Text objects (bumped only
/// when setText() stores different content), so an in-place change
/// invalidates while steady-state frames compare plain integers -- no
/// per-frame allocation, no string reads, no hashing. Actual text is read
/// only inside rebuild().
struct CalypsoGeoscapeHdSnapshotKey
{
	std::uint64_t viewportGeneration = 0; ///< layout/resize/DPR/safe-area/context-of-viewport
	std::uint64_t contextGeneration = 0;  ///< GL context generation
	bool fundsVisible = false;            ///< funds owner visibility gate
	bool drawerOpen = false;              ///< More/session drawer state
	bool extendedLinks = false;           ///< oxceLinks availability gate
	bool debugOption = false;             ///< debug availability gate
	bool ironman = false;                 ///< non-ironman save-row gate
	const void* selectedSpeed = nullptr;  ///< selected speed widget identity
	std::uint64_t hourTextGeneration = 0;
	std::uint64_t minuteTextGeneration = 0;
	std::uint64_t dayTextGeneration = 0;
	std::uint64_t monthTextGeneration = 0;
	std::uint64_t yearTextGeneration = 0;
	std::uint64_t fundsTextGeneration = 0;
};

inline bool operator==(const CalypsoGeoscapeHdSnapshotKey& a,
	const CalypsoGeoscapeHdSnapshotKey& b)
{
	return a.viewportGeneration == b.viewportGeneration
		&& a.contextGeneration == b.contextGeneration
		&& a.fundsVisible == b.fundsVisible
		&& a.drawerOpen == b.drawerOpen
		&& a.extendedLinks == b.extendedLinks
		&& a.debugOption == b.debugOption
		&& a.ironman == b.ironman
		&& a.selectedSpeed == b.selectedSpeed
		&& a.hourTextGeneration == b.hourTextGeneration
		&& a.minuteTextGeneration == b.minuteTextGeneration
		&& a.dayTextGeneration == b.dayTextGeneration
		&& a.monthTextGeneration == b.monthTextGeneration
		&& a.yearTextGeneration == b.yearTextGeneration
		&& a.fundsTextGeneration == b.fundsTextGeneration;
}

inline bool operator!=(const CalypsoGeoscapeHdSnapshotKey& a,
	const CalypsoGeoscapeHdSnapshotKey& b)
{
	return !(a == b);
}

/// One state-owned snapshot: rebuilds via `rebuild()` exactly when `key`
/// differs from the cached generation. Teardown destroys the whole cache
/// (it lives inside the per-state renderer); invalidate() marks an explicit
/// lifecycle boundary cold.
template <typename Model>
class CalypsoGeoscapeHdSnapshotCache
{
public:
	template <typename Rebuild>
	const Model& current(const CalypsoGeoscapeHdSnapshotKey& key, Rebuild&& rebuild)
	{
		if (_valid && _key == key) return _model;
		_model = rebuild();
		_key = key;
		_valid = true;
		++_rebuilds;
		return _model;
	}

	void invalidate() { _valid = false; }

	/// Observed rebuild count (diagnostic/test oracle for steady-state reuse).
	std::size_t rebuildCount() const { return _rebuilds; }

private:
	CalypsoGeoscapeHdSnapshotKey _key;
	Model _model;
	bool _valid = false;
	std::size_t _rebuilds = 0;
};

} // namespace Calypso
} // namespace OpenXcom
