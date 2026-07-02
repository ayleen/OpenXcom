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
#include "TTFFont.h"
#include "Logger.h"
#include <functional>

namespace OpenXcom
{

// ── KeyHash ──────────────────────────────────────────────────────────────────

size_t TTFFont::KeyHash::operator()(const Key& k) const
{
	size_t h = std::hash<std::string>{}(k.s);
	h ^= std::hash<uint32_t>{}(k.rgba) + 0x9e3779b9u + (h << 6) + (h >> 2);
	return h;
}

// ── TTFFont ───────────────────────────────────────────────────────────────────

#ifdef __EMSCRIPTEN__

TTFFont::TTFFont(const std::string& vfsPath, int pixelSize)
	: _vfsPath(vfsPath), _pixelSize(pixelSize), _fontAttempted(false), _font(nullptr)
{
	// L12 memory-reduction: defer TTF_OpenFontRW to the first renderText() /
	// lineHeight() call.  FreeType glyph-outline structures are created eagerly
	// at open time and account for the bulk of the extra/fonts heap spike; by
	// deferring we avoid paying for battlescape-only sizes (e.g. FONT_HD_HUD
	// at 48 px) during boot and geoscape sessions where they are never drawn.
}

void TTFFont::ensureFont() const
{
	if (_fontAttempted) return;
	_fontAttempted = true;

	if (!FileMap::fileExists(_vfsPath))
	{
		Log(LOG_ERROR) << "TTFFont: file not found in VFS: \"" << _vfsPath << "\"";
		return;
	}
	SDL_RWops* rw = FileMap::getRWops(_vfsPath);
	if (!rw)
	{
		Log(LOG_ERROR) << "TTFFont: getRWops failed for \"" << _vfsPath << "\"";
		return;
	}
	_font = TTF_OpenFontRW(rw, /*freesrc=*/1, _pixelSize);
	if (!_font)
	{
		Log(LOG_ERROR) << "TTFFont: TTF_OpenFontRW failed for \""
		               << _vfsPath << "\" size=" << _pixelSize
		               << ": " << TTF_GetError();
	}
	else
	{
		Log(LOG_INFO) << "TTFFont: opened \"" << _vfsPath
		              << "\" size=" << _pixelSize << " (lazy init)";
	}
}

#else // !__EMSCRIPTEN__

TTFFont::TTFFont(const std::string& vfsPath, int pixelSize)
	: _font(nullptr)
{
	// fileExists() is safe (no throw); getRWops/at() throws on missing file,
	// propagating up through Mod::loadMod to disable the entire mod.
	if (!FileMap::fileExists(vfsPath))
	{
		Log(LOG_ERROR) << "TTFFont: file not found in VFS: \"" << vfsPath << "\"";
		return;
	}
	SDL_RWops* rw = FileMap::getRWops(vfsPath);
	if (!rw)
	{
		Log(LOG_ERROR) << "TTFFont: getRWops failed for \"" << vfsPath << "\"";
		return;
	}
	_font = TTF_OpenFontRW(rw, /*freesrc=*/1, pixelSize);
	if (!_font)
	{
		Log(LOG_ERROR) << "TTFFont: TTF_OpenFontRW failed for \"" << vfsPath << "\": " << TTF_GetError();
	}
}

#endif // __EMSCRIPTEN__

TTFFont::~TTFFont()
{
	for (auto& kv : _cache)
		SDL_FreeSurface(kv.second);
	if (_font)
		TTF_CloseFont(_font);
}

SDL_Surface* TTFFont::renderText(const std::string& utf8, SDL_Color rgba)
{
#ifdef __EMSCRIPTEN__
	ensureFont();
#endif
	if (!_font) return nullptr;

	uint32_t packed = ((uint32_t)rgba.a << 24) | ((uint32_t)rgba.r << 16)
	                | ((uint32_t)rgba.g << 8)  |  (uint32_t)rgba.b;
	Key key{utf8, packed};

	auto it = _cache.find(key);
	if (it != _cache.end())
		return it->second;

	// Cache miss — rasterise.
	SDL_Surface* surf = TTF_RenderUTF8_Blended(_font, utf8.c_str(), rgba);
	if (!surf)
	{
		Log(LOG_WARNING) << "TTFFont: TTF_RenderUTF8_Blended failed: " << TTF_GetError();
		return nullptr;
	}

	// Evict oldest entry if at capacity.
	if (_cache.size() >= MAX_CACHE)
	{
		const Key& oldest = _order.front();
		auto evict = _cache.find(oldest);
		if (evict != _cache.end())
		{
			SDL_FreeSurface(evict->second);
			_cache.erase(evict);
		}
		_order.pop_front();
	}

	_cache.emplace(key, surf);
	_order.push_back(key);
	return surf;
}

int TTFFont::lineHeight() const
{
#ifdef __EMSCRIPTEN__
	ensureFont();
#endif
	return _font ? TTF_FontLineSkip(_font) : 0;
}

} // namespace OpenXcom
