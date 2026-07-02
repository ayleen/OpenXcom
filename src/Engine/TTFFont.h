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
#pragma once
#include <string>
#include <unordered_map>
#include <list>
#include <SDL_ttf.h>
#include <SDL.h>
#include "FileMap.h"

namespace OpenXcom
{

/**
 * Phase 16: TrueType font wrapper with a bounded ARGB glyph-string cache.
 *
 * Owns one TTF_Font* at a fixed pixel size.  renderText() rasterises a UTF-8
 * string via TTF_RenderUTF8_Blended and caches the resulting SDL_Surface*.
 * Callers blit from the returned surface but must NOT free it — TTFFont owns
 * all cached surfaces.
 *
 * Cache is bounded at 256 entries (FIFO eviction).  The cursor-text working
 * set is ~50 strings × 2 colours, so 256 is a comfortable ceiling.
 *
 * L12 memory-reduction: font face is opened lazily on the first renderText() /
 * lineHeight() call under __EMSCRIPTEN__.  Construction stores the VFS path and
 * pixel size but does NOT call TTF_OpenFontRW, deferring the FreeType heap cost
 * until the font is actually needed (battlescape-only sizes pay nothing on boot).
 * On native builds the eager path is kept unchanged.
 */
class TTFFont
{
public:
	TTFFont(const std::string& vfsPath, int pixelSize);
	~TTFFont();

	/** Returns a cached, owned ARGB surface.  Caller blits, does not free.
	 *  Returns nullptr if the font failed to load. */
	SDL_Surface* renderText(const std::string& utf8, SDL_Color rgba);

	int lineHeight() const;

private:
#ifdef __EMSCRIPTEN__
	/** Deferred open: attempts TTF_OpenFontRW on the first call, then noops. */
	void ensureFont() const;

	std::string       _vfsPath;
	int               _pixelSize;
	mutable bool      _fontAttempted; // true once ensureFont() has run
	mutable TTF_Font* _font;
#else
	TTF_Font* _font;
#endif

	struct Key
	{
		std::string s;
		uint32_t    rgba;
		bool operator==(const Key& o) const { return s == o.s && rgba == o.rgba; }
	};
	struct KeyHash
	{
		size_t operator()(const Key& k) const;
	};

	static constexpr size_t MAX_CACHE = 256;

	std::unordered_map<Key, SDL_Surface*, KeyHash> _cache;
	std::list<Key>                                  _order; // FIFO eviction order
};

} // namespace OpenXcom
