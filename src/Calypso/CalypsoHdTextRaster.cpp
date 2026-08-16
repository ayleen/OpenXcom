/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- see CalypsoHdTextRaster.h.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdTextRaster.h"

#include "../Engine/FileMap.h"
#include "../Engine/Logger.h"

#include <vector>

namespace OpenXcom
{
namespace Calypso
{

namespace
{

/// True iff every renderable codepoint in `text` has a real glyph in `face`.
///
/// SDL_ttf's TTF_Render* NEVER fails on missing glyphs -- it silently substitutes
/// the font's .notdef box (tofu). Treating a successful render as "text is fine"
/// therefore breaks the HD overlay's atomic-fallback contract: a Cyrillic /
/// Arabic / CJK string rendered against a Latin-only F34 face would raster to a
/// row of squares, the subgroup would be marked Ready, and the CORRECT bitmap
/// Font text would be suppressed by the claim (external review #5). We instead
/// pre-flight glyph coverage and report failure when any codepoint is missing so
/// the whole atomic subgroup stays logical (correct bitmap text) rather than
/// showing tofu.
///
/// Whitespace / control codepoints (<= 0x20: space, '\n' line break, '\t') are
/// layout, not glyphs, and are skipped. Astral codepoints (> 0xFFFF: emoji, rare
/// CJK ext) cannot be probed with the BMP-only TTF_GlyphIsProvided(Uint16) that
/// is guaranteed present across SDL_ttf versions, so they are conservatively
/// treated as UNCOVERED -- the safe direction (fall back to bitmap, never tofu).
bool faceCoversText(TTF_Font* face, const std::string& text)
{
	const unsigned char* s = reinterpret_cast<const unsigned char*>(text.c_str());
	const std::size_t n = text.size();
	std::size_t i = 0;
	while (i < n)
	{
		const unsigned char b0 = s[i];
		std::uint32_t cp;
		std::size_t len;
		if (b0 < 0x80u) { cp = b0; len = 1; }
		else if ((b0 & 0xE0u) == 0xC0u && i + 1 < n)
		{
			cp = ((b0 & 0x1Fu) << 6) | (s[i + 1] & 0x3Fu); len = 2;
		}
		else if ((b0 & 0xF0u) == 0xE0u && i + 2 < n)
		{
			cp = ((b0 & 0x0Fu) << 12) | ((s[i + 1] & 0x3Fu) << 6) | (s[i + 2] & 0x3Fu); len = 3;
		}
		else if ((b0 & 0xF8u) == 0xF0u && i + 3 < n)
		{
			cp = ((b0 & 0x07u) << 18) | ((s[i + 1] & 0x3Fu) << 12)
				| ((s[i + 2] & 0x3Fu) << 6) | (s[i + 3] & 0x3Fu); len = 4;
		}
		else
		{
			return false; // malformed UTF-8 -> logical fallback
		}
		i += len;
		if (cp <= 0x20u) continue;        // space / control / newline: not a glyph
		if (cp > 0xFFFFu) return false;   // astral: can't BMP-probe -> safe fallback
		if (!TTF_GlyphIsProvided(face, static_cast<Uint16>(cp))) return false;
	}
	return true;
}

/// Decode one UTF-8 codepoint at `i` in `s` (size n). Returns false on
/// malformed input. BMP-only callers: astral codepoints are rejected by
/// faceCoversText before any rendering happens.
bool decodeUtf8At(const unsigned char* s, std::size_t n, std::size_t i,
	std::uint32_t& cp, std::size_t& len)
{
	const unsigned char b0 = s[i];
	if (b0 < 0x80u) { cp = b0; len = 1; }
	else if ((b0 & 0xE0u) == 0xC0u && i + 1 < n)
	{
		cp = ((b0 & 0x1Fu) << 6) | (s[i + 1] & 0x3Fu); len = 2;
	}
	else if ((b0 & 0xF0u) == 0xE0u && i + 2 < n)
	{
		cp = ((b0 & 0x0Fu) << 12) | ((s[i + 1] & 0x3Fu) << 6) | (s[i + 2] & 0x3Fu); len = 3;
	}
	else if ((b0 & 0xF8u) == 0xF0u && i + 3 < n)
	{
		cp = ((b0 & 0x07u) << 18) | ((s[i + 1] & 0x3Fu) << 12)
			| ((s[i + 2] & 0x3Fu) << 6) | (s[i + 3] & 0x3Fu); len = 4;
	}
	else
	{
		return false;
	}
	return true;
}

void encodeUtf8(std::uint32_t cp, char out[5])
{
	if (cp < 0x80u)
	{
		out[0] = (char)cp; out[1] = 0;
	}
	else if (cp < 0x800u)
	{
		out[0] = (char)(0xC0u | (cp >> 6)); out[1] = (char)(0x80u | (cp & 0x3Fu)); out[2] = 0;
	}
	else if (cp < 0x10000u)
	{
		out[0] = (char)(0xE0u | (cp >> 12)); out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
		out[2] = (char)(0x80u | (cp & 0x3Fu)); out[3] = 0;
	}
	else
	{
		out[0] = (char)(0xF0u | (cp >> 18)); out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
		out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu)); out[3] = (char)(0x80u | (cp & 0x3Fu)); out[4] = 0;
	}
}

/// Tracked single-line composition (letterSpacingPx > 0, wrapWidth == 0).
/// SDL_ttf's string renderer has no tracking, so each codepoint is rendered
/// as a one-character string (bearing math stays SDL_ttf's own) and copied
/// onto a transparent ARGB canvas, advancing by glyph advance + tracking.
/// '\n' still breaks lines (the wrapWidth==0 contract); wrapped text never
/// takes this path.
SDL_Surface* rasterTracked(TTF_Font* face, const std::string& text, int trackingPx, SDL_Color c)
{
	int minx = 0, maxx = 0, miny = 0, maxy = 0;
	int advance = 0;
	int spaceAdvance = 0;
	if (!TTF_GlyphMetrics(face, static_cast<Uint16>(u' '), &minx, &maxx, &miny, &maxy, &spaceAdvance))
	{
		spaceAdvance = TTF_FontHeight(face) / 3; // defensive fallback; metrics rarely fail
	}

	const unsigned char* s = reinterpret_cast<const unsigned char*>(text.c_str());
	const std::size_t n = text.size();

	// Pass 1: measure per line (advance + tracking, no trailing tracking).
	int maxWidth = 1;
	int lineCount = 1;
	{
		int pen = 0;
		std::size_t i = 0;
		while (i < n)
		{
			std::uint32_t cp; std::size_t len;
			if (!decodeUtf8At(s, n, i, cp, len)) return nullptr;
			i += len;
			if (cp == u'\n') { maxWidth = std::max(maxWidth, pen); pen = 0; ++lineCount; continue; }
			if (cp <= 0x20u) { pen += spaceAdvance + trackingPx; continue; }
			if (!TTF_GlyphMetrics(face, static_cast<Uint16>(cp), &minx, &maxx, &miny, &maxy, &advance))
			{
				return nullptr;
			}
			pen += advance + trackingPx;
		}
		maxWidth = std::max(maxWidth, pen - trackingPx);
	}

	const int lineSkip = TTF_FontLineSkip(face);
	const int fontHeight = TTF_FontHeight(face);
	const int height = lineCount <= 1 ? fontHeight : (lineCount - 1) * lineSkip + fontHeight;

	SDL_Surface* canvas = SDL_CreateRGBSurfaceWithFormat(0, maxWidth, height, 32,
		SDL_PIXELFORMAT_ARGB8888);
	if (!canvas) return nullptr;

	// Pass 2: compose.
	{
		int x = 0, y = 0;
		std::size_t i = 0;
		while (i < n)
		{
			std::uint32_t cp; std::size_t len;
			if (!decodeUtf8At(s, n, i, cp, len)) { SDL_FreeSurface(canvas); return nullptr; }
			i += len;
			if (cp == u'\n') { x = 0; y += lineSkip; continue; }
			if (cp <= 0x20u) { x += spaceAdvance + trackingPx; continue; }

			char utf8[5];
			encodeUtf8(cp, utf8);
			SDL_Surface* glyph = TTF_RenderUTF8_Blended(face, utf8, c);
			if (glyph)
			{
				// Copy, don't alpha-blend: the canvas is transparent and glyph
				// boxes advance past each other, so overwrite is exact.
				SDL_SetSurfaceBlendMode(glyph, SDL_BLENDMODE_NONE);
				SDL_Rect dst{ x, y, glyph->w, glyph->h };
				SDL_BlitSurface(glyph, nullptr, canvas, &dst);
				SDL_FreeSurface(glyph);
			}
			if (!TTF_GlyphMetrics(face, static_cast<Uint16>(cp), &minx, &maxx, &miny, &maxy, &advance))
			{
				SDL_FreeSurface(canvas);
				return nullptr;
			}
			x += advance + trackingPx;
		}
	}
	return canvas;
}

} // namespace

CalypsoHdTextRaster::CalypsoHdTextRaster(std::size_t rasterByteBudget, std::size_t maxFaces)
	: _maxFaces(maxFaces), _lru(rasterByteBudget)
{
}

CalypsoHdTextRaster::~CalypsoHdTextRaster()
{
	clear();
}

TTF_Font* CalypsoHdTextRaster::faceFor(const std::string& vfsPath, int physicalPixelHeight,
	std::uint64_t resourceGeneration)
{
	if (physicalPixelHeight <= 0)
	{
		return nullptr;
	}

	FaceKey key{vfsPath, physicalPixelHeight, resourceGeneration};
	auto it = _faces.find(key);
	if (it != _faces.end())
	{
		return it->second;
	}

	if (!FileMap::fileExists(vfsPath))
	{
		Log(LOG_ERROR) << "CalypsoHdTextRaster: file not found in VFS: \"" << vfsPath << "\"";
		return nullptr;
	}
	SDL_RWops* rw = FileMap::getRWops(vfsPath);
	if (!rw)
	{
		Log(LOG_ERROR) << "CalypsoHdTextRaster: getRWops failed for \"" << vfsPath << "\"";
		return nullptr;
	}
	TTF_Font* face = TTF_OpenFontRW(rw, /*freesrc=*/1, physicalPixelHeight);
	if (!face)
	{
		Log(LOG_ERROR) << "CalypsoHdTextRaster: TTF_OpenFontRW failed for \""
			<< vfsPath << "\" size=" << physicalPixelHeight << ": " << TTF_GetError();
		return nullptr;
	}

	if (_faces.size() >= _maxFaces && !_faceOrder.empty())
	{
		FaceKey oldest = _faceOrder.front();
		_faceOrder.pop_front();
		auto oit = _faces.find(oldest);
		if (oit != _faces.end())
		{
			TTF_CloseFont(oit->second);
			_faces.erase(oit);
		}
	}

	_faces.emplace(key, face);
	_faceOrder.push_back(key);
	return face;
}

SDL_Surface* CalypsoHdTextRaster::rasterFor(const CalypsoHdTextRasterKey& key)
{
	if (key.text.empty())
	{
		return nullptr;
	}

	auto it = _rasters.find(key);
	if (it != _rasters.end())
	{
		auto hit = _rasterHandles.find(key);
		if (hit != _rasterHandles.end())
		{
			SDL_Surface* surf = it->second;
			std::size_t byteCost = static_cast<std::size_t>(surf->w) * static_cast<std::size_t>(surf->h) * 4u;
			_lru.touch(hit->second, byteCost); // refresh recency; not evicted (just-touched)
		}
		return it->second;
	}

	TTF_Font* face = faceFor(key.source.canonicalVfsPath, key.physicalPixelHeight,
		key.source.resourceGeneration);
	if (!face)
	{
		return nullptr;
	}

	// Glyph-coverage pre-flight (external review #5): if any codepoint has no
	// glyph in this face, rendering would emit .notdef tofu. Report failure so the
	// atomic HD subgroup stays fully logical (correct bitmap text) instead of
	// showing squares. Not cached: a miss is cheap (one scan) and rare.
	if (!faceCoversText(face, key.text))
	{
		return nullptr;
	}

	// RGBA packed as R in the high byte (matches CalypsoHdTextRasterKey's
	// colorRgba convention: 0xRRGGBBAA).
	SDL_Color c;
	c.r = static_cast<Uint8>((key.colorRgba >> 24) & 0xFFu);
	c.g = static_cast<Uint8>((key.colorRgba >> 16) & 0xFFu);
	c.b = static_cast<Uint8>((key.colorRgba >> 8) & 0xFFu);
	c.a = static_cast<Uint8>(key.colorRgba & 0xFFu);

	// Wrapped render (Fable #1 / Codex #5). key.wrapWidth == 0 wraps only on
	// embedded '\n' (single-line or author-broken text); key.wrapWidth > 0 lets
	// SDL_ttf break the text at that physical pixel width -- which breaks CJK and
	// no-space text correctly, unlike an ASCII space pre-wrap. The single-line
	// TTF_RenderUTF8_Blended would render '\n' as a notdef glyph, so always use
	// the _Wrapped variant. Tracked single-line text composes per-glyph instead
	// (SDL_ttf has no letter-spacing); the two paths share the coverage gate.
	SDL_Surface* surf = nullptr;
	if (key.letterSpacingPx > 0 && key.wrapWidth == 0)
	{
		surf = rasterTracked(face, key.text, key.letterSpacingPx, c);
		if (!surf)
		{
			Log(LOG_ERROR) << "CalypsoHdTextRaster: tracked raster failed: " << TTF_GetError();
		}
	}
	else
	{
		const Uint32 wrap = key.wrapWidth > 0 ? static_cast<Uint32>(key.wrapWidth) : 0u;
		surf = TTF_RenderUTF8_Blended_Wrapped(face, key.text.c_str(), c, wrap);
		if (!surf)
		{
			Log(LOG_ERROR) << "CalypsoHdTextRaster: TTF_RenderUTF8_Blended_Wrapped failed: " << TTF_GetError();
		}
	}
	if (!surf)
	{
		return nullptr;
	}

	const std::uint64_t handle = _nextHandle++;
	_rasters.emplace(key, surf);
	_rasterHandles.emplace(key, handle);
	_handleToKey.emplace(handle, key);

	const std::size_t byteCost = static_cast<std::size_t>(surf->w) * static_cast<std::size_t>(surf->h) * 4u;
	std::vector<std::uint64_t> evicted = _lru.touch(handle, byteCost);
	for (std::uint64_t evictedHandle : evicted)
	{
		auto kit = _handleToKey.find(evictedHandle);
		if (kit == _handleToKey.end())
		{
			continue;
		}
		const CalypsoHdTextRasterKey evictedKey = kit->second;
		auto sit = _rasters.find(evictedKey);
		if (sit != _rasters.end())
		{
			SDL_FreeSurface(sit->second);
			_rasters.erase(sit);
		}
		_rasterHandles.erase(evictedKey);
		_handleToKey.erase(kit);
	}

	return surf;
}

bool CalypsoHdTextRaster::warm(const CalypsoHdTextRasterKey& key)
{
	return rasterFor(key) != nullptr;
}

void CalypsoHdTextRaster::dropContextTextures()
{
	// No-op placeholder: this cache holds only CPU SDL_Surface* rasters, which
	// are not GL resources and survive a WebGL context loss unmodified. The
	// future GPU-texture cache layered on top of this class (HD.4) will
	// implement the equivalent hook to actually drop its GL textures.
}

void CalypsoHdTextRaster::clear()
{
	for (auto& kv : _rasters)
	{
		SDL_FreeSurface(kv.second);
	}
	_rasters.clear();
	_rasterHandles.clear();
	_handleToKey.clear();
	_lru.clear();

	for (auto& kv : _faces)
	{
		TTF_CloseFont(kv.second);
	}
	_faces.clear();
	_faceOrder.clear();
}

std::size_t CalypsoHdTextRaster::rasterBytes() const
{
	return _lru.bytes();
}

std::size_t CalypsoHdTextRaster::rasterCount() const
{
	return _rasters.size();
}

std::size_t CalypsoHdTextRaster::faceCount() const
{
	return _faces.size();
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
