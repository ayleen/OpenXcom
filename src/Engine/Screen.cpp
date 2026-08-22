/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "Screen.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoResolutionFloor.h"
#endif
#include "Game.h"
#include "../Mod/Mod.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <climits>
#include <cstdio>
#include <vector>
#include "../lodepng.h"
#include "Exception.h"
#include "Surface.h"
#include "Logger.h"
#include "Action.h"
#include "Options.h"
#include "CrossPlatform.h"
#include "FileMap.h"
#include "Zoom.h"
#include "Timer.h"
#include "GpuInit.h"
#include "GpuTimer.h"
#include "ShaderManager.h"
#include "../Calypso/CalypsoHdUiOverlay.h" // Phase 46.2-HD (empty on native)
#include <SDL.h>
#include <SDL_render.h>
#include <algorithm>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <GLES3/gl3.h>
/* M6c: context-lost flag; C-linkage definition lives in ../Calypso/CalypsoMainLoopGate.cpp.
 * Declared at file scope (extern "C" is not allowed at block scope) — same
 * pattern as g_calypsoSsaaScale in Map.cpp. */
extern "C" int g_calypsoContextLost;
#endif

namespace OpenXcom
{

const int Screen::ORIGINAL_WIDTH = 320;
const int Screen::ORIGINAL_HEIGHT = 200;


/**
 * Sets up all the internal display flags depending on
 * the current video settings.
 */
void Screen::makeVideoFlags()
{
	/* All paths use ARGB32 surfaces and an SDL2 window/renderer/texture chain. */
	_bpp = 32;
	_baseWidth  = Options::baseXResolution;
	_baseHeight = Options::baseYResolution;
}


/**
 * Initializes a new display screen for the game to render contents to.
 * The screen is set up based on the current options.
 */
Screen::Screen() : _screen(nullptr), _window(nullptr), _renderer(nullptr), _texture(nullptr),
	_baseWidth(ORIGINAL_WIDTH), _baseHeight(ORIGINAL_HEIGHT), _scaleX(1.0), _scaleY(1.0),
	_bpp(32), _numColors(0), _firstColor(0), _pushPalette(false), _flickerFix(false)
{
	_flickerFix = Options::oxceEnablePaletteFlickerFix;

	resetDisplay();
	memset(deferredPalette, 0, 256*sizeof(SDL_Color));
}

/**
 * Deletes the buffer from memory. The display screen itself
 * is automatically freed once SDL shuts down.
 */
Screen::~Screen()
{
#ifdef __EMSCRIPTEN__
	/* GPU passes own Shader/GpuTexture/VAO objects. Destroy their closures while
	 * SDL's WebGL context is still alive; member destruction happens after this
	 * destructor body and would otherwise issue glDelete* against a dead context. */
	_gpuPasses.clear();
	_gpuPassesPre.clear();
#endif
	if (_texture)  { SDL_DestroyTexture(_texture);   _texture  = nullptr; }
	if (_renderer) { SDL_DestroyRenderer(_renderer); _renderer = nullptr; }
	if (_window)   { SDL_DestroyWindow(_window);     _window   = nullptr; }
	if (_screen)   { SDL_FreeSurface(_screen);       _screen   = nullptr; }
}

#ifdef __EMSCRIPTEN__
/**
 * Destroys and re-creates the SDL renderer and streaming screen texture
 * after a real WebGL context loss (webglcontextrestored event path).
 *
 * Why this is necessary
 * ---------------------
 * Every SDL_Texture and every internal SDL_Renderer object (shader program,
 * vertex buffer, etc.) carries a GL object ID that was allocated in the DEAD
 * context.  Even though the browser revives those GL IDs if the hardware
 * permits, SDL2's Emscripten/GLES2 backend has no webglcontextrestored
 * handling — it never rebinds or regenerates its own GL objects.  The first
 * SDL_RenderCopy after restore therefore triggers:
 *   "bindTexture: object does not belong to this context"
 *   "bindBuffer: object does not belong to this context"
 *   "drawArrays: no valid shader program in use"
 * and the screen stays black.
 *
 * Emscripten GL table safety
 * --------------------------
 * Re-creating the renderer calls SDL_GL_CreateContext on the same canvas.
 * The browser returns the SAME (now-restored) WebGL context object.
 * Emscripten's GL object tables (GL.textures / GL.buffers / GL.programs)
 * are module-global, not per-context, so numeric IDs that our engine
 * allocated via GpuTexture / ShaderManager remain valid after this call.
 * ShaderManager::reuploadAll() (called immediately after) re-populates
 * those IDs with fresh GPU objects, completing the restore.
 *
 * Ordering constraint
 * -------------------
 * This function MUST run before ShaderManager::reuploadAll().  reuploadAll()
 * makes raw GL calls that need a live GL context; the new renderer provides it.
 */
void Screen::recreateRendererGL()
{
	/* M6c Task 2 — dead-mouse fix.
	 *
	 * Root cause of the dead mouse after context restore:
	 *   SDL_DestroyRenderer → GLES2_DestroyRenderer → SDL_GL_DeleteContext →
	 *   Emscripten GL.deleteContext → JSEvents.removeAllHandlersOnTarget(canvas)
	 *
	 * GL.deleteContext strips EVERY handler registered via Emscripten's
	 * JSEvents infrastructure from the canvas — including SDL's own mouse and
	 * keyboard event listeners registered by Emscripten_RegisterEventHandlers
	 * (called from Emscripten_CreateWindow).  SDL_CreateRenderer does NOT
	 * re-register them because GLES2_CreateRenderer's SDL_RecreateWindow
	 * branch is skipped once the window already carries an OpenGL ES profile.
	 *
	 * Fix: use the full resetDisplay(true, …) path which destroys the SDL
	 * window too.  SDL_CreateWindow → Emscripten_CreateWindow →
	 * Emscripten_RegisterEventHandlers re-registers all mouse/key handlers on
	 * the canvas, restoring input to the identical state as initial boot.
	 * resetDisplay also calls GpuInit::init() (re-enables float extensions),
	 * recalculates _scaleX/_scaleY, and resets all SDL surface/texture state.
	 *
	 * ShaderManager::reuploadAll() is still called by Screen::handle() after
	 * this returns to rebuild GPU resources. */
	try
	{
		resetDisplay(true, false);

		/* M6h: force the canvas-size rebase block in flip() to run on the next
		 * frame even though the polled canvas dimensions will equal
		 * Options::displayWidth/Height (resetDisplay just wrote them back).
		 *
		 * The previous M6g approach zeroed displayWidth/Height to trick the
		 * canvas-poll condition (wW != Options::displayWidth) into firing.  That
		 * worked for scale re-derivation but introduced Defect M6h: with the old
		 * value recorded as 0, BattlescapeState::resize(dX, dY) (called from
		 * zoom() during the same tick) computed a huge delta (new – 0 = canvas
		 * width) and shifted the entire battlescape HUD off-screen.
		 *
		 * Using a flag instead leaves Options::displayWidth/Height intact (correct
		 * values set by resetDisplay above).  When the forced rebase pass runs in
		 * flip(), it assigns displayWidth = wW (same value → delta 0) and then
		 * calls Screen::updateScale for both scales — re-deriving
		 * baseXBattlescape / baseYBattlescape / baseXGeoscape / baseYGeoscape from
		 * the current (correct) display size without displacing any state. */
		_forceCanvasRebase = true;
	}
	catch (Exception &e)
	{
		/* M6g Defect 2: on a second GPU crash Chrome throttles WebGL context
		 * creation, so SDL_CreateRenderer fails inside resetDisplay and throws.
		 * Never let this exception escape iterate() — catch it here, log it, then
		 * re-enter the lost state so the JS 13-s reload fallback takes over.
		 *
		 * Ordering safety: calypso_gl_context_restored() resumed the main loop
		 * just before the SDL_RENDER_TARGETS_RESET event was dispatched to this
		 * handler.  Setting g_calypsoContextLost = 1 here and calling
		 * emscripten_pause_main_loop() ensures:
		 *   (a) flip()'s guard at the top ("if (g_calypsoContextLost) return;")
		 *       skips every GL call in the remainder of this iterate() tick — the
		 *       flag is set synchronously inside this catch block, before flip()
		 *       is reached in the same Game::run() iteration, so no GL call slips
		 *       through between resume and re-pause.
		 *   (b) The main loop is paused after this iterate() tick completes,
		 *       giving the JS 13-s reload timer a chance to fire. */
		Log(LOG_ERROR) << "M6g: recreateRendererGL failed — " << e.what();
		g_calypsoContextLost = 1;
		emscripten_pause_main_loop();
	}
}
#endif /* __EMSCRIPTEN__ */

/**
 * Returns the screen's internal buffer surface. Any
 * contents that need to be shown will be blitted to this.
 * @return Pointer to the buffer surface.
 */
SDL_Surface *Screen::getSurface()
{
	_pushPalette = true;
	return _surface.get();
}

/**
 * Handles screen key shortcuts.
 * @param action Pointer to an action.
 */
void Screen::handle(Action *action)
{
	if (Options::debug)
	{
		if (action->getDetails()->type == SDL_KEYDOWN && action->getDetails()->key.keysym.sym == SDLK_F8 && (SDL_GetModState() & KMOD_ALT) != 0)
		{
			switch(Timer::gameSlowSpeed)
			{
				case 1: Timer::gameSlowSpeed = 5; break;
				case 5: Timer::gameSlowSpeed = 15; break;
				default: Timer::gameSlowSpeed = 1; break;
			}
		}
	}

	if (action->getDetails()->type == SDL_RENDER_TARGETS_RESET)
	{
		/* WebGL context has been restored after a tab-suspend or chrome://gpucrash.
		 *
		 * The SDL renderer and its streaming screen texture both hold internal GLES2
		 * objects (shader programs, vertex buffer, texture handle) that were created
		 * in the dead context.  SDL2's Emscripten/GLES2 backend has no
		 * webglcontextrestored handling, so those objects are permanently stale.
		 *
		 * Destroy and re-create the renderer chain first, then let reuploadAll()
		 * restore our own GPU resources (GpuTexture atlases, ShaderManager programs,
		 * FBOs).  The order is critical: recreateRendererGL() must run before
		 * reuploadAll() so the new renderer establishes the fresh GL context that
		 * reuploadAll() uploads into. */
#ifdef __EMSCRIPTEN__
		recreateRendererGL();
#endif
		ShaderManager::instance().reuploadAll();
	}
	else if (action->getDetails()->type == SDL_KEYDOWN && action->getDetails()->key.keysym.sym == SDLK_RETURN && (SDL_GetModState() & KMOD_ALT) != 0)
	{
		Options::fullscreen = !Options::fullscreen;
		resetDisplay();
	}
	else if (action->getDetails()->type == SDL_KEYDOWN && action->getDetails()->key.keysym.sym == Options::keyScreenshot)
	{
		std::ostringstream ss;
		int i = 0;
		do
		{
			ss.str("");
			ss << Options::getMasterUserFolder() << "screen" << std::setfill('0') << std::setw(3) << i << ".png";
			i++;
		}
		while (CrossPlatform::fileExists(ss.str()));
		screenshot(ss.str());
		return;
	}
}


/**
 * Renders the buffer's contents onto the screen, applying
 * any necessary filters or conversions in the process.
 * If the scaling factor is bigger than 1, the entire contents
 * of the buffer are resized by that factor (eg. 2 = doubled)
 * before being put on screen.
 */
bool Screen::flip()
{
#ifdef __EMSCRIPTEN__
	/* M6c: do not issue any GL calls while the WebGL context is dead.
	 * calypso_gl_context_lost() pauses the main loop, but this guard also
	 * covers the brief window between emscripten_resume_main_loop() and the
	 * first frame processed after Screen::handle() finishes recreating the
	 * renderer (the event is consumed in the same loop tick as the resume). */
	if (g_calypsoContextLost) return false;

	/* Browser canvas resize: poll canvas.width each frame (physical pixels).
	 * SDL_GetWindowSize returns CSS logical pixels in Emscripten, which differ
	 * from Options::displayWidth (physical) on HiDPI/Retina screens (DPR > 1),
	 * causing a spurious mismatch and scale corruption on every flip().
	 * Reading canvas.width directly avoids the DPR mismatch. */
	if (_window)
	{
		int wW = (int)EM_ASM_INT({ return document.getElementById('canvas').width; });
		int wH = (int)EM_ASM_INT({ return document.getElementById('canvas').height; });
		if (wW > 0 && wH > 0 &&
		    (wW != Options::displayWidth || wH != Options::displayHeight || _forceCanvasRebase))
		{
			reflowCanvasFallback(wW, wH);
		}
	}
	// Phase 46.2-HD: the HD overlay's per-frame metrics freeze + frame advance now
	// happen at the pre-blit boundary in Game::run() (prepareFrame), so nothing is
	// advanced here -- Screen::flip() only consumes the committed queue below.
#endif

	/* When States call Screen::updateScale, Options::baseXResolution changes
	 * but resetDisplay is not called.  Detect the mismatch and re-create
	 * surfaces at the new size, keeping window/renderer alive. */
	if (_surface && (_surface->w != Options::baseXResolution
	              || _surface->h != Options::baseYResolution))
	{
		resetDisplay(false, false);
	}

	if (useOpenGL())
	{
		const bool presented = Zoom::flipWithZoom(_surface.get(), _screen, _topBlackBand,
		                                          _bottomBlackBand, _leftBlackBand,
		                                          _rightBlackBand, &glOutput, _window);
		_numColors = 0;
		_pushPalette = false;
		return presented;
	}

	/* SDL2 renderer path — shared by Emscripten and native non-OpenGL. */

	/* HD pack: HD floor cells leave their 256×320 rectangle's non-diamond
	 * corners transparent (so CPU-drawn walls/objects above them aren't
	 * occluded). The CPU surface is also fully transparent there (Map::draw
	 * fills RGBA(0,0,0,0) in HD mode), so SDL_RenderCopy's BLENDMODE_BLEND
	 * lets the framebuffer clear color show through those gaps. SDL2's
	 * default render draw color is opaque black — which painted black
	 * squares between HD diamonds where vanilla shows palette index 15
	 * (the bgColor used by Surface::draw). Match vanilla by clearing to
	 * pal[15]. */
	const SDL_Color* pal = getPalette();
	if (pal)
	{
		SDL_SetRenderDrawColor(_renderer, pal[15].r, pal[15].g, pal[15].b, 255);
	}
#ifdef __EMSCRIPTEN__
	// When a Battlescape pre-composite GPU pass is registered (HD pack active +
	// in-mission), override the clear color with the solid teal baked into HD
	// overlay tiles (#468A9A) — this hides the thin "seam" lines between
	// adjacent HD diamonds where transparent corners of the base buffer expose
	// the clear color through SDL_RenderCopy's BLENDMODE_BLEND. Geoscape /
	// menus / loading have no _gpuPassesPre and keep the palette[15] clear
	// above so non-Battlescape scenes don't get a teal background.
	if (!_gpuPassesPre.empty())
	{
		// Hardcoded to match docs/seabed-floor-sprites/tiles-hd-selected/*.png
		SDL_SetRenderDrawColor(_renderer, 0x46, 0x8A, 0x9A, 255);
	}
#endif

	/* Phase 13.3: pre-composite GPU passes (HD tile floor) fire before the SDL
	 * surface composite so CPU-drawn units / walls / HUD land on top of them.
	 *
	 * Save the active GL shader program before our raw GL passes and restore it
	 * after. SDL2's batch state-cache assumes its own program is bound when it
	 * issues SDL_RenderCopy below; if we leave a custom program bound the
	 * RenderCopy draw produces no pixels and the HUD becomes invisible.
	 * (We deliberately don't restore VAO/buffer — doing so additionally clobbers
	 * the pre-composite floor pixels on the framebuffer, see post-Phase-14 fix.) */
	SDL_RenderClear(_renderer);
#ifdef __EMSCRIPTEN__
	if (!_gpuPassesPre.empty())
	{
		SDL_RenderFlush(_renderer);

		GLint savedProg = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, &savedProg);

		ShaderManager::instance().resetFrameFlag();
		for (auto& pass : _gpuPassesPre)
			pass();
		ShaderManager::instance().setHadGPUPass(true);

		// SDL_RenderCopy below draws the surface texture with BLENDMODE_BLEND
		// expecting GL_BLEND enabled with the standard alpha func. Pre-composite
		// passes exit with glDisable(GL_BLEND) leaving SDL's batch-cached blend
		// state out of sync with actual GL — RenderCopy then runs unblended and
		// overwrites our pre-composite floor pixels with texture's alpha=0
		// transparent black. Force the state SDL expects before the composite.
		glUseProgram((GLuint)savedProg);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
#endif

	/* Upload the CPU surface (units, walls, HUD) as a texture and composite
	 * it over whatever the pre-composite passes drew.  _texture blend mode is
	 * SDL_BLENDMODE_BLEND so transparent surface pixels let GPU content show. */
	SDL_BlitScaled(_surface.get(), nullptr, _screen, nullptr);

	void *texPixels;
	int   texPitch;
	SDL_LockTexture(_texture, nullptr, &texPixels, &texPitch);
	if (texPitch == _screen->pitch)
	{
		memcpy(texPixels, _screen->pixels, (size_t)_screen->h * texPitch);
	}
	else
	{
		for (int y = 0; y < _screen->h; y++)
		{
			memcpy((char*)texPixels + y * texPitch,
			       (char*)_screen->pixels + y * _screen->pitch,
			       (size_t)_screen->w * 4);
		}
	}
	SDL_UnlockTexture(_texture);
	SDL_RenderCopy(_renderer, _texture, nullptr, nullptr);

#ifdef __EMSCRIPTEN__
	// Phase 46.2-HD: HD UI + diagnostics stages draw above the legacy composite.
	const bool hdPresentOk = Calypso::CalypsoHdUiOverlay::instance().renderStages(_renderer);
#endif

	/* GPU shader passes (Phase 8b): cursor, projectile, smoke — overlay on top.
	 * SDL_RenderFlush submits SDL's internal vertex batch before any raw
	 * GL calls are made.  Each pass saves/restores all GL state. */
	if (!_gpuPasses.empty())
	{
		SDL_RenderFlush(_renderer);
		if (_gpuPassesPre.empty())   // resetFrameFlag not already called above
			ShaderManager::instance().resetFrameFlag();

		GpuTimer timer;
		timer.start();
		for (auto& pass : _gpuPasses)
			pass();
		timer.stop();

		ShaderManager::instance().setHadGPUPass(true);

		/* Log average GPU-pass time every 60 frames (Phase 8b.9). */
		_gpuPassAccumUs += timer.elapsedUs();
		++_gpuFrameCount;
		if (_gpuFrameCount >= 60u)
		{
			long long avg = _gpuPassAccumUs / (long long)_gpuFrameCount;
			Log(LOG_DEBUG) << "GPU passes avg: " << avg << " us/frame"
			               << " (" << _gpuPasses.size() << " pass(es))";
			_gpuFrameCount  = 0u;
			_gpuPassAccumUs = 0;
		}
	}

#ifdef __EMSCRIPTEN__
	// Enabled HD routes throw before this boundary on any draw failure. The bool
	// remains the dormant/harness presentation gate (its result now feeds the
	// flip() return value and hence the presented-frame serial); it is not a
	// vanilla retry.
	if (!hdPresentOk)
	{
		_numColors = 0;
		_pushPalette = false;
		return false;
	}
#endif
	SDL_RenderPresent(_renderer);

	_numColors = 0;
	_pushPalette = false;
	return true;
}

/**
 * Clears all the contents out of the internal buffer.
 */
void Screen::clear()
{
	Surface::CleanSdlSurface(_surface.get());
	Surface::CleanSdlSurface(_screen);
}

/**
 * Changes the 8bpp palette used to render the screen's contents.
 * @param colors Pointer to the set of colors.
 * @param firstcolor Offset of the first color to replace.
 * @param ncolors Amount of colors to replace.
 * @param immediately Apply palette changes immediately, otherwise wait for next blit.
 */
void Screen::setPalette(const SDL_Color* colors, int firstcolor, int ncolors, bool immediately)
{
	if (_numColors && (_numColors != ncolors) && (_firstColor != firstcolor))
	{
		// an initial palette setup has not been committed to the screen yet
		// just update it with whatever colors are being sent now
		memmove(&(deferredPalette[firstcolor]), colors, sizeof(SDL_Color)*ncolors);
		_numColors = 256; // all the use cases are just a full palette with 16-color follow-ups
		_firstColor = 0;
	}
	else
	{
		memmove(&(deferredPalette[firstcolor]), colors, sizeof(SDL_Color) * ncolors);
		_numColors = ncolors;
		_firstColor = firstcolor;
	}

	// _screen is ARGB32; palette changes are deferred to shade-table lookups only.
	(void)immediately;

	// Sanity check
	/*
	SDL_Color *newcolors = _screen->format->palette->colors;
	for (int i = firstcolor, j = 0; i < firstcolor + ncolors; i++, j++)
	{
		Log(LOG_DEBUG) << (int)newcolors[i].r << " - " << (int)newcolors[i].g << " - " << (int)newcolors[i].b;
		Log(LOG_DEBUG) << (int)colors[j].r << " + " << (int)colors[j].g << " + " << (int)colors[j].b;
		if (newcolors[i].r != colors[j].r ||
			newcolors[i].g != colors[j].g ||
			newcolors[i].b != colors[j].b)
		{
			Log(LOG_ERROR) << "Display palette doesn't match requested palette";
			break;
		}
	}
	*/
}

/**
 * Returns the screen's 8bpp palette.
 * @return Pointer to the palette's colors.
 */
SDL_Color *Screen::getPalette() const
{
	return (SDL_Color*)deferredPalette;
}

/**
 * Returns the width of the screen.
 * @return Width in pixels.
 */
int Screen::getWidth() const
{
	return _screen->w;
}

/**
 * Returns the height of the screen.
 * @return Height in pixels
 */
int Screen::getHeight() const
{
	return _screen->h;
}

/**
 * Resets the screen surfaces based on the current display options,
 * as they don't automatically take effect.
 * @param resetVideo Reset display surface.
 */
void Screen::resetDisplay(bool resetVideo, bool noShaders)
{
	int width  = Options::displayWidth;
	int height = Options::displayHeight;
	makeVideoFlags(); /* sets _bpp=32, _baseWidth, _baseHeight */

	/* (Re)allocate ARGB32 game surface if size changed. */
	if (!_surface || _surface->format->BitsPerPixel != 32 ||
	    _surface->w != _baseWidth || _surface->h != _baseHeight)
	{
		std::tie(_buffer, _surface) = Surface::NewPair32Bit(_baseWidth, _baseHeight);
	}

#ifdef __EMSCRIPTEN__
	/* Emscripten: resize screen/texture when canvas size changed without full resetVideo. */
	if (!resetVideo && _window && _screen
	    && (_screen->w != width || _screen->h != height))
	{
		if (_texture) { SDL_DestroyTexture(_texture);  _texture = nullptr; }
		SDL_FreeSurface(_screen); _screen = nullptr;

		_screen = SDL_CreateRGBSurface(0, width, height, 32,
		    0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
		if (!_screen) throw Exception(SDL_GetError());

		_texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ARGB8888,
		    SDL_TEXTUREACCESS_STREAMING, width, height);
		if (!_texture) throw Exception(SDL_GetError());
		// Phase 13.3: BLEND lets pre-composite GPU content show through transparent surface regions.
		SDL_SetTextureBlendMode(_texture, SDL_BLENDMODE_BLEND);

		Log(LOG_INFO) << "Display rebased: canvas=" << width << "x" << height
		              << ", base=" << _baseWidth << "x" << _baseHeight;
	}
#endif

	if (resetVideo || !_window)
	{
		Log(LOG_INFO) << "Creating SDL2 window " << width << "x" << height;

		if (_texture)  { SDL_DestroyTexture(_texture);   _texture  = nullptr; }
		if (_renderer) { SDL_DestroyRenderer(_renderer); _renderer = nullptr; }
		if (_screen)   { SDL_FreeSurface(_screen);       _screen   = nullptr; }
		if (_window)   { SDL_DestroyWindow(_window);     _window   = nullptr; }

		Uint32 winFlags = 0;
#ifdef __EMSCRIPTEN__
		winFlags = SDL_WINDOW_OPENGL;
		if (Options::allowResize) winFlags |= SDL_WINDOW_RESIZABLE;
		int posX = SDL_WINDOWPOS_UNDEFINED, posY = SDL_WINDOWPOS_UNDEFINED;
#else
		if (useOpenGL())          winFlags |= SDL_WINDOW_OPENGL;
		if (Options::allowResize) winFlags |= SDL_WINDOW_RESIZABLE;
		if (Options::fullscreen)  winFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		if (Options::borderless)  winFlags |= SDL_WINDOW_BORDERLESS;
		int posX = SDL_WINDOWPOS_CENTERED, posY = SDL_WINDOWPOS_CENTERED;
		if (!Options::fullscreen && Options::rootWindowedMode)
		{
			posX = Options::windowedModePositionX;
			posY = Options::windowedModePositionY;
		}
#endif

#ifdef __EMSCRIPTEN__
		// Request a depth buffer in the WebGL2 context so the iso-depth GPU
		// pipeline (Map::drawTileGLPass) can sort tiles/units/items by their
		// per-instance iso priority. Default emscripten/SDL2 attributes
		// usually include depth=true, but setting this explicitly is safer.
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
#endif
#ifdef __EMSCRIPTEN__
		// SDL2's emscripten backend mirrors the window title into document.title,
		// which would otherwise overwrite the web shell's own <title> at boot.
		_window = SDL_CreateWindow("Project Calypso — The depths are hungry", posX, posY, width, height, winFlags);
#else
		_window = SDL_CreateWindow("OpenXcom Extended", posX, posY, width, height, winFlags);
#endif
		if (!_window)
		{
			Log(LOG_ERROR) << "SDL_CreateWindow failed: " << SDL_GetError();
			throw Exception(SDL_GetError());
		}

		if (useOpenGL())
		{
			/* OpenGL context is managed by glOutput (Zoom::flipWithZoom).
			 * Only a staging surface is needed here; no renderer/texture. */
			_screen = SDL_CreateRGBSurface(0, width, height, 32,
			    0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
			if (!_screen)
			{
				Log(LOG_ERROR) << "SDL_CreateRGBSurface failed: " << SDL_GetError();
				throw Exception(SDL_GetError());
			}
		}
		else
		{
			_renderer = SDL_CreateRenderer(_window, -1,
			    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
			if (!_renderer)
			{
				Log(LOG_ERROR) << "SDL_CreateRenderer failed: " << SDL_GetError();
				throw Exception(SDL_GetError());
			}

			_screen = SDL_CreateRGBSurface(0, width, height, 32,
			    0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
			if (!_screen)
			{
				Log(LOG_ERROR) << "SDL_CreateRGBSurface failed: " << SDL_GetError();
				throw Exception(SDL_GetError());
			}

			_texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ARGB8888,
			    SDL_TEXTUREACCESS_STREAMING, width, height);
			if (!_texture)
			{
				Log(LOG_ERROR) << "SDL_CreateTexture failed: " << SDL_GetError();
				throw Exception(SDL_GetError());
			}
			// Phase 13.3: BLEND lets pre-composite GPU content show through transparent surface regions.
			SDL_SetTextureBlendMode(_texture, SDL_BLENDMODE_BLEND);
			// Force NEAREST scaling on the main display texture: preScaleHDBilinear
			// (Surface.cpp) sets SDL_HINT_RENDER_SCALE_QUALITY=1 globally for HD
			// pre-scaling and never restores it, so by the time we create the main
			// display texture the hint is "1" → SDL bilinear-blurs base→display
			// upscale and partial-alpha pixels at HD-overlay tile borders blend
			// with neighboring transparent fragments, producing the visible thin
			// dark "seam" between adjacent diamonds. NEAREST keeps tile edges crisp.
			SDL_SetTextureScaleMode(_texture, SDL_ScaleModeNearest);
		}

		Log(LOG_INFO) << "Display set: " << width << "x" << height
		              << ", base=" << _baseWidth << "x" << _baseHeight;

		/* Initialise the GPU shader pipeline once the GL context is live.
		 * On Emscripten, SDL_WINDOW_OPENGL + SDL_CreateRenderer establishes
		 * the WebGL2 context; GpuInit::init() is a no-op on native. */
		GpuInit::init();
	}
	else
	{
		clear();
	}

	Options::displayWidth  = getWidth();
	Options::displayHeight = getHeight();
	_scaleX = (_baseWidth  > 0) ? (double)getWidth()  / _baseWidth  : 1.0;
	_scaleY = (_baseHeight > 0) ? (double)getHeight() / _baseHeight : 1.0;

#ifndef __EMSCRIPTEN__
	/* Aspect-ratio black bands for the OpenGL scaler and cursor-clip logic. */
	double pixelRatioY = 1.0;
	if (Options::nonSquarePixelRatio && !Options::allowResize)
		pixelRatioY = 1.2;

	bool cursorInBlackBands =
		!Options::keepAspectRatio ? false :
		Options::fullscreen       ? Options::cursorInBlackBandsInFullscreen :
		!Options::borderless      ? Options::cursorInBlackBandsInWindow :
		                            Options::cursorInBlackBandsInBorderlessWindow;

	if (_scaleX > _scaleY && Options::keepAspectRatio)
	{
		int targetWidth = (int)floor(_scaleY * (double)_baseWidth);
		_topBlackBand = _bottomBlackBand = 0;
		_leftBlackBand = (getWidth() - targetWidth) / 2;
		if (_leftBlackBand < 0) _leftBlackBand = 0;
		_rightBlackBand = getWidth() - targetWidth - _leftBlackBand;
		_cursorTopBlackBand = 0;
		if (cursorInBlackBands) { _scaleX = _scaleY; _cursorLeftBlackBand = _leftBlackBand; }
		else _cursorLeftBlackBand = 0;
	}
	else if (_scaleY > _scaleX && Options::keepAspectRatio)
	{
		int targetHeight = (int)floor(_scaleX * (double)_baseHeight * pixelRatioY);
		_topBlackBand = (getHeight() - targetHeight) / 2;
		if (_topBlackBand < 0) _topBlackBand = 0;
		_bottomBlackBand = getHeight() - targetHeight - _topBlackBand;
		if (_bottomBlackBand < 0) _bottomBlackBand = 0;
		_leftBlackBand = _rightBlackBand = 0;
		_cursorLeftBlackBand = 0;
		if (cursorInBlackBands) { _scaleY = _scaleX; _cursorTopBlackBand = _topBlackBand; }
		else _cursorTopBlackBand = 0;
	}
	else
	{
		_topBlackBand = _bottomBlackBand = _leftBlackBand = _rightBlackBand =
		    _cursorTopBlackBand = _cursorLeftBlackBand = 0;
	}

	if (useOpenGL())
	{
#ifndef __NO_OPENGL
		OpenGL::checkErrors = Options::checkOpenGLErrors;
		glOutput.init(_baseWidth, _baseHeight);
		glOutput.linear = Options::useOpenGLSmoothing;
		if (!noShaders && FileMap::fileExists(Options::useOpenGLShader))
		{
			if (!glOutput.set_shader(Options::useOpenGLShader.c_str()))
				Options::useOpenGLShader = "";
		}
		glOutput.setVSync(Options::vSyncForOpenGL);
#endif
	}
#else
	_topBlackBand = _bottomBlackBand = _leftBlackBand = _rightBlackBand = 0;
	_cursorTopBlackBand = _cursorLeftBlackBand = 0;
	(void)noShaders;
#endif

	setPalette(getPalette());
}

/**
 * Returns the screen's X scale.
 * @return Scale factor.
 */
double Screen::getXScale() const
{
	return _scaleX;
}

/**
 * Returns the screen's Y scale.
 * @return Scale factor.
 */
double Screen::getYScale() const
{
	return _scaleY;
}

/**
 * Returns the screen's top black forbidden to cursor band's height.
 * @return Height in pixel.
 */
int Screen::getCursorTopBlackBand() const
{
	return _cursorTopBlackBand;
}

/**
 * Returns the screen's left black forbidden to cursor band's width.
 * @return Width in pixel.
 */
int Screen::getCursorLeftBlackBand() const
{
	return _cursorLeftBlackBand;
}

/**
 * Saves a screenshot of the screen's contents.
 * @param filename Filename of the PNG file.
 */
void Screen::screenshot(const std::string &filename) const
{
	SDL_Surface *screenshot = SDL_CreateRGBSurface(0, getWidth() - getWidth()%4, getHeight(), 24, 0xff, 0xff00, 0xff0000, 0);

	if (useOpenGL())
	{
#ifndef __NO_OPENGL
		GLenum format = GL_RGB;

		for (int y = 0; y < getHeight(); ++y)
		{
			glReadPixels(0, getHeight()-(y+1), getWidth() - getWidth()%4, 1, format, GL_UNSIGNED_BYTE, ((Uint8*)screenshot->pixels) + y*screenshot->pitch);
		}
		glErrorCheck();
#endif
	}
	else
	{
		SDL_BlitSurface(_screen, 0, screenshot, 0);
	}
	std::vector<unsigned char> out;
	if (_screen->format->BitsPerPixel == 8 && Options::oxceRawScreenShots)
	{
		SDL_Color *palette = getPalette();
		lodepng::State state;
		for (size_t i = 0; i < 256; ++i)
		{
			SDL_Color color = palette[i];
			lodepng_palette_add(&state.info_png.color, color.r, color.g, color.b, 255);
			lodepng_palette_add(&state.info_raw, color.r, color.g, color.b, 255);
		}
		state.info_png.color.colortype = LCT_PALETTE; //if you comment this line, and create the above palette in info_raw instead, then you get the same image in a RGBA PNG.
		state.info_png.color.bitdepth = 8;
		state.info_raw.colortype = LCT_PALETTE;
		state.info_raw.bitdepth = 8;
		state.encoder.auto_convert = 0; //we specify ourselves exactly what output PNG color mode we want
		unsigned error = lodepng::encode(out, (const unsigned char *)(_surface->pixels), _surface->w, _surface->h, state);
		if (error)
		{
			Log(LOG_ERROR) << "Saving to PNG failed: " << lodepng_error_text(error);
		}
	}
	else
	{
		unsigned error = lodepng::encode(out, (const unsigned char *)(screenshot->pixels), getWidth() - getWidth()%4, getHeight(), LCT_RGB);
		if (error)
		{
			Log(LOG_ERROR) << "Saving to PNG failed: " << lodepng_error_text(error);
		}
	}

	SDL_FreeSurface(screenshot);

	CrossPlatform::writeFile(filename, out);
}


/**
 * Check whether a 32bpp scaler has been selected.
 * @return if it is enabled with a compatible resolution.
 */
bool Screen::use32bitScaler()
{
	int w = Options::displayWidth;
	int h = Options::displayHeight;
	int baseW = Options::baseXResolution;
	int baseH = Options::baseYResolution;
	int maxScale = 0;

	if (Options::useHQXFilter)
	{
		maxScale = 4;
	}
	else if (Options::useXBRZFilter)
	{
		maxScale = 6;
	}

	for (int i = 2; i <= maxScale; i++)
	{
		if (w == baseW * i && h == baseH * i)
		{
			return true;
		}
	}
	return false;
}

/**
 * Check if OpenGL is enabled.
 * @return if it is enabled.
 */
bool Screen::useOpenGL()
{
#ifdef __NO_OPENGL
	return false;
#else
	return Options::useOpenGL;
#endif
}

/**
 * Gets the Horizontal offset from the mid-point of the screen, in pixels.
 * @return the horizontal offset.
 */
int Screen::getDX() const
{
	return (_baseWidth - ORIGINAL_WIDTH) / 2;
}

/**
 * Gets the Vertical offset from the mid-point of the screen, in pixels.
 * @return the vertical offset.
 */
int Screen::getDY() const
{
	return (_baseHeight - ORIGINAL_HEIGHT) / 2;
}

/**
 * Changes a given scale, and if necessary, switch the current base resolution.
 * @param type the new scale level.
 * @param width reference to which x scale to adjust.
 * @param height reference to which y scale to adjust.
 * @param change should we change the current scale.
 */
void Screen::updateScale(int type, int &width, int &height, bool change)
{
#ifdef __EMSCRIPTEN__
	const Calypso::CalypsoScaleResult scale = Calypso::calypsoPromoteScale(
		Options::displayWidth, Options::displayHeight, Options::nonSquarePixelRatio, type);
	width = scale.width;
	height = scale.height;
#else
	double pixelRatioY = Options::nonSquarePixelRatio ? 1.2 : 1.0;
	switch (type)
	{
	case SCALE_15X:
		width = Screen::ORIGINAL_WIDTH * 1.5;
		height = Screen::ORIGINAL_HEIGHT * 1.5;
		break;
	case SCALE_2X:
		width = Screen::ORIGINAL_WIDTH * 2;
		height = Screen::ORIGINAL_HEIGHT * 2;
		break;
	case SCALE_SCREEN_DIV_10:
		width = Options::displayWidth / 10.0;
		height = Options::displayHeight / pixelRatioY / 10.0;
		break;
	case SCALE_SCREEN_DIV_8:
		width = Options::displayWidth / 8.0;
		height = Options::displayHeight / pixelRatioY / 8.0;
		break;
	case SCALE_SCREEN_DIV_6:
		width = Options::displayWidth / 6.0;
		height = Options::displayHeight / pixelRatioY / 6.0;
		break;
	case SCALE_SCREEN_DIV_5:
		width = Options::displayWidth / 5.0;
		height = Options::displayHeight / pixelRatioY / 5.0;
		break;
	case SCALE_SCREEN_DIV_4:
		width = Options::displayWidth / 4.0;
		height = Options::displayHeight / pixelRatioY / 4.0;
		break;
	case SCALE_SCREEN_DIV_3:
		width = Options::displayWidth / 3.0;
		height = Options::displayHeight / pixelRatioY / 3.0;
		break;
	case SCALE_SCREEN_DIV_2:
		width = Options::displayWidth / 2.0;
		height = Options::displayHeight / pixelRatioY  / 2.0;
		break;
	case SCALE_SCREEN:
		width = Options::displayWidth;
		height = Options::displayHeight / pixelRatioY;
		break;
	case SCALE_SCREEN_3_4:
		width = Options::displayWidth * 3 / 4;
		height = Options::displayHeight / pixelRatioY * 3 / 4;
		break;
	case SCALE_3X:
		width = Screen::ORIGINAL_WIDTH * 3;
		height = Screen::ORIGINAL_HEIGHT * 3;
		break;
	case SCALE_4X:
		width = Screen::ORIGINAL_WIDTH * 4;
		height = Screen::ORIGINAL_HEIGHT * 4;
		break;
	case SCALE_5X:
		width = Screen::ORIGINAL_WIDTH * 5;
		height = Screen::ORIGINAL_HEIGHT * 5;
		break;
	case SCALE_6X:
		width = Screen::ORIGINAL_WIDTH * 6;
		height = Screen::ORIGINAL_HEIGHT * 6;
		break;
	case SCALE_8X:
		width = Screen::ORIGINAL_WIDTH * 8;
		height = Screen::ORIGINAL_HEIGHT * 8;
		break;
	case SCALE_ORIGINAL:
	default:
		width = Screen::ORIGINAL_WIDTH;
		height = Screen::ORIGINAL_HEIGHT;
		break;
	}
#endif

	// don't go under minimum resolution... it's bad, mmkay?
	width = std::max(width, Screen::ORIGINAL_WIDTH);
	height = std::max(height, Screen::ORIGINAL_HEIGHT);

	if (change && (Options::baseXResolution != width || Options::baseYResolution != height))
	{
		Options::baseXResolution = width;
		Options::baseYResolution = height;
	}
}

/**
 * Maps a screen-relative ScaleType to its display fraction (num/den), e.g.
 * SCALE_SCREEN -> 1/1, SCALE_SCREEN_3_4 -> 3/4, SCALE_SCREEN_DIV_2 -> 1/2.
 * Legacy fixed-resolution scales (no longer offered in the Calypso menu) and any
 * unknown value fall back to 1/2 — a safe proportional default. The Battlescape
 * and Geoscape proportional resize paths and the Video-menu labels share this so
 * the stretched canvas always keeps the display aspect ratio.
 */
void Screen::getScreenScaleFraction(int type, int &num, int &den)
{
#ifdef __EMSCRIPTEN__
	Calypso::calypsoScaleFraction(type, num, den);
#else
	switch (type)
	{
	case SCALE_SCREEN:        num = 1; den = 1;  break;
	case SCALE_SCREEN_3_4:    num = 3; den = 4;  break;
	case SCALE_SCREEN_DIV_2:  num = 1; den = 2;  break;
	case SCALE_SCREEN_DIV_3:  num = 1; den = 3;  break;
	case SCALE_SCREEN_DIV_4:  num = 1; den = 4;  break;
	case SCALE_SCREEN_DIV_5:  num = 1; den = 5;  break;
	case SCALE_SCREEN_DIV_6:  num = 1; den = 6;  break;
	case SCALE_SCREEN_DIV_8:  num = 1; den = 8;  break;
	case SCALE_SCREEN_DIV_10: num = 1; den = 10; break;
	default:                  num = 1; den = 2;  break; // fixed/legacy → ½ fallback
	}
#endif
}

/**
 * Registers a per-frame GPU shader pass (Phase 8b).
 * The callable will be invoked once per flip() after SDL_RenderFlush,
 * and must save/restore all GL state around its own calls.
 */
void Screen::registerGPUPass(std::function<void()> pass)
{
	_gpuPasses.push_back(std::move(pass));
}

void Screen::registerGPUPassPreComposite(std::function<void()> pass)
{
	_gpuPassesPre.push_back(std::move(pass));
}

/**
 * Saves a screenshot by reading back the GPU framebuffer (Phase 8b).
 * Uses SDL_RenderReadPixels (not glReadPixels) so the path is renderer-agnostic.
 * Always requests SDL_PIXELFORMAT_RGBA32 to ensure RGBA byte order regardless
 * of backend endianness; lodepng expects RGBA.
 * Falls back to the CPU screenshot path when the GPU pipeline is not active.
 */
void Screen::screenshotGPU(const std::string& filename) const
{
	int w = getWidth(), h = getHeight();
	std::vector<unsigned char> pixels((size_t)w * h * 4);

	/* SDL_PIXELFORMAT_RGBA32 = RGBA on all endiannesses. */
	if (SDL_RenderReadPixels(_renderer, nullptr,
	                          SDL_PIXELFORMAT_RGBA32,
	                          pixels.data(), w * 4) != 0)
	{
		Log(LOG_ERROR) << "screenshotGPU: SDL_RenderReadPixels failed: " << SDL_GetError();
		screenshot(filename);
		return;
	}

	std::vector<unsigned char> png;
	unsigned err = lodepng::encode(png, pixels, (unsigned)w, (unsigned)h);
	if (err)
	{
		Log(LOG_ERROR) << "screenshotGPU: lodepng error " << err
		               << ": " << lodepng_error_text(err);
		return;
	}
	CrossPlatform::writeFile(filename, png);
	Log(LOG_DEBUG) << "GPU screenshot: " << filename;
}

}
