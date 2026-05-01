#pragma once
/*
 * Copyright 2010-2015 OpenXcom Developers.
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
#include "../Engine/Surface.h"
#include "../Mod/Mod.h"

namespace OpenXcom
{

class BattleUnit;
class BattleItem;
class SavedBattleGame;
class SurfaceSet;
class Mod;

/**
 * A class that renders a specific unit, given its render rules
 * combining the right frames from the surfaceset.
 */
class ItemSprite
{
private:
	const SurfaceSet *_itemSurface;
	int _animationFrame;
	Surface *_dest;
	const SavedBattleGame *_save;
#ifdef __EMSCRIPTEN__
	void* _emitTarget = nullptr;       // std::vector<Map::TileInstance>* — body emits
	void* _emitZTarget = nullptr;      // std::vector<int>*               — Z per emit
	const Mod::UnitAtlasSpec* _emitSpec = nullptr;
	int   _emitZ = 0;
#endif

public:
	/// Creates a new ItemSprite at the specified position and size.
	ItemSprite(Surface* dest, const Mod* mod, const SavedBattleGame *_save, int frame);
	/// Cleans up the ItemSprite.
	~ItemSprite();
	/// Draws the item.
	void draw(const BattleItem* item, int x, int y, int shade);
	/// Draws the item shadow.
	void drawShadow(const BattleItem* item, int x, int y);
#ifdef __EMSCRIPTEN__
	/// Emit-mode: redirect draw() into a TileInstance vector instead of CPU blit.
	void setEmitMode(void* target, const Mod::UnitAtlasSpec* spec, int emitZ, void* zTarget)
	{
		_emitTarget = target;
		_emitSpec   = spec;
		_emitZ      = emitZ;
		_emitZTarget = zTarget;
	}
	void clearEmitMode()
	{
		_emitTarget = nullptr;
		_emitSpec   = nullptr;
		_emitZ      = 0;
		_emitZTarget = nullptr;
	}
#endif
};

} //namespace OpenXcom
