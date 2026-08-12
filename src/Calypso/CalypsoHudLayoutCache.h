/*
 * Calypso — HD HUD layout invalidation cache.
 *
 * Pure header (no engine dependencies) so it can be unit-tested natively
 * (tests/unit_tests/CalypsoHudLayoutCacheTest.cpp) AND consumed by the
 * Emscripten-only BattlescapeState::layoutHudGl() without duplicating the
 * invalidation policy.
 *
 * Bug anchor (GitHub PR ayleen/calypso#78, comment 4986445239 / P2): the
 * original layout cache keyed only on Options::baseXResolution
 * (_hudLastBaseX). A width-preserving base-HEIGHT resize therefore left
 * panelY, Map hudTopY/scissor, the BattlescapeButton transform, and the
 * portrait/rank/name/stat GL overlay rectangles stale. The cache now keys on
 * BOTH base width and base height:
 *   - first layout (either cached dim == -1) rebuilds,
 *   - any change in either dimension rebuilds,
 *   - identical dimensions are a no-op.
 */
#ifndef CALYPSO_HUD_LAYOUT_CACHE_H
#define CALYPSO_HUD_LAYOUT_CACHE_H

namespace OpenXcom
{

struct CalypsoHudLayoutCache
{
	int lastBaseX = -1;   // last laid-out base width; -1 = never laid out
	int lastBaseY = -1;   // last laid-out base HEIGHT; -1 = never laid out

	/// True when the HD HUD must be re-laid-out for (baseX, baseY):
	///   - first layout (either cached dim still -1), or
	///   - either dimension differs from what was last recorded.
	/// Identical dimensions return false (no-op).
	bool needsRebuild(int baseX, int baseY) const
	{
		return lastBaseX == -1 || lastBaseY == -1
			|| lastBaseX != baseX || lastBaseY != baseY;
	}

	/// Record the dimensions that were just laid out. Call AFTER a rebuild
	/// (or unconditionally at the top of layoutHudGl once needsRebuild passed).
	void record(int baseX, int baseY)
	{
		lastBaseX = baseX;
		lastBaseY = baseY;
	}
};

} // namespace OpenXcom

#endif // CALYPSO_HUD_LAYOUT_CACHE_H
