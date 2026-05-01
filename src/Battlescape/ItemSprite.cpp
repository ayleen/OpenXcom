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
#include "ItemSprite.h"
#include "../Engine/SurfaceSet.h"
#include "../Mod/Mod.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/BattleItem.h"
#include "../Savegame/SavedBattleGame.h"
#ifdef __EMSCRIPTEN__
#include "Map.h"  // Map::TileInstance
#include <vector>
#endif

namespace OpenXcom
{

namespace
{

void ensureIndexedSetPalette(const SurfaceSet *set, const Surface *paletteSource)
{
	if (!set || !paletteSource)
		return;
	const SDL_Color *colors = paletteSource->getEffectivePalette();
	if (!colors)
		return;
	for (size_t i = 0; i < set->getTotalFrames(); ++i)
	{
		const Surface *frame = set->getFrame((int)i);
		if (frame && !frame->isARGB())
		{
			const_cast<SurfaceSet *>(set)->setPalette(colors);
			return;
		}
	}
}

}

/**
 * Sets up a ItemSprite with the specified size and position.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
ItemSprite::ItemSprite(Surface* dest, const Mod* mod, const SavedBattleGame* save, int frame) :
	_itemSurface(const_cast<Mod*>(mod)->getSurfaceSet("FLOOROB.PCK")),
	_animationFrame(frame),
	_dest(dest),
	_save(save)
{

}

/**
 * Deletes the ItemSprite.
 */
ItemSprite::~ItemSprite()
{

}

/**
 * Draws a item, using the drawing rules of the item or unit if it's corpse.
 * This function is called by Map, for each item on the screen.
 */
void ItemSprite::draw(const BattleItem* item, int x, int y, int shade)
{
	ensureIndexedSetPalette(_itemSurface, _dest);
	const Surface* sprite = item->getFloorSprite(_itemSurface, _save, _animationFrame, shade);
	if (!sprite) return;
#ifdef __EMSCRIPTEN__
	if (_emitTarget && _emitSpec && _emitSpec->atlas)
	{
		// Use the rules' base FloorSprite PCK index — atlas builder maps
		// PCK index 1:1 to atlas slot. Script-modulated sprite lookups in
		// getFloorSprite are not reflected in the atlas, so ScriptFill paths
		// that rewrite the floor sprite index are visually frozen on the
		// base sprite for now.
		const int frameIdx = item->getRules()->getFloorSprite();
		if (frameIdx < 0) return;
		auto* vec = static_cast<std::vector<Map::TileInstance>*>(_emitTarget);
		const int col = frameIdx % _emitSpec->columns;
		const int row = frameIdx / _emitSpec->columns;
		const float uvW = (float)_emitSpec->tileWidth  / (float)_emitSpec->atlasW;
		const float uvH = (float)_emitSpec->tileHeight / (float)_emitSpec->atlasH;
		Map::TileInstance inst;
		inst.screenX        = (float)x;
		inst.screenY        = (float)y;
		inst.atlasU         = col * uvW;
		inst.atlasV         = row * uvH;
		inst.shade          = (float)shade;
		inst.animFrameCount = 1.0f;
		inst.alphaMask      = 1.0f;
		vec->push_back(inst);
		if (_emitZTarget)
			static_cast<std::vector<int>*>(_emitZTarget)->push_back(_emitZ);
		return;
	}
#endif
	ScriptWorkerBlit work;
	BattleItem::ScriptFill(&work, item, _save, BODYPART_ITEM_FLOOR, _animationFrame, shade);
	work.executeBlit(sprite, _dest, x, y, shade);
}

/**
 * Draws shadow of item.
 */
void ItemSprite::drawShadow(const BattleItem* item, int x, int y)
{
	ensureIndexedSetPalette(_itemSurface, _dest);
	const Surface* sprite = item->getFloorSprite(_itemSurface, _save, _animationFrame, 16);
	if (sprite)
	{
		sprite->blitNShade(_dest, x, y, 16);
	}
}

} //namespace OpenXcom
