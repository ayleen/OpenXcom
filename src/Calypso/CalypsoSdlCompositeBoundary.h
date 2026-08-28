#pragma once
/*
 * Calypso Emscripten-only SDL composite context-loss boundary -- narrow seam
 * consumed by Engine/Screen.cpp flip() (policy R3/R6 relocation-only from
 * Engine/Screen.cpp; the owning implementation lives in
 * CalypsoSdlCompositeBoundary.cpp).
 */
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>

namespace OpenXcom
{
namespace Calypso
{

/// Owns SDL's deferred composite-error handling at every named Screen::flip()
/// boundary (Emscripten WebGL only; native never compiles any of it). The
/// supported-Emscripten context probe, reset-sentinel C seams, initial-loss
/// arming, logging, and failHdRoute stay private to the implementation file.
namespace SdlCompositeBoundary
{

/* Reads the deferred SDL composite error at a named boundary (one glGetError()
 * per call) and handles it. Returns:
 *   true  -- the boundary read was clean or an owned reset sentinel was
 *            consumed; continue this presented-frame attempt.
 *   false -- initial live->lost transition observed before the JS
 *            'webglcontextlost' listener could run: calypso_gl_context_lost()
 *            was armed synchronously and the caller must abort this flip
 *            immediately without further SDL/GL/world/chrome work in the frame
 *            (nonfatal single-frame abort). */
bool check(const char *phase);

/* Handles an already-read GL error at a named boundary. Boundaries that need
 * their glGetError() read to double as another check's input (world state
 * snapshot) call this directly with the already-read error. */
bool handle(GLenum error, const char *phase);

}
}
}
#endif /* __EMSCRIPTEN__ */
