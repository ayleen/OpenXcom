#pragma once
/*
 * Phase 6b — HD-only sprite batch for whole-unit composites.
 *
 * Each visible HD unit is flattened into one arena-owned ARGB surface
 * (see UnitSprite::blitBodyHD / whole-unit composite path). That surface
 * is pushed here as a single Entry. After all unit draws in the frame,
 * Map::drawTerrain calls sortAndFlushIntoQueue() once, which depth-sorts
 * the HD entries and pushes them into HDQueue for Screen::flip to blit.
 *
 * Depth value: screen-Y of the unit's tile centre. Equal-depth order is
 * preserved (stable_sort) in the order Map::drawUnit visited them.
 *
 * This batch sorts HD units among themselves only. Vanilla 8 bpp content
 * is already in _surface; we do not interleave with it.
 *
 * Intentionally unused in non-Emscripten builds.
 */
#ifdef __EMSCRIPTEN__

#include <SDL.h>
#include <vector>

namespace OpenXcom
{
namespace HDSpriteBatch
{

struct Entry
{
	SDL_Surface *src;  // whole-unit composite; owned by FrameArena
	SDL_Rect     dst;
	int          depth; // screen-Y of tile centre for sort
};

std::vector<Entry> &get();
void push(Entry e);
/// Depth-sort entries, then push each into HDQueue.
void sortAndFlushIntoQueue();

} /* namespace HDSpriteBatch */
} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */
