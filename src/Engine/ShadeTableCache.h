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
 * Process-scoped cache of recoloured ShadeTable instances.
 *
 * Maps (base ShadeTable*, nbcShifted) → shared_ptr<ShadeTable> built via
 * ShadeTable::buildRecoloured.  Used by Surface::blitNShade for the
 * ColorReplace path so armour recolouring does not rebuild a 16 KB table
 * on every blit call.
 *
 * Eviction is intentionally simple: when the map reaches kMaxEntries, it is
 * fully cleared before the next insert.  This is *not* an LRU — the plan
 * called for LRU but vanilla TFTD uses well under 20 distinct (base, nbc)
 * combinations across a full session, so the cap is never reached in
 * practice and the linked-list bookkeeping LRU would impose is pure
 * overhead.  If a future mod repeatedly cycles >50 base tables, swap this
 * for an LRU; until then the flush-all policy is the right trade-off.
 *
 * Lifetime caveat: the cache is a function-local static in Surface.cpp and
 * lives for the whole process.  If the engine ever supports unloading and
 * reloading a Mod within one process, call clear() on Mod teardown to
 * invalidate stale (base ShadeTable*, …) keys before the underlying
 * shared_ptr<ShadeTable> goes away.
 *
 * R2.4 (Phase 7 remediation); doc updated by Q8.
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
