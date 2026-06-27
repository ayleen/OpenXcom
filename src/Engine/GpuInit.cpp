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
#endif

namespace OpenXcom
{

bool GpuInit::_ready = false;
bool GpuInit::_hdrColorBuffer = false;
bool GpuInit::_floatBlend = false;

void GpuInit::init()
{
    if (_ready)
    {
        /* Already initialised, but a context restore may have dropped the
         * float-render extensions — re-enable them on the live context. */
        enableExtensions();
        return;
    }

#ifdef __EMSCRIPTEN__
    /* WebGL2 context is always GLES 3.0; all symbols available directly. */
    _ready = true;
    enableExtensions();
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

bool GpuInit::hdr()
{
    return _hdrColorBuffer;
}

bool GpuInit::floatBlend()
{
    return _floatBlend;
}

} // namespace OpenXcom
