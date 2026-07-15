/*
 * GpuTexture.cpp — RGBA texture upload (Phase 8b).
 */
#include "GpuTexture.h"
#include "GpuInit.h"
#include "ShaderManager.h"
#include "Logger.h"

#include <iomanip>

#ifdef __EMSCRIPTEN__
#  include <GLES3/gl3.h>
#  include "../Calypso/GpuTextureValidation.h"
#endif

namespace OpenXcom
{

GpuTexture::GpuTexture(bool srgb, Wrap wrap, Filter filter) : _srgb(srgb), _wrap(wrap), _filter(filter)
{
    ShaderManager::instance().registerTexture(this);
}

GpuTexture::~GpuTexture()
{
    ShaderManager::instance().unregisterTexture(this);
    release();
}

bool GpuTexture::uploadRGBA(const uint8_t* data, int w, int h, int mipLevel)
{
#ifdef __EMSCRIPTEN__
    if (!GpuInit::ready()) return false;
    if (mipLevel < 0)
    {
        Log(LOG_ERROR) << "GpuTexture::uploadRGBA: negative mip level";
        return false;
    }
    CalypsoGpuTextureValidation::drainPriorGlErrors("uploadRGBA");
    if (!CalypsoGpuTextureValidation::dimensionsFitRuntime(
            "uploadRGBA", data, w, h, 4)) return false;

    if (!_tex)
    {
        glGenTextures(1, &_tex);
        glBindTexture(GL_TEXTURE_2D, _tex);
        GLenum minF = (_filter == Filter::Nearest) ? GL_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
        GLenum magF = (_filter == Filter::Nearest) ? GL_NEAREST : GL_LINEAR;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)minF);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint)magF);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        (_wrap == Wrap::RepeatS_ClampT || _wrap == Wrap::Repeat)
                            ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                        _wrap == Wrap::Repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, _tex);
    }

    /* GL_SRGB8_ALPHA8: driver converts from sRGB on sample → linear math.
     * GL_RGBA8: linear; use for procedural patterns that are already linear. */
    GLenum internalFmt = _srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, mipLevel, (GLint)internalFmt,
                 w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    if (mipLevel == 0)
    {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    const unsigned int uploadError = CalypsoGpuTextureValidation::takeGlError();
    if (uploadError != GL_NO_ERROR)
    {
        Log(LOG_ERROR) << "GpuTexture::uploadRGBA: GL upload failed for "
                       << w << "x" << h << " mip " << mipLevel
                       << " with error 0x" << std::hex << (unsigned)uploadError
                       << std::dec;
        glBindTexture(GL_TEXTURE_2D, 0u);
        if (_tex) glDeleteTextures(1, &_tex);
        _tex = 0u;
        return false;
    }
    if (mipLevel == 0)
    {
        // Guard against self-assign: the cached reupload() path passes
        // _cachedData.data() back in, and assigning a vector from its own storage
        // is UB. Skip the no-op copy when the source already aliases the cache.
        if (!_skipCache && data != _cachedData.data())
            _cachedData.assign(data, data + (size_t)w * h * 4);
        _cachedW = w; _cachedH = h;
        _w = w; _h = h;
        _isR8 = false;
    }
    glBindTexture(GL_TEXTURE_2D, 0u);
    return true;
#else
    (void)data; (void)w; (void)h; (void)mipLevel;
    return false;
#endif
}

bool GpuTexture::uploadR8(const uint8_t* data, int w, int h)
{
#ifdef __EMSCRIPTEN__
    if (!GpuInit::ready()) return false;
    CalypsoGpuTextureValidation::drainPriorGlErrors("uploadR8");
    if (!CalypsoGpuTextureValidation::dimensionsFitRuntime(
            "uploadR8", data, w, h, 1)) return false;
    if (!_tex)
    {
        glGenTextures(1, &_tex);
        glBindTexture(GL_TEXTURE_2D, _tex);
        GLenum f = (_filter == Filter::Nearest) ? GL_NEAREST : GL_LINEAR;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)f);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint)f);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        (_wrap == Wrap::RepeatS_ClampT || _wrap == Wrap::Repeat)
                            ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                        _wrap == Wrap::Repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, _tex);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, data);
    const unsigned int uploadError = CalypsoGpuTextureValidation::takeGlError();
    if (uploadError != GL_NO_ERROR)
    {
        Log(LOG_ERROR) << "GpuTexture::uploadR8: GL upload failed for "
                       << w << "x" << h << " with error 0x" << std::hex
                       << (unsigned)uploadError << std::dec;
        glBindTexture(GL_TEXTURE_2D, 0u);
        if (_tex) glDeleteTextures(1, &_tex);
        _tex = 0u;
        return false;
    }
    // Guard against self-assign (see uploadRGBA): reupload() feeds _cachedData
    // back in, and assigning a vector from its own storage is UB.
    if (!_skipCache && data != _cachedData.data())
        _cachedData.assign(data, data + (size_t)w * h);
    _cachedW = w; _cachedH = h;
    _w = w; _h = h;
    _isR8 = true;
    glBindTexture(GL_TEXTURE_2D, 0u);
    return true;
#else
    (void)data; (void)w; (void)h;
    return false;
#endif
}

void GpuTexture::bind(int textureUnit)
{
#ifdef __EMSCRIPTEN__
    glActiveTexture(GL_TEXTURE0 + (GLenum)textureUnit);
    glBindTexture(GL_TEXTURE_2D, _tex);
#else
    (void)textureUnit;
#endif
}

void GpuTexture::reupload()
{
#ifdef __EMSCRIPTEN__
    /* Drop the stale GL handle first — shared by both paths below. On a real
     * context loss glDeleteTextures is a harmless no-op (the name is already
     * invalid); on a reset without a true loss it prevents leaking the old
     * texture object. Nulling _tex makes uploadRGBA/uploadR8 re-gen it. */
    glDeleteTextures(1, &_tex);
    _tex = 0u;
#endif
    if (!_cachedData.empty())
    {
        /* Cached re-upload path (textures without a reload callback). */
        if (_isR8)
            uploadR8(_cachedData.data(), _cachedW, _cachedH);
        else
            uploadRGBA(_cachedData.data(), _cachedW, _cachedH, 0);
        return;
    }
    /* L3/L4 callback path: _cachedData deliberately empty (_skipCache=true);
     * re-decode the source from MEMFS and re-upload. */
#ifdef __EMSCRIPTEN__
    if (_reloadCb) _reloadCb();
#endif
}

void GpuTexture::evictGL()
{
    release(); // glDeleteTextures + _tex = 0; no-op when _tex is already 0
}

void GpuTexture::release()
{
#ifdef __EMSCRIPTEN__
    if (_tex) { glDeleteTextures(1, &_tex); _tex = 0u; }
#endif
}

} // namespace OpenXcom
