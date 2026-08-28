#ifdef __EMSCRIPTEN__
/*
 * Calypso Emscripten-only SDL composite context-loss boundary -- extracted
 * verbatim from Engine/Screen.cpp (policy R3/R6 relocation-only).
 *
 * Owns the supported-Emscripten current-context probe, the reset-sentinel
 * C-linkage seams, initial-loss arming, logging, and failHdRoute for every
 * named Screen::flip() composite boundary. Engine/Screen.cpp consumes only the
 * narrow OpenXcom::Calypso::SdlCompositeBoundary::{check,handle} seam declared
 * in CalypsoSdlCompositeBoundary.h.
 */
#include "CalypsoSdlCompositeBoundary.h"

#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>

#include <sstream>
#include <string>

#include "../Engine/Logger.h"
#include "CalypsoHdUiOverlay.h"

/* Reset-sentinel C-linkage seams; definitions live in CalypsoMainLoopGate.cpp.
 * Declared at file scope (extern "C" is not allowed at block scope). */
extern "C" void calypso_gl_context_lost(void);
extern "C" int calypso_context_reset_sentinel_pending(void);
extern "C" void calypso_context_reset_sentinel_observed(void);
extern "C" int calypso_context_reset_boundary_open(void);
extern "C" void calypso_context_reset_boundary_close(void);
extern "C" void calypso_context_reset_sentinel_consumed(void);

namespace OpenXcom
{
namespace Calypso
{
namespace SdlCompositeBoundary
{

/* Keep SDL's deferred composite errors owned by the exact boundary that
 * observed them. The one post-swap context sentinel is accepted only while
 * the reset-boundary window owns it (open window or transferred pending
 * token); consuming it cleanly drops pending ownership and closes the window
 * at that boundary. Intermediate clean reads never mutate the window — only
 * an owned-token consume or the end-of-chain close in Screen::flip() bounds
 * it. Every other error either arms calypso_gl_context_lost() synchronously
 * (only for an unowned token proven against an objectively lost CURRENT
 * Emscripten context — the initial-loss race) or closes the registered HD
 * route before a physical world callback can publish pixels. */
static int calypsoEmscriptenCurrentContextLost()
{
	const EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = emscripten_webgl_get_current_context();
	if (context <= 0) return -1;
	return emscripten_is_webgl_context_lost(context) ? 1 : 0;
}

/* Returns:
 *   true  — the boundary read was clean or an owned reset sentinel was
 *           consumed; continue this presented-frame attempt.
 *   false — initial live→lost transition observed before the JS
 *           'webglcontextlost' listener could run: calypso_gl_context_lost()
 *           was armed synchronously and the caller must abort this flip
 *           immediately without further SDL/GL/world/chrome work in the frame
 *           (nonfatal single-frame abort).
 * Every other error is logged and fails the registered HD route (throws). */
static bool calypsoHandleSdlCompositeError(GLenum error, const char *phase)
{
	static const GLenum CALYPSO_CONTEXT_LOST_WEBGL = 0x9242;
	if (error == CALYPSO_CONTEXT_LOST_WEBGL
		&& (calypso_context_reset_boundary_open() || calypso_context_reset_sentinel_pending()))
	{
		calypso_context_reset_sentinel_observed();
		const GLenum followUp = glGetError();
		if (followUp == 0) /* documented clean read right behind the owned token */
		{
			calypso_context_reset_sentinel_consumed();
			calypso_context_reset_boundary_close();
			return true;
		}
		error = followUp;
	}
	if (error == GL_NO_ERROR) return true;
	if (error == CALYPSO_CONTEXT_LOST_WEBGL && calypsoEmscriptenCurrentContextLost() == 1)
	{
		/* Initial-loss race: 'webglcontextlost' is dispatched asynchronously,
		 * so an in-flight flip can observe the very first token before
		 * web/src/main.js runs its listener. Arm the identical pause here
		 * synchronously (the C guard makes this idempotent; if the listener
		 * runs later it is a safe no-op) and abort only this frame. The
		 * reset-owned window above already had its chance and this path
		 * neither opens nor closes it; every later flip short-circuits on the
		 * top-of-flip g_calypsoContextLost guard, so nothing drains. */
		Log(LOG_INFO) << "Calypso initial WebGL context loss observed by SDL composite boundary " << phase
		              << " before the JS listener ran — arming pause, aborting this frame nonfatally";
		calypso_gl_context_lost();
		return false;
	}
	Log(LOG_ERROR) << "Calypso SDL composite GL error at " << phase << " (0x"
		           << std::hex << (unsigned)error << std::dec << ")";
	Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
		"WebGL SDL composite " + std::string(phase) + " failed (0x"
		+ [&]() { std::ostringstream out; out << std::hex << (unsigned)error; return out.str(); }() + ")");
}

/* Reads the deferred SDL composite error at a named boundary. Boundaries that
 * need their glGetError() read to double as another check's input call
 * calypsoHandleSdlCompositeError directly with the already-read error. */
static bool calypsoCheckSdlCompositeBoundary(const char *phase)
{
	return calypsoHandleSdlCompositeError(glGetError(), phase);
}

/* Narrow API consumed by Engine/Screen.cpp flip(); both forward to the
 * relocated implementations above unchanged. */
bool check(const char *phase)
{
	return calypsoCheckSdlCompositeBoundary(phase);
}

bool handle(GLenum error, const char *phase)
{
	return calypsoHandleSdlCompositeError(error, phase);
}

}
}
}
#endif /* __EMSCRIPTEN__ */
