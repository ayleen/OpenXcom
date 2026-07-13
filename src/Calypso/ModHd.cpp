/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Calypso — emscripten-only HD/atlas + lazy-surface engine code for Mod.
 * Extracted from Mod/Mod.cpp (Phase 36, pure relocation). Member declarations
 * remain in Mod/Mod.h inside its #ifdef __EMSCRIPTEN__ class section.
 */
#ifdef __EMSCRIPTEN__

#include "../Mod/Mod.h"
#include "CalypsoEconomy.h"
#include "CalypsoUiFamilies.h"
#include "../Mod/ModScript.h"
#include <algorithm>
#include <functional>
#include <iomanip>
#include <sstream>
#include <climits>
#include <cassert>
#include <cstring>
#include "../version.h"
#include "../Engine/CrossPlatform.h"
#include "../Engine/FileMap.h"
#include "../Engine/Palette.h"
#include "../Engine/Font.h"
#include "../Engine/TTFFont.h"
#include "../Engine/Surface.h"
#include "../Engine/SurfaceSet.h"
#include "../Engine/Music.h"
#include "../Engine/GMCat.h"
#include "../Engine/SoundSet.h"
#include "../Engine/Sound.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Mod/MapDataSet.h"
#include "../Mod/RuleMusic.h"
#include "../Engine/ShaderDraw.h"
#include "../Engine/ShaderMove.h"
#include "../Engine/Exception.h"
#include "../Engine/Logger.h"
#include "../Engine/ScriptBind.h"
#include "../Engine/Collections.h"
#include "../Mod/SoundDefinition.h"
#include "../Mod/ExtraSprites.h"
#include "../Mod/CustomPalettes.h"
#ifdef __EMSCRIPTEN__
#  include "../Engine/GpuTexture.h"
#  include "../Engine/GpuInit.h"
#  include <SDL_image.h>
#  include <GLES3/gl3.h>
#  include <webp/decode.h>
#  include "../Mod/TileAtlasBuilder.h"
#  include "../Mod/UnitSpriteAtlasBuilder.h"
#  include <set>
// M5: heap-attribution marks (function defined in Calypso/EmscriptenHarness.cpp)
extern "C" void calypso_log_heap(const char *tag);
#endif
#include "../Mod/ExtraSounds.h"
#include "../Engine/AdlibMusic.h"
#include "../Engine/CatFile.h"
#include "../fmath.h"
#include "../Engine/RNG.h"
#include "../Engine/Options.h"
#include "../Battlescape/Pathfinding.h"
#include "../Mod/RuleCountry.h"
#include "../Mod/RuleRegion.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Mod/RuleCraft.h"
#include "../Mod/RuleCraftWeapon.h"
#include "../Mod/RuleItemCategory.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleWeaponSet.h"
#include "../Mod/RuleUfo.h"
#include "../Mod/RuleTerrain.h"
#include "../Mod/MapScript.h"
#include "../Mod/RuleSoldier.h"
#include "../Mod/RuleSkill.h"
#include "../Mod/RuleCommendations.h"
#include "../Mod/AlienRace.h"
#include "../Mod/RuleEnviroEffects.h"
#include "../Mod/RuleStartingCondition.h"
#include "../Mod/AlienDeployment.h"
#include "../Mod/Armor.h"
#include "../Mod/ArticleDefinition.h"
#include "../Mod/RuleInventory.h"
#include "../Mod/RuleResearch.h"
#include "../Mod/RuleManufacture.h"
#include "../Mod/RuleManufactureShortcut.h"
#include "../Mod/ExtraStrings.h"
#include "../Mod/RuleInterface.h"
#include "../Mod/RuleArcScript.h"
#include "../Mod/RuleEventScript.h"
#include "../Mod/RuleEvent.h"
#include "../Mod/RuleMissionScript.h"
#include "../Geoscape/Globe.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/Region.h"
#include "../Savegame/Base.h"
#include "../Savegame/Country.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/Craft.h"
#include "../Savegame/CraftWeapon.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/Transfer.h"
#include "../Ufopaedia/Ufopaedia.h"
#include "../Savegame/AlienStrategy.h"
#include "../Savegame/GameTime.h"
#include "../Savegame/SoldierDiary.h"
#include "../Mod/UfoTrajectory.h"
#include "../Mod/RuleAlienMission.h"
#include "../Mod/MCDPatch.h"
#include "../Mod/StatString.h"
#include "../Mod/RuleGlobe.h"
#include "../Mod/RuleVideo.h"
#include "../Mod/RuleConverter.h"
#include "../Mod/RuleSoldierTransformation.h"
#include "../Mod/RuleSoldierBonus.h"

#define ARRAYLEN(x) (std::size(x))


namespace OpenXcom
{

/**
 * L10: Materialises a deferred geo/flat surface on first getSurface() call.
 * Decodes the file from MEMFS (which stays resident — files are never unlinked),
 * promotes 8bpp pixels to ARGB via PAL_GEOSCAPE, and inserts the result into
 * _surfaces.  The lazy-registry entry is erased so subsequent calls skip this
 * function immediately.
 *
 * Palette note: PAL_GEOSCAPE is loaded eagerly before the geo/flat block, so
 * getPalette("PAL_GEOSCAPE") is always valid here.  The palette never changes
 * after load() completes, so applying it at materialisation time is identical
 * to the eager path's immediate setPalette() call.
 */
void Mod::materializeGeoSurface(const std::string &name)
{
	auto it = _lazyGeoSurfaces.find(name);
	if (it == _lazyGeoSurfaces.end()) return;

	const LazyGeoEntry &e = it->second;
	Surface *s = new Surface(e.w, e.h);
	switch (e.format)
	{
	case LazyGeoFormat::Scr: s->loadScr(e.vfsPath); break;
	case LazyGeoFormat::Bdy: s->loadBdy(e.vfsPath); break;
	case LazyGeoFormat::Spk: s->loadSpk(e.vfsPath); break;
	}
	Palette *geoPal = getPalette("PAL_GEOSCAPE", false);
	if (geoPal) s->setPalette(geoPal->getColors(), 0, 256);

	_surfaces[name] = s;
	_lazyGeoSurfaces.erase(it);
	Log(LOG_DEBUG) << "[L10] materialized geo surface: " << name;
}

GpuTexture* Mod::getGlobeTexture(const std::string& id) const
{
	auto it = _globeTextures.find(id);
	return (it != _globeTextures.end()) ? it->second : nullptr;
}

void Mod::clearGlobeTextures()
{
	for (auto& pair : _globeTextures)
		delete pair.second;
	_globeTextures.clear();
}

void Mod::evictGlobeGL()
{
	if (_globeGpuEvicted) return;
	int n = 0;
	for (auto& pair : _globeTextures)
		if (pair.second) { pair.second->evictGL(); ++n; }
	_globeGpuEvicted = true;
	Log(LOG_INFO) << "[L5] evictGlobeGL: released " << n << " globe GL handle(s)";
}

void Mod::restoreGlobeGL()
{
	if (!_globeGpuEvicted) return;
	int n = 0;
	for (auto& pair : _globeTextures)
		if (pair.second) { pair.second->reupload(); ++n; }
	_globeGpuEvicted = false;
	Log(LOG_INFO) << "[L5] restoreGlobeGL: reuploaded " << n << " globe GL handle(s)";
}

const Mod::TileAtlasSpec* Mod::getTileAtlasSpec(const std::string& dataset) const
{
	auto it = _tileAtlasSpecs.find(dataset);
	return it != _tileAtlasSpecs.end() ? &it->second : nullptr;
}

GpuTexture* Mod::getTileAtlas(const std::string& dataset) const
{
	auto it = _tileAtlases.find(dataset);
	return it != _tileAtlases.end() ? it->second : nullptr;
}

void Mod::ensureVanillaAtlas(MapDataSet* mds, const SDL_Color* palette, int ncolors)
{
	if (!mds || !palette || ncolors < 1) return;
	if (!GpuInit::ready()) return;

	const std::string& name = mds->getName();

	// Already built for this dataset in this session.
	// Note: baseline:none datasets store nullptr in _tileAtlases as a visited sentinel.
	//
	// Re-emit the activeDataset marker even on the cached path: Map.cpp calls
	// ensureVanillaAtlas() for every dataset whenever a battlescape palette is
	// set, so this function fires on every mission entry. The marker has to
	// track "active now" (which dataset the user is iterating on), not
	// "first time loaded". Without this, a session that opens BLANKS after
	// SAND then returns to SAND leaves window.__activeDataset stuck on BLANKS
	// and Cmd+Shift+R rebuilds the wrong atlas.
	if (_tileAtlases.count(name))
	{
		Log(LOG_INFO) << "[CALYPSO] activeDataset " << name;
		return;
	}

	// If there's an explicit YAML tileAtlas: spec with a file path, try to load
	// the HD atlas PNG.  On any failure (file missing, decode error, GPU upload
	// error) we log a warning, erase the stale spec, and fall through to the
	// vanilla synthesiser below — so terrain always renders.
	auto specIt = _tileAtlasSpecs.find(name);

	// Phase 20.5: load sequential sub-layer overlays (-L1.png, -L2.png …).
	// The atlas builder writes them alongside overlayFile when any hdTile
	// declares subLayers[]. Stops at the first missing index — so absent
	// files are not an error. Used by both baseline:none and hybrid paths.
	auto loadSubLayerAtlases = [&name](TileAtlasSpec& spec) {
		const std::string& base = spec.overlayFile;
		const size_t extPos = base.rfind(".png");
		if (extPos == std::string::npos) return;
		for (int li = 1; li <= 8; ++li)
		{
			std::string layerPath = base.substr(0, extPos)
			                      + "-L" + std::to_string(li) + ".png";
			// fileExists() probe — FileMap::at() throws on miss; without this
			// the "stop at the first missing index" contract becomes "first
			// missing index kills the whole mod load".
			if (!FileMap::fileExists(layerPath)) break;
			/* L4b: skip _cachedData; re-decode sub-layer PNG from MEMFS on context loss. */
			const std::string capturedLayerPath = layerPath;
			const std::string capturedLayerName = name;
			const int         capturedSpecW     = spec.width;
			const int         capturedSpecH     = spec.height;
			GpuTexture* layerTex = new GpuTexture(/*srgb=*/false,
			                                      GpuTexture::Wrap::ClampToEdge,
			                                      GpuTexture::Filter::Linear);  // LINEAR: smooths the anti-aliased diamond alpha edge + HD texture on downscale
			layerTex->setSkipCache(true);
			auto doLayerUpload = [layerTex, capturedLayerPath, capturedLayerName]() {
				if (!FileMap::fileExists(capturedLayerPath))
				{
					Log(LOG_WARNING) << "tileAtlas[" << capturedLayerName
					                 << "] sub-layer not found on reload: " << capturedLayerPath;
					return;
				}
				const FileMap::FileRecord* layerRec2 = FileMap::at(capturedLayerPath);
				SDL_RWops* layerRw = layerRec2->getRWops();
				SDL_Surface* layerRaw = IMG_Load_RW(layerRw, SDL_TRUE);
				if (!layerRaw)
				{
					Log(LOG_WARNING) << "tileAtlas[" << capturedLayerName
					                 << "] sub-layer IMG_Load_RW failed (" << IMG_GetError() << ")";
					return;
				}
				SDL_Surface* layerRgba =
				    SDL_ConvertSurfaceFormat(layerRaw, SDL_PIXELFORMAT_ABGR8888, 0);
				SDL_FreeSurface(layerRaw);
				if (!layerRgba) return;
				if (SDL_MUSTLOCK(layerRgba)) SDL_LockSurface(layerRgba);
				bool layerOk = layerTex->uploadRGBA(
				    static_cast<const uint8_t*>(layerRgba->pixels), layerRgba->w, layerRgba->h);
				if (SDL_MUSTLOCK(layerRgba)) SDL_UnlockSurface(layerRgba);
				SDL_FreeSurface(layerRgba);
				if (!layerOk)
					Log(LOG_WARNING) << "tileAtlas[" << capturedLayerName
					                 << "] sub-layer GPU upload failed";
			};
			layerTex->setReloadCb(doLayerUpload);
			doLayerUpload();
			if (!layerTex->isValid())
			{
				delete layerTex;
				Log(LOG_WARNING) << "tileAtlas[" << capturedLayerName << "] sub-layer L" << li
				                 << " GPU upload failed";
				break;
			}
			if (layerTex->width() != capturedSpecW || layerTex->height() != capturedSpecH)
			{
				Log(LOG_WARNING) << "tileAtlas[" << capturedLayerName << "] sub-layer L" << li
				                 << " " << layerTex->width() << "x" << layerTex->height()
				                 << " != base " << capturedSpecW << "x" << capturedSpecH
				                 << " — skipping remaining sub-layers";
				delete layerTex;
				break;
			}
			spec.subLayerAtlases.push_back(layerTex);
			Log(LOG_INFO) << "tileAtlas[" << capturedLayerName << "] sub-layer L" << li
			              << " RGBA " << layerTex->width() << "x" << layerTex->height();
		}
	};

	// Phase 25 R3: load the optional tangent-space normal-map atlas (-normal.png).
	// RGBA Linear NON-sRGB — normals are linear direction data; sRGB gamma would
	// skew the decoded vectors. Same dimensions as the overlay (shared UVs); a
	// mismatch is rejected. Absent file = no relief for this dataset. Used by
	// both the baseline:none and hybrid paths.
	auto loadNormalAtlas = [&name](TileAtlasSpec& spec) {
		if (spec.normalFile.empty() || !FileMap::fileExists(spec.normalFile)) return;
		/* L4b: skip _cachedData; re-decode normal-map PNG from MEMFS on context loss. */
		const std::string capturedNormalFile = spec.normalFile;
		const std::string capturedName       = name;
		const int         capturedSpecW      = spec.width;
		const int         capturedSpecH      = spec.height;
		GpuTexture* ntex = new GpuTexture(/*srgb=*/false,   // LINEAR data, not sRGB
		                                  GpuTexture::Wrap::ClampToEdge,
		                                  GpuTexture::Filter::Linear);  // smooth normal interp
		ntex->setSkipCache(true);
		auto doNormalUpload = [ntex, capturedNormalFile, capturedName]() {
			if (!FileMap::fileExists(capturedNormalFile))
			{
				Log(LOG_WARNING) << "tileAtlas[" << capturedName
				                 << "] normal not found on reload: " << capturedNormalFile;
				return;
			}
			const FileMap::FileRecord* rec2 = FileMap::at(capturedNormalFile);
			SDL_RWops* rw = rec2->getRWops();
			SDL_Surface* raw = IMG_Load_RW(rw, SDL_TRUE);
			if (!raw)
			{
				Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] normal IMG_Load_RW failed ("
				                 << IMG_GetError() << ")";
				return;
			}
			SDL_Surface* rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ABGR8888, 0);
			SDL_FreeSurface(raw);
			if (!rgba) return;
			if (SDL_MUSTLOCK(rgba)) SDL_LockSurface(rgba);
			bool ok = ntex->uploadRGBA(static_cast<const uint8_t*>(rgba->pixels), rgba->w, rgba->h);
			if (SDL_MUSTLOCK(rgba)) SDL_UnlockSurface(rgba);
			SDL_FreeSurface(rgba);
			if (!ok)
				Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] normal GPU upload failed";
		};
		ntex->setReloadCb(doNormalUpload);
		doNormalUpload();
		if (!ntex->isValid())
		{
			delete ntex;
			Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] normal GPU upload failed";
			return;
		}
		if (capturedSpecW > 0 && capturedSpecH > 0
		    && (ntex->width() != capturedSpecW || ntex->height() != capturedSpecH))
		{
			Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] normal "
			                 << ntex->width() << "x" << ntex->height()
			                 << " != overlay " << capturedSpecW << "x" << capturedSpecH
			                 << " — skipping (UVs would mismatch)";
			delete ntex;
			return;
		}
		spec.normalAtlas = ntex;
		Log(LOG_INFO) << "tileAtlas[" << capturedName << "] normal atlas RGBA "
		              << ntex->width() << "x" << ntex->height();
	};

	// Phase 25 R6: load the optional material emissive atlas (-emissive.png).
	// RGBA Linear NON-sRGB to match the (raw, srgb=false) overlay colour path, so
	// the glow colour is added in the same space the lit colour is written. Same
	// dims as the overlay (shared UVs); a mismatch is rejected. Absent = no glow.
	auto loadEmissiveAtlas = [&name](TileAtlasSpec& spec) {
		if (spec.emissiveFile.empty() || !FileMap::fileExists(spec.emissiveFile)) return;
		/* L4b: skip _cachedData; re-decode emissive PNG from MEMFS on context loss. */
		const std::string capturedEmissiveFile = spec.emissiveFile;
		const std::string capturedName         = name;
		const int         capturedSpecW        = spec.width;
		const int         capturedSpecH        = spec.height;
		GpuTexture* etex = new GpuTexture(/*srgb=*/false,   // raw colour, matches overlay
		                                  GpuTexture::Wrap::ClampToEdge,
		                                  GpuTexture::Filter::Linear);  // smooth glow
		etex->setSkipCache(true);
		auto doEmissiveUpload = [etex, capturedEmissiveFile, capturedName]() {
			if (!FileMap::fileExists(capturedEmissiveFile))
			{
				Log(LOG_WARNING) << "tileAtlas[" << capturedName
				                 << "] emissive not found on reload: " << capturedEmissiveFile;
				return;
			}
			const FileMap::FileRecord* rec2 = FileMap::at(capturedEmissiveFile);
			SDL_RWops* rw = rec2->getRWops();
			SDL_Surface* raw = IMG_Load_RW(rw, SDL_TRUE);
			if (!raw)
			{
				Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] emissive IMG_Load_RW failed ("
				                 << IMG_GetError() << ")";
				return;
			}
			SDL_Surface* rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ABGR8888, 0);
			SDL_FreeSurface(raw);
			if (!rgba) return;
			if (SDL_MUSTLOCK(rgba)) SDL_LockSurface(rgba);
			bool ok = etex->uploadRGBA(static_cast<const uint8_t*>(rgba->pixels), rgba->w, rgba->h);
			if (SDL_MUSTLOCK(rgba)) SDL_UnlockSurface(rgba);
			SDL_FreeSurface(rgba);
			if (!ok)
				Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] emissive GPU upload failed";
		};
		etex->setReloadCb(doEmissiveUpload);
		doEmissiveUpload();
		if (!etex->isValid())
		{
			delete etex;
			Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] emissive GPU upload failed";
			return;
		}
		if (capturedSpecW > 0 && capturedSpecH > 0
		    && (etex->width() != capturedSpecW || etex->height() != capturedSpecH))
		{
			Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] emissive "
			                 << etex->width() << "x" << etex->height()
			                 << " != overlay " << capturedSpecW << "x" << capturedSpecH
			                 << " — skipping (UVs would mismatch)";
			delete etex;
			return;
		}
		spec.emissiveAtlas = etex;
		Log(LOG_INFO) << "tileAtlas[" << capturedName << "] emissive atlas RGBA "
		              << etex->width() << "x" << etex->height();
	};

	// Phase 20: baseline:none path — HD-only dataset, load RGBA overlay only.
	if (specIt != _tileAtlasSpecs.end()
	    && specIt->second.baseline == BaselineMode::None
	    && !specIt->second.overlayFile.empty())
	{
		TileAtlasSpec& spec = specIt->second;
		// fileExists() probe — FileMap::at() throws on miss; the fallback to
		// vanilla atlas only works if we don't unwind through ruleset loading.
		if (!FileMap::fileExists(spec.overlayFile))
		{
			Log(LOG_WARNING) << "tileAtlas[" << name << "] baseline:none overlay not found: "
			                 << spec.overlayFile;
			_tileAtlasSpecs.erase(specIt);
		}
		else
		{
			/* L4: skip _cachedData; re-decode overlay PNG from MEMFS on context loss. */
			const std::string capturedOverlayFile = spec.overlayFile;
			const std::string capturedName        = name;
			GpuTexture* tex = new GpuTexture(/*srgb=*/false,
			                                 GpuTexture::Wrap::ClampToEdge,
			                                 GpuTexture::Filter::Linear);  // LINEAR: smooths the anti-aliased diamond alpha edge + HD texture on downscale
			tex->setSkipCache(true);
			auto doOverlayUpload = [this, tex, capturedOverlayFile, capturedName]() {
				if (!FileMap::fileExists(capturedOverlayFile))
				{
					Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] baseline:none overlay not found on reload: " << capturedOverlayFile;
					return;
				}
				const FileMap::FileRecord* rec2 = FileMap::at(capturedOverlayFile);
				SDL_RWops* rw = rec2->getRWops();
				SDL_Surface* raw = IMG_Load_RW(rw, SDL_TRUE);
				if (!raw)
				{
					Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] baseline:none IMG_Load_RW failed ("
					                 << IMG_GetError() << ")";
					return;
				}
				SDL_Surface* rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ABGR8888, 0);
				SDL_FreeSurface(raw);
				if (!rgba) return;
				if (SDL_MUSTLOCK(rgba)) SDL_LockSurface(rgba);
				bool ok = tex->uploadRGBA(static_cast<const uint8_t*>(rgba->pixels), rgba->w, rgba->h);
				if (SDL_MUSTLOCK(rgba)) SDL_UnlockSurface(rgba);
				SDL_FreeSurface(rgba);
				if (!ok)
					Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] baseline:none GPU upload failed";
			};
			tex->setReloadCb(doOverlayUpload);
			doOverlayUpload();
			if (tex->isValid())
			{
				spec.overlayAtlas = tex;
				spec.width  = tex->width();
				spec.height = tex->height();
				// Sentinel: mark as visited without a baseline texture.
				_tileAtlases[name] = nullptr;
				if (spec.tileWidth > 0 && spec.tileHeight > 0 && spec.pckToAtlas.empty())
				{
					const int totalCells = (spec.width / spec.tileWidth) * (spec.height / spec.tileHeight);
					for (int n = 0; n < totalCells; ++n)
						spec.pckToAtlas[n] = n;
				}
				loadSubLayerAtlases(spec);
				loadNormalAtlas(spec);   // Phase 25 R3
				loadEmissiveAtlas(spec); // Phase 25 R6
				Log(LOG_INFO) << "tileAtlas[" << name << "] baseline:none overlay RGBA "
				              << spec.width << "x" << spec.height;
				Log(LOG_INFO) << "[CALYPSO] activeDataset " << name;
			}
			else
			{
				delete tex;
				Log(LOG_WARNING) << "tileAtlas[" << name << "] baseline:none GPU upload failed";
				_tileAtlasSpecs.erase(specIt);
			}
		}
		return;
	}

	// Phase 17: hybrid dual-atlas path (R8 baseline + RGBA sparse overlay).
	if (specIt != _tileAtlasSpecs.end() && specIt->second.hybrid)
	{
		TileAtlasSpec& spec = specIt->second;
		GpuTexture* baselineTex = nullptr;
		GpuTexture* overlayTex  = nullptr;

		// Load baseline R8 — palette indices stored directly as greyscale values.
		{
			// fileExists() probe — FileMap::at() throws on miss; the hybrid
			// path's fall-through to vanilla synth only works without unwind.
			const bool baselineHere =
			    !spec.baselineFile.empty() && FileMap::fileExists(spec.baselineFile);
			if (!baselineHere)
			{
				Log(LOG_WARNING) << "tileAtlas[" << name << "] hybrid: baseline not found: "
				                 << spec.baselineFile;
			}
			else
			{
				/* L4: skip _cachedData; re-decode baseline R8 from MEMFS on context loss. */
				const std::string capturedBaselineFile = spec.baselineFile;
				const std::string capturedName         = name;
				GpuTexture* tex = new GpuTexture(/*srgb=*/false, GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest) /* R8 palette: NEAREST only, else index interpolation = rainbow seams */;
				tex->setSkipCache(true);
				auto doBaselineUpload = [this, tex, capturedBaselineFile, capturedName]() {
					if (!FileMap::fileExists(capturedBaselineFile))
					{
						Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] hybrid: baseline not found on reload: "
						                 << capturedBaselineFile;
						return;
					}
					const FileMap::FileRecord* rec2 = FileMap::at(capturedBaselineFile);
					SDL_RWops* rw  = rec2->getRWops();
					SDL_Surface* raw = IMG_Load_RW(rw, SDL_TRUE);
					if (!raw)
					{
						Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] hybrid: IMG_Load_RW baseline failed ("
						                 << IMG_GetError() << ")";
						return;
					}
					SDL_Surface* rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ABGR8888, 0);
					SDL_FreeSurface(raw);
					if (!rgba) return;
					const int w = rgba->w, h = rgba->h;
					std::vector<uint8_t> r8(static_cast<size_t>(w) * static_cast<size_t>(h), 0u);
					if (SDL_MUSTLOCK(rgba)) SDL_LockSurface(rgba);
					const uint8_t* src = static_cast<const uint8_t*>(rgba->pixels);
					// Greyscale L-mode PNG: R channel holds the palette index directly.
					for (int y = 0; y < h; ++y)
					{
						const uint8_t* row = src + y * rgba->pitch;
						for (int x = 0; x < w; ++x)
							r8[y * w + x] = row[x * 4 + 0];
					}
					if (SDL_MUSTLOCK(rgba)) SDL_UnlockSurface(rgba);
					SDL_FreeSurface(rgba);
					if (!tex->uploadR8(r8.data(), w, h))
						Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] hybrid: baseline GPU upload failed";
					else
						Log(LOG_INFO) << "tileAtlas[" << capturedName << "] hybrid: baseline R8 " << w << "x" << h;
				};
				tex->setReloadCb(doBaselineUpload);
				doBaselineUpload();
				if (tex->isValid())
				{
					baselineTex  = tex;
					spec.width   = tex->width();
					spec.height  = tex->height();
					spec.format  = TileAtlasSpec::Format::Palette;
				}
				else
				{
					delete tex;
				}
			}
		}

		// Load overlay RGBA — sparse; (0,0,0,0) outside HD cells.
		if (baselineTex)
		{
			// fileExists() probe — FileMap::at() throws on miss.
			const bool overlayHere =
			    !spec.overlayFile.empty() && FileMap::fileExists(spec.overlayFile);
			if (!overlayHere)
			{
				Log(LOG_WARNING) << "tileAtlas[" << name << "] hybrid: overlay not found: "
				                 << spec.overlayFile;
				delete baselineTex; baselineTex = nullptr;
			}
			else
			{
				/* L4: skip _cachedData; re-decode overlay RGBA from MEMFS on context loss. */
				const std::string capturedOverlayFile = spec.overlayFile;
				const std::string capturedName        = name;
				const int         capturedW           = spec.width;
				const int         capturedH           = spec.height;
				GpuTexture* tex = new GpuTexture(/*srgb=*/false,
				                                 GpuTexture::Wrap::ClampToEdge,
				                                 GpuTexture::Filter::Linear);  // LINEAR: smooths the anti-aliased diamond alpha edge + HD texture on downscale
				tex->setSkipCache(true);
				auto doOverlayUpload = [this, tex, capturedOverlayFile, capturedName]() {
					if (!FileMap::fileExists(capturedOverlayFile))
					{
						Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] hybrid: overlay not found on reload: "
						                 << capturedOverlayFile;
						return;
					}
					const FileMap::FileRecord* rec2 = FileMap::at(capturedOverlayFile);
					SDL_RWops* rw  = rec2->getRWops();
					SDL_Surface* raw = IMG_Load_RW(rw, SDL_TRUE);
					if (!raw)
					{
						Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] hybrid: IMG_Load_RW overlay failed ("
						                 << IMG_GetError() << ")";
						return;
					}
					SDL_Surface* rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ABGR8888, 0);
					SDL_FreeSurface(raw);
					if (!rgba) return;
					if (SDL_MUSTLOCK(rgba)) SDL_LockSurface(rgba);
					bool ok = tex->uploadRGBA(static_cast<const uint8_t*>(rgba->pixels), rgba->w, rgba->h);
					if (SDL_MUSTLOCK(rgba)) SDL_UnlockSurface(rgba);
					SDL_FreeSurface(rgba);
					if (!ok)
						Log(LOG_WARNING) << "tileAtlas[" << capturedName << "] hybrid: overlay GPU upload failed";
				};
				tex->setReloadCb(doOverlayUpload);
				doOverlayUpload();
				if (tex->isValid())
				{
					// Reject hybrid pair if overlay dimensions don't match baseline —
					// otherwise UV mapping would silently drift.  Falls through to
					// vanilla synth.
					if (tex->width() != capturedW || tex->height() != capturedH)
					{
						Log(LOG_WARNING) << "tileAtlas[" << name << "] hybrid: overlay "
						                 << tex->width() << "x" << tex->height() << " != baseline "
						                 << capturedW << "x" << capturedH
						                 << " — rejecting hybrid pair";
						delete tex;
						delete baselineTex; baselineTex = nullptr;
					}
					else
					{
						overlayTex = tex;
						Log(LOG_INFO) << "tileAtlas[" << name << "] hybrid: overlay RGBA "
						              << tex->width() << "x" << tex->height();
					}
				}
				else
				{
					delete tex;
					Log(LOG_WARNING) << "tileAtlas[" << name << "] hybrid: overlay GPU upload failed";
					delete baselineTex; baselineTex = nullptr;
				}
			}
		}

		if (baselineTex && overlayTex)
		{
			_tileAtlases[name] = baselineTex;
			spec.overlayAtlas  = overlayTex;
			loadSubLayerAtlases(spec);
			loadNormalAtlas(spec);   // Phase 25 R3 (spec.width/height set from baseline above)
			loadEmissiveAtlas(spec); // Phase 25 R6
			if (spec.tileWidth > 0 && spec.tileHeight > 0 && spec.pckToAtlas.empty())
			{
				const int totalCells = (spec.width / spec.tileWidth) * (spec.height / spec.tileHeight);
				for (int n = 0; n < totalCells; ++n)
					spec.pckToAtlas[n] = n;
				Log(LOG_INFO) << "tileAtlas[" << name << "] hybrid: auto-built identity "
				              << "pckToAtlas (" << totalCells << " entries)";
			}
			Log(LOG_INFO) << "[CALYPSO] activeDataset " << name;
			return;
		}

		if (baselineTex) { delete baselineTex; }
		if (overlayTex)  { delete overlayTex;  }
		_tileAtlasSpecs.erase(specIt);
		// Fall through to vanilla synthesiser.
	}
	else if (specIt != _tileAtlasSpecs.end() && !specIt->second.file.empty())
	{
		const std::string& filePath = specIt->second.file;
		// fileExists() probe — FileMap::at() throws on miss; the legacy
		// single-file path's fall-through to vanilla synth only works
		// without unwind.
		const FileMap::FileRecord* rec = FileMap::fileExists(filePath)
		                                 ? FileMap::at(filePath) : nullptr;
		if (!rec)
		{
			Log(LOG_WARNING) << "tileAtlas[" << name << "]: file not found: "
			                 << filePath << " — falling back to vanilla atlas";
			_tileAtlasSpecs.erase(specIt);
			// Fall through to vanilla synthesiser.
		}
		else
		{
			bool loaded = false;
			SDL_RWops* rw = rec->getRWops();
			SDL_Surface* raw = IMG_Load_RW(rw, SDL_TRUE); // SDL_TRUE = auto-close rw
			if (!raw)
			{
				Log(LOG_WARNING) << "tileAtlas[" << name << "]: IMG_Load_RW failed ("
				                 << IMG_GetError() << ") — falling back to vanilla atlas";
			}
			else
			{
				// Convert to ABGR8888: memory layout [R,G,B,A] on little-endian WASM.
				SDL_Surface* rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ABGR8888, 0);
				SDL_FreeSurface(raw);
				if (!rgba)
				{
					Log(LOG_WARNING) << "tileAtlas[" << name
					                 << "]: ConvertSurface failed — falling back to vanilla atlas";
				}
				else
				{
					const int w = rgba->w, h = rgba->h;

					if (specIt->second.format == TileAtlasSpec::Format::Rgba)
					{
						// RGBA path: upload verbatim, no palette reverse-mapping.
						// L4b: free the already-decoded surface; lambda re-decodes on
						// initial upload and on context-loss reload.
						SDL_FreeSurface(rgba);
						const std::string capturedFilePath2 = std::string(filePath);
						const std::string capturedName2     = name;
						GpuTexture* tex = new GpuTexture(/*srgb=*/false,
						                                 GpuTexture::Wrap::ClampToEdge,
						                                 GpuTexture::Filter::Linear);  // LINEAR: smooths the anti-aliased diamond alpha edge + HD texture on downscale
						tex->setSkipCache(true);
						auto doRgbaUpload = [this, tex, capturedFilePath2, capturedName2]() {
							if (!FileMap::fileExists(capturedFilePath2))
							{
								Log(LOG_WARNING) << "tileAtlas[" << capturedName2
								                 << "]: not found on reload: " << capturedFilePath2;
								return;
							}
							const FileMap::FileRecord* rec2 = FileMap::at(capturedFilePath2);
							SDL_RWops* rw2 = rec2->getRWops();
							SDL_Surface* raw2 = IMG_Load_RW(rw2, SDL_TRUE);
							if (!raw2)
							{
								Log(LOG_WARNING) << "tileAtlas[" << capturedName2
								                 << "]: IMG_Load_RW failed (" << IMG_GetError() << ")";
								return;
							}
							SDL_Surface* rgba2 = SDL_ConvertSurfaceFormat(raw2, SDL_PIXELFORMAT_ABGR8888, 0);
							SDL_FreeSurface(raw2);
							if (!rgba2) return;
							if (SDL_MUSTLOCK(rgba2)) SDL_LockSurface(rgba2);
							bool ok = tex->uploadRGBA(
							    static_cast<const uint8_t*>(rgba2->pixels), rgba2->w, rgba2->h);
							if (SDL_MUSTLOCK(rgba2)) SDL_UnlockSurface(rgba2);
							SDL_FreeSurface(rgba2);
							if (!ok)
								Log(LOG_WARNING) << "tileAtlas[" << capturedName2
								                 << "]: RGBA GPU upload failed";
						};
						tex->setReloadCb(doRgbaUpload);
						doRgbaUpload();
						loaded = tex->isValid();
						if (loaded)
						{
							_tileAtlases[name] = tex;
							specIt->second.width  = tex->width();
							specIt->second.height = tex->height();
							Log(LOG_INFO) << "tileAtlas[" << name << "]: loaded RGBA atlas "
							              << tex->width() << "x" << tex->height() << " (GL_NEAREST filter)";
						}
						else
						{
							delete tex;
							Log(LOG_WARNING) << "tileAtlas[" << name
							                 << "]: RGBA GPU upload failed — falling back to vanilla atlas";
						}
					}
					else
					{
						// Palette path: reverse-map RGBA PNG → R8 palette index.
						std::vector<uint8_t> r8(static_cast<size_t>(w) * static_cast<size_t>(h), 0u);

						// Reverse-palette map for exact hits: RGB packed as r|(g<<8)|(b<<16) → index.
						// palette[0] is transparent; do not map it.
						std::map<uint32_t, uint8_t> revPal;
						for (int i = 1; i < ncolors; ++i)
						{
							uint32_t key = (uint32_t)palette[i].r
							             | ((uint32_t)palette[i].g << 8)
							             | ((uint32_t)palette[i].b << 16);
							revPal[key] = (uint8_t)i;
						}

						if (SDL_MUSTLOCK(rgba)) SDL_LockSurface(rgba);
						const uint8_t* src = static_cast<const uint8_t*>(rgba->pixels);
						int nearestCount = 0;
						for (int y = 0; y < h; ++y)
						{
							const uint8_t* row = src + y * rgba->pitch;
							for (int x = 0; x < w; ++x)
							{
								const uint8_t a = row[x * 4 + 3];
								if (a < 128) { r8[y * w + x] = 0; continue; }
								const uint8_t pr = row[x * 4 + 0];
								const uint8_t pg = row[x * 4 + 1];
								const uint8_t pb = row[x * 4 + 2];
								uint32_t key = (uint32_t)pr | ((uint32_t)pg << 8) | ((uint32_t)pb << 16);
								auto it = revPal.find(key);
								if (it != revPal.end())
								{
									r8[y * w + x] = it->second;
								}
								else
								{
									// Non-palette-exact pixel: nearest palette entry by squared RGB distance.
									int bestIdx = 1, bestDist = INT_MAX;
									for (int i = 1; i < ncolors; ++i)
									{
										int dr = (int)pr - (int)palette[i].r;
										int dg = (int)pg - (int)palette[i].g;
										int db = (int)pb - (int)palette[i].b;
										int dist = dr*dr + dg*dg + db*db;
										if (dist < bestDist) { bestDist = dist; bestIdx = i; }
									}
									r8[y * w + x] = (uint8_t)bestIdx;
									++nearestCount;
								}
							}
						}
						if (SDL_MUSTLOCK(rgba)) SDL_UnlockSurface(rgba);
						SDL_FreeSurface(rgba);
						if (nearestCount > 0) {
							Log(LOG_WARNING) << "tileAtlas[" << name << "]: "
							                 << nearestCount << " pixel(s) are not exact palette "
							                 << "matches and were nearest-colour quantised — "
							                 << "atlas art should use only TFTD palette colours";
						}

						/* L4b: skip _cachedData; re-upload R8 palette buffer on context loss.
						 * r8 is moved into the lambda — no re-quantisation needed on reload. */
						const std::string capturedName3 = name;
						GpuTexture* tex = new GpuTexture(/*srgb=*/false, GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest) /* R8 palette: NEAREST only, else index interpolation = rainbow seams */;
						tex->setSkipCache(true);
						auto doR8Upload = [tex, capturedName3,
						                   capturedW = w, capturedH = h,
						                   capturedR8 = std::move(r8)]() {
							if (!tex->uploadR8(capturedR8.data(), capturedW, capturedH))
								Log(LOG_WARNING) << "tileAtlas[" << capturedName3
								                 << "]: R8 GPU upload failed";
						};
						tex->setReloadCb(doR8Upload);
						doR8Upload();
						if (!tex->isValid())
						{
							Log(LOG_WARNING) << "tileAtlas[" << name
							                 << "]: GPU upload failed — falling back to vanilla atlas";
							delete tex;
						}
						else
						{
							_tileAtlases[name] = tex;
							specIt->second.width  = w;
							specIt->second.height = h;
							Log(LOG_INFO) << "tileAtlas[" << name << "]: loaded HD atlas "
							              << w << "x" << h << " (RGBA PNG → R8 palette)";
							loaded = true;
						}
					}
				}
			}
			if (!loaded) _tileAtlasSpecs.erase(specIt);
			if (loaded)
			{
				// YAML HD specs only describe mcdIdx → atlas cell via frameMap,
				// but Map.cpp tries pckToAtlas[animPCK] first when resolving
				// animation frames. Without an entry, every animation step
				// falls back to frameMap[mcdIdx] = primary frame's cell —
				// freezing the animation. Vanilla SAND MCD#18 (deep-shadow)
				// animates through sand-ripple PCK frames; missing this
				// mapping painted MCD#18 tiles as a constant deep-shadow
				// sprite (visually black) instead of the cycling sand-ripple.
				//
				// Auto-build identity pckToAtlas: assume cell N contains the
				// sprite for PCK frame N. gen-sand-atlas.py / gen-blanks-atlas.py
				// both lay out vanilla baseline cells at identity offsets, and
				// HD overrides occupy the same cells for conceptually-equivalent
				// sprites — so identity remains correct.
				TileAtlasSpec& s = specIt->second;
				if (s.tileWidth > 0 && s.tileHeight > 0 && s.pckToAtlas.empty())
				{
					const int totalCells = (s.width / s.tileWidth) * (s.height / s.tileHeight);
					for (int n = 0; n < totalCells; ++n)
						s.pckToAtlas[n] = n;
					Log(LOG_INFO) << "tileAtlas[" << name << "]: auto-built identity "
					              << "pckToAtlas (" << totalCells << " entries)";
				}
				Log(LOG_INFO) << "[CALYPSO] activeDataset " << name;
				return;
			}
			// Fall through to vanilla synthesiser.
		}
	}
	std::map<int,int> frameMap;
	std::map<int,int> pckToAtlas;
	GpuTexture* tex = buildVanillaAtlas(*mds, palette, ncolors, frameMap, pckToAtlas);
	if (!tex) return;

	// Discard any stale entry (shouldn't happen, but be safe).
	auto old = _tileAtlases.find(name);
	if (old != _tileAtlases.end())
	{
		delete old->second;
	}
	_tileAtlases[name] = tex;

	// Store the frame map inside a TileAtlasSpec so downstream code has a
	// uniform lookup path regardless of whether the atlas came from YAML or
	// was synthesised here.
	TileAtlasSpec& spec   = _tileAtlasSpecs[name];
	spec.dataset          = name;
	spec.file             = "";           // synthesised — no file path
	spec.width            = tex->width();
	spec.height           = tex->height();
	spec.tileWidth        = 64;
	spec.tileHeight       = 80;
	spec.columns          = 16;
	spec.frameMap         = std::move(frameMap);
	spec.pckToAtlas       = std::move(pckToAtlas);
}

void Mod::clearTileAtlases()
{
	for (auto& pair : _tileAtlases)
		delete pair.second;
	_tileAtlases.clear();

	// Phase 17: delete hybrid overlay atlases stored in TileAtlasSpec (not in _tileAtlases).
	for (auto& pair : _tileAtlasSpecs)
	{
		if (pair.second.overlayAtlas)
		{
			delete pair.second.overlayAtlas;
			pair.second.overlayAtlas = nullptr;
		}
		// Phase 25 R3: delete the normal atlas (owned by TileAtlasSpec, like overlay).
		if (pair.second.normalAtlas)
		{
			delete pair.second.normalAtlas;
			pair.second.normalAtlas = nullptr;
		}
		// Phase 25 R6: delete the emissive atlas (owned by TileAtlasSpec, like overlay).
		if (pair.second.emissiveAtlas)
		{
			delete pair.second.emissiveAtlas;
			pair.second.emissiveAtlas = nullptr;
		}
		// Phase 20.5 leak fix: sub-layer atlases are pushed in ensureVanillaAtlas
		// but were never freed here (only overlayAtlas was). Release them too.
		for (GpuTexture* lt : pair.second.subLayerAtlases) delete lt;
		pair.second.subLayerAtlases.clear();
	}
}

void Mod::evictTileAtlasGL()
{
	if (_tileAtlasGpuEvicted) return;
	// Deduplicate: collect every non-null GpuTexture* before calling evictGL()
	// so a pointer aliased across _tileAtlases and _tileAtlasSpecs is only evicted once.
	std::set<GpuTexture*> seen;
	int n = 0;
	auto evictOne = [&](GpuTexture* t) {
		if (t && seen.insert(t).second) { t->evictGL(); ++n; }
	};
	for (auto& pair : _tileAtlases)
		evictOne(pair.second);
	for (auto& pair : _tileAtlasSpecs)
	{
		auto& s = pair.second;
		evictOne(s.overlayAtlas);
		evictOne(s.normalAtlas);
		evictOne(s.emissiveAtlas);
		for (GpuTexture* lt : s.subLayerAtlases) evictOne(lt);
	}
	// Unit atlases are battle-only; free them on geoscape too.
	for (auto& pair : _unitAtlases)
	{
		evictOne(pair.second.atlas);
		// Phase 42 E1: RGBA overlay pages.
		for (GpuTexture* p : pair.second.rgbaOverlayPages) evictOne(p);
	}
	_tileAtlasGpuEvicted = true;
	Log(LOG_INFO) << "[L5] evictTileAtlasGL: released " << n << " atlas GL handle(s)";
}

void Mod::restoreTileAtlasGL()
{
	if (!_tileAtlasGpuEvicted) return;
	std::set<GpuTexture*> seen;
	int n = 0;
	auto restoreOne = [&](GpuTexture* t) {
		if (t && seen.insert(t).second) { t->reupload(); ++n; }
	};
	for (auto& pair : _tileAtlases)
		restoreOne(pair.second);
	for (auto& pair : _tileAtlasSpecs)
	{
		auto& s = pair.second;
		restoreOne(s.overlayAtlas);
		restoreOne(s.normalAtlas);
		restoreOne(s.emissiveAtlas);
		for (GpuTexture* lt : s.subLayerAtlases) restoreOne(lt);
	}
	for (auto& pair : _unitAtlases)
	{
		restoreOne(pair.second.atlas);
		// Phase 42 E1: RGBA overlay pages re-decode via their MEMFS reload cb.
		for (GpuTexture* p : pair.second.rgbaOverlayPages) restoreOne(p);
	}
	_tileAtlasGpuEvicted = false;
	Log(LOG_INFO) << "[L5] restoreTileAtlasGL: reuploaded " << n << " atlas GL handle(s)";
}

void Mod::buildUnitRgbaOverlay(UnitAtlasSpec& spec, const std::string& name,
                               int frameCount)
{
	if (!GpuInit::ready()) return;
	for (GLenum stale = glGetError(); stale != GL_NO_ERROR; stale = glGetError())
		Log(LOG_WARNING) << "unitAtlas[" << name
		                 << "]: clearing pre-existing GL error 0x" << std::hex
		                 << (unsigned)stale << std::dec;
	GLint runtimeMaxTextureSize = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &runtimeMaxTextureSize);
	const GLenum sizeQueryError = glGetError();
	if (sizeQueryError != GL_NO_ERROR || runtimeMaxTextureSize <= 0)
	{
		Log(LOG_WARNING) << "unitAtlas[" << name
		                 << "]: cannot query runtime GL_MAX_TEXTURE_SIZE (error 0x"
		                 << std::hex << (unsigned)sizeQueryError << std::dec
		                 << "); using R8 fallback";
		return;
	}
	const int configuredCap = spec.maxPageSize > 0 ? spec.maxPageSize : 4096;
	const int cap = std::min(configuredCap, (int)runtimeMaxTextureSize);
	if (spec.frameWidth <= 0 || spec.frameHeight <= 0 || spec.rgbaColumns <= 0
	 || frameCount <= 0 || cap <= 0 || spec.frameWidth > cap / spec.rgbaColumns)
	{
		Log(LOG_WARNING) << "unitAtlas[" << name << "]: invalid rgba-overlay "
		                 << "frame/page geometry; using R8 fallback";
		return;
	}
	const int pageW = spec.rgbaColumns * spec.frameWidth;
	if (spec.pages.empty() || spec.pages.size() > (size_t)INT_MAX)
	{
		Log(LOG_WARNING) << "unitAtlas[" << name
		                 << "]: invalid page count; using R8 fallback";
		return;
	}
	std::vector<GpuTexture*> pageTextures;
	pageTextures.reserve(spec.pages.size());
	int totalHdFrames = 0;
	std::vector<uint8_t> hasHd((size_t)frameCount, 0);
	std::vector<int> pageOf((size_t)frameCount, -1);
	std::set<std::string> uniquePaths;
	int pageH = 0;
	int rowsPerPage = 0;
	int framesPerPage = 0;
	auto fail = [&](const std::string& why) {
		for (GpuTexture* tex : pageTextures) delete tex;
		pageTextures.clear();
		Log(LOG_WARNING) << "unitAtlas[" << name << "]: " << why
		                 << "; production overlay disabled, R8 fallback remains";
	};

	for (int pi = 0; pi < (int)spec.pages.size(); ++pi)
	{
		const std::string& path = spec.pages[(size_t)pi];
		if (!uniquePaths.insert(path).second || !FileMap::fileExists(path))
		{
			fail("duplicate or missing page " + path);
			return;
		}
		const FileMap::FileRecord* rec = FileMap::at(path);
		SDL_RWops* rw = rec->getRWops();
		SDL_Surface* raw = IMG_Load_RW(rw, SDL_TRUE);
		if (!raw)
		{
			fail("page decode failed: " + path);
			return;
		}
		SDL_Surface* rgbaSurf = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ABGR8888, 0);
		SDL_FreeSurface(raw);
		if (!rgbaSurf)
		{
			fail("page conversion failed: " + path);
			return;
		}
		if (rgbaSurf->w != pageW || rgbaSurf->h <= 0 || rgbaSurf->h > cap
		 || rgbaSurf->h % spec.frameHeight != 0)
		{
			SDL_FreeSurface(rgbaSurf);
			fail("page geometry is not a bounded whole-frame grid: " + path);
			return;
		}
		if (pi == 0)
		{
			pageH = rgbaSurf->h;
			rowsPerPage = pageH / spec.frameHeight;
			if (rowsPerPage <= 0 || rowsPerPage > INT_MAX / spec.rgbaColumns)
			{
				SDL_FreeSurface(rgbaSurf);
				fail("page grid overflows");
				return;
			}
			framesPerPage = rowsPerPage * spec.rgbaColumns;
			const int maxPages = (frameCount - 1) / framesPerPage + 1;
			if (spec.pages.size() > (size_t)maxPages)
			{
				SDL_FreeSurface(rgbaSurf);
				fail("configured pages exceed the PCK frame range");
				return;
			}
		}
		else if (rgbaSurf->h != pageH)
		{
			SDL_FreeSurface(rgbaSurf);
			fail("all configured pages must use identical dimensions");
			return;
		}
		if (SDL_MUSTLOCK(rgbaSurf)) SDL_LockSurface(rgbaSurf);
		for (int cell = 0; cell < framesPerPage; ++cell)
		{
			const int frameIdx = pi * framesPerPage + cell;
			if (frameIdx >= frameCount) break;
			const int cx = (cell % spec.rgbaColumns) * spec.frameWidth;
			const int cy = (cell / spec.rgbaColumns) * spec.frameHeight;
			bool anyOpaque = false;
			for (int y = 0; y < spec.frameHeight && !anyOpaque; ++y)
			{
				const uint8_t* row = static_cast<const uint8_t*>(rgbaSurf->pixels)
				                   + (size_t)(cy + y) * rgbaSurf->pitch
				                   + (size_t)cx * 4u + 3u; // alpha byte (ABGR8888)
				for (int x = 0; x < spec.frameWidth; ++x)
				{
					if (row[(size_t)x * 4u] != 0) { anyOpaque = true; break; }
				}
			}
			if (anyOpaque)
			{
				hasHd[(size_t)frameIdx] = 1;
				pageOf[(size_t)frameIdx] = pi;
				++totalHdFrames;
			}
		}

		GpuTexture* tex = new GpuTexture(true, GpuTexture::Wrap::ClampToEdge,
		                                 GpuTexture::Filter::Linear);
		tex->setSkipCache(true);
		const bool uploaded = tex->uploadRGBA(
		    static_cast<const uint8_t*>(rgbaSurf->pixels), rgbaSurf->w, rgbaSurf->h);
		if (SDL_MUSTLOCK(rgbaSurf)) SDL_UnlockSurface(rgbaSurf);
		SDL_FreeSurface(rgbaSurf);
		if (!uploaded)
		{
			delete tex;
			fail("GPU upload failed for page " + path);
			return;
		}
		const std::string capturedPath = path;
		const std::string capturedName = name;
		const int expectedW = pageW, expectedH = pageH;
		tex->setReloadCb([tex, capturedPath, capturedName, expectedW, expectedH]() {
			auto reloadFailed = [&capturedName, &capturedPath](const char* why) {
				Log(LOG_WARNING) << "unitAtlas[" << capturedName << "]: page "
				                 << capturedPath << " unavailable after context restore: "
				                 << why << "; using R8 fallback";
			};
			if (!FileMap::fileExists(capturedPath)) { reloadFailed("missing source"); return; }
			SDL_RWops* reloadRw = FileMap::at(capturedPath)->getRWops();
			SDL_Surface* reloadRaw = IMG_Load_RW(reloadRw, SDL_TRUE);
			if (!reloadRaw) { reloadFailed("decode failed"); return; }
			SDL_Surface* reloadRgba = SDL_ConvertSurfaceFormat(
			    reloadRaw, SDL_PIXELFORMAT_ABGR8888, 0);
			SDL_FreeSurface(reloadRaw);
			if (!reloadRgba) { reloadFailed("conversion failed"); return; }
			if (reloadRgba->w != expectedW || reloadRgba->h != expectedH)
			{
				Log(LOG_WARNING) << "unitAtlas[" << capturedName
				                 << "]: page geometry changed during reload";
				SDL_FreeSurface(reloadRgba);
				return;
			}
			if (SDL_MUSTLOCK(reloadRgba)) SDL_LockSurface(reloadRgba);
			const bool restored = tex->uploadRGBA(
				static_cast<const uint8_t*>(reloadRgba->pixels),
				reloadRgba->w, reloadRgba->h);
			if (SDL_MUSTLOCK(reloadRgba)) SDL_UnlockSurface(reloadRgba);
			SDL_FreeSurface(reloadRgba);
			if (!restored) reloadFailed("GPU upload failed");
		});
		pageTextures.push_back(tex);
	}

	if (pageTextures.empty() || framesPerPage <= 0)
	{
		fail("no RGBA overlay pages loaded");
		return;
	}

	spec.rgbaOverlayPages  = std::move(pageTextures);
	spec.rgbaHasHd         = std::move(hasHd);
	spec.rgbaPageOf        = std::move(pageOf);
	spec.rgbaFramesPerPage = framesPerPage;
	spec.rgbaRowsPerPage   = rowsPerPage;
	spec.rgbaPageW         = pageW;
	spec.rgbaPageH         = pageH;

	Log(LOG_INFO) << "unitAtlas[" << name << "]: rgba-overlay "
	              << spec.rgbaOverlayPages.size() << " page(s) "
	              << pageW << "x" << pageH << ", " << totalHdFrames
	              << " HD frames / " << frameCount << " PCK frames";
}

void Mod::ensureUnitAtlas(SurfaceSet* ss, const std::string& name,
                           const SDL_Color* palette, int ncolors)
{
	if (!GpuInit::ready()) return;
	if (!ss) return;

	// A `unitAtlas:` YAML entry may have pre-created this record (carrying RGBA
	// overlay config but no R8 atlas yet). Access-or-create so the declarative
	// overlay config is preserved while the R8 baseline is built into it.
	UnitAtlasSpec& spec = _unitAtlases[name];

	// Phase 42 E2: derive the routine-offset scale from actual source-frame
	// dimensions, not from hard-coded 32x40 constants. Every selected PCK frame
	// shares the same frame box; a malformed heterogeneous SurfaceSet or a
	// non-integral/non-uniform RGBA ratio disables the overlay fail-closed.
	spec.sourceFrameWidth = 0;
	spec.sourceFrameHeight = 0;
	spec.partOffsetScale = 1;
	spec.partOffsetScaleConfigured =
	    spec.rgbaFormat == UnitAtlasSpec::RgbaOverlayFormat::RgbaOverlay;
	spec.partOffsetScaleValid = true;
	for (size_t i = 0; i < ss->getTotalFrames(); ++i)
	{
		const Surface* frame = ss->getFrame((int)i);
		if (!frame) continue;
		const int w = frame->getWidth();
		const int h = frame->getHeight();
		if (spec.sourceFrameWidth == 0)
		{
			spec.sourceFrameWidth = w;
			spec.sourceFrameHeight = h;
		}
		else if (w != spec.sourceFrameWidth || h != spec.sourceFrameHeight)
		{
			spec.partOffsetScaleValid = false;
			break;
		}
	}
	if (spec.partOffsetScaleConfigured)
	{
		const bool dimensionsValid = spec.sourceFrameWidth > 0 && spec.sourceFrameHeight > 0
		    && spec.frameWidth > 0 && spec.frameHeight > 0
		    && spec.frameWidth % spec.sourceFrameWidth == 0
		    && spec.frameHeight % spec.sourceFrameHeight == 0;
		const int scaleX = dimensionsValid ? spec.frameWidth / spec.sourceFrameWidth : 0;
		const int scaleY = dimensionsValid ? spec.frameHeight / spec.sourceFrameHeight : 0;
		spec.partOffsetScaleValid = spec.partOffsetScaleValid
		    && dimensionsValid && scaleX > 0 && scaleX == scaleY;
		if (spec.partOffsetScaleValid)
			spec.partOffsetScale = scaleX;
		else
			Log(LOG_WARNING) << "unitAtlas[" << name << "]: RGBA frame "
			                 << spec.frameWidth << "x" << spec.frameHeight
			                 << " is not a uniform integer scale of source frame "
			                 << spec.sourceFrameWidth << "x" << spec.sourceFrameHeight
			                 << "; overlay and scaled part offsets disabled";
	}

	// R8 baseline (idempotent — never rebuild once atlas != null).
	if (!spec.atlas)
	{
		int atlasW = 0, atlasH = 0, cols = 0;
		GpuTexture* tex = buildUnitAtlas(*ss, palette, ncolors, atlasW, atlasH, cols, name);
		if (tex)
		{
			spec.atlas      = tex;
			spec.atlasW     = atlasW;
			spec.atlasH     = atlasH;
			spec.tileWidth  = 64;
			spec.tileHeight = 80;
			spec.columns    = cols;
		}
	}

	// Phase 42 E1: build/load the optional RGBA overlay pages (idempotent —
	// never rebuild once rgbaOverlayPages is non-empty).
	if (spec.rgbaFormat == UnitAtlasSpec::RgbaOverlayFormat::RgbaOverlay
	 && spec.partOffsetScaleValid
	 && spec.rgbaOverlayPages.empty()
	 && !spec.pages.empty())
	{
		buildUnitRgbaOverlay(spec, name, static_cast<int>(ss->getTotalFrames()));
	}
}

const Mod::UnitAtlasSpec* Mod::getUnitAtlas(const std::string& name) const
{
	auto it = _unitAtlases.find(name);
	return it != _unitAtlases.end() ? &it->second : nullptr;
}

void Mod::clearUnitAtlases()
{
	for (auto& pair : _unitAtlases)
	{
		delete pair.second.atlas;
		pair.second.atlas = nullptr;
		pair.second.atlasW = 0;
		pair.second.atlasH = 0;
		// Phase 42 E1: delete production RGBA overlay pages (owned by Mod).
		for (GpuTexture* p : pair.second.rgbaOverlayPages) delete p;
		pair.second.rgbaOverlayPages.clear();
		pair.second.rgbaHasHd.clear();
		pair.second.rgbaPageOf.clear();
		pair.second.rgbaFramesPerPage = 0;
		pair.second.rgbaRowsPerPage = 0;
		pair.second.rgbaPageW = 0;
		pair.second.rgbaPageH = 0;
		pair.second.sourceFrameWidth = 0;
		pair.second.sourceFrameHeight = 0;
		pair.second.partOffsetScale = 1;
		pair.second.partOffsetScaleConfigured = false;
		pair.second.partOffsetScaleValid = true;
		// Disposable G0 spike atlas (harness-owned lifetime, but cleared here on
		// full mod teardown so a stale pointer can't survive a reload).
		pair.second.g0OverlayAtlas = nullptr;
		pair.second.g0OverlayMask.clear();
	}
	// Keep declarative unitAtlas configuration across Map::setPalette rebuilds.
	// Runtime pointers and metadata above are reset; config-only records are
	// harmless during Mod destruction and are rebuilt on the next battle entry.
}

/*
 * Phase 21.3.0: animation-blind resolver for the Corner-Wang lookup.
 *
 * The render path in Map::emitTilePass resolves the atlas cell from the
 * tile's *animated* frame index (tile->getCurrentFrame(part)) — required
 * for UFO doors and animated water tiles that step through PCK frames each
 * tick. Wang must NOT follow animation: wangType is a static property of
 * the cell. Returning frame-0's atlas-cell index preserves stable Wang
 * behaviour through any animation cycle.
 *
 * Signature note (deviation from v3 plan §21.3.0): the v3 plan called the
 * helper with `(spec, md)` only, then did `spec->frameMap.find(md->getObjectType())`.
 * That is wrong — MapData::getObjectType() returns the TilePart slot
 * (O_FLOOR/O_OBJECT/...), not the MCD record index that frameMap is keyed
 * by. The render path obtains the MCD record index via the out-param
 * overload `tile->getMapData(&mcdIdx, &mdsID, part)`, and Wang's caller
 * (computeWangMask §21.3.1) must do the same and pass mcdIdx in.
 */
static int resolveStaticAtlasCellIndex(const Mod::TileAtlasSpec* spec,
                                       const MapData* md,
                                       int mcdIdx)
{
	if (!spec || !md) return -1;
	// Primary-frame PCK index — what the tile "is", independent of
	// the animation cycle the renderer happens to be on.
	const int primaryPCK = md->getSprite(0);
	if (primaryPCK > 0)
	{
		auto it = spec->pckToAtlas.find(primaryPCK);
		if (it != spec->pckToAtlas.end()) return it->second;
	}
	// Fallback: MCD-record-indexed primary frame, populated by the
	// `frameMap:` ruleset block at parse time. Same fallback the render
	// code uses when pckToAtlas misses (see Map.cpp::emitTilePass).
	auto fit = spec->frameMap.find(mcdIdx);
	if (fit != spec->frameMap.end()) return fit->second;
	return -1;
}

/*
 * Phase 22 (M1 perf): intern a wangType tag to a stable integer id so the
 * per-tile emit scan in computeWangMask compares ints instead of constructing
 * and comparing std::string for self + four neighbours. Interning is global
 * across datasets (same tag → same id) so a cross-dataset "sand" vs "sand"
 * neighbour comparison still matches. Empty tag → -1 ("no wang" / opt-out).
 */
int Mod::internWangType(const std::string& tag)
{
	if (tag.empty()) return -1;
	auto it = _wangTypeIds.find(tag);
	if (it != _wangTypeIds.end()) return it->second;
	const int id = (int)_wangTypeNames.size();
	_wangTypeIds.emplace(tag, id);
	_wangTypeNames.push_back(tag);
	return id;
}

/*
 * Phase 21.3.1: Corner-Wang mask + variant-cell lookup for one tile.
 *
 * Returns:
 *   mask         — 4-bit OR-corner mask (NW=8, NE=4, SE=2, SW=1)
 *   variantCell  — atlas-cell index from the matching wangSet, or -1
 *
 * Algorithm (mirrors the v3 plan §21.3.1):
 *   1. Resolve `self`'s wang-capable cell via a dual-slot scan (FLOOR
 *      then OBJECT). A cell qualifies when its hdTiles[] entry's
 *      `tilePart` matches the slot it was found in AND its effective
 *      wangType is non-empty.
 *   2. typeAt(dx,dy) scans O_OBJECT before O_FLOOR for each orthogonal
 *      neighbour. A Wang-enabled O_OBJECT is a floor-like surface override
 *      (not a decorative object), so it must win over the base floor.
 *   3. Same-type fast-path: if all four orthogonals match self_type,
 *      no transition fires. Critical for homogeneous floors (SEABED's
 *      ~90% uniform sand).
 *   4. OR-corner mask: a corner is foreign if EITHER of its two
 *      adjacent orthogonals is foreign. See docs §21.4.2 for the rule
 *      diagram and the OR-vs-strict trade-off recorded in the plan.
 *   5. First-found collision: when multiple foreign neighbours collide,
 *      the wangSet of the first foreign one in N→E→S→W order wins.
 *
 * `resolveStaticAtlasCellIndex` (§21.3.0) is used everywhere instead of
 * the renderer's animated resolver — wangType is a static property of
 * the cell, so Wang must read frame-0 even mid-animation.
 */
Mod::WangResult Mod::computeWangMask(const TileAtlasSpec* spec,
                                     const Tile* self,
                                     SavedBattleGame* save) const
{
	WangResult result;
	if (!spec || spec->wangSets.empty()) return result;
	if (!self || !save) return result;

	auto resolveSlot = [](const TileAtlasSpec* s, const MapData* md,
	                      int mcdIdx, TilePart part, int* outCell) -> bool
	{
		if (!md || !md->getDataset()) return false;
		const int cell = resolveStaticAtlasCellIndex(s, md, mcdIdx);
		if (cell < 0) return false;
		auto cellIt = s->hdTilesByCell.find(cell);
		if (cellIt == s->hdTilesByCell.end()) return false;
		const int idx = cellIt->second;
		if (idx < 0 || idx >= (int)s->hdTiles.size()) return false;
		const auto& cellSpec = s->hdTiles[idx];
		if (s->effectiveTilePart(cellSpec) != part) return false;
		if (s->effectiveWangTypeId(cellSpec) < 0) return false;
		*outCell = cell;
		return true;
	};

	// 1. Resolve self. MAP authors may park a wang cell in O_FLOOR or
	//    O_OBJECT regardless of the MCD Tile_Type hint — see
	//    docs/qa/phase-21-cross-terrain-floor-inventory.md.
	int selfCell = -1;
	for (TilePart part : {O_FLOOR, O_OBJECT})
	{
		const MapData* md = self->getMapData(part);
		int mcdIdx = 0, mdsID = 0;
		self->getMapData(&mcdIdx, &mdsID, part);
		if (resolveSlot(spec, md, mcdIdx, part, &selfCell)) break;
	}
	if (selfCell < 0) return result;

	const auto& selfSpec = spec->hdTiles[spec->hdTilesByCell.at(selfCell)];
	const int self_typeId = spec->effectiveWangTypeId(selfSpec);

	// 2. typeAt does the same dual-slot scan for each orthogonal neighbour,
	//    against that neighbour's own dataset spec (a cross-dataset MAP can
	//    have SAND under one tile and BLANKS under the next).
	const Position pos = self->getPosition();
	auto typeAt = [&](int dx, int dy) -> int {
		const Tile* t = save->getTile(Position(pos.x + dx, pos.y + dy, pos.z));
		if (!t) return -1;
		// Some SEABED surfaces (notably SAND#19 pale-sand) live in O_OBJECT
		// above a regular O_FLOOR. Check the Wang-enabled override first;
		// ordinary objects have no Wang type and therefore fall through.
		for (TilePart part : {O_OBJECT, O_FLOOR})
		{
			const MapData* md = t->getMapData(part);
			if (!md || !md->getDataset()) continue;
			const auto* nspec = getTileAtlasSpec(md->getDataset()->getName());
			if (!nspec) continue;
			int mcdIdx = 0, mdsID = 0;
			t->getMapData(&mcdIdx, &mdsID, part);
			int nCell = -1;
			if (!resolveSlot(nspec, md, mcdIdx, part, &nCell)) continue;
			const int nTypeId = nspec->effectiveWangTypeId(
				nspec->hdTiles[nspec->hdTilesByCell.at(nCell)]);
			if (nTypeId >= 0) return nTypeId;
		}
		return -1;
	};

	const int n_n = typeAt( 0, -1);
	const int n_e = typeAt( 1,  0);
	const int n_s = typeAt( 0,  1);
	const int n_w = typeAt(-1,  0);

	// 3. Same-type fast-path: all four orthogonals match self → no
	//    transition possible, skip mask + wangSet lookup entirely.
	if (n_n == self_typeId && n_e == self_typeId
	 && n_s == self_typeId && n_w == self_typeId)
		return result;

	auto foreign = [&](int n) {
		return n >= 0 && n != self_typeId;
	};

	// 4. OR-corner mask: a corner is foreign iff EITHER adjacent
	//    orthogonal is foreign. (Strict-corner — both must be foreign —
	//    is recorded as a deferred alternative in plan §21.4.2.)
	const bool nw = foreign(n_n) || foreign(n_w);
	const bool ne = foreign(n_n) || foreign(n_e);
	const bool se = foreign(n_s) || foreign(n_e);
	const bool sw = foreign(n_s) || foreign(n_w);
	result.mask = (uint8_t)((nw << 3) | (ne << 2) | (se << 1) | sw);
	if (result.mask == 0) return result;

	// 5. First-found collision in N→E→S→W order picks the wangSet.
	//    §21.4.2: if the tile has no entry for the picked neighbour,
	//    variantCell stays -1 (renderer falls through to the base cell).
	int winId = -1;
	int winDx = 0, winDy = 0;
	for (int i = 0; i < 4; ++i)
	{
		const int n = (i == 0) ? n_n : (i == 1) ? n_e : (i == 2) ? n_s : n_w;
		if (foreign(n)) {
			winId = n;
			if      (i == 0) { winDx =  0; winDy = -1; }
			else if (i == 1) { winDx =  1; winDy =  0; }
			else if (i == 2) { winDx =  0; winDy =  1; }
			else             { winDx = -1; winDy =  0; }
			break;
		}
	}
	if (winId < 0) return result;

	// Map the winning neighbour id back to its tag for the (rare) wangSet lookup.
	const std::string& lookup = _wangTypeNames[winId];
	auto wit = spec->wangSets.find(lookup);
	if (wit == spec->wangSets.end()) return result;
	const WangNeighbour& neigh = wit->second;
	result.blend   = neigh.blend;
	result.matched = &neigh;
	if (neigh.blend)
	{
		result.surfaceCell  = neigh.surfaceCell;
		result.neighbourDx  = winDx;
		result.neighbourDy  = winDy;
	}
	else
	{
		result.variantCell = neigh.variantCells[result.mask];
	}
	return result;
}

void Mod::loadFileCalypso(YAML::YamlNodeReader& reader)
{
	// Phase 36: re-declared loadFile glue so the extracted ruleset parsing below
	// stays a byte-identical move. InfoTag mirrors the file-scope const in Mod.cpp
	// (internal linkage there, so it cannot be shared across translation units).
	const std::string InfoTag = "!info";
	auto loadDocInfoHelper = [&](const char* nodeName)
	{
		if (reader.hasValTag(InfoTag))
		{
			Logger info;
			info.get() << "Available rule '" << nodeName << ":'";
		}
		return reader[nodeName];
	};
	auto iterateRulesSpecific = [&](const char* nodeName)
	{
		const auto& node = loadDocInfoHelper(nodeName);
		return node.children();
	};

	int _globeTexUploaded = 0;  // M5b: count uploads to suppress the mark on ruleset files with no globeTextures
	for (const auto& ruleReader : iterateRulesSpecific("globeTextures"))
	{
		if (!GpuInit::ready()) continue;
		std::string id;
		ruleReader["id"].tryReadVal<std::string>(id);
		if (id.empty()) continue;

		auto filesNode = ruleReader["files"];
		if (!filesNode) continue;

		// Only mip 0 is required; glGenerateMipmap fills the rest.
		std::string relPath;
		filesNode["0"].tryReadVal<std::string>(relPath);
		if (relPath.empty()) continue;

		bool equirect = false;
		ruleReader["equirectangular"].tryReadVal<bool>(equirect);
		const auto wrap = equirect ? GpuTexture::Wrap::RepeatS_ClampT
		                           : GpuTexture::Wrap::ClampToEdge;

		// FileMap::at() throws Exception on miss, so a plain `!rec` test would
		// never fire — the throw bubbles all the way up and the entire mod is
		// disabled by the ruleset loader. Guard with fileExists() so optional
		// HD globe textures (gitignored, sometimes absent on CI runners) are
		// soft-missing instead of mod-killing.
		if (!FileMap::fileExists(relPath))
		{
			Log(LOG_WARNING) << "globeTextures[" << id << "]: file not found: " << relPath;
			continue;
		}
		const FileMap::FileRecord* rec = FileMap::at(relPath);

		// WebP: use WebPDecodeRGBA() directly — SDL2_image's sdl2_image port
		// has no libwebp in SUPPORTED_FORMATS.  Output is R,G,B,A in memory
		// order, matching GL_RGBA + GL_UNSIGNED_BYTE exactly.
		const bool isWebP = relPath.size() >= 5 &&
		                    relPath.compare(relPath.size() - 5, 5, ".webp") == 0;
		if (isWebP)
		{
			/* L3: skip _cachedData; re-decode from MEMFS on context loss. */
			GpuTexture* tex = new GpuTexture(/*srgb=*/true, wrap);
			tex->setSkipCache(true);
			auto doUpload = [this, tex, relPath, id]() {
				if (!FileMap::fileExists(relPath))
				{
					Log(LOG_WARNING) << "globeTextures[" << id << "]: file not found on reload: " << relPath;
					return;
				}
				const FileMap::FileRecord* rec2 = FileMap::at(relPath);
				SDL_RWops* rw = rec2->getRWops();
				Sint64 fileSize = SDL_RWsize(rw);
				if (fileSize <= 0)
				{
					Log(LOG_WARNING) << "globeTextures[" << id << "]: RWsize failed";
					SDL_RWclose(rw);
					return;
				}
				std::vector<uint8_t> buf(static_cast<size_t>(fileSize));
				if (SDL_RWread(rw, buf.data(), 1, buf.size()) != buf.size())
				{
					Log(LOG_WARNING) << "globeTextures[" << id << "]: RWread failed";
					SDL_RWclose(rw);
					return;
				}
				SDL_RWclose(rw);
				int w = 0, h = 0;
				uint8_t* pixels = WebPDecodeRGBA(buf.data(), buf.size(), &w, &h);
				if (!pixels)
				{
					Log(LOG_WARNING) << "globeTextures[" << id << "]: WebPDecodeRGBA failed";
					return;
				}
				if (!tex->uploadRGBA(pixels, w, h, 0))
					Log(LOG_WARNING) << "globeTextures[" << id << "]: GpuTexture upload failed";
				else
					Log(LOG_INFO) << "globeTextures[" << id << "]: " << w << "x" << h << " uploaded (WebP RGBA)";
				WebPFree(pixels);
			};
			tex->setReloadCb(doUpload);
			doUpload();
			if (tex->isValid())
			{
				delete _globeTextures[id]; // free any previous only after the new one succeeds
				_globeTextures[id] = tex;
				++_globeTexUploaded;  // M5b
			}
			else
				delete tex; // keep any previous texture intact on failure
			continue;
		}

		// JPEG / PNG: load via SDL_image, then convert to ABGR8888.
		// SDL_PIXELFORMAT_ABGR8888 on little-endian (WASM) gives memory layout
		// [R, G, B, A], which is what GL_RGBA + GL_UNSIGNED_BYTE expects.
		// (SDL_PIXELFORMAT_RGBA8888 would give [A, B, G, R] — wrong.)
		/* L3: skip _cachedData; re-decode from MEMFS on context loss. */
		GpuTexture* tex = new GpuTexture(/*srgb=*/true, wrap);
		tex->setSkipCache(true);
		auto doUpload = [this, tex, relPath, id]() {
			if (!FileMap::fileExists(relPath))
			{
				Log(LOG_WARNING) << "globeTextures[" << id << "]: file not found on reload: " << relPath;
				return;
			}
			const FileMap::FileRecord* rec2 = FileMap::at(relPath);
			SDL_RWops* rw = rec2->getRWops();
			SDL_Surface* raw = IMG_Load_RW(rw, SDL_TRUE); // SDL_TRUE = auto-close rw
			if (!raw)
			{
				Log(LOG_WARNING) << "globeTextures[" << id << "]: IMG_Load_RW failed: " << IMG_GetError();
				return;
			}
			SDL_Surface* rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ABGR8888, 0);
			SDL_FreeSurface(raw);
			if (!rgba)
			{
				Log(LOG_WARNING) << "globeTextures[" << id << "]: ConvertSurface failed";
				return;
			}
			if (!tex->uploadRGBA(static_cast<const uint8_t*>(rgba->pixels), rgba->w, rgba->h, 0))
				Log(LOG_WARNING) << "globeTextures[" << id << "]: GpuTexture upload failed";
			else
				Log(LOG_INFO) << "globeTextures[" << id << "]: " << rgba->w << "x" << rgba->h << " uploaded";
			SDL_FreeSurface(rgba);
		};
		tex->setReloadCb(doUpload);
		doUpload();
		if (tex->isValid())
		{
			delete _globeTextures[id]; // free any previous only after the new one succeeds
			_globeTextures[id] = tex;
			++_globeTexUploaded;  // M5b
		}
		else
			delete tex; // keep any previous texture intact on failure
	}
	{
		const char* required[] = {"bathymetry", "diffuse", "night", "clouds"};
		int loaded = 0;
		for (auto* k : required) loaded += (int)_globeTextures.count(k);
		if (loaded > 0 && loaded != 4)
		{
			Log(LOG_WARNING) << "Phase 8c: HD globe disabled — loaded "
			                 << loaded << "/4 required textures";
			clearGlobeTextures();
		}
	}
	if (_globeTexUploaded > 0)
		calypso_log_heap("globe-textures");  // M5: after GPU upload; only fires when ≥1 texture uploaded this file
	for (const auto& ruleReader : iterateRulesSpecific("tileAtlas"))
	{
		std::string dataset;
		ruleReader["dataset"].tryReadVal<std::string>(dataset);
		if (dataset.empty()) continue;

		TileAtlasSpec spec;
		spec.dataset = dataset;
		ruleReader["file"].tryReadVal<std::string>(spec.file);
		ruleReader["width"].tryReadVal<int>(spec.width);
		ruleReader["height"].tryReadVal<int>(spec.height);
		ruleReader["tileWidth"].tryReadVal<int>(spec.tileWidth);
		ruleReader["tileHeight"].tryReadVal<int>(spec.tileHeight);
		ruleReader["columns"].tryReadVal<int>(spec.columns);

		{
			std::string fmtStr;
			ruleReader["format"].tryReadVal<std::string>(fmtStr);
			if (fmtStr == "rgba")
			{
				spec.format = TileAtlasSpec::Format::Rgba;
			}
			else if (!fmtStr.empty() && fmtStr != "palette")
			{
				Log(LOG_WARNING) << "tileAtlas[" << dataset << "]: unknown format '"
				                 << fmtStr << "', defaulting to palette";
			}
		}

		// Phase 17: hybrid dual-atlas fields
		ruleReader["hybrid"].tryReadVal<bool>(spec.hybrid);
		if (spec.hybrid)
		{
			ruleReader["baselineFile"].tryReadVal<std::string>(spec.baselineFile);
			ruleReader["overlayFile"].tryReadVal<std::string>(spec.overlayFile);
		}
		// Phase 25 R3: normal-map atlas path (optional; applies to all modes).
		ruleReader["normalFile"].tryReadVal<std::string>(spec.normalFile);
		// Phase 25 R6: material emissive atlas path (optional; applies to all modes).
		ruleReader["emissiveFile"].tryReadVal<std::string>(spec.emissiveFile);

		// Phase 20: baseline mode (vanilla = default, none = skip R8 pass)
		{
			std::string baselineStr;
			ruleReader["baseline"].tryReadVal<std::string>(baselineStr);
			if (baselineStr == "none")
				spec.baseline = BaselineMode::None;
			// "vanilla" or missing → BaselineMode::Vanilla (default)
		}

		// Phase 20: atlas-wide authoring fields
		ruleReader["bleed"].tryReadVal<int>(spec.bleed);
		ruleReader["premultipliedAlpha"].tryReadVal<bool>(spec.premultipliedAlpha);
		ruleReader["fallbackImage"].tryReadVal<std::string>(spec.fallbackImage);
		ruleReader["fallbackOpacity"].tryReadVal<float>(spec.fallbackOpacity);

		// Phase 27: world-position ground super-tile (base/tilesX/tilesY).
		auto groundPoolNode = ruleReader["groundPool"];
		if (groundPoolNode)
		{
			groundPoolNode["base"].tryReadVal<int>(spec.groundBase);
			groundPoolNode["tilesX"].tryReadVal<int>(spec.groundTilesX);
			groundPoolNode["tilesY"].tryReadVal<int>(spec.groundTilesY);
		}

		// Phase 20: per-cell hdTiles[] metadata
		auto hdTilesNode = ruleReader["hdTiles"];
		if (hdTilesNode)
		{
			for (const auto& tileNode : hdTilesNode.children())
			{
				HDTileSpec ts;
				tileNode["cell"].tryReadVal<int>(ts.cell);
				tileNode["image"].tryReadVal<std::string>(ts.image);
				tileNode["mask"].tryReadVal<std::string>(ts.mask);
				tileNode["opacity"].tryReadVal<float>(ts.opacity);
				tileNode["zBias"].tryReadVal<int>(ts.zBias);
				auto anchorNode = tileNode["anchor"];
				if (anchorNode && anchorNode.children().size() == 2)
				{
					anchorNode[(size_t)0].tryReadVal<int>(ts.anchor[0]);
					anchorNode[(size_t)1].tryReadVal<int>(ts.anchor[1]);
				}
				// parse subLayers[] recursively
				auto subLayersNode = tileNode["subLayers"];
				if (subLayersNode)
				{
					for (const auto& slNode : subLayersNode.children())
					{
						HDTileSpec sl;
						slNode["image"].tryReadVal<std::string>(sl.image);
						slNode["mask"].tryReadVal<std::string>(sl.mask);
						slNode["opacity"].tryReadVal<float>(sl.opacity);
						slNode["zBias"].tryReadVal<int>(sl.zBias);
						auto slAnchorNode = slNode["anchor"];
						if (slAnchorNode && slAnchorNode.children().size() == 2)
						{
							slAnchorNode[(size_t)0].tryReadVal<int>(sl.anchor[0]);
							slAnchorNode[(size_t)1].tryReadVal<int>(sl.anchor[1]);
						}
						// Phase 22.7: state gate. "fire" → emit only while the tile burns;
						// "always"/absent → unconditional (legacy).
						std::string slCond;
						if (slNode["condition"].tryReadVal<std::string>(slCond))
						{
							if      (slCond == "fire")   sl.condition = HDTileSpec::COND_FIRE;
							else if (slCond == "always") sl.condition = HDTileSpec::COND_ALWAYS;
							else Log(LOG_WARNING) << "tileAtlas[" << dataset
							                      << "]: subLayer condition '" << slCond
							                      << "' unknown, treating as always";
						}
						ts.subLayers.push_back(std::move(sl));
					}
				}
				// Phase 21: per-cell wangType with presence flag. Empty string is a
				// meaningful explicit value ("opt-out of Wang"); absence inherits the
				// dataset default. tryReadVal returns true iff the key was present.
				if (tileNode["wangType"].tryReadVal<std::string>(ts.wangType))
				{
					ts.hasWangType = true;
					// Phase 22 (M1 perf): intern now so the emit-time neighbour scan
					// compares ints. internWangType("") returns -1 (explicit opt-out).
					ts.wangTypeId = internWangType(ts.wangType);
				}
				// Phase 21: per-cell TilePart slot hint. Accepts "O_FLOOR" or "O_OBJECT".
				// Required for cells the MAP places in the object slot (e.g. SAND#19).
				std::string tilePartStr;
				if (tileNode["tilePart"].tryReadVal<std::string>(tilePartStr))
				{
					if      (tilePartStr == "O_OBJECT") { ts.tilePart = O_OBJECT; ts.hasTilePart = true; }
					else if (tilePartStr == "O_FLOOR")  { ts.tilePart = O_FLOOR;  ts.hasTilePart = true; }
					else Log(LOG_WARNING) << "tileAtlas[" << dataset << "]: hdTiles[cell="
					                      << ts.cell << "].tilePart: unknown value '"
					                      << tilePartStr << "', leaving default";
				}
				// Phase 22: anti-repeat variant count; base cell carries metadata,
				// cells [cell..cell+variants-1] form the visual pool (§22.5).
				tileNode["variants"].tryReadVal<int>(ts.variants);
				if (ts.variants < 1) ts.variants = 1;
				// Phase 22 (M3): clamp the anti-repeat pool to the atlas grid so a
				// misauthored variants count cannot push visualCell (cell + hash%variants)
				// past the atlas and sample a wrong/edge cell on the GPU. Mirrors the
				// wangSet surfaceVariants guard. spec dimensions are parsed above.
				if (ts.variants > 1 && spec.tileWidth > 0 && spec.tileHeight > 0
				 && spec.width > 0 && spec.height > 0)
				{
					const int nCells = (spec.width / spec.tileWidth)
					                 * (spec.height / spec.tileHeight);
					if (ts.cell < 0 || ts.cell + ts.variants - 1 >= nCells)
					{
						Log(LOG_WARNING) << "tileAtlas[" << dataset << "]: hdTiles[cell="
						                 << ts.cell << "].variants=" << ts.variants
						                 << " exceeds atlas cell count " << nCells
						                 << "; clamping to 1";
						ts.variants = 1;
					}
				}
				spec.hdTiles.push_back(std::move(ts));
			}
		}

		auto frameMapNode = ruleReader["frameMap"];
		if (frameMapNode)
		{
			for (const auto& child : frameMapNode.children())
			{
				int mcdIdx = 0, atlasIdx = 0;
				if (child.tryReadKey<int>(mcdIdx) && child.tryReadVal<int>(atlasIdx))
					spec.frameMap[mcdIdx] = atlasIdx;
			}
		}

		// Phase 21: dataset-level wangType + wangTilePart + wangSet[] variant tables.
		// wangType absence == no default neighbour-tag (cells without an own wangType
		// then have no Wang transition either). wangSet[] entries map neighbour tag
		// to per-mask atlas cell; mask is the 4-bit corner-coverage key (0..15).
		ruleReader["wangType"].tryReadVal<std::string>(spec.wangType);
		// Phase 22 (M1 perf): intern the dataset-default tag once for effectiveWangTypeId.
		spec.wangTypeIdDefault = internWangType(spec.wangType);
		std::string dsTilePartStr;
		if (ruleReader["wangTilePart"].tryReadVal<std::string>(dsTilePartStr))
		{
			if      (dsTilePartStr == "O_OBJECT") spec.wangTilePart = O_OBJECT;
			else if (dsTilePartStr == "O_FLOOR")  spec.wangTilePart = O_FLOOR;
			else Log(LOG_WARNING) << "tileAtlas[" << dataset
			                      << "]: wangTilePart: unknown value '"
			                      << dsTilePartStr << "', leaving default O_FLOOR";
		}
		auto wangSetNode = ruleReader["wangSet"];
		if (wangSetNode)
		{
			for (const auto& wsNode : wangSetNode.children())
			{
				std::string neighbour;
				wsNode["neighbour"].tryReadVal<std::string>(neighbour);
				if (neighbour.empty()) continue;
				WangNeighbour wn;
				// Phase 22: mode field selects bake (default, Phase 21 compat) or blend.
				std::string mode = "bake";
				wsNode["mode"].tryReadVal<std::string>(mode);
				wn.blend = (mode == "blend");
				if (wn.blend)
				{
					wsNode["surfaceCell"].tryReadVal<int>(wn.surfaceCell);
					wsNode["surfaceVariants"].tryReadVal<int>(wn.surfaceVariants);
					if (wn.surfaceVariants < 1) wn.surfaceVariants = 1;
					wsNode["feather"].tryReadVal<float>(wn.feather);
					wsNode["noiseScale"].tryReadVal<float>(wn.noiseScale);
					wsNode["noiseAmp"].tryReadVal<float>(wn.noiseAmp);
				}
				else
				{
					// bake (Phase 21): read atlasCells mask→cell map
					auto cellsNode = wsNode["atlasCells"];
					if (cellsNode)
					{
						for (const auto& cellNode : cellsNode.children())
						{
							int maskKey = 0, cellIdx = -1;
							if (cellNode.tryReadKey<int>(maskKey)
							 && cellNode.tryReadVal<int>(cellIdx)
							 && maskKey >= 0 && maskKey < 16)
							{
								wn.variantCells[maskKey] = cellIdx;
							}
						}
					}
				}
				// Bounds-check surfaceCell against the atlas grid so an out-of-range
				// value degrades to no-blend rather than sampling a wrong UV on the GPU.
				if (wn.blend && spec.tileWidth > 0 && spec.tileHeight > 0
				 && spec.width > 0 && spec.height > 0)
				{
					int nCells = (spec.width / spec.tileWidth) * (spec.height / spec.tileHeight);
					if (wn.surfaceCell < 0 || wn.surfaceCell >= nCells
					 || wn.surfaceCell + wn.surfaceVariants - 1 >= nCells)
					{
						Log(LOG_WARNING) << "tileAtlas[" << dataset << "]: wangSet neighbour '"
						                 << neighbour << "': surfaceCell=" << wn.surfaceCell
						                 << " variants=" << wn.surfaceVariants
						                 << " out of range [0," << nCells << "); blend disabled";
						wn.blend = false;
					}
				}
				spec.wangSets[neighbour] = std::move(wn);
			}
		}

		// Build O(1) cell→hdTiles index lookup.
		for (int hi = 0; hi < (int)spec.hdTiles.size(); ++hi)
			spec.hdTilesByCell[spec.hdTiles[hi].cell] = hi;

		_tileAtlasSpecs[dataset] = std::move(spec);
		_hdPackActive = true;
		Log(LOG_INFO) << "tileAtlas[" << dataset << "]: registered "
		              << _tileAtlasSpecs[dataset].frameMap.size() << " frameMap entries"
		              << " hdTiles=" << _tileAtlasSpecs[dataset].hdTiles.size()
		              << " baseline=" << (_tileAtlasSpecs[dataset].baseline == BaselineMode::None ? "none" : "vanilla")
		              << " wangType='" << _tileAtlasSpecs[dataset].wangType << "'"
		              << " wangSets=" << _tileAtlasSpecs[dataset].wangSets.size();
	}
	{
		int v = _battlescapeTileScale;
		reader["battlescapeTileScale"].tryReadVal<int>(v);
		if (v == 1 || v == 2 || v == 4)
			_battlescapeTileScale = v;  // assign 1 too, so a later ruleset can reset to native
		else
			Log(LOG_WARNING) << "battlescapeTileScale: " << v << " is not supported (use 1, 2, or 4); ignored";
	}
	// Phase 42 E1: production sparse per-PCK-frame RGBA overlay pages.
	// Configures the EXISTING Mod::UnitAtlasSpec for a unit spriteSheet with an
	// optional rgba-overlay (HD frames layered over the R8 baseline). The R8
	// atlas + RGBA pages are built later by ensureUnitAtlas() at battle time
	// (GPU-ready, SurfaceSet loaded). `sheet:` is the SurfaceSet name (e.g.
	// TDXCOM_0.PCK / HANDOB.PCK). Per D1 there is no second pose key: each PCK
	// frame keeps its index; missing/transparent overlay slots fall back to R8.
	for (const auto& ruleReader : iterateRulesSpecific("unitAtlas"))
	{
		std::string sheet;
		ruleReader["sheet"].tryReadVal<std::string>(sheet);
		if (sheet.empty())
		{
			Log(LOG_WARNING) << "unitAtlas: entry missing 'sheet'; skipped";
			continue;
		}
		UnitAtlasSpec& spec = _unitAtlases[sheet]; // access-or-create (atlas built later)
		// A later ruleset entry replaces the declarative overlay configuration for
		// this sheet. Runtime textures are built only after all rules load.
		spec.rgbaFormat = UnitAtlasSpec::RgbaOverlayFormat::None;
		spec.frameWidth = 0;
		spec.frameHeight = 0;
		spec.rgbaColumns = 16;
		spec.maxPageSize = 4096;
		spec.pages.clear();
		{
			std::string fmtStr;
			ruleReader["format"].tryReadVal<std::string>(fmtStr);
			if (fmtStr == "rgba-overlay")
				spec.rgbaFormat = UnitAtlasSpec::RgbaOverlayFormat::RgbaOverlay;
			else if (!fmtStr.empty())
				Log(LOG_WARNING) << "unitAtlas[" << sheet << "]: unknown format '"
				                 << fmtStr << "' (expected 'rgba-overlay'); ignored";
		}
		ruleReader["frameWidth"].tryReadVal<int>(spec.frameWidth);
		ruleReader["frameHeight"].tryReadVal<int>(spec.frameHeight);
		ruleReader["columns"].tryReadVal<int>(spec.rgbaColumns);
		ruleReader["maxPageSize"].tryReadVal<int>(spec.maxPageSize);
		if (spec.rgbaColumns <= 0) spec.rgbaColumns = 16;
		if (spec.maxPageSize <= 0) spec.maxPageSize = 4096;
		auto pagesNode = ruleReader["pages"];
		if (pagesNode)
		{
			for (const auto& pNode : pagesNode.children())
			{
				std::string path;
				pNode.tryReadVal<std::string>(path);
				if (!path.empty()) spec.pages.push_back(path);
			}
		}
		_hdPackActive = true;
		Log(LOG_INFO) << "unitAtlas[" << sheet << "]: registered rgba-overlay "
		              << "format=" << (spec.rgbaFormat == UnitAtlasSpec::RgbaOverlayFormat::RgbaOverlay ? "rgba-overlay" : "none")
		              << " frame=" << spec.frameWidth << "x" << spec.frameHeight
		              << " columns=" << spec.rgbaColumns
		              << " maxPageSize=" << spec.maxPageSize
		              << " pages=" << spec.pages.size();
	}
	// Phase 25 R5: floating unit nameplates / HP-TU-energy bars (off by default).
	reader["calypso_hud_overlay"].tryReadVal<bool>(_calypsoHudOverlay);
	// Phase 38: trade & economy ruleset. Disabled unless the key is present (kill-switch).
	Calypso::loadEconomyRules(reader["calypsoEconomy"], _calypsoEconomyRules);
	// Phase 37 (Calypso): parse the top-level `tutorial:` block (content lives
	// in the calypso-tutorial mod). Each step is built from its mapping; a
	// step missing id/trigger/pages is logged and skipped per the ruleset
	// schema (phase-37-tutorial.md §2). chainDelayMs is read+discarded.
	if (reader["tutorial"])
	{
		auto tutorialReader = reader["tutorial"];
		if (tutorialReader["steps"])
		{
			for (const auto& entry : tutorialReader["steps"].children())
			{
				CalypsoTutorialStep step;
				step.id         = entry["id"].readVal<std::string>("");
				step.trigger    = entry["trigger"].readVal<std::string>("");
				step.triggerArg = entry["triggerArg"].readVal<std::string>("");
				step.triggerArgs = entry["triggerArgs"].readVal<std::vector<std::string>>(std::vector<std::string>{});
				step.anchor     = entry["anchor"].readVal<std::string>("");
				step.pages      = entry["pages"].readVal<std::vector<std::string>>(std::vector<std::string>{});
				step.pageAnchors= entry["pageAnchors"].readVal<std::vector<std::string>>(std::vector<std::string>{});
				// chainDelayMs is reserved for future use — read + discard.
				int chainDelayMs = 0;
				entry["chainDelayMs"].tryReadVal<int>(chainDelayMs);
				(void)chainDelayMs;
				if (step.id.empty() || step.trigger.empty() || step.pages.empty())
				{
					Log(LOG_WARNING) << "tutorial: step missing id/trigger/pages; skipped";
					continue;
				}
				_calypsoTutorialSteps.push_back(std::move(step));
			}
		}
		if (tutorialReader["checklist"])
		{
			for (const auto& entry : tutorialReader["checklist"].children())
			{
				CalypsoChecklistItem item;
				item.id        = entry["id"].readVal<std::string>("");
				item.label     = entry["label"].readVal<std::string>("");
				item.check     = entry["check"].readVal<std::string>("");
				item.checkArg  = entry["checkArg"].readVal<std::string>("");
				item.afterStep = entry["afterStep"].readVal<std::string>("");
				if (item.id.empty() || item.label.empty() || item.check.empty())
				{
					Log(LOG_WARNING) << "tutorial: checklist item missing id/label/check; skipped";
					continue;
				}
				if (item.check != "researched" && item.check != "facilityBuilt"
					&& item.check != "itemInStores" && item.check != "stepShown")
				{
					Log(LOG_WARNING) << "tutorial: checklist item '" << item.id
						<< "' has unknown check '" << item.check << "'; will read as not-done";
				}
				_calypsoChecklist.push_back(std::move(item));
			}
		}
		if (tutorialReader["advisors"])
		{
			for (const auto& entry : tutorialReader["advisors"].children())
			{
				CalypsoAdvisorRule rule;
				rule.id       = entry["id"].readVal<std::string>("");
				rule.check    = entry["check"].readVal<std::string>("");
				rule.checkArg = entry["checkArg"].readVal<std::string>("");
				entry["afterMonth"].tryReadVal<int>(rule.afterMonth);
				entry["graceDays"].tryReadVal<int>(rule.graceDays);
				if (rule.id.empty() || rule.check.empty())
				{
					Log(LOG_WARNING) << "tutorial: advisor rule missing id/check; skipped";
					continue;
				}
				if (rule.check != "idleScientists" && rule.check != "idleEngineers"
					&& rule.check != "noFacility" && rule.check != "craftWeaponEquipped"
					&& rule.check != "singleBase" && rule.check != "facilityBuilt")
				{
					Log(LOG_WARNING) << "tutorial: advisor rule '" << rule.id
						<< "' has unknown check '" << rule.check << "'; will never fire";
				}
				_calypsoAdvisors.push_back(std::move(rule));
			}
		}
	}
	// Phase 46.1.3 (Calypso): parse the top-level `hdUiFamilies:` sequence into
	// the sorted, deduplicated _hdUiFamilies gate. Fail-safe behavior per node
	// shape (single node lookup, no throw on any path):
	//   * key absent             -> guard skips this block; _hdUiFamilies is left
	//                               untouched (default-constructed empty on a
	//                               fresh Mod);
	//   * empty sequence `[]`    -> readVal returns the empty default and
	//                               overwrites _hdUiFamilies with an empty list;
	//   * valid sequence         -> invalid ids are dropped with one LOG_WARNING
	//                               per distinct id, valid ids kept sorted+deduped;
	//   * present but NOT a seq  -> fail-safe: one LOG_WARNING, clear _hdUiFamilies
	//                               to empty, do NOT call readVal on it (avoids the
	//                               ryml type-error throw tryReadVal would raise),
	//                               and continue with the legacy layout.
	// The key is owned by calypso-hd-pack; only a file that carries the key
	// touches _hdUiFamilies (last-wins, matching battlescapeTileScale), so an
	// unrelated ruleset file can never silently clear an owning mod's list. The
	// list ships EMPTY; a family id is added only in the commit that passes its
	// implementation checkpoint. isHdUiFamilyEnabled() does no YAML scanning or
	// allocation at all.
	auto hdNode = reader["hdUiFamilies"];
	if (hdNode)
	{
		if (!hdNode.isSeq())
		{
			Log(LOG_WARNING) << "hdUiFamilies: expected a YAML sequence; ignoring malformed entry (legacy layout)";
			_hdUiFamilies.clear();
		}
		else
		{
			std::vector<std::string> rawFamilies =
				hdNode.readVal<std::vector<std::string>>(
					std::vector<std::string>{});
			Calypso::ParsedHdUiFamilies parsed = Calypso::parseHdUiFamilies(rawFamilies);
			for (const std::string& bad : parsed.rejected)
			{
				Log(LOG_WARNING) << "hdUiFamilies: ignoring invalid family id '"
				                 << bad << "' (expected F01..F38)";
			}
			_hdUiFamilies = std::move(parsed.families);
			Log(LOG_INFO) << "hdUiFamilies: " << _hdUiFamilies.size()
			              << " family(ies) enabled";
		}
	}
}

/// Phase 46.1.3 (Calypso): the single engine API family adapters call. Delegates
/// to the pure fail-safe core (CalypsoUiFamilies.h). _hdPackActive is the hard
/// gate: a missing/inactive HD pack forces false for every family even when the
/// id is valid and listed, so a mod that ships `hdUiFamilies:` without the HD
/// pack infrastructure can never flip a family on. _hdUiFamilies is sorted +
/// deduplicated at parse time, so the listed-id check is an O(log n) binary
/// search with no per-call allocation and no mutation. Always false for a
/// malformed id, an unknown family, an inactive HD pack, or an empty list (the
/// shipped default).
bool Mod::isHdUiFamilyEnabled(const std::string& familyId) const
{
	return Calypso::isHdUiFamilyEnabled(_hdPackActive, &_hdUiFamilies, familyId);
}

} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
