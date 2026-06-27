#pragma once
/*
 * UnitSpriteAtlasBuilder — Phase 14.1 (Calypso GPU Battlescape compositor).
 *
 * Builds a vanilla R8 atlas from a SurfaceSet (unit PCK set) when the
 * HD-pack mod is active.  Every PCK frame is packed in declaration order so
 * atlas_tile_index == PCK_frame_index — no deduplication needed.
 *
 * Atlas format: 2× nearest-neighbour upscale (32×40 → 64×80 cells),
 * 16 columns, single-channel R8 (palette index; shade-table lookup at draw
 * time via the same tile_atlas shader used for terrain tiles).
 *
 * All code is inside #ifdef __EMSCRIPTEN__ — the native build is unaffected.
 */
#ifdef __EMSCRIPTEN__
#include <string>

struct SDL_Color;

namespace OpenXcom
{

class SurfaceSet;
class GpuTexture;

/// Build a unit sprite atlas from all frames in a SurfaceSet.
/// atlas_tile_index == PCK_frame_index (packed in declaration order).
/// On success: fills outAtlasW/H with pixel dimensions, outColumns with column
/// count, and returns ownership of a new GpuTexture (R8).
/// Returns nullptr if the set has no frames or GPU is not ready.
GpuTexture* buildUnitAtlas(const SurfaceSet& ss,
                            const SDL_Color*  palette,
                            int               ncolors,
                            int&              outAtlasW,
                            int&              outAtlasH,
                            int&              outColumns,
                            const std::string& setName);

} // namespace OpenXcom
#endif // __EMSCRIPTEN__
