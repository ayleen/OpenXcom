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
#include <algorithm>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <climits>
#include <cstdio>
#include "../lodepng.h"
#include "Exception.h"
#include "Surface.h"
#include "Logger.h"
#include "Action.h"
#include "Options.h"
#include "CrossPlatform.h"
#include "FileMap.h"
#include "Zoom.h"
#ifdef __EMSCRIPTEN__
#include "HDQueue.h"
#include "FrameArena.h"
#endif
#include "Timer.h"
#include <SDL.h>
#ifdef __EMSCRIPTEN__
#include <SDL_render.h>
#endif
#include <algorithm>

namespace OpenXcom
{

const int Screen::ORIGINAL_WIDTH = 320;
const int Screen::ORIGINAL_HEIGHT = 200;

#ifndef __EMSCRIPTEN__
static const int VIDEO_WINDOW_POS_LEN = 40;
static char VIDEO_WINDOW_POS[VIDEO_WINDOW_POS_LEN];

static const char* SDL_VIDEO_CENTERED_UNSET = "SDL_VIDEO_CENTERED=";
static const char* SDL_VIDEO_CENTERED_CENTER = "SDL_VIDEO_CENTERED=center";
static const char* SDL_VIDEO_WINDOW_POS_UNSET = "SDL_VIDEO_WINDOW_POS=";
#endif

/**
 * Sets up all the internal display flags depending on
 * the current video settings.
 */
void Screen::makeVideoFlags()
{
#ifdef __EMSCRIPTEN__
	/* Under SDL2/Emscripten the window is created via SDL_CreateWindow in
	 * resetDisplay(); SDL1 flags are irrelevant. Just set bpp and base size. */
	_bpp = 8;
	_baseWidth  = Options::baseXResolution;
	_baseHeight = Options::baseYResolution;
#else
	_flags = SDL_HWSURFACE|SDL_DOUBLEBUF|SDL_HWPALETTE;
	if (Options::asyncBlit)
	{
		_flags |= SDL_ASYNCBLIT;
	}
	if (useOpenGL())
	{
		_flags = SDL_OPENGL;
		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 5 );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 5 );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 5 );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	}
	if (Options::allowResize)
	{
		_flags |= SDL_RESIZABLE;
	}

	// Handle window positioning
	if (!Options::fullscreen && Options::rootWindowedMode)
	{
		snprintf(VIDEO_WINDOW_POS, VIDEO_WINDOW_POS_LEN, "SDL_VIDEO_WINDOW_POS=%d,%d", Options::windowedModePositionX, Options::windowedModePositionY);
		SDL_putenv(VIDEO_WINDOW_POS);
		SDL_putenv((char *)SDL_VIDEO_CENTERED_UNSET);
	}
	else if (Options::borderless)
	{
		SDL_putenv((char *)SDL_VIDEO_WINDOW_POS_UNSET);
		SDL_putenv((char *)SDL_VIDEO_CENTERED_CENTER);
	}
	else
	{
		SDL_putenv((char *)SDL_VIDEO_WINDOW_POS_UNSET);
		SDL_putenv((char *)SDL_VIDEO_CENTERED_UNSET);
	}

	// Handle display mode
	if (Options::fullscreen)
	{
		_flags |= SDL_FULLSCREEN;
	}
	if (Options::borderless)
	{
		_flags |= SDL_NOFRAME;
	}

	_bpp = (use32bitScaler() || useOpenGL()) ? 32 : 8;
	_baseWidth = Options::baseXResolution;
	_baseHeight = Options::baseYResolution;
#endif
}


/**
 * Initializes a new display screen for the game to render contents to.
 * The screen is set up based on the current options.
 */
Screen::Screen() : _screen(nullptr), _baseWidth(ORIGINAL_WIDTH), _baseHeight(ORIGINAL_HEIGHT), _scaleX(1.0), _scaleY(1.0), _flags(0), _numColors(0), _firstColor(0), _pushPalette(false), _flickerFix(false)
#ifdef __EMSCRIPTEN__
	, _window(nullptr), _renderer(nullptr), _texture(nullptr)
#endif
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
	if (_texture)  { SDL_DestroyTexture(_texture);   _texture  = nullptr; }
	if (_renderer) { SDL_DestroyRenderer(_renderer); _renderer = nullptr; }
	if (_window)   { SDL_DestroyWindow(_window);     _window   = nullptr; }
	if (_screen)   { SDL_FreeSurface(_screen);       _screen   = nullptr; }
#endif
}

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

	if (action->getDetails()->type == SDL_KEYDOWN && action->getDetails()->key.keysym.sym == SDLK_RETURN && (SDL_GetModState() & KMOD_ALT) != 0)
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
void Screen::flip()
{
#ifdef __EMSCRIPTEN__
	{
		/* Browser window resize: the SDL2/Emscripten port does not emit
		 * SDL_WINDOWEVENT_SIZE_CHANGED reliably, so poll the canvas size each
		 * frame.  When it changes, update display options and let downstream
		 * states reset their base scale on the next updateScale call. */
		if (_window)
		{
			int wW = 0, wH = 0;
			SDL_GetWindowSize(_window, &wW, &wH);
			if (wW > 0 && wH > 0 &&
			    (wW != Options::displayWidth || wH != Options::displayHeight))
			{
				Options::displayWidth  = wW;
				Options::displayHeight = wH;
				/* Re-evaluate scale-driven base resolution for mainmenu/geoscape
				 * (battlescape state does its own per-state recompute). */
				Screen::updateScale(Options::geoscapeScale,
				                    Options::baseXGeoscape,
				                    Options::baseYGeoscape,
				                    true);
			}
		}

		/* States call Screen::updateScale which mutates Options::baseXResolution
		 * but does not call resetDisplay.  When the requested base differs from
		 * what _surface/_screen/_texture were sized to, recreate them at the new
		 * size; otherwise the state would draw partial geometry into a smaller
		 * surface.  Window/renderer are kept (resetVideo=false). */
		if (_surface && (_surface->w != Options::baseXResolution
		              || _surface->h != Options::baseYResolution))
		{
			resetDisplay(false, false);
		}

		/* Blit internal 8bpp surface into RGBA32 _screen via SDL2 cross-format
		 * blit (SDL2 performs palette lookup automatically from _surface's palette). */
		SDL_BlitSurface(_surface.get(), nullptr, _screen, nullptr);

		HDQueue::flush(_screen);
		// Arena reset MUST come after flush — surfaces are referenced by HDQueue
		// entries until flush() completes. Resetting before flush dangling-pointers them.
		frameArena().reset();

		/* Upload RGBA32 pixels to streaming texture and present. */
		void *texPixels;
		int   texPitch;
		SDL_LockTexture(_texture, nullptr, &texPixels, &texPitch);
		if (texPitch == _screen->pitch) {
			memcpy(texPixels, _screen->pixels, (size_t)_screen->h * texPitch);
		} else {
			for (int y = 0; y < _screen->h; y++) {
				memcpy((char*)texPixels + y * texPitch,
				       (char*)_screen->pixels + y * _screen->pitch,
				       (size_t)_screen->w * 4);
			}
		}
		SDL_UnlockTexture(_texture);
		SDL_RenderClear(_renderer);
		SDL_RenderCopy(_renderer, _texture, nullptr, nullptr);
		SDL_RenderPresent(_renderer);

		_numColors = 0;
		_pushPalette = false;
		return;
	}
#else /* !__EMSCRIPTEN__ — native SDL1 flip path */

	// perform any requested palette update
	if (_flickerFix && _pushPalette && _numColors && _screen->format->BitsPerPixel == 8)
	{
		if (_screen->format->BitsPerPixel == 8 && SDL_SetColors(_screen, &(deferredPalette[_firstColor]), _firstColor, _numColors) == 0)
		{
			Log(LOG_DEBUG) << "Display palette doesn't match requested palette";
		}
		_numColors = 0;
		_pushPalette = false;
	}

	if (getWidth() != _baseWidth || getHeight() != _baseHeight || useOpenGL())
	{
		Zoom::flipWithZoom(_surface.get(), _screen, _topBlackBand, _bottomBlackBand, _leftBlackBand, _rightBlackBand, &glOutput);
	}
	else
	{
		SDL_BlitSurface(_surface.get(), 0, _screen, 0);
	}

	// perform any requested palette update
	if (!_flickerFix && _pushPalette && _numColors && _screen->format->BitsPerPixel == 8)
	{
		if (_screen->format->BitsPerPixel == 8 && SDL_SetColors(_screen, &(deferredPalette[_firstColor]), _firstColor, _numColors) == 0)
		{
			Log(LOG_DEBUG) << "Display palette doesn't match requested palette";
		}
		_numColors = 0;
		_pushPalette = false;
	}

	if (SDL_Flip(_screen) == -1)
	{
		throw Exception(SDL_GetError());
	}
#endif /* __EMSCRIPTEN__ */
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

	SDL_SetColors(_surface.get(), const_cast<SDL_Color *>(colors), firstcolor, ncolors);

	// defer actual update of screen until SDL_Flip()
	if (immediately && _screen->format->BitsPerPixel == 8 && SDL_SetColors(_screen, const_cast<SDL_Color *>(colors), firstcolor, ncolors) == 0)
	{
		Log(LOG_DEBUG) << "Display palette doesn't match requested palette";
	}

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
#ifdef __EMSCRIPTEN__
	(void)noShaders;

	int width  = Options::displayWidth;
	int height = Options::displayHeight;
	makeVideoFlags(); /* sets _bpp=8, _baseWidth, _baseHeight */

	/* (Re)allocate 8bpp internal surface if size or bpp changed. */
	if (!_surface || _surface->format->BitsPerPixel != 8 ||
	    _surface->w != _baseWidth || _surface->h != _baseHeight)
	{
		std::tie(_buffer, _surface) = Surface::NewPair8Bit(_baseWidth, _baseHeight);
		SDL_SetColors(_surface.get(), deferredPalette, 0, 256);
	}
	SDL_SetColorKey(_surface.get(), 0, 0);

	/* Recreate _screen and _texture if base size changed (state called
	 * Screen::updateScale).  Cheap — no window/renderer churn. */
	if (!resetVideo && _window && _screen
	    && (_screen->w != _baseWidth || _screen->h != _baseHeight))
	{
		if (_texture) { SDL_DestroyTexture(_texture);  _texture = nullptr; }
		SDL_FreeSurface(_screen); _screen = nullptr;

		_screen = SDL_CreateRGBSurface(0, _baseWidth, _baseHeight, 32,
		    0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
		if (!_screen) throw Exception(SDL_GetError());

		_texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ARGB8888,
		    SDL_TEXTUREACCESS_STREAMING, _baseWidth, _baseHeight);
		if (!_texture) throw Exception(SDL_GetError());

		/* Match canvas backing-store to the texture so SDL_RenderCopy is a
		 * 1:1 identity copy (Emscripten SDL2's WebGL renderer doesn't honour
		 * SDL_RenderSetLogicalSize/dst rect stretching reliably).  CSS scales
		 * the canvas to fill the viewport via `width: 100vw; height: 100vh`. */
		SDL_SetWindowSize(_window, _baseWidth, _baseHeight);

		Log(LOG_INFO) << "Display rebased to " << _baseWidth << "x" << _baseHeight << " (RGBA staging).";
	}

	if (resetVideo || !_window)
	{
		Log(LOG_INFO) << "Emscripten: creating SDL2 window " << width << "x" << height;

		/* Destroy any previous SDL2 objects. */
		if (_texture)  { SDL_DestroyTexture(_texture);   _texture  = nullptr; }
		if (_renderer) { SDL_DestroyRenderer(_renderer); _renderer = nullptr; }
		if (_screen)   { SDL_FreeSurface(_screen);       _screen   = nullptr; }
		if (_window)   { SDL_DestroyWindow(_window);     _window   = nullptr; }

		Uint32 winFlags = SDL_WINDOW_OPENGL;
		if (Options::allowResize) winFlags |= SDL_WINDOW_RESIZABLE;

		_window = SDL_CreateWindow("Calypso",
		    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		    width, height, winFlags);
		if (!_window)
		{
			Log(LOG_ERROR) << "SDL_CreateWindow failed: " << SDL_GetError();
			throw Exception(SDL_GetError());
		}

		_renderer = SDL_CreateRenderer(_window, -1,
		    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
		if (!_renderer)
		{
			Log(LOG_ERROR) << "SDL_CreateRenderer failed: " << SDL_GetError();
			throw Exception(SDL_GetError());
		}
		/* Match canvas backing to texture size; CSS will stretch.  Avoids the
		 * Emscripten SDL2 port's broken SDL_RenderSetLogicalSize/dst-rect
		 * stretching for WebGL textures. */
		SDL_SetWindowSize(_window, _baseWidth, _baseHeight);

		/* RGBA32 staging surface: 8bpp _surface blits into this before texture upload. */
		_screen = SDL_CreateRGBSurface(0, _baseWidth, _baseHeight, 32,
		    0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
		if (!_screen)
		{
			Log(LOG_ERROR) << "SDL_CreateRGBSurface failed: " << SDL_GetError();
			throw Exception(SDL_GetError());
		}

		_texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ARGB8888,
		    SDL_TEXTUREACCESS_STREAMING, _baseWidth, _baseHeight);
		if (!_texture)
		{
			Log(LOG_ERROR) << "SDL_CreateTexture failed: " << SDL_GetError();
			throw Exception(SDL_GetError());
		}

		Log(LOG_INFO) << "Display set to " << getWidth() << "x" << getHeight() << "x32 (RGBA staging).";
	}
	else
	{
		clear();
	}

	Options::displayWidth  = getWidth();
	Options::displayHeight = getHeight();
	_scaleX = 1.0;
	_scaleY = 1.0;
	_topBlackBand = _bottomBlackBand = _leftBlackBand = _rightBlackBand = 0;
	_cursorTopBlackBand = _cursorLeftBlackBand = 0;

	setPalette(getPalette());
	return;
#else /* !__EMSCRIPTEN__ */

#if defined __linux__ || defined _WIN32 || defined  __CYGWIN__
	Uint32 oldFlags = _flags;
#endif

	int width = Options::displayWidth;
	int height = Options::displayHeight;
	makeVideoFlags();

	if (!_surface || (_surface->format->BitsPerPixel != _bpp ||
		_surface->w != _baseWidth ||
		_surface->h != _baseHeight)) // don't reallocate _surface if not necessary, it's a waste of CPU cycles
	{
		if (_bpp == 32)
		{
			std::tie(_buffer, _surface) = Surface::NewPair32Bit(_baseWidth, _baseHeight);
		}
		else
		{
			std::tie(_buffer, _surface) = Surface::NewPair8Bit(_baseWidth, _baseHeight);
		}

		if (_surface->format->BitsPerPixel == 8)
		{
			SDL_SetColors(_surface.get(), deferredPalette, 0, 255);
		}
	}
	SDL_SetColorKey(_surface.get(), 0, 0); // turn off color key!

	if (resetVideo || _screen->format->BitsPerPixel != _bpp)
	{
		Log(LOG_INFO) << "Attempting to set display to " << width << "x" << height << "x" << _bpp << "...";

#if defined __linux__ || defined _WIN32 || defined  __CYGWIN__
		// Workaround for segfault when switching to opengl
		if ((oldFlags & SDL_OPENGL) != (_flags & SDL_OPENGL))
		{
			Uint8 cursor = 0;
			char *_oldtitle = 0;
			SDL_WM_GetCaption(&_oldtitle, NULL);
			std::string title(_oldtitle);
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
			SDL_InitSubSystem(SDL_INIT_VIDEO);

			// recreate operations done by `Game::Game` constructor
			SDL_ShowCursor(SDL_ENABLE);
			SDL_EnableUNICODE(1);
			SDL_WM_SetCaption(title.c_str(), 0);
			SDL_WM_GrabInput(Options::captureMouse);
			SDL_SetCursor(SDL_CreateCursor(&cursor, &cursor, 1,1,0,0));
		}
#endif
		_screen = SDL_SetVideoMode(width, height, _bpp, _flags);
		if (_screen == 0)
		{
			Log(LOG_ERROR) << SDL_GetError();
			Log(LOG_INFO) << "Attempting to set display to default resolution...";
			_screen = SDL_SetVideoMode(640, 400, _bpp, _flags);
			if (_screen == 0)
			{
				if (_flags & SDL_OPENGL)
				{
					Options::useOpenGL = false;
				}
				throw Exception(SDL_GetError());
			}
		}
		Log(LOG_INFO) << "Display set to " << getWidth() << "x" << getHeight() << "x" << (int)_screen->format->BitsPerPixel << ".";
	}
	else
	{
		clear();
	}

	Options::displayWidth = getWidth();
	Options::displayHeight = getHeight();
	_scaleX = getWidth() / (double)_baseWidth;
	_scaleY = getHeight() / (double)_baseHeight;

	double pixelRatioY = 1.0;
	if (Options::nonSquarePixelRatio && !Options::allowResize)
	{
		pixelRatioY = 1.2;
	}
	bool cursorInBlackBands;
	if (!Options::keepAspectRatio)
	{
		cursorInBlackBands = false;
	}
	else if (Options::fullscreen)
	{
		cursorInBlackBands = Options::cursorInBlackBandsInFullscreen;
	}
	else if (!Options::borderless)
	{
		cursorInBlackBands = Options::cursorInBlackBandsInWindow;
	}
	else
	{
		cursorInBlackBands = Options::cursorInBlackBandsInBorderlessWindow;
	}

	if (_scaleX > _scaleY && Options::keepAspectRatio)
	{
		int targetWidth = (int)floor(_scaleY * (double)_baseWidth);
		_topBlackBand = _bottomBlackBand = 0;
		_leftBlackBand = (getWidth() - targetWidth) / 2;
		if (_leftBlackBand < 0)
		{
			_leftBlackBand = 0;
		}
		_rightBlackBand = getWidth() - targetWidth - _leftBlackBand;
		_cursorTopBlackBand = 0;

		if (cursorInBlackBands)
		{
			_scaleX = _scaleY;
			_cursorLeftBlackBand = _leftBlackBand;
		}
		else
		{
			_cursorLeftBlackBand = 0;
		}
	}
	else if (_scaleY > _scaleX && Options::keepAspectRatio)
	{
		int targetHeight = (int)floor(_scaleX * (double)_baseHeight * pixelRatioY);
		_topBlackBand = (getHeight() - targetHeight) / 2;
		if (_topBlackBand < 0)
		{
			_topBlackBand = 0;
		}
		_bottomBlackBand = getHeight() - targetHeight - _topBlackBand;
		if (_bottomBlackBand < 0)
		{
			_bottomBlackBand = 0;
		}
		_leftBlackBand = _rightBlackBand = 0;
		_cursorLeftBlackBand = 0;

		if (cursorInBlackBands)
		{
			_scaleY = _scaleX;
			_cursorTopBlackBand = _topBlackBand;
		}
		else
		{
			_cursorTopBlackBand = 0;
		}
	}
	else
	{
		_topBlackBand = _bottomBlackBand = _leftBlackBand = _rightBlackBand = _cursorTopBlackBand = _cursorLeftBlackBand = 0;
	}

	if (useOpenGL())
	{
#ifndef __NO_OPENGL
		OpenGL::checkErrors = Options::checkOpenGLErrors;
		glOutput.init(_baseWidth, _baseHeight);
		glOutput.linear = Options::useOpenGLSmoothing; // setting from shader file will override this, though
		if (!noShaders && FileMap::fileExists(Options::useOpenGLShader))
		{
			if (!glOutput.set_shader(Options::useOpenGLShader.c_str()))
			{
				Options::useOpenGLShader = "";
			}
		}
		glOutput.setVSync(Options::vSyncForOpenGL);
#endif
	}

	if (_screen->format->BitsPerPixel == 8)
	{
		setPalette(getPalette());
	}
#endif /* !__EMSCRIPTEN__ */
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
	double pixelRatioY = 1.0;

	if (Options::nonSquarePixelRatio)
	{
		pixelRatioY = 1.2;
	}

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
	case SCALE_ORIGINAL:
	default:
		width = Screen::ORIGINAL_WIDTH;
		height = Screen::ORIGINAL_HEIGHT;
		break;
	}

	// don't go under minimum resolution... it's bad, mmkay?
	width = std::max(width, Screen::ORIGINAL_WIDTH);
	height = std::max(height, Screen::ORIGINAL_HEIGHT);

	if (change && (Options::baseXResolution != width || Options::baseYResolution != height))
	{
		Options::baseXResolution = width;
		Options::baseYResolution = height;
	}
}

}
