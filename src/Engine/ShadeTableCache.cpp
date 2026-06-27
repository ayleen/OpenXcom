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
#include "ShadeTableCache.h"

namespace OpenXcom
{

const ShadeTable *ShadeTableCache::getOrBuild(const ShadeTable *base,
                                               const SDL_Color *pal,
                                               Uint8 nbcShifted)
{
	if (!base || !pal) return nullptr;

	const Key k { base, nbcShifted };
	const auto it = _map.find(k);
	if (it != _map.end())
		return it->second.get();

	if (_map.size() >= kMaxEntries)
		_map.clear();

	auto tbl = std::make_shared<ShadeTable>();
	tbl->buildRecoloured(pal, nbcShifted);
	return _map.emplace(k, std::move(tbl)).first->second.get();
}

void ShadeTableCache::clear()
{
	_map.clear();
}

} // namespace OpenXcom

#ifdef __EMSCRIPTEN__
#include "GpuTexture.h"
#include <vector>

namespace OpenXcom
{

std::unique_ptr<GpuTexture> ShadeTableCache::uploadGPU(const ShadeTable* table) const
{
	if (!table || table->empty()) return nullptr;

	// 16 columns (shade 0..15) x 256 rows (palette index 0..255), RGBA8
	constexpr int W = 16, H = 256;
	std::vector<uint8_t> pixels(W * H * 4u);
	for (int palIdx = 0; palIdx < H; ++palIdx)
	{
		for (int shade = 0; shade < W; ++shade)
		{
			Uint32 argb = table->get(static_cast<Uint8>(palIdx), shade);
			uint8_t a = (argb >> 24) & 0xFFu;
			uint8_t r = (argb >> 16) & 0xFFu;
			uint8_t g = (argb >>  8) & 0xFFu;
			uint8_t b =  argb        & 0xFFu;
			size_t i = (static_cast<size_t>(palIdx) * W + shade) * 4u;
			pixels[i+0] = r;
			pixels[i+1] = g;
			pixels[i+2] = b;
			pixels[i+3] = a;
		}
	}

	// GL_NEAREST: shade levels are discrete; interpolation would give wrong colors.
	auto tex = std::make_unique<GpuTexture>(false, GpuTexture::Wrap::ClampToEdge,
	                                        GpuTexture::Filter::Nearest);
	tex->uploadRGBA(pixels.data(), W, H);
	return tex;
}

} // namespace OpenXcom
#endif
