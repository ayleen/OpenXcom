#pragma once
/*
 * GpuTexture — RGBA / sRGBA texture upload wrapper (Phase 8b).
 *
 * Internal format: GL_SRGB8_ALPHA8 by default (driver-managed gamma).
 * Pass srgb=false for linear RGBA8 (e.g. procedural test patterns).
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
    enum class Wrap { ClampToEdge, RepeatS_ClampT };

    explicit GpuTexture(bool srgb = true, Wrap wrap = Wrap::ClampToEdge);
    ~GpuTexture();

    /* Upload pixel data to the GPU. mipLevel=0 triggers glGenerateMipmap. */
    bool uploadRGBA(const uint8_t* data, int w, int h, int mipLevel = 0);
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
    std::vector<uint8_t> _cachedData;  // owned copy for lost-context recovery
    int                  _cachedW = 0;
    int                  _cachedH = 0;

    void release();
};
} // namespace OpenXcom
