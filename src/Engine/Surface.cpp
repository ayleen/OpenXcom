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
#include "Surface.h"
#include "ShaderDraw.h"
#include "ShaderMove.h"
#include <vector>
#include <algorithm>
#include <SDL_gfxPrimitives.h>
#include <SDL_image.h>
#include "../lodepng.h"
#include "Palette.h"
#include "Exception.h"
#include "Logger.h"
#include "SDL2Helpers.h"
#include "FileMap.h"
#ifdef __EMSCRIPTEN__
#include "HDQueue.h"
#endif
#ifdef _WIN32
#include <malloc.h>
#endif
#if defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
#define _aligned_malloc __mingw_aligned_malloc
#define _aligned_free   __mingw_aligned_free
#endif //MINGW
#ifdef __MORPHOS__
#include <ppcinline/exec.h>
#endif

namespace OpenXcom
{


namespace
{

/**
 * Helper function counting pitch in bytes with 16byte padding
 * @param bpp bits per pixel
 * @param width number of pixel in row
 * @return pitch in bytes
 */
inline int GetPitch(int bpp, int width)
{
	return ((bpp/8) * width + 15) & ~0xF;
}


/**
 * Raw copy without any change of pixel index value between two SDL surface, palette is ignored
 * @param dest Destination surface
 * @param src Source surface
 */
inline void RawCopySurf(const Surface::UniqueSurfacePtr& dest, const Surface::UniqueSurfacePtr& src)
{
	ShaderDrawFunc(
		[](Uint8& destStuff, Uint8& srcStuff)
		{
			destStuff = srcStuff;
		},
		ShaderMove<Uint8>(dest.get()),
		ShaderMove<Uint8>(src.get())
	);
}

/**
 * TODO: function for purge, we should accept only "standard" surfaces
 * Helper function correcting graphic that should have index 0 as transparent,
 * but some do not have, we swap correct with incorrect
 * for maintain 0 as correct transparent index.
 * @param dest Surface to fix
 * @param currentTransColor current transparent color index
 */
inline void FixTransparent(const Surface::UniqueSurfacePtr& dest, int currentTransColor)
{
	if (currentTransColor != 0)
	{
		ShaderDrawFunc(
			[&](Uint8& destStuff)
			{
				if (destStuff == currentTransColor)
				{
					destStuff = 0;
				}
			},
			ShaderMove<Uint8>(dest.get())
		);
	}
}

} //namespace

/**
 * Helper function creating aligned buffer
 * @param bpp bits per pixel
 * @param width number of pixel in row
 * @param height number of rows
 * @return pointer to memory
 */
Surface::UniqueBufferPtr Surface::NewAlignedBuffer(int bpp, int width, int height)
{
	const int pitch = GetPitch(bpp, width);
	const int total = pitch * height;
	void* buffer = 0;

#ifndef _WIN32

	#ifdef __MORPHOS__

	buffer = calloc( total, 1 );
	if (!buffer)
	{
		throw Exception("Failed to allocate surface");
	}

	#else
	int rc;
	if ((rc = posix_memalign(&buffer, 16, total)))
	{
		throw Exception(strerror(rc));
	}
	#endif

#else

	// of course Windows has to be difficult about this!
	buffer = _aligned_malloc(total, 16);
	if (!buffer)
	{
		throw Exception("Failed to allocate surface");
	}

#endif

	memset(buffer, 0, total);
	return Surface::UniqueBufferPtr((Uint8*)buffer);
}

/**
 * Helper function creating new unique pointer
 * @param surface
 * @return Unique pointer
 */
Surface::UniqueSurfacePtr Surface::NewSdlSurface(SDL_Surface* surface)
{
	return Surface::UniqueSurfacePtr(surface);
}

/**
 * Helper function creating new SDL surface in unique pointer
 * @param buffer memory buffer
 * @param bpp bit depth
 * @param width width of surface
 * @param height height of surface
 * @return Unique pointer
 */
Surface::UniqueSurfacePtr Surface::NewSdlSurface(const Surface::UniqueBufferPtr& buffer, int bpp, int width, int height)
{
	auto surface = SDL_CreateRGBSurfaceFrom(buffer.get(), width, height, bpp, GetPitch(bpp, width), 0, 0, 0, 0);
	if (!surface)
	{
		throw Exception(SDL_GetError());
	}

#ifdef __EMSCRIPTEN__
	// SDL_CreateRGBSurfaceFrom under Emscripten SDL1 JS mode ignores the pixel
	// data for non-32bpp surfaces and sets pitch=width*4, BytesPerPixel=4.
	// Patch the C struct directly so that all C-side pixel access is correct.
	// JS SDL functions use SDL.surfaces[surf].image (canvas), not surface->pixels.
	surface->pixels = buffer.get();
	surface->pitch  = GetPitch(bpp, width);
	if (surface->format)
	{
		surface->format->BitsPerPixel  = (Uint8)bpp;
		surface->format->BytesPerPixel = (Uint8)((bpp + 7) / 8);
	}
#endif

	return NewSdlSurface(surface);
}

/**
 * Zero whole surface.
 */
void Surface::CleanSdlSurface(SDL_Surface* surface)
{
#ifdef __EMSCRIPTEN__
	if (surface->pixels)
		memset(surface->pixels, 0, (size_t)surface->h * surface->pitch);
#else
	if (surface->flags & SDL_SWSURFACE)
	{
		memset(surface->pixels, 0, surface->h * surface->pitch);
	}
	else
	{
		SDL_Rect c;
		c.x = 0;
		c.y = 0;
		c.w = surface->w;
		c.h = surface->h;
		SDL_FillRect(surface, &c, 0);
	}
#endif
}
/**
 * Default deleter for alignment buffer
 * @param buffer
 */
void Surface::UniqueBufferDeleter::operator ()(Uint8* buffer)
{
	if (buffer)
	{
#ifdef _WIN32
		_aligned_free(buffer);
#else
		free(buffer);
#endif
	}
}

/**
 * Default deleter for SDL surface
 * @param surf
 */
void Surface::UniqueSurfaceDeleter::operator ()(SDL_Surface* surf)
{
	SDL_FreeSurface(surf);
}



/**
 * Default empty surface.
 */
Surface::Surface() : _x{ }, _y{ }, _width{ }, _height{ }, _pitch{ }, _visible(true), _hidden(false), _redraw(false)
{

}

/**
 * Sets up a blank 8bpp surface with the specified size and position,
 * with pure black as the transparent color.
 * @note Surfaces don't have to fill the whole size since their
 * background is transparent, specially subclasses with their own
 * drawing logic, so it just covers the maximum drawing area.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 * @param bpp Bits-per-pixel depth.
 */
Surface::Surface(int width, int height, int x, int y) : _x(x), _y(y), _visible(true), _hidden(false), _redraw(false)
{
	std::tie(_alignedBuffer, _surface) = Surface::NewPair8Bit(width, height);
	_width = _surface->w;
	_height = _surface->h;
	_pitch = _surface->pitch;
	SDL_SetColorKey(_surface.get(), SDL_SRCCOLORKEY, 0);
}

#ifdef __EMSCRIPTEN__
/**
 * Creates a new surface with an explicit pixel format.
 * Format::ARGB8888 allocates a 32bpp buffer suitable for HD UI containers.
 */
Surface::Surface(int width, int height, int x, int y, Format fmt)
    : _x(x), _y(y), _visible(true), _hidden(false), _redraw(false)
{
	if (fmt == Format::ARGB8888)
	{
		std::tie(_alignedBuffer, _surface) = Surface::NewPair32Bit(width, height);
		SDL_SetSurfaceBlendMode(_surface.get(), SDL_BLENDMODE_BLEND);
	}
	else
	{
		std::tie(_alignedBuffer, _surface) = Surface::NewPair8Bit(width, height);
		SDL_SetColorKey(_surface.get(), SDL_SRCCOLORKEY, 0);
	}
	_width  = (Uint16)_surface->w;
	_height = (Uint16)_surface->h;
	_pitch  = (Uint16)_surface->pitch;
}
#endif

/**
 * Performs a deep copy of an existing surface.
 * @param other Surface to copy from.
 */
Surface::Surface(const Surface& other) : Surface{ }
{
#ifdef __EMSCRIPTEN__
	// HD surfaces (loadImageHD) and promoted-ARGB surfaces (promoteToARGB) both
	// carry a 32bpp _surface. Handle before the 8bpp guard below, which would
	// otherwise short-circuit on empty _alignedBuffer or call getPalette() on a
	// surface with no palette object.
	if (other._surface && other._surface->format->BitsPerPixel == 32)
	{
		// SDL_ConvertSurfaceFormat creates a properly-formatted ARGB8888 copy
		// (with correct R/G/B/A masks). NewPair32Bit cannot be used here:
		// its underlying SDL_CreateRGBSurfaceFrom call passes all-zero masks
		// (an Emscripten-port quirk), and SDL_BlitSurface to such a surface
		// silently loses channel data. SDL owns the pixel buffer for the
		// converted surface, so _alignedBuffer stays null (matches the post-
		// loadImageHD layout exactly).
		auto copy = NewSdlSurface(
			SDL_ConvertSurfaceFormat(other._surface.get(), SDL_PIXELFORMAT_ARGB8888, 0));
		if (!copy) return;
		SDL_SetSurfaceBlendMode(copy.get(), SDL_BLENDMODE_BLEND);
		_alignedBuffer = nullptr;
		_surface = std::move(copy);
		_width   = (Uint16)_surface->w;
		_height  = (Uint16)_surface->h;
		_pitch   = (Uint16)_surface->pitch;
		_x       = other._x;
		_y       = other._y;
		_visible = other._visible;
		_hidden  = other._hidden;
		_redraw  = other._redraw;
		_isHD    = other._isHD;   // preserve HD-queue routing flag
		_logicalW = other._logicalW;
		_logicalH = other._logicalH;
		return;
	}
#endif

	if (!other)
	{
		return;
	}
	int width = other.getWidth();
	int height = other.getHeight();

	//move copy
	*this = Surface(width, height, other._x, other._y);
	//cant call `setPalette` because its virtual function and it doesn't work correctly in constructor
	SDL_SetColors(_surface.get(), other.getPalette(), 0, 255);
	RawCopySurf(_surface, other._surface);

	_x = other._x;
	_y = other._y;
	_visible = other._visible;
	_hidden = other._hidden;
	_redraw = other._redraw;
}

/**
 * Deletes the surface from memory.
 */
Surface::~Surface()
{

}

/**
 * Performs a fast copy of a pixel array, accounting for pitch.
 * @param src Source array.
 */
template <typename T>
void Surface::rawCopy(const std::vector<T> &src)
{
	// Copy whole thing
	if (_surface->pitch == _surface->w)
	{
		size_t end = std::min(size_t(_surface->w * _surface->h * _surface->format->BytesPerPixel), src.size());
		std::copy(src.begin(), src.begin() + end, (T*)_surface->pixels);
	}
	// Copy row by row
	else
	{
		for (int y = 0; y < _surface->h; ++y)
		{
			size_t begin = y * _surface->w;
			size_t end = std::min(begin + _surface->w, src.size());
			if (begin >= src.size())
				break;
			std::copy(src.begin() + begin, src.begin() + end, (T*)getRaw(0, y));
		}
	}
}

/**
 * Loads a raw array of pixels into the surface. The pixels must be
 * in the same BPP as the surface.
 * @param bytes Pixel array.
 */
void Surface::loadRaw(const std::vector<unsigned char> &bytes)
{
#ifdef __EMSCRIPTEN__
	_isHD = false; _logicalW = _logicalH = 0;
#endif
	lock();
	rawCopy(bytes);
	unlock();
}

/**
 * Loads a raw array of pixels into the surface. The pixels must be
 * in the same BPP as the surface.
 * @param bytes Pixel array.
 */
void Surface::loadRaw(const std::vector<char> &bytes)
{
#ifdef __EMSCRIPTEN__
	_isHD = false; _logicalW = _logicalH = 0;
#endif
	lock();
	rawCopy(bytes);
	unlock();
}

/**
 * Loads the contents of an X-Com SCR image file into
 * the surface. SCR files are simply uncompressed images
 * containing the palette offset of each pixel.
 * @param filename Filename of the SCR image.
 * @sa http://www.ufopaedia.org/index.php?title=Image_Formats#SCR_.26_DAT
 */
void Surface::loadScr(const std::string& filename)
{
	// Load file and put pixels in surface
	auto istream = FileMap::getIStream(filename);
	std::vector<char> buffer((std::istreambuf_iterator<char>(*(istream))), (std::istreambuf_iterator<char>()));
	loadRaw(buffer);
}
#ifdef __EMSCRIPTEN__
/* ---- IFF/LBM decoder (Emscripten only) ----------------------------------------
 *
 * SDL_image on Emscripten SDL1 uses JS IMG_Load_RW which only supports formats
 * covered by STB_IMAGE (JPEG, PNG, BMP, PSD, GIF, HDR, PIC, PNM) — not LBM.
 * TFTD uses Deluxe Paint LBM files both as images and for their 256-color palette.
 * We implement a minimal but complete IFF ILBM/PBM decoder here.
 *
 * Supported:
 *   - FORM/ILBM and FORM/PBM  (Deluxe Paint DOS)
 *   - 8bpp images (nPlanes == 8)
 *   - compression 0 (none) and 1 (ByteRun1 / PackBits)
 *   - CMAP palette chunk (up to 256 RGB entries)
 * ---------------------------------------------------------------------------------*/
namespace {

/* PackBits / ByteRun1 decompressor. Decompresses exactly dstLen bytes into dst,
 * advancing *src. Returns false on input underrun or output overrun. */
static bool lbmUnpackRow(const Uint8 **src, const Uint8 *srcEnd, Uint8 *dst, int dstLen)
{
    int written = 0;
    while (written < dstLen) {
        if (*src >= srcEnd) return false;
        Sint8 n = (Sint8)(*(*src)++);
        if (n == -128) continue; // NOP
        if (n >= 0) {
            int count = (int)n + 1;
            if (*src + count > srcEnd || written + count > dstLen) return false;
            memcpy(dst + written, *src, count);
            *src += count; written += count;
        } else {
            int count = -(int)n + 1;
            if (*src >= srcEnd || written + count > dstLen) return false;
            memset(dst + written, (int)*(*src)++, count);
            written += count;
        }
    }
    return true;
}

/* Decode raw IFF/LBM bytes into a Surface. Returns true on success.
 * Handles PBM (chunky 8bpp) and ILBM (bitplanes, converted to 8bpp). */
static bool loadLbmInto(Surface &out, const Uint8 *data, size_t size)
{
    if (size < 12) return false;
    if (memcmp(data, "FORM", 4) != 0) return false;
    bool isPBM  = (memcmp(data + 8, "PBM ", 4) == 0);
    bool isILBM = (memcmp(data + 8, "ILBM", 4) == 0);
    if (!isPBM && !isILBM) return false;

    int w = 0, h = 0, nPlanes = 0, compression = 0;
    SDL_Color palette[256] = {};
    int palCount = 0;
    const Uint8 *body = nullptr;
    Uint32 bodyLen = 0;

    // Scan IFF chunks
    const Uint8 *p = data + 12, *end = data + size;
    while (p + 8 <= end) {
        Uint32 sz = ((Uint32)p[4]<<24)|((Uint32)p[5]<<16)|((Uint32)p[6]<<8)|p[7];
        const Uint8 *cd = p + 8;
        if (cd + sz > end) sz = (Uint32)(end - cd);

        if (memcmp(p, "BMHD", 4) == 0 && sz >= 20) {
            w           = (int)((cd[0]<<8)|cd[1]);
            h           = (int)((cd[2]<<8)|cd[3]);
            nPlanes     = (int)cd[8];
            compression = (int)cd[10];
        } else if (memcmp(p, "CMAP", 4) == 0) {
            palCount = (int)(sz / 3);
            if (palCount > 256) palCount = 256;
            for (int i = 0; i < palCount; i++) {
                palette[i] = { cd[i*3], cd[i*3+1], cd[i*3+2], 255 };
            }
        } else if (memcmp(p, "BODY", 4) == 0 && !body) {
            body = cd; bodyLen = sz;
        }
        p += 8 + sz + (sz & 1); // IFF chunks are word-padded
    }

    if (!body || w <= 0 || h <= 0 || nPlanes < 1 || nPlanes > 8) return false;

    out = Surface(w, h, 0, 0);
    if (palCount > 0) out.setPalette(palette, 0, palCount);

    SDL_Surface *surf = out.getSurface();
    if (!surf || !surf->pixels) return false;

    const Uint8 *src = body, *srcEnd = body + bodyLen;

    if (isPBM) {
        /* Chunky 8bpp: one byte per pixel per row.
         * Rows are word-padded when ByteRun1 compressed (required by IFF spec),
         * stored consecutively when uncompressed. */
        int rowBuf = (w + 1) & ~1;
        std::vector<Uint8> row((size_t)rowBuf);
        for (int y = 0; y < h; y++) {
            Uint8 *dst = (Uint8*)surf->pixels + y * surf->pitch;
            if (compression == 1) {
                if (!lbmUnpackRow(&src, srcEnd, row.data(), rowBuf)) break;
                memcpy(dst, row.data(), w);
            } else {
                if (src + w > srcEnd) break;
                memcpy(dst, src, w);
                src += rowBuf; // skip possible pad byte
            }
        }
    } else {
        /* ILBM bitplanes → 8bpp chunky.
         * Each scan line has nPlanes plane rows, each word-aligned. */
        int planeRow = ((w + 15) / 16) * 2;
        std::vector<Uint8> planes((size_t)(nPlanes * planeRow));
        bool ok = true;
        for (int y = 0; y < h && ok; y++) {
            Uint8 *dst = (Uint8*)surf->pixels + y * surf->pitch;
            for (int pn = 0; pn < nPlanes; pn++) {
                Uint8 *pbuf = planes.data() + pn * planeRow;
                if (compression == 1) {
                    if (!lbmUnpackRow(&src, srcEnd, pbuf, planeRow)) { ok = false; break; }
                } else {
                    if (src + planeRow > srcEnd) { ok = false; break; }
                    memcpy(pbuf, src, planeRow); src += planeRow;
                }
            }
            if (!ok) break;
            for (int x = 0; x < w; x++) {
                Uint8 pix = 0;
                for (int pn = 0; pn < nPlanes; pn++) {
                    if (planes[pn * planeRow + x/8] & (0x80u >> (x & 7)))
                        pix |= (Uint8)(1u << pn);
                }
                dst[x] = pix;
            }
        }
    }
    return true;
}

} // anonymous namespace
#endif // __EMSCRIPTEN__

/**
 * Loads the contents of an image file of a
 * known format into the surface.
 * @param filename Filename of the image.
 */
void Surface::loadImage(const std::string &filename)
{
#ifdef __EMSCRIPTEN__
	_isHD = false; _logicalW = _logicalH = 0;
#endif
	// Destroy current surface (will be replaced)
	_alignedBuffer = nullptr;
	_surface = nullptr;

	Log(LOG_VERBOSE) << "Loading image: " << filename;
	auto rw = FileMap::getRWops(filename);
	if (!rw) { return; } // relevant message gets logged in FileMap.

	// Try loading with LodePNG first
	if (CrossPlatform::compareExt(filename, "png"))
	{
		size_t size;
		void *data = SDL_LoadFile_RW(rw, &size, SDL_FALSE);
		if ((data != NULL) && (size > 8 + 12 + 12)) // minimal PNG file size: header and two empty chunks
		{
			std::vector<unsigned char> png;
			png.resize(size);
			memcpy(&png[0], data, size);

			std::vector<unsigned char> image;
			unsigned width, height;
			lodepng::State state;
			state.decoder.color_convert = 0;
			unsigned error = lodepng::decode(image, width, height, state, png);
			if (!error)
			{
				LodePNGColorMode *color = &state.info_png.color;
				unsigned bpp = lodepng_get_bpp(color);
				if (bpp == 8)
				{
					*this = Surface(width, height, 0, 0);
					setPalette((SDL_Color*)color->palette, 0, color->palettesize);

					ShaderDrawFunc(
						[](Uint8& dest, unsigned char& src)
						{
							dest = src;
						},
						ShaderSurface(this),
						ShaderSurface(SurfaceRaw<unsigned char>(image, width, height))
					);
					int transparent = 0;
					for (int c = 0; c < _surface->format->palette->ncolors; ++c)
					{
						SDL_Color *palColor = _surface->format->palette->colors + c;
						if (palColor->a == 0)
						{
							transparent = c;
							break;
						}
					}
					FixTransparent(_surface, transparent);
					if (transparent != 0)
					{
						Log(LOG_WARNING) << "Image " << filename << " (from lodepng) has incorrect transparent color index " << transparent << " (instead of 0).";
					}
				}
			} else {
				Log(LOG_ERROR) << "Image " << filename << " lodepng failed:" << lodepng_error_text(error);
			}
		}
		if (data) { SDL_free(data); }
	}
	if (_surface)
	{
		SDL_RWclose(rw);
	}
#ifdef __EMSCRIPTEN__
	else if (CrossPlatform::compareExt(filename, "lbm"))
	{
		// SDL_image on Emscripten SDL1 cannot load LBM (not supported by STB_IMAGE).
		// Use our native IFF/LBM decoder instead.
		SDL_RWseek(rw, 0, RW_SEEK_SET);
		size_t lbmSize;
		void *lbmData = SDL_LoadFile_RW(rw, &lbmSize, SDL_TRUE); // SDL_TRUE = freesrc
		if (lbmData) {
			if (!loadLbmInto(*this, (const Uint8*)lbmData, lbmSize)) {
				Log(LOG_ERROR) << "Image " << filename << ": LBM decode failed";
			}
			SDL_free(lbmData);
		} else {
			Log(LOG_ERROR) << "Image " << filename << ": could not read file";
		}
	}
#endif
	else // Otherwise default to SDL_Image
	{
		SDL_RWseek(rw, RW_SEEK_SET, 0); // rewind in case .png was no PNG at all
		auto surface = NewSdlSurface(IMG_Load_RW(rw, SDL_TRUE));
		if (!surface)
		{
			std::string err = filename + ":" + IMG_GetError();
			throw Exception(err);
		}
		if (surface->format->BitsPerPixel != 8)
		{
			std::string err = filename + ": OpenXcom supports only 8bit images.";
			throw Exception(err);
		}

		*this = Surface(surface->w, surface->h, 0, 0);
		setPalette(surface->format->palette->colors, 0, surface->format->palette->ncolors);
		RawCopySurf(_surface, surface);
		Uint32 colorkey = 0;
#ifndef __EMSCRIPTEN__
		colorkey = surface->format->colorkey; // SDL1 struct field; no colorkey in Emscripten SDL1 emulation
#endif
		FixTransparent(_surface, colorkey);
		if (colorkey != 0)
		{
			Log(LOG_WARNING) << "Image " << filename << " (from SDL) has incorrect transparent color index " << colorkey << " (instead of 0).";
		}
	}
}

/**
 * Loads a 32-bit RGBA/ARGB PNG (or any SDL_image-supported format) without
 * palette quantization. Used by the HD asset path (ExtraSprites with hd: true).
 * The surface is stored as SDL_PIXELFORMAT_ARGB8888 with blend mode BLEND so
 * it composes correctly over the 8-bpp framebuffer during SDL_BlitSurface calls.
 * @param filename Filename of the image (resolved via FileMap).
 */
void Surface::loadImageHD(const std::string &filename)
{
	_alignedBuffer = nullptr;
	_surface = nullptr;

	Log(LOG_VERBOSE) << "Loading HD image: " << filename;
	auto rw = FileMap::getRWops(filename);
	if (!rw) { return; }

	// IMG_Load_RW with SDL_TRUE takes ownership of rw and closes it on return.
	auto raw = NewSdlSurface(IMG_Load_RW(rw, SDL_TRUE));
	if (!raw)
	{
		throw Exception(filename + ": " + IMG_GetError());
	}

	auto converted = NewSdlSurface(SDL_ConvertSurfaceFormat(raw.get(), SDL_PIXELFORMAT_ARGB8888, 0));
	if (!converted)
	{
		throw Exception(filename + ": SDL_ConvertSurfaceFormat: " + SDL_GetError());
	}

	SDL_SetSurfaceBlendMode(converted.get(), SDL_BLENDMODE_BLEND);

	_width  = (Uint16)converted->w;
	_height = (Uint16)converted->h;
	_pitch  = (Uint16)converted->pitch;
	_x = 0;
	_y = 0;
	_surface = std::move(converted);
#ifdef __EMSCRIPTEN__
	_isHD = true;
	// Intentionally leave _logicalW/_logicalH at 0 — the caller MUST follow
	// loadImageHD() with setLogicalSize(). The blit() warning fires when this
	// contract is violated; auto-defaulting here would silence it.
	_logicalW = 0;
	_logicalH = 0;
#endif
}

/**
 * Loads the contents of an X-Com SPK image file into
 * the surface. SPK files are compressed with a custom
 * algorithm since they're usually full-screen images.
 * @param filename Filename of the SPK image.
 * @sa http://www.ufopaedia.org/index.php?title=Image_Formats#SPK
 */
void Surface::loadSpk(const std::string& filename)
{
#ifdef __EMSCRIPTEN__
	_isHD = false; _logicalW = _logicalH = 0;
#endif
	Uint16 flag;
	int x = 0, y = 0;
	auto rw = FileMap::getRWopsReadAll(filename);
	if (!rw) { return; }
	auto rwsize = SDL_RWsize(rw);
	// Lock the surface
	lock();
	while(SDL_RWtell(rw) < rwsize - 1) {
		flag = SDL_ReadLE16(rw);
		if (flag == 65535) {
			flag = SDL_ReadLE16(rw);
			for (int i = 0; i < flag * 2; ++i) { setPixelIterative(&x, &y, 0); }
		} else if (flag == 65534) {
			flag = SDL_ReadLE16(rw);
			for (int i = 0; i < flag * 2; ++i) { setPixelIterative(&x, &y, SDL_ReadU8(rw)); }
		}
	}
	// Unlock the surface
	unlock();
	SDL_RWclose(rw);
}

/**
 * Loads the contents of a TFTD BDY image file into
 * the surface. BDY files are compressed with a custom
 * algorithm.
 * @param filename Filename of the BDY image.
 * @sa http://www.ufopaedia.org/index.php?title=Image_Formats#BDY
 */
void Surface::loadBdy(const std::string &filename)
{
#ifdef __EMSCRIPTEN__
	_isHD = false; _logicalW = _logicalH = 0;
#endif
	Uint8 dataByte;
	int pixelCnt;
	int x = 0, y = 0;
	int currentRow = 0;
	auto rw = FileMap::getRWopsReadAll(filename);
	if (!rw) { return; }
	auto rwsize = SDL_RWsize(rw);
	// Lock the surface
	lock();
	while (SDL_RWtell(rw) < rwsize) {
		dataByte = SDL_ReadU8(rw);
		if (dataByte >= 129)
		{
			pixelCnt = 257 - (int)dataByte;
			dataByte = SDL_ReadU8(rw);
			currentRow = y;
			for (int i = 0; i < pixelCnt; ++i)
			{
				setPixelIterative(&x, &y, dataByte);
				if (currentRow != y) // avoid overscan into next row
					break;
			}
		}
		else
		{
			pixelCnt = 1 + (int)dataByte;
			currentRow = y;
			for (int i = 0; i < pixelCnt; ++i)
			{
				dataByte = SDL_ReadU8(rw);
				if (currentRow == y) // avoid overscan into next row
					setPixelIterative(&x, &y, dataByte);
			}
		}
	}
	// Unlock the surface
	unlock();
	SDL_RWclose(rw);
}

/**
 * Clears the entire contents of the surface, resulting
 * in a blank image of the specified color. (0 for transparent)
 * @param color the colour for the background of the surface.
 */
void Surface::clear()
{
	CleanSdlSurface(_surface.get());
}

/**
 * Shifts all the colors in the surface by a set amount.
 * This is a common method in 8bpp games to simulate color
 * effects for cheap.
 * @param off Amount to shift.
 * @param min Minimum color to shift to.
 * @param max Maximum color to shift to.
 * @param mul Shift multiplier.
 */
void Surface::offset(int off, int min, int max, int mul)
{
	if (off == 0)
		return;

	// Lock the surface
	lock();

	for (int x = 0, y = 0; x < getWidth() && y < getHeight();)
	{
		Uint8 pixel = getPixel(x, y);
		int p;
		if (off > 0)
		{
			p = pixel * mul + off;
		}
		else
		{
			p = (pixel + off) / mul;
		}
		if (min != -1 && p < min)
		{
			p = min;
		}
		else if (max != -1 && p > max)
		{
			p = max;
		}

		if (pixel > 0)
		{
			setPixelIterative(&x, &y, p);
		}
		else
		{
			setPixelIterative(&x, &y, 0);
		}
	}

	// Unlock the surface
	unlock();
}

/**
 * Shifts all the colors in the surface by a set amount, but
 * keeping them inside a fixed-size color block chunk.
 * @param off Amount to shift.
 * @param blk Color block size.
 * @param mul Shift multiplier.
 */
void Surface::offsetBlock(int off, int blk, int mul)
{
	if (off == 0)
		return;

	// Lock the surface
	lock();

	for (int x = 0, y = 0; x < getWidth() && y < getHeight();)
	{
		Uint8 pixel = getPixel(x, y);
		int min = pixel / blk * blk;
		int max = min + blk;
		int p;
		if (off > 0)
		{
			p = pixel * mul + off;
		}
		else
		{
			p = (pixel + off) / mul;
		}
		if (min != -1 && p < min)
		{
			p = min;
		}
		else if (max != -1 && p > max)
		{
			p = max;
		}

		if (pixel > 0)
		{
			setPixelIterative(&x, &y, p);
		}
		else
		{
			setPixelIterative(&x, &y, 0);
		}
	}

	// Unlock the surface
	unlock();
}

/**
 * Inverts all the colors in the surface according to a middle point.
 * Used for effects like shifting a button between pressed and unpressed.
 * @param mid Middle point.
 */
void Surface::invert(Uint8 mid)
{
	// Lock the surface
	lock();

	for (int x = 0, y = 0; x < getWidth() && y < getHeight();)
	{
		Uint8 pixel = getPixel(x, y);
		if (pixel > 0)
		{
			setPixelIterative(&x, &y, pixel + 2 * ((int)mid - (int)pixel));
		}
		else
		{
			setPixelIterative(&x, &y, 0);
		}
	}

	// Unlock the surface
	unlock();
}

/**
 * Runs any code the surface needs to keep updating every
 * game cycle, like animations and other real-time elements.
 */
void Surface::think()
{

}

/**
 * Draws the graphic that the surface contains before it
 * gets blitted onto other surfaces. The surface is only
 * redrawn if the flag is set by a property change, to
 * avoid unnecessary drawing.
 */
void Surface::draw()
{
	_redraw = false;
	clear();
}

/**
 * Blits this surface onto another one, with its position
 * relative to the top-left corner of the target surface.
 * The cropping rectangle controls the portion of the surface
 * that is blitted.
 * @param surface Pointer to surface to blit onto.
 */
void Surface::blit(SDL_Surface *surface)
{
	if (_visible && !_hidden)
	{
		if (_redraw)
			draw();

		SDL_Rect target {};
		target.x = getX();
		target.y = getY();

#ifdef __EMSCRIPTEN__
		// Four-way blit table (6a.2):
		//   ARGB src → 8bpp dst  : HDQueue (lands on final _screen via flush)
		//   ARGB src → ARGB dst  : direct SDL_BlitSurface (alpha blend)
		//   8bpp src → 8bpp dst  : SDL_BlitSurface (palette index copy)
		//   8bpp src → ARGB dst  : SDL_BlitSurface (SDL2 cross-format)
		if (_surface->format->BitsPerPixel == 32)
		{
			target.w = _logicalW > 0 ? _logicalW : _width;
			target.h = _logicalH > 0 ? _logicalH : _height;
			if (_isHD && _logicalW == 0)
			{
				static thread_local bool warned = false;
				if (!warned)
				{
					Log(LOG_WARNING) << "HD surface hit blit() with logicalW=0 — setLogicalSize not called";
					warned = true;
				}
			}
			if (surface->format->BitsPerPixel == 32)
			{
				// ARGB → ARGB: alpha-blend directly into the ARGB parent.
				SDL_BlitSurface(_surface.get(), nullptr, surface, &target);
			}
			else
			{
				// ARGB → 8bpp: queue for final _screen composite.
				HDQueue::push(_surface.get(), target);
			}
			return;
		}
#endif

		SDL_BlitSurface(_surface.get(), nullptr, surface, &target);
	}
}

/**
 * Copies the exact contents of another surface onto this one.
 * Only the content that would overlap both surfaces is copied, in
 * accordance with their positions. This is handy for applying
 * effects over another surface without modifying the original.
 * @param surface Pointer to surface to copy from.
 */
void Surface::copy(Surface *surface)
{
	/*
	SDL_BlitSurface uses colour matching,
	and is therefor unreliable as a means
	to copy the contents of one surface to another
	instead we have to do this manually

	SDL_Rect from;
	from.x = getX() - surface->getX();
	from.y = getY() - surface->getY();
	from.w = getWidth();
	from.h = getHeight();
	SDL_BlitSurface(surface->getSurface(), &from, _surface, 0);
	*/
	const int from_x = getX() - surface->getX();
	const int from_y = getY() - surface->getY();

	lock();

	ShaderDrawFunc(
		[](Uint8& dest, const Uint8& src)
		{
			dest = src;
		},
		ShaderMove<Uint8>(_surface.get(), from_x, from_y),
		ShaderMove<Uint8>(surface, 0, 0)
	);

	unlock();
}

/**
 * Draws a filled rectangle on the surface.
 * @param rect Pointer to Rect.
 * @param color Color of the rectangle.
 */
void Surface::drawRect(SDL_Rect *rect, Uint8 color)
{
	if (rect->w == 0 || rect->h == 0) return;

#ifdef __EMSCRIPTEN__
	SDL_Surface *s = _surface.get();
	if (s && s->pixels)
	{
		int x = rect->x, y = rect->y, w = rect->w, h = rect->h;
		if (x < 0) { w += x; x = 0; }
		if (y < 0) { h += y; y = 0; }
		if (x + w > s->w) w = s->w - x;
		if (y + h > s->h) h = s->h - y;
		if (w > 0 && h > 0)
		{
			int bpp = s->format ? s->format->BytesPerPixel : 1;
			Uint8 *pixels = (Uint8 *)s->pixels;
			for (int row = y; row < y + h; row++)
				memset(pixels + row * s->pitch + x * bpp, (Uint8)color, (size_t)w * bpp);
		}
	}
#else
	SDL_FillRect(_surface.get(), rect, color);
#endif
}

/**
 * Draws a filled rectangle on the surface.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 * @param w Width in pixels.
 * @param h Height in pixels.
 * @param color Color of the rectangle.
 */
void Surface::drawRect(Sint16 x, Sint16 y, Sint16 w, Sint16 h, Uint8 color)
{
	if (w == 0 || h == 0) return;

	SDL_Rect rect;
	rect.w = w;
	rect.h = h;
	rect.x = x;
	rect.y = y;
#ifdef __EMSCRIPTEN__
	SDL_Surface *s = _surface.get();
	if (s && s->pixels)
	{
		int cx = x, cy = y, cw = w, ch = h;
		if (cx < 0) { cw += cx; cx = 0; }
		if (cy < 0) { ch += cy; cy = 0; }
		if (cx + cw > s->w) cw = s->w - cx;
		if (cy + ch > s->h) ch = s->h - cy;
		if (cw > 0 && ch > 0)
		{
			int bpp = s->format ? s->format->BytesPerPixel : 1;
			Uint8 *pixels = (Uint8 *)s->pixels;
			for (int row = cy; row < cy + ch; row++)
				memset(pixels + row * s->pitch + cx * bpp, (Uint8)color, (size_t)cw * bpp);
		}
	}
#else
	SDL_FillRect(_surface.get(), &rect, color);
#endif
}

/**
 * Draws a line on the surface.
 * @param x1 Start x coordinate in pixels.
 * @param y1 Start y coordinate in pixels.
 * @param x2 End x coordinate in pixels.
 * @param y2 End y coordinate in pixels.
 * @param color Color of the line.
 */
void Surface::drawLine(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 color)
{
#ifdef __EMSCRIPTEN__
	if (!getPalette()) { static bool once = false; if (!once) { once = true; Log(LOG_WARNING) << "drawLine on ARGB surface — no-op"; } return; }
#endif
	lineColor(_surface.get(), x1, y1, x2, y2, Palette::getRGBA(getPalette(), color));
}

/**
 * Draws a filled circle on the surface.
 * @param x X coordinate in pixels.
 * @param y Y coordinate in pixels.
 * @param r Radius in pixels.
 * @param color Color of the circle.
 */
void Surface::drawCircle(Sint16 x, Sint16 y, Sint16 r, Uint8 color)
{
#ifdef __EMSCRIPTEN__
	if (!getPalette()) { static bool once = false; if (!once) { once = true; Log(LOG_WARNING) << "drawCircle on ARGB surface — no-op"; } return; }
#endif
	filledCircleColor(_surface.get(), x, y, r, Palette::getRGBA(getPalette(), color));
}

/**
 * Draws a filled polygon on the surface.
 * @param x Array of x coordinates.
 * @param y Array of y coordinates.
 * @param n Number of points.
 * @param color Color of the polygon.
 */
void Surface::drawPolygon(Sint16 *x, Sint16 *y, int n, Uint8 color)
{
#ifdef __EMSCRIPTEN__
	if (!getPalette()) { static bool once = false; if (!once) { once = true; Log(LOG_WARNING) << "drawPolygon on ARGB surface — no-op"; } return; }
#endif
	filledPolygonColor(_surface.get(), x, y, n, Palette::getRGBA(getPalette(), color));
}

/**
 * Draws a textured polygon on the surface.
 * @param x Array of x coordinates.
 * @param y Array of y coordinates.
 * @param n Number of points.
 * @param texture Texture for polygon.
 * @param dx X offset of texture relative to the screen.
 * @param dy Y offset of texture relative to the screen.
 */
void Surface::drawTexturedPolygon(Sint16 *x, Sint16 *y, int n, Surface *texture, int dx, int dy)
{
	texturedPolygon(_surface.get(), x, y, n, texture->getSurface(), dx, dy);
}

/**
 * Draws a text string on the surface.
 * @param x X coordinate in pixels.
 * @param y Y coordinate in pixels.
 * @param s Character string to draw.
 * @param color Color of string.
 */
void Surface::drawString(Sint16 x, Sint16 y, const char *s, Uint8 color)
{
#ifdef __EMSCRIPTEN__
	if (!getPalette()) { static bool once = false; if (!once) { once = true; Log(LOG_WARNING) << "drawString on ARGB surface — no-op"; } return; }
#endif
	stringColor(_surface.get(), x, y, s, Palette::getRGBA(getPalette(), color));
}

/**
 * Changes the position of the surface in the X axis.
 * @param x X position in pixels.
 */
void Surface::setX(int x)
{
	_x = x;
}

/**
 * Changes the position of the surface in the Y axis.
 * @param y Y position in pixels.
 */
void Surface::setY(int y)
{
	_y = y;
}

/**
 * Changes the visibility of the surface. A hidden surface
 * isn't blitted nor receives events.
 * @param visible New visibility.
 */
void Surface::setVisible(bool visible)
{
	_visible = visible;
}

/**
 * Returns the visible state of the surface.
 * @return Current visibility.
 */
bool Surface::getVisible() const
{
	return _visible;
}

/**
 * Returns the cropping rectangle for this surface.
 * @return Pointer to the cropping rectangle.
 */
SurfaceCrop Surface::getCrop() const
{
	return SurfaceCrop{ this };
}

/**
 * Replaces a certain amount of colors in the surface's palette.
 * @param colors Pointer to the set of colors.
 * @param firstcolor Offset of the first color to replace.
 * @param ncolors Amount of colors to replace.
 */
void Surface::setPalette(const SDL_Color *colors, int firstcolor, int ncolors)
{
	if (colors && _surface->format->BitsPerPixel == 8)
		SDL_SetColors(_surface.get(), const_cast<SDL_Color *>(colors), firstcolor, ncolors);
}

/**
 * This is a separate visibility setting intended
 * for temporary effects like window popups,
 * so as to not override the default visibility setting.
 * @note Do not confuse with setVisible!
 * @param hidden Shown or hidden.
 */
void Surface::setHidden(bool hidden)
{
	_hidden = hidden;
}

/**
 * Locks the surface from outside access
 * for pixel-level access. Must be unlocked
 * afterwards.
 * @sa unlock()
 */
void Surface::lock()
{
	SDL_LockSurface(_surface.get());
}

/**
 * Unlocks the surface after it's been locked
 * to resume blitting operations.
 * @sa lock()
 */
void Surface::unlock()
{
	SDL_UnlockSurface(_surface.get());
}

/**
 * Specific blit function to blit battlescape terrain data in different shades in a fast way.
 */
void Surface::blitRaw(SurfaceRaw<Uint8> destSurf, SurfaceRaw<const Uint8> srcSurf, int x, int y, int shade, bool half, int newBaseColor)
{
	ShaderMove<const Uint8> src(srcSurf, x, y);
	if (half)
	{
		GraphSubset g = src.getDomain();
		g.beg_x = g.end_x/2;
		src.setDomain(g);
	}
	if (newBaseColor)
	{
		--newBaseColor;
		newBaseColor <<= 4;
		ShaderDraw<helper::ColorReplace>(ShaderSurface(destSurf), src, ShaderScalar(shade), ShaderScalar(newBaseColor));
	}
	else
	{
		ShaderDraw<helper::StandardShade>(ShaderSurface(destSurf), src, ShaderScalar(shade));
	}
}

/**
 * Specific blit function to blit battlescape terrain data in different shades in a fast way.
 * Notice there is no surface locking here - you have to make sure you lock the surface yourself
 * at the start of blitting and unlock it when done.
 * @param surface to blit to
 * @param x
 * @param y
 * @param off
 * @param half some tiles are blitted only the right half
 * @param newBaseColor Attention: the actual color + 1, because 0 is no new base color.
 */
void Surface::blitNShade(SurfaceRaw<Uint8> surface, int x, int y, int shade, bool half, int newBaseColor) const
{
	blitRaw(surface, SurfaceRaw<const Uint8>(this), x, y, shade, half, newBaseColor);
}

/**
 * Specific blit function to blit battlescape terrain data in different shades in a fast way.
 * @param surface destination blit to
 * @param x
 * @param y
 * @param shade shade offset
 * @param range area that limit draw surface
 */
void Surface::blitNShade(SurfaceRaw<Uint8> surface, int x, int y, int shade, GraphSubset range) const
{
	ShaderMove<const Uint8> src(this, x, y);
	ShaderMove<Uint8> dest(surface);

	dest.setDomain(range);

	ShaderDraw<helper::StandardShade>(dest, src, ShaderScalar(shade));
}

/**
 * Set the surface to be redrawn.
 * @param valid true means redraw.
 */
void Surface::invalidate(bool valid)
{
	_redraw = valid;
}

/**
 * Recreates the surface with a new size.
 * Old contents will not be altered, and may be
 * cropped to fit the new size.
 * @param width Width in pixels.
 * @param height Height in pixels.
 */
void Surface::resize(int width, int height)
{
#ifdef __EMSCRIPTEN__
	if (_surface && _surface->format->BitsPerPixel == 32)
	{
		// HD/ARGB resize path. RawCopySurf is hard-coded to Uint8 stride and
		// SDL_SetColors / getPalette() do not apply to 32 bpp surfaces, so
		// the 8 bpp path below would silently corrupt ARGB pixels.
		auto pair = Surface::NewPair32Bit(width, height);
		SDL_SetSurfaceBlendMode(pair.second.get(), SDL_BLENDMODE_BLEND);
		SDL_SetSurfaceBlendMode(_surface.get(), SDL_BLENDMODE_NONE);
		SDL_BlitSurface(_surface.get(), nullptr, pair.second.get(), nullptr);
		SDL_SetSurfaceBlendMode(_surface.get(), SDL_BLENDMODE_BLEND);
		std::tie(_alignedBuffer, _surface) = std::move(pair);
		_width  = (Uint16)_surface->w;
		_height = (Uint16)_surface->h;
		_pitch  = (Uint16)_surface->pitch;
		// Keep _isHD = true. Reset _logicalW/H — the caller must call
		// setLogicalSize() again for the new dimensions.
		_logicalW = _logicalH = 0;
		return;
	}
	_isHD = false; _logicalW = _logicalH = 0;
#endif
	// Set up new surface (8 bpp path)
	Uint8 bpp = _surface->format->BitsPerPixel;
	auto alignedBuffer = NewAlignedBuffer(bpp, width, height);
	auto surface = NewSdlSurface(alignedBuffer, bpp, width, height);

	// Copy old contents
	SDL_SetColorKey(surface.get(), SDL_SRCCOLORKEY, 0);
	SDL_SetColors(surface.get(), getPalette(), 0, 256);

	RawCopySurf(surface, _surface);

	// Delete old surface
	_surface = std::move(surface);
	_alignedBuffer = std::move(alignedBuffer);
	_width = _surface->w;
	_height = _surface->h;
	_pitch = _surface->pitch;
}

#ifdef __EMSCRIPTEN__
/**
 * Scales the HD surface to the target logical size using bilinear filtering.
 * Done once at load time; subsequent blit() uses the pre-scaled pixels.
 * No-op for 8bpp surfaces or when the size already matches.
 */
static Surface::UniqueSurfacePtr preScaleHDBilinear(SDL_Surface *src, int w, int h)
{
	auto dst = Surface::NewSdlSurface(
		SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888));
	if (!dst) return nullptr;

	SDL_SetSurfaceBlendMode(dst.get(), SDL_BLENDMODE_BLEND);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(dst.get());
	if (!renderer) return nullptr;

	SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, src);
	if (!tex) { SDL_DestroyRenderer(renderer); return nullptr; }

	SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, tex, nullptr, nullptr);
	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(renderer);

	return dst;
}

/**
 * Stores the logical (game-coordinate) size for this HD surface and
 * pre-scales the pixel data to match if the native size differs.
 * Call immediately after loadImageHD to set the intended display size.
 */
void Surface::setLogicalSize(int w, int h)
{
	_logicalW = (Uint16)w;
	_logicalH = (Uint16)h;

	if (_isHD && _surface && (_surface->w != w || _surface->h != h))
	{
		if (auto scaled = preScaleHDBilinear(_surface.get(), w, h))
		{
			_surface = std::move(scaled);
			_width   = (Uint16)w;
			_height  = (Uint16)h;
			_pitch   = (Uint16)_surface->pitch;
		}
	}
}

/**
 * Promotes this surface from 8bpp indexed to 32bpp ARGB in-place.
 * No-op if already ARGB. Used by UI containers that need to host HD children.
 * Existing palette pixels are composited into the new ARGB buffer.
 */
void Surface::promoteToARGB()
{
	if (!_surface || _surface->format->BitsPerPixel == 32) return;

	auto pair = Surface::NewPair32Bit(_width, _height);
	SDL_SetSurfaceBlendMode(pair.second.get(), SDL_BLENDMODE_BLEND);
	SDL_BlitSurface(_surface.get(), nullptr, pair.second.get(), nullptr);
	std::tie(_alignedBuffer, _surface) = std::move(pair);
	_pitch = (Uint16)_surface->pitch;
}
#endif

/**
 * Changes the width of the surface.
 * @warning This is not a trivial setter!
 * It will force the surface to be recreated for the new size.
 * @param width New width in pixels.
 */
void Surface::setWidth(int width)
{
	resize(width, getHeight());
	_redraw = true;
}

/**
 * Changes the height of the surface.
 * @warning This is not a trivial setter!
 * It will force the surface to be recreated for the new size.
 * @param height New height in pixels.
 */
void Surface::setHeight(int height)
{
	resize(getWidth(), height);
	_redraw = true;
}

/**
 * Blit surface with crop
 * @param dest
 */
void SurfaceCrop::blit(Surface* dest)
{
	if (_surface)
	{
		auto srcShader = ShaderCrop(*this, _x, _y);
		auto destShader = ShaderMove<Uint8>(dest, 0, 0);

		ShaderDrawFunc(
			[](Uint8& d, Uint8 s)
			{
				if (s) d = s;
			},
			destShader,
			srcShader
		);
	}
}

}
