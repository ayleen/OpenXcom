/* Disposable Phase-42 G0: sparse RGBA overlay on the real Battlescape unit path. */
#if defined(__EMSCRIPTEN__) && defined(CALYPSO_HD_UNIT_SPIKE)

#include "HdUnitBattleSpike.h"
#include "../Engine/Game.h"
#include "../Engine/GpuTexture.h"
#include "../Engine/Logger.h"
#include "../Engine/Screen.h"
#include "../Engine/Surface.h"
#include "../Engine/ShadeTable.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/Tile.h"
#include "../Battlescape/BattlescapeState.h"
#include "../Battlescape/Map.h"
#include "../Battlescape/Camera.h"
#include "../Battlescape/Pathfinding.h"
#include "../Battlescape/BattlescapeGame.h"
#include "../Mod/Armor.h"
#include "../Mod/MapDataSet.h"

#include <GLES3/gl3.h>
#include <SDL.h>
#include <SDL_image.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

namespace OpenXcom
{
namespace
{
struct Sample
{
	std::string checkpoint, kind;
	int unitId = -1, direction = -1, sequence = -1, frame = -1, drawSerial = 0;
	float priority = 0.0f;
	bool overlay = false;
};

struct Rect { float x = 0, y = 0, w = 0, h = 0; };

struct ForegroundGeometry
{
	Rect rect;
	const Surface *source = nullptr;
	int shade = 0, partPriority = 0;
	float depthPriority = 0.0f;
};

struct OverlayGeometry
{
	Rect rect;
	int frame = -1;
	float depthPriority = 0.0f;
};

struct PixelProof
{
	bool available = false, passed = false;
	std::string reason = "no exact opaque source-pixel intersection was emitted";
	int screenX = -1, screenY = -1, framebufferX = -1, framebufferY = -1;
	int overlaySourceX = -1, overlaySourceY = -1, overlayFrame = -1;
	int foregroundSourceX = -1, foregroundSourceY = -1;
	int overlayR = 0, overlayG = 0, overlayB = 0, overlayA = 0;
	int foregroundPaletteIndex = 0, foregroundShade = 0;
	int expectedR = 0, expectedG = 0, expectedB = 0, expectedA = 0;
	int framebufferR = 0, framebufferG = 0, framebufferB = 0, framebufferA = 0;
	float overlayPriority = 0.0f, foregroundPriority = 0.0f;
	int tolerance = 1;
};

static bool overlaps(const Rect &a, const Rect &b)
{
	return a.x < b.x + b.w && b.x < a.x + a.w
	    && a.y < b.y + b.h && b.y < a.y + a.h;
}

struct Probe
{
	Game *game = nullptr;
	Mod::UnitAtlasSpec *spec = nullptr;
	std::string assetPath, metricsPath, checkpoint = "activated", error;
	std::unique_ptr<GpuTexture> atlas;
	std::vector<Sample> samples;
	std::vector<int> bodyFrames, handobFrames;
	int overlayHits = 0, fallbackHits = 0, reloadCount = 0;
	int foregroundOccluderCandidates = 0;
	int selectedUnitId = -1;
	Position selectedPosition;
	std::string screenshotPath;
	size_t screenshotBytes = 0;
	int autoSelectedUnitId = -1;
	Position autoSelectedPosition;
	std::vector<Rect> foregroundRects, overlayRects;
	std::vector<ForegroundGeometry> foregroundGeometry;
	std::vector<OverlayGeometry> overlayGeometry;
	std::vector<uint8_t> overlayPixels;
	int overlayAtlasW = 0, overlayAtlasH = 0;
	PixelProof pixelProof;
	bool orderPreserved = true;
	int drawSerial = 0;
	GLenum glError = GL_NO_ERROR;

	static bool decode(const std::string &path, int &w, int &h,
	                   std::vector<uint8_t> &pixels, std::string &why)
	{
		SDL_RWops *rw = SDL_RWFromFile(path.c_str(), "rb");
		if (!rw) { why = std::string("SDL_RWFromFile: ") + SDL_GetError(); return false; }
		SDL_Surface *raw = IMG_Load_RW(rw, SDL_TRUE);
		if (!raw) { why = std::string("IMG_Load_RW: ") + IMG_GetError(); return false; }
		SDL_Surface *rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
		SDL_FreeSurface(raw);
		if (!rgba) { why = std::string("SDL_ConvertSurfaceFormat: ") + SDL_GetError(); return false; }
		w = rgba->w; h = rgba->h;
		pixels.resize((size_t)w * (size_t)h * 4u);
		if (SDL_MUSTLOCK(rgba) && SDL_LockSurface(rgba) != 0)
		{
			why = std::string("SDL_LockSurface: ") + SDL_GetError();
			SDL_FreeSurface(rgba); return false;
		}
		for (int y = 0; y < h; ++y)
		{
			const uint8_t *src = static_cast<const uint8_t *>(rgba->pixels) + (size_t)y * rgba->pitch;
			std::copy(src, src + (size_t)w * 4u, pixels.begin() + (size_t)y * (size_t)w * 4u);
		}
		if (SDL_MUSTLOCK(rgba)) SDL_UnlockSurface(rgba);
		SDL_FreeSurface(rgba);
		return true;
	}

	bool upload()
	{
		int w = 0, h = 0;
		if (!decode(assetPath, w, h, overlayPixels, error)) return false;
		if (w != 1024 || h != 1440)
		{
			error = "G0 overlay must exactly match the 16x18 64x80 TDXCOM_0 probe atlas (1024x1440)";
			return false;
		}
		overlayAtlasW = w; overlayAtlasH = h;
		if (!atlas->uploadRGBA(overlayPixels.data(), w, h, 0))
		{
			error = "G0 RGBA upload failed"; return false;
		}
		return true;
	}

	void detach()
	{
		if (spec && spec->g0OverlayAtlas == atlas.get())
		{
			spec->g0OverlayAtlas = nullptr;
			spec->g0OverlayMask.clear();
		}
		spec = nullptr;
	}
};

std::unique_ptr<Probe> g_probe;

static std::string escapeJson(const std::string &s)
{
	std::ostringstream o;
	for (char c : s)
	{
		if (c == '\\' || c == '"') o << '\\' << c;
		else if (c == '\n') o << "\\n";
		else if ((unsigned char)c >= 0x20) o << c;
	}
	return o.str();
}

static void appendInts(std::ostringstream &o, const std::vector<int> &v)
{
	o << '[';
	for (size_t i = 0; i < v.size(); ++i) { if (i) o << ','; o << v[i]; }
	o << ']';
}

static void writeMetrics(Probe &p)
{
	std::sort(p.bodyFrames.begin(), p.bodyFrames.end());
	p.bodyFrames.erase(std::unique(p.bodyFrames.begin(), p.bodyFrames.end()), p.bodyFrames.end());
	std::sort(p.handobFrames.begin(), p.handobFrames.end());
	p.handobFrames.erase(std::unique(p.handobFrames.begin(), p.handobFrames.end()), p.handobFrames.end());
	const bool actualFrameHit = p.overlayHits > 0;
	const bool handob = !p.handobFrames.empty();
	const bool mixed = p.overlayHits > 0 && p.fallbackHits > 0;
	const bool foregroundCandidateGeometry = p.foregroundOccluderCandidates > 0;
	const bool foregroundPixelProof = p.pixelProof.passed;
	const bool gl0 = p.glError == GL_NO_ERROR;
	const bool screenshot = p.screenshotBytes > 0;
	std::ostringstream o;
	o << "{\n  \"probeOnly\":true,\n  \"probeCellSize\":{\"width\":64,\"height\":80},\n"
	  << "  \"overlayHits\":" << p.overlayHits << ",\n  \"fallbackHits\":" << p.fallbackHits
	  << ",\n  \"bodyFrames\":"; appendInts(o, p.bodyFrames);
	o << ",\n  \"handobFrames\":"; appendInts(o, p.handobFrames);
	o << ",\n  \"foregroundOccluderCandidates\":" << p.foregroundOccluderCandidates
	  << ",\n  \"autoSelectedUnitId\":" << p.autoSelectedUnitId
	  << ",\n  \"autoSelectedPosition\":{\"x\":" << p.autoSelectedPosition.x
	  << ",\"y\":" << p.autoSelectedPosition.y << ",\"z\":" << p.autoSelectedPosition.z << "}"
	  << ",\n  \"reloadCount\":" << p.reloadCount
	  << ",\n  \"cachedMirrorBytes\":" << (p.atlas ? p.atlas->debugCachedBytes() : 0)
	  << ",\n  \"glError\":" << (unsigned)p.glError << ",\n  \"orderSamples\":[";
	for (size_t i = 0; i < p.samples.size(); ++i)
	{
		const Sample &s = p.samples[i]; if (i) o << ',';
		o << "{\"checkpoint\":\"" << escapeJson(s.checkpoint) << "\",\"unitId\":" << s.unitId
		  << ",\"direction\":" << s.direction << ",\"sequence\":" << s.sequence
		  << ",\"drawSerial\":" << s.drawSerial
		  << ",\"kind\":\"" << s.kind << "\",\"frame\":" << s.frame
		  << ",\"priority\":" << s.priority << ",\"overlay\":" << (s.overlay ? "true" : "false") << '}';
	}
	o << "],\n  \"assertions\":{\"actualFrameHit\":" << (actualFrameHit ? "true" : "false")
	  << ",\"actualHandobObserved\":" << (handob ? "true" : "false")
	  << ",\"orderPreserved\":" << (p.orderPreserved ? "true" : "false")
	  << ",\"mixedFallback\":" << (mixed ? "true" : "false")
	  << ",\"foregroundCandidateGeometry\":" << (foregroundCandidateGeometry ? "true" : "false")
	  << ",\"foregroundPixelProof\":" << (foregroundPixelProof ? "true" : "false")
	  << ",\"gl0\":" << (gl0 ? "true" : "false")
	  << ",\"screenshot\":" << (screenshot ? "true" : "false") << "},\n"
	  << "  \"foregroundPixelProof\":{\"method\":\"cpu-source-intersection-plus-webgl-fbo-readback\","
	     "\"version\":1,\"available\":" << (p.pixelProof.available ? "true" : "false")
	  << ",\"passed\":" << (p.pixelProof.passed ? "true" : "false")
	  << ",\"reason\":\"" << escapeJson(p.pixelProof.reason) << "\""
	  << ",\"screen\":{\"x\":" << p.pixelProof.screenX << ",\"y\":" << p.pixelProof.screenY << "}"
	  << ",\"framebuffer\":{\"x\":" << p.pixelProof.framebufferX << ",\"y\":" << p.pixelProof.framebufferY
	  << ",\"rgba\":[" << p.pixelProof.framebufferR << ',' << p.pixelProof.framebufferG << ','
	  << p.pixelProof.framebufferB << ',' << p.pixelProof.framebufferA << "]}"
	  << ",\"overlaySource\":{\"frame\":" << p.pixelProof.overlayFrame
	  << ",\"x\":" << p.pixelProof.overlaySourceX << ",\"y\":" << p.pixelProof.overlaySourceY
	  << ",\"rgba\":[" << p.pixelProof.overlayR << ',' << p.pixelProof.overlayG << ','
	  << p.pixelProof.overlayB << ',' << p.pixelProof.overlayA << "]}"
	  << ",\"foregroundSource\":{\"x\":" << p.pixelProof.foregroundSourceX
	  << ",\"y\":" << p.pixelProof.foregroundSourceY
	  << ",\"paletteIndex\":" << p.pixelProof.foregroundPaletteIndex
	  << ",\"shade\":" << p.pixelProof.foregroundShade
	  << ",\"expectedRgba\":[" << p.pixelProof.expectedR << ',' << p.pixelProof.expectedG << ','
	  << p.pixelProof.expectedB << ',' << p.pixelProof.expectedA << "]}"
	  << ",\"depth\":{\"overlayPriority\":" << p.pixelProof.overlayPriority
	  << ",\"foregroundPriority\":" << p.pixelProof.foregroundPriority
	  << ",\"foregroundStrictlyInFront\":"
	  << (p.pixelProof.foregroundPriority > p.pixelProof.overlayPriority ? "true" : "false") << "}"
	  << ",\"rgbTolerance\":" << p.pixelProof.tolerance << "},\n"
	  << "  \"screenshotPath\":\"" << escapeJson(p.screenshotPath) << "\",\n"
	  << "  \"screenshotBytes\":" << p.screenshotBytes << ",\n"
	  << "  \"error\":\"" << escapeJson(p.error) << "\"\n}\n";
	std::ofstream f(p.metricsPath, std::ios::binary | std::ios::trunc);
	if (f) f << o.str();
}
}

bool HdUnitBattleSpike::active() { return g_probe && g_probe->spec && g_probe->atlas; }

bool HdUnitBattleSpike::activate(Game *game, const char *assetPath, const char *metricsPath)
{
	if (!game || !game->getMod()) return false;
	if (g_probe) { g_probe->detach(); g_probe.reset(); }
	auto p = std::make_unique<Probe>();
	p->game = game;
	p->assetPath = assetPath ? assetPath : "/tmp/hd-unit-battle-g0.png";
	p->metricsPath = metricsPath ? metricsPath : "/tmp/hd-unit-battle-g0.json";
	const Mod::UnitAtlasSpec *found = game->getMod()->getUnitAtlas("TDXCOM_0.PCK");
	if (!found) found = game->getMod()->getUnitAtlas("TDXCOM_0");
	if (!found || !found->atlas || found->atlasW != 1024 || found->atlasH != 1440
	 || found->tileWidth != 64 || found->tileHeight != 80 || found->columns != 16)
	{
		p->error = "live TDXCOM_0 64x80 GPU atlas is not ready"; writeMetrics(*p); return false;
	}
	p->spec = const_cast<Mod::UnitAtlasSpec *>(found);
	p->atlas = std::make_unique<GpuTexture>(true, GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest);
	p->atlas->setSkipCache(true);
	if (!p->upload()) { writeMetrics(*p); return false; }
	Probe *raw = p.get();
	p->atlas->setReloadCb([raw]() { if (raw->upload()) ++raw->reloadCount; });
	p->atlas->evictGL();
	p->atlas->reupload();
	if (!p->atlas->isValid() || p->reloadCount != 1 || p->atlas->debugCachedBytes() != 0)
	{
		p->error = "skip-cache forced reload failed"; writeMetrics(*p); return false;
	}
	p->spec->g0OverlayMask.assign(286, 0u);
	const int frames[] = {22,30,38,184,192,200,187,195,203,191,199,207,238,246,259};
	for (int frame : frames) p->spec->g0OverlayMask[(size_t)frame] = 1u;
	p->spec->g0OverlayAtlas = p->atlas.get();
	g_probe = std::move(p);
	Log(LOG_INFO) << "[HD-UNIT-G0] active: real TDXCOM_0 overlay installed";
	return true;
}

int HdUnitBattleSpike::select(Game *game, int unitId, int activeHand, int aiming, bool center)
{
	if (!active() || !game || !game->getSavedGame()) return 0;
	SavedBattleGame *save = game->getSavedGame()->getSavedBattle();
	if (!save) return 0;
	BattleUnit *unit = nullptr;
	if (unitId == -2)
	{
		for (BattleUnit *candidate : *save->getUnits())
		{
			if (!candidate || candidate->getId() <= 0 || !candidate->getVisible()
			 || candidate->isOut() || !candidate->getArmor()) continue;
			const std::string &sheet = candidate->getArmor()->getSpriteSheet();
			if (sheet != "TDXCOM_0" && sheet != "TDXCOM_0.PCK") continue;
			Tile *tile = save->getTile(candidate->getPosition());
			if (!tile || !tile->getSprite(O_OBJECT) || tile->isBackTileObject(O_OBJECT)) continue;
			unit = candidate;
			break;
		}
		if (!unit)
		{
			g_probe->error = "no visible TDXCOM_0 unit with a live same-cell front object";
			writeMetrics(*g_probe);
			return 0;
		}
		g_probe->autoSelectedUnitId = unit->getId();
		g_probe->autoSelectedPosition = unit->getPosition();
	}
	else
	{
		for (BattleUnit *candidate : *save->getUnits())
			if (candidate && candidate->getId() == unitId) { unit = candidate; break; }
	}
	if (!unit) return 0;
	save->setSelectedUnit(unit);
	g_probe->selectedUnitId = unit->getId();
	g_probe->selectedPosition = unit->getPosition();
	g_probe->foregroundRects.clear();
	g_probe->overlayRects.clear();
	g_probe->foregroundGeometry.clear();
	g_probe->overlayGeometry.clear();
	if (unitId != -2)
	{
		if (activeHand == 0) unit->setActiveRightHand();
		else if (activeHand == 1) unit->setActiveLeftHand();
		if (aiming >= 0) unit->aim(aiming != 0);
	}
	if (center)
	{
		if (auto *battle = dynamic_cast<BattlescapeState *>(game->getTopState()))
			battle->getMap()->getCamera()->centerOnPosition(unit->getPosition());
	}
	return unitId == -2 ? unit->getId() : 1;
}

bool HdUnitBattleSpike::findOccluderTarget(Game *game, int unitId, const char *outJson)
{
	const std::string path = outJson ? outJson : "/tmp/hd-unit-occluder-target.json";
	auto fail = [&](const std::string &why) {
		std::ofstream f(path, std::ios::binary | std::ios::trunc);
		if (f) f << "{\"ok\":false,\"error\":\"" << escapeJson(why) << "\"}\n";
		return false;
	};
	if (!active() || !game || !game->getSavedGame()) return fail("probe or game unavailable");
	SavedBattleGame *save = game->getSavedGame()->getSavedBattle();
	auto *battle = dynamic_cast<BattlescapeState *>(game->getTopState());
	if (!save || !battle || !battle->getMap()) return fail("battlescape unavailable");
	BattleUnit *unit = nullptr;
	for (BattleUnit *candidate : *save->getUnits())
		if (candidate && candidate->getId() == unitId) { unit = candidate; break; }
	if (!unit || unit != save->getSelectedUnit() || !unit->getVisible()
	 || unit->isOut() || !unit->getArmor()) return fail("requested unit is not the live visible selection");
	const std::string &sheet = unit->getArmor()->getSpriteSheet();
	if (sheet != "TDXCOM_0" && sheet != "TDXCOM_0.PCK") return fail("selected unit is not TDXCOM_0");
	BattlescapeGame *battleGame = battle->getBattleGame();
	if (!battleGame || battleGame->isBusy()) return fail("battle busy; retry when idle");
	if (battleGame->getReservedAction() != BA_NONE || save->getKneelReserved())
		return fail("TU reservation active; reachable click would not be deterministic");
	// Use an isolated scratch pathfinder so ordinary hover/preview paths in the
	// live Battlescape UI are neither overwritten nor aborted by this probe.
	Pathfinding scratch(save);
	Pathfinding *pf = &scratch;

	bool found = false;
	Position best;
	int bestCost = 1000000, bestEnergy = 0, bestSteps = 0, bestFinalDirection = -1;
	const Position origin = unit->getPosition();
	for (int y = 0; y < save->getMapSizeY(); ++y)
	for (int x = 0; x < save->getMapSizeX(); ++x)
	{
		Position target(x, y, origin.z);
		if (target == origin) continue;
		Tile *tile = save->getTile(target);
		const Surface *objectSource = tile ? tile->getSprite(O_OBJECT) : nullptr;
		if (!objectSource || tile->isBackTileObject(O_OBJECT)
		 || !objectSource->getShadeTable()) continue;
		// The closing proof is specifically a vanilla palette/R8 object. Reject
		// RGBA-only atlas groups here instead of discovering after a long walk
		// that recordForegroundOccluder quite correctly ignored the draw.
		int mcdId = -1, mdsId = -1;
		tile->getMapData(&mcdId, &mdsId, O_OBJECT);
		auto *sets = save->getMapDataSets();
		if (!sets || mdsId < 0 || mdsId >= (int)sets->size()) continue;
		const Mod::TileAtlasSpec *tileSpec = game->getMod()->getTileAtlasSpec((*sets)[mdsId]->getName());
		if (!tileSpec || tileSpec->baseline == Mod::BaselineMode::None
		 || !game->getMod()->getTileAtlas((*sets)[mdsId]->getName())
		 || (!tileSpec->hybrid && tileSpec->format != Mod::TileAtlasSpec::Format::Palette)) continue;
		bool opaqueVanillaTexel = false;
		for (int sy = 0; sy < objectSource->getHeight() && !opaqueVanillaTexel; ++sy)
		for (int sx = 0; sx < objectSource->getWidth(); ++sx)
		{
			const Uint8 paletteIndex = objectSource->getPixel(sx, sy);
			if (paletteIndex && ((objectSource->getShadeTable()->get(paletteIndex, 0) >> 24) & 0xffu) == 255u)
			{
				opaqueVanillaTexel = true;
				break;
			}
		}
		if (!opaqueVanillaTexel) continue;
		// Direction 6 always emits torso frame 38. Its G0 cell is a deliberately
		// opaque, original-art proof backdrop, so every projected opaque object
		// texel inside the shared 64x80 cell has an alpha-255 RGBA counterpart.
		// The live capture still repeats the exact CPU texel test and must match
		// the R8 shade-table colour through glReadPixels before the gate passes.
		const int proofFrame = 38;
		if (!g_probe || g_probe->overlayPixels.empty()
		 || g_probe->overlayAtlasW <= 0 || g_probe->overlayAtlasH <= 0) continue;
		bool proofCellFullyOpaque = true;
		const int proofOx = (proofFrame % 16) * 64, proofOy = (proofFrame / 16) * 80;
		for (int py = 0; py < 80 && proofCellFullyOpaque; ++py)
		for (int px = 0; px < 64; ++px)
		{
			const size_t oi = ((size_t)(proofOy + py) * (size_t)g_probe->overlayAtlasW
			                 + (size_t)(proofOx + px)) * 4u;
			if (g_probe->overlayPixels[oi + 3] != 255) { proofCellFullyOpaque = false; break; }
		}
		if (!proofCellFullyOpaque) continue;
		pf->calculate(unit, target, BAM_NORMAL, nullptr, unit->getTimeUnits());
		const int cost = pf->getTotalTUCost();
		const int energy = pf->getTotalEnergyCost();
		const int steps = (int)pf->getPath().size();
		const int finalDirection = steps > 0 ? pf->getPath().front() : -1;
		pf->abortPath();
		if (steps <= 0 || cost <= 0
		 || cost > unit->getTimeUnits() || energy > unit->getEnergy()) continue;
		if (!found || cost < bestCost)
		{
			found = true; best = target; bestCost = cost; bestEnergy = energy;
			bestSteps = steps; bestFinalDirection = finalDirection;
		}
	}
	if (!found) return fail("no reachable live front-object tile within current TU budget");

	Camera *camera = battle->getMap()->getCamera();
	if (camera->getViewLevel() != best.z) return fail("reachable target is not on the current camera level");
	Position raw, offset = camera->getMapOffset();
	camera->convertMapToScreen(best, &raw);
	const int expectedX = raw.x + offset.x + 32;
	const int expectedY = raw.y + offset.y + 20;
	int clickX = expectedX, clickY = expectedY;
	bool clickFound = false;
	for (int radius = 0; radius <= 48 && !clickFound; ++radius)
	for (int dy = -radius; dy <= radius && !clickFound; ++dy)
	for (int dx = -radius; dx <= radius; ++dx)
	{
		if (std::max(std::abs(dx), std::abs(dy)) != radius) continue;
		int mx = -1, my = -1;
		camera->convertScreenToMap(expectedX + dx, expectedY + dy + 20, &mx, &my);
		if (mx == best.x && my == best.y)
		{
			clickX = expectedX + dx; clickY = expectedY + dy; clickFound = true; break;
		}
	}
	if (!clickFound) return fail("reachable target has no verified current-camera click point");
	const int viewportW = battle->getMap()->getWidth();
	const int viewportH = battle->getMap()->getHeight();
	if (clickX < 0 || clickX >= viewportW || clickY < 0 || clickY >= viewportH)
	{
		camera->centerOnPosition(best);
		std::ofstream pending(path, std::ios::binary | std::ios::trunc);
		if (pending)
			pending << "{\"ok\":false,\"error\":\"camera centering\",\"cameraCenterRequested\":true"
			        << ",\"unitId\":" << unitId
			        << ",\"target\":{\"x\":" << best.x << ",\"y\":" << best.y << ",\"z\":" << best.z << "}}\n";
		return false;
	}
	// A natural right-click on the direction-6 neighbour turns the unit after
	// walking onto the object. This keeps the fixed sparse overlay cell opaque
	// without mutating BattleUnit direction from the harness.
	Position faceTarget = best;
	Position faceStep;
	Pathfinding::directionToVector(6, &faceStep);
	faceTarget += faceStep;
	if (!save->getTile(faceTarget)) return fail("direction-6 facing tile is outside the map");
	Position faceRaw;
	camera->convertMapToScreen(faceTarget, &faceRaw);
	const int faceExpectedX = faceRaw.x + offset.x + 32;
	const int faceExpectedY = faceRaw.y + offset.y + 20;
	int faceClickX = faceExpectedX, faceClickY = faceExpectedY;
	bool faceClickFound = false;
	for (int radius = 0; radius <= 48 && !faceClickFound; ++radius)
	for (int dy = -radius; dy <= radius && !faceClickFound; ++dy)
	for (int dx = -radius; dx <= radius; ++dx)
	{
		if (std::max(std::abs(dx), std::abs(dy)) != radius) continue;
		int mx = -1, my = -1;
		camera->convertScreenToMap(faceExpectedX + dx, faceExpectedY + dy + 20, &mx, &my);
		if (mx == faceTarget.x && my == faceTarget.y)
		{
			faceClickX = faceExpectedX + dx; faceClickY = faceExpectedY + dy;
			faceClickFound = true; break;
		}
	}
	if (!faceClickFound || faceClickX < 0 || faceClickX >= viewportW
	 || faceClickY < 0 || faceClickY >= viewportH)
		return fail("direction-6 facing click is not visible after camera centering");
	std::ofstream f(path, std::ios::binary | std::ios::trunc);
	if (!f) return false;
	f << "{\"ok\":true,\"unitId\":" << unitId
	  << ",\"logicalSize\":{\"width\":" << viewportW << ",\"height\":" << viewportH << "}"
	  << ",\"target\":{\"x\":" << best.x << ",\"y\":" << best.y << ",\"z\":" << best.z << "}"
	  << ",\"clickBase\":{\"x\":" << clickX << ",\"y\":" << clickY << "}"
	  << ",\"faceDirection6ClickBase\":{\"x\":" << faceClickX << ",\"y\":" << faceClickY << "}"
	  << ",\"pathSteps\":" << bestSteps << ",\"finalDirection\":" << bestFinalDirection
	  << ",\"proofEligibility\":{\"vanillaR8\":true,\"foregroundOpaqueTexel\":true"
	     ",\"overlayFrame\":38,\"overlayCellFullyOpaque\":true}"
	  << ",\"tuCost\":" << bestCost
	  << ",\"energyCost\":" << bestEnergy << "}\n";
	return true;
}

bool HdUnitBattleSpike::findWalkTarget(Game *game, int unitId, const char *outJson)
{
	const std::string path = outJson ? outJson : "/tmp/hd-unit-walk-target.json";
	auto fail = [&](const std::string &why) {
		std::ofstream f(path, std::ios::binary | std::ios::trunc);
		if (f) f << "{\"ok\":false,\"error\":\"" << escapeJson(why) << "\"}\n";
		return false;
	};
	if (!active() || !game || !game->getSavedGame()) return fail("probe or game unavailable");
	SavedBattleGame *save = game->getSavedGame()->getSavedBattle();
	auto *battle = dynamic_cast<BattlescapeState *>(game->getTopState());
	if (!save || !battle || !battle->getMap()) return fail("battlescape unavailable");
	BattleUnit *unit = nullptr;
	for (BattleUnit *candidate : *save->getUnits())
		if (candidate && candidate->getId() == unitId) { unit = candidate; break; }
	if (!unit || unit != save->getSelectedUnit() || !unit->getVisible() || unit->isOut())
		return fail("requested unit is not the live visible selection");
	BattlescapeGame *battleGame = battle->getBattleGame();
	if (!battleGame || battleGame->isBusy()) return fail("battle busy; retry when idle");
	if (battleGame->getReservedAction() != BA_NONE || save->getKneelReserved())
		return fail("TU reservation active; reachable click would not be deterministic");

	Pathfinding scratch(save);
	bool found = false;
	Position best;
	int bestCost = 1000000, bestEnergy = 0, bestSteps = 0, bestDirection = -1;
	const Position origin = unit->getPosition();
	for (int y = 0; y < save->getMapSizeY(); ++y)
	for (int x = 0; x < save->getMapSizeX(); ++x)
	{
		const Position target(x, y, origin.z);
		if (target == origin || !save->getTile(target)) continue;
		scratch.calculate(unit, target, BAM_NORMAL, nullptr, unit->getTimeUnits());
		const int cost = scratch.getTotalTUCost();
		const int energy = scratch.getTotalEnergyCost();
		const int steps = (int)scratch.getPath().size();
		const int firstDirection = steps > 0 ? scratch.getPath().back() : -1;
		scratch.abortPath();
		// One natural tile executes the complete routine-13 walking animation
		// cycle (phases 0..7). Prefer the cheapest exact-arrival target instead
		// of assuming PATH_FULL will queue every tile in a multi-step preview.
		if (steps < 1 || cost <= 0 || cost > unit->getTimeUnits() || energy > unit->getEnergy()) continue;
		// The fixed semantic gate is direction 6: frames 184/192/200 plus
		// phases 3 and 7. Prefer that natural adjacent step when reachable;
		// other directions remain a fail-closed diagnostic fallback.
		const bool preferredDirection = firstDirection == 6 && bestDirection != 6;
		const bool samePreference = (firstDirection == 6) == (bestDirection == 6);
		if (!found || preferredDirection
		 || (samePreference && (cost < bestCost || (cost == bestCost && steps < bestSteps))))
		{
			found = true; best = target; bestCost = cost; bestEnergy = energy;
			bestSteps = steps; bestDirection = firstDirection;
		}
	}
	if (!found) return fail("no reachable same-level natural step in current TU/energy budget");

	Camera *camera = battle->getMap()->getCamera();
	if (camera->getViewLevel() != best.z) return fail("walk target is not on the current camera level");
	Position raw, offset = camera->getMapOffset();
	camera->convertMapToScreen(best, &raw);
	const int expectedX = raw.x + offset.x + 32;
	const int expectedY = raw.y + offset.y + 20;
	int clickX = expectedX, clickY = expectedY;
	bool clickFound = false;
	for (int radius = 0; radius <= 48 && !clickFound; ++radius)
	for (int dy = -radius; dy <= radius && !clickFound; ++dy)
	for (int dx = -radius; dx <= radius; ++dx)
	{
		if (std::max(std::abs(dx), std::abs(dy)) != radius) continue;
		int mx = -1, my = -1;
		camera->convertScreenToMap(expectedX + dx, expectedY + dy + 20, &mx, &my);
		if (mx == best.x && my == best.y)
		{
			clickX = expectedX + dx; clickY = expectedY + dy; clickFound = true; break;
		}
	}
	if (!clickFound) return fail("walk target has no verified current-camera click point");
	std::ofstream f(path, std::ios::binary | std::ios::trunc);
	if (!f) return false;
	f << "{\"ok\":true,\"unitId\":" << unitId
	  << ",\"logicalSize\":{\"width\":" << battle->getMap()->getWidth()
	  << ",\"height\":" << battle->getMap()->getHeight() << "}"
	  << ",\"target\":{\"x\":" << best.x << ",\"y\":" << best.y << ",\"z\":" << best.z << "}"
	  << ",\"clickBase\":{\"x\":" << clickX << ",\"y\":" << clickY << "}"
	  << ",\"pathSteps\":" << bestSteps << ",\"tuCost\":" << bestCost
	  << ",\"moveDirection\":" << bestDirection
	  << ",\"energyCost\":" << bestEnergy << ",\"currentTu\":" << unit->getTimeUnits()
	  << ",\"currentEnergy\":" << unit->getEnergy() << "}\n";
	return true;
}

bool HdUnitBattleSpike::inspectInput(Game *game, const char *outJson)
{
	const std::string path = outJson ? outJson : "/tmp/hd-unit-input-state.json";
	if (!active() || !game || !game->getSavedGame()) return false;
	SavedBattleGame *save = game->getSavedGame()->getSavedBattle();
	auto *battle = dynamic_cast<BattlescapeState *>(game->getTopState());
	if (!save || !battle || !battle->getMap()) return false;
	Map *map = battle->getMap();
	Position selector;
	map->getSelectorPosition(&selector);
	BattleUnit *selected = save->getSelectedUnit();
	std::ofstream f(path, std::ios::binary | std::ios::trunc);
	if (!f) return false;
	f << "{\"ok\":true,\"logicalSize\":{\"width\":" << map->getWidth()
	  << ",\"height\":" << map->getHeight() << "},\"selector\":{\"x\":" << selector.x << ",\"y\":" << selector.y
	  << ",\"z\":" << selector.z << "},\"cursorType\":" << (int)map->getCursorType()
	  << ",\"busy\":" << (battle->getBattleGame() && battle->getBattleGame()->isBusy() ? "true" : "false")
	  << ",\"selectedUnitId\":" << (selected ? selected->getId() : -1)
	  << ",\"selectedPosition\":{\"x\":" << (selected ? selected->getPosition().x : -1)
	  << ",\"y\":" << (selected ? selected->getPosition().y : -1)
	  << ",\"z\":" << (selected ? selected->getPosition().z : -1) << "}"
	  << ",\"selectedDirection\":" << (selected ? selected->getDirection() : -1)
	  << ",\"selectedTu\":" << (selected ? selected->getTimeUnits() : -1)
	  << ",\"selectedEnergy\":" << (selected ? selected->getEnergy() : -1) << "}\n";
	return true;
}

bool HdUnitBattleSpike::checkpoint(const char *label)
{
	if (!active()) return false;
	g_probe->checkpoint = label ? label : "checkpoint";
	return true;
}

void HdUnitBattleSpike::beginRenderFrame()
{
	if (!active()) return;
	g_probe->foregroundRects.clear();
	g_probe->overlayRects.clear();
	g_probe->foregroundGeometry.clear();
	g_probe->overlayGeometry.clear();
}

void HdUnitBattleSpike::recordDrawStart(int unitId)
{
	if (!active() || unitId != g_probe->selectedUnitId) return;
	++g_probe->drawSerial;
	if (g_probe->game && g_probe->game->getSavedGame())
	{
		SavedBattleGame *save = g_probe->game->getSavedGame()->getSavedBattle();
		BattleUnit *selected = save ? save->getSelectedUnit() : nullptr;
		if (selected && selected->getId() == unitId)
			g_probe->selectedPosition = selected->getPosition();
	}
}

void HdUnitBattleSpike::recordEmit(int unitId, int direction, int sequence,
	                                const char *kind, int frame, float priority, bool overlay)
{
	if (!active()) return;
	Probe &p = *g_probe;
	if (unitId != p.selectedUnitId) return;
	const float expectedPriority = 4.0f + sequence * 0.25f + (overlay ? 0.125f : 0.0f);
	if (priority != expectedPriority || priority >= 6.0f) p.orderPreserved = false;
	if (overlay)
	{
		if (p.samples.empty()) p.orderPreserved = false;
		else
		{
			const Sample &base = p.samples.back();
			if (base.overlay || base.unitId != unitId || base.checkpoint != p.checkpoint
			 || base.drawSerial != p.drawSerial || base.sequence != sequence
			 || base.frame != frame || base.kind != "body")
				p.orderPreserved = false;
		}
	}
	else
	{
		for (auto it = p.samples.rbegin(); it != p.samples.rend(); ++it)
		{
			if (it->checkpoint != p.checkpoint || it->unitId != unitId
			 || it->drawSerial != p.drawSerial) break;
			if (!it->overlay)
			{
				if (sequence != 0 && sequence <= it->sequence) p.orderPreserved = false;
				break;
			}
		}
	}
	Sample s; s.checkpoint = p.checkpoint; s.kind = kind; s.unitId = unitId; s.direction = direction;
	s.sequence = sequence; s.frame = frame; s.priority = priority; s.overlay = overlay; s.drawSerial = p.drawSerial;
	if (p.samples.size() < 4096) p.samples.push_back(std::move(s));
	if (std::string(kind) == "HANDOB") p.handobFrames.push_back(frame);
	else
	{
		p.bodyFrames.push_back(frame);
		if (overlay) ++p.overlayHits;
		else if (!p.spec || frame < 0 || (size_t)frame >= p.spec->g0OverlayMask.size()
		      || !p.spec->g0OverlayMask[(size_t)frame]) ++p.fallbackHits;
	}
}

void HdUnitBattleSpike::recordGlError(unsigned error)
{
	if (active() && error != GL_NO_ERROR && g_probe->glError == GL_NO_ERROR)
		g_probe->glError = (GLenum)error;
}

void HdUnitBattleSpike::recordForegroundOccluder(int x, int y, int z, int priority,
	                                             float depthPriority, int shade, const Surface *source,
	                                             bool verifiedVanillaR8,
	                                             float screenX, float screenY, float w, float h)
{
	if (!active() || g_probe->selectedUnitId < 0 || priority != 6 || !source
	 || !verifiedVanillaR8) return;
	const Position &p = g_probe->selectedPosition;
	if (p.x != x || p.y != y || p.z != z) return;
	Rect front{screenX, screenY, w, h};
	g_probe->foregroundRects.push_back(front);
	ForegroundGeometry geometry;
	geometry.rect = front; geometry.source = source; geometry.shade = shade;
	geometry.partPriority = priority; geometry.depthPriority = depthPriority;
	g_probe->foregroundGeometry.push_back(geometry);
	for (const Rect &overlay : g_probe->overlayRects)
		if (overlaps(front, overlay)) { ++g_probe->foregroundOccluderCandidates; break; }
}

void HdUnitBattleSpike::recordOverlayGeometry(int unitId, int frame, float depthPriority,
	                                          float screenX, float screenY, float w, float h)
{
	if (!active() || unitId != g_probe->selectedUnitId) return;
	Rect overlay{screenX, screenY, w, h};
	g_probe->overlayRects.push_back(overlay);
	OverlayGeometry geometry;
	geometry.rect = overlay; geometry.frame = frame; geometry.depthPriority = depthPriority;
	g_probe->overlayGeometry.push_back(geometry);
	for (const Rect &front : g_probe->foregroundRects)
		if (overlaps(front, overlay)) { ++g_probe->foregroundOccluderCandidates; break; }
}

void HdUnitBattleSpike::captureForegroundPixelProof(int framebufferW, int framebufferH,
	                                                 int logicalW, int logicalH,
	                                                 bool floatingPointTarget)
{
	if (!active() || g_probe->pixelProof.passed || framebufferW <= 0 || framebufferH <= 0
	 || logicalW <= 0 || logicalH <= 0) return;
	Probe &p = *g_probe;
	PixelProof candidate;
	for (const OverlayGeometry &overlay : p.overlayGeometry)
	{
		if (overlay.frame < 0 || p.overlayPixels.empty()) continue;
		const int atlasCol = overlay.frame % 16, atlasRow = overlay.frame / 16;
		for (const ForegroundGeometry &front : p.foregroundGeometry)
		{
			if (!front.source || front.depthPriority <= overlay.depthPriority) continue;
			const int left = std::max(0, (int)std::ceil(std::max(overlay.rect.x, front.rect.x)));
			const int top = std::max(0, (int)std::ceil(std::max(overlay.rect.y, front.rect.y)));
			const int right = std::min(logicalW - 1,
				(int)std::floor(std::min(overlay.rect.x + overlay.rect.w,
				                         front.rect.x + front.rect.w) - 1.0f));
			const int bottom = std::min(logicalH - 1,
				(int)std::floor(std::min(overlay.rect.y + overlay.rect.h,
				                         front.rect.y + front.rect.h) - 1.0f));
			for (int sy = top; sy <= bottom; ++sy)
			for (int sx = left; sx <= right; ++sx)
			{
				// Match the two vertex shaders exactly: RGBA quads overdraw by two
				// logical pixels per edge; palette terrain quads by one.
				const float ou = ((float)sx + 0.5f - (overlay.rect.x - 2.0f)) / (overlay.rect.w + 4.0f);
				const float ov = ((float)sy + 0.5f - (overlay.rect.y - 2.0f)) / (overlay.rect.h + 4.0f);
				const int ox = std::min(63, std::max(0, (int)std::floor(ou * 64.0f)));
				const int oy = std::min(79, std::max(0, (int)std::floor(ov * 80.0f)));
				const int ax = atlasCol * 64 + ox, ay = atlasRow * 80 + oy;
				if (ax < 0 || ax >= p.overlayAtlasW || ay < 0 || ay >= p.overlayAtlasH) continue;
				const size_t oi = ((size_t)ay * (size_t)p.overlayAtlasW + (size_t)ax) * 4u;
				// A partially-transparent source can round to the same framebuffer
				// value as the foreground and would not prove that depth won.
				if (p.overlayPixels[oi + 3] != 255) continue;

				const int fw = front.source->getWidth(), fh = front.source->getHeight();
				const float fu = ((float)sx + 0.5f - (front.rect.x - 1.0f)) / (front.rect.w + 2.0f);
				const float fv = ((float)sy + 0.5f - (front.rect.y - 1.0f)) / (front.rect.h + 2.0f);
				const int fx = std::min(fw - 1, std::max(0, (int)std::floor(fu * fw)));
				const int fy = std::min(fh - 1, std::max(0, (int)std::floor(fv * fh)));
				const Uint8 paletteIndex = front.source->getPixel(fx, fy);
				const ShadeTable *shadeTable = front.source->getShadeTable();
				if (!paletteIndex || !shadeTable) continue;
				const Uint32 expected = shadeTable->get(paletteIndex, front.shade);
				if (((expected >> 24) & 0xffu) != 255u) continue;
				const int expectedR = (expected >> 16) & 0xffu;
				const int expectedG = (expected >> 8) & 0xffu;
				const int expectedB = expected & 0xffu;
				// If both sources already have the same RGB this pixel cannot prove
				// which draw won, even with a perfect readback. Fail it as ambiguous.
				if (std::abs((int)p.overlayPixels[oi] - expectedR) <= candidate.tolerance
				 && std::abs((int)p.overlayPixels[oi + 1] - expectedG) <= candidate.tolerance
				 && std::abs((int)p.overlayPixels[oi + 2] - expectedB) <= candidate.tolerance)
					continue;

				candidate.available = true; candidate.reason.clear();
				candidate.screenX = sx; candidate.screenY = sy;
				candidate.overlaySourceX = ox; candidate.overlaySourceY = oy;
				candidate.overlayFrame = overlay.frame;
				candidate.overlayR = p.overlayPixels[oi]; candidate.overlayG = p.overlayPixels[oi + 1];
				candidate.overlayB = p.overlayPixels[oi + 2]; candidate.overlayA = p.overlayPixels[oi + 3];
				candidate.foregroundSourceX = fx; candidate.foregroundSourceY = fy;
				candidate.foregroundPaletteIndex = paletteIndex; candidate.foregroundShade = front.shade;
				candidate.expectedR = expectedR; candidate.expectedG = expectedG;
				candidate.expectedB = expectedB; candidate.expectedA = (expected >> 24) & 0xffu;
				candidate.overlayPriority = overlay.depthPriority;
				candidate.foregroundPriority = front.depthPriority;
				goto found;
			}
		}
	}
found:
	if (!candidate.available) { p.pixelProof = candidate; return; }

	const float scaleX = (float)framebufferW / (float)logicalW;
	const float scaleY = (float)framebufferH / (float)logicalH;
	candidate.framebufferX = std::min(framebufferW - 1,
		std::max(0, (int)std::floor(((float)candidate.screenX + 0.5f) * scaleX)));
	const int topY = std::min(framebufferH - 1,
		std::max(0, (int)std::floor(((float)candidate.screenY + 0.5f) * scaleY)));
	candidate.framebufferY = framebufferH - 1 - topY;
	GLint oldPack = 4; glGetIntegerv(GL_PACK_ALIGNMENT, &oldPack); glPixelStorei(GL_PACK_ALIGNMENT, 1);
	GLenum errBefore = glGetError();
	if (errBefore != GL_NO_ERROR) recordGlError((unsigned)errBefore);
	if (floatingPointTarget)
	{
		float rgba[4] = {0, 0, 0, 0};
		glReadPixels(candidate.framebufferX, candidate.framebufferY, 1, 1, GL_RGBA, GL_FLOAT, rgba);
		candidate.framebufferR = (int)std::lround(std::min(1.0f, std::max(0.0f, rgba[0])) * 255.0f);
		candidate.framebufferG = (int)std::lround(std::min(1.0f, std::max(0.0f, rgba[1])) * 255.0f);
		candidate.framebufferB = (int)std::lround(std::min(1.0f, std::max(0.0f, rgba[2])) * 255.0f);
		candidate.framebufferA = (int)std::lround(std::min(1.0f, std::max(0.0f, rgba[3])) * 255.0f);
	}
	else
	{
		uint8_t rgba[4] = {0, 0, 0, 0};
		glReadPixels(candidate.framebufferX, candidate.framebufferY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
		candidate.framebufferR = rgba[0]; candidate.framebufferG = rgba[1];
		candidate.framebufferB = rgba[2]; candidate.framebufferA = rgba[3];
	}
	const GLenum readError = glGetError();
	glPixelStorei(GL_PACK_ALIGNMENT, oldPack);
	if (readError != GL_NO_ERROR)
	{
		recordGlError((unsigned)readError);
		candidate.reason = "glReadPixels failed";
		p.pixelProof = candidate;
		return;
	}
	auto near = [&](int a, int b) { return std::abs(a - b) <= candidate.tolerance; };
	candidate.passed = candidate.foregroundPriority > candidate.overlayPriority
	                && near(candidate.framebufferR, candidate.expectedR)
	                && near(candidate.framebufferG, candidate.expectedG)
	                && near(candidate.framebufferB, candidate.expectedB);
	candidate.reason = candidate.passed ? "foreground source color won exact WebGL depth test"
	                                    : "WebGL readback did not match the opaque foreground source";
	p.pixelProof = candidate;
}

bool HdUnitBattleSpike::finish(Game *game, const char *outPng)
{
	if (!active() || !game || !game->getScreen()) return false;
	Probe &p = *g_probe;
	p.screenshotPath = outPng ? outPng : "/tmp/hd-unit-battle-g0.png";
	std::remove(p.screenshotPath.c_str());
	game->getScreen()->screenshotGPU(p.screenshotPath);
	recordGlError((unsigned)glGetError());
	std::ifstream shot(p.screenshotPath, std::ios::binary | std::ios::ate);
	if (shot)
	{
		const std::streampos end = shot.tellg();
		if (end > 0) p.screenshotBytes = (size_t)end;
	}
	const bool success = p.screenshotBytes > 0;
	if (!success) p.error = "GPU screenshot was not written to MEMFS";
	writeMetrics(p);
	p.detach();
	g_probe.reset();
	return success;
}
}
#endif
