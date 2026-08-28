	#pragma once
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
#include <SDL.h>
#include <string>
#include <functional>
#include <vector>
#include <cstdint>
#include "OpenGL.h"
#include "Surface.h"

namespace OpenXcom
{

class Surface;
class Action;
class Screen;

namespace Calypso
{
bool calypsoScreenRecreateRendererGL(Screen &);
void calypsoScreenUploadLogicalTexture(Screen &);
bool calypsoScreenFlipWorldPass(Screen &, bool);
bool calypsoScreenRenderChrome(Screen &);
void calypsoScreenResetDisplayRendererOnly(Screen &);
void calypsoScreenRefreshLogicalTexture(Screen &);
void calypsoScreenRebaseStagingSurface(Screen &, int, int);
}

struct ScreenWorldPassHandle
{
	Screen *owner = nullptr;
	std::uint64_t id = 0u;
	bool valid() const { return owner != nullptr && id != 0u; }
};

/**
 * A display screen, handles rendering onto the game window.
 * In SDL a Screen is treated like a Surface, so this is just
 * a specialized version of a Surface with functionality more
 * relevant for display screens. Contains a Surface buffer
 * where all the contents are kept, so any filters or conversions
 * can be applied before rendering the screen.
 */
class Screen
{
private:
	SDL_Surface *_screen;
	SDL_Window   *_window;
	SDL_Renderer *_renderer;
	SDL_Texture  *_texture;
	int _bpp;
	int _baseWidth, _baseHeight;
	double _scaleX, _scaleY;
	int _topBlackBand, _bottomBlackBand, _leftBlackBand, _rightBlackBand, _cursorTopBlackBand, _cursorLeftBlackBand;
	SDL_Color deferredPalette[256];
	int _numColors, _firstColor;
	bool _pushPalette;
	bool _flickerFix;
	OpenGL glOutput;
	Surface::UniqueBufferPtr _buffer;
	Surface::UniqueSurfacePtr _surface;
	/** GPU passes that fire BEFORE the SDL surface composite (Phase 13.3). */
	std::vector<std::function<void()>> _gpuPassesPre;
	/** Registered physical world passes: after SDL_RenderCopy, before HD chrome. */
	struct WorldPassEntry
	{
		std::uint64_t id;
		std::function<void()> pass;
		bool removed = false;
	};
	std::vector<WorldPassEntry> _gpuPassesWorld;
	std::vector<WorldPassEntry> _gpuPendingWorldPasses;
	std::uint64_t _nextGpuWorldPassId = 1u;
	bool _gpuWorldPassDispatching = false;
	bool _gpuWorldPassNeedsCompaction = false;
	void finishGPUPassWorldDispatch();
	/** GPU passes registered via registerGPUPass — called each frame in flip(). */
	std::vector<std::function<void()>> _gpuPasses;
	/** Frame counter for periodic GPU pass timing logs (Phase 8b.9). */
	unsigned _gpuFrameCount = 0u;
	/** Accumulated GPU pass wall-clock time for the current 60-frame window (µs). */
	long long _gpuPassAccumUs = 0;
	/// Sets _bpp, _baseWidth, _baseHeight from current options.
	void makeVideoFlags();
#ifdef __EMSCRIPTEN__
	/// Force the canvas-size rebase block in flip() to run on the next frame even
	/// when the polled canvas dimensions match Options::displayWidth/Height.
	/// Set by recreateRendererGL() after a successful context recovery so
	/// Screen::updateScale re-derives all scale state without the side effect of
	/// computing a fake old-size of 0 that shifts the battlescape HUD off-screen.
	bool _forceCanvasRebase = false;
	/// Destroys and re-creates _renderer + _texture after a WebGL context restore.
	/// Called by handle() on SDL_RENDER_TARGETS_RESET before ShaderManager::reuploadAll().
	bool recreateRendererGL();
	friend bool Calypso::calypsoScreenRecreateRendererGL(Screen &);
	friend void Calypso::calypsoScreenUploadLogicalTexture(Screen &);
	friend bool Calypso::calypsoScreenFlipWorldPass(Screen &, bool);
	friend bool Calypso::calypsoScreenRenderChrome(Screen &);
	friend void Calypso::calypsoScreenResetDisplayRendererOnly(Screen &);
	friend void Calypso::calypsoScreenRefreshLogicalTexture(Screen &);
	friend void Calypso::calypsoScreenRebaseStagingSurface(Screen &, int, int);
#endif
public:
	using WorldPassHandle = ScreenWorldPassHandle;
	static const int ORIGINAL_WIDTH;
	static const int ORIGINAL_HEIGHT;

	/// Creates a new display screen.
	Screen();
	/// Cleans up the display screen.
	~Screen();
	/// Get horizontal offset.
	int getDX() const;
	/// Get vertical offset.
	int getDY() const;
	/// Gets the internal buffer.
	SDL_Surface *getSurface();
	/// Handles keyboard events.
	void handle(Action *action);
	/// Renders the screen onto the game window. Returns true iff this call
	/// actually presented (SDL_RenderPresent / SDL_GL_SwapWindow) — skipped
	/// presents (context loss, HD-overlay gate, missing GL buffer) return
	/// false so callers can distinguish a real presentation from a no-op.
	bool flip();
	/// Clears the screen.
	void clear();
	/// Sets the screen's 8bpp palette.
	void setPalette(const SDL_Color *colors, int firstcolor = 0, int ncolors = 256, bool immediately = false);
	/// Gets the screen's 8bpp palette.
	SDL_Color *getPalette() const;
	/// Gets the screen's width.
	int getWidth() const;
	/// Gets the screen's height.
	int getHeight() const;
	/// Resets the screen display.
	void resetDisplay(bool resetVideo = true, bool noShaders = false);
	/// Gets the screen's X scale.
	double getXScale() const;
	/// Gets the screen's Y scale.
	double getYScale() const;
	/// Gets the screen's top black forbidden to cursor band's height.
	int getCursorTopBlackBand() const;
	/// Gets the screen's left black forbidden to cursor band's width.
	int getCursorLeftBlackBand() const;
	/// Takes a screenshot from the CPU surface.
	void screenshot(const std::string &filename) const;
	/// Takes a screenshot by reading back the GPU framebuffer (Phase 8b).
	void screenshotGPU(const std::string &filename) const;
	/** Register a per-frame GPU shader pass.  The callable must save and restore
	 *  all GL state (program, VAO, blend, depth) around its own GL calls.
	 *  SDL_RenderFlush is called before the first pass each frame. */
	void registerGPUPass(std::function<void()> pass);
	/** Register a pass that fires BEFORE the SDL surface composite (Phase 13.3).
	 *  Use for HD tile geometry so it renders under CPU-drawn units / HUD. */
	void registerGPUPassPreComposite(std::function<void()> pass);
	/** Register a physical world pass after the SDL composite and before HD chrome. */
	WorldPassHandle registerGPUPassWorld(std::function<void()> pass);
	void unregisterGPUPassWorld(WorldPassHandle handle);
	/// Checks whether a 32bit scaler is requested and works for the selected resolution
	static bool use32bitScaler();
	/// Checks whether OpenGL output is requested
	static bool useOpenGL();
	/// update the game scale as required.
	static void updateScale(int type, int &width, int &height, bool change);
	/// Maps a screen-relative ScaleType to its display fraction (num/den).
	/// Used by the Battlescape/Geoscape resize() proportional path and the
	/// Video-menu labels so the canvas keeps the display aspect ratio.
	static void getScreenScaleFraction(int scaleType, int &num, int &den);
#ifdef __EMSCRIPTEN__
	/// Independently promote invalid live and pending browser scene fractions.
	static void normalizeBrowserScales();
	/// Apply one bridge-authorized canvas-size change (flip() resize path) as a
	/// single reflow via the Calypso viewport bridge. Body lives in
	/// src/Calypso/CalypsoBrowserScale.cpp (policy R3). Phase 46.4 10.2.9:
	/// classifies the polled canvas through CalypsoBackingStorePolicy first --
	/// only an exactly matching PENDING viewport notification adopts; any
	/// other divergence is restored to Options::displayWidth/Height via the
	/// Emscripten canvas-size API without adopting or reflowing layout.
	/// Returns true when such a restoration happened -- the caller (flip)
	/// must return false for that frame so the stale HD frame is never
	/// presented.
	bool reflowCanvasFallback(int canvasWidth, int canvasHeight);
#endif
};

}
