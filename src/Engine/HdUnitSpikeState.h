#pragma once
/*
 * Disposable Phase-42 G0 harness for the synthetic HD-unit atlas.
 *
 * This is deliberately not a production State. It is compiled only for
 * Emscripten and registers a short-lived GPU pass, following GpuSmokeState.
 */
#ifdef __EMSCRIPTEN__

#include <string>

namespace OpenXcom
{
class Screen;

struct HdUnitSpikeState
{
	/* Returns true only when the asset decoded, both real atlas shaders compiled,
	 * texture upload/reload succeeded, and the diagnostic pass was registered. */
	static bool activate(Screen *screen, const std::string &assetPath,
	                     const std::string &outPath,
	                     const std::string &metricsPath);
};
} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
