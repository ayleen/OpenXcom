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
#include "ShadeTable.h"

namespace OpenXcom
{

// Mirrors helper::ColorGroup / helper::ColorShade from ShaderDraw.h.
// Kept local so ShadeTable.h does not pull in the full ShaderDraw template chain.
static constexpr Uint8 kColorGroup = 0xF0;
static constexpr Uint8 kColorShade = 0x0F;

static inline Uint32 toArgb(const SDL_Color &c)
{
	return ((Uint32)0xFF << 24)
	     | ((Uint32)c.r  << 16)
	     | ((Uint32)c.g  <<  8)
	     |  (Uint32)c.b;
}

/**
 * Fills _columns using StandardShade semantics:
 *   newIdx = idx + shade
 *   if colour-group boundary crossed → palette[kColorShade] (black)
 *   else → palette[newIdx]
 * Index 0 (transparent) maps to ARGB 0 at every shade level.
 */
void ShadeTable::buildFromPalette(const SDL_Color *pal)
{
	if (!pal) { _empty = true; return; }
	_empty = false;
	for (int idx = 0; idx < 256; ++idx)
	{
		for (int shade = 0; shade < 16; ++shade)
		{
			const Uint8 newShade = (Uint8)(idx + shade);
			const Uint8 finalIdx = ((newShade ^ (Uint8)idx) & kColorGroup)
			    ? kColorShade
			    : newShade;
			_columns[idx][shade] = toArgb(pal[finalIdx]);
		}
	}
	// Index 0 is transparent in palette mode.
	_columns[0].fill(0);
}

/**
 * Fills _columns using ColorReplace semantics:
 *   strip the colour group from idx, replace with newBaseColor, then shade.
 *   newBaseColor must already be left-shifted into the high nibble.
 * Index 0 maps to ARGB 0 at every shade level.
 */
void ShadeTable::buildRecoloured(const SDL_Color *pal, Uint8 newBaseColor)
{
	if (!pal) { _empty = true; return; }
	_empty = false;
	_columns[0].fill(0);
	for (int idx = 1; idx < 256; ++idx)
	{
		const Uint8 shadeLow = (Uint8)idx & kColorShade;
		for (int shade = 0; shade < 16; ++shade)
		{
			const Uint8 newShade = shadeLow + (Uint8)shade;
			// newBaseColor already in high-nibble position.
			const Uint8 finalIdx = (newShade & kColorGroup)
			    ? kColorShade
			    : (newBaseColor | newShade);
			_columns[idx][shade] = toArgb(pal[finalIdx]);
		}
	}
}

} // namespace OpenXcom
