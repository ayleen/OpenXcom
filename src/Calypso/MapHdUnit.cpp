/* Calypso-owned typed Map -> UnitSprite HD-unit emit adapter. */
#ifdef __EMSCRIPTEN__

#include "../Battlescape/Map.h"

namespace OpenXcom
{

HdUnitEmitTargets Map::makeHdUnitEmitTargets(size_t bodyIdx, bool haveItem,
	                                         size_t itemIdx, int z, int y, int x,
	                                         int renderWidth, int renderHeight)
{
	HdUnitEmitTargets targets;
	UnitAtlasGroup& body = _unitAtlasGroups[bodyIdx];
	targets.bodyInstances = &body.instances;
	targets.itemInstances = haveItem ? &_unitAtlasGroups[itemIdx].instances : nullptr;
	targets.bodySpec = body.spec;
	targets.itemSpec = haveItem ? _unitAtlasGroups[itemIdx].spec : nullptr;
	targets.emitZ = z;
	targets.emitY = y;
	targets.emitX = x;
	targets.renderWidth = renderWidth;
	targets.renderHeight = renderHeight;
	targets.zTargetBody = &body.zLevels;
	targets.zTargetItem = haveItem ? &_unitAtlasGroups[itemIdx].zLevels : nullptr;
	targets.yTargetBody = &body.yLevels;
	targets.yTargetItem = haveItem ? &_unitAtlasGroups[itemIdx].yLevels : nullptr;
	targets.g0OverlayTarget = &body.g0OverlayInstances;
	targets.rgbaOverlayBodyPages = &body.rgbaOverlayInstances;
	targets.rgbaOverlayItemPages = haveItem
		? &_unitAtlasGroups[itemIdx].rgbaOverlayInstances : nullptr;
	return targets;
}

} // namespace OpenXcom

#endif
