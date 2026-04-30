/*
 * TileAtlasBuilder — Block 11.2, Phase 11 (Calypso GPU Battlescape compositor).
 *
 * Synthesises a "vanilla atlas" GpuTexture from a loaded MapDataSet when the
 * HD-pack mod is active but no explicit `tileAtlas:` YAML entry is present.
 *
 * Algorithm:
 *   - Walk the MCD entry list; deduplicate PCK frame indices.
 *   - For animation frames (sprite slots 1-7) also pack unique frames.
 *   - Lay out into a 16-column grid of 64x80 RGBA8 tiles (2x NN upscale).
 *   - Upload to a new GpuTexture (sRGB, ClampToEdge) and return ownership.
 *
 * All code is inside `#ifdef __EMSCRIPTEN__` — the native build is unaffected.
 */
#ifdef __EMSCRIPTEN__

#include "TileAtlasBuilder.h"
#include "MapDataSet.h"
#include "MapData.h"
#include "../Engine/SurfaceSet.h"
#include "../Engine/Surface.h"
#include "../Engine/GpuTexture.h"
#include "../Engine/GpuInit.h"
#include "../Engine/Logger.h"
#include <SDL.h>
#include <vector>
#include <map>

namespace OpenXcom
{

GpuTexture* buildVanillaAtlas(const MapDataSet& mds,
                               const SDL_Color*  palette,
                               int               ncolors,
                               std::map<int,int>& frameMap,
                               std::map<int,int>& pckToAtlas)
{
    if (!GpuInit::ready()) return nullptr;
    if (!palette || ncolors < 1) return nullptr;

    const SurfaceSet* ss = mds.getSurfaceset();
    if (!ss) return nullptr;

    const int numFrames = static_cast<int>(ss->getTotalFrames());
    if (numFrames == 0) return nullptr;

    const std::vector<MapData*>* objects = const_cast<MapDataSet&>(mds).getObjectsRaw();
    if (!objects || objects->empty()) return nullptr;

    // Step 1: build dedup_map (PCK frame idx -> atlas tile idx) and atlas_tiles list.
    std::map<int,int> dedup_map;             // PCK frame index -> atlas tile index
    std::vector<int>  atlas_tiles;           // ordered list: atlas_tile_idx -> PCK frame index

    frameMap.clear();

    for (int mcd_index = 0; mcd_index < static_cast<int>(objects->size()); ++mcd_index)
    {
        const MapData* entry = (*objects)[mcd_index];
        if (!entry) continue;

        const int primary_pck = entry->getSprite(0);
        if (primary_pck < 0 || primary_pck >= numFrames)
        {
            // skip entries with no valid primary sprite
            continue;
        }

        // Deduplicate primary sprite
        auto it = dedup_map.find(primary_pck);
        if (it != dedup_map.end())
        {
            frameMap[mcd_index] = it->second;
        }
        else
        {
            const int atlas_tile_idx = static_cast<int>(atlas_tiles.size());
            atlas_tiles.push_back(primary_pck);
            dedup_map[primary_pck] = atlas_tile_idx;
            frameMap[mcd_index] = atlas_tile_idx;
        }

        // Also pack animation frames (sprite slots 1..7)
        for (int k = 1; k < 8; ++k)
        {
            const int anim_pck = entry->getSprite(k);
            // Slot 0 repeating or out-of-range or invalid means end of animation
            if (anim_pck <= 0 || anim_pck == primary_pck || anim_pck >= numFrames)
                break;

            if (dedup_map.find(anim_pck) == dedup_map.end())
            {
                dedup_map[anim_pck] = static_cast<int>(atlas_tiles.size());
                atlas_tiles.push_back(anim_pck);
            }
        }
    }

    const int N = static_cast<int>(atlas_tiles.size());
    if (N == 0) return nullptr;

    // Step 2: compute atlas dimensions
    static const int COLS   = 16;
    static const int TILE_W = 64;
    static const int TILE_H = 80;
    static const int SRC_W  = 32;
    static const int SRC_H  = 40;

    const int ROWS    = (N + COLS - 1) / COLS;
    const int atlas_w = COLS * TILE_W;
    const int atlas_h = ROWS * TILE_H;

    // One byte per pixel: palette index (0 = transparent, 1..255 = opaque).
    std::vector<uint8_t> pixels(static_cast<size_t>(atlas_w) * static_cast<size_t>(atlas_h), 0u);

    // Step 3: rasterise each tile
    for (int tile_idx = 0; tile_idx < N; ++tile_idx)
    {
        const int pck_frame_idx = atlas_tiles[tile_idx];
        const Surface* src = ss->getFrame(pck_frame_idx);
        if (!src) continue;

        const int col   = tile_idx % COLS;
        const int row   = tile_idx / COLS;
        const int dst_x = col * TILE_W;
        const int dst_y = row * TILE_H;

        // Lock if needed (SDL may require it for direct pixel access)
        // Surface::lock/unlock are public — use the SDL surface directly for speed.
        const SDL_Surface* sdl = src->getSurface();
        if (!sdl) continue;

        // Lock the surface for pixel access if SDL requires it
        const bool needs_lock = SDL_MUSTLOCK(sdl);
        if (needs_lock)
        {
            // Cast away const to lock; we read-only, but SDL_LockSurface needs mutable.
            SDL_LockSurface(const_cast<SDL_Surface*>(sdl));
        }

        // Determine actual pixel dimensions from the surface (may be smaller than SRC_W/H)
        const int sw = sdl->w;
        const int sh = sdl->h;

        for (int sy = 0; sy < sh && sy < SRC_H; ++sy)
        {
            for (int sx = 0; sx < sw && sx < SRC_W; ++sx)
            {
                // Read palette index from the Surface mirror (most reliable path):
                // getPixel() returns the palette index for both 8bpp and ARGB surfaces
                // (the latter uses _paletteMirror when available).
                const uint8_t pal_idx = src->getPixel(sx, sy);

                // 2x nearest-neighbour upscale: each src pixel -> 2x2 block in dst.
                // Store the raw palette index; the GPU shade-table lookup maps
                // (shade, palIdx) -> final RGBA at draw time.
                const uint8_t store = (pal_idx == 0 || static_cast<int>(pal_idx) >= ncolors)
                                      ? 0u : pal_idx;
                for (int dy = sy * 2; dy <= sy * 2 + 1; ++dy)
                {
                    for (int dx = sx * 2; dx <= sx * 2 + 1; ++dx)
                    {
                        const size_t dst_i =
                            static_cast<size_t>(dst_y + dy) * static_cast<size_t>(atlas_w)
                            + static_cast<size_t>(dst_x + dx);
                        pixels[dst_i] = store;
                    }
                }
            }
        }

        if (needs_lock)
        {
            SDL_UnlockSurface(const_cast<SDL_Surface*>(sdl));
        }
    }

    // Step 4: upload to GPU as R8 (palette-index atlas; shade applied at draw time)
    GpuTexture* tex = new GpuTexture(/*srgb=*/false);
    if (!tex->uploadR8(pixels.data(), atlas_w, atlas_h))
    {
        Log(LOG_WARNING) << "TileAtlasBuilder[" << mds.getName()
                         << "]: GpuTexture upload failed (" << atlas_w << "x" << atlas_h << ")";
        delete tex;
        return nullptr;
    }

    pckToAtlas = dedup_map;

    Log(LOG_INFO) << "TileAtlasBuilder[" << mds.getName() << "]: "
                  << atlas_w << "x" << atlas_h << " atlas, "
                  << N << " tiles, "
                  << frameMap.size() << " MCD entries mapped";

    return tex;
}

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
