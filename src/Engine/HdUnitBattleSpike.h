#pragma once

#ifdef __EMSCRIPTEN__
#include <string>

namespace OpenXcom
{
class Game;
class Surface;

class HdUnitBattleSpike
{
public:
	static bool activate(Game *game, const char *assetPath, const char *metricsPath);
	static int select(Game *game, int unitId, int activeHand, int aiming, bool center);
	static bool findOccluderTarget(Game *game, int unitId, const char *outJson);
	static bool findWalkTarget(Game *game, int unitId, const char *outJson);
	static bool inspectInput(Game *game, const char *outJson);
	static bool checkpoint(const char *label);
	static bool finish(Game *game, const char *outPng);
	static bool active();
	static void beginRenderFrame();
	static void recordDrawStart(int unitId);
	static void recordEmit(int unitId, int direction, int sequence,
	                       const char *kind, int frame, float priority, bool overlay);
	static void recordGlError(unsigned error);
	static void recordForegroundOccluder(int x, int y, int z, int priority,
	                                     float depthPriority, int shade, const Surface *source,
	                                     bool verifiedVanillaR8,
	                                     float screenX, float screenY, float w, float h);
	static void recordOverlayGeometry(int unitId, int frame, float depthPriority,
	                                  float screenX, float screenY, float w, float h);
	/// Read one exact opaque O_OBJECT/RGBA intersection from the currently-bound
	/// WebGL render target. Called immediately after the disposable overlay pass.
	static void captureForegroundPixelProof(int framebufferW, int framebufferH,
	                                        int logicalW, int logicalH,
	                                        bool floatingPointTarget);
};
}
#endif
