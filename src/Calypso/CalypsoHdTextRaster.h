#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- CPU-side HD text rasteriser. Owns a bounded
 * sized-face factory (TTF_Font* opened at an arbitrary physical pixel
 * height, independent of any mod-registered TTFFont's fixed size) and a
 * bounded, byte-budgeted raster cache keyed by CalypsoHdTextRasterKey, so a
 * warm hit at an unchanged key costs zero rasterisation work. This batch is
 * CPU rasterisation only -- no GL/WebGL calls; dropContextTextures() is a
 * placeholder seam for the GPU-texture cache layered on top of this class in
 * a later HD.4 batch. Whole-file Emscripten guard per the Phase 36 placement
 * policy (see CalypsoViewportMailbox.cpp for the reference shape).
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdDiagnostics.h"
#include "CalypsoHdTextRasterKey.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>

namespace OpenXcom
{
namespace Calypso
{

/// CPU-side HD text rasteriser: owns a bounded sized-face factory (TTF_Font*
/// opened at a specific physical pixel height) and a bounded, byte-budgeted
/// raster cache (SDL_Surface* keyed by CalypsoHdTextRasterKey). A warm hit
/// does zero rasterisation work; a cold miss opens (or reuses) the sized
/// face and rasterises once via TTF_RenderUTF8_Blended.
class CalypsoHdTextRaster
{
public:
	explicit CalypsoHdTextRaster(std::size_t rasterByteBudget = 8u * 1024u * 1024u,
		std::size_t maxFaces = 16);
	~CalypsoHdTextRaster();

	CalypsoHdTextRaster(const CalypsoHdTextRaster&) = delete;
	CalypsoHdTextRaster& operator=(const CalypsoHdTextRaster&) = delete;

	/// Returns a cached, raster-owned straight-alpha ARGB surface for `key`
	/// (a single-line render of key.text at key.physicalPixelHeight in
	/// colour key.colorRgba). Caller must NOT free it -- this cache owns all
	/// returned surfaces. Returns nullptr if the face can't be opened, the
	/// string is empty, or rendering fails (nothing is cached in that case).
	SDL_Surface* rasterFor(const CalypsoHdTextRasterKey& key);

	/// Convenience: warms the cache for `key`. Returns rasterFor(key) != nullptr.
	bool warm(const CalypsoHdTextRasterKey& key);

	/// Placeholder for the future GPU-texture cache's context-loss hook
	/// (HD.4). This class holds only CPU SDL_Surface* rasters, which are not
	/// GL resources and are unaffected by a WebGL context loss, so this is a
	/// deliberate no-op here.
	void dropContextTextures();

	/// Frees every cached raster surface and closes every open face.
	void clear();

	std::size_t rasterBytes() const;
	std::size_t rasterCount() const;
	std::size_t faceCount() const;

private:
	struct FaceKey
	{
		std::string vfsPath;
		int physicalPixelHeight = 0;
		std::uint64_t resourceGeneration = 0; // bump => open a fresh face (Codex #9)

		bool operator==(const FaceKey& o) const
		{
			return physicalPixelHeight == o.physicalPixelHeight
			    && resourceGeneration == o.resourceGeneration
			    && vfsPath == o.vfsPath;
		}
	};
	struct FaceKeyHash
	{
		std::size_t operator()(const FaceKey& k) const
		{
			std::size_t h = std::hash<std::string>()(k.vfsPath);
			h ^= std::hash<int>()(k.physicalPixelHeight) + 0x9e3779b9u + (h << 6) + (h >> 2);
			h ^= std::hash<std::uint64_t>()(k.resourceGeneration) + 0x9e3779b9u + (h << 6) + (h >> 2);
			return h;
		}
	};

	/// Opens (or reuses) a TTF_Font* at `physicalPixelHeight` for the face at
	/// `vfsPath`. Returns nullptr for a non-positive size or a load failure.
	/// Evicts the least-recently-opened face (FIFO over `_faceOrder`) once
	/// `_maxFaces` is exceeded.
	TTF_Font* faceFor(const std::string& vfsPath, int physicalPixelHeight,
		std::uint64_t resourceGeneration = 0);

	std::size_t _maxFaces;
	std::unordered_map<FaceKey, TTF_Font*, FaceKeyHash> _faces;
	std::list<FaceKey> _faceOrder; // front = oldest

	std::uint64_t _nextHandle = 1;
	std::unordered_map<CalypsoHdTextRasterKey, SDL_Surface*> _rasters;
	std::unordered_map<CalypsoHdTextRasterKey, std::uint64_t> _rasterHandles;
	std::unordered_map<std::uint64_t, CalypsoHdTextRasterKey> _handleToKey;
	CalypsoLruByteBudget _lru;

	/// Bounded diagnostics gate (Phase 46.4-F33): one error per
	/// (failure class, font generation, text instance) until recovery.
	CalypsoDiagnosticGate _diag;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
