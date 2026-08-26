#pragma once
#ifdef __EMSCRIPTEN__
namespace OpenXcom
{
class Globe;
struct Cord;
namespace Calypso
{
bool calypsoGlobeInitSphereGPU(Globe& globe);
Cord calypsoGlobeSunDirectionWorld(const Globe& globe);
void calypsoGlobeDrawHDStarfield(Globe& globe);
void calypsoGlobeDrawSphereGPU(Globe& globe);
void calypsoGlobeDrawHoverCircles(Globe& globe);
void calypsoGlobeHoverOverlayFrame(Globe& globe);
} // namespace Calypso
} // namespace OpenXcom
#endif
