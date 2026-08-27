#ifdef __EMSCRIPTEN__
/* Guard R3: browser-only Geoscape HD direct-composite implementation extracted
 * from Geoscape/Globe.cpp (policy R3). Whole-file emscripten TU; empty native.
 * Works on Globe state through the CalypsoGlobeGpuState pointer in Globe.h._gpuState.
 */
#include "../Geoscape/Globe.h"
#include "CalypsoGeoscapeHdGlobeDirect.h"
#include "CalypsoGlobeHdSphere.h"
#include "CalypsoGeoscapeQaPresentation.h"
#include "CalypsoGeoscapeColoredLineBatch.h"
#include "CalypsoHdUiOverlay.h"
#include "../Engine/GpuInit.h"
#include "../Engine/GpuTexture.h"
#include "../Engine/Logger.h"
#include "../Engine/Options.h"
#include "../Engine/Shader.h"
#include "../Engine/ShaderDraw.h"
#include "../Engine/SurfaceSet.h"
#include "../Interface/Text.h"
#include "../Mod/Mod.h"
#include "../Mod/AlienDeployment.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/AlienBase.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/Craft.h"
#include "../Savegame/Target.h"
#include "../Savegame/Ufo.h"
#include <SDL.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>


extern "C" int calypso_context_reset_sentinel_pending(void);
extern "C" void calypso_context_reset_sentinel_consumed(void);
extern "C" void calypso_context_reset_boundary_close(void);
namespace OpenXcom {
namespace {
static std::string calypsoGlFailure(const char *operation, GLenum error)
{
	std::ostringstream detail;
	detail << operation << " (0x" << std::hex << error << ")";
	return detail.str();
}


static GLenum calypsoOwnedResetError()
{
	static const GLenum CALYPSO_CONTEXT_LOST_WEBGL = 0x9242;
	const GLenum error = glGetError();
	/* The browser can defer the reset token until the first physical-world
	 * query, after Screen's SDL-flush query has already drained an earlier
	 * token. Consume it only when the reset-boundary observer transferred the
	 * one-shot ownership; consuming it also ends the bounded ownership window
	 * at this boundary. The same numeric value later is a real pass error. */
	if (error == CALYPSO_CONTEXT_LOST_WEBGL && calypso_context_reset_sentinel_pending())
	{
		calypso_context_reset_sentinel_consumed();
		calypso_context_reset_boundary_close();
		return GL_NO_ERROR;
	}
	return error;
}

} // anonymous namespace
} // namespace OpenXcom
namespace OpenXcom {
void CalypsoGeoscapeHdGlobeDirect::invalidatePaletteCaches(Globe* globe)
{
	/* Uploaded marker and label pixels are immutable palette snapshots. */
	for (auto& entry : globe->_gpuState->_gpuMarkerTextures)
		delete entry.texture;
	globe->_gpuState->_gpuMarkerTextures.clear();
	++globe->_gpuState->_gpuMarkerPaletteGeneration;
	/* The radar/flight snapshot key rides the same palette boundary. */
	++globe->_gpuState->_gpuRadarPaletteGeneration;
	for (auto& entry : globe->_gpuState->_gpuLabelTextures)
	{
		delete entry.texture;
		delete entry.frame;
	}
	globe->_gpuState->_gpuLabelTextures.clear();
	globe->_gpuState->_gpuLabelIconPendingDraws.clear();
	globe->_gpuState->_gpuLabelIconCommittedDraws.clear();
	globe->_gpuState->_gpuDebugLines.clear();
	globe->_gpuState->_gpuDebugVertices.clear();
	globe->_gpuState->_gpuDebugCapacityExceeded = false;
	++globe->_gpuState->_gpuLabelPaletteGeneration;
}

void CalypsoGeoscapeHdGlobeDirect::destroyGpuState(Globe* globe)
{
	CalypsoGeoscapeHdGlobeDirect::setGpuDirect(globe, false);
	globe->_gpuState->_gpuAliveFlag.reset();  // M6: expire reset callback before deleting GL objects
	delete globe->_gpuState->_globeShader;
	delete globe->_gpuState->_markerShader;
	delete globe->_gpuState->_borderShader;
	/* Review fix: coloured-line resources were leaked on teardown. */
	delete globe->_gpuState->_coloredLineShader;
	globe->_gpuState->_coloredLineShader = nullptr;
	for (auto& entry : globe->_gpuState->_gpuMarkerTextures) delete entry.texture;
	globe->_gpuState->_gpuMarkerTextures.clear();
	for (auto& entry : globe->_gpuState->_gpuLabelTextures)
	{
		delete entry.texture;
		delete entry.frame;
	}
	globe->_gpuState->_gpuLabelTextures.clear();
	if (globe->_gpuState->_sphereFBO)    glDeleteFramebuffers(1,  &globe->_gpuState->_sphereFBO);
	if (globe->_gpuState->_sphereFBOTex) glDeleteTextures(1,      &globe->_gpuState->_sphereFBOTex);
	if (globe->_gpuState->_sphereVAO)    glDeleteVertexArrays(1,  &globe->_gpuState->_sphereVAO);
	if (globe->_gpuState->_markerVAO)    glDeleteVertexArrays(1,  &globe->_gpuState->_markerVAO);
	if (globe->_gpuState->_markerVBO)    glDeleteBuffers(1, &globe->_gpuState->_markerVBO);
	if (globe->_gpuState->_borderVAO)    glDeleteVertexArrays(1,  &globe->_gpuState->_borderVAO);
	if (globe->_gpuState->_borderVBO)    glDeleteBuffers(1, &globe->_gpuState->_borderVBO);
	/* Review fix: VAO/VBO handles for the one-draw batch. */
	if (globe->_gpuState->_coloredLineVAO) glDeleteVertexArrays(1, &globe->_gpuState->_coloredLineVAO);
	if (globe->_gpuState->_coloredLineVBO) glDeleteBuffers(1, &globe->_gpuState->_coloredLineVBO);
	globe->_gpuState->_coloredLineResourcesReady = false;
	/* §16.5: hover overlay resources and transient committed data. */
	if (globe->_gpuState->_hoverLineVAO) glDeleteVertexArrays(1, &globe->_gpuState->_hoverLineVAO);
	if (globe->_gpuState->_hoverLineVBO) glDeleteBuffers(1, &globe->_gpuState->_hoverLineVBO);
	globe->_gpuState->_hoverLineVAO = 0u;
	globe->_gpuState->_hoverLineVBO = 0u;
	globe->_gpuState->_hoverLineResourcesReady = false;
	globe->_gpuState->_hoverLineUploadDirty = false;
	globe->_gpuState->_hoverOverlayActive = false;
	globe->_gpuState->_hoverLineBatch.clearCommands();
	globe->_gpuState->_activeLineBatch = nullptr;
	delete globe->_gpuState;
	globe->_gpuState = nullptr;
}
void CalypsoGeoscapeHdGlobeDirect::drawPass(Globe* globe)
	{
		if (!globe || !globe->_gpuState->_gpuDirectMode || !globe->_gpuState->_directScreen) return;
		if (Calypso::calypsoRadarCountersEnabled())
			++Calypso::calypsoRadarCounters().frames;
		const GLenum worldPreflightError = calypsoOwnedResetError();
		if (worldPreflightError != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				calypsoGlFailure("WebGL context restore world preflight failed", worldPreflightError));
		GlobeSphereGlSave preflightState;
		const GLenum stateSaveError = preflightState.save();
		if (stateSaveError != GL_NO_ERROR)
		{
			std::string detail = calypsoGlFailure("Geoscape world GL state snapshot failed", stateSaveError);
			if (preflightState.errorOperation)
				detail += std::string(" at ") + preflightState.errorOperation;
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(detail);
		}
		if (!GpuInit::ready() || !globe->_gpuState->_gpuSphereOK || !globe->_gpuState->_sphereVAO)
		{
			if (!globe->initSphereGPU() || !globe->_gpuState->_gpuSphereOK || !globe->_gpuState->_sphereVAO)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape Earth GPU resources unavailable");
		}
		if (!globe->_gpuState->_globeShader || !globe->_gpuState->_globeShader->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape Earth shader unavailable");
		CalypsoGeoscapeHdGlobeDirect::ensureLogicalWorldComplete(globe);
		CalypsoGeoscapeHdGlobeDirect::ensureBorderResources(globe);
		CalypsoGeoscapeHdGlobeDirect::ensureColoredLineResources(globe);
		CalypsoGeoscapeHdGlobeDirect::ensureHoverLineResources(globe);
		CalypsoGeoscapeHdGlobeDirect::ensureMarkerResources(globe);
		CalypsoGeoscapeHdGlobeDirect::ensureLabelResources(globe);
		preflightState.restore();
		if (preflightState.restoreError != GL_NO_ERROR)
		{
			std::string detail = calypsoGlFailure("Geoscape world GL state restore failed", preflightState.restoreError);
			if (preflightState.restoreErrorOperation)
				detail += std::string(" at ") + preflightState.restoreErrorOperation;
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(detail);
		}
		Mod* mod = globe->_game->getMod();
		GpuTexture* bathyTex = mod->getGlobeTexture("bathymetry");
		GpuTexture* diffuseTex = mod->getGlobeTexture("diffuse");
		GpuTexture* nightTex = mod->getGlobeTexture("night");
		GpuTexture* cloudsTex = mod->getGlobeTexture("clouds");
		if (!bathyTex || !diffuseTex || !nightTex || !cloudsTex
			|| !bathyTex->isValid() || !diffuseTex->isValid()
			|| !nightTex->isValid() || !cloudsTex->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape Earth textures unavailable");
		CalypsoGeoscapeHdGlobeDirect::PhysicalGlobeRect globeRect;
		if (!CalypsoGeoscapeHdGlobeDirect::physicalGlobeRect(globe, globeRect))
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape physical globe rect is invalid");
		const double xs = globe->_gpuState->_directScreen->getXScale();
		const double ys = globe->_gpuState->_directScreen->getYScale();
		const int dispX = globeRect.x;
		const int dispY = globeRect.y;
		const int dispW = globeRect.w;
		const int dispH = globeRect.h;
		const Uint64 calypsoEarthStart = Calypso::calypsoPassTimersEnabled()
			? SDL_GetPerformanceCounter() : 0;
		GlobeSphereGlSave st; st.save();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(dispX, (int)Options::displayHeight - dispY - dispH, dispW, dispH);
		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		globe->_gpuState->_globeShader->use();
		bathyTex->bind(0);   globe->_gpuState->_globeShader->setUniform1i("u_bathymetry", 0);
		diffuseTex->bind(1); globe->_gpuState->_globeShader->setUniform1i("u_diffuse", 1);
		nightTex->bind(2);   globe->_gpuState->_globeShader->setUniform1i("u_night", 2);
		/* Stage 13 QA (loopback-only): capture rows may bind a transparent
		 * cloud input; production always samples the mod clouds texture.
		 * Hidden allocates/uploads its persistent 1x1 input once on mode
		 * entry and reuses it steady-state; context loss re-creates it via
		 * the existing ShaderManager recovery path. */
		GpuTexture* cloudsBound = cloudsTex;
		const auto& qa = Calypso::calypsoGeoscapeQaPresentation();
		if (qa.cloudMode == Calypso::GeoscapeQaCloudMode::Hidden)
		{
			if (!Calypso::calypsoGlobeQaHiddenCloudsTexture()->isValid()) cloudsBound = cloudsTex;
			else cloudsBound = Calypso::calypsoGlobeQaHiddenCloudsTexture();
		}
		cloudsBound->bind(3); globe->_gpuState->_globeShader->setUniform1i("u_clouds", 3);
		globe->_gpuState->_globeShader->setUniform1i("u_background", 1);
		globe->_gpuState->_globeShader->setUniform2f("u_viewportSize", (float)dispW, (float)dispH);
		globe->_gpuState->_globeShader->setUniform2f("u_globeCenter", (float)(globe->_cenX * xs), (float)(globe->_cenY * ys));
		globe->_gpuState->_globeShader->setUniform1f("u_globeRadius", (float)(globe->_zoomRadius[globe->_zoom] * std::min(xs, ys)));
		globe->_gpuState->_globeShader->setUniform1f("u_camLat", (float)globe->_cenLat);
		globe->_gpuState->_globeShader->setUniform1f("u_camLon", (float)globe->_cenLon);
		Cord sd = globe->getSunDirectionWorld();
		/* Stage 13 QA (loopback-only): deterministic day/night rows replace the
		 * fed value only; campaign time is never read or mutated. */
		if (qa.sunMode != Calypso::GeoscapeQaSunMode::Live)
			sd = Calypso::calypsoGlobeQaCord(Calypso::calypsoGeoscapeQaSunDirection(qa.sunMode, globe->_cenLon, globe->_cenLat));
		globe->_gpuState->_globeShader->setUniform3f("u_sunDir", (float)sd.x, (float)sd.y, (float)sd.z);
		/* Stage 13 QA (loopback-only): fixed/reduced decorative clock for the
		 * cloud drift. Production keeps its exact expression when no QA
		 * control is active. */
		float timeMs = Calypso::calypsoGlobeQaEffectiveMs((float)SDL_GetTicks());
		globe->_gpuState->_globeShader->setUniform1f("u_time", timeMs * 0.001f);
		float mipLvl = std::max(0.f, std::min(1.35f, 1.35f - (float)globe->_zoom * 0.27f));
		globe->_gpuState->_globeShader->setUniform1f("u_mipLevel", mipLvl);
		glBindVertexArray(globe->_gpuState->_sphereVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0u);
		for (int i = 3; i >= 0; --i) { glActiveTexture(GL_TEXTURE0 + i); glBindTexture(GL_TEXTURE_2D, 0u); }
		if (glGetError() != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape Earth draw failed");
		st.restore();
		if (calypsoEarthStart)
			Calypso::calypsoPassTimers().earthUs +=
				(Uint64)((SDL_GetPerformanceCounter() - calypsoEarthStart) * 1000000ull / SDL_GetPerformanceFrequency());
		const Uint64 calypsoBorderStart = Calypso::calypsoPassTimersEnabled()
			? SDL_GetPerformanceCounter() : 0;
		/* SS15.4.6 consolidation (Option A): ONE state guard serves the border
		 * and radar passes together; each previously snapshotted/restored the
		 * same GL state every frame. */
		GlobeSphereGlSave stLines; stLines.save();
		CalypsoGeoscapeHdGlobeDirect::setPhysicalGlobeClip(globe);
		/* Review fix: the Earth guard restores with blending disabled, so the
		 * shared line guard must re-enable it before the border pass draws. */
		glEnable(GL_BLEND);
		CalypsoGeoscapeHdGlobeDirect::drawBorderPass(globe);
		if (calypsoBorderStart)
			Calypso::calypsoPassTimers().borderUs +=
				(Uint64)((SDL_GetPerformanceCounter() - calypsoBorderStart) * 1000000ull / SDL_GetPerformanceFrequency());
		const Uint64 calypsoRadarStart = Calypso::calypsoPassTimersEnabled()
			? SDL_GetPerformanceCounter() : 0;
		CalypsoGeoscapeHdGlobeDirect::drawRadarFlightPass(globe);
		/* §16.5: draw the hover-circle overlay immediately after the static
		 * radar/flight pass, within the same GL state guard.  The hover
		 * batch uses its own VAO/VBO so it never overwrites the static data. */
		CalypsoGeoscapeHdGlobeDirect::drawHoverPass(globe);
		if (calypsoRadarStart)
			Calypso::calypsoPassTimers().radarUs +=
				(Uint64)((SDL_GetPerformanceCounter() - calypsoRadarStart) * 1000000ull / SDL_GetPerformanceFrequency());
		stLines.restore();
		const Uint64 calypsoLabelStart = Calypso::calypsoPassTimersEnabled()
			? SDL_GetPerformanceCounter() : 0;
		CalypsoGeoscapeHdGlobeDirect::drawLabelIconPass(globe);
		if (calypsoLabelStart)
			Calypso::calypsoPassTimers().labelUs +=
				(Uint64)((SDL_GetPerformanceCounter() - calypsoLabelStart) * 1000000ull / SDL_GetPerformanceFrequency());
		const Uint64 calypsoMarkerStart = Calypso::calypsoPassTimersEnabled()
			? SDL_GetPerformanceCounter() : 0;
		CalypsoGeoscapeHdGlobeDirect::drawDebugPass(globe);
		CalypsoGeoscapeHdGlobeDirect::drawMarkerPass(globe);
		if (calypsoMarkerStart)
			Calypso::calypsoPassTimers().markerUs +=
				(Uint64)((SDL_GetPerformanceCounter() - calypsoMarkerStart) * 1000000ull / SDL_GetPerformanceFrequency());
	}

	void CalypsoGeoscapeHdGlobeDirect::recordMarker(Globe* globe, Surface* frame, int x, int y, int shade)
	{
		if (!globe || !frame)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker frame unavailable");
		globe->_gpuState->_gpuMarkerPendingDraws.push_back({frame, x, y, shade});
	}

	void CalypsoGeoscapeHdGlobeDirect::recordBorderLine(Globe* globe, int x1, int y1, int x2, int y2)
	{
		if (!globe)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border owner unavailable");
		if (globe->_gpuState->_gpuBorderLines.size() >= Globe::GPU_BORDER_LINE_CAPACITY
			|| globe->_gpuState->_gpuBorderLines.size() >= globe->_gpuState->_gpuBorderLines.capacity())
		{
			globe->_gpuState->_gpuBorderCapacityExceeded = true;
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border batch capacity exhausted");
		}
		globe->_gpuState->_gpuBorderLines.push_back({(float)x1, (float)y1, (float)x2, (float)y2});
	}

	void CalypsoGeoscapeHdGlobeDirect::recordDebugLine(Globe* globe, double lon1, double lat1,
		double lon2, double lat2, Uint8 color)
	{
		if (!globe)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry owner unavailable");
		double sx = lon2 - lon1;
		double sy = lat2 - lat1;
		if (sx < 0.0) sx += 2.0 * M_PI;
		const int segments = std::max(1, std::abs(sx) < 0.01
			? (int)std::abs(sy / (2.0 * M_PI) * 48.0)
			: (int)std::abs(sx / (2.0 * M_PI) * 96.0));
		sx /= segments;
		sy /= segments;
		for (int i = 0; i < segments; ++i)
		{
			const double ln1 = lon1 + sx * i;
			const double lt1 = lat1 + sy * i;
			const double ln2 = lon1 + sx * (i + 1);
			const double lt2 = lat1 + sy * (i + 1);
			if (globe->pointBack(ln2, lt2) || globe->pointBack(ln1, lt1)) continue;
			Sint16 px1, py1, px2, py2;
			globe->polarToCart(ln1, lt1, &px1, &py1);
			globe->polarToCart(ln2, lt2, &px2, &py2);
			if (globe->_gpuState->_gpuDebugLines.size() >= Globe::GPU_DEBUG_LINE_CAPACITY
				|| globe->_gpuState->_gpuDebugLines.size() >= globe->_gpuState->_gpuDebugLines.capacity())
			{
				globe->_gpuState->_gpuDebugCapacityExceeded = true;
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry batch capacity exhausted");
			}
			globe->_gpuState->_gpuDebugLines.push_back({(float)px1, (float)py1, (float)px2, (float)py2, color});
		}
	}

	void CalypsoGeoscapeHdGlobeDirect::recordRadarFlightLine(Globe* globe, double x1, double y1, double x2, double y2,
		double lon1, double lat1, double lon2, double lat2, int shade)
	{
		if (!globe)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape radar/flight owner unavailable");
		const bool front1 = !globe->pointBack(lon1, lat1);
		const bool front2 = !globe->pointBack(lon2, lat2);
		if (!front1 && !front2) return;
		/* Effective palette resolves once per logical segment; every raster
		 * step records its final RGBA bytes directly into the batch. */
		const SDL_Color* radarPalette = globe->getEffectivePalette();
		if (front1 != front2)
		{
			const Cord a(CordPolar(lon1, lat1));
			const Cord b(CordPolar(lon2, lat2));
			double lo = front1 ? 0.0 : 1.0;
			double hi = front1 ? 1.0 : 0.0;
			for (int i = 0; i < 24; ++i)
			{
				const double mid = (lo + hi) * 0.5;
				Cord m(a.x + (b.x - a.x) * mid, a.y + (b.y - a.y) * mid, a.z + (b.z - a.z) * mid);
				const double norm = m.norm();
				if (norm > 0.0) m /= norm;
				const CordPolar p(m);
				const bool front = !globe->pointBack(p.lon, p.lat);
				if (front == front1) lo = mid;
				else hi = mid;
			}
			const double limb = (lo + hi) * 0.5;
			Cord m(a.x + (b.x - a.x) * limb, a.y + (b.y - a.y) * limb, a.z + (b.z - a.z) * limb);
			const double norm = m.norm();
			if (norm > 0.0) m /= norm;
			const CordPolar p(m);
			double lx = 0.0, ly = 0.0;
			globe->polarToCart(p.lon, p.lat, &lx, &ly);
			if (front1) { x2 = lx; y2 = ly; }
			else { x1 = lx; y1 = ly; }
		}
		if (!globe->_clipper || globe->_clipper->LineClip(&x1, &y1, &x2, &y2) != 1)
			return;

		/* XuLine advances one floating-point raster step at a time and samples
		 * the source pixel before advancing.  Keep that progression here rather
		 * than assigning one shade to an entire physical segment: a path can
		 * cross land/ocean palette boundaries inside one logical segment. */
		const double deltax = x2 - x1;
		const double deltay = y2 - y1;
		const bool yDominant = std::abs((int)y2 - (int)y1) > std::abs((int)x2 - (int)x1);
		double len = yDominant
			? std::abs((int)y2 - (int)y1)
			: std::abs((int)x2 - (int)x1);
		if (len <= 0.0) return;
		double stepX = 0.0;
		double stepY = 0.0;
		if (yDominant)
		{
			stepX = deltax / len;
			stepY = y2 < y1 ? -1.0 : (std::abs(deltay) < 1e-12 ? 0.0 : 1.0);
		}
		else
		{
			stepX = x2 < x1 ? -1.0 : (std::abs(deltax) < 1e-12 ? 0.0 : 1.0);
			stepY = deltay / len;
		}

		const auto resolveStepColor = [globe, shade](double sampleX, double sampleY) -> Uint8
		{
			const Sint16 px = (Sint16)std::floor(sampleX);
			const Sint16 py = (Sint16)std::floor(sampleY);
			double sampleLon = 0.0, sampleLat = 0.0;
			globe->cartToPolar(px, py, &sampleLon, &sampleLat);
			if (!std::isfinite(sampleLon) || !std::isfinite(sampleLat)) return 0;
			int texture = -1, unusedShade = 0;
			globe->getPolygonTextureAndShade(sampleLon, sampleLat, &texture, &unusedShade);
			Uint8 dest = Globe::OCEAN_COLOR;
			if (texture >= 0 && globe->_texture)
			{
				Surface* frame = globe->_texture->getFrame(texture + globe->_zoomTexture);
				if (frame && frame->getWidth() > 0 && frame->getHeight() > 0)
				{
					int tx = (int)px % frame->getWidth();
					int ty = (int)py % frame->getHeight();
					if (tx < 0) tx += frame->getWidth();
					if (ty < 0) ty += frame->getHeight();
					dest = frame->getPixel(tx, ty);
				}
			}
			if (!dest) return 0;
			if (Globe::OCEAN_SHADING && dest >= Globe::OCEAN_COLOR && dest < Globe::OCEAN_COLOR + 32)
				return Globe::OCEAN_COLOR + (Uint8)(shade + 8);
			const Uint8 shadow = (Uint8)(shade * 3);
			if (shadow == 0) return dest;
			const int shadeOffset = shadow / 3;
			const int shaded = dest + shadeOffset;
			const int group = dest & helper::ColorGroup;
			return shaded > group + helper::ColorShade
				? (Uint8)(group + helper::ColorShade)
				: (Uint8)shaded;
		};

		double sampleX = x1;
		double sampleY = y1;
		while (len > 0.0)
		{
			const Uint8 color = resolveStepColor(sampleX, sampleY);
			if (color)
			{
				if (!radarPalette)
					Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape radar/flight palette unavailable");
				const bool hoverBatch =
					globe->_gpuState->_activeLineBatch == &globe->_gpuState->_hoverLineBatch;
				const size_t commandCapacity = globe->_gpuState->_activeLineBatch
					? globe->_gpuState->_activeLineBatch->commandCapacity()
					: OpenXcom::Calypso::COLORED_LINE_COMMAND_CAPACITY;
				const char* capacityError = hoverBatch
					? "Geoscape hover overlay batch capacity exhausted"
					: "Geoscape radar/flight batch capacity exhausted";
				/* Fail closed before publication: the selected hard command
				 * bound is checked before append and by the batch itself. */
				if (globe->_gpuState->_activeLineBatch
					&& globe->_gpuState->_activeLineBatch->commandCount() >= commandCapacity)
				{
					if (!hoverBatch)
						globe->_gpuState->_gpuRadarFlightCapacityExceeded = true;
					Calypso::CalypsoHdUiOverlay::instance().failHdRoute(capacityError);
				}
				const double nextX = len > 1.0 ? sampleX + stepX : x2;
				const double nextY = len > 1.0 ? sampleY + stepY : y2;
				const SDL_Color resolved = radarPalette[color];
				if (globe->_gpuState->_activeLineBatch
					&& !globe->_gpuState->_activeLineBatch->tryRecordCommand(
						sampleX, sampleY, nextX, nextY,
						resolved.r, resolved.g, resolved.b, resolved.a))
				{
					if (!hoverBatch)
						globe->_gpuState->_gpuRadarFlightCapacityExceeded = true;
					Calypso::CalypsoHdUiOverlay::instance().failHdRoute(capacityError);
				}
			}
			sampleX += stepX;
			sampleY += stepY;
			len -= 1.0;
		}
	}

	void CalypsoGeoscapeHdGlobeDirect::recordLabelText(Globe* globe, const std::string& text,
		int width, int height, int x, int y, Uint8 color)
	{
		if (!globe)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label owner unavailable");
		if (globe->_gpuState->_gpuLabelIconPendingDraws.size() >= Globe::GPU_LABEL_DRAW_CAPACITY
			|| globe->_gpuState->_gpuLabelIconPendingDraws.size() >= globe->_gpuState->_gpuLabelIconPendingDraws.capacity())
		{
			globe->_gpuState->_gpuLabelCapacityExceeded = true;
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label batch capacity exhausted");
		}
		Calypso::CalypsoGlobeGpuState::LabelTexture* found = nullptr;
		for (auto& entry : globe->_gpuState->_gpuLabelTextures)
		{
			if (entry.text == text && entry.width == width && entry.height == height
				&& entry.color == color && entry.paletteGeneration == globe->_gpuState->_gpuLabelPaletteGeneration)
			{
				found = &entry;
				break;
			}
		}
		if (!found)
		{
			if (globe->_gpuState->_gpuLabelTextures.size() >= Globe::GPU_LABEL_TEXTURE_CAPACITY
				|| globe->_gpuState->_gpuLabelTextures.size() >= globe->_gpuState->_gpuLabelTextures.capacity())
			{
				globe->_gpuState->_gpuLabelCapacityExceeded = true;
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label texture capacity exhausted");
			}
			Surface* frame = new (std::nothrow) Surface(width, height, 0, 0);
			if (!frame)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label surface allocation failed");
			Text label(width, height, 0, 0);
			label.setPalette(globe->getEffectivePalette());
			label.initText(globe->_game->getMod()->getFont("FONT_BIG"),
				globe->_game->getMod()->getFont("FONT_SMALL"), globe->_game->getLanguage());
			label.setAlign(ALIGN_CENTER);
			label.setColor(color);
			label.setText(text);
			label.blit(frame->getSurface());
			globe->_gpuState->_gpuLabelTextures.push_back({text, width, height, color,
				globe->_gpuState->_gpuLabelPaletteGeneration, frame, nullptr});
			found = &globe->_gpuState->_gpuLabelTextures.back();
		}
		globe->_gpuState->_gpuLabelIconPendingDraws.push_back({found, nullptr, x, y, 0});
	}

	void CalypsoGeoscapeHdGlobeDirect::recordLabelIcon(Globe* globe, Surface* frame,
		int x, int y, int shade)
	{
		if (!globe || !frame)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape city marker frame unavailable");
		if (globe->_gpuState->_gpuLabelIconPendingDraws.size() >= Globe::GPU_LABEL_DRAW_CAPACITY
			|| globe->_gpuState->_gpuLabelIconPendingDraws.size() >= globe->_gpuState->_gpuLabelIconPendingDraws.capacity())
		{
			globe->_gpuState->_gpuLabelCapacityExceeded = true;
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label batch capacity exhausted");
		}
		globe->_gpuState->_gpuLabelIconPendingDraws.push_back({nullptr, frame, x, y, shade});
	}

	void CalypsoGeoscapeHdGlobeDirect::ensureLogicalWorldComplete(Globe* globe)
	{
		if (!globe || globe->_gpuState->_gpuDebugCapacityExceeded)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				"Geoscape HD debug geometry batch capacity exhausted");
		if (!globe || !globe->_gpuState->_gpuLogicalWorldComplete)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				"Geoscape HD world incomplete: an unclaimed logical layer remains");
	}

	void CalypsoGeoscapeHdGlobeDirect::ensureBorderResources(Globe* globe)
	{
		if (!globe || !GpuInit::ready())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border GPU is unavailable");
		if (globe->_gpuState->_gpuBorderCapacityExceeded)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border batch capacity exhausted");
		if (globe->_gpuState->_gpuRadarFlightCapacityExceeded)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape radar/flight batch capacity exhausted");
		if (globe->_gpuState->_gpuDebugCapacityExceeded)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry batch capacity exhausted");
		if (globe->_gpuState->_gpuBorderLines.empty()
			&& globe->_gpuState->_coloredLineBatch.commandCount() == 0u
			&& globe->_gpuState->_gpuDebugLines.empty())
		{
			globe->_gpuState->_gpuBorderReady = true;
			return;
		}
		if (!globe->_gpuState->_borderShader)
		{
			globe->_gpuState->_borderShader = new Shader();
			if (!globe->_gpuState->_borderShader->loadFromEmbedded("colorquad"))
			{
				delete globe->_gpuState->_borderShader;
				globe->_gpuState->_borderShader = nullptr;
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border shader compilation failed");
			}
		}
		if (!globe->_gpuState->_borderShader->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border shader is invalid");
		if (!globe->_gpuState->_borderVAO || !globe->_gpuState->_borderVBO)
		{
			glGenVertexArrays(1, &globe->_gpuState->_borderVAO);
			glGenBuffers(1, &globe->_gpuState->_borderVBO);
			const GLenum resourceError = glGetError();
			if (!globe->_gpuState->_borderVAO || !globe->_gpuState->_borderVBO || resourceError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					resourceError == GL_NO_ERROR
						? "Geoscape border vertex resources unavailable"
						: calypsoGlFailure("Geoscape border vertex allocation failed", resourceError));
			glBindVertexArray(globe->_gpuState->_borderVAO);
			glBindBuffer(GL_ARRAY_BUFFER, globe->_gpuState->_borderVBO);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * (GLsizei)sizeof(float), (void*)0);
			const GLenum attributeError = glGetError();
			glBindVertexArray(0);
			const GLenum vertexUnbindError = glGetError();
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			const GLenum attributeUnbindError = glGetError();
			if (attributeError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape border vertex attribute setup failed", attributeError));
			if (vertexUnbindError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape border vertex array unbind failed", vertexUnbindError));
			if (attributeUnbindError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape border array buffer unbind failed", attributeUnbindError));
		}
		const size_t requiredBorderVertices = globe->_gpuState->_gpuBorderLines.size() * 2u;
		const size_t requiredDebugVertices = globe->_gpuState->_gpuDebugLines.size() * 2u;
		/* Radar/flight vertices live in the dedicated coloured-line batch since
		 * Phase 46.4 §15; the shared border buffer never resizes for them. */
		const size_t requiredVertices = std::max(requiredBorderVertices, requiredDebugVertices);
		if (requiredBorderVertices * 2u > Globe::GPU_BORDER_VERTEX_FLOAT_CAPACITY
			|| requiredDebugVertices * 2u > Globe::GPU_DEBUG_VERTEX_FLOAT_CAPACITY)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape world vertex capacity exhausted");
		if (requiredVertices > globe->_gpuState->_gpuBorderCapacity)
		{
			if (requiredVertices * 2u > Globe::GPU_BORDER_VERTEX_FLOAT_CAPACITY)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border vertex capacity exhausted");
			glBindBuffer(GL_ARRAY_BUFFER, globe->_gpuState->_borderVBO);
			glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(requiredVertices * 2u * sizeof(float)), nullptr, GL_DYNAMIC_DRAW);
			const GLenum bufferError = glGetError();
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			const GLenum bufferUnbindError = glGetError();
			if (bufferError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape border vertex buffer allocation failed", bufferError));
			if (bufferUnbindError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape border vertex buffer unbind failed", bufferUnbindError));
			globe->_gpuState->_gpuBorderCapacity = requiredVertices;
		}
		if (requiredDebugVertices > globe->_gpuState->_gpuDebugCapacity)
			globe->_gpuState->_gpuDebugCapacity = requiredDebugVertices;
		const GLenum borderPreflightError = glGetError();
		if (borderPreflightError != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				calypsoGlFailure("Geoscape border GL preflight failed", borderPreflightError));
		globe->_gpuState->_gpuBorderReady = true;
	}

	void CalypsoGeoscapeHdGlobeDirect::drawBorderPass(Globe* globe)
	{
		if (!globe || globe->_gpuState->_gpuBorderLines.empty() || !globe->_gpuState->_directScreen) return;
		if (!globe->_gpuState->_gpuBorderReady || !globe->_gpuState->_borderShader || !globe->_gpuState->_borderShader->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border resources disappeared");
		const double xs = globe->_gpuState->_directScreen->getXScale();
		const double ys = globe->_gpuState->_directScreen->getYScale();
		const float displayW = static_cast<float>(Options::displayWidth);
		const float displayH = static_cast<float>(Options::displayHeight);
		CalypsoGeoscapeHdGlobeDirect::PhysicalGlobeRect rect;
		if (!CalypsoGeoscapeHdGlobeDirect::physicalGlobeRect(globe, rect))
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape physical globe rect is invalid");
		const SDL_Color* palette = globe->getEffectivePalette();
		if (!palette)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border palette unavailable");
		const SDL_Color color = palette[Globe::LINE_COLOR];
		const size_t requiredVertexFloats = globe->_gpuState->_gpuBorderLines.size() * 4u;
		if (requiredVertexFloats > Globe::GPU_BORDER_VERTEX_FLOAT_CAPACITY
			|| requiredVertexFloats > globe->_gpuState->_gpuBorderVertices.capacity())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border vertex capacity exhausted");
		globe->_gpuState->_gpuBorderVertices.resize(requiredVertexFloats);
		size_t vertexIndex = 0;
		for (const auto& line : globe->_gpuState->_gpuBorderLines)
		{
			const float x1 = 2.0f * ((rect.x + line.x1 * (float)xs) / displayW) - 1.0f;
			const float x2 = 2.0f * ((rect.x + line.x2 * (float)xs) / displayW) - 1.0f;
			const float y1 = -(2.0f * ((rect.y + line.y1 * (float)ys) / displayH) - 1.0f);
			const float y2 = -(2.0f * ((rect.y + line.y2 * (float)ys) / displayH) - 1.0f);
			globe->_gpuState->_gpuBorderVertices[vertexIndex++] = x1;
			globe->_gpuState->_gpuBorderVertices[vertexIndex++] = y1;
			globe->_gpuState->_gpuBorderVertices[vertexIndex++] = x2;
			globe->_gpuState->_gpuBorderVertices[vertexIndex++] = y2;
		}
		/* Option A: state setup hoisted into the shared line-guard in drawPass. */
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(1.0f);
		globe->_gpuState->_borderShader->use();
		globe->_gpuState->_borderShader->setUniform4f("u_color", color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
		glBindBuffer(GL_ARRAY_BUFFER, globe->_gpuState->_borderVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(globe->_gpuState->_gpuBorderVertices.size() * sizeof(float)), globe->_gpuState->_gpuBorderVertices.data());
		glBindVertexArray(globe->_gpuState->_borderVAO);
		glDrawArrays(GL_LINES, 0, (GLsizei)(globe->_gpuState->_gpuBorderLines.size() * 2u));
		glBindVertexArray(0u);
		if (glGetError() != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape border draw failed");
	}

	/* Attribute 0 = vec2 position; attribute 1 = four normalised unsigned
	 * bytes taken from the locked portable vertex layout. Bound once at VAO
	 * creation so steady-state draws never touch vertex-array state. */
	static void enableColoredLineAttributes(Globe* globe)
	{
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
			(GLsizei)sizeof(OpenXcom::Calypso::CalypsoGeoscapeColoredLineVertex),
			(void*)OpenXcom::Calypso::COLORED_LINE_POSITION_OFFSET);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE,
			(GLsizei)sizeof(OpenXcom::Calypso::CalypsoGeoscapeColoredLineVertex),
			(void*)OpenXcom::Calypso::COLORED_LINE_COLOR_OFFSET);
	}

	void CalypsoGeoscapeHdGlobeDirect::ensureColoredLineResources(Globe* globe)
	{
		if (!globe || !GpuInit::ready())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape coloured-line GPU is unavailable");
		if (globe->_gpuState->_gpuRadarFlightCapacityExceeded)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape radar/flight batch capacity exhausted");
		if (!globe->_gpuState->_coloredLineShader)
		{
			globe->_gpuState->_coloredLineShader = new Shader();
			if (!globe->_gpuState->_coloredLineShader->loadFromEmbedded("geoscape_colored_lines"))
			{
				delete globe->_gpuState->_coloredLineShader;
				globe->_gpuState->_coloredLineShader = nullptr;
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape coloured-line shader compilation failed");
			}
		}
		if (!globe->_gpuState->_coloredLineShader->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape coloured-line shader is invalid");
		if (!globe->_gpuState->_coloredLineVAO || !globe->_gpuState->_coloredLineVBO)
		{
			glGenVertexArrays(1, &globe->_gpuState->_coloredLineVAO);
			glGenBuffers(1, &globe->_gpuState->_coloredLineVBO);
			const GLenum resourceError = glGetError();
			if (!globe->_gpuState->_coloredLineVAO || !globe->_gpuState->_coloredLineVBO || resourceError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					resourceError == GL_NO_ERROR
						? "Geoscape coloured-line vertex resources unavailable"
						: calypsoGlFailure("Geoscape coloured-line vertex allocation failed", resourceError));
			/* One fixed-capacity GPU allocation at creation time: steady-state
			 * frames never resize or reallocate storage, they only sub-update
			 * the committed prefix of the interleaved vertex buffer. */
			glBindBuffer(GL_ARRAY_BUFFER, globe->_gpuState->_coloredLineVBO);
			glBufferData(GL_ARRAY_BUFFER,
				(GLsizeiptr)(OpenXcom::Calypso::COLORED_LINE_VERTEX_CAPACITY
					* sizeof(OpenXcom::Calypso::CalypsoGeoscapeColoredLineVertex)),
				nullptr, GL_DYNAMIC_DRAW);
			const GLenum bufferError = glGetError();
			glBindVertexArray(globe->_gpuState->_coloredLineVAO);
			enableColoredLineAttributes(globe);
			glBindVertexArray(0u);
			const GLenum attributeUnbindError = glGetError();
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			const GLenum bufferUnbindError = glGetError();
			if (bufferError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape coloured-line buffer allocation failed", bufferError));
			if (attributeUnbindError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape coloured-line attribute setup failed", attributeUnbindError));
			if (bufferUnbindError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape coloured-line array buffer unbind failed", bufferUnbindError));
			globe->_gpuState->_coloredLineResourcesReady = true;
		}
	}

	void CalypsoGeoscapeHdGlobeDirect::ensureHoverLineResources(Globe* globe)
	{
		if (!globe || !GpuInit::ready())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape hover overlay GPU is unavailable");
		if (!globe->_gpuState->_hoverLineVAO || !globe->_gpuState->_hoverLineVBO)
		{
			glGenVertexArrays(1, &globe->_gpuState->_hoverLineVAO);
			glGenBuffers(1, &globe->_gpuState->_hoverLineVBO);
			const GLenum resourceError = glGetError();
			if (!globe->_gpuState->_hoverLineVAO || !globe->_gpuState->_hoverLineVBO || resourceError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					resourceError == GL_NO_ERROR
						? "Geoscape hover overlay vertex resources unavailable"
						: calypsoGlFailure("Geoscape hover overlay vertex allocation failed", resourceError));
			/* §16.5: fixed-capacity GPU allocation for the hover overlay,
			 * sized to HOVER_LINE_VERTEX_CAPACITY.  Steady-state frames
			 * never resize or reallocate; they only sub-update the buffer. */
			glBindBuffer(GL_ARRAY_BUFFER, globe->_gpuState->_hoverLineVBO);
			glBufferData(GL_ARRAY_BUFFER,
				(GLsizeiptr)(OpenXcom::Calypso::HOVER_LINE_VERTEX_CAPACITY
					* sizeof(OpenXcom::Calypso::CalypsoGeoscapeColoredLineVertex)),
				nullptr, GL_DYNAMIC_DRAW);
			const GLenum bufferError = glGetError();
			glBindVertexArray(globe->_gpuState->_hoverLineVAO);
			enableColoredLineAttributes(globe);
			glBindVertexArray(0u);
			const GLenum attributeUnbindError = glGetError();
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			const GLenum bufferUnbindError = glGetError();
			if (bufferError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape hover overlay buffer allocation failed", bufferError));
			if (attributeUnbindError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape hover overlay attribute setup failed", attributeUnbindError));
			if (bufferUnbindError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape hover overlay array buffer unbind failed", bufferUnbindError));
			globe->_gpuState->_hoverLineResourcesReady = true;
		}
	}

/* SS15.4.3: fold every dynamic campaign input that can change radar/flight
 * output into one deterministic POD signature. The walk is linear in the
 * number of relevant entities, allocates nothing, reads no rendered pixels,
 * and performs no projection or shade lookup; floating-point fields hash
 * their exact bit patterns. Fixed presentation inputs ride the key struct.
 * This complete key is the correctness backstop: existing direct mutation
 * hooks may invalidate early, but omitting one never leaves stale geometry. */
static std::uint64_t calypsoBuildRadarFlightSignature(SavedGame* save)
{
	Calypso::CalypsoGeoscapeColoredLineSignature sig;
	for (auto* xbase : *save->getBases())
	{
		sig.mixDouble(xbase->getLatitude());
		sig.mixDouble(xbase->getLongitude());
		/* Completed facility build state and radar range. */
		for (auto* fac : *xbase->getFacilities())
		{
			const bool completed = fac->getBuildTime() == 0;
			sig.mixBool(completed);
			if (completed)
				sig.mixDouble(fac->getRules()->getRadarRange());
		}
		/* Every relevant craft: status, position, radar range, destination,
		 * meet-calculated flag, and meet position. */
		for (auto* craft : *xbase->getCrafts())
		{
			const bool out = craft->getStatus() == "STR_OUT";
			sig.mixBool(out);
			if (!out)
				continue;
			sig.mixDouble(craft->getLongitude());
			sig.mixDouble(craft->getLatitude());
			sig.mixDouble(craft->getCraftStats().radarRange);
			sig.mixBool(craft->getDestination() != 0);
			if (craft->getDestination() != 0)
			{
				sig.mixDouble(craft->getDestination()->getLongitude());
				sig.mixDouble(craft->getDestination()->getLatitude());
			}
			sig.mixBool(craft->isMeetCalculated());
			if (craft->isMeetCalculated())
			{
				sig.mixDouble(craft->getMeetLongitude());
				sig.mixDouble(craft->getMeetLatitude());
			}
		}
	}
	/* Every relevant UFO: hunter/detection state, position, radar range,
	 * hunting state, and destination position. */
	for (auto* ufo : *save->getUfos())
	{
		sig.mixBool(ufo->getStatus() == Ufo::IGNORE_ME);
		sig.mixInt64((std::int64_t)ufo->getDetected());
		sig.mixBool(ufo->getHyperDetected());
		sig.mixBool(ufo->isHunterKiller());
		sig.mixBool(ufo->isHunting());
		sig.mixDouble(ufo->getLongitude());
		sig.mixDouble(ufo->getLatitude());
		sig.mixDouble(ufo->getCraftStats().radarRange);
		sig.mixBool(ufo->getDestination() != 0);
		if (ufo->getDestination() != 0)
		{
			sig.mixDouble(ufo->getDestination()->getLongitude());
			sig.mixDouble(ufo->getDestination()->getLatitude());
		}
	}
	/* Every discovered alien-base position and detection range. */
	for (auto* ab : *save->getAlienBases())
	{
		sig.mixBool(ab->isDiscovered());
		if (!ab->isDiscovered())
			continue;
		sig.mixDouble(ab->getLatitude());
		sig.mixDouble(ab->getLongitude());
		sig.mixDouble(ab->getDeployment()->getBaseDetectionRange());
	}
	return sig.value();
}

	bool CalypsoGeoscapeHdGlobeDirect::beginRadarFlightFrame(Globe* globe)
	{
		if (!globe || !globe->_gpuState->_directScreen)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape radar/flight preparation owner unavailable");
		CalypsoGeoscapeHdGlobeDirect::PhysicalGlobeRect rect;
		if (!CalypsoGeoscapeHdGlobeDirect::physicalGlobeRect(globe, rect))
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape physical globe rect is invalid");
		Calypso::CalypsoGeoscapeColoredLineSnapshotKey key = Calypso::CalypsoGeoscapeColoredLineSnapshotKey();
		key.viewportGeneration = 0u;
		key.rectX = rect.x;
		key.rectY = rect.y;
		key.rectW = rect.w;
		key.rectH = rect.h;
		key.displayWidth = Options::displayWidth;
		key.displayHeight = Options::displayHeight;
		key.sdlScaleX = globe->_gpuState->_directScreen->getXScale();
		key.sdlScaleY = globe->_gpuState->_directScreen->getYScale();
		key.centreLongitude = globe->_cenLon;
		key.centreLatitude = globe->_cenLat;
		key.zoomLevel = (double)globe->_zoom;
		key.globeRadius = globe->_zoomRadius[globe->_zoom];
		key.textureZoom = (double)globe->_zoomTexture;
		key.craftRangeEnabled = globe->_craft;
		key.craftLongitude = globe->_craftLon;
		key.craftLatitude = globe->_craftLat;
		key.craftRange = globe->_craftRange;
		key.optionRadarLines = Options::globeRadarLines;
		key.optionFlightPaths = Options::globeFlightPaths;
		key.optionAllRadarsOnBaseBuild = Options::globeAllRadarsOnBaseBuild;
		/* §16.5 review fix: hoverEnabled in the key so that enter/exit hover
		 * triggers one static radar/flight rebuild (drawRadars depends on
		 * _hover && globeAllRadarsOnBaseBuild).  Hover-coord movement alone
		 * stays a cache hit. */
		key.hoverEnabled = globe->_hover;
		key.paletteGeneration = globe->_gpuState->_gpuRadarPaletteGeneration;
		key.enemyRadarMode = (std::int64_t)globe->_game->getMod()->getDrawEnemyRadarCircles();
		key.debugMode = globe->_game->getSavedGame()->getDebugMode();
		key.dynamicSignature = calypsoBuildRadarFlightSignature(globe->_game->getSavedGame());
		const Calypso::ColoredLinePrepareResult verdict = globe->_gpuState->_coloredLineCache.prepare(key);
		if (Calypso::calypsoRadarCountersEnabled())
		{
			Calypso::CalypsoGeoscapeRadarCounters& counters = Calypso::calypsoRadarCounters();
			++counters.radarFingerprintChecks;
			if (verdict == Calypso::COLORED_LINE_CACHE_HIT) ++counters.radarCacheHits;
		}
		if (verdict != Calypso::COLORED_LINE_REBUILT)
			return true; /* Cache hit: packed vertices and uploaded VBO stay */
		             /* authoritative; skip ALL radar/flight CPU work.       */
		/* Miss: clear the batch so the owners below record a fresh snapshot. */
		globe->_gpuState->_coloredLineBatch.clearCommands();
		globe->_gpuState->_gpuRadarFlightCapacityExceeded = false;
		if (Calypso::calypsoRadarCountersEnabled()) ++Calypso::calypsoRadarCounters().radarRebuilds;
		return false;
	}

	void CalypsoGeoscapeHdGlobeDirect::finishRadarFlightFrame(Globe* globe, bool rebuilt)
	{
		if (!globe || !rebuilt) return; /* Cache-hit frame: nothing to pack. */
		CalypsoGeoscapeHdGlobeDirect::PhysicalGlobeRect rect;
		if (!CalypsoGeoscapeHdGlobeDirect::physicalGlobeRect(globe, rect))
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape physical globe rect is invalid");
		Calypso::CalypsoGeoscapeColoredLineViewport viewport;
		viewport.rectX = rect.x;
		viewport.rectY = rect.y;
		viewport.scaleX = globe->_gpuState->_directScreen->getXScale();
		viewport.scaleY = globe->_gpuState->_directScreen->getYScale();
		viewport.displayWidth = Options::displayWidth;
		viewport.displayHeight = Options::displayHeight;
		const Uint64 calypsoPackStart = Calypso::calypsoRadarCountersEnabled()
			? SDL_GetPerformanceCounter() : 0;
		const size_t vertices = globe->_gpuState->_coloredLineBatch.packVertices(viewport);
		if (vertices == static_cast<size_t>(-1))
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape radar/flight pack preflight exhausted capacity");
		if (calypsoPackStart)
			Calypso::calypsoRadarCounters().radarPrepareUs +=
				(Uint64)((SDL_GetPerformanceCounter() - calypsoPackStart) * 1000000ull / SDL_GetPerformanceFrequency());
		if (Calypso::calypsoRadarCountersEnabled())
		{
			Calypso::CalypsoGeoscapeRadarCounters& counters = Calypso::calypsoRadarCounters();
			counters.radarPreparedCommands += globe->_gpuState->_coloredLineBatch.commandCount();
			counters.radarPreparedVertices += vertices;
		}
	}

	void CalypsoGeoscapeHdGlobeDirect::commitLabelIconSnapshot(Globe* globe)
	{
		if (!globe || !globe->_gpuState->_gpuDirectMode) return;
		/* SS15.4.5: labels/icons publish exactly once per frame, here -- never
		 * as a drawFlights() side effect -- so a radar/flight cache hit can
		 * never freeze or erase label publication. */
		globe->_gpuState->_gpuLabelIconPendingDraws.swap(globe->_gpuState->_gpuLabelIconCommittedDraws);
		globe->_gpuState->_gpuLabelIconPendingDraws.clear();
	}

	void CalypsoGeoscapeHdGlobeDirect::drawRadarFlightPass(Globe* globe)
	{
		if (!globe || globe->_gpuState->_coloredLineBatch.vertexCount() == 0u || !globe->_gpuState->_directScreen) return;
		if (!globe->_gpuState->_coloredLineResourcesReady || !globe->_gpuState->_coloredLineShader
			|| !globe->_gpuState->_coloredLineShader->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape coloured-line resources disappeared");
		/* SS15.4.6: the single upload/draw boundary lives inside ONE shared
		 * state guard owned by drawPass (border+radar), so steady-state frames
		 * perform no capacity queries, no shader metadata lookups, and no
		 * redundant state snapshots here. */
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(1.0f);
		globe->_gpuState->_coloredLineShader->use();
		Calypso::CalypsoGeoscapeRadarCounters& counters = Calypso::calypsoRadarCounters();
		const bool instrumented = Calypso::calypsoRadarCountersEnabled();
		/* SS15.4.2: an unchanged snapshot performs zero uploads; a context
		 * restore reuploads the committed CPU snapshot exactly once. */
		if (!globe->_gpuState->_coloredLineCache.uploadCurrent())
		{
			const Uint64 uploadStart = instrumented ? SDL_GetPerformanceCounter() : 0;
			glBindBuffer(GL_ARRAY_BUFFER, globe->_gpuState->_coloredLineVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0,
				(GLsizeiptr)globe->_gpuState->_coloredLineBatch.packedVertexBytes(),
				globe->_gpuState->_coloredLineBatch.packedVertices());
			globe->_gpuState->_coloredLineCache.markUploaded();
			if (instrumented)
			{
				++counters.radarUploads;
				counters.radarUploadBytes += globe->_gpuState->_coloredLineBatch.packedVertexBytes();
				counters.radarUploadUs += (std::uint64_t)((SDL_GetPerformanceCounter() - uploadStart) * 1000000ull / SDL_GetPerformanceFrequency());
			}
		}
		const GLenum uploadError = glGetError();
		glBindVertexArray(globe->_gpuState->_coloredLineVAO);
		/* The literal contract: a non-empty snapshot is submitted by exactly
		 * one WebGL draw call. No colour-run scan remains. */
		glDrawArrays(GL_LINES, 0, (GLsizei)globe->_gpuState->_coloredLineBatch.vertexCount());
		glBindVertexArray(0u);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		const GLenum drawError = glGetError();
		if (instrumented)
			++counters.radarDrawCalls;
		if (uploadError != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				calypsoGlFailure("Geoscape coloured-line upload failed", uploadError));
		if (drawError != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				calypsoGlFailure("Geoscape coloured-line draw failed", drawError));
	}

	void CalypsoGeoscapeHdGlobeDirect::drawHoverPass(Globe* globe)
	{
		/* §16.5: hover owns a separate VAO/VBO. Geometry rebuild and upload
		 * readiness are independent: unchanged active geometry redraws the
		 * committed VBO without glBufferSubData. */
		if (!globe || !globe->_hover
			|| globe->_gpuState->_hoverLineBatch.vertexCount() == 0u
			|| !globe->_gpuState->_directScreen)
			return;
		if (!globe->_gpuState->_hoverLineResourcesReady
			|| !globe->_gpuState->_coloredLineShader
			|| !globe->_gpuState->_coloredLineShader->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				"Geoscape hover overlay resources disappeared");
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(1.0f);
		globe->_gpuState->_coloredLineShader->use();
		if (globe->_gpuState->_hoverLineUploadDirty)
		{
			glBindBuffer(GL_ARRAY_BUFFER, globe->_gpuState->_hoverLineVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0,
				(GLsizeiptr)globe->_gpuState->_hoverLineBatch.packedVertexBytes(),
				globe->_gpuState->_hoverLineBatch.packedVertices());
			const GLenum uploadError = glGetError();
			if (uploadError != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
					calypsoGlFailure("Geoscape hover overlay upload failed", uploadError));
			globe->_gpuState->_hoverLineUploadDirty = false;
		}
		glBindVertexArray(globe->_gpuState->_hoverLineVAO);
		glDrawArrays(GL_LINES, 0, (GLsizei)globe->_gpuState->_hoverLineBatch.vertexCount());
		glBindVertexArray(0u);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		const GLenum drawError = glGetError();
		if (drawError != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute(
				calypsoGlFailure("Geoscape hover overlay draw failed", drawError));
	}

	void CalypsoGeoscapeHdGlobeDirect::drawDebugPass(Globe* globe)
	{
		if (!globe || globe->_gpuState->_gpuDebugLines.empty() || !globe->_gpuState->_directScreen) return;
		if (!globe->_gpuState->_gpuBorderReady || !globe->_gpuState->_borderShader || !globe->_gpuState->_borderShader->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry resources disappeared");
		if (globe->_gpuState->_gpuDebugCapacityExceeded)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry batch capacity exhausted");
		const double xs = globe->_gpuState->_directScreen->getXScale();
		const double ys = globe->_gpuState->_directScreen->getYScale();
		const float displayW = static_cast<float>(Options::displayWidth);
		const float displayH = static_cast<float>(Options::displayHeight);
		PhysicalGlobeRect rect;
		if (!physicalGlobeRect(globe, rect))
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape physical globe rect is invalid");
		const SDL_Color* palette = globe->getEffectivePalette();
		if (!palette)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry palette unavailable");
		const size_t requiredVertexFloats = globe->_gpuState->_gpuDebugLines.size() * 4u;
		if (requiredVertexFloats > Globe::GPU_DEBUG_VERTEX_FLOAT_CAPACITY
			|| requiredVertexFloats > globe->_gpuState->_gpuDebugVertices.capacity())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry vertex capacity exhausted");
		globe->_gpuState->_gpuDebugVertices.resize(requiredVertexFloats);
		size_t vertexIndex = 0;
		for (const auto& line : globe->_gpuState->_gpuDebugLines)
		{
			globe->_gpuState->_gpuDebugVertices[vertexIndex++] = (float)(2.0 * ((rect.x + line.x1 * xs) / displayW) - 1.0);
			globe->_gpuState->_gpuDebugVertices[vertexIndex++] = (float)-(2.0 * ((rect.y + line.y1 * ys) / displayH) - 1.0);
			globe->_gpuState->_gpuDebugVertices[vertexIndex++] = (float)(2.0 * ((rect.x + line.x2 * xs) / displayW) - 1.0);
			globe->_gpuState->_gpuDebugVertices[vertexIndex++] = (float)-(2.0 * ((rect.y + line.y2 * ys) / displayH) - 1.0);
		}
		GlobeSphereGlSave st; st.save();
		setPhysicalGlobeClip(globe);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(1.0f);
		globe->_gpuState->_borderShader->use();
		glBindBuffer(GL_ARRAY_BUFFER, globe->_gpuState->_borderVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0,
			(GLsizeiptr)(globe->_gpuState->_gpuDebugVertices.size() * sizeof(float)), globe->_gpuState->_gpuDebugVertices.data());
		glBindVertexArray(globe->_gpuState->_borderVAO);
		size_t begin = 0;
		while (begin < globe->_gpuState->_gpuDebugLines.size())
		{
			const Uint8 colorIndex = globe->_gpuState->_gpuDebugLines[begin].color;
			size_t end = begin + 1;
			while (end < globe->_gpuState->_gpuDebugLines.size()
				&& globe->_gpuState->_gpuDebugLines[end].color == colorIndex) ++end;
			const SDL_Color color = palette[colorIndex];
			globe->_gpuState->_borderShader->setUniform4f("u_color", color.r / 255.0f, color.g / 255.0f,
				color.b / 255.0f, color.a / 255.0f);
			glDrawArrays(GL_LINES, (GLint)(begin * 2u), (GLsizei)((end - begin) * 2u));
			begin = end;
		}
		glBindVertexArray(0u);
		if (glGetError() != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape debug geometry draw failed");
		st.restore();
	}

	void CalypsoGeoscapeHdGlobeDirect::ensureMarkerResources(Globe* globe)
	{
		if (!globe || !GpuInit::ready())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker GPU is unavailable");
		if (!globe->_gpuState->_markerShader)
		{
			globe->_gpuState->_markerShader = new Shader();
			if (!globe->_gpuState->_markerShader->loadFromEmbedded("textured"))
			{
				delete globe->_gpuState->_markerShader;
				globe->_gpuState->_markerShader = nullptr;
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker shader compilation failed");
			}
		}
		if (!globe->_gpuState->_markerShader->isValid())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker shader is invalid");
		if (!globe->_gpuState->_markerVAO || !globe->_gpuState->_markerVBO)
		{
			glGenVertexArrays(1, &globe->_gpuState->_markerVAO);
			glGenBuffers(1, &globe->_gpuState->_markerVBO);
			if (!globe->_gpuState->_markerVAO || !globe->_gpuState->_markerVBO || glGetError() != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker vertex resources unavailable");
			glBindVertexArray(globe->_gpuState->_markerVAO);
			glBindBuffer(GL_ARRAY_BUFFER, globe->_gpuState->_markerVBO);
			glBufferData(GL_ARRAY_BUFFER, 6 * 4 * (GLsizeiptr)sizeof(float), nullptr, GL_DYNAMIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)(2 * sizeof(float)));
			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			if (glGetError() != GL_NO_ERROR)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker vertex setup failed");
		}
		globe->_gpuState->_gpuMarkerReady = true;
		for (const auto& command : globe->_gpuState->_gpuMarkerCommittedDraws)
		{
			if (!command.frame || command.frame->getWidth() <= 0 || command.frame->getHeight() <= 0
				|| !CalypsoGeoscapeHdGlobeDirect::markerTexture(globe, command.frame, command.shade))
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker texture upload failed");
		}
		if (glGetError() != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker GL preflight failed");
	}

	GpuTexture* CalypsoGeoscapeHdGlobeDirect::markerTexture(Globe* globe, Surface* frame, int shade)
	{
		for (const auto& entry : globe->_gpuState->_gpuMarkerTextures)
		{
			if (entry.frame == frame && entry.shade == shade
				&& entry.paletteGeneration == globe->_gpuState->_gpuMarkerPaletteGeneration
				&& entry.texture && entry.texture->isValid())
				return entry.texture;
		}
		const int w = frame->getWidth();
		const int h = frame->getHeight();
		if (w <= 0 || h <= 0) return nullptr;
		const ShadeTable* table = frame->getShadeTable();
		const Uint8* mirror = frame->getPaletteMirror();
		std::vector<uint8_t> rgba(static_cast<size_t>(w * h * 4), 0u);
		for (int py = 0; py < h; ++py)
		{
			for (int px = 0; px < w; ++px)
			{
				Uint32 argb = frame->getPixel32(px, py);
				if (mirror && table) argb = table->get(mirror[py * w + px], shade);
				const size_t i = static_cast<size_t>((py * w + px) * 4);
				rgba[i + 0] = static_cast<uint8_t>((argb >> 16) & 0xffu);
				rgba[i + 1] = static_cast<uint8_t>((argb >> 8) & 0xffu);
				rgba[i + 2] = static_cast<uint8_t>(argb & 0xffu);
				rgba[i + 3] = static_cast<uint8_t>((argb >> 24) & 0xffu);
			}
		}
		GpuTexture* texture = new GpuTexture(false, GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest);
		if (!texture->uploadRGBA(rgba.data(), w, h))
		{
			delete texture;
			return nullptr;
		}
		globe->_gpuState->_gpuMarkerTextures.push_back({frame, shade, globe->_gpuState->_gpuMarkerPaletteGeneration, texture});
		return texture;
	}

	GpuTexture* CalypsoGeoscapeHdGlobeDirect::labelTexture(Globe* globe, Calypso::CalypsoGlobeGpuState::LabelTexture& entry)
	{
		if (!globe || !entry.frame || entry.width <= 0 || entry.height <= 0)
			return nullptr;
		if (entry.texture && entry.paletteGeneration == globe->_gpuState->_gpuLabelPaletteGeneration
			&& entry.texture->isValid())
			return entry.texture;
		const int w = entry.frame->getWidth();
		const int h = entry.frame->getHeight();
		if (w <= 0 || h <= 0) return nullptr;
		std::vector<uint8_t> rgba(static_cast<size_t>(w * h * 4), 0u);
		for (int py = 0; py < h; ++py)
		{
			for (int px = 0; px < w; ++px)
			{
				const Uint32 argb = entry.frame->getPixel32(px, py);
				const size_t i = static_cast<size_t>((py * w + px) * 4);
				rgba[i + 0] = static_cast<uint8_t>((argb >> 16) & 0xffu);
				rgba[i + 1] = static_cast<uint8_t>((argb >> 8) & 0xffu);
				rgba[i + 2] = static_cast<uint8_t>(argb & 0xffu);
				rgba[i + 3] = static_cast<uint8_t>((argb >> 24) & 0xffu);
			}
		}
		GpuTexture* texture = new (std::nothrow) GpuTexture(false,
			GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest);
		if (!texture || !texture->uploadRGBA(rgba.data(), w, h))
		{
			delete texture;
			return nullptr;
		}
		entry.texture = texture;
		entry.paletteGeneration = globe->_gpuState->_gpuLabelPaletteGeneration;
		return texture;
	}

	void CalypsoGeoscapeHdGlobeDirect::ensureLabelResources(Globe* globe)
	{
		if (!globe || !GpuInit::ready())
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label GPU is unavailable");
		if (globe->_gpuState->_gpuLabelCapacityExceeded)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label batch capacity exhausted");
		if (globe->_gpuState->_gpuLabelIconCommittedDraws.empty()) return;
		CalypsoGeoscapeHdGlobeDirect::ensureMarkerResources(globe);
		for (auto& command : globe->_gpuState->_gpuLabelIconCommittedDraws)
		{
			if (command.label)
			{
				if (!CalypsoGeoscapeHdGlobeDirect::labelTexture(globe, *command.label))
					Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label texture upload failed");
			}
			else if (!command.frame || command.frame->getWidth() <= 0 || command.frame->getHeight() <= 0
				|| !CalypsoGeoscapeHdGlobeDirect::markerTexture(globe, command.frame, command.shade))
			{
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape city marker texture upload failed");
			}
		}
		if (glGetError() != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label GL preflight failed");
	}

	void CalypsoGeoscapeHdGlobeDirect::drawLabelIconPass(Globe* globe)
	{
		if (!globe || globe->_gpuState->_gpuLabelIconCommittedDraws.empty() || !globe->_gpuState->_directScreen) return;
		ensureLabelResources(globe);
		const double xs = globe->_gpuState->_directScreen->getXScale();
		const double ys = globe->_gpuState->_directScreen->getYScale();
		const int lbb = globe->_gpuState->_directScreen->getCursorLeftBlackBand();
		const int tbb = globe->_gpuState->_directScreen->getCursorTopBlackBand();
		const float displayW = static_cast<float>(Options::displayWidth);
		const float displayH = static_cast<float>(Options::displayHeight);
		GlobeSphereGlSave st; st.save();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		CalypsoGeoscapeHdGlobeDirect::setPhysicalGlobeClip(globe);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		globe->_gpuState->_markerShader->use();
		globe->_gpuState->_markerShader->setUniform1f("u_darken", 0.0f);
		globe->_gpuState->_markerShader->setUniform1i("u_tex", 0);
		for (const auto& command : globe->_gpuState->_gpuLabelIconCommittedDraws)
		{
			Surface* frame = command.label ? command.label->frame : command.frame;
			GpuTexture* texture = command.label
				? labelTexture(globe, *command.label)
				: markerTexture(globe, command.frame, command.shade);
			if (!frame || !texture)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label texture disappeared during draw");
			const float x = static_cast<float>((globe->getX() + command.x) * xs + lbb);
			const float y = static_cast<float>((globe->getY() + command.y) * ys + tbb);
			const float w = static_cast<float>(frame->getWidth() * xs);
			const float h = static_cast<float>(frame->getHeight() * ys);
			const float x0 = 2.0f * x / displayW - 1.0f;
			const float x1 = 2.0f * (x + w) / displayW - 1.0f;
			const float y0 = -(2.0f * y / displayH - 1.0f);
			const float y1 = -(2.0f * (y + h) / displayH - 1.0f);
			const float verts[6 * 4] = {x0, y0, 0.f, 0.f, x1, y0, 1.f, 0.f,
				x0, y1, 0.f, 1.f, x0, y1, 0.f, 1.f, x1, y0, 1.f, 0.f,
				x1, y1, 1.f, 1.f};
			glBindBuffer(GL_ARRAY_BUFFER, globe->_gpuState->_markerVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(verts), verts);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			texture->bind(0);
			glBindVertexArray(globe->_gpuState->_markerVAO);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glBindVertexArray(0);
		}
		if (glGetError() != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape label draw failed");
		st.restore();
	}

	void CalypsoGeoscapeHdGlobeDirect::drawMarkerPass(Globe* globe)
	{
		if (!globe || globe->_gpuState->_gpuMarkerCommittedDraws.empty() || !globe->_gpuState->_directScreen) return;
		CalypsoGeoscapeHdGlobeDirect::ensureMarkerResources(globe);
		const double xs = globe->_gpuState->_directScreen->getXScale();
		const double ys = globe->_gpuState->_directScreen->getYScale();
		const int lbb = globe->_gpuState->_directScreen->getCursorLeftBlackBand();
		const int tbb = globe->_gpuState->_directScreen->getCursorTopBlackBand();
		const float displayW = static_cast<float>(Options::displayWidth);
		const float displayH = static_cast<float>(Options::displayHeight);
		GlobeSphereGlSave st; st.save();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		CalypsoGeoscapeHdGlobeDirect::setPhysicalGlobeClip(globe);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		globe->_gpuState->_markerShader->use();
		globe->_gpuState->_markerShader->setUniform1f("u_darken", 0.0f);
		globe->_gpuState->_markerShader->setUniform1i("u_tex", 0);
		for (const auto& command : globe->_gpuState->_gpuMarkerCommittedDraws)
		{
			GpuTexture* texture = markerTexture(globe, command.frame, command.shade);
			if (!texture)
				Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker texture disappeared during draw");
			const float x = static_cast<float>((globe->getX() + command.x) * xs + lbb);
			const float y = static_cast<float>((globe->getY() + command.y) * ys + tbb);
			const float w = static_cast<float>(command.frame->getWidth() * xs);
			const float h = static_cast<float>(command.frame->getHeight() * ys);
			const float x0 = 2.0f * x / displayW - 1.0f;
			const float x1 = 2.0f * (x + w) / displayW - 1.0f;
			const float y0 = -(2.0f * y / displayH - 1.0f);
			const float y1 = -(2.0f * (y + h) / displayH - 1.0f);
			const float verts[6 * 4] = {x0, y0, 0.f, 0.f, x1, y0, 1.f, 0.f, x0, y1, 0.f, 1.f, x0, y1, 0.f, 1.f, x1, y0, 1.f, 0.f, x1, y1, 1.f, 1.f};
			glBindBuffer(GL_ARRAY_BUFFER, globe->_gpuState->_markerVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(verts), verts);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			texture->bind(0);
			glBindVertexArray(globe->_gpuState->_markerVAO);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glBindVertexArray(0);
		}
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0u);
		if (glGetError() != GL_NO_ERROR)
			Calypso::CalypsoHdUiOverlay::instance().failHdRoute("Geoscape marker draw failed");
		st.restore();
	}

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
