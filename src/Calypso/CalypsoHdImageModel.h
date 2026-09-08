#pragma once
// F01 shared RGBA image item (T05): pure descriptor, validator, UV/clip math,
// and cache-key helpers. Dependency-free: no SDL, GL, engine, or allocation.
// Not #ifdef-guarded, matching the Calypso pure-helper convention.

#include <cstdint>
#include <functional>
#include <string>

namespace OpenXcom
{
namespace Calypso
{

inline constexpr int kCalypsoHdImageMaxExtent = 2048;
inline constexpr long long kCalypsoHdImageMaxBytes = 64LL * 1024LL * 1024LL;

struct CalypsoHdImageDescriptor
{
	std::string source;
	std::uint32_t generation = 0;
	int srcX = 0;
	int srcY = 0;
	int srcW = 0;
	int srcH = 0;
	bool hasClip = false;
	int clipX = 0;
	int clipY = 0;
	int clipW = 0;
	int clipH = 0;
	// Center-crop UVs to the physical destination; the decoded texture is reused.
	bool cover = false;
};

struct CalypsoHdImageValidation
{
	bool ok = false;
	const char *reason = "";
};

inline CalypsoHdImageValidation calypsoHdImageValidate(
	const CalypsoHdImageDescriptor &desc, int decodedW, int decodedH)
{
	if (desc.source.empty())
	{
		return {false, "empty image source"};
	}
	if (decodedW <= 0 || decodedH <= 0)
	{
		return {false, "non-positive decoded extent"};
	}
	if (decodedW > kCalypsoHdImageMaxExtent || decodedH > kCalypsoHdImageMaxExtent)
	{
		return {false, "decoded extent exceeds the image ceiling"};
	}
	const long long bytes = static_cast<long long>(decodedW) * decodedH * 4LL;
	if (bytes > kCalypsoHdImageMaxBytes)
	{
		return {false, "decoded bytes exceed the image ceiling"};
	}
	if (desc.srcW == 0 && desc.srcH == 0)
	{
		if (desc.srcX != 0 || desc.srcY != 0)
		{
			return {false, "full-image select must use a zero origin"};
		}
	}
	else
	{
		if (desc.srcW <= 0 || desc.srcH <= 0)
		{
			return {false, "non-positive UV source extent"};
		}
		if (desc.srcX < 0 || desc.srcY < 0)
		{
			return {false, "negative UV source origin"};
		}
		if (desc.srcX + desc.srcW > decodedW || desc.srcY + desc.srcH > decodedH)
		{
			return {false, "UV source rect escapes the decoded image"};
		}
	}
	if (desc.hasClip && (desc.clipW <= 0 || desc.clipH <= 0))
	{
		return {false, "non-positive clip extent"};
	}
	return {true, ""};
}

struct CalypsoHdImageUvRect
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

inline CalypsoHdImageUvRect calypsoHdImageUv(
	const CalypsoHdImageDescriptor &desc, int decodedW, int decodedH,
	int destW, int destH)
{
	CalypsoHdImageUvRect uv = desc.srcW == 0 && desc.srcH == 0
		? CalypsoHdImageUvRect{0, 0, decodedW, decodedH}
		: CalypsoHdImageUvRect{desc.srcX, desc.srcY, desc.srcW, desc.srcH};
	if (!desc.cover) return uv;
	if (destW <= 0 || destH <= 0 || uv.w <= 0 || uv.h <= 0) return {};
	if (static_cast<std::int64_t>(uv.w) * destH > static_cast<std::int64_t>(uv.h) * destW)
	{
		int width = static_cast<int>((static_cast<std::int64_t>(uv.h) * destW + destH / 2) / destH);
		if (width < 1) width = 1;
		uv.x += (uv.w - width) / 2;
		uv.w = width;
	}
	else
	{
		int height = static_cast<int>((static_cast<std::int64_t>(uv.w) * destH + destW / 2) / destW);
		if (height < 1) height = 1;
		uv.y += (uv.h - height) / 2;
		uv.h = height;
	}
	return uv;
}

struct CalypsoHdImageClipResult
{
	bool visible = false;
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

inline CalypsoHdImageClipResult calypsoHdImageClip(
	const CalypsoHdImageDescriptor &desc, int destX, int destY, int destW, int destH)
{
	if (!desc.hasClip)
	{
		return {destW > 0 && destH > 0, destX, destY, destW, destH};
	}
	const long long ix0 = desc.clipX > destX ? desc.clipX : destX;
	const long long iy0 = desc.clipY > destY ? desc.clipY : destY;
	const long long clipX1 = static_cast<long long>(desc.clipX) + desc.clipW;
	const long long clipY1 = static_cast<long long>(desc.clipY) + desc.clipH;
	const long long destX1 = static_cast<long long>(destX) + destW;
	const long long destY1 = static_cast<long long>(destY) + destH;
	const long long ix1 = clipX1 < destX1 ? clipX1 : destX1;
	const long long iy1 = clipY1 < destY1 ? clipY1 : destY1;
	if (ix1 <= ix0 || iy1 <= iy0)
	{
		return {false, 0, 0, 0, 0};
	}
	return {true, static_cast<int>(ix0), static_cast<int>(iy0),
		static_cast<int>(ix1 - ix0), static_cast<int>(iy1 - iy0)};
}

struct CalypsoHdImageCacheKey
{
	std::string source;
	std::uint32_t generation = 0;
	int srcX = 0;
	int srcY = 0;
	int srcW = 0;
	int srcH = 0;

	bool operator==(const CalypsoHdImageCacheKey &other) const
	{
		return source == other.source && generation == other.generation
			&& srcX == other.srcX && srcY == other.srcY
			&& srcW == other.srcW && srcH == other.srcH;
	}
};

struct CalypsoHdImageCacheKeyHash
{
	std::size_t operator()(const CalypsoHdImageCacheKey &key) const noexcept
	{
		std::size_t h = std::hash<std::string>{}(key.source);
		const std::size_t mix = 0x9e3779b9u;
		h ^= std::hash<std::uint32_t>{}(key.generation) + mix + (h << 6) + (h >> 2);
		h ^= std::hash<int>{}(key.srcX) + mix + (h << 6) + (h >> 2);
		h ^= std::hash<int>{}(key.srcY) + mix + (h << 6) + (h >> 2);
		h ^= std::hash<int>{}(key.srcW) + mix + (h << 6) + (h >> 2);
		h ^= std::hash<int>{}(key.srcH) + mix + (h << 6) + (h >> 2);
		return h;
	}
};

inline CalypsoHdImageCacheKey calypsoHdImageCacheKey(
	const CalypsoHdImageDescriptor &desc)
{
	return {desc.source, desc.generation, desc.srcX, desc.srcY, desc.srcW, desc.srcH};
}

struct CalypsoHdImageTextureKey
{
	CalypsoHdImageCacheKey image;
	std::uint64_t contextGeneration = 0;

	bool operator==(const CalypsoHdImageTextureKey &other) const
	{
		return image == other.image && contextGeneration == other.contextGeneration;
	}
};

struct CalypsoHdImageTextureKeyHash
{
	std::size_t operator()(const CalypsoHdImageTextureKey &key) const noexcept
	{
		std::size_t h = CalypsoHdImageCacheKeyHash{}(key.image);
		h ^= std::hash<std::uint64_t>{}(key.contextGeneration) + 0x9e3779b9u + (h << 6) + (h >> 2);
		return h;
	}
};

} // namespace Calypso
} // namespace OpenXcom
