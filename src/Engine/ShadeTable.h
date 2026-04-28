#pragma once
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
#include <SDL.h>
#include <array>

namespace OpenXcom
{

/// 16 ARGB output values for one source palette index, one per shade level.
using ShadeColumn = std::array<Uint32, 16>;

/**
 * Per-asset shade table.
 *
 * Maps each source palette index (0–255) to 16 ARGB values, one for each
 * shade level 0 (full brightness) through 15 (black).  The math mirrors
 * helper::StandardShade in ShaderDraw.h:
 *
 *   newIdx = idx + shade
 *   if (newIdx ^ idx) & 0xF0   // colour-group boundary crossed
 *       result = palette[0x0F]  // black
 *   else
 *       result = palette[newIdx]
 *
 * Index 0 is transparent in palette mode; all shades for idx==0 return 0.
 *
 * A second variant, buildRecoloured(), pre-computes the ColorReplace path:
 * the source colour group is replaced with newBaseColor before shading.
 *
 * Phase 7 note: this class is cross-platform (no __EMSCRIPTEN__ guard).
 * Guards on the 6a/6b scaffolding are audited and removed in block 7.K.
 */
class ShadeTable
{
public:
	ShadeTable() = default;

	/// Build from palette using StandardShade semantics.
	void buildFromPalette(const SDL_Color *pal);

	/// Build a recoloured variant using ColorReplace semantics.
	/// newBaseColor must be pre-shifted into the high nibble
	/// (i.e. (baseColor - 1) << 4, matching blitNShade callers).
	void buildRecoloured(const SDL_Color *pal, Uint8 newBaseColor);

	/// O(1) lookup.  idx in [0, 255], shade 16+ is forced black.
	inline Uint32 get(Uint8 idx, int shade) const
	{
		if (idx == 0)
		{
			return 0;
		}
		if (shade >= 16)
		{
			return _black;
		}
		if (shade <= 0)
		{
			return _columns[idx][0];
		}
		return _columns[idx][shade];
	}

	bool empty() const { return _empty; }

private:
	std::array<ShadeColumn, 256> _columns{};
	Uint32 _black = 0xFF000000u;
	bool _empty = true;
};

} // namespace OpenXcom
