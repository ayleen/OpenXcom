#pragma once
/*
 * TileAtlasBuilder — Block 11.2, Phase 11 (Calypso GPU Battlescape compositor).
 *
 * Synthesises a "vanilla atlas" GpuTexture from a loaded MapDataSet when the
 * HD-pack mod is active but no explicit `tileAtlas:` YAML entry is present.
 * The atlas is a 2× nearest-neighbour upscale of the 32×40 8bpp PCK frames,
 * packed into a 16-column grid of 64×80 RGBA8 tiles.
 *
 * All code is inside `#ifdef __EMSCRIPTEN__` — the native build is unaffected.
 */
#ifdef __EMSCRIPTEN__
#include <cstdint>
#include <map>

struct SDL_Color;

namespace OpenXcom
{
class MapDataSet;
class GpuTexture;

/// Build a vanilla atlas from a loaded MapDataSet.
/// Returns ownership of a new GpuTexture (16×N cell grid, 64×80 cells, R8).
/// Also fills frameMap: MCD entry index -> atlas tile index (primary frames only).
/// Also fills pckToAtlas: PCK frame index -> atlas tile index (all frames incl. animation).
/// Returns nullptr if the dataset has no frames or GPU is not ready.
GpuTexture* buildVanillaAtlas(const MapDataSet& mds,
                               const SDL_Color*  palette,
                               int               ncolors,
                               std::map<int,int>& frameMap,
                               std::map<int,int>& pckToAtlas);

} // namespace OpenXcom
#endif // __EMSCRIPTEN__
