#ifdef __EMSCRIPTEN__
#include "CalypsoScreenWorld.h"
#include "../Engine/Screen.h"
#include "CalypsoSdlCompositeBoundary.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoPassTimers.h"
#include "../Engine/ShaderManager.h"
#include <SDL.h>
#include <SDL_render.h>
#include <GLES3/gl3.h>
#include <sstream>

extern "C" void calypso_context_reset_boundary_close(void);

namespace OpenXcom {
namespace Calypso {

bool calypsoScreenFlipWorldPass(Screen &screen, bool hasWorldPasses)
{
	if (!hasWorldPasses)
	{
		calypso_context_reset_boundary_close();
		return true;
	}
	if (SDL_RenderFlush(screen._renderer) != 0)
		Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Calypso HD world SDL flush failed");
	if (!Calypso::SdlCompositeBoundary::check("after SDL_RenderFlush"))
		return false;
	if (screen._gpuPassesPre.empty()) ShaderManager::instance().resetFrameFlag();
	if (!Calypso::SdlCompositeBoundary::check("before world callbacks"))
		return false;
	GLint savedWorldProgram = 0;
	GLint savedWorldVao = 0;
	GLint savedWorldArrayBuffer = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &savedWorldProgram);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &savedWorldVao);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &savedWorldArrayBuffer);
	const GLenum worldStateError = glGetError();
	if (!Calypso::SdlCompositeBoundary::handle(worldStateError, "after world state snapshot")) return false;
	auto restoreWorldSdlState = [&]() {
		glUseProgram(static_cast<GLuint>(savedWorldProgram));
		glBindVertexArray(static_cast<GLuint>(savedWorldVao));
		glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(savedWorldArrayBuffer));
		const GLenum restoreError = glGetError();
		if (restoreError != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				"Calypso HD world SDL state restore failed (0x" + [&]() {
					std::ostringstream out;
					out << std::hex << (unsigned)restoreError;
					return out.str();
				}() + ")");
	};
	screen._gpuWorldPassDispatching = true;
	try
	{
		for (auto& entry : screen._gpuPassesWorld)
			if (!entry.removed) entry.pass();
	}
	catch (...)
	{
		screen.finishGPUPassWorldDispatch();
		restoreWorldSdlState();
		throw;
	}
	screen.finishGPUPassWorldDispatch();
	restoreWorldSdlState();
	ShaderManager::instance().setHadGPUPass(true);
	calypso_context_reset_boundary_close();
	return true;
}

bool calypsoScreenRenderChrome(Screen &screen)
{
	const Uint64 calypsoChromeStart = calypsoPassTimersEnabled()
		? SDL_GetPerformanceCounter() : 0;
	const bool presentOk = CalypsoHdUiOverlay::instance().renderStages(screen._renderer);
	if (calypsoChromeStart)
		calypsoPassTimers().chromeUs +=
			(Uint64)((SDL_GetPerformanceCounter() - calypsoChromeStart) * 1000000ull / SDL_GetPerformanceFrequency());
	return presentOk;
}

} // namespace Calypso
} // namespace OpenXcom
#endif /* __EMSCRIPTEN__ */
