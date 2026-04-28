#pragma once
/*
 * GpuInit — one-time initialization of the GPU shader pipeline.
 *
 * On Emscripten (WebGL2), all GLES 3.0 symbols are available directly
 * from <GLES3/gl3.h>; init() is a no-op.
 * On native, the pipeline is not yet wired (Phase 8b is Emscripten-first);
 * ready() returns false so all GPU classes skip their GL calls gracefully.
 */
#pragma once

namespace OpenXcom
{
struct GpuInit
{
    /* Call once after the SDL2 renderer is created (from Screen::resetDisplay). */
    static void init();
    /* Returns true iff GL3/ES3 functions are available. */
    static bool ready();

private:
    static bool _ready;
};
} // namespace OpenXcom
