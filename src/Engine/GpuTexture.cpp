/*
 * GpuTexture.cpp — RGBA texture upload (Phase 8b).
 */
#include "GpuTexture.h"
#include "GpuInit.h"
#include "ShaderManager.h"
#include "Logger.h"

#ifdef __EMSCRIPTEN__
#  include <GLES3/gl3.h>
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
        if (!_skipCache) _cachedData.assign(data, data + (size_t)w * h * 4);
        _cachedW = w; _cachedH = h;
        _w = w; _h = h;
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
    if (!_skipCache) _cachedData.assign(data, data + (size_t)w * h);
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
    if (!_cachedData.empty())
    {
        /* Existing cached re-upload path: GL handle is stale; delete + re-gen. */
#ifdef __EMSCRIPTEN__
        glDeleteTextures(1, &_tex);
        _tex = 0u;
#endif
        if (_isR8)
            uploadR8(_cachedData.data(), _cachedW, _cachedH);
        else
            uploadRGBA(_cachedData.data(), _cachedW, _cachedH, 0);
        return;
    }
    /* L3/L4 callback path: _cachedData deliberately empty (_skipCache=true).
     * Do NOT glDeleteTextures — the context is gone; the handle is already
     * invalid.  Null it so uploadRGBA/uploadR8 will call glGenTextures. */
#ifdef __EMSCRIPTEN__
    if (_reloadCb) { _tex = 0u; _reloadCb(); }
#endif
}

void GpuTexture::release()
{
#ifdef __EMSCRIPTEN__
    if (_tex) { glDeleteTextures(1, &_tex); _tex = 0u; }
#endif
}

} // namespace OpenXcom
