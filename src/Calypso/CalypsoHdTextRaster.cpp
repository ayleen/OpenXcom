/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- see CalypsoHdTextRaster.h.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdTextRaster.h"

#include "CalypsoHdDiagnostics.h"
#include "CalypsoHdTrackedText.h"

#include "../Engine/FileMap.h"
#include "../Engine/Logger.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace OpenXcom
{
namespace Calypso
{

namespace
{

std::size_t nextUtf8Boundary(const std::string& text, std::size_t offset)
{
	if (offset >= text.size()) return text.size();
	const unsigned char c = static_cast<unsigned char>(text[offset]);
	std::size_t length = 1;
	if ((c & 0xE0u) == 0xC0u) length = 2;
	else if ((c & 0xF0u) == 0xE0u) length = 3;
	else if ((c & 0xF8u) == 0xF0u) length = 4;
	return std::min(text.size(), offset + length);
}

bool isAsciiSpaceAt(const std::string& text, std::size_t offset)
{
	return offset < text.size()
		&& static_cast<unsigned char>(text[offset]) <= 0x20u;
}

int utf8Width(TTF_Font* face, const std::string& text)
{
	int width = 0;
	int height = 0;
	return TTF_SizeUTF8(face, text.c_str(), &width, &height) == 0 ? width : -1;
}

void trimLineEnd(std::string& line)
{
	while (!line.empty() && static_cast<unsigned char>(line.back()) <= 0x20u)
	{
		line.pop_back();
	}
}

void trimLineStart(std::string& text)
{
	std::size_t first = 0;
	while (first < text.size() && isAsciiSpaceAt(text, first)) ++first;
	text.erase(0, first);
}

/// Portable equivalent of SDL_ttf's wrapped renderer with a caller-owned line
/// skip. Emscripten's pinned SDL_ttf exposes the wrapped renderer and metrics,
/// but not TTF_SetFontLineSkip, so line breaks are measured explicitly and each
/// line is blitted at the canonical physical line height.
SDL_Surface* renderWrappedWithLineHeight(TTF_Font* face, const std::string& text,
	int wrapWidth, int lineHeightPx, int horizontalScalePermille, SDL_Color color)
{
	if (!face || text.empty() || wrapWidth <= 0 || lineHeightPx <= 0) return nullptr;
	const double horizontalScale = std::max(0.01, horizontalScalePermille / 1000.0);
	const int textWrapWidth = std::max(1, static_cast<int>(wrapWidth / horizontalScale));

	std::vector<std::string> lines;
	std::size_t paragraphStart = 0;
	while (paragraphStart <= text.size())
	{
		const std::size_t newline = text.find('\n', paragraphStart);
		const std::size_t paragraphEnd = newline == std::string::npos ? text.size() : newline;
		std::string remaining = text.substr(paragraphStart, paragraphEnd - paragraphStart);
		if (remaining.empty())
		{
			lines.emplace_back();
		}
		else
		{
			while (!remaining.empty())
			{
				if (utf8Width(face, remaining) <= textWrapWidth)
				{
					lines.push_back(remaining);
					break;
				}

				std::size_t cursor = 0;
				std::size_t fittingEnd = 0;
				std::size_t lastSpaceEnd = 0;
				while (cursor < remaining.size())
				{
					const std::size_t next = nextUtf8Boundary(remaining, cursor);
					const std::string candidate = remaining.substr(0, next);
					if (utf8Width(face, candidate) > textWrapWidth) break;
					fittingEnd = next;
					if (isAsciiSpaceAt(remaining, cursor)) lastSpaceEnd = next;
					cursor = next;
				}
				std::size_t breakAt = lastSpaceEnd > 0 ? lastSpaceEnd : fittingEnd;
				if (breakAt == 0) breakAt = nextUtf8Boundary(remaining, 0);

				std::string line = remaining.substr(0, breakAt);
				trimLineEnd(line);
				lines.push_back(std::move(line));
				remaining.erase(0, breakAt);
				trimLineStart(remaining);
			}
		}

		if (newline == std::string::npos) break;
		paragraphStart = newline + 1;
	}

	const int lineSkip = lineHeightPx;
	std::vector<SDL_Surface*> rendered;
	rendered.reserve(lines.size());
	int width = 1;
	for (const std::string& line : lines)
	{
		SDL_Surface* glyphs = line.empty() ? nullptr : TTF_RenderUTF8_Blended(face, line.c_str(), color);
		if (!line.empty() && !glyphs)
		{
			for (SDL_Surface* prior : rendered) SDL_FreeSurface(prior);
			return nullptr;
		}
		if (glyphs)
		{
			const int scaledWidth = std::max(1,
				static_cast<int>(glyphs->w * horizontalScale + 0.5));
			width = std::max(width, scaledWidth);
		}
		rendered.push_back(glyphs);
	}

	const int glyphHeight = TTF_FontHeight(face);
	const int height = std::max(glyphHeight,
		(static_cast<int>(rendered.size()) - 1) * lineSkip + glyphHeight);
	SDL_Surface* canvas = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
		SDL_PIXELFORMAT_ARGB8888);
	if (!canvas)
	{
		for (SDL_Surface* glyphs : rendered) SDL_FreeSurface(glyphs);
		return nullptr;
	}
	SDL_SetSurfaceBlendMode(canvas, SDL_BLENDMODE_NONE);
	for (std::size_t i = 0; i < rendered.size(); ++i)
	{
		SDL_Surface* glyphs = rendered[i];
		if (!glyphs) continue;
		SDL_SetSurfaceBlendMode(glyphs, SDL_BLENDMODE_NONE);
		const int scaledWidth = std::max(1,
			static_cast<int>(glyphs->w * horizontalScale + 0.5));
		SDL_Rect destination{ 0, static_cast<int>(i) * lineSkip, scaledWidth, glyphs->h };
		const int blitResult = horizontalScalePermille == 1000
			? SDL_BlitSurface(glyphs, nullptr, canvas, &destination)
			: SDL_BlitScaled(glyphs, nullptr, canvas, &destination);
		if (blitResult != 0)
		{
			for (SDL_Surface* prior : rendered) SDL_FreeSurface(prior);
			SDL_FreeSurface(canvas);
			return nullptr;
		}
	}
	for (SDL_Surface* glyphs : rendered) SDL_FreeSurface(glyphs);
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
		if (_diag.shouldLog(CalypsoDiagnosticKey{ "faceOpen", resourceGeneration,
			std::hash<std::string>{}(vfsPath) }))
		{
			Log(LOG_ERROR) << "CalypsoHdTextRaster: file not found in VFS: \"" << vfsPath << "\"";
		}
		return nullptr;
	}
	SDL_RWops* rw = FileMap::getRWops(vfsPath);
	if (!rw)
	{
		if (_diag.shouldLog(CalypsoDiagnosticKey{ "faceOpen", resourceGeneration,
			std::hash<std::string>{}(vfsPath) }))
		{
			Log(LOG_ERROR) << "CalypsoHdTextRaster: getRWops failed for \"" << vfsPath << "\"";
		}
		return nullptr;
	}
	TTF_Font* face = TTF_OpenFontRW(rw, /*freesrc=*/1, physicalPixelHeight);
	if (!face)
	{
		if (_diag.shouldLog(CalypsoDiagnosticKey{ "faceOpen", resourceGeneration,
			std::hash<std::string>{}(vfsPath) }))
		{
			Log(LOG_ERROR) << "CalypsoHdTextRaster: TTF_OpenFontRW failed for \""
				<< vfsPath << "\" size=" << physicalPixelHeight << ": " << TTF_GetError();
		}
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
	// The face opened: re-arm the diagnostics gate for this (gen, path) tuple
	// so a LATER transient open failure may report again (F33-PARITY-001:
	// "no further copies until that tuple recovers").
	_diag.noteRecovered(CalypsoDiagnosticKey{ "faceOpen", resourceGeneration,
		std::hash<std::string>{}(vfsPath) });
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
	if (!calypsoFaceCoversText(face, key.text))
	{
		if (_diag.shouldLog(CalypsoDiagnosticKey{ "glyphCoverage",
			key.source.resourceGeneration, std::hash<std::string>{}(key.text) }))
		{
			Log(LOG_ERROR) << "CalypsoHdTextRaster: glyph coverage miss on \""
				<< key.source.canonicalVfsPath << "\" for \"" << key.text << "\"";
		}
		return nullptr;
	}
	// Coverage recovered: re-arm the gate so a future miss may report again.
	_diag.noteRecovered(CalypsoDiagnosticKey{ "glyphCoverage",
		key.source.resourceGeneration, std::hash<std::string>{}(key.text) });

	// RGBA packed as R in the high byte (matches CalypsoHdTextRasterKey's
	// colorRgba convention: 0xRRGGBBAA).
	SDL_Color c;
	c.r = static_cast<Uint8>((key.colorRgba >> 24) & 0xFFu);
	c.g = static_cast<Uint8>((key.colorRgba >> 16) & 0xFFu);
	c.b = static_cast<Uint8>((key.colorRgba >> 8) & 0xFFu);
	c.a = static_cast<Uint8>(key.colorRgba & 0xFFu);

	// Wrapped render (Fable #1 / Codex #5). key.wrapWidth == 0 wraps only on
	// embedded '\n' (single-line or author-broken text); key.wrapWidth > 0 lets
	// SDL_ttf's wrapped renderer handles CJK and no-space text correctly. When a
	// canonical line height is requested, the portable compositor below performs
	// equivalent metric-based wrapping and blits each line at the contract skip;
	// otherwise the stock wrapped path is retained. Tracked single-line text
	// composes per-glyph instead (SDL_ttf has no letter-spacing).
	SDL_Surface* surf = nullptr;
	if (key.lineHeightPx > 0 && key.wrapWidth > 0)
	{
		surf = renderWrappedWithLineHeight(face, key.text, key.wrapWidth,
			key.lineHeightPx, key.horizontalScalePermille, c);
	}
	else if (key.letterSpacingPx > 0 && key.wrapWidth == 0)
	{
		surf = calypsoRasterTracked(face, key.text, key.letterSpacingPx, c);
		if (!surf)
		{
			if (_diag.shouldLog(CalypsoDiagnosticKey{ "trackedRaster",
				key.source.resourceGeneration, std::hash<std::string>{}(key.text) }))
			{
				Log(LOG_ERROR) << "CalypsoHdTextRaster: tracked raster failed: " << TTF_GetError();
			}
		}
	}
	else
	{
		const Uint32 wrap = key.wrapWidth > 0 ? static_cast<Uint32>(key.wrapWidth) : 0u;
		surf = TTF_RenderUTF8_Blended_Wrapped(face, key.text.c_str(), c, wrap);
		if (!surf)
		{
			if (_diag.shouldLog(CalypsoDiagnosticKey{ "wrappedRaster",
				key.source.resourceGeneration, std::hash<std::string>{}(key.text) }))
			{
				Log(LOG_ERROR) << "CalypsoHdTextRaster: TTF_RenderUTF8_Blended_Wrapped failed: " << TTF_GetError();
			}
		}
	}
	if (!surf)
	{
		return nullptr;
	}

	// Success re-arms the diagnostics gate for THE class that actually
	// recovered. Re-arming unrelated classes would let a still-failing path
	// (e.g. a persistent wrapped raster while tracked text succeeds) emit a
	// fresh diagnostic per episode, violating F33-PARITY-001 ("no further
	// copies until THAT tuple recovers").
	if (key.letterSpacingPx > 0 && key.wrapWidth == 0)
	{
		_diag.noteRecovered(CalypsoDiagnosticKey{ "trackedRaster",
			key.source.resourceGeneration, std::hash<std::string>{}(key.text) });
	}
	else
	{
		_diag.noteRecovered(CalypsoDiagnosticKey{ "wrappedRaster",
			key.source.resourceGeneration, std::hash<std::string>{}(key.text) });
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
