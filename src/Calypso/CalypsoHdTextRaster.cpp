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

#include <vector>

namespace OpenXcom
{
namespace Calypso
{

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
	// SDL_ttf break the text at that physical pixel width -- which breaks CJK and
	// no-space text correctly, unlike an ASCII space pre-wrap. The single-line
	// TTF_RenderUTF8_Blended would render '\n' as a notdef glyph, so always use
	// the _Wrapped variant. Tracked single-line text composes per-glyph instead
	// (SDL_ttf has no letter-spacing); the two paths share the coverage gate.
	SDL_Surface* surf = nullptr;
	if (key.letterSpacingPx > 0 && key.wrapWidth == 0)
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
