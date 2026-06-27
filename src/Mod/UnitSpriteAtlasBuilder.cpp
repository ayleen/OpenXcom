/*
 * UnitSpriteAtlasBuilder — Phase 14.1 (Calypso GPU Battlescape compositor).
 *
 * Packs every frame from a unit PCK SurfaceSet into a 2× NN-upscaled R8 atlas
 * so that GPU unit rendering (Phase 14.2) can use the same tile_atlas shader
 * as terrain tiles.  atlas_tile_index == PCK_frame_index.
 */
#ifdef __EMSCRIPTEN__

#include "UnitSpriteAtlasBuilder.h"
#include "../Engine/SurfaceSet.h"
#include "../Engine/Surface.h"
#include "../Engine/GpuTexture.h"
#include "../Engine/GpuInit.h"
#include "../Engine/Logger.h"
#include <SDL.h>
#include <vector>
#include <cstdint>

namespace OpenXcom
{

GpuTexture* buildUnitAtlas(const SurfaceSet& ss,
                            const SDL_Color*  palette,
                            int               ncolors,
                            int&              outAtlasW,
                            int&              outAtlasH,
                            int&              outColumns,
                            const std::string& setName)
{
    if (!GpuInit::ready()) return nullptr;
    if (!palette || ncolors < 1) return nullptr;

    const int numFrames = static_cast<int>(ss.getTotalFrames());
    if (numFrames == 0) return nullptr;

    static const int COLS   = 16;
    static const int TILE_W = 64;   // 2x upscale of 32
    static const int TILE_H = 80;   // 2x upscale of 40
    static const int SRC_W  = 32;
    static const int SRC_H  = 40;

    const int ROWS    = (numFrames + COLS - 1) / COLS;
    const int atlas_w = COLS * TILE_W;
    const int atlas_h = ROWS * TILE_H;

    std::vector<uint8_t> pixels(static_cast<size_t>(atlas_w) * static_cast<size_t>(atlas_h), 0u);

    for (int frame_idx = 0; frame_idx < numFrames; ++frame_idx)
    {
        const Surface* src = ss.getFrame(frame_idx);
        if (!src) continue;

        const int col   = frame_idx % COLS;
        const int row   = frame_idx / COLS;
        const int dst_x = col * TILE_W;
        const int dst_y = row * TILE_H;

        const SDL_Surface* sdl = src->getSurface();
        if (!sdl) continue;

        const bool needs_lock = SDL_MUSTLOCK(sdl);
        if (needs_lock)
            SDL_LockSurface(const_cast<SDL_Surface*>(sdl));

        const int sw = std::min(sdl->w, SRC_W);
        const int sh = std::min(sdl->h, SRC_H);

        for (int sy = 0; sy < sh; ++sy)
        {
            for (int sx = 0; sx < sw; ++sx)
            {
                const uint8_t pal_idx = src->getPixel(sx, sy);
                const uint8_t store = (pal_idx == 0 || static_cast<int>(pal_idx) >= ncolors)
                                      ? 0u : pal_idx;
                // 2× NN upscale
                for (int dy = sy * 2; dy <= sy * 2 + 1; ++dy)
                    for (int dx = sx * 2; dx <= sx * 2 + 1; ++dx)
                        pixels[static_cast<size_t>(dst_y + dy) * static_cast<size_t>(atlas_w)
                               + static_cast<size_t>(dst_x + dx)] = store;
            }
        }

        if (needs_lock)
            SDL_UnlockSurface(const_cast<SDL_Surface*>(sdl));
    }

    // MUST be GL_NEAREST: linear filtering of palette *indices* interpolates to
    // unrelated palette entries, drawing rainbow seams along every colour
    // boundary inside the unit sprite.
    GpuTexture* tex = new GpuTexture(/*srgb=*/false,
                                     GpuTexture::Wrap::ClampToEdge,
                                     GpuTexture::Filter::Nearest);
    if (!tex->uploadR8(pixels.data(), atlas_w, atlas_h))
    {
        Log(LOG_WARNING) << "UnitSpriteAtlasBuilder[" << setName
                         << "]: GpuTexture upload failed (" << atlas_w << "x" << atlas_h << ")";
        delete tex;
        return nullptr;
    }

    outAtlasW   = atlas_w;
    outAtlasH   = atlas_h;
    outColumns  = COLS;

    Log(LOG_INFO) << "UnitSpriteAtlasBuilder[" << setName << "]: "
                  << atlas_w << "x" << atlas_h << " atlas, "
                  << numFrames << " frames";

    return tex;
}

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
