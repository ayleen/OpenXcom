/*
 * GpuInit.cpp — GPU pipeline initialisation (Phase 8b).
 *
 * Emscripten / WebGL2: all GLES 3.0 symbols are directly callable via
 *   <GLES3/gl3.h>; no runtime loading required.
 * Native: the pipeline is Emscripten-first for Phase 8b; full GL 3.3 core
 *   function loading can be added in a later phase once native smoke tests
 *   are required.
 */
#include "GpuInit.h"
#include "Logger.h"

#ifdef __EMSCRIPTEN__
#  include <emscripten/html5.h>
#  include <emscripten/html5_webgl.h>
#  include <GLES3/gl3.h>
#endif

namespace OpenXcom
{

bool GpuInit::_ready = false;
bool GpuInit::_hdrColorBuffer = false;
bool GpuInit::_floatBlend = false;
int GpuInit::_maxTextureSize = 0;

void GpuInit::init()
{
    if (_ready)
    {
        /* A caller may repeat init after a renderer/context replacement. Do
         * not trust the old publication without probing the current context. */
        if (contextReady())
        {
            enableExtensions();
            return;
        }
    }

#ifdef __EMSCRIPTEN__
    _ready = false;
    _maxTextureSize = 0;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_get_current_context();
    const GLubyte *version = ctx ? glGetString(GL_VERSION) : nullptr;
    GLint maxTextureSize = 0;
    if (ctx && version && *version)
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    const GLenum probeError = glGetError();
    if (!ctx || !version || !*version || maxTextureSize <= 0 || probeError != GL_NO_ERROR)
    {
        Log(LOG_ERROR) << "GpuInit: current WebGL2 probe failed (version/context/max texture)";
        return;
    }
    /* Publish the capability only after the entire current-context probe is
     * valid; failed init leaves the transactional false/zero state. */
    _maxTextureSize = maxTextureSize;
    enableExtensions();
    _ready = true;
    Log(LOG_INFO) << "GpuInit: WebGL2 / GLES 3.0 pipeline ready"
                  << " (HDR float buffer: " << (_hdrColorBuffer ? "yes" : "no")
                  << ", EXT_float_blend: " << (_floatBlend ? "yes" : "no") << ")";
#else
    Log(LOG_INFO) << "GpuInit: GPU shader pipeline requires Emscripten/WebGL2 — "
                  << "skipped on native (Phase 8b is browser-first)";
    /* _ready stays false; all Shader/GpuTexture/RenderTarget methods no-op. */
#endif
}

void GpuInit::enableExtensions()
{
#ifdef __EMSCRIPTEN__
    /* Phase 25 (R0): WebGL2 requires getExtension() before float-render
     * functionality is usable. EXT_color_buffer_float is the load-bearing one
     * (RGBA16F renderable); the other two are best-effort (16F linear-filtering
     * and 16F blending are already core in WebGL2, so their absence is benign —
     * we still request them in case a driver gates them). Must run on the
     * *current* context, so call this after the GL context is live. */
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_get_current_context();
    if (!ctx)
    {
        _hdrColorBuffer = false;
        return;
    }
    _hdrColorBuffer = emscripten_webgl_enable_extension(ctx, "EXT_color_buffer_float");
    // EXT_float_blend governs 32-bit float blending only; blending into RGBA16F
    // (our HDR target) is core WebGL2, so this is best-effort + diagnostic, NOT a
    // gate on the HDR path. OES_texture_float_linear likewise covers 32F only
    // (16F linear filtering is core) — requested for completeness.
    _floatBlend = emscripten_webgl_enable_extension(ctx, "EXT_float_blend");
    emscripten_webgl_enable_extension(ctx, "OES_texture_float_linear");
#else
    _hdrColorBuffer = false;
    _floatBlend = false;
#endif
}

bool GpuInit::ready()
{
    return _ready;
}

void GpuInit::invalidate()
{
    _ready = false;
    _hdrColorBuffer = false;
    _floatBlend = false;
    _maxTextureSize = 0;
}

bool GpuInit::contextReady()
{
#ifdef __EMSCRIPTEN__
    if (!_ready || !emscripten_webgl_get_current_context())
    {
        _ready = false;
        _maxTextureSize = 0;
        return false;
    }
    const GLubyte *version = glGetString(GL_VERSION);
    GLint maxTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    const bool ready = version && *version && maxTextureSize > 0 && glGetError() == GL_NO_ERROR;
    if (!ready)
    {
        _ready = false;
        _maxTextureSize = 0;
        return false;
    }
	_maxTextureSize = maxTextureSize;
    return ready;
#else
    return false;
#endif
}

int GpuInit::maxTextureSize()
{
    return _maxTextureSize;
}

bool GpuInit::hdr()
{
    return _hdrColorBuffer;
}

bool GpuInit::floatBlend()
{
    return _floatBlend;
}

} // namespace OpenXcom
