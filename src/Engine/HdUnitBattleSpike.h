#pragma once

#ifdef __EMSCRIPTEN__
#include <string>

namespace OpenXcom
{
class Game;

class HdUnitBattleSpike
{
public:
	static bool activate(Game *game, const char *assetPath, const char *metricsPath);
	static int select(Game *game, int unitId, int activeHand, int aiming, bool center);
	static bool findOccluderTarget(Game *game, int unitId, const char *outJson);
	static bool checkpoint(const char *label);
	static bool finish(Game *game, const char *outPng);
	static bool active();
	static void beginRenderFrame();
	static void recordDrawStart(int unitId);
	static void recordEmit(int unitId, int direction, int sequence,
	                       const char *kind, int frame, float priority, bool overlay);
	static void recordGlError(unsigned error);
	static void recordForegroundOccluder(int x, int y, int z, int priority,
	                                     float screenX, float screenY, float w, float h);
	static void recordOverlayGeometry(int unitId, float screenX, float screenY, float w, float h);
};
}
#endif
