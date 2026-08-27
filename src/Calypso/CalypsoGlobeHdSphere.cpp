#ifdef __EMSCRIPTEN__
/* Guard R3: HD sphere GPU path (initSphereGPU/drawHDStarfield/drawSphereGPU/
 * getSunDirectionWorld) extracted from Geoscape/Globe.cpp. Whole-file
 * emscripten TU; empty native. */
#include "CalypsoGlobeHdSphere.h"
#include "../Geoscape/Globe.h"
#include "CalypsoGeoscapeHdGlobeDirect.h"
#include "CalypsoGeoscapeQaPresentation.h"
#include "CalypsoGeoscapeColoredLineBatch.h"
#include "CalypsoSdlCompositeBoundary.h"
#include "../Engine/GpuInit.h"
#include "../Engine/GpuTexture.h"
#include "../Engine/Logger.h"
#include "../Engine/Options.h"
#include "../Engine/Shader.h"
#include "../Engine/ShaderManager.h"
#include "../Mod/Mod.h"
#include "../Interface/Text.h"
#include "../Savegame/SavedGame.h"
#include <SDL.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

extern "C" int g_calypsoProfileGlobe;

namespace OpenXcom {
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

/* Stage 13 QA (loopback-only): shared seam helpers for BOTH globe passes
 * (readback Globe::drawSphereGPU() and direct drawPass()). With every control
 * at its production default each helper returns its input unchanged, so the
 * live paths keep their exact existing math and texture bindings. */

/* Effective decorative milliseconds for the shader clocks (`u_time`): a
 * frozen or reduced-motion capture row replaces ONLY the millisecond source
 * through calypsoGeoscapeQaPresentationSeconds() (live seconds =
 * SDL_GetTicks() * 0.001); the callers' uniform expressions keep their exact
 * production form. */
static float calypsoGlobeQaEffectiveMs(float liveMs)
{
	const auto& qa = Calypso::calypsoGeoscapeQaPresentation();
	if (!qa.frozenClock && !qa.reducedMotion) return liveMs;
	return (float)(Calypso::calypsoGeoscapeQaPresentationSeconds(qa, liveMs * 0.001) * 1000.0);
}

/* GeoscapeQaVec3 -> shader-world Cord conversion for the deterministic
 * day/night rows produced by calypsoGeoscapeQaSunDirection(). Live rows never
 * reach this helper; they keep the verbatim getSunDirectionWorld() call. */
static Cord calypsoGlobeQaCord(const Calypso::GeoscapeQaVec3& v)
{
	return Cord(v.x, v.y, v.z);
}

	
bool calypsoGlobeInitSphereGPU(OpenXcom::Globe& globe)
{
	if (!GpuInit::ready()) return false;

	if (!globe._gpuState->_globeShader || !globe._gpuState->_globeShader->isValid())
	{
		delete globe._gpuState->_globeShader;
		globe._gpuState->_globeShader = new Shader();
		if (!globe._gpuState->_globeShader->loadFromEmbedded("globe_sphere"))
		{
			Log(LOG_ERROR) << "Globe::initSphereGPU: shader compile failed";
			delete globe._gpuState->_globeShader; globe._gpuState->_globeShader = nullptr;
			return false;
		}
	}

	/* Fullscreen-quad VAO (NDC -1..+1, UV 0..1). */
	float verts[] = {
		-1.f,-1.f, 0.f,0.f,   1.f,-1.f, 1.f,0.f,  -1.f, 1.f, 0.f,1.f,
		-1.f, 1.f, 0.f,1.f,   1.f,-1.f, 1.f,0.f,   1.f, 1.f, 1.f,1.f,
	};
	GLuint vbo = 0u;
	glGenVertexArrays(1, &globe._gpuState->_sphereVAO);
	glBindVertexArray(globe._gpuState->_sphereVAO);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
	/* VBO is owned by the VAO after bind; no need to keep a separate handle. */

	/* FBO + colour attachment (same size as globe surface). */
	int w = 0, h = 0; CalypsoGeoscapeHdGlobeDirect::computeSphereRes(&globe, w, h);
	glGenTextures(1, &globe._gpuState->_sphereFBOTex);
	glBindTexture(GL_TEXTURE_2D, globe._gpuState->_sphereFBOTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0u);

	glGenFramebuffers(1, &globe._gpuState->_sphereFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, globe._gpuState->_sphereFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, globe._gpuState->_sphereFBOTex, 0);
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0u);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		Log(LOG_ERROR) << "Globe::initSphereGPU: FBO incomplete (status=" << (int)status << ")";
		return false;
	}
	if (glGetError() != GL_NO_ERROR || !globe._gpuState->_sphereVAO || !globe._gpuState->_sphereFBO || !globe._gpuState->_sphereFBOTex)
	{
		Log(LOG_ERROR) << "Globe::initSphereGPU: Earth GL resources unavailable";
		return false;
	}

	globe._gpuState->_gpuSphereOK = true;
	Log(LOG_INFO) << "Globe::initSphereGPU: ready (" << w << "x" << h << ")";

	/* M6: register a ShaderManager reset callback so a real WebGL context
	 * loss (GPU crash, iOS tab switch) is handled correctly.  On restore,
	 * reuploadAll() re-compiles the shaders/textures; this callback nulls raw
	 * GL handles and clears both pass-ready flags so the next frame rebuilds
	 * the sphere and marker overlay without stale commands. */
	if (!globe._gpuState->_gpuAliveFlag) globe._gpuState->_gpuAliveFlag = std::make_shared<bool>(true);
	if (!globe._gpuState->_gpuResetCallbackRegistered)
	{
	ShaderManager::instance().registerResetCallback(globe._gpuState->_gpuAliveFlag, [&globe]() {
		globe._gpuState->_sphereVAO    = 0u;
		globe._gpuState->_sphereFBO    = 0u;
		globe._gpuState->_sphereFBOTex = 0u;
		globe._gpuState->_gpuSphereOK  = false;
		globe._gpuState->_markerVAO    = 0u;
		globe._gpuState->_markerVBO    = 0u;
		globe._gpuState->_gpuMarkerReady = false;
		globe._gpuState->_borderVAO    = 0u;
		globe._gpuState->_borderVBO    = 0u;
		globe._gpuState->_gpuBorderReady = false;
		globe._gpuState->_gpuBorderCapacity = 0u;
		globe._gpuState->_gpuDebugCapacity = 0u;
		globe._gpuState->_coloredLineVAO = 0u;
		globe._gpuState->_coloredLineVBO = 0u;
		globe._gpuState->_coloredLineResourcesReady = false;
		globe._gpuState->_coloredLineCache.notifyContextReset();
		/* §16.5 review fix: reset hover overlay GL handles and dirty state so
		 * context restore re-creates the hover VAO/VBO and rebuilds geometry
		 * on the next hover frame instead of using stale GL handles. */
		globe._gpuState->_hoverLineVAO = 0u;
		globe._gpuState->_hoverLineVBO = 0u;
		globe._gpuState->_hoverLineResourcesReady = false;
		globe._gpuState->_hoverOverlayDirty = true;
		globe._gpuState->_gpuBorderCapacityExceeded = false;
		globe._gpuState->_gpuDebugCapacityExceeded = false;
		globe._gpuState->_gpuRadarFlightCapacityExceeded = false;
		for (auto& entry : globe._gpuState->_gpuMarkerTextures) delete entry.texture;
		globe._gpuState->_gpuMarkerTextures.clear();
		++globe._gpuState->_gpuMarkerPaletteGeneration;
		for (auto& entry : globe._gpuState->_gpuLabelTextures)
		{
			delete entry.texture;
			entry.texture = nullptr;
		}
		globe._gpuState->_gpuLabelCapacityExceeded = false;
		/* Keep the committed command snapshot. Context restore invalidates raw
		 * resources only; gameplay-owned commands remain the source for the
		 * first rebuilt physical frame.  Hover batch is transient (refilled
		 * every frame), so clear it to prevent stale vertex draw after reset. */
		globe._gpuState->_hoverLineBatch.clearCommands();
		});
		globe._gpuState->_gpuResetCallbackRegistered = true;
	}

	return true;
}

/**
 * Sun direction in the fixed world frame the GPU shader uses.
 * World frame: Y = north pole, X = +90° lon (east), Z = 0° lon (prime meridian).
 * This is independent of the observer position — unlike getSunDirection(lon, lat)
 * which returns a camera-relative vector.
 */
OpenXcom::Cord calypsoGlobeSunDirectionWorld(const OpenXcom::Globe& globe)
{
	const double rot = globe._game->getSavedGame()->getTime()->getDaylight() * 2*M_PI;
	double decl = 0;
	if (Options::globeSeasons)
	{
		const int MonthDays1[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
		const int MonthDays2[] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366};

		int year  = globe._game->getSavedGame()->getTime()->getYear();
		int month = globe._game->getSavedGame()->getTime()->getMonth()-1;
		int day   = globe._game->getSavedGame()->getTime()->getDay()-1;

		double tm = (double)((globe._game->getSavedGame()->getTime()->getHour() * 60
			+ globe._game->getSavedGame()->getTime()->getMinute()) * 60
			+ globe._game->getSavedGame()->getTime()->getSecond()) / 86400.0;

		double CurDay;
		if (year%4 == 0 && !(year%100 == 0 && year%400 != 0))
			CurDay = (MonthDays2[month] + day + tm)/366 - 0.219;
		else
			CurDay = (MonthDays1[month] + day + tm)/365 - 0.219;
		if (CurDay < 0) CurDay += 1.;

		decl = -0.261 * sin(CurDay * 2*M_PI);
	}
	// Subsolar point lon = π/2 − rot, lat = decl.
	// getDaylight()=0 corresponds to 6h GMT (sub-solar at 90° E), daylight=0.25
	// is noon at Greenwich (sub-solar at 0°), so the offset from rot is +π/2.
	const double sunLon = M_PI / 2.0 - rot;
	return Cord(cos(decl) * sin(sunLon),
	            sin(decl),
	            cos(decl) * cos(sunLon));
}

void calypsoGlobeDrawHDStarfield(OpenXcom::Globe& globe)
{
	if (!globe.isARGB()) return;

	const int w = globe.getWidth();
	const int h = globe.getHeight();
	const double globeLimit = (globe._zoomRadius[globe._zoom] + 5.0) * (globe._zoomRadius[globe._zoom] + 5.0);

	globe.lock();
	for (int y = 0; y < h; ++y)
	{
		const float t = (h > 1) ? (float)y / (float)(h - 1) : 0.f;
		const Uint8 r = (Uint8)(1 + t * 2);
		const Uint8 g = (Uint8)(5 + t * 9);
		const Uint8 b = (Uint8)(17 + t * 18);
		const Uint32 bg = 0xFF000000u | ((Uint32)r << 16) | ((Uint32)g << 8) | (Uint32)b;
		for (int x = 0; x < w; ++x)
		{
			globe.setPixel32(x, y, bg);
		}
	}

	/* Deterministic sparse stars: bright enough to give the globe a space
	 * setting, sparse enough to avoid fighting Geoscape labels and markers.
	 *
	 * Stage 13 QA (loopback-only): freeze/reduced-motion replace ONLY the
	 * twinkle clock's millisecond source (liveSeconds = SDL_GetTicks()*0.001,
	 * resolved through calypsoGeoscapeQaPresentationSeconds(); twinkleTime is
	 * that value scaled by 1.7). With defaults the expression below stays
	 * exactly the production SDL_GetTicks() * 0.0017f math. */
	float twinkleMs = (float)SDL_GetTicks();
	const auto& qa = Calypso::calypsoGeoscapeQaPresentation();
	if (qa.frozenClock || qa.reducedMotion)
		twinkleMs = (float)(Calypso::calypsoGeoscapeQaPresentationSeconds(qa, twinkleMs * 0.001) * 1000.0);
	const float twinkleTime = twinkleMs * 0.0017f;
	for (unsigned i = 0; i < 125; ++i)
	{
		unsigned n = i * 747796405u + 2891336453u;
		n = ((n >> ((n >> 28u) + 4u)) ^ n) * 277803737u;
		n = (n >> 22u) ^ n;
		const int x = (int)(n % (unsigned)w);
		const int y = (int)((n / (unsigned)w) % (unsigned)h);
		const double dx = (double)x - (double)globe._cenX;
		const double dy = (double)y - (double)globe._cenY;
		if (dx * dx + dy * dy < globeLimit) continue;

		const float phase = (float)((n >> 8u) & 0xFFu) * 0.024543693f;
		const float pulse = 0.62f + 0.38f * (0.5f + 0.5f * sinf(twinkleTime + phase));
		const Uint8 v = (Uint8)((100 + (n & 0x7Fu)) * pulse);
		const Uint32 star = 0xFF000000u
			| ((Uint32)(v * 78 / 100) << 16)
			| ((Uint32)(v * 92 / 100) << 8)
			| (Uint32)v;
		globe.setPixel32(x, y, star);
		if ((n & 0x0Fu) == 0 && x + 1 < w) globe.setPixel32(x + 1, y, star);
		if ((n & 0x1Fu) == 0 && y + 1 < h) globe.setPixel32(x, y + 1, star);
	}
	globe.unlock();
}

/**
 * Renders the HD sphere using the GPU shader pipeline and reads the pixels
 * back into this Surface so the existing CPU overlay (polylines, markers,
 * text) can be composited on top in the same Globe::draw() call.
 *
 * Performance: glReadPixels for the globe surface is ~0.2–1 ms on typical
 * hardware; acceptable for a 60 fps Geoscape.
 */
void calypsoGlobeDrawSphereGPU(OpenXcom::Globe& globe)
{
	if (!globe._gpuState->_gpuSphereOK && !initSphereGPU()) return;
	if (globe._gpuState->_gpuDirectMode)
	{
		/* Direct mode is owned by Screen's registered world slot. The slot
		 * performs Earth resource preflight and the ordered draw after SDL's
		 * composite, so do not paint an early duplicate here. */
		return;
	}

	Mod* mod = globe._game->getMod();
	GpuTexture* bathyTex   = mod->getGlobeTexture("bathymetry");
	GpuTexture* diffuseTex = mod->getGlobeTexture("diffuse");
	GpuTexture* nightTex   = mod->getGlobeTexture("night");
	GpuTexture* cloudsTex  = mod->getGlobeTexture("clouds");
	if (!bathyTex || !diffuseTex || !nightTex || !cloudsTex) return;

	/* Stage 13 QA (loopback-only): loopback capture controls; every field
	 * defaults to the production inputs consumed below. */
	const auto& qa = Calypso::calypsoGeoscapeQaPresentation();

	int w = 0, h = 0; CalypsoGeoscapeHdGlobeDirect::computeSphereRes(&globe, w, h);

	/* Phase 8c.10 perf instrumentation: wall-clock GPU pass time.  ENTIRELY
	 * gated on ::g_calypsoProfileGlobe — when the flag is 0 (production
	 * default) the GpuTimer object is never constructed and steady_clock is
	 * never read, so the path costs one int load + one branch-not-taken.
	 * Sampled at the local level instead of Screen::registerGPUPass because
	 * Globe's draw cycle does FBO render + glReadPixels synchronously into
	 * globe._surface; restructuring would have been disproportionate. */
	const int profileGlobe = ::g_calypsoProfileGlobe;
	GpuTimer perfTimer;
	if (profileGlobe) perfTimer.start();

	GlobeSphereGlSave st; st.save();

	/* Render sphere to FBO. */
	glBindFramebuffer(GL_FRAMEBUFFER, globe._gpuState->_sphereFBO);
	glViewport(0, 0, w, h);
	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	globe._gpuState->_globeShader->use();

	bathyTex ->bind(0);
	diffuseTex->bind(1);
	nightTex ->bind(2);
	/* Stage 13 QA (loopback-only): capture rows may bind the persistent
	 * transparent 1x1 input; production always samples the mod clouds
	 * texture, and an upload failure falls back to it identically. */
	GpuTexture* cloudsBound = cloudsTex;
	if (qa.cloudMode == Calypso::GeoscapeQaCloudMode::Hidden)
	{
		if (!calypsoGlobeQaHiddenCloudsTexture()->isValid()) cloudsBound = cloudsTex;
		else cloudsBound = calypsoGlobeQaHiddenCloudsTexture();
	}
	cloudsBound->bind(3);
	globe._gpuState->_globeShader->setUniform1i("u_bathymetry", 0);
	globe._gpuState->_globeShader->setUniform1i("u_diffuse",    1);
	globe._gpuState->_globeShader->setUniform1i("u_night",      2);
	globe._gpuState->_globeShader->setUniform1i("u_clouds",     3);
	globe._gpuState->_globeShader->setUniform1i("u_background", 0);

	/* Viewport and globe geometry. */
	globe._gpuState->_globeShader->setUniform2f("u_viewportSize", (float)w, (float)h);
	globe._gpuState->_globeShader->setUniform2f("u_globeCenter",  (float)globe._cenX, (float)globe._cenY);
	globe._gpuState->_globeShader->setUniform1f("u_globeRadius",  (float)globe._zoomRadius[globe._zoom]);
	globe._gpuState->_globeShader->setUniform1f("u_camLat",       (float)globe._cenLat);
	globe._gpuState->_globeShader->setUniform1f("u_camLon",       (float)globe._cenLon);

	/* Sun direction in world frame (8c.5 fix: was camera-relative, now world frame). */
	Cord sd = getSunDirectionWorld();
	/* Stage 13 QA (loopback-only): deterministic day/night rows replace the
	 * fed value only; campaign time is never read or mutated. */
	if (qa.sunMode != Calypso::GeoscapeQaSunMode::Live)
		sd = calypsoGlobeQaCord(Calypso::calypsoGeoscapeQaSunDirection(qa.sunMode, globe._cenLon, globe._cenLat));
	globe._gpuState->_globeShader->setUniform3f("u_sunDir", (float)sd.x, (float)sd.y, (float)sd.z);

	/* Cloud drift time. */
	float timeMs = calypsoGlobeQaEffectiveMs((float)SDL_GetTicks());
	globe._gpuState->_globeShader->setUniform1f("u_time", timeMs * 0.001f);

	/* Mip level curve: keep the overview detailed enough that land does not
	 * read as a low-res smear; the globe is small, but 1k mips are too soft. */
	float mipLvl = std::max(0.f, std::min(1.35f, 1.35f - (float)globe._zoom * 0.27f));
	globe._gpuState->_globeShader->setUniform1f("u_mipLevel", mipLvl);

	glBindVertexArray(globe._gpuState->_sphereVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0u);

	/* Read back RGBA pixels from FBO; FBO rows are bottom-up, SDL is top-down. */
	std::vector<uint8_t> rgba((size_t)w * h * 4);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

	/* Unbind our textures from units 0..3 and reset the active unit to 0.
	 * SDL2's renderer reuses these units for SDL_Texture rendering and would
	 * otherwise pick up our globe textures, blasting them across the canvas
	 * (sphere shader output is overridden by raw bathymetry on UI blits). */
	for (int i = 3; i >= 0; --i) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, 0u);
	}

	st.restore();

	/* Convert RGBA (GL) → ARGB8888 (SDL little-endian) and flip Y.
	 * SDL_PIXELFORMAT_ARGB8888 memory layout: byte0=B, byte1=G, byte2=R, byte3=A. */
	globe.lock();
	uint8_t* dst   = reinterpret_cast<uint8_t*>(globe.getSurface()->pixels);
	int      pitch = globe.getSurface()->pitch;
	for (int y = 0; y < h; ++y)
	{
		const uint8_t* src = rgba.data() + (size_t)(h - 1 - y) * w * 4;
		uint8_t*       row = dst + y * pitch;
		for (int x = 0; x < w; ++x)
		{
			const uint8_t a = src[x*4 + 3];
			if (a == 0) continue; // discarded by shader — preserve starfield
			if (a == 255)
			{
				row[x*4 + 0] = src[x*4 + 2]; /* B */
				row[x*4 + 1] = src[x*4 + 1]; /* G */
				row[x*4 + 2] = src[x*4 + 0]; /* R */
				row[x*4 + 3] = 255;
			}
			else
			{
				const int inv = 255 - a;
				row[x*4 + 0] = (uint8_t)((src[x*4 + 2] * a + row[x*4 + 0] * inv) / 255);
				row[x*4 + 1] = (uint8_t)((src[x*4 + 1] * a + row[x*4 + 1] * inv) / 255);
				row[x*4 + 2] = (uint8_t)((src[x*4 + 0] * a + row[x*4 + 2] * inv) / 255);
				row[x*4 + 3] = 255;
			}
		}
	}
	globe.unlock();

	/* Perf log is opt-in via JS-side calypso_set_profile_globe(1)
	 * (EmscriptenHarness).  Production builds never call the setter so
	 * g_calypsoProfileGlobe stays 0, perfTimer was never started, and
	 * the entire branch below is skipped — zero clock reads, zero
	 * accumulator math, zero log output. */
	if (profileGlobe)
	{
		perfTimer.stop();
		static long long s_accumUs = 0;
		static unsigned  s_frameCount = 0;
		s_accumUs += perfTimer.elapsedUs();
		const unsigned BATCH = 30u;
		if (++s_frameCount >= BATCH)
		{
			Log(LOG_INFO) << "Globe::drawSphereGPU avg: "
			              << (s_accumUs / (long long)s_frameCount) << " us/frame"
			              << " (" << w << "x" << h << ", n=" << s_frameCount
			              << ", readback included)";
			s_accumUs    = 0;
			s_frameCount = 0;
		}
	}
}
void calypsoGlobeDrawHoverCircles(OpenXcom::Globe& globe)
{
	if (!globe._hover || !globe._gpuState->_gpuDirectMode || !globe._gpuState->_activeLineBatch) return;

	/* Lazy-init: precompute canonical ranges from the facility list once.
	 * The list is constant after game load so this never reallocates. */
	if (!globe._gpuState->_hoverRangesReady)
	{
		globe._gpuState->_hoverCanonicalRanges.clear();
		for (auto& facType : globe._game->getMod()->getBaseFacilitiesList())
			globe._gpuState->_hoverCanonicalRanges.push_back(
				Nautical(globe._game->getMod()->getBaseFacility(facType)->getRadarRange()));
		const size_t distinctRanges = globe._gpuState->_hoverCanonicalRanges.empty()
			? 0u
			: OpenXcom::Calypso::calypsoCanonicalizeHoverRanges(
				&globe._gpuState->_hoverCanonicalRanges[0], globe._gpuState->_hoverCanonicalRanges.size());
		globe._gpuState->_hoverCanonicalRanges.resize(distinctRanges);
		globe._gpuState->_hoverRangesReady = true;
	}

	/* §16.5 review fix: dirty/key gate — skip clear+fill+pack when observable
	 * inputs are unchanged.  The committed VBO is redrawn by drawHoverPass()
	 * without touching CPU geometry.  The force-dirty flag is set on the first
	 * frame and after context reset. */
	CalypsoGeoscapeHdGlobeDirect::PhysicalGlobeRect rect;
	if (!CalypsoGeoscapeHdGlobeDirect::physicalGlobeRect(&globe, rect))
		return;
	const double sx = globe._gpuState->_directScreen->getXScale();
	const double sy = globe._gpuState->_directScreen->getYScale();
	const int dw = Options::displayWidth;
	const int dh = Options::displayHeight;
	if (!globe._gpuState->_hoverOverlayDirty
		&& globe._gpuState->_lastHoverOverlayLon == globe._hoverLon
		&& globe._gpuState->_lastHoverOverlayLat == globe._hoverLat
		&& globe._gpuState->_lastHoverOverlayRectX == rect.x
		&& globe._gpuState->_lastHoverOverlayRectY == rect.y
		&& globe._gpuState->_lastHoverOverlayRectW == rect.w
		&& globe._gpuState->_lastHoverOverlayRectH == rect.h
		&& globe._gpuState->_lastHoverOverlayScaleX == sx
		&& globe._gpuState->_lastHoverOverlayScaleY == sy
		&& globe._gpuState->_lastHoverOverlayDisplayW == dw
		&& globe._gpuState->_lastHoverOverlayDisplayH == dh)
		return; /* All inputs unchanged: skip rebuild, draw committed VBO. */

	globe._gpuState->_lastHoverOverlayLon = globe._hoverLon;
	globe._gpuState->_lastHoverOverlayLat = globe._hoverLat;
	globe._gpuState->_lastHoverOverlayRectX = rect.x;
	globe._gpuState->_lastHoverOverlayRectY = rect.y;
	globe._gpuState->_lastHoverOverlayRectW = rect.w;
	globe._gpuState->_lastHoverOverlayRectH = rect.h;
	globe._gpuState->_lastHoverOverlayScaleX = sx;
	globe._gpuState->_lastHoverOverlayScaleY = sy;
	globe._gpuState->_lastHoverOverlayDisplayW = dw;
	globe._gpuState->_lastHoverOverlayDisplayH = dh;
	globe._gpuState->_hoverOverlayDirty = false;

	globe._gpuState->_activeLineBatch->clearCommands();
	for (size_t j = 0; j < globe._gpuState->_hoverCanonicalRanges.size(); ++j)
		globe.drawGlobeCircle(globe._hoverLat, globe._hoverLon, globe._gpuState->_hoverCanonicalRanges[j], 48);
	/* Pack the freshly recorded hover commands into the interleaved vertex
	 * buffer for GPU upload in drawHoverPass. */
	Calypso::CalypsoGeoscapeColoredLineViewport viewport;
	viewport.rectX = rect.x;
	viewport.rectY = rect.y;
	viewport.scaleX = sx;
	viewport.scaleY = sy;
	viewport.displayWidth = dw;
	viewport.displayHeight = dh;
	globe._gpuState->_activeLineBatch->packVertices(viewport);
}
void calypsoGlobeHoverOverlayFrame(Globe& globe)
{
	// §16.5: hover circles live in the separate overlay batch; when inactive,
	// clear so drawHoverPass() never draws stale circles.
	if (globe._hover)
	{
		globe._gpuState->_activeLineBatch = &globe._gpuState->_hoverLineBatch;
		globe.drawHoverCircles();
		globe._gpuState->_activeLineBatch = nullptr;
	}
	else
	{
		globe._gpuState->_hoverLineBatch.clearCommands();
		globe._gpuState->_hoverOverlayDirty = true;
	}
}

} // namespace OpenXcom
#endif // __EMSCRIPTEN__
