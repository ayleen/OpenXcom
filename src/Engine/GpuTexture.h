#pragma once
/*
 * GpuTexture — RGBA / sRGBA / R8 texture upload wrapper (Phase 8b; R8 added 11.5).
 *
 * Internal format: GL_SRGB8_ALPHA8 by default (driver-managed gamma).
 * Pass srgb=false for linear RGBA8 (e.g. procedural test patterns).
 * Use uploadR8() for single-channel palette-index atlases (always GL_NEAREST).
 *
 * Instances register themselves with ShaderManager for lost-context recovery.
 */
#include <cstdint>
#include <vector>

namespace OpenXcom
{
class GpuTexture
{
public:
    enum class Wrap   { ClampToEdge, RepeatS_ClampT };
    enum class Filter { Linear, Nearest };

    explicit GpuTexture(bool srgb = true,
                        Wrap   wrap   = Wrap::ClampToEdge,
                        Filter filter = Filter::Linear);
    ~GpuTexture();

    /* Upload RGBA pixel data to the GPU. mipLevel=0 triggers glGenerateMipmap. */
    bool uploadRGBA(const uint8_t* data, int w, int h, int mipLevel = 0);
    /* Upload a single-channel R8 palette-index atlas. Always GL_NEAREST, no mipmaps. */
    bool uploadR8(const uint8_t* data, int w, int h);
    /* Bind to the given texture unit (0-based). */
    void bind(int textureUnit = 0);

    int  width()  const { return _w; }
    int  height() const { return _h; }
    bool isValid() const { return _tex != 0u; }

    /* Called by ShaderManager on SDL_RENDER_TARGETS_RESET. */
    void reupload();

private:
    unsigned             _tex     = 0u;
    int                  _w       = 0;
    int                  _h       = 0;
    bool                 _srgb    = true;
    Wrap                 _wrap    = Wrap::ClampToEdge;
    Filter               _filter  = Filter::Linear;
    bool                 _isR8    = false;
    std::vector<uint8_t> _cachedData;  // owned copy for lost-context recovery
    int                  _cachedW = 0;
    int                  _cachedH = 0;

    void release();
};
} // namespace OpenXcom
