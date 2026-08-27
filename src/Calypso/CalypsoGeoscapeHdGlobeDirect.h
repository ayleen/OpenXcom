#pragma once

#ifdef __EMSCRIPTEN__
/* Browser-only Geoscape direct-composite state and narrow frozen-file seam.
 * Implementations live in CalypsoGeoscapeHdGlobeDirect.cpp. */
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <GLES3/gl3.h>

#include "../Engine/Game.h"
#include "../Engine/Logger.h"
#include "CalypsoHdUiOverlay.h"
#include "../Engine/GpuTexture.h"
#include "../Engine/Options.h"
#include "../Engine/Screen.h"
#include "../Mod/Mod.h"

namespace OpenXcom
{
namespace Calypso
{
/// Guard-R3: browser-only GPU state owned by Globe (extracted from Globe.h).
/// Whole-file emscripten TU counterpart: CalypsoGeoscapeHdGlobeDirect.cpp.
struct CalypsoGlobeGpuState
{
	unsigned  _sphereVAO    = 0u;
	unsigned  _sphereFBO    = 0u;
	unsigned  _sphereFBOTex = 0u;
	bool      _gpuSphereOK  = false;
	Shader*   _globeShader  = nullptr; // owned; created in initSphereGPU()
	std::shared_ptr<bool> _gpuAliveFlag;
	bool      _gpuResetCallbackRegistered = false;
	bool      _gpuDirectMode = false;
	Screen*   _directScreen  = nullptr;
	ScreenWorldPassHandle _gpuWorldPass;
	struct MarkerDraw
	{
		Surface* frame = nullptr;
		int x = 0;
		int y = 0;
		int shade = 0;
	};
	std::vector<MarkerDraw> _gpuMarkerPendingDraws;
	std::vector<MarkerDraw> _gpuMarkerCommittedDraws;
	struct BorderLine
	{
		float x1 = 0.f;
		float y1 = 0.f;
		float x2 = 0.f;
		float y2 = 0.f;
	};
	std::vector<BorderLine> _gpuBorderLines;
	std::vector<float> _gpuBorderVertices;
	size_t _gpuBorderCapacity = 0;
	bool _gpuBorderCapacityExceeded = false;
	CalypsoGeoscapeColoredLineBatchState _coloredLineBatch;
	CalypsoGeoscapeColoredLineCacheState _coloredLineCache;
	bool _gpuRadarFlightCapacityExceeded = false;
	unsigned  _coloredLineVAO     = 0u;
	unsigned  _coloredLineVBO     = 0u;
	Shader*   _coloredLineShader  = nullptr;
	bool      _coloredLineResourcesReady = false;
	CalypsoGeoscapeColoredLineBatchState _hoverLineBatch;
	unsigned  _hoverLineVAO      = 0u;
	unsigned  _hoverLineVBO      = 0u;
	bool      _hoverLineResourcesReady = false;
	CalypsoGeoscapeColoredLineBatchState* _activeLineBatch = nullptr;
	std::vector<double> _hoverCanonicalRanges;
	bool _hoverRangesReady = false;
	double _lastHoverOverlayLon = 0.0;
	double _lastHoverOverlayLat = 0.0;
	int _lastHoverOverlayRectX = 0;
	int _lastHoverOverlayRectY = 0;
	int _lastHoverOverlayRectW = 0;
	int _lastHoverOverlayRectH = 0;
	double _lastHoverOverlayScaleX = 0.0;
	double _lastHoverOverlayScaleY = 0.0;
	int _lastHoverOverlayDisplayW = 0;
	int _lastHoverOverlayDisplayH = 0;
	bool _hoverOverlayDirty = true;
	struct DebugLine
	{
		float x1 = 0.f;
		float y1 = 0.f;
		float x2 = 0.f;
		float y2 = 0.f;
		Uint8 color = 0;
	};
	std::vector<DebugLine> _gpuDebugLines;
	std::vector<float> _gpuDebugVertices;
	size_t _gpuDebugCapacity = 0;
	bool _gpuDebugCapacityExceeded = false;
	bool _gpuLogicalWorldComplete = true;
	std::uint64_t _gpuMarkerPaletteGeneration = 0;
	std::uint64_t _gpuLabelPaletteGeneration = 0;
	std::uint64_t _gpuRadarPaletteGeneration = 0;
	struct MarkerTexture
	{
		Surface* frame = nullptr;
		int shade = 0;
		std::uint64_t paletteGeneration = 0;
		GpuTexture* texture = nullptr;
	};
	std::vector<MarkerTexture> _gpuMarkerTextures;
	struct LabelTexture
	{
		std::string text;
		int width = 0;
		int height = 0;
		Uint8 color = 0;
		std::uint64_t paletteGeneration = 0;
		Surface* frame = nullptr;
		GpuTexture* texture = nullptr;
	};
	struct LabelIconDraw
	{
		LabelTexture* label = nullptr;
		Surface* frame = nullptr;
		int x = 0;
		int y = 0;
		int shade = 0;
	};
	std::vector<LabelTexture> _gpuLabelTextures;
	std::vector<LabelIconDraw> _gpuLabelIconPendingDraws;
	std::vector<LabelIconDraw> _gpuLabelIconCommittedDraws;
	bool _gpuLabelCapacityExceeded = false;
	unsigned  _markerVAO     = 0u;
	unsigned  _markerVBO     = 0u;
	Shader*   _markerShader  = nullptr;
	bool      _gpuMarkerReady = false;
	unsigned  _borderVAO     = 0u;
	unsigned  _borderVBO     = 0u;
	Shader*   _borderShader  = nullptr;
	bool      _gpuBorderReady = false;
};
} // namespace Calypso

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
		if (!globe || !globe->_gpuState->_directScreen || Options::displayWidth <= 0 || Options::displayHeight <= 0)
			return false;
		const double xs = globe->_gpuState->_directScreen->getXScale();
		const double ys = globe->_gpuState->_directScreen->getYScale();
		rect.x = (int)std::lround(globe->getX() * xs)
			+ globe->_gpuState->_directScreen->getCursorLeftBlackBand();
		rect.y = (int)std::lround(globe->getY() * ys)
			+ globe->_gpuState->_directScreen->getCursorTopBlackBand();
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
		if (globe->_gpuState->_gpuDirectMode && globe->_gpuState->_directScreen != nullptr)
		{
			w = std::max(1, (int)std::lround(w * globe->_gpuState->_directScreen->getXScale()));
			h = std::max(1, (int)std::lround(h * globe->_gpuState->_directScreen->getYScale()));
		}
	}

	static void setGpuDirect(Globe* globe, bool on)
	{
		if (!on)
		{
			if (globe->_gpuState->_gpuWorldPass.valid())
			{
				globe->_gpuState->_gpuWorldPass.owner->unregisterGPUPassWorld(globe->_gpuState->_gpuWorldPass);
				globe->_gpuState->_gpuWorldPass = {};
			}
			globe->_gpuState->_gpuDirectMode = false;
			globe->_gpuState->_directScreen = nullptr;
			globe->_gpuState->_gpuBorderLines.clear();
			globe->_gpuState->_gpuBorderVertices.clear();
			globe->_gpuState->_coloredLineBatch.clearCommands();
			globe->_gpuState->_hoverLineBatch.clearCommands();
			globe->_gpuState->_gpuDebugLines.clear();
			globe->_gpuState->_gpuDebugVertices.clear();
			globe->_gpuState->_gpuLabelIconPendingDraws.clear();
			globe->_gpuState->_gpuLabelIconCommittedDraws.clear();
			globe->_gpuState->_gpuBorderCapacityExceeded = false;
			globe->_gpuState->_gpuRadarFlightCapacityExceeded = false;
			globe->_gpuState->_gpuDebugCapacityExceeded = false;
			globe->_gpuState->_gpuLabelCapacityExceeded = false;
			globe->_gpuState->_gpuLogicalWorldComplete = true;
			return;
		}
		Screen* screen = globe && globe->_game ? globe->_game->getScreen() : nullptr;
		if (!screen)
		{
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				"Geoscape GPU-direct Screen/world slot unavailable");
			return;
		}
		if (globe->_gpuState->_gpuDirectMode)
		{
			if (!globe->_gpuState->_gpuWorldPass.valid() && globe->_gpuState->_directScreen)
			{
				Screen* screen = globe->_gpuState->_directScreen;
				std::weak_ptr<bool> wf = globe->_gpuState->_gpuAliveFlag;
				globe->_gpuState->_gpuWorldPass = screen->registerGPUPassWorld([globe, wf]() {
					if (!wf.lock()) return;
					CalypsoGeoscapeHdGlobeDirect::drawPass(globe);
				});
			}
			return;
		}
		globe->_gpuState->_gpuDirectMode = on;
		/* One stable activation marker: logged exactly when this Globe
		 * transitions from canonical readback to the physical direct
		 * composite. The repeat-call early return above guarantees repeated
		 * setGpuDirect(true) calls never re-log it. */
		Log(LOG_INFO) << "Globe: gpu-direct composite active";
		globe->_gpuState->_gpuBorderLines.reserve(Globe::GPU_BORDER_LINE_CAPACITY);
		globe->_gpuState->_gpuBorderVertices.reserve(Globe::GPU_BORDER_VERTEX_FLOAT_CAPACITY);
		globe->_gpuState->_gpuDebugLines.reserve(Globe::GPU_DEBUG_LINE_CAPACITY);
		globe->_gpuState->_gpuDebugVertices.reserve(Globe::GPU_DEBUG_VERTEX_FLOAT_CAPACITY);
		globe->_gpuState->_gpuLabelTextures.reserve(Globe::GPU_LABEL_TEXTURE_CAPACITY);
		globe->_gpuState->_gpuLabelIconPendingDraws.reserve(Globe::GPU_LABEL_DRAW_CAPACITY);
		globe->_gpuState->_gpuLabelIconCommittedDraws.reserve(Globe::GPU_LABEL_DRAW_CAPACITY);
		globe->_gpuState->_directScreen = screen;
		SDL_SetColorKey(globe->getSurface(), SDL_SRCCOLORKEY, 0);
		if (!globe->_gpuState->_gpuAliveFlag) globe->_gpuState->_gpuAliveFlag = std::make_shared<bool>(true);
		if (!globe->_gpuState->_gpuSphereOK && !globe->initSphereGPU())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape GPU-direct sphere resources unavailable");
		globe->drawMarkers();
		std::weak_ptr<bool> wf = globe->_gpuState->_gpuAliveFlag;
		globe->_gpuState->_gpuWorldPass = screen->registerGPUPassWorld([globe, wf]() {
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
	static GpuTexture* labelTexture(Globe* globe, Calypso::CalypsoGlobeGpuState::LabelTexture& entry);
	static void ensureBorderResources(Globe* globe);
	static void ensureColoredLineResources(Globe* globe);
	/* §16.5: hover-circle overlay GPU resources (separate VAO/VBO from static). */
	static void ensureHoverLineResources(Globe* globe);
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
	/* §16.5: dynamic hover-circle overlay draw pass.  Called after the static
	 * radar/flight pass; uploads and draws the per-frame hover batch. */
	static void drawHoverPass(Globe* globe);
	static void drawDebugPass(Globe* globe);
	static void ensureMarkerResources(Globe* globe);
	static void drawMarkerPass(Globe* globe);
	static void ensureLabelResources(Globe* globe);
	static void drawLabelIconPass(Globe* globe);
	static void destroyGpuState(Globe* globe);
	static void drawPass(Globe* globe);

}; /* struct */

} /* namespace OpenXcom */

#endif /* __EMSCRIPTEN__ */
