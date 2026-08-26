#ifdef __EMSCRIPTEN__
/*
 * CalypsoScreenRecovery -- WebGL CONTEXT_LOST_WEBGL sentinel ordering gate
 * (PR #51 engine).
 *
 * Splits renderer/context creation from GpuInit publication so the first
 * GL error probe after the replacement context goes through the sentinel-
 * aware owner. The recovery transaction owns exactly one 0x9242 token within
 * its bounded window; any other GL error fails closed. This TU is empty on
 * native.
 */
#include "CalypsoScreenRecovery.h"
#include "../Engine/Logger.h"
#include "../Engine/GpuInit.h"
#include "../Engine/Screen.h"
#include "../Engine/Surface.h"
#include "../Engine/Options.h"
#include "../Engine/Exception.h"
#include <SDL.h>
#include <SDL_render.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgl.h>
#include <GLES3/gl3.h>
#include <tuple>
#include <string>

extern "C" int calypso_context_reset_sentinel_pending(void);
extern "C" void calypso_context_reset_sentinel_observed(void);
extern "C" int calypso_context_reset_boundary_open(void);
extern "C" void calypso_context_reset_sentinel_consumed(void);
extern "C" SDL_Texture *calypsoCreateLogicalStreamingTexture(SDL_Renderer *renderer);

namespace OpenXcom {
namespace Calypso {

bool calypsoScreenRecoveryProbeAndInit()
{
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_get_current_context();
    if (!ctx || emscripten_is_webgl_context_lost(ctx))
        return false;

    static const GLenum SENTINEL = 0x9242;
    GLenum err = glGetError();
    if (err == SENTINEL)
    {
        if (!calypso_context_reset_boundary_open() && !calypso_context_reset_sentinel_pending())
            return false;
        calypso_context_reset_sentinel_observed();
        err = glGetError();
        if (err != GL_NO_ERROR)
            return false;
        calypso_context_reset_sentinel_consumed();
    }
    if (err != GL_NO_ERROR)
        return false;

    ctx = emscripten_webgl_get_current_context();
    if (!ctx || emscripten_is_webgl_context_lost(ctx))
        return false;

    GpuInit::init();
    if (!GpuInit::ready())
        return false;
    return GpuInit::contextReady();
}

void calypsoScreenResetDisplayRendererOnly(Screen &screen)
{
    // Renderer-only path: create window/renderer/texture without any GL probe.
    int width  = Options::displayWidth;
    int height = Options::displayHeight;
    screen.makeVideoFlags();

    if (!screen._surface || screen._surface->format->BitsPerPixel != 32 ||
        screen._surface->w != screen._baseWidth || screen._surface->h != screen._baseHeight)
    {
        std::tie(screen._buffer, screen._surface) = Surface::NewPair32Bit(screen._baseWidth, screen._baseHeight);
    }

    Log(LOG_INFO) << "Creating SDL2 window " << width << "x" << height;

    if (screen._texture)  { SDL_DestroyTexture(screen._texture);   screen._texture  = nullptr; }
    if (screen._renderer) { SDL_DestroyRenderer(screen._renderer); screen._renderer = nullptr; }
    if (screen._screen)   { SDL_FreeSurface(screen._screen);       screen._screen   = nullptr; }
    if (screen._window)   { SDL_DestroyWindow(screen._window);     screen._window   = nullptr; }

    Uint32 winFlags = SDL_WINDOW_OPENGL;
    if (Options::allowResize) winFlags |= SDL_WINDOW_RESIZABLE;
    int posX = SDL_WINDOWPOS_UNDEFINED, posY = SDL_WINDOWPOS_UNDEFINED;

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    screen._window = SDL_CreateWindow("Project Calypso \u2014 The depths are hungry", posX, posY, width, height, winFlags);
    if (!screen._window)
    {
        Log(LOG_ERROR) << "SDL_CreateWindow failed: " << SDL_GetError();
        throw Exception(SDL_GetError());
    }

    if (screen.useOpenGL())
    {
        screen._screen = SDL_CreateRGBSurface(0, width, height, 32,
            0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
        if (!screen._screen)
        {
            Log(LOG_ERROR) << "SDL_CreateRGBSurface failed: " << SDL_GetError();
            throw Exception(SDL_GetError());
        }
    }
    else
    {
        SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
        screen._renderer = SDL_CreateRenderer(screen._window, -1, SDL_RENDERER_ACCELERATED);
        if (!screen._renderer)
        {
            Log(LOG_ERROR) << "SDL_CreateRenderer failed: " << SDL_GetError();
            throw Exception(SDL_GetError());
        }

        screen._screen = SDL_CreateRGBSurface(0, width, height, 32,
            0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
        if (!screen._screen)
        {
            Log(LOG_ERROR) << "SDL_CreateRGBSurface failed: " << SDL_GetError();
            throw Exception(SDL_GetError());
        }

        screen._texture = calypsoCreateLogicalStreamingTexture(screen._renderer);
        if (!screen._texture)
        {
            Log(LOG_ERROR) << "SDL_CreateTexture failed: " << SDL_GetError();
            throw Exception(SDL_GetError());
        }
    }

    Log(LOG_INFO) << "Display set: " << width << "x" << height
                  << ", base=" << screen._baseWidth << "x" << screen._baseHeight;

    Options::displayWidth  = screen.getWidth();
    Options::displayHeight = screen.getHeight();
    screen._scaleX = (screen._baseWidth  > 0) ? (double)screen.getWidth()  / screen._baseWidth  : 1.0;
    screen._scaleY = (screen._baseHeight > 0) ? (double)screen.getHeight() / screen._baseHeight : 1.0;

    screen._topBlackBand = screen._bottomBlackBand = screen._leftBlackBand = screen._rightBlackBand = 0;
    screen._cursorTopBlackBand = screen._cursorLeftBlackBand = 0;

    screen.setPalette(screen.getPalette());
}

bool calypsoScreenRecreateRendererGL(Screen &screen)
{
    try
    {
        GpuInit::invalidate();
        calypsoScreenResetDisplayRendererOnly(screen);
        if (!calypsoScreenRecoveryProbeAndInit())
            throw Exception("replacement WebGL2 context is not ready");
        screen._forceCanvasRebase = true;
        return true;
    }
    catch (Exception &e)
    {
        Log(LOG_ERROR) << "M6g: recreateRendererGL failed — " << e.what();
        GpuInit::invalidate();
        extern int g_calypsoContextLost;
        g_calypsoContextLost = 1;
        emscripten_pause_main_loop();
        return false;
    }
}


void calypsoScreenUploadLogicalTexture(Screen &screen)
{
	const Uint64 calypsoTexStart = Calypso::calypsoPassTimersEnabled() ? SDL_GetPerformanceCounter() : 0;
	void *texPixels;
	int texPitch;
	if (SDL_LockTexture(screen._texture, nullptr, &texPixels, &texPitch) != 0)
		Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Calypso HD logical texture lock failed");
	if (screen._surface->pitch == texPitch)
		memcpy(texPixels, screen._surface->pixels, (size_t)screen._surface->h * texPitch);
	else
		for (int y = 0; y < screen._surface->h; y++)
			memcpy((char*)texPixels + y * texPitch, (char*)screen._surface->pixels + y * screen._surface->pitch, (size_t)screen._surface->w * 4);
	SDL_UnlockTexture(screen._texture);
	if (calypsoTexStart)
		Calypso::calypsoPassTimers().sdlMemcpyUs += (Uint64)((SDL_GetPerformanceCounter() - calypsoTexStart) * 1000000ull / SDL_GetPerformanceFrequency());
}

} // namespace Calypso
} // namespace OpenXcom
extern "C" SDL_Texture *calypsoCreateLogicalStreamingTexture(SDL_Renderer *renderer)
{
	SDL_Texture *texture = SDL_CreateTexture(renderer,
	    SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
	    OpenXcom::Options::baseXResolution, OpenXcom::Options::baseYResolution);
	if (!texture) return nullptr;
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
	SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
	return texture;
}

#endif // __EMSCRIPTEN__
