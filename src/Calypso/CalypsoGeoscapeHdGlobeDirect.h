#pragma once

#ifdef __EMSCRIPTEN__
/* Included from Geoscape/Globe.cpp AFTER Globe.h and after the local
 * GlobeSphereGlSave helper definition (drawPass body lives in the .cpp). */
#include <cmath>
#include <memory>
#include <string>

#include "../Engine/Game.h"
#include "../Engine/Logger.h"
#include "CalypsoHdUiOverlay.h"
#include "../Engine/GpuTexture.h"
#include "../Engine/Options.h"
#include "../Engine/Screen.h"
#include "../Mod/Mod.h"

namespace OpenXcom
{

struct CalypsoGeoscapeHdGlobeDirect
{
	struct PhysicalGlobeRect
	{
		int x = 0;
		int y = 0;
		int w = 0;
		int h = 0;
		int bottom = 0;
	};

	static bool physicalGlobeRect(const Globe* globe, PhysicalGlobeRect& rect)
	{
		if (!globe || !globe->_directScreen || Options::displayWidth <= 0 || Options::displayHeight <= 0)
			return false;
		const double xs = globe->_directScreen->getXScale();
		const double ys = globe->_directScreen->getYScale();
		rect.x = (int)std::lround(globe->getX() * xs)
			+ globe->_directScreen->getCursorLeftBlackBand();
		rect.y = (int)std::lround(globe->getY() * ys)
			+ globe->_directScreen->getCursorTopBlackBand();
		rect.w = std::max(1, (int)std::lround(globe->getWidth() * xs));
		rect.h = std::max(1, (int)std::lround(globe->getHeight() * ys));
		rect.bottom = Options::displayHeight - rect.y - rect.h;
		return rect.x >= 0 && rect.y >= 0 && rect.w > 0 && rect.h > 0
			&& rect.x < Options::displayWidth && rect.y < Options::displayHeight
			&& rect.bottom >= 0 && rect.x + rect.w <= Options::displayWidth
			&& rect.y + rect.h <= Options::displayHeight;
	}

	static void setPhysicalGlobeClip(const Globe* globe)
	{
		PhysicalGlobeRect rect;
		if (!physicalGlobeRect(globe, rect))
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				"Geoscape physical globe rect is invalid");
		glViewport(0, 0, Options::displayWidth, Options::displayHeight);
		glEnable(GL_SCISSOR_TEST);
		glScissor(rect.x, rect.bottom, rect.w, rect.h);
	}

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
		if (!on)
		{
			if (globe->_gpuWorldPass.valid())
			{
				globe->_gpuWorldPass.owner->unregisterGPUPassWorld(globe->_gpuWorldPass);
				globe->_gpuWorldPass = {};
			}
			globe->_gpuDirectMode = false;
			globe->_directScreen = nullptr;
			globe->_gpuBorderLines.clear();
			globe->_gpuBorderVertices.clear();
			globe->_coloredLineBatch.clearCommands();
			globe->_gpuDebugLines.clear();
			globe->_gpuDebugVertices.clear();
			globe->_gpuLabelIconPendingDraws.clear();
			globe->_gpuLabelIconCommittedDraws.clear();
			globe->_gpuBorderCapacityExceeded = false;
			globe->_gpuRadarFlightCapacityExceeded = false;
			globe->_gpuDebugCapacityExceeded = false;
			globe->_gpuLabelCapacityExceeded = false;
			globe->_gpuLogicalWorldComplete = true;
			return;
		}
		Screen* screen = globe && globe->_game ? globe->_game->getScreen() : nullptr;
		if (!screen)
		{
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				"Geoscape GPU-direct Screen/world slot unavailable");
			return;
		}
		if (globe->_gpuDirectMode)
		{
			if (!globe->_gpuWorldPass.valid() && globe->_directScreen)
			{
				Screen* screen = globe->_directScreen;
				std::weak_ptr<bool> wf = globe->_gpuAliveFlag;
				globe->_gpuWorldPass = screen->registerGPUPassWorld([globe, wf]() {
					if (!wf.lock()) return;
					CalypsoGeoscapeHdGlobeDirect::drawPass(globe);
				});
			}
			return;
		}
		globe->_gpuDirectMode = on;
		/* One stable activation marker: logged exactly when this Globe
		 * transitions from canonical readback to the physical direct
		 * composite. The repeat-call early return above guarantees repeated
		 * setGpuDirect(true) calls never re-log it. */
		Log(LOG_INFO) << "Globe: gpu-direct composite active";
		globe->_directScreen = screen;
		SDL_SetColorKey(globe->getSurface(), SDL_SRCCOLORKEY, 0);
		if (!globe->_gpuAliveFlag) globe->_gpuAliveFlag = std::make_shared<bool>(true);
		if (!globe->_gpuSphereOK && !globe->initSphereGPU())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape GPU-direct sphere resources unavailable");
		globe->drawMarkers();
		std::weak_ptr<bool> wf = globe->_gpuAliveFlag;
		globe->_gpuWorldPass = screen->registerGPUPassWorld([globe, wf]() {
			if (!wf.lock()) return;
			CalypsoGeoscapeHdGlobeDirect::drawPass(globe);
		});
	}

	static void recordMarker(Globe* globe, Surface* frame, int x, int y, int shade);
	static void recordBorderLine(Globe* globe, int x1, int y1, int x2, int y2);
	static void recordDebugLine(Globe* globe, double lon1, double lat1, double lon2, double lat2, Uint8 color);
	static void recordRadarFlightLine(Globe* globe, double x1, double y1, double x2, double y2,
		double lon1, double lat1, double lon2, double lat2, int shade);
	static void recordLabelText(Globe* globe, const std::string& text, int width, int height,
		int x, int y, Uint8 color);
	static void recordLabelIcon(Globe* globe, Surface* frame, int x, int y, int shade);
	static void ensureLogicalWorldComplete(Globe* globe);
	static GpuTexture* markerTexture(Globe* globe, Surface* frame, int shade);
	static GpuTexture* labelTexture(Globe* globe, Globe::LabelTexture& entry);
	static void ensureBorderResources(Globe* globe);
	static void ensureColoredLineResources(Globe* globe);
	static void drawBorderPass(Globe* globe);
	/* §15.4.2 (review-corrected lifecycle): called from Globe::draw() BEFORE
	 * drawRadars/drawFlights. Builds the snapshot key and renders the cache
	 * verdict; on a miss it clears the command batch so the owners record a
	 * fresh snapshot, and returns false. On a hit it skips ALL radar/flight
	 * CPU generation and returns true - the committed packed vertices and the
	 * uploaded VBO stay authoritative across frames and context restores. */
	static bool beginRadarFlightFrame(Globe* globe);
	/* Packs the freshly recorded commands when begin returned false; a no-op
	 * on cache-hit frames. */
	static void finishRadarFlightFrame(Globe* globe, bool rebuilt);
	/* §15.4.5: explicit per-frame label/icon snapshot commit, owned by
	 * Globe::draw() after drawDetail() has recorded the current frame. */
	static void commitLabelIconSnapshot(Globe* globe);
	static void drawRadarFlightPass(Globe* globe);
	static void drawDebugPass(Globe* globe);
	static void ensureMarkerResources(Globe* globe);
	static void drawMarkerPass(Globe* globe);
	static void ensureLabelResources(Globe* globe);
	static void drawLabelIconPass(Globe* globe);
	static void drawPass(Globe* globe);   // body in Globe.cpp

}; /* struct */

} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */
