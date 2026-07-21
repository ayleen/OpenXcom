#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- portable HD text raster/texture cache keys and
 * the byte-budgeted LRU eviction bookkeeper.
 *
 * The pixel-affecting identity of an HD text raster is captured here so the
 * Emscripten rasteriser (CalypsoHdTextRaster) can split its CPU-raster cache
 * from its GPU-texture cache and prove that an unchanged frame at unchanged
 * metrics costs zero rasterisations and zero uploads.
 *
 * Two keys:
 *   - CalypsoHdTextRasterKey: EVERY input that changes the produced pixels --
 *     the immutable font source descriptor (+ its resource generation), the
 *     physical pixel height, the resolved text, the authoritative processed
 *     line-break signature, colour, style, and direction. The absolute
 *     destination X/Y is deliberately NOT part of it (that is a draw-placement
 *     key, not a raster key).
 *   - CalypsoHdTextTextureKey: the raster key PLUS the WebGL context
 *     generation, so a context loss drops stale GPU textures while valid CPU
 *     rasters may survive.
 *
 * The LRU bookkeeper (CalypsoLruByteBudget) is value-type agnostic: it tracks
 * key insertion/access order and per-entry byte cost and reports which keys to
 * evict when a byte budget is exceeded, so the real caches stay bounded and
 * byte-accounted. All of this is dependency-free (std containers only) and
 * natively unit tested.
 */
#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>

namespace OpenXcom
{
namespace Calypso
{

/// Immutable identity of a physical TTF source. `resourceGeneration` bumps when
/// the underlying font resource is reloaded (mod reload / VFS change), which
/// invalidates every raster keyed on the old generation.
struct CalypsoTtfSourceDescriptor
{
	std::string canonicalVfsPath;
	int faceIndex = 0;
	int logicalDesignSize = 0;
	std::uint64_t resourceGeneration = 0;

	bool operator==(const CalypsoTtfSourceDescriptor& o) const
	{
		return canonicalVfsPath == o.canonicalVfsPath
		    && faceIndex == o.faceIndex
		    && logicalDesignSize == o.logicalDesignSize
		    && resourceGeneration == o.resourceGeneration;
	}
};

/// Text direction. Kept explicit so LTR/RTL rasters never alias.
enum class CalypsoTextDirection { LTR, RTL };

/// The complete pixel-affecting key for a CPU raster.
struct CalypsoHdTextRasterKey
{
	CalypsoTtfSourceDescriptor source;
	int physicalPixelHeight = 0;
	std::string text;                      // resolved UTF-8
	std::uint64_t breakSignature = 0;      // hash of the approved processed line breaks
	std::uint32_t colorRgba = 0;           // packed RGBA
	std::uint32_t styleFlags = 0;          // bold/underline/etc. bitfield
	CalypsoTextDirection direction = CalypsoTextDirection::LTR;

	bool operator==(const CalypsoHdTextRasterKey& o) const
	{
		return source == o.source
		    && physicalPixelHeight == o.physicalPixelHeight
		    && text == o.text
		    && breakSignature == o.breakSignature
		    && colorRgba == o.colorRgba
		    && styleFlags == o.styleFlags
		    && direction == o.direction;
	}
};

/// The GPU-texture key: raster identity plus the WebGL context generation.
struct CalypsoHdTextTextureKey
{
	CalypsoHdTextRasterKey raster;
	std::uint64_t contextGeneration = 0;

	bool operator==(const CalypsoHdTextTextureKey& o) const
	{
		return contextGeneration == o.contextGeneration && raster == o.raster;
	}
};

/// Stable 64-bit hash of the approved processed line breaks. Two snapshots
/// that share the same text but broke it differently (different wrap width,
/// different processed breaks) produce different signatures and therefore
/// cache separately. FNV-1a over the break offsets.
inline std::uint64_t calypsoHashLineBreaks(const std::vector<int>& breaks)
{
	std::uint64_t h = 1469598103934665603ull; // FNV offset basis
	for (int b : breaks)
	{
		std::uint32_t u = static_cast<std::uint32_t>(b);
		for (int i = 0; i < 4; ++i)
		{
			h ^= static_cast<std::uint8_t>(u & 0xFF);
			h *= 1099511628211ull; // FNV prime
			u >>= 8;
		}
	}
	return h;
}

} // namespace Calypso
} // namespace OpenXcom

// --- Hash specialisations (outside the OpenXcom namespace) -----------------

namespace std
{
template <>
struct hash<OpenXcom::Calypso::CalypsoHdTextRasterKey>
{
	std::size_t operator()(const OpenXcom::Calypso::CalypsoHdTextRasterKey& k) const
	{
		std::size_t h = std::hash<std::string>()(k.source.canonicalVfsPath);
		auto mix = [&h](std::uint64_t v) {
			h ^= std::hash<std::uint64_t>()(v) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
		};
		mix(static_cast<std::uint64_t>(k.source.faceIndex));
		mix(static_cast<std::uint64_t>(k.source.logicalDesignSize));
		mix(k.source.resourceGeneration);
		mix(static_cast<std::uint64_t>(k.physicalPixelHeight));
		h ^= std::hash<std::string>()(k.text) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
		mix(k.breakSignature);
		mix(k.colorRgba);
		mix(k.styleFlags);
		mix(static_cast<std::uint64_t>(k.direction == OpenXcom::Calypso::CalypsoTextDirection::RTL ? 1 : 0));
		return h;
	}
};

template <>
struct hash<OpenXcom::Calypso::CalypsoHdTextTextureKey>
{
	std::size_t operator()(const OpenXcom::Calypso::CalypsoHdTextTextureKey& k) const
	{
		std::size_t h = std::hash<OpenXcom::Calypso::CalypsoHdTextRasterKey>()(k.raster);
		h ^= std::hash<std::uint64_t>()(k.contextGeneration) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
		return h;
	}
};
} // namespace std

namespace OpenXcom
{
namespace Calypso
{

/// Value-type-agnostic byte-budgeted LRU bookkeeper. The real caches store the
/// actual raster/texture next to the key; this class owns only the eviction
/// policy: access recency and per-key byte cost. touch() records an
/// insertion/access and returns the keys that must be evicted to stay within
/// the byte budget (least-recently-used first, never evicting the just-touched
/// key). Keys are std::uint64_t handles the caller assigns.
class CalypsoLruByteBudget
{
public:
	explicit CalypsoLruByteBudget(std::size_t byteBudget = 0) : _budget(byteBudget) {}

	void setBudget(std::size_t byteBudget) { _budget = byteBudget; }
	std::size_t budget() const { return _budget; }
	std::size_t bytes() const { return _bytes; }
	std::size_t size() const { return _map.size(); }
	bool contains(std::uint64_t key) const { return _map.find(key) != _map.end(); }

	/// Record an insertion or access of `key` costing `byteCost` bytes, then
	/// evict least-recently-used entries until total bytes <= budget (or only
	/// the just-touched entry remains). Returns evicted keys in eviction order.
	/// A single entry larger than the budget is kept (cannot satisfy the
	/// budget by evicting it) but reported via overBudget().
	std::vector<std::uint64_t> touch(std::uint64_t key, std::size_t byteCost)
	{
		auto it = _map.find(key);
		if (it != _map.end())
		{
			_bytes -= it->second.bytes;
			it->second.bytes = byteCost;
			_bytes += byteCost;
			_order.erase(it->second.pos);
			_order.push_front(key);
			it->second.pos = _order.begin();
		}
		else
		{
			_order.push_front(key);
			Entry e;
			e.bytes = byteCost;
			e.pos = _order.begin();
			_map.emplace(key, e);
			_bytes += byteCost;
		}

		std::vector<std::uint64_t> evicted;
		while (_bytes > _budget && _order.size() > 1)
		{
			std::uint64_t lru = _order.back();
			if (lru == key) break; // never evict the just-touched key
			auto lit = _map.find(lru);
			_bytes -= lit->second.bytes;
			_map.erase(lit);
			_order.pop_back();
			evicted.push_back(lru);
		}
		return evicted;
	}

	/// Explicitly drop a key (e.g. context-generation invalidation).
	void erase(std::uint64_t key)
	{
		auto it = _map.find(key);
		if (it == _map.end()) return;
		_bytes -= it->second.bytes;
		_order.erase(it->second.pos);
		_map.erase(it);
	}

	void clear()
	{
		_map.clear();
		_order.clear();
		_bytes = 0;
	}

	/// True when the single tracked set cannot fit the budget even after
	/// evicting everything but the most recent entry.
	bool overBudget() const { return _bytes > _budget; }

private:
	struct Entry
	{
		std::size_t bytes = 0;
		std::list<std::uint64_t>::iterator pos;
	};
	std::size_t _budget = 0;
	std::size_t _bytes = 0;
	std::list<std::uint64_t> _order; // front = most recently used
	std::unordered_map<std::uint64_t, Entry> _map;
};

} // namespace Calypso
} // namespace OpenXcom
