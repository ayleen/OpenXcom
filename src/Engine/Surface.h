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
#include "SDL2Helpers.h"
#include <string>
#include <vector>
#include <memory>
#include <vector>
#include <assert.h>
#include "GraphSubset.h"

namespace OpenXcom
{

class Font;
class Language;
class ScriptWorkerBase;
class SurfaceCrop;
class ShadeTable;
template<typename Pixel> class SurfaceRaw;

/**
 * Element that is blit (rendered) onto the screen.
 * Mainly an encapsulation for SDL's SDL_Surface struct, so it
 * borrows a lot of its terminology. Takes care of all the common
 * rendering tasks and color effects, while serving as the base
 * class for more specialized screen elements.
 */
class Surface
{
public:
	struct UniqueBufferDeleter
	{
		void operator()(Uint8*);
	};
	struct UniqueSurfaceDeleter
	{
		void operator()(SDL_Surface*);
	};

	using UniqueBufferPtr = std::unique_ptr<Uint8, UniqueBufferDeleter>;
	using UniqueSurfacePtr = std::unique_ptr<SDL_Surface, UniqueSurfaceDeleter>;

	/// Create aligned buffer for surface.
	static UniqueBufferPtr NewAlignedBuffer(int bpp, int width, int height);
	/// Smart pointer for for SDL_Surface.
	static UniqueSurfacePtr NewSdlSurface(SDL_Surface* surface);
	/// Create surface from aligned buffer.
	static UniqueSurfacePtr NewSdlSurface(const UniqueBufferPtr& buffer, int bpp, int width, int height);
	/// Create buffer and surface.
	static std::pair<UniqueBufferPtr, UniqueSurfacePtr> NewPair32Bit(int width, int height)
	{
		auto tempBuffer = Surface::NewAlignedBuffer(32, width, height);
		auto tempSurface = Surface::NewSdlSurface(tempBuffer, 32, width, height);
		return std::make_pair(std::move(tempBuffer), std::move(tempSurface));
	}

	/// Create 8bpp buffer+surface for palette-index scratch use during loading.
	/// Palette loaders write indices here; setPalette() rebuilds the shade table from palette.
	static std::pair<UniqueBufferPtr, UniqueSurfacePtr> NewLoadScratch8Bit(int width, int height)
	{
		auto tempBuffer = Surface::NewAlignedBuffer(8, width, height);
		auto tempSurface = Surface::NewSdlSurface(tempBuffer, 8, width, height);
		return std::make_pair(std::move(tempBuffer), std::move(tempSurface));
	}

	/// Zero whole surface.
	static void CleanSdlSurface(SDL_Surface* surface);

protected:
	UniqueBufferPtr _alignedBuffer;
	UniqueSurfacePtr _surface;
	Sint16 _x, _y;
	Uint16 _width, _height, _pitch;
	Uint8 _visible: 1;
	Uint8 _hidden: 1;
	Uint8 _redraw: 1;
#ifdef __EMSCRIPTEN__
	bool _isHD = false;
	Uint16 _logicalW = 0, _logicalH = 0;
#endif
	// Saved copy of the most recent setPalette call on ARGB surfaces (no SDL palette object).
	// Cross-platform: native build may also use ARGB surfaces after 7.K.
	SDL_Color _savedPalette[256] = {};
	bool _hasSavedPalette = false;
	// Phase 7: per-asset shade table (cross-platform; guard removed in 7.K).
	std::shared_ptr<ShadeTable> _shadeTable;
	// Cycle-phase auxiliary tables for palette-cycling assets (empty for static assets).
	std::vector<std::shared_ptr<ShadeTable>> _shadeCycle;

	/// Copies raw pixels.
	template <typename T>
	void rawCopy(const std::vector<T> &bytes);
	/// Resizes the surface.
	void resize(int width, int height);
	/// Rebuilds shade table from current 8bpp SDL palette if one exists.
	void rebuildShadeTable();
public:
	/// Default empty surface.
	Surface();
	/// Creates a new surface with the specified size and position.
	/// 7.C: on Emscripten this produces a 32bpp ARGB surface by default.
	Surface(int width, int height, int x = 0, int y = 0);
	/// Creates a new surface from an existing one.
	Surface(const Surface& other);
	/// Move surface to another place.
	Surface(Surface&& other) = default;
	/// Move assignment
	Surface& operator=(Surface&& other) = default;
	/// Copy assignment
	Surface& operator=(const Surface& other) { *this = Surface(other); return *this; };
	/// Cleans up the surface.
	virtual ~Surface();

	/// Is surface empty?
	explicit operator bool() const
	{
#ifdef __EMSCRIPTEN__
		return _alignedBuffer.get() || _surface.get();
#else
		return _alignedBuffer.get();
#endif
	}

	/// Loads a raw pixel array.
	void loadRaw(const std::vector<unsigned char> &bytes);
	/// Loads a raw pixel array.
	void loadRaw(const std::vector<char> &bytes);
	/// Loads an X-Com SCR graphic.
	void loadScr(const std::string &filename);
	/// Loads an X-Com SPK graphic.
	void loadSpk(const std::string &filename);
	/// Loads a TFTD BDY graphic.
	void loadBdy(const std::string &filename);
	/// Loads a general image file.
	void loadImage(const std::string &filename);
	/// Loads a 32-bit RGBA image (HD asset path — preserves alpha, skips palette quantization).
	void loadImageHD(const std::string &filename);
#ifdef __EMSCRIPTEN__
	/// Convert a shade level (0=full … 15=black) to an SDL colour-mod byte (255…0).
	/// Single source of truth for the calibration curve; both shadeARGB() and
	/// SDL_SetSurfaceColorMod callers use this so the curve is never duplicated.
	/// Curve calibrated vs palette shading; see tests/regression/shade-curve.md.
	static inline Uint8 shadeToColorMod(int shade)
	{
		static const float k[16] = {
			1.00f, 0.93f, 0.87f, 0.80f, 0.74f, 0.67f, 0.60f, 0.53f,
			0.47f, 0.40f, 0.33f, 0.27f, 0.20f, 0.13f, 0.07f, 0.00f
		};
		return (Uint8)(k[shade & 0x0f] * 255.0f);
	}
	/// Apply linear-RGB shade attenuation to one ARGB pixel (HD-only path).
	/// shade 0 = full brightness … shade 15 = black.  Alpha is preserved.
	static inline void shadeARGB(Uint32 &dst, Uint32 src, int shade)
	{
		if ((src >> 24) == 0) { dst = 0; return; }
		float m = shadeToColorMod(shade) / 255.0f;
		Uint8 a = (src >> 24) & 0xff;
		Uint8 r = (src >> 16) & 0xff;
		Uint8 g = (src >>  8) & 0xff;
		Uint8 b =  src        & 0xff;
		dst = ((Uint32)a       << 24)
		    | ((Uint32)(Uint8)(r * m) << 16)
		    | ((Uint32)(Uint8)(g * m) <<  8)
		    |  (Uint32)(Uint8)(b * m);
	}
	/// Returns true if this surface carries an HD (ARGB8888) image.
	bool isHD() const { return _isHD; }
	/// Sets the logical (game-coordinate) size and pre-scales the HD surface to match.
	void setLogicalSize(int w, int h);
	/// Returns true if this surface is in 32bpp ARGB mode.
	bool isARGB() const { return _surface && _surface->format->BitsPerPixel == 32; }
#endif
	// Phase 7: shade table accessors (cross-platform; guard removed in 7.K).
	/// Returns the primary shade table, or nullptr if not yet built.
	const ShadeTable *getShadeTable() const { return _shadeTable.get(); }

	/// Returns the cycle-phase shade table, or the primary table as fallback.
	const ShadeTable *getShadeTable(int cyclePhase) const;
	/// Attaches a pre-built shade table (e.g. recoloured variant from the cache).
	void attachShadeTable(std::shared_ptr<ShadeTable> t) { _shadeTable = std::move(t); }
	/// Replaces the full palette-cycle table list (7.A.4).
	void attachShadeCycle(std::vector<std::shared_ptr<ShadeTable>> cycle) { _shadeCycle = std::move(cycle); }
	/// Builds shade table from an externally-provided palette (for ARGB surfaces that
	/// received direct ARGB pixel writes in loaders, bypassing the normal setPalette flow).
	void rebuildShadeTableFromPalette(const SDL_Color *pal, int ncolors = 256);
	/// Returns getPalette() if available (8bpp), else _savedPalette (ARGB surface
	/// that received setPalette after promotion), else nullptr.
	const SDL_Color *getEffectivePalette() const
	{
		const SDL_Color *p = getPalette();
		if (!p && _hasSavedPalette) return _savedPalette;
		return p;
	}
	/// Clears the surface's contents with a specified colour.
	void clear();
	/// Offsets the surface's colors by a set amount.
	void offset(int off, int min = -1, int max = -1, int mul = 1);
	/// Offsets the surface's colors in a color block.
	void offsetBlock(int off, int blk = 16, int mul = 1);
	/// Inverts the surface's colors.
	void invert(Uint8 mid);
	/// Runs surface functionality every cycle
	virtual void think();
	/// Draws the surface's graphic.
	virtual void draw();
	/// Blits this surface onto another one.
	virtual void blit(SDL_Surface *surface);
	/// Initializes the surface's various text resources.
	virtual void initText(Font *, Font *, Language *) {};
	/// Copies a portion of another surface into this one.
	void copy(Surface *surface);
	/// Draws a filled rectangle on the surface.
	void drawRect(SDL_Rect *rect, Uint8 color);
	/// Draws a filled rectangle on the surface.
	void drawRect(Sint16 x, Sint16 y, Sint16 w, Sint16 h, Uint8 color);
	/// Draws a line on the surface.
	void drawLine(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 color);
	/// Draws a filled circle on the surface.
	void drawCircle(Sint16 x, Sint16 y, Sint16 r, Uint8 color);
	/// Draws a filled polygon on the surface.
	void drawPolygon(Sint16 *x, Sint16 *y, int n, Uint8 color);
	/// Draws a textured polygon on the surface.
	void drawTexturedPolygon(Sint16 *x, Sint16 *y, int n, Surface *texture, int dx, int dy);
	/// Draws a string on the surface.
	void drawString(Sint16 x, Sint16 y, const char *s, Uint8 color);
	/// Sets the surface's palette.
	virtual void setPalette(const SDL_Color *colors, int firstcolor = 0, int ncolors = 256);
	/**
	 * Returns the surface's 8bpp palette, or nullptr for ARGB surfaces.
	 * @return Pointer to the palette's colors, or nullptr.
	 */
	SDL_Color *getPalette() const
	{
		if (!_surface || !_surface->format->palette) return nullptr;
		return _surface->format->palette->colors;
	}
	/// Sets the X position of the surface.
	virtual void setX(int x);
	/**
	 * Returns the position of the surface in the X axis.
	 * @return X position in pixels.
	 */
	int getX() const
	{
		return _x;
	}
	/// Sets the Y position of the surface.
	virtual void setY(int y);
	/**
	 * Returns the position of the surface in the Y axis.
	 * @return Y position in pixels.
	 */
	int getY() const
	{
		return _y;
	}
	/// Sets the surface's visibility.
	virtual void setVisible(bool visible);
	/// Gets the surface's visibility.
	bool getVisible() const;
	/// Gets the cropping rectangle for the surface.
	SurfaceCrop getCrop() const;
	/// Writes a 32bpp ARGB pixel. Only valid on ARGB surfaces.
	void setPixel32(int x, int y, Uint32 pixel)
	{
		if (x < 0 || x >= getWidth() || y < 0 || y >= getHeight()) return;
		*(Uint32*)getRaw(x, y) = pixel;
	}
	/**
	 * Changes the color of a pixel in the surface, relative to
	 * the top-left corner of the surface. Invalid positions are ignored.
	 * @param x X position of the pixel.
	 * @param y Y position of the pixel.
	 * @param pixel New color for the pixel.
	 */
	void setPixel(int x, int y, Uint8 pixel)
	{
		if (x < 0 || x >= getWidth() || y < 0 || y >= getHeight())
		{
			return;
		}
#ifdef __EMSCRIPTEN__
		if (_surface && _surface->format->BitsPerPixel == 32)
		{
			// 7.F.3: resolve palette index → ARGB; index 0 is always transparent.
			const SDL_Color *pal = getEffectivePalette();
			Uint32 argb = (pixel == 0) ? 0u
				: pal ? (0xFF000000u | ((Uint32)pal[pixel].r << 16) | ((Uint32)pal[pixel].g << 8) | (Uint32)pal[pixel].b)
				: 0u;
			*(Uint32*)getRaw(x, y) = argb;
			return;
		}
#else
#ifndef NDEBUG
		assert(!_surface || _surface->format->BitsPerPixel == 8);
#endif
#endif
		*getRaw(x, y) = pixel;
	}
	/**
	 * Changes the color of a pixel in the surface and returns the
	 * next pixel position. Useful when changing a lot of pixels in
	 * a row, eg. manipulating images.
	 * @param x Pointer to the X position of the pixel. Changed to the next X position in the sequence.
	 * @param y Pointer to the Y position of the pixel. Changed to the next Y position in the sequence.
	 * @param pixel New color for the pixel.
	 */
	void setPixelIterative(int *x, int *y, Uint8 pixel)
	{
		setPixel(*x, *y, pixel);
		(*x)++;
		if (*x == getWidth())
		{
			(*y)++;
			*x = 0;
		}
	}
	/**
	 * Returns the color of a specified pixel in the surface.
	 * @param x X position of the pixel.
	 * @param y Y position of the pixel.
	 * @return Color of the pixel, zero if the position is invalid.
	 */
	Uint8 getPixel(int x, int y) const
	{
		if (x < 0 || x >= getWidth() || y < 0 || y >= getHeight())
		{
			return 0;
		}
		return *getRaw(x, y);
	}
	/**
	 * Returns the pointer to a specified pixel in the surface.
	 * @param x X position of the pixel.
	 * @param y Y position of the pixel.
	 * @return Pointer to the pixel.
	 */
	const Uint8 *getRaw(int x, int y) const
	{
		return (Uint8 *)_surface->pixels + (y * _surface->pitch + x * _surface->format->BytesPerPixel);
	}
	/**
	 * Returns the pointer to a specified pixel in the surface.
	 * @param x X position of the pixel.
	 * @param y Y position of the pixel.
	 * @return Pointer to the pixel.
	 */
	Uint8 *getRaw(int x, int y)
	{
		return (Uint8 *)_surface->pixels + (y * _surface->pitch + x * _surface->format->BytesPerPixel);
	}
	/**
	 * Returns the internal SDL_Surface for SDL calls.
	 * @return Pointer to the surface.
	 */
	SDL_Surface *getSurface()
	{
		return _surface.get();
	}
	const SDL_Surface *getSurface() const
	{
		return _surface.get();
	}
	/**
	 * Returns the width of the surface.
	 * @return Width in pixels.
	 */
	int getWidth() const
	{
		return _width;
	}
	/// Sets the width of the surface.
	virtual void setWidth(int width);
	/**
	 * Returns the height of the surface.
	 * @return Height in pixels
	 */
	int getHeight() const
	{
		return _height;
	}
	/// Sets the height of the surface.
	virtual void setHeight(int height);
	/// Get surface pitch in bytes.
	int getPitch() const
	{
		return _pitch;
	}
	/// Get pointer to buffer
	Uint8* getBuffer()
	{
		return _alignedBuffer.get();
	}
	/// Get pointer to buffer
	const Uint8* getBuffer() const
	{
		return _alignedBuffer.get();
	}
	/// Sets the surface's special hidden flag.
	void setHidden(bool hidden);
	/// Locks the surface.
	void lock();
	/// Unlocks the surface.
	void unlock();
	/// Specific blit function to blit battlescape terrain data in different shades in a fast way.
	static void blitRaw(SurfaceRaw<Uint8> dest, SurfaceRaw<const Uint8> src, int x, int y, int shade, bool half = false, int newBaseColor = 0);
	/// Specific blit function to blit battlescape terrain data in different shades in a fast way.
	void blitNShade(SurfaceRaw<Uint8> surface, int x, int y, int shade = 0, bool half = false, int newBaseColor = 0) const;
	/// Specific blit function to blit battlescape terrain data in different shades in a fast way.
	void blitNShade(SurfaceRaw<Uint8> surface, int x, int y, int shade, GraphSubset range) const;
	/// 7.B: ARGB blit — uses per-asset shade table for pixel-perfect vanilla shading.
	static void blitRaw(SurfaceRaw<Uint32> dest, SurfaceRaw<const Uint32> src,
	                    int x, int y, int shade, bool half = false, int newBaseColor = 0,
	                    const ShadeTable *srcTable = nullptr, const ShadeTable *recolouredTable = nullptr);
	/// 7.B: ARGB blitNShade — shade table taken from this->getShadeTable().
	void blitNShade(SurfaceRaw<Uint32> surface, int x, int y, int shade = 0, bool half = false, int newBaseColor = 0) const;
	/// 7.B: ARGB blitNShade with explicit clip region.
	void blitNShade(SurfaceRaw<Uint32> surface, int x, int y, int shade, GraphSubset range) const;
	/// 7.E: dispatch — Surface* dest resolves ambiguity; routes to Uint32 (Emscripten) or Uint8 (native).
	void blitNShade(Surface* dest, int x, int y, int shade = 0, bool half = false, int newBaseColor = 0) const;
	/// 7.E: dispatch with clip region.
	void blitNShade(Surface* dest, int x, int y, int shade, GraphSubset range) const;
	/// 7.E: dispatch — both Surface* args; routes to Uint32 (Emscripten) or Uint8 (native).
	static void blitRaw(Surface* dest, const Surface* src, int x, int y, int shade, bool half = false, int newBaseColor = 0);
	/// Invalidate the surface: force it to be redrawn
	void invalidate(bool valid = true);

	/// Sets the color of the surface.
	virtual void setColor(Uint8 /*color*/) { /* empty by design */ };
	/// Sets the secondary color of the surface.
	virtual void setSecondaryColor(Uint8 /*color*/) { /* empty by design */ };
	/// Sets the border colour of the surface.
	virtual void setBorderColor(Uint8 /*color*/) { /* empty by design */ };
	/// Sets the high contrast color setting of the surface.
	virtual void setHighContrast(bool /*contrast*/) { /* empty by design */ };
};

/**
 * Raw pointer to surface buffer, can be created from different sources
 */
template<typename Pixel>
class SurfaceRaw
{
	Pixel* _buffer;
	Uint16 _width, _height, _pitch;

public:
	/// Default constructor
	SurfaceRaw() :
		_buffer{ nullptr },
		_width{ 0 },
		_height{ 0 },
		_pitch{ 0 }
	{

	}

	/// Copy constructor
	SurfaceRaw(const SurfaceRaw&) = default;

	/// Move constructor
	SurfaceRaw(SurfaceRaw&&) = default;

	/// Constructor
	SurfaceRaw(Pixel* buffer, int width, int height, int pitch) :
		_buffer{ buffer },
		_width{ static_cast<Uint16>(width) },
		_height{ static_cast<Uint16>(height) },
		_pitch{ static_cast<Uint16>(pitch) }
	{

	}

	/// Constructor enabled only for `Uint8` pixel type.
	template<typename T = Pixel, typename std::enable_if<std::is_same<T, Uint8>::value, int>::type* = nullptr>
	SurfaceRaw(Surface* surf) : SurfaceRaw{ }
	{
		if (surf)
		{
			*this = SurfaceRaw{ surf->getBuffer(), surf->getWidth(), surf->getHeight(), surf->getPitch() };
		}
	}

	/// Constructor enabled only for `const Uint8` pixel type.
	template<typename T = Pixel, typename std::enable_if<std::is_same<T, const Uint8>::value, int>::type* = nullptr>
	SurfaceRaw(const Surface* surf) : SurfaceRaw{ }
	{
		if (surf)
		{
			*this = SurfaceRaw{ surf->getBuffer(), surf->getWidth(), surf->getHeight(), surf->getPitch() };
		}
	}

	/// Constructor enabled only for `Uint8` pixel type.
	template<typename T = Pixel, typename std::enable_if<std::is_same<T, Uint8>::value, int>::type* = nullptr>
	SurfaceRaw(SDL_Surface* surf) : SurfaceRaw{ }
	{
		if (surf)
		{
			*this = SurfaceRaw{ (Pixel*)surf->pixels, surf->w, surf->h, surf->pitch };
		}
	}

	/// Constructor enabled only for `const Uint8` pixel type.
	template<typename T = Pixel, typename std::enable_if<std::is_same<T, const Uint8>::value, int>::type* = nullptr>
	SurfaceRaw(const SDL_Surface* surf) : SurfaceRaw{ }
	{
		if (surf)
		{
			*this = SurfaceRaw{ (Pixel*)surf->pixels, surf->w, surf->h, surf->pitch };
		}
	}

	/// 7.D: Constructor for mutable Uint32 pixel type (ARGB surfaces).
	template<typename T = Pixel, typename std::enable_if<std::is_same<T, Uint32>::value, int>::type* = nullptr>
	SurfaceRaw(Surface* surf) : SurfaceRaw{ }
	{
		if (surf)
			*this = SurfaceRaw{ (Uint32*)surf->getBuffer(), surf->getWidth(), surf->getHeight(), surf->getPitch() };
	}

	/// 7.D: Constructor for const Uint32 pixel type (ARGB surfaces, read-only).
	template<typename T = Pixel, typename std::enable_if<std::is_same<T, const Uint32>::value, int>::type* = nullptr>
	SurfaceRaw(const Surface* surf) : SurfaceRaw{ }
	{
		if (surf)
			*this = SurfaceRaw{ (const Uint32*)surf->getBuffer(), surf->getWidth(), surf->getHeight(), surf->getPitch() };
	}

	/// Constructor, SFINAE enable it only for non const `PixelType`
	template<typename = std::enable_if<std::is_const<Pixel>::value == false, void>>
	SurfaceRaw(std::vector<Pixel>& vec, int width, int height) : SurfaceRaw{ vec.data(), width, height, static_cast<Uint16>(width*sizeof(Pixel)) }
	{
		assert((size_t)(width*height) <= vec.size() && "Incorrect dimensions compared to vector size");
	}

	/// Constructor, SFINAE enable it only for `const PixelType`
	template<typename = std::enable_if<std::is_const<Pixel>::value, void>>
	SurfaceRaw(const std::vector<typename std::remove_const<Pixel>::type>& vec, int width, int height) : SurfaceRaw{ vec.data(), width, height, static_cast<Uint16>(width*sizeof(Pixel)) }
	{
		assert((size_t)(width*height) <= vec.size() && "Incorrect dimensions compared to vector size");
	}

	/// Constructor
	template<int I>
	SurfaceRaw(Pixel (&buffer)[I], int width, int height) : SurfaceRaw{ buffer, width, height, static_cast<Uint16>(width*sizeof(Pixel)) }
	{
		assert(width*height <= I && "Incorrect dimensions compared to array size");
	}

	/// Assignment from nullptr
	SurfaceRaw& operator=(std::nullptr_t)
	{
		*this = SurfaceRaw{};
		return *this;
	}

	/// Assignment
	SurfaceRaw& operator=(const SurfaceRaw&) = default;

	/// Is empty?
	explicit operator bool() const
	{
		return _buffer;
	}

	/// Returns the width of the surface.
	int getWidth() const
	{
		return _width;
	}

	/// Returns the height of the surface.
	int getHeight() const
	{
		return _height;
	}

	/// Get surface pitch in bytes.
	int getPitch() const
	{
		return _pitch;
	}

	/// Get pointer to buffer
	Pixel* getBuffer() const
	{
		return _buffer;
	}
};

/**
 * Helper class used to blit part of surface to another one.
 */
class SurfaceCrop
{
	const Surface* _surface;
	SDL_Rect _crop;
	int _x, _y;

public:
	/// Default constructor
	SurfaceCrop() : _surface{ nullptr }, _crop{ }, _x{ }, _y{ }
	{

	}

	/// Constructor
	SurfaceCrop(const Surface* surf) : _surface{ surf }, _crop{ }, _x{ surf->getX() }, _y{ surf->getY() }
	{

	}

	/// Get crop rectangle.
	SDL_Rect* getCrop()
	{
		return &_crop;
	}

	/// Get Surface.
	const Surface* getSurface()
	{
		return _surface;
	}

	/// Sets the X position of the surface.
	void setX(int x)
	{
		_x = x;
	}

	/// Returns the position of the surface in the X axis.
	int getX() const
	{
		return _x;
	}

	/// Sets the Y position of the surface.
	void setY(int y)
	{
		_y = y;
	}

	/// Returns the position of the surface in the Y axis.
	int getY() const
	{
		return _y;
	}

	/// Blit Cropped surface to another surface.
	void blit(Surface* dest);
};

/**
 * Phase 7.B: HD-path shade attenuation for ARGB pixels without a shade table.
 * Applies a perceptually calibrated linear darkening curve.
 * shade 0 = full brightness, shade 15 = black. Alpha is preserved.
 * Index 0 (transparent, alpha==0) is passed through unchanged.
 *
 * This is a free function so ShaderDraw helpers can call it without including
 * the full Surface class.  Cross-platform; no __EMSCRIPTEN__ guard (D4b).
 */
inline Uint32 shadeARGBCurve(Uint32 src, int shade)
{
	if ((src >> 24) == 0) return 0;
	static const float k[16] = {
		1.00f, 0.93f, 0.87f, 0.80f, 0.74f, 0.67f, 0.60f, 0.53f,
		0.47f, 0.40f, 0.33f, 0.27f, 0.20f, 0.13f, 0.07f, 0.00f
	};
	const float m = k[shade & 0x0f];
	const Uint8 a = (Uint8)((src >> 24) & 0xff);
	const Uint8 r = (Uint8)(((src >> 16) & 0xff) * m);
	const Uint8 g = (Uint8)(((src >>  8) & 0xff) * m);
	const Uint8 b = (Uint8)( (src        & 0xff) * m);
	return ((Uint32)a << 24) | ((Uint32)r << 16) | ((Uint32)g << 8) | b;
}

}
