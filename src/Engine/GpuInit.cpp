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

namespace OpenXcom
{

bool GpuInit::_ready = false;

void GpuInit::init()
{
    if (_ready) return;

#ifdef __EMSCRIPTEN__
    /* WebGL2 context is always GLES 3.0; all symbols available directly. */
    _ready = true;
    Log(LOG_INFO) << "GpuInit: WebGL2 / GLES 3.0 pipeline ready";
#else
    Log(LOG_INFO) << "GpuInit: GPU shader pipeline requires Emscripten/WebGL2 — "
                  << "skipped on native (Phase 8b is browser-first)";
    /* _ready stays false; all Shader/GpuTexture/RenderTarget methods no-op. */
#endif
}

bool GpuInit::ready()
{
    return _ready;
}

} // namespace OpenXcom
