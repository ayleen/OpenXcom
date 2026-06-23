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

namespace OpenXcom
{

class Surface;

/**
 * Calypso: shared helpers for compositing rasterised TrueType (ARGB) text onto
 * an ARGB Surface. Extracted from the Battlescape HUD scaler so any HD UI path
 * (scaled menus, buttons, labels) can reuse the same bilinear fit-blit.
 */
namespace TTFUtil
{
	enum HAlign { H_LEFT, H_CENTER, H_RIGHT };
	enum VAlign { V_TOP, V_MIDDLE, V_BOTTOM };

	/**
	 * Bilinear, downscale-only fit-blit of a 32-bit ARGB TTF surface into a
	 * 32-bit ARGB Surface. The destination is cleared first; the rendered glyph
	 * block is scaled to fit (never upscaled), shrunk by @a fillFrac, and placed
	 * by @a halign / @a valign. No-op if either surface is not 32 bpp.
	 */
	void blitFit(SDL_Surface* ttf, Surface* dest,
	             HAlign halign = H_LEFT, VAlign valign = V_MIDDLE, float fillFrac = 1.0f);

	/**
	 * Bilinear stretch a 32-bit ARGB surface to completely fill a 32-bit ARGB
	 * Surface (e.g. a scaled menu background upscaled from its 320×200 source).
	 * Writes every destination pixel; no-op if either surface is not 32 bpp.
	 */
	void blitStretch(SDL_Surface* src, Surface* dest);
}

}
