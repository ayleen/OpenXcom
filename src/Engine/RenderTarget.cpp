/*
 * RenderTarget.cpp — FBO wrapper (Phase 8b).
 */
#include "RenderTarget.h"
#include "GpuInit.h"
#include "ShaderManager.h"
#include "Logger.h"
#include <SDL.h>
#include <cstring>
#include <vector>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#  include <GLES3/gl3.h>
#endif

namespace OpenXcom
{

RenderTarget::~RenderTarget()
{
    ShaderManager::instance().unregisterTarget(this);
    release();
}

bool RenderTarget::create(int w, int h)
{
#ifdef __EMSCRIPTEN__
    if (!GpuInit::ready()) return false;
    release();

    /* Color attachment texture. */
    glGenTextures(1, &_colorTex);
    glBindTexture(GL_TEXTURE_2D, _colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0u);

    glGenFramebuffers(1, &_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, _colorTex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0u);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        Log(LOG_ERROR) << "RenderTarget::create FBO incomplete (status=0x"
                       << std::hex << status << ")";
        release();
        return false;
    }

    _w = w; _h = h;
    ShaderManager::instance().registerTarget(this);
    return true;
#else
    (void)w; (void)h;
    return false;
#endif
}

void RenderTarget::bind()
{
#ifdef __EMSCRIPTEN__
    GLint prev = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
    _prevFbo = prev;
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glViewport(0, 0, _w, _h);
#endif
}

void RenderTarget::unbind()
{
#ifdef __EMSCRIPTEN__
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)_prevFbo);
#endif
}

void RenderTarget::blitTo(SDL_Texture* dest)
{
#ifdef __EMSCRIPTEN__
    if (!_fbo || !dest) return;

    /* Read pixels from our FBO. */
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    std::vector<uint8_t> pixels((size_t)_w * _h * 4);
    glReadPixels(0, 0, _w, _h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0u);

    /* Flip rows: GL origin is bottom-left; SDL expects top-left. */
    const int rowBytes = _w * 4;
    for (int y = 0; y < _h / 2; ++y)
    {
        uint8_t* rowA = pixels.data() + (size_t)y * rowBytes;
        uint8_t* rowB = pixels.data() + (size_t)(_h - 1 - y) * rowBytes;
        for (int x = 0; x < rowBytes; ++x) std::swap(rowA[x], rowB[x]);
    }

    void* texPixels = nullptr;
    int   texPitch  = 0;
    if (SDL_LockTexture(dest, nullptr, &texPixels, &texPitch) == 0)
    {
        for (int y = 0; y < _h; ++y)
            memcpy((char*)texPixels + y * texPitch,
                   pixels.data() + (size_t)y * rowBytes,
                   (size_t)rowBytes);
        SDL_UnlockTexture(dest);
    }
#else
    (void)dest;
#endif
}

void RenderTarget::reupload()
{
    int w = _w, h = _h;
    create(w, h);
}

void RenderTarget::release()
{
#ifdef __EMSCRIPTEN__
    if (_fbo)      { glDeleteFramebuffers(1, &_fbo);   _fbo      = 0u; }
    if (_colorTex) { glDeleteTextures(1, &_colorTex);  _colorTex = 0u; }
#endif
    _w = _h = 0;
}

} // namespace OpenXcom
