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
#include "../Mod/ModScript.h"
#include <algorithm>
#include <functional>
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
#  include <webp/decode.h>
#  include "../Mod/TileAtlasBuilder.h"
#  include "../Mod/UnitSpriteAtlasBuilder.h"
#  include <set>
// M5: heap-attribution marks (function defined in EmscriptenHarness.cpp)
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
		evictOne(pair.second.atlas);
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
		restoreOne(pair.second.atlas);
	_tileAtlasGpuEvicted = false;
	Log(LOG_INFO) << "[L5] restoreTileAtlasGL: reuploaded " << n << " atlas GL handle(s)";
}

void Mod::ensureUnitAtlas(SurfaceSet* ss, const std::string& name,
                           const SDL_Color* palette, int ncolors)
{
	if (!GpuInit::ready()) return;
	if (!ss) return;
	if (_unitAtlases.count(name)) return;

	int atlasW = 0, atlasH = 0, cols = 0;
	GpuTexture* tex = buildUnitAtlas(*ss, palette, ncolors, atlasW, atlasH, cols, name);
	if (!tex) return;

	UnitAtlasSpec& spec = _unitAtlases[name];
	spec.atlas      = tex;
	spec.atlasW     = atlasW;
	spec.atlasH     = atlasH;
	spec.tileWidth  = 64;
	spec.tileHeight = 80;
	spec.columns    = cols;
}

const Mod::UnitAtlasSpec* Mod::getUnitAtlas(const std::string& name) const
{
	auto it = _unitAtlases.find(name);
	return it != _unitAtlases.end() ? &it->second : nullptr;
}

void Mod::clearUnitAtlases()
{
	for (auto& pair : _unitAtlases)
		delete pair.second.atlas;
	_unitAtlases.clear();
}

} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
