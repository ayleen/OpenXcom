#pragma once
/*
 * Phase 6b — Per-frame surface arena.
 *
 * HD composite code creates temporary SDL_Surfaces during Map::drawTerrain
 * (pre-shaded body parts, recoloured composites, final whole-unit buffers).
 * Those surfaces must outlive drawTerrain and survive until Screen::flip
 * consumes them via HDQueue::flush().
 *
 * Ownership contract:
 *   1. Map::drawTerrain calls frameArena().reset() at the TOP of each frame.
 *   2. HD composite code calls frameArena().alloc(w, h) to obtain an
 *      arena-owned ARGB surface; the raw pointer is safe to push into
 *      HDSpriteBatch / HDQueue.
 *   3. Screen::flip calls frameArena().reset() AFTER HDQueue::flush().
 *      Violating this order dangling-pointers the queue entries.
 *
 * Intentionally unused in non-Emscripten builds.
 */
#ifdef __EMSCRIPTEN__

#include "Surface.h"
#include <vector>

namespace OpenXcom
{

class FrameArena
{
public:
	/// Allocate a zeroed ARGB8888 surface owned by the arena.
	/// Returns a raw pointer valid until the next reset().
	SDL_Surface *alloc(int w, int h);

	/// Release all owned surfaces. MUST be called after HDQueue::flush().
	void reset();

private:
	std::vector<Surface::UniqueSurfacePtr> _owned;
};

/// Global per-frame arena — shared by Map::drawTerrain and Screen::flip.
FrameArena &frameArena();

} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */
