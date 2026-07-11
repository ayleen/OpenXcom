/* Disposable Phase-42 G0: sparse RGBA overlay on the real Battlescape unit path. */
#ifdef __EMSCRIPTEN__

#include "HdUnitBattleSpike.h"
#include "Game.h"
#include "GpuTexture.h"
#include "Logger.h"
#include "Screen.h"
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

#include <GLES3/gl3.h>
#include <SDL.h>
#include <SDL_image.h>
#include <algorithm>
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
		int w = 0, h = 0; std::vector<uint8_t> pixels;
		if (!decode(assetPath, w, h, pixels, error)) return false;
		if (w != 1024 || h != 1440)
		{
			error = "G0 overlay must exactly match the 16x18 64x80 TDXCOM_0 probe atlas (1024x1440)";
			return false;
		}
		if (!atlas->uploadRGBA(pixels.data(), w, h, 0))
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
	  << ",\"gl0\":" << (gl0 ? "true" : "false")
	  << ",\"screenshot\":" << (screenshot ? "true" : "false") << "},\n"
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
	int bestCost = 1000000, bestEnergy = 0, bestSteps = 0;
	const Position origin = unit->getPosition();
	for (int y = 0; y < save->getMapSizeY(); ++y)
	for (int x = 0; x < save->getMapSizeX(); ++x)
	{
		Position target(x, y, origin.z);
		if (target == origin) continue;
		Tile *tile = save->getTile(target);
		if (!tile || !tile->getSprite(O_OBJECT) || tile->isBackTileObject(O_OBJECT)) continue;
		pf->calculate(unit, target, BAM_NORMAL, nullptr, unit->getTimeUnits());
		const int cost = pf->getTotalTUCost();
		const int energy = pf->getTotalEnergyCost();
		const int steps = (int)pf->getPath().size();
		pf->abortPath();
		if (steps <= 0 || cost <= 0 || cost > unit->getTimeUnits() || energy > unit->getEnergy()) continue;
		if (!found || cost < bestCost)
		{
			found = true; best = target; bestCost = cost; bestEnergy = energy; bestSteps = steps;
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
	std::ofstream f(path, std::ios::binary | std::ios::trunc);
	if (!f) return false;
	f << "{\"ok\":true,\"unitId\":" << unitId
	  << ",\"target\":{\"x\":" << best.x << ",\"y\":" << best.y << ",\"z\":" << best.z << "}"
	  << ",\"clickBase\":{\"x\":" << clickX << ",\"y\":" << clickY << "}"
	  << ",\"pathSteps\":" << bestSteps << ",\"tuCost\":" << bestCost
	  << ",\"energyCost\":" << bestEnergy << "}\n";
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
	                                             float screenX, float screenY, float w, float h)
{
	if (!active() || g_probe->selectedUnitId < 0 || priority != 6) return;
	const Position &p = g_probe->selectedPosition;
	if (p.x != x || p.y != y || p.z != z) return;
	Rect front{screenX, screenY, w, h};
	g_probe->foregroundRects.push_back(front);
	for (const Rect &overlay : g_probe->overlayRects)
		if (overlaps(front, overlay)) { ++g_probe->foregroundOccluderCandidates; break; }
}

void HdUnitBattleSpike::recordOverlayGeometry(int unitId, float screenX, float screenY, float w, float h)
{
	if (!active() || unitId != g_probe->selectedUnitId) return;
	Rect overlay{screenX, screenY, w, h};
	g_probe->overlayRects.push_back(overlay);
	for (const Rect &front : g_probe->foregroundRects)
		if (overlaps(front, overlay)) { ++g_probe->foregroundOccluderCandidates; break; }
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
