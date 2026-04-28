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

GpuTexture::GpuTexture(bool srgb) : _srgb(srgb)
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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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
    if (mipLevel == 0) glGenerateMipmap(GL_TEXTURE_2D);

    _w = w; _h = h;
    _cachedData = data; _cachedW = w; _cachedH = h;
    glBindTexture(GL_TEXTURE_2D, 0u);
    return true;
#else
    (void)data; (void)w; (void)h; (void)mipLevel;
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
    if (!_cachedData) return;
#ifdef __EMSCRIPTEN__
    glDeleteTextures(1, &_tex);
    _tex = 0u;
#endif
    uploadRGBA(_cachedData, _cachedW, _cachedH, 0);
}

void GpuTexture::release()
{
#ifdef __EMSCRIPTEN__
    if (_tex) { glDeleteTextures(1, &_tex); _tex = 0u; }
#endif
}

} // namespace OpenXcom
