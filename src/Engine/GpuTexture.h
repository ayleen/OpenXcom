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
#include <functional>
#include <string>
#include <vector>

namespace OpenXcom
{
class GpuTexture
{
public:
    enum class Wrap   { ClampToEdge, RepeatS_ClampT, Repeat };
    enum class Filter { Linear, Nearest };

    explicit GpuTexture(bool srgb = true,
                        Wrap   wrap   = Wrap::ClampToEdge,
                        Filter filter = Filter::Linear);
    ~GpuTexture();

    /* Upload RGBA pixel data to the GPU. mipLevel=0 triggers glGenerateMipmap. */
    bool uploadRGBA(const uint8_t* data, int w, int h, int mipLevel = 0);
    /* Upload a single-channel R8 atlas. Respects _filter and _wrap; no mipmaps. */
    bool uploadR8(const uint8_t* data, int w, int h);
    /* Bind to the given texture unit (0-based). */
    void bind(int textureUnit = 0);

    int  width()  const { return _w; }
    int  height() const { return _h; }
    bool isValid() const { return _tex != 0u; }

    /* Called by ShaderManager on SDL_RENDER_TARGETS_RESET. */
    void reupload();

    /* L3/L4: skip storing _cachedData for large textures that carry a reload CB. */
    void setSkipCache(bool s) { _skipCache = s; }
    /* L3/L4: re-decode + re-upload callback used when _cachedData is empty on context loss. */
    void setReloadCb(std::function<void()> cb) { _reloadCb = std::move(cb); }

private:
    unsigned             _tex     = 0u;
    int                  _w       = 0;
    int                  _h       = 0;
    bool                 _srgb    = true;
    Wrap                 _wrap    = Wrap::ClampToEdge;
    Filter               _filter  = Filter::Linear;
    bool                 _isR8    = false;
    std::vector<uint8_t> _cachedData;   // owned copy for lost-context recovery
    int                  _cachedW = 0;
    int                  _cachedH = 0;
    bool                 _skipCache = false;    // L3/L4: skip _cachedData for textures with a reload CB
    std::function<void()> _reloadCb;            // L3/L4: re-decode + re-upload on context loss

    void release();
};
} // namespace OpenXcom
