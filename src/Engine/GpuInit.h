#pragma once
/*
 * GpuInit — one-time initialization of the GPU shader pipeline.
 *
 * On Emscripten (WebGL2), all GLES 3.0 symbols are available directly
 * from <GLES3/gl3.h>; init() is a no-op.
 * On native, the pipeline is not yet wired (Phase 8b is Emscripten-first);
 * ready() returns false so all GPU classes skip their GL calls gracefully.
 *
 * Phase 25 (R0): hdr() reports whether the WebGL2 EXT_color_buffer_float
 * extension is available so the SSAA scene buffer can be a GL_RGBA16F float
 * target (HDR). Falls back to GL_RGBA8 when absent.
 */

namespace OpenXcom
{
struct GpuInit
{
    /* Call once after the SDL2 renderer is created (from Screen::resetDisplay). */
    static void init();
    /* Returns true iff GL3/ES3 functions are available. */
    static bool ready();
    /* Mark the current context unusable until init() proves a replacement. */
    static void invalidate();
    /* Probe the current WebGL2 context without presenting anything. */
    static bool contextReady();
    /* Validated texture dimension limit captured at the reset boundary. */
    static int maxTextureSize();

    /* Phase 25 (R0): true iff EXT_color_buffer_float is available, i.e. an
     * RGBA16F colour attachment can be rendered to (the HDR scene buffer).
     * Always false on native (the GPU pipeline is Emscripten-first). */
    static bool hdr();

    /* Phase 25 (R0): true iff EXT_float_blend is available. NOTE this gates
     * 32-bit float blending only — blending into 16-bit float (RGBA16F, what we
     * use) is CORE in WebGL2, so the HDR path does NOT require it. Tracked purely
     * for the startup diagnostic log. */
    static bool floatBlend();

    /* (Re)enable the WebGL2 float-render extensions on the *current* context
     * and refresh hdr(). Idempotent — call from init() and after a context
     * restore (the restored context drops previously-enabled extensions). */
    static void enableExtensions();

private:
    static bool _ready;
    static bool _hdrColorBuffer;
    static bool _floatBlend;
    static int _maxTextureSize;
};
} // namespace OpenXcom
