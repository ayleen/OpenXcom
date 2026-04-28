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
