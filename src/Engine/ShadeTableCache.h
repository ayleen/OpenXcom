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
#include <memory>
#include <unordered_map>
#include "ShadeTable.h"

namespace OpenXcom
{

/**
 * Session-scoped cache of recoloured ShadeTable instances.
 *
 * Maps (base ShadeTable*, nbcShifted) → shared_ptr<ShadeTable> built via
 * ShadeTable::buildRecoloured.  Used by Surface::blitNShade for the
 * ColorReplace path so armour recolouring does not rebuild a 16 KB table
 * on every blit call.
 *
 * Eviction: when the map reaches kMaxEntries, it is cleared in full before
 * the next insert.  TFTD uses fewer than 20 distinct colour combinations,
 * so the cap is effectively never reached in practice.
 *
 * R2.4 (Phase 7 remediation).
 */
class ShadeTableCache
{
public:
	static constexpr size_t kMaxEntries = 50;

	/// Returns a cached or newly-built recoloured ShadeTable.
	/// base and nbcShifted form the cache key; pal is used only on a miss.
	/// nbcShifted must already be left-shifted into the high nibble,
	/// i.e. (newBaseColor - 1) << 4, matching blitNShade conventions.
	const ShadeTable *getOrBuild(const ShadeTable *base,
	                              const SDL_Color *pal,
	                              Uint8 nbcShifted);

	void clear();

private:
	struct Key
	{
		const ShadeTable *base;
		Uint8 nbc;
		bool operator==(const Key &o) const noexcept
		{
			return base == o.base && nbc == o.nbc;
		}
	};
	struct KeyHash
	{
		std::size_t operator()(const Key &k) const noexcept
		{
			// k.nbc is in the high nibble of a Uint8 (0x10..0xF0); rotating by 24
			// keeps it within the 32-bit size_t on Emscripten and away from the
			// low pointer bits that std::hash<void*> emphasises.
			return std::hash<const void *>()(k.base) ^ ((std::size_t)k.nbc << 24);
		}
	};
	std::unordered_map<Key, std::shared_ptr<ShadeTable>, KeyHash> _map;
};

} // namespace OpenXcom
