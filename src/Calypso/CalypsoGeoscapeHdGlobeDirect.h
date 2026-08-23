#pragma once

#ifdef __EMSCRIPTEN__
/* Included from Geoscape/Globe.cpp AFTER Globe.h and after the local
 * GlobeSphereGlSave helper definition (drawPass body lives in the .cpp). */
#include <cmath>
#include <memory>
#include <string>

#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Screen.h"
#include "../Mod/Mod.h"

namespace OpenXcom
{

struct CalypsoGeoscapeHdGlobeDirect
{

	static void computeSphereRes(const Globe* globe, int& w, int& h)
	{
		w = globe->getWidth(); h = globe->getHeight();
		if (globe->_gpuDirectMode && globe->_directScreen != nullptr)
		{
			w = std::max(1, (int)std::lround(w * globe->_directScreen->getXScale()));
			h = std::max(1, (int)std::lround(h * globe->_directScreen->getYScale()));
		}
	}

	static void setGpuDirect(Globe* globe, bool on)
	{
		if (on == globe->_gpuDirectMode) return;
		globe->_gpuDirectMode = on;
		globe->_directScreen = on ? globe->_game->getScreen() : nullptr;
		if (!(on && globe->_directScreen)) return;
		SDL_SetColorKey(globe->getSurface(), SDL_SRCCOLORKEY, 0);
		if (!globe->_gpuAliveFlag) globe->_gpuAliveFlag = std::make_shared<bool>(true);
		if (!globe->_gpuSphereOK && !globe->initSphereGPU()) { globe->_gpuDirectMode = false; return; }
		std::weak_ptr<bool> wf = globe->_gpuAliveFlag;
		Screen* screen = globe->_directScreen;
		screen->registerGPUPassPreComposite([globe, wf, screen]() {
			if (!wf.lock()) return;
			CalypsoGeoscapeHdGlobeDirect::drawPass(globe);
		});
	}

	static void drawPass(Globe* globe);   // body in Globe.cpp

}; /* struct */

} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */