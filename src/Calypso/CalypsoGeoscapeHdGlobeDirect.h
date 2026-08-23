#pragma once
/*
 * Phase 46.4 Stage 10.2.1 (Calypso) — GPU-direct sphere composite.
 *
 * When g_calypsoGlobeGpuDirect is set, the sphere shader renders straight
 * into the default framebuffer at physical backing-store resolution through
 * a PreComposite pass (Screen::flip fires it before the SDL surface
 * composite; transparent pixels of the CPU surface let the GPU content show
 * through — Screen.cpp:409-411). The canonical logical-readback path stays
 * byte-identical when the flag is off.
 *
 * Whole file is Emscripten-only; friendship with Globe grants member access
 * exactly like CalypsoGeoscapeHd.
 */
#include <memory>
#include <string>

#include "CalypsoViewportRuntime.h"
#include "Generated/CalypsoGeoscapeCommandShell.generated.h"

namespace OpenXcom
{

class Globe;
class Mod;
class Screen;
class GpuTexture;
struct Cord;

struct CalypsoGeoscapeHdGlobeDirect
{
	static void setGpuDirect(Globe *globe, bool on);
	static void computeSphereRes(const Globe *globe, int& w, int& h);
	static void drawPass(Globe *globe);
};

} // namespace OpenXcom
