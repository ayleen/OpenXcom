#pragma once
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
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <bitset>
#include <array>
#include <set>
#include <SDL.h>
#include "../Engine/Yaml.h"
#include "../Engine/Options.h"
#include "../Engine/FileMap.h"
#include "../Engine/Collections.h"
#include "../Savegame/GameTime.h"
#include "../Savegame/Soldier.h"
#include "RuleDamageType.h"
#include "RuleAlienMission.h"
#include "RuleBaseFacilityFunctions.h"
#include "RuleItem.h"
#include "MapData.h"            // Phase 21: TilePart enum (O_FLOOR/O_OBJECT)
#ifdef __EMSCRIPTEN__
#  include "../Calypso/CalypsoEconomy.h"   // Phase 38: Calypso::EconomyRules (ruleset config type)
#  include "../Calypso/CalypsoTutorial.h"  // Phase 37: Calypso tutorial steps
#  include "../Calypso/CalypsoChecklist.h" // Phase 39: Calypso checklist items
#  include "../Calypso/CalypsoAdvisor.h"   // Phase 39: Calypso strategic advisor rules
#  include "../Calypso/HdUnitAtlas.h"      // Phase 42: UnitAtlasSpec (relocated, review #2/#6)
#  include "../Calypso/RuleVoiceProfile.h" // Phase 44: persistent voice identity rules
#  include "../Calypso/RuleVoiceRegion.h" // Phase 44: operation-locale geography
#endif

namespace OpenXcom
{

class Surface;
class SurfaceSet;
class Font;
class TTFFont;
class Palette;
class Music;
class SoundSet;
class Sound;
class CatFile;
class GMCatFile;
class Music;
class Palette;
class SavedGame;
class Soldier;
class RuleCountry;
class RuleRegion;
class RuleBaseFacility;
class RuleCraft;
class RuleCraftWeapon;
class RuleItemCategory;
class RuleItem;
class RuleWeaponSet;
struct RuleDamageType;
class RuleUfo;
class RuleTerrain;
class MapDataSet;
class RuleSkill;
class RuleSoldier;
class Unit;
class Armor;
class ArticleDefinition;
class RuleInventory;
class RuleResearch;
class RuleManufacture;
class RuleManufactureShortcut;
class RuleSoldierBonus;
class RuleSoldierTransformation;
class AlienRace;
class RuleEnviroEffects;
class RuleStartingCondition;
class AlienDeployment;
class UfoTrajectory;
class RuleAlienMission;
class Base;
class MCDPatch;
class ExtraSprites;
class ExtraSounds;
class CustomPalettes;
class ExtraStrings;
class RuleCommendations;
class StatString;
class RuleInterface;
class RuleGlobe;
class RuleConverter;
class SoundDefinition;
class MapScript;
class ModInfo;
class RuleVideo;
class RuleMusic;
class RuleArcScript;
class RuleEventScript;
class RuleEvent;
class RuleMissionScript;
class ModScript;
class ModScriptGlobal;
class ScriptParserBase;
class ScriptGlobal;
struct StatAdjustment;
#ifdef __EMSCRIPTEN__
class GpuTexture;
class Tile;
class SavedBattleGame;
#endif

enum GameDifficulty : int;
enum AIAttackWeight : int;

/**
 * Mod data used when loading resources
 */
struct ModData
{
	/// Mod name
	std::string name;
	/// Optional info about mod
	const ModInfo* info;
	/// Offset that mod use is common sets
	size_t offset;
	/// Maximum size allowed by mod in common sets
	size_t size;
};

/**
 * Helper exception representing the final message with all the required context for the end user to fix the errors in rulesets
 */
struct LoadRuleException : Exception
{
	LoadRuleException(const std::string& parent, const YAML::YamlNodeReader& reader, const std::string& message)
		: Exception{"Error for '" + parent + "': " + message + " at line " + std::to_string(reader.getLocationInFile().line)}
	{

	}

	LoadRuleException(const std::string& parent, const std::string& message) : Exception{ "Error for '" + parent + "': " + message}
	{

	}
};

/**
 * Contains all the game-specific static data that never changes
 * throughout the game, like rulesets and resources.
 */
class Mod
{
public:
	/// Number of color per opacity level.
	constexpr static int TransparenciesPaletteColors = 256;
	/// Number of opacity levels.
	constexpr static int TransparenciesOpacityLevels = 4;

	// Realistic Accuracy mod configuration (Joy Narical's Realistic Accuracy v3.0,
	// adapted from Brutal-OXCE by Xilmi; github.com/Xilmi/OpenXcom).
	struct AccuracyModConfig
	{
		int peekDistance = 25;
		int suicideProtectionDistance = 50;

		// Deviation in voxels per 10 tiles of shot trajectory
		// first for "realistic", second for "normal"
		int distanceDeviation[2]       = { 13, 13 };
		int oneHandWeaponDeviation[2]  = { 3, 3 };
		int kneelDeviation[2]          = { -2, -2 };
		int aimedDeviation[2]          = { 1, 1 };
		int snapDeviation[2]           = { 4, 4 };
		int autoDeviation[2]           = { 7, 7 };

		float horizontalSpreadCoeff[2] = { 1.2f, 1.6f };
		float verticalSpreadCoeff[2]   = { 0.65f, 0.65f };

		int coverEfficiency[5] = { 0, 30, 50, 70, 100 };
	};

#ifdef __EMSCRIPTEN__
	/// Phase 20: per-cell HD authoring metadata, used by the atlas builder and renderer.
	struct HDTileSpec
	{
		int                    cell      = 0;
		std::string            image;          // RGBA PNG path (mod-relative)
		std::string            mask;           // "diamond" | "silhouette" | PNG path
		float                  opacity   = 1.0f;
		std::array<int,2>      anchor    = {0, 0}; // tile-native pixels (tileWidth/tileHeight units)
		int                    zBias     = 0;       // tile-native pixels, for draw ordering
		std::vector<HDTileSpec> subLayers;           // additional draw passes over this cell
		// Phase 21: Corner-Wang transition support.
		// wangType is meaningful only when hasWangType==true. With hasWangType==false the
		// dataset-level TileAtlasSpec::wangType is used (inheritance). With
		// hasWangType==true && wangType=="" the cell explicitly opts out of Wang.
		std::string            wangType;
		bool                   hasWangType = false;
		// tilePart hints which TilePart slot the MAP author placed this cell into.
		// Required when the cell lives in O_OBJECT (e.g. SAND#19 pale-sand, BLANKS#1
		// brown-debris in vanilla SEABED). hasTilePart==false inherits the dataset-level
		// TileAtlasSpec::wangTilePart (hard default O_FLOOR).
		TilePart               tilePart    = O_FLOOR;
		bool                   hasTilePart = false;
		// Phase 22: anti-repeat variant count (§22.5). The base cell (this one)
		// carries all metadata; cells [cell..cell+variants-1] are the visual pool.
		int                    variants    = 1;
		// Phase 22 (M1 perf): interned id of this cell's wangType, resolved once at
		// parse time so computeWangMask compares ints instead of constructing/comparing
		// std::string on the per-tile emit path (hot during camera scroll). -1 = no
		// wang (empty string / explicit opt-out). Meaningful only when hasWangType==true;
		// cells without a per-cell override fall back to TileAtlasSpec::wangTypeIdDefault.
		int                    wangTypeId  = -1;
		// Phase 22.7: state-gated sub-layer condition. A sub-layer with a non-Always
		// condition is emitted only when the underlying tile is in that battle state
		// (e.g. a scorch overlay shown only while the tile burns). COND_ALWAYS keeps
		// the legacy unconditional behaviour. Parsed from the sub-layer's `condition:`
		// string ("fire"); top-level cells ignore it.
		static constexpr int   COND_ALWAYS = 0;
		static constexpr int   COND_FIRE   = 1;
		int                    condition   = COND_ALWAYS;
	};

	/// Phase 20: controls whether the R8 baseline atlas is built and drawn.
	enum class BaselineMode { Vanilla, None };

	/// Phase 22: per-neighbour wangSet entry, supporting both bake (Phase 21)
	/// and runtime blend (Phase 22) modes.
	struct WangNeighbour
	{
		int   variantCells[16];          // bake: per-mask atlas cell (-1 = no variant)
		bool  blend          = false;    // true → runtime blend path
		int   surfaceCell    = -1;       // blend: base neighbour-surface cell in atlas
		int   surfaceVariants = 1;       // blend: anti-repeat cell count from surfaceCell
		float feather        = 0.18f;   // blend: boundary softness
		float noiseScale     = 2.71f;   // blend: tiling frequency of u_noise (non-integer avoids tile-grid aliasing)
		float noiseAmp       = 0.35f;   // blend: noise perturbation of the boundary
		WangNeighbour() { std::fill(variantCells, variantCells + 16, -1); }
	};

	/// Atlas layout for a single mapDataSet's GPU tile sheet.
	struct TileAtlasSpec
	{
		enum class Format { Palette, Rgba };
		std::string       dataset;    // mapDataSet name, e.g. "SAND"
		std::string       file;       // relative path to PNG (single-file mode)
		// Phase 17: hybrid dual-atlas mode
		bool              hybrid       = false;
		std::string       baselineFile; // R8 PNG (palette indices); used when hybrid=true
		std::string       overlayFile;  // RGBA PNG (sparse HD overrides); used when hybrid=true
		GpuTexture*       overlayAtlas = nullptr; // RGBA overlay texture; nullptr for non-hybrid
		// Phase 25 R3: tangent-space normal-map atlas (optional). RGBA Linear NON-sRGB
		// (normals are linear direction data). Same dims as overlay → shared UVs.
		// Owned by TileAtlasSpec; deleted in Mod::clearTileAtlases().
		std::string       normalFile;             // ruleset key normalFile:; empty = no normal map
		GpuTexture*       normalAtlas = nullptr;  // nullptr if absent or load failed
		// Phase 25 R6: material emissive atlas (optional). RGBA Linear NON-sRGB —
		// RGB = glow colour, A = emission intensity. Added to the lit colour in
		// tile_atlas_rgba.frag, so it lands in the HDR SSAA buffer (R0) and the
		// >1.0 highlights survive tonemapping (lava/bioluminescence glow).
		// Same dims as overlay → shared UVs. Owned here; freed in clearTileAtlases.
		std::string       emissiveFile;             // ruleset key emissiveFile:; empty = none
		GpuTexture*       emissiveAtlas = nullptr;  // nullptr if absent or load failed
		int               width      = 0;
		int               height     = 0;
		int               tileWidth  = 64;
		int               tileHeight = 80;
		int               columns    = 16;
		Format            format     = Format::Palette;
		std::map<int,int> frameMap;    // MCD entry index → atlas tile index (primary frame)
		std::map<int,int> pckToAtlas;  // PCK frame index → atlas tile index (all frames incl. animation)
		// Phase 20: declarative HD authoring fields
		BaselineMode             baseline           = BaselineMode::Vanilla;
		int                      bleed              = 0;    // transparent gutter px between cells
		bool                     premultipliedAlpha = false;
		std::string              fallbackImage;             // default HD image for unlisted cells
		float                    fallbackOpacity    = 1.0f; // opacity for fallback cells
		std::vector<HDTileSpec>  hdTiles;                   // per-cell HD metadata
		// Phase 20 runtime: per-sub-layer overlay atlases (index 0 = base overlay)
		std::vector<GpuTexture*> subLayerAtlases;           // populated by ensureVanillaAtlas
		// Phase 20.6: O(1) cell→hdTiles index lookup (built by parser, not serialised).
		std::unordered_map<int,int> hdTilesByCell;          // cell index → hdTiles[] index
		// Phase 21/22: Corner-Wang transition support (dataset-level defaults + variant tables).
		std::string            wangType;                                                  // default neighbour-tag for cells without per-cell override
		int                    wangTypeIdDefault = -1;                                    // Phase 22 (M1 perf): interned id of dataset-default wangType; -1 = none
		TilePart               wangTilePart = O_FLOOR;                                    // default TilePart slot for cells without per-cell override
		std::unordered_map<std::string, WangNeighbour> wangSets;                          // neighbour tag → WangNeighbour (bake or blend)
		// Phase 27: world-position ground super-tile. groundBase < 0 = disabled.
		// For every dataset-default-wangType O_FLOOR tile the engine selects atlas
		// cell  groundBase + (worldY % groundTilesY)*groundTilesX + (worldX % groundTilesX),
		// reconstructing one big seamless ground (bold relief baked, ripples spanning
		// many tiles) across the map with no per-tile repeat. atlas-build slices the
		// groundPool sheet into cells groundBase..groundBase+tilesX*tilesY-1.
		int                    groundBase   = -1;
		int                    groundTilesX = 0;
		int                    groundTilesY = 0;

		// Phase 21: resolve cell-vs-dataset inheritance.
		// effectiveWangType returns "" iff the cell explicitly opts out (hasWangType + ""),
		// otherwise the per-cell override, otherwise the dataset default.
		std::string effectiveWangType(const HDTileSpec& cell) const
		{
			return cell.hasWangType ? cell.wangType : wangType;
		}
		// Phase 22 (M1 perf): integer-interned counterpart of effectiveWangType, used by
		// computeWangMask so the per-tile neighbour scan compares ints (no std::string).
		// Returns -1 for "no wang" (empty / opt-out). Parity with effectiveWangType:
		// hasWangType ? cell id : dataset-default id (both resolved at parse time).
		int effectiveWangTypeId(const HDTileSpec& cell) const
		{
			return cell.hasWangType ? cell.wangTypeId : wangTypeIdDefault;
		}
		// effectiveTilePart returns the per-cell override when present,
		// otherwise the dataset default (hard default O_FLOOR).
		TilePart effectiveTilePart(const HDTileSpec& cell) const
		{
			return cell.hasTilePart ? cell.tilePart : wangTilePart;
		}
	};

	/// Phase 21/22: result of a Corner-Wang lookup for one tile.
	/// mask is the 4-bit OR-corner mask (NW=8, NE=4, SE=2, SW=1).
	/// Bake (Phase 21): variantCell is the atlas-cell from the wangSet, or -1.
	/// Blend (Phase 22): surfaceCell is the neighbour-surface cell; matched points
	/// to the WangNeighbour for per-instance knobs; neighbourDx/Dy give the
	/// grid offset to the dominant foreign neighbour tile.
	struct WangResult
	{
		uint8_t mask              = 0;
		int     variantCell       = -1;          // BAKE: variant cell index; -1 = no transition
		int     surfaceCell       = -1;          // BLEND: neighbour-surface base cell; -1 = no blend
		bool    blend             = false;
		int     neighbourDx       = 0;           // BLEND: grid offset (±1) to dominant neighbour
		int     neighbourDy       = 0;
		const WangNeighbour* matched = nullptr;  // BLEND: knobs + surfaceVariants; null if bake/none
	};

	/// Layout record for a unit-PCK GPU sprite atlas (Phase 14.1).
	/// atlas_tile_index == PCK_frame_index (frames packed in declaration order).
	/// Definition lives in src/Calypso/HdUnitAtlas.h (Phase 42 review #2: the
	/// struct grew past the in-place #ifdef limit once RGBA overlay pages were
	/// added). Exposed here as `Mod::UnitAtlasSpec` so all call sites —
	/// getUnitAtlas(), Map, UnitSprite, ModHd — compile unchanged.
	using UnitAtlasSpec = HdUnitAtlasSpec;
#endif

private:
	Music *_muteMusic;
	Sound *_muteSound;
	std::string _playingMusic, _currentMusicTrack;

	std::map<std::string, Palette*> _palettes;
	std::map<std::string, Font*> _fonts;
	std::map<std::string, TTFFont*> _ttfFonts;
	std::map<std::string, Surface*> _surfaces;
	std::map<std::string, SurfaceSet*> _sets;
	std::map<std::string, SoundSet*> _sounds;
	std::map<std::string, Music*> _musics;
	std::vector<Uint16> _voxelData;
	std::vector<std::vector<Uint8> > _transparencyLUTs;

	std::map<std::string, RuleCountry*> _countries, _extraGlobeLabels;
	std::map<std::string, RuleRegion*> _regions;
	std::map<std::string, RuleBaseFacility*> _facilities;
	std::map<std::string, RuleCraft*> _crafts;
	std::map<std::string, RuleCraftWeapon*> _craftWeapons;
	std::map<std::string, RuleItemCategory*> _itemCategories;
	std::map<std::string, RuleItem*> _items;
	std::map<std::string, RuleWeaponSet*> _weaponSets;
	std::map<std::string, RuleUfo*> _ufos;
	std::map<std::string, RuleTerrain*> _terrains;
	std::map<std::string, MapDataSet*> _mapDataSets;
	std::map<std::string, RuleSkill*> _skills;
	std::map<std::string, RuleSoldier*> _soldiers;
	std::map<std::string, Unit*> _units;
	std::map<std::string, AlienRace*> _alienRaces;
	std::map<std::string, RuleEnviroEffects*> _enviroEffects;
	std::map<std::string, RuleStartingCondition*> _startingConditions;
	std::map<std::string, AlienDeployment*> _alienDeployments;
	std::map<std::string, Armor*> _armors;
	std::map<std::string, ArticleDefinition*> _ufopaediaArticles;
	std::map<std::string, RuleInventory*> _invs;
	bool _inventoryOverlapsPaperdoll;
	std::map<std::string, RuleResearch *> _research;
	std::map<std::string, RuleManufacture *> _manufacture;
	std::map<std::string, RuleManufactureShortcut *> _manufactureShortcut;
	std::map<std::string, RuleSoldierBonus *> _soldierBonus;
	std::map<std::string, RuleSoldierTransformation *> _soldierTransformation;
	std::map<std::string, UfoTrajectory *> _ufoTrajectories;
	std::map<std::string, RuleAlienMission *> _alienMissions;
	std::map<std::string, RuleInterface *> _interfaces;
	std::map<std::string, SoundDefinition *> _soundDefs;
	std::map<std::string, RuleVideo *>_videos;
	std::map<std::string, MCDPatch *> _MCDPatches;
	std::map<std::string, std::vector<MapScript *> > _mapScripts;
	std::map<std::string, RuleCommendations *> _commendations;
	std::map<std::string, RuleArcScript*> _arcScripts;
	std::map<std::string, RuleEventScript*> _eventScripts;
	std::map<std::string, RuleEvent*> _events;
	std::map<std::string, RuleMissionScript*> _missionScripts;
	std::map<std::string, RuleMissionScript*> _adhocScripts;
	std::map<std::string, std::vector<ExtraSprites *> > _extraSprites;
#ifdef __EMSCRIPTEN__
	std::map<std::string, GpuTexture*> _globeTextures;
	std::map<std::string, TileAtlasSpec> _tileAtlasSpecs;
	/// Synthesised vanilla tile atlases: mapDataSet name -> GpuTexture*.
	/// Populated lazily by ensureVanillaAtlas() (Block 11.2).
	std::map<std::string, GpuTexture*> _tileAtlases;
	/// Phase 22 (M1 perf): global wangType string -> int interning, so computeWangMask
	/// can compare integer ids on the per-tile emit path instead of constructing and
	/// comparing std::string for self + four neighbours. Interning is global (one id per
	/// distinct tag across all datasets) so a cross-dataset "sand"=="sand" still matches.
	/// _wangTypeNames is the reverse map (id -> tag) for the single wangSet lookup that
	/// fires only when a transition is actually detected.
	std::unordered_map<std::string, int> _wangTypeIds;
	std::vector<std::string>             _wangTypeNames;
	/// Interns a wangType tag, returning a stable id (>=0); "" returns -1 (no wang).
	int internWangType(const std::string& tag);
	/// Unit sprite atlases: SurfaceSet name -> UnitAtlasSpec (Phase 14.1).
	/// Populated lazily by ensureUnitAtlas().
	std::map<std::string, UnitAtlasSpec> _unitAtlases;
	/// True once at least one tileAtlas: YAML entry has been parsed.
	bool _hdPackActive = false;
	/// Tile render scale factor (1=32×40 native, 2=64×80, 4=128×160).
	/// Set via battlescapeTileScale: in the mod ruleset.
	int _battlescapeTileScale = 1;
	/// Phase 25 R5: draw floating HD nameplates + HP/TU/energy bars over player
	/// units in the Battlescape. Off by default; set via calypso_hud_overlay:.
	bool _calypsoHudOverlay = false;
	/// Phase 38 (Calypso): parsed trade/economy ruleset config (disabled
	/// sentinel when the `calypsoEconomy:` key is absent). Body lives in
	/// Calypso/ModHd.cpp::loadFileCalypso; type defined in Calypso/CalypsoEconomy.h.
	Calypso::EconomyRules _calypsoEconomyRules;
	/// Phase 37 (Calypso): parsed tutorial steps from the `tutorial:` top-level
	/// ruleset key. Lives on Mod so it loads once at mod-load time and survives
	/// save/load round-trips without re-parsing. Gated like battlescapeTileScale.
	std::vector<CalypsoTutorialStep> _calypsoTutorialSteps;
	std::vector<CalypsoChecklistItem> _calypsoChecklist;
	std::vector<CalypsoAdvisorRule> _calypsoAdvisors;
	/// Phase 44: text-only voice registry. Audio is fetched lazily by the web shell.
	std::map<std::string, RuleVoiceProfile> _voiceProfiles;
	std::map<std::string, RuleVoiceRegion> _voiceRegions;
	mutable std::set<std::string> _voiceProfileRepairWarnings;
	/// L5: globe GL handles were evicted on battle entry; restore on geoscape return.
	bool _globeGpuEvicted     = false;
	/// L5: tile + unit atlas GL handles were evicted on geoscape; restore on battle entry.
	bool _tileAtlasGpuEvicted = false;
	/// L7: true once loadBattlescapeResources() has finished; cleared on unload.
	bool _battlescapeResourcesLoaded = false;
	/// L7: keys of every _sets entry that was registered as battle-only and can
	/// be freed when returning to the geoscape.  Populated by
	/// loadBattlescapeResources(), cleared by unloadBattlescapeResources().
	std::vector<std::string> _battlescapeOnlySets;
	/// L10: source-file format tag for a deferred geo/flat surface.
	enum class LazyGeoFormat { Scr, Bdy, Spk };
	struct LazyGeoEntry
	{
		std::string   vfsPath;   ///< VFS path used by load* calls (stays in MEMFS).
		LazyGeoFormat format;
		int           w, h;
	};
	/// L10: surfaces registered but not yet decoded; erased on first getSurface() call.
	std::map<std::string, LazyGeoEntry> _lazyGeoSurfaces;
	/// L10: decodes a registered lazy geo surface on first request.
	void materializeGeoSurface(const std::string &name);
	/// Phase 36: Calypso-only ruleset keys (globeTextures/tileAtlas/hdTiles/
	/// battlescapeTileScale/...) parsed out of loadFile; body in Calypso/ModHd.cpp.
	void loadFileCalypso(YAML::YamlNodeReader& reader);
	void validateVoiceProfiles() const;
	/// Phase 42 E1: decode + upload the optional RGBA overlay pages for one
	/// UnitAtlasSpec, compute the per-PCK-frame hasHd mask (transparent slots
	/// fall back to R8), and wire MEMFS-backed reload callbacks for context
	/// loss recovery. No-op when GPU is not ready or pages[] is empty.
	void buildUnitRgbaOverlay(UnitAtlasSpec& spec, const std::string& name,
	                          int frameCount);
#endif
	std::map<std::string, CustomPalettes *> _customPalettes;
	std::vector<std::pair<std::string, ExtraSounds *> > _extraSounds;
	std::map<std::string, ExtraStrings *> _extraStrings;
	std::vector<StatString*> _statStrings;
	std::vector<RuleDamageType*> _damageTypes;
	std::map<std::string, RuleMusic *> _musicDefs;

	RuleGlobe *_globe;
	RuleConverter *_converter;
	ModScriptGlobal *_scriptGlobal;

	int _maxViewDistance, _maxDarknessToSeeUnits;
	int _maxStaticLightDistance, _maxDynamicLightDistance, _enhancedLighting;
	int _costHireEngineer, _costHireScientist;
	int _costEngineer, _costScientist, _timePersonnel, _hireByCountryOdds, _hireByRegionOdds, _initialFunding;
	int _globalTransferCostMult, _globalTransferCostDiv;

	int _aiUseDelayBlaster, _aiUseDelayFirearm, _aiUseDelayGrenade, _aiUseDelayProxy, _aiUseDelayMelee, _aiUseDelayPsionic, _aiUseDelayMedikit;
	int _aiFireChoiceIntelCoeff, _aiFireChoiceAggroCoeff;
	bool _aiExtendedFireModeChoice, _aiRespectMaxRange, _aiDestroyBaseFacilities;
	bool _aiPickUpWeaponsMoreActively, _aiPickUpWeaponsMoreActivelyCiv;
	bool _aiSmartCivilians;
	bool _aiFairAliens;
	bool _aiTerrorHuntCivilians;
	// Phase 34.6 (Calypso): terrain-tactics master switch -- deliberate floor-drops and
	// wall-breaches by the hostile AI. Default off -> byte-identical vanilla when no mod opts in.
	bool _aiTerrainTactics;
	// Phase 34.8 (Calypso): hearing master switch -- transient noise events on weapon fire /
	// explosions + zone-quantized investigation patrol bias for blind hostiles. Default off ->
	// byte-identical vanilla when no mod opts in.
	bool _aiHearing;
	// Phase 34.7 (Calypso): suppression master switch -- near-miss projectile fire applies a
	// morale hit + energy drain (engine-symmetric: any faction can suppress any faction; the
	// shot's target is excluded, a hit is not a near-miss). Default off -> byte-identical.
	bool _aiSuppression;
	// Phase 34.7 (Calypso): per-near-miss morale loss / energy drain knobs. Applied flat (the
	// knob IS the tuning point), capped per turn per victim (see BattleUnit::addNearMiss).
	int _aiSuppressionMorale;
	int _aiSuppressionEnergy;
	// Phase 34.9 (Calypso): master switch for the squad-coordination blackboard (soft focus-fire
	// cap, pin-and-flank preference, wounded retreat toward the friendly cluster). Default off;
	// with the flag off the per-faction blackboard stays empty and every AI read hook degrades
	// to its pre-34.9 value, so native AND WASM behavior is byte-for-byte unchanged.
	bool _aiSquadCoordination;
	// Phase 34.5: Brutal-AI port knobs (adapted from Brutal-OXCE by Xilmi). Folded into the
	// per-mod ai: block instead of global engine Options for parity with smartCivilians.
	bool _aiBrutalAI;
	int _aiCheatMode;
	bool _aiAvoidMines;
	bool _aiPerformanceOptimization;
	std::string _aiCivilianGuardType;
	int _aiCivilianGuardChance;
	int _aiReactionFireThreshold, _aiReactionFireThresholdCiv;
	AIAttackWeight _aiTargetWeightThreatThreshold = AIAttackWeight{ 50 };
	AIAttackWeight _aiTargetWeightAsHostile = AIAttackWeight{ 100 };
	AIAttackWeight _aiTargetWeightAsHostileCivilians = AIAttackWeight{ 50 };
	AIAttackWeight _aiTargetWeightAsFriendly = AIAttackWeight{ -200 };
	AIAttackWeight _aiTargetWeightAsNeutral = AIAttackWeight{ -100 };

	int _maxLookVariant, _tooMuchSmokeThreshold, _customTrainingFactor;
	int _chanceToStopRetaliation;
	int _chanceToDetectAlienBaseEachMonth;
	bool _lessAliensDuringBaseDefense;
	bool _allowCountriesToCancelAlienPact, _buildInfiltrationBaseCloseToTheCountry, _infiltrateRandomCountryInTheRegion;
	bool _allowAlienBasesOnWrongTextures;
	int _kneelBonusGlobal, _oneHandedPenaltyGlobal;
	int _enableCloseQuartersCombat, _closeQuartersAccuracyGlobal, _closeQuartersTuCostGlobal, _closeQuartersEnergyCostGlobal, _closeQuartersSneakUpGlobal;
	int _noLOSAccuracyPenaltyGlobal;
	int _explodeInventoryGlobal;
	int _surrenderMode;
	int _bughuntMinTurn, _bughuntMaxEnemies, _bughuntRank, _bughuntLowMorale, _bughuntTimeUnitsLeft;

	int _manaMissingWoundThreshold = 200;
	int _healthMissingWoundThreshold = 100;
	bool _manaEnabled, _manaBattleUI, _manaTrainingPrimary, _manaTrainingSecondary, _manaReplenishAfterMission;
	bool _healthReplenishAfterMission = true;
	std::string _manaUnlockResearch;

	std::string _loseMoney, _loseRating, _loseDefeat;
	int _ufoGlancingHitThreshold, _ufoBeamWidthParameter;
	int _ufoTractorBeamSizeModifiers[5];
	int _escortRange, _drawEnemyRadarCircles;
	bool _escortsJoinFightAgainstHK, _hunterKillerFastRetarget;
	int _crewEmergencyEvacuationSurvivalChance, _pilotsEmergencyEvacuationSurvivalChance;
	bool _showUfoPreviewInBaseDefense;
	std::array<int, (size_t)(RANK_COMMANDER + 1)> _soldiersPerRank;
	int _pilotAccuracyZeroPoint, _pilotAccuracyRange, _pilotReactionsZeroPoint, _pilotReactionsRange;
	int _pilotBraveryThresholds[3];
	double _performanceBonusFactor;
	bool _enableNewResearchSorting;
	int _displayCustomCategories;
	bool _shareAmmoCategories, _showDogfightDistanceInKm, _showFullNameInAlienInventory;
	int _alienInventoryOffsetX, _alienInventoryOffsetBigUnit;
	bool _hidePediaInfoButton;
	int _extraNerdyPediaInfoType;
	bool _giveScoreAlsoForResearchedArtifacts, _statisticalBulletConservation, _stunningImprovesMorale;
	int _tuRecoveryWakeUpNewTurn;
	int _shortRadarRange;
	int _buildTimeReductionScaling;
	int _defeatScore, _defeatFunds;
	bool _difficultyDemigod;
	std::pair<std::string, int> _alienFuel;
	RuleResearch* _finalResearch = nullptr;
	std::string _fontName, _psiUnlockResearch, _fakeUnderwaterBaseUnlockResearch, _newBaseUnlockResearch;
	std::string _hireScientistsUnlockResearch, _hireEngineersUnlockResearch;
	RuleBaseFacilityFunctions _hireScientistsRequiresBaseFunc, _hireEngineersRequiresBaseFunc;

	// Realistic Accuracy: mod-tunable parameters + per-unit-size distance/accuracy -> hit% lookup tables.
	AccuracyModConfig _realisticAccuracyConfig;
	std::map<int, std::vector<int>> _hitChancesTable;

	std::string _destroyedFacility;
	YAML::YamlString _startingBaseDefault, _startingBaseBeginner, _startingBaseExperienced, _startingBaseVeteran, _startingBaseGenius, _startingBaseSuperhuman;
	Collections::NamesToIndex _baseFunctionNames;

	GameTime _startingTime;
	int _startingDifficulty;
	int _baseDefenseMapFromLocation;
	std::map<int, std::string> _missionRatings, _monthlyRatings;
	std::map<std::string, std::string> _fixedUserOptions, _recommendedUserOptions;
	std::vector<std::string> _hiddenMovementBackgrounds;
	std::vector<std::string> _baseNamesFirst, _baseNamesMiddle, _baseNamesLast;
	std::vector<std::string> _operationNamesFirst, _operationNamesLast;
	bool _disableUnderwaterSounds;
	bool _enableUnitResponseSounds;
	std::map<std::string, std::vector<int> > _selectUnitSound, _startMovingSound, _selectWeaponSound, _annoyedSound;
	std::vector<int> _selectBaseSound, _startDogfightSound;
	std::vector<int> _flagByKills;
	int _pediaReplaceCraftFuelWithRangeType;
	std::vector<StatAdjustment> _statAdjustment;

	// overrides for DIFFICULTY_COEFFICIENT[]
	std::vector<int> _monthlyRatingThresholds;
	std::vector<int> _ufoFiringRateCoefficients;
	std::vector<int> _ufoEscapeCountdownCoefficients;
	std::vector<int> _retaliationTriggerOdds;
	std::vector<int> _retaliationBaseRegionOdds;
	std::vector<int> _aliensFacingCraftOdds;

	std::map<std::string, int> _ufopaediaSections;
	std::vector<std::string> _countriesIndex, _extraGlobeLabelsIndex, _regionsIndex, _facilitiesIndex, _craftsIndex, _craftWeaponsIndex, _itemCategoriesIndex, _itemsIndex, _invsIndex, _ufosIndex;
	std::vector<std::string> _aliensIndex, _enviroEffectsIndex, _startingConditionsIndex, _deploymentsIndex, _armorsIndex, _ufopaediaIndex, _ufopaediaCatIndex, _researchIndex, _manufactureIndex;
	std::vector<std::string> _skillsIndex, _soldiersIndex, _soldierTransformationIndex, _soldierBonusIndex;
	std::vector<std::string> _alienMissionsIndex, _terrainIndex, _customPalettesIndex, _arcScriptIndex, _eventScriptIndex, _eventIndex, _missionScriptIndex, _adhocScriptIndex;
	std::vector<std::vector<int> > _alienItemLevels;
	std::vector<std::array<SDL_Color, TransparenciesOpacityLevels>> _transparencies;
	int _facilityListOrder, _craftListOrder, _itemCategoryListOrder, _itemListOrder, _armorListOrder, _alienRaceListOrder, _researchListOrder,  _manufactureListOrder;
	int _soldierBonusListOrder, _transformationListOrder, _ufopaediaListOrder, _invListOrder, _soldierListOrder;
	std::vector<ModData> _modData;
	ModData* _modCurrent;
	const SDL_Color *_statePalette;

	std::vector<std::string> _psiRequirements; // it's a cache for psiStrengthEval
	std::vector<const Armor*> _armorsForSoldiersCache;
	std::vector<const RuleItem*> _armorStorageItemsCache;
	std::vector<const RuleItem*> _craftWeaponStorageItemsCache;
	/// Track of what mod create rule object.
	std::unordered_map<const void*, const ModData*> _ruleCreationTracking;
	/// Track of what mod last update rule object.
	std::unordered_map<const void*, const ModData*> _ruleLastUpdateTracking;

	size_t _surfaceOffsetBigobs = 0;
	size_t _surfaceOffsetFloorob = 0;
	size_t _surfaceOffsetHandob = 0;
	size_t _surfaceOffsetSmoke = 0;
	size_t _surfaceOffsetHit = 0;
	size_t _surfaceOffsetBasebits = 0;
	size_t _soundOffsetBattle = 0;
	size_t _soundOffsetGeo = 0;

	/// Loads a ruleset from a YAML file that have basic resources configuration.
	void loadResourceConfigFile(const FileMap::FileRecord &filerec);
	void loadConstants(const YAML::YamlNodeReader& reader);
	/// Loads a ruleset from a YAML file.
	void loadFile(const FileMap::FileRecord &filerec, ModScript &parsers);

	template<typename T>
	struct RuleFactory
	{
		T* operator()(const std::string& type) { return new T(type); }
	};
	template<typename T>
	struct RuleListOrderedFactory
	{
		int& _currentListOrder;
		int offset;

		T* operator()(const std::string& type) { _currentListOrder += offset; return new T(type, _currentListOrder); }
	};

	/// Loads a ruleset element.
	template <typename T, typename F = RuleFactory<T>>
	T* loadRule(const YAML::YamlNodeReader& reader, std::map<std::string, T*>* map, std::vector<std::string>* index = 0, const std::string& key = "type", F&& factory = {});
	/// Gets a ruleset element.
	template <typename T>
	T *getRule(const std::string &id, const std::string &name, const std::map<std::string, T*> &map, bool error) const;
	/// Gets a random music. This is private to prevent access, use playMusic(name, true) instead.
	Music *getRandomMusic(const std::string &name) const;
	/// Gets a particular sound set. This is private to prevent access, use getSound(name, id) instead.
	SoundSet *getSoundSet(const std::string &name, bool error = true) const;
	/// Loads battlescape specific resources.
	void loadBattlescapeResources();
	/// Loads a specified music file.
	Music* loadMusic(MusicFormat fmt, RuleMusic* rule, CatFile* adlibcat, CatFile* aintrocat, GMCatFile* gmcat) const;
	/// Creates a transparency lookup table for a given palette.
	void createTransparencyLUT(Palette *pal);
	/// Loads a specified mod content.
	void loadMod(const std::vector<FileMap::FileRecord> &rulesetFiles, ModScript &parsers);
	/// Loads resources from vanilla.
	void loadVanillaResources();
	/// Loads resources from extra rulesets.
	void loadExtraResources();
	/// Loads surfaces on demand.
	void lazyLoadSurface(const std::string &name);
	/// Loads an external sprite.
	void loadExtraSprite(ExtraSprites *spritePack);
	/// Applies mods to vanilla resources.
	void modResources();
	/// Builds palette-cycle shade tables for vanilla TFTD assets (7.A.4 stub).
	void buildVanillaCycleTables();
	/// Sorts all our lists according to their weight.
	void sortLists();
public:
	static int DOOR_OPEN;
	static int SLIDING_DOOR_OPEN;
	static int SLIDING_DOOR_CLOSE;
	static int SMALL_EXPLOSION;
	static int LARGE_EXPLOSION;
	static int EXPLOSION_OFFSET;
	static int SMOKE_OFFSET;
	static int UNDERWATER_SMOKE_OFFSET;

	/// Empty surface.
	constexpr static int NO_SURFACE = -1;
	/// Empty sound.
	constexpr static int NO_SOUND = -1;
	/// Special value for default string different to empty one.
	static const std::string STR_NULL;
	// R2.2 scope cut: isLoadInProgress / setLoadInProgress removed.
	// The runtime buildFromPalette cost (~4 KB/setPalette) is acceptable;
	// gating adds complexity for no visible gain.

	static int ITEM_DROP;
	static int ITEM_THROW;
	static int ITEM_RELOAD;
	static int WALK_OFFSET;
	static int FLYING_SOUND;
	static int BUTTON_PRESS;
	static int WINDOW_POPUP[3];
	static int UFO_FIRE;
	static int UFO_HIT;
	static int UFO_CRASH;
	static int UFO_EXPLODE;
	static int INTERCEPTOR_HIT;
	static int INTERCEPTOR_EXPLODE;

	static int GEOSCAPE_CURSOR;
	static int BASESCAPE_CURSOR;
	static int BATTLESCAPE_CURSOR;
	static int UFOPAEDIA_CURSOR;
	static int GRAPHS_CURSOR;
	static int DAMAGE_RANGE;
	static int EXPLOSIVE_DAMAGE_RANGE;
	static int FIRE_DAMAGE_RANGE[2];
	static std::string DEBRIEF_MUSIC_GOOD;
	static std::string DEBRIEF_MUSIC_BAD;
	static int DIFFICULTY_COEFFICIENT[5];
	static int SELL_PRICE_COEFFICIENT[5];
	static int BUY_PRICE_COEFFICIENT[5];
	static int DIFFICULTY_BASED_RETAL_DELAY[5];
	static int UNIT_RESPONSE_SOUNDS_FREQUENCY[4];
	static int PEDIA_FACILITY_RENDER_PARAMETERS[4];
	static bool EXTENDED_ITEM_RELOAD_COST;
	static bool EXTENDED_INVENTORY_SLOT_SORTING;
	static bool EXTENDED_RUNNING_COST;
	static int EXTENDED_MOVEMENT_COST_ROUNDING;
	static bool EXTENDED_HWP_LOAD_ORDER;
	static int EXTENDED_SPOT_ON_HIT_FOR_SNIPING;
	static int EXTENDED_BERSERK_WITH_AIMED;
	static int EXTENDED_MELEE_REACTIONS;
	static int EXTENDED_TERRAIN_MELEE;
	static int EXTENDED_UNDERWATER_THROW_FACTOR;
	static bool EXTENDED_EXPERIENCE_AWARD_SYSTEM;
	static bool EXTENDED_FORCE_SPAWN;
	static int EXTENDED_SMOKE_OFFSET;

	// Realistic Accuracy hit-chance lookup-table dimensions.
	static const int distanceRows = 40;
	static const int maxAccuracy = 120; // Should be even number
	static const int accPerRowCount = 61; // Accuracy from 0% to 120%, with 2% step


	/// Return `true` when given string is empty or pseudo null value.
	static bool isEmptyRuleName(const std::string& s)
	{
		return s.empty() || s == Mod::STR_NULL;
	}
	// reset all the statics in all classes to default values
	static void resetGlobalStatics();

	/// Name of class used in script.
	static constexpr const char *ScriptName = "RuleMod";
	/// Register all useful function used by script.
	static void ScriptRegister(ScriptParserBase* parser);

	/// Creates a blank mod.
	Mod();
	/// Cleans up the mod.
	~Mod();

	/// For internal use only
	const std::map<std::string, int> &getUfopaediaSections() const { return _ufopaediaSections; }

	/// Gets a particular font.
	Font *getFont(const std::string &name, bool error = true) const;
	/// Gets a TTF font registered via extraTTFFonts (Phase 16).
	TTFFont *getTTFFont(const std::string &id, bool error = true) const;
	/// Gets a particular surface.
	Surface *getSurface(const std::string &name, bool error = true);
	/// Gets a particular surface set.
	SurfaceSet *getSurfaceSet(const std::string &name, bool error = true);
#ifdef __EMSCRIPTEN__
	/// Returns true if all four required globeTextures are loaded (HD sphere path active).
	bool hasGlobeTextures() const {
		return _globeTextures.count("bathymetry")
		    && _globeTextures.count("diffuse")
		    && _globeTextures.count("night")
		    && _globeTextures.count("clouds");
	}
	/// Returns the GpuTexture for the given id, or nullptr if not found.
	GpuTexture* getGlobeTexture(const std::string& id) const;
	/// Releases all GpuTextures (called on mod change or disable).
	void clearGlobeTextures();
	/// Returns the TileAtlasSpec for the given mapDataSet name, or nullptr if not registered.
	const TileAtlasSpec* getTileAtlasSpec(const std::string& dataset) const;
	/// Phase 21.3.1: compute the Corner-Wang transition mask + variant cell for `self`.
	/// Dual-slot scan (O_FLOOR then O_OBJECT) on self and each orthogonal neighbour;
	/// OR-corner classification; same-type fast-path; first-found foreign neighbour
	/// (N→E→S→W) drives the wangSet selection. Returns {mask=0, variantCell=-1} when
	/// the tile carries no Wang configuration or all neighbours match.
	WangResult computeWangMask(const TileAtlasSpec* spec,
	                           const Tile* self,
	                           SavedBattleGame* save) const;
	/// Returns the synthesised vanilla atlas for the given mapDataSet, or nullptr if not built.
	GpuTexture* getTileAtlas(const std::string& dataset) const;
	/// Build a vanilla atlas for a fully-loaded MapDataSet (Block 11.2).
	/// Called after MapDataSet::loadData() for datasets that have no tileAtlas: YAML entry.
	/// palette/ncolors must be the active battlescape palette (PAL_BATTLESCAPE).
	/// No-op if the atlas was already built or GPU is not ready.
	void ensureVanillaAtlas(MapDataSet* mds, const SDL_Color* palette, int ncolors);
	/// Deletes all synthesised vanilla atlases (called in ~Mod and on mod reload).
	void clearTileAtlases();
	/// L5: evict globe GL handles to free VRAM during a battle.
	/// Guard-flagged: repeated calls before a matching restore are no-ops.
	void evictGlobeGL();
	/// L5: restore previously-evicted globe GL handles (called from GeoscapeState::init()).
	/// No-op when the globe was not evicted.
	void restoreGlobeGL();
	/// L5: evict tile + unit atlas GL handles to free battle VRAM on geoscape.
	/// Guard-flagged: repeated calls (popup closes, etc.) are O(1) no-ops.
	void evictTileAtlasGL();
	/// L5: restore previously-evicted tile + unit atlas GL handles (called before battle).
	/// No-op when nothing was evicted.
	void restoreTileAtlasGL();
	/// Build or retrieve a unit-sprite atlas for the named SurfaceSet (Phase 14.1).
	/// No-op if already built.  palette/ncolors = active battlescape palette.
	void ensureUnitAtlas(SurfaceSet* ss, const std::string& name,
	                     const SDL_Color* palette, int ncolors);
	/// Returns the UnitAtlasSpec for the named SurfaceSet, or nullptr if not built.
	const UnitAtlasSpec* getUnitAtlas(const std::string& name) const;
	/// Deletes all unit-sprite atlases (called alongside clearTileAtlases).
	void clearUnitAtlases();
	/// Returns true when at least one tileAtlas: YAML entry was loaded.
	bool hasHDPack() const { return _hdPackActive; }
	/// Returns the battlescape tile scale factor (1, 2, or 4).
	int getBattlescapeTileScale() const { return _battlescapeTileScale; }
	/// Phase 25 R5: true when floating unit nameplates/bars should be drawn.
	bool getCalypsoHudOverlay() const { return _calypsoHudOverlay; }
	/// Phase 38 (Calypso): parsed trade/economy ruleset config (disabled
	/// sentinel when the `calypsoEconomy:` key is absent).
	const Calypso::EconomyRules& getCalypsoEconomyRules() const { return _calypsoEconomyRules; }
	/// Phase 37 (Calypso): tutorial step table parsed from the ruleset.
	const std::vector<CalypsoTutorialStep>& getCalypsoTutorialSteps() const { return _calypsoTutorialSteps; }
	const std::vector<CalypsoChecklistItem>& getCalypsoChecklist() const { return _calypsoChecklist; }
	const std::vector<CalypsoAdvisorRule>& getCalypsoAdvisors() const { return _calypsoAdvisors; }
	const RuleVoiceProfile *getVoiceProfile(const std::string &id) const;
	const std::map<std::string, RuleVoiceProfile> &getVoiceProfiles() const { return _voiceProfiles; }
	std::string selectVoiceProfile(const std::string &locale,
		const std::string &unitClass, const std::string &gender, int stableId,
		const std::string &stored = std::string()) const;
	std::string resolveCivilianVoiceLocale(double longitude,
		double latitude) const;
	/// L7: ensures battlescape-only SurfaceSets are resident.
	/// Delegates to the private loadBattlescapeResources(); idempotent (guarded
	/// by _battlescapeResourcesLoaded).  Called from BattlescapeState ctor.
	void ensureBattlescapeResources() { loadBattlescapeResources(); }
	/// L7: releases all battle-only SurfaceSets so WASM heap is not held during
	/// long geoscape sessions.  Sets _battlescapeResourcesLoaded=false so a
	/// subsequent ensureBattlescapeResources() call will reload them.
	void unloadBattlescapeResources();
#endif
	/// Gets a particular music.
	Music *getMusic(const std::string &name, bool error = true) const;
	/// Gets the available music tracks.
	const std::map<std::string, Music*> &getMusicTrackList() const;
	const std::string& getCurrentMusicTrack() const { return _currentMusicTrack; }
	void setCurrentMusicTrack(const std::string& currentMusicTrack) { _currentMusicTrack = currentMusicTrack; }
	/// Plays a particular music.
	void playMusic(const std::string &name, int id = 0);
	/// Gets a particular sound.
	Sound *getSound(const std::string &set, int sound) const;
	/// Gets all palettes.
	const std::map<std::string, Palette*> &getPalettes() const { return _palettes; }
	/// Gets a particular palette.
	Palette *getPalette(const std::string &name, bool error = true) const;
	/// Gets list of voxel data.
	const std::vector<Uint16> *getVoxelData() const;
	/// Returns a specific sound from either the land or underwater sound set.
	Sound *getSoundByDepth(unsigned int depth, unsigned int sound) const;
	/// Gets list of LUT data.
	const std::vector<std::vector<Uint8> > *getLUTs() const;
	/// Gets parameters for the Realistic Accuracy mod.
	const AccuracyModConfig *getAccuracyModConfig() const;
	/// Gets the Realistic Accuracy hit-chances lookup table for a given unit size.
	const std::vector<int>* getHitChancesTable(int size) const;


	/// Check for obsolete error based on year.
	bool checkForObsoleteErrorByYear(const std::string& parent, const YAML::YamlNodeReader& reader, const std::string& error, int year) const;
	/// Check for error that we can ignore by user request.
	bool checkForSoftError(bool check, const std::string& parent, const YAML::YamlNodeReader& reader, const std::string& error, SeverityLevel level = LOG_WARNING) const;
	/// Check for error that we can ignore by user request.
	bool checkForSoftError(bool check, const std::string &parent, const std::string &error, SeverityLevel level = LOG_WARNING) const;


	/// Verify if value have defined surface in given set.
	void verifySpriteOffset(const std::string &parent, const int& sprite, const std::string &set) const;
	/// Verify if value have defined surface in given set.
	void verifySpriteOffset(const std::string &parent, const std::vector<int>& sprites, const std::string &set) const;
	/// Verify if value have defined sound in given set.
	void verifySoundOffset(const std::string &parent, const int& sound, const std::string &set) const;
	/// Verify if value have defined sound in given set.
	void verifySoundOffset(const std::string &parent, const std::vector<int>& sounds, const std::string &set) const;


	/// Gets the mod offset.
	int getModOffset() const;
	/// Get offset and index for sound set or sprite set.
	void loadOffsetNode(const std::string &parent, int& offset, const YAML::YamlNodeReader& reader, int shared, const std::string &set, size_t multiplier, size_t sizeScale = 1) const;
	/// Gets the mod offset for a certain sprite.
	void loadSpriteOffset(const std::string& parent, int& sprite, const YAML::YamlNodeReader& reader, const std::string& set, size_t multiplier = 1) const;
	/// Gets the mod offset array for a certain sprite.
	void loadSpriteOffset(const std::string& parent, std::vector<int>& sprites, const YAML::YamlNodeReader& reader, const std::string& set) const;
	/// Gets the mod offset for a certain sound.
	void loadSoundOffset(const std::string &parent, int& sound, const YAML::YamlNodeReader& reader, const std::string &set) const;
	/// Gets the mod offset array for a certain sound.
	void loadSoundOffset(const std::string& parent, std::vector<int>& sounds, const YAML::YamlNodeReader& reader, const std::string& set) const;
	/// Gets the mod offset array for a certain transparency index.
	void loadTransparencyOffset(const std::string& parent, int& index, const YAML::YamlNodeReader& reader) const;
	/// Gets the mod offset for a generic value.
	int getOffset(int id, int max) const;

	/// Gets base functions from string array in yaml.
	void loadBaseFunction(const std::string& parent, RuleBaseFacilityFunctions& f, const YAML::YamlNodeReader& reader);
	/// Get names of function names in given bitset.
	std::vector<std::string> getBaseFunctionNames(RuleBaseFacilityFunctions f) const;

	/// Loads a list of ints.
	void loadInts(const std::string& parent, std::vector<int>& ints, const YAML::YamlNodeReader& reader) const;
	/// Loads a list of ints where order of items does not matter.
	void loadUnorderedInts(const std::string& parent, std::vector<int>& ints, const YAML::YamlNodeReader& reader) const;

	/// Loads a name.
	void loadName(const std::string& parent, std::string& names, const YAML::YamlNodeReader& reader) const;
	/// Loads a name.
	void loadNameNull(const std::string& parent, std::string& names, const YAML::YamlNodeReader& reader) const;
	/// Loads a list of names.
	void loadNames(const std::string& parent, std::vector<std::string>& names, const YAML::YamlNodeReader& reader) const;
	/// Loads a list of names where order of items does not matter.
	void loadUnorderedNames(const std::string& parent, std::vector<std::string>& names, const YAML::YamlNodeReader& reader) const;

	/// Loads a map from names to names.
	void loadNamesToNames(const std::string& parent, std::vector<std::pair<std::string, std::vector<std::string> > >& names, const YAML::YamlNodeReader& reader) const;
	/// Loads a map from names to names.
	void loadUnorderedNamesToNames(const std::string& parent, std::map<std::string, std::string>& names, const YAML::YamlNodeReader& reader) const;
	/// Loads a map from names to ints.
	void loadUnorderedNamesToInt(const std::string &parent, std::map<std::string, int>& names, const YAML::YamlNodeReader &reader) const;
	/// Loads a map from names to vector of ints.
	void loadUnorderedNamesToInts(const std::string& parent, std::map<std::string, std::vector<int> >& names, const YAML::YamlNodeReader& reader) const;
	/// Loads a map from names to names to int.
	void loadUnorderedNamesToNamesToInt(const std::string& parent, std::map<std::string, std::map<std::string, int> >& names, const YAML::YamlNodeReader& reader) const;

	/// Loads data for kill criteria from Commendations.
	void loadKillCriteria(const std::string& parent, std::vector<std::vector<std::pair<int, std::vector<std::string> > > >& names, const YAML::YamlNodeReader& reader) const;


	/// Convert names to correct rule objects
	template<typename T>
	void linkRule(const T*& rule, std::string& name) const
	{
		if constexpr (std::is_same_v<T, RuleItem>)
		{
			rule = getItem(name, true);
		}
		else if constexpr (std::is_same_v<T, RuleSoldier>)
		{
			rule = getSoldier(name, true);
		}
		else if constexpr (std::is_same_v<T, RuleSoldierBonus>)
		{
			rule = getSoldierBonus(name, true);
		}
		else if constexpr (std::is_same_v<T, Armor>)
		{
			rule = getArmor(name, true);
		}
		else if constexpr (std::is_same_v<T, RuleResearch>)
		{
			rule = getResearch(name, true);
		}
		else if constexpr (std::is_same_v<T, Unit>)
		{
			rule = getUnit(name, true);
		}
		else if constexpr (std::is_same_v<T, RuleSkill>)
		{
			rule = getSkill(name, true);
		}
		else if constexpr (std::is_same_v<T, RuleCraft>)
		{
			rule = getCraft(name, true);
		}
		else if constexpr (std::is_same_v<T, RuleUfo>)
		{
			rule = getUfo(name, true);
		}
		else if constexpr (std::is_same_v<T, RuleBaseFacility>)
		{
			rule = getBaseFacility(name, true);
		}
		else if constexpr (std::is_same_v<T, RuleInventory>)
		{
			rule = getInventory(name, true);
		}
		else if constexpr (std::is_same_v<T, RuleEvent>)
		{
			rule = getEvent(name, true);
		}
		else if constexpr (std::is_same_v<T, RuleCommendations>)
		{
			rule = getCommendation(name, true);
		}
		else
		{
			static_assert(sizeof(T) == 0, "Unsupported type to link");
		}
		name = {};
	}
	/// Convert names to correct rule objects
	template<typename T>
	void linkRule(std::vector<T>& rule, std::vector<std::string>& names) const
	{
		rule.reserve(names.size());
		for (auto& n : names)
		{
			linkRule(rule.emplace_back(), n);
		}
		names = {};
	}
	/// Convert names to correct rule objects
	template<typename T>
	void linkRule(std::vector<T>& rule, std::vector<std::vector<std::string>>& names) const
	{
		rule.reserve(names.size());
		for (auto& n : names)
		{
			linkRule(rule.emplace_back(), n);
		}
		names = {};
	}


	/// Loads a list of mods.
	void loadAll();
	/// Generates the starting saved game.
	SavedGame *newSave(GameDifficulty diff) const;
	/// Gets the ruleset for a country type.
	RuleCountry *getCountry(const std::string &id, bool error = false) const;
	/// Gets the available countries.
	const std::vector<std::string> &getCountriesList() const;
	/// Gets the ruleset for an extra globe label type.
	RuleCountry *getExtraGlobeLabel(const std::string &id, bool error = false) const;
	/// Gets the available extra globe labels.
	const std::vector<std::string> &getExtraGlobeLabelsList() const;
	/// Gets the ruleset for a region type.
	RuleRegion *getRegion(const std::string &id, bool error = false) const;
	/// Gets the available regions.
	const std::vector<std::string> &getRegionsList() const;
	/// Gets the ruleset for a facility type.
	RuleBaseFacility *getBaseFacility(const std::string &id, bool error = false) const;
	/// Gets the available facilities.
	const std::vector<std::string> &getBaseFacilitiesList() const;
	/// Gets the ruleset for a craft type.
	RuleCraft *getCraft(const std::string &id, bool error = false) const;
	/// Gets the available crafts.
	const std::vector<std::string> &getCraftsList() const;

	/// Gets the ruleset for a craft weapon type.
	RuleCraftWeapon *getCraftWeapon(const std::string &id, bool error = false) const;
	/// Gets the available craft weapons.
	const std::vector<std::string> &getCraftWeaponsList() const;
	/// Is given item a launcher or ammo for craft weapon.
	bool isCraftWeaponStorageItem(const RuleItem* item) const;

	/// Gets the ruleset for an item category type.
	RuleItemCategory *getItemCategory(const std::string &id, bool error = false) const;
	/// Gets the available item categories.
	const std::vector<std::string> &getItemCategoriesList() const;
	/// Gets the ruleset for an item type.
	RuleItem *getItem(const std::string &id, bool error = false) const;
	/// Gets the available items.
	const std::vector<std::string> &getItemsList() const;
	/// Gets the ruleset for a weapon set type.
	RuleWeaponSet* getWeaponSet(const std::string& type, bool error = false) const;
	/// Gets the ruleset for a UFO type.
	RuleUfo *getUfo(const std::string &id, bool error = false) const;
	/// Gets the available UFOs.
	const std::vector<std::string> &getUfosList() const;
	/// Gets terrains for battlescape games.
	RuleTerrain *getTerrain(const std::string &name, bool error = false) const;
	/// Gets the available terrains.
	const std::vector<std::string> &getTerrainList() const;
	/// Gets mapdatafile for battlescape games.
	MapDataSet *getMapDataSet(const std::string &name);
	/// Gets skill rules.
	RuleSkill *getSkill(const std::string &name, bool error = false) const;
	/// Gets soldier unit rules.
	RuleSoldier *getSoldier(const std::string &name, bool error = false) const;
	/// Gets the available soldiers.
	const std::vector<std::string> &getSoldiersList() const;
	/// Gets commendation rules.
	RuleCommendations *getCommendation(const std::string &id, bool error = false) const;
	/// Gets the available commendations.
	const std::map<std::string, RuleCommendations *> &getCommendationsList() const;
	/// Gets generated unit rules.
	Unit *getUnit(const std::string &name, bool error = false) const;
	/// Gets alien race rules.
	AlienRace *getAlienRace(const std::string &name, bool error = false) const;
	/// Gets the available alien races.
	const std::vector<std::string> &getAlienRacesList() const;
	/// Gets an enviro effects definition.
	RuleEnviroEffects* getEnviroEffects(const std::string& name) const;
	/// Gets the available enviro effects.
	const std::vector<std::string>& getEnviroEffectsList() const;
	/// Gets a starting condition.
	RuleStartingCondition* getStartingCondition(const std::string& name) const;
	/// Gets the available starting conditions.
	const std::vector<std::string>& getStartingConditionsList() const;
	/// Gets deployment rules.
	AlienDeployment *getDeployment(const std::string &name, bool error = false) const;
	/// Gets the available alien deployments.
	const std::vector<std::string> &getDeploymentsList() const;

	/// Gets armor rules.
	Armor *getArmor(const std::string &name, bool error = false) const;
	/// Gets the all armors.
	const std::vector<std::string> &getArmorsList() const;
	/// Gets the available armors for soldiers.
	const std::vector<const Armor*> &getArmorsForSoldiers() const;
	/// Check if item is used for armor storage.
	bool isArmorStorageItem(const RuleItem* item) const;

	/// Gets Ufopaedia article definition.
	ArticleDefinition *getUfopaediaArticle(const std::string &name, bool error = false) const;
	/// Gets the available articles.
	const std::vector<std::string> &getUfopaediaList() const;
	/// Gets the available article categories.
	const std::vector<std::string> &getUfopaediaCategoryList() const;

	/// Gets the inventory list.
	std::map<std::string, RuleInventory*> *getInventories();
	/// Gets the ruleset for a specific inventory.
	RuleInventory *getInventory(const std::string &id, bool error = false) const;
	/// Gets the ruleset for right hand inventory slot.
	RuleInventory *getInventoryRightHand() const { return getInventory("STR_RIGHT_HAND", true); }
	/// Gets the ruleset for left hand inventory slot.
	RuleInventory *getInventoryLeftHand() const { return getInventory("STR_LEFT_HAND", true); }
	/// Gets the ruleset for backpack inventory slot.
	RuleInventory *getInventoryBackpack() const { return getInventory("STR_BACK_PACK", true); }
	/// Gets the ruleset for belt inventory slot.
	RuleInventory *getInventoryBelt() const { return getInventory("STR_BELT", true); }
	/// Gets the ruleset for ground inventory slot.
	RuleInventory *getInventoryGround() const { return getInventory("STR_GROUND", true); }

	/// Gets whether or not the inventory slots overlap with the paperdoll button
	bool getInventoryOverlapsPaperdoll() const { return _inventoryOverlapsPaperdoll; }
	/// Gets max view distance in BattleScape.
	int getMaxViewDistance() const { return _maxViewDistance; }
	/// Gets threshold of darkness for LoS calculation.
	int getMaxDarknessToSeeUnits() const { return _maxDarknessToSeeUnits; }
	/// Gets max static (tiles & fire) light distance in BattleScape.
	int getMaxStaticLightDistance() const { return _maxStaticLightDistance; }
	/// Gets max dynamic (items & units) light distance in BattleScape.
	int getMaxDynamicLightDistance() const { return _maxDynamicLightDistance; }
	/// Get flags for enhanced lighting, 0x1 - tiles and fire, 0x2 - items, 0x4 - units.
	int getEnhancedLighting() const { return _enhancedLighting; }
	/// Get basic damage type
	const RuleDamageType *getDamageType(ItemDamageType type) const;

	/// Gets the cost of hiring an engineer.
	int getHireEngineerCost() const;
	/// Gets the research topic required for hiring new engineers.
	const std::string &getHireEngineersUnlockResearch() const { return _hireEngineersUnlockResearch; }
	/// Gets the base functions required for hiring new engineers.
	RuleBaseFacilityFunctions getHireEngineersRequiresBaseFunc() const { return _hireEngineersRequiresBaseFunc; }
	/// Gets the cost of hiring a scientist.
	int getHireScientistCost() const;
	/// Gets the research topic required for hiring new scientists.
	const std::string &getHireScientistsUnlockResearch() const { return _hireScientistsUnlockResearch; }
	/// Gets the base functions topic required for hiring new scientists.
	RuleBaseFacilityFunctions getHireScientistsRequiresBaseFunc() const { return _hireScientistsRequiresBaseFunc; }
	/// Gets the monthly cost of an engineer.
	int getEngineerCost() const;
	/// Gets the monthly cost of a scientist.
	int getScientistCost() const;
	/// Gets the transfer time of personnel.
	int getPersonnelTime() const;
	/// Gets the odds of hiring soldiers by country.
	int getHireByCountryOdds() const { return _hireByCountryOdds; }
	/// Gets the odds of hiring soldiers by region.
	int getHireByRegionOdds() const { return _hireByRegionOdds; }

	/// Gets the global transfer cost multiplier.
	int getGlobalTransferCostMultiplier() const { return _globalTransferCostMult; }
	/// Gets the global transfer cost divider.
	int getGlobalTransferCostDivider() const { return _globalTransferCostDiv; }

	/// Gets first turn when AI can use Blaster launcher.
	int getAIUseDelayBlaster() const  {return _aiUseDelayBlaster;}
	/// Gets first turn when AI can use firearms.
	int getAIUseDelayFirearm() const  {return _aiUseDelayFirearm;}
	/// Gets first turn when AI can use grenades.
	int getAIUseDelayGrenade() const  {return _aiUseDelayGrenade;}
	/// Gets first turn when AI can use proxy grenades.
	int getAIUseDelayProxy() const  {return _aiUseDelayProxy;}
	/// Gets first turn when AI can use martial arts.
	int getAIUseDelayMelee() const {return _aiUseDelayMelee;}
	/// Gets first turn when AI can use psionic abilities.
	int getAIUseDelayPsionic() const  {return _aiUseDelayPsionic;}
	/// Gets first turn when AI can use self-target medikits.
	int getAIUseDelayMedikit() const { return _aiUseDelayMedikit; }
	/// Gets how much AI intelligence should be used to determine firing mode for sniping.
	int getAIFireChoiceIntelCoeff() const {return _aiFireChoiceIntelCoeff;}
	/// Gets how much AI aggression should be used to determine firing mode for sniping.
	int getAIFireChoiceAggroCoeff() const {return _aiFireChoiceAggroCoeff;}
	/// Gets whether or not to use extended firing mode scoring for determining which attack the AI should use
	bool getAIExtendedFireModeChoice() const {return _aiExtendedFireModeChoice;}
	/// Gets whether or not the AI should try to shoot beyond a weapon's max range, true = don't shoot if you can't
	bool getAIRespectMaxRange() const {return _aiRespectMaxRange;}
	/// Gets whether or not the AI should be allowed to continue destroying base facilities after first encountering XCom
	bool getAIDestroyBaseFacilities() const { return _aiDestroyBaseFacilities; }
	/// Gets whether or not the alien AI should pick up weapons more actively.
	bool getAIPickUpWeaponsMoreActively() const { return _aiPickUpWeaponsMoreActively; }
	/// Gets whether or not the civilian AI should pick up weapons more actively.
	bool getAIPickUpWeaponsMoreActivelyCiv() const { return _aiPickUpWeaponsMoreActivelyCiv; }
	/// Gets whether the Calypso smarter-civilian AI behaviors are enabled (Phase 32; default off).
	bool getAISmartCivilians() const { return _aiSmartCivilians; }
	/// Gets whether the Calypso fair-aliens behavior is enabled: disables the positional cheat (Phase 34; default off).
	bool getAIFairAliens() const { return _aiFairAliens; }
	/// Gets whether unengaged terror-mission aliens bias patrol toward the (quantized) civilian zone (Phase 34.4; default off).
	bool getAITerrorHuntCivilians() const { return _aiTerrorHuntCivilians; }
	/// Phase 34.6 (Calypso): master switch for hostile-AI terrain tactics (floor-drop and wall-breach targeting).
	/// Default off; with the flag off, native AND WASM behavior is byte-for-byte unchanged.
	bool getAITerrainTactics() const { return _aiTerrainTactics; }
	/// Phase 34.8 (Calypso): master switch for hostile-AI hearing (noise events + investigation
	/// patrol bias). Default off; with the flag off the AI read path is never reached, so native
	/// AND WASM behavior is byte-for-byte unchanged (noise emission is unconditional transient
	/// bookkeeping, mirroring the 34.5 knowledge-layer convention -- nothing reads it when off).
	bool getAIHearing() const { return _aiHearing; }
	/// Phase 34.7 (Calypso): master switch for near-miss suppression fire (morale/energy pin,
	/// engine-symmetric). Unlike 34.8's noise emission, the near-miss mechanic CHANGES unit
	/// state, so the gate lives at the mechanic seam (SavedBattleGame::applySuppression), not
	/// only on the read path. Default off; with the flag off no state changes, byte-identical.
	bool getAISuppression() const { return _aiSuppression; }
	/// Phase 34.7 (Calypso): per-near-miss morale loss (default 5). Flat; the knob is the
	/// tuning point, capped per turn per victim in BattleUnit::addNearMiss.
	int getAISuppressionMorale() const { return _aiSuppressionMorale; }
	/// Phase 34.7 (Calypso): per-near-miss energy drain (default 10). Flat; the knob is the
	/// tuning point, capped per turn per victim in BattleUnit::addNearMiss.
	int getAISuppressionEnergy() const { return _aiSuppressionEnergy; }
	/// Phase 34.9 (Calypso): master switch for squad coordination (blackboard-lite: soft
	/// focus-fire cap, pin-and-flank preference, wounded retreat toward the friendly cluster).
	/// Default off; with the flag off the blackboard stays empty and every read hook degrades
	/// to its pre-34.9 value, so behavior is byte-identical.
	bool getAISquadCoordination() const { return _aiSquadCoordination; }
	/// Brutal-AI (Phase 34.5): master switch for the ported Brutal-OXCE hostile AI (default off -> byte-identical vanilla).
	bool getAIBrutalAI() const { return _aiBrutalAI; }
	/// Brutal-AI: hostile omniscience level (-1 no squadsight .. 0 fair .. 2 wallhack). Default 0 (fair). Only fair levels are enabled in the mod.
	int getAICheatMode() const { return _aiCheatMode; }
	/// Brutal-AI: whether brutal units avoid known proximity mines/grenades (default true; only applies when brutalAI is on).
	bool getAIAvoidMines() const { return _aiAvoidMines; }
	/// Brutal-AI: whether to trim the per-turn option set the brutal AI evaluates, to cut turn time (default false).
	bool getAIPerformanceOptimization() const { return _aiPerformanceOptimization; }
	/// Gets the civilian unit type spawned as an armed guard (Phase 32; empty = no guards).
	const std::string& getAICivilianGuardType() const { return _aiCivilianGuardType; }
	/// Gets the per-civilian chance (%) of spawning as a guard instead (Phase 32; default 0).
	int getAICivilianGuardChance() const { return _aiCivilianGuardChance; }
	/// Gets the reaction fire threshold (default = 0).
	int getReactionFireThreshold(UnitFaction faction) const;
	/// Gets weight value that AI use to determine if target is dangerous.
	AIAttackWeight getAITargetWeightThreatThreshold() const { return _aiTargetWeightThreatThreshold; }
	/// Gets default weight value of hostile unit.
	AIAttackWeight getAITargetWeightAsHostile() const { return _aiTargetWeightAsHostile; }
	/// Gets default weight value of civilian unit when consider by aliens.
	AIAttackWeight getAITargetWeightAsHostileCivilians() const { return _aiTargetWeightAsHostileCivilians; }
	/// Gets default weight value of same faction unit.
	AIAttackWeight getAITargetWeightAsFriendly() const { return _aiTargetWeightAsFriendly; }
	/// Gets default weight value of neutral unit (xcom to civ or vice versa).
	AIAttackWeight getAITargetWeightAsNeutral() const { return _aiTargetWeightAsNeutral; }

	/// Gets maximum supported lookVariant.
	int getMaxLookVariant() const;
	/// Gets the threshold for too much smoke (vanilla default = 10).
	int getTooMuchSmokeThreshold() const  {return _tooMuchSmokeThreshold;}
	/// Gets the custom physical training factor in percent (default = 100).
	int getCustomTrainingFactor() const { return _customTrainingFactor; }
	/// Gets the chance to stop retaliation after unsuccessful xcom base attack (default = 0).
	int getChanceToStopRetaliation() const { return _chanceToStopRetaliation; }
	/// Gets the chance to detect an alien base by xcom operatives each month (default = 20).
	int getChanceToDetectAlienBaseEachMonth() const { return _chanceToDetectAlienBaseEachMonth; }
	/// Should a damaged UFO deploy less aliens during the base defense?
	bool getLessAliensDuringBaseDefense() const { return _lessAliensDuringBaseDefense; }
	/// Will countries join the good side again after the infiltrator base is destroyed?
	bool getAllowCountriesToCancelAlienPact() const { return _allowCountriesToCancelAlienPact; }
	/// Should alien infiltration bases be built close to the infiltrated country?
	bool getBuildInfiltrationBaseCloseToTheCountry() const { return _buildInfiltrationBaseCloseToTheCountry; }
	/// Should alien infiltration pick countries in a region randomly (or in a pre-defined ruleset order)?
	bool getInfiltrateRandomCountryInTheRegion() const { return _infiltrateRandomCountryInTheRegion; }
	/// Should alien bases be allowed (in worst case) on invalid globe textures or not?
	bool getAllowAlienBasesOnWrongTextures() const { return _allowAlienBasesOnWrongTextures; }
	/// Gets the global kneel bonus (default = 115).
	int getKneelBonusGlobal() const { return _kneelBonusGlobal; }
	/// Gets the global one-handed penalty (default = 80).
	int getOneHandedPenaltyGlobal() const { return _oneHandedPenaltyGlobal; }
	/// Gets whether close quarters combat is enabled (default = 0 is off).
	int getEnableCloseQuartersCombat() const { return _enableCloseQuartersCombat; }
	/// Gets the default close quarters combat accuracy (default = 100).
	int getCloseQuartersAccuracyGlobal() const { return _closeQuartersAccuracyGlobal; }
	/// Gets the default close quarters combat TU cost (default = 12).
	int getCloseQuartersTuCostGlobal() const { return _closeQuartersTuCostGlobal; }
	/// Gets the default close quarters combat energy cost (default = 8).
	int getCloseQuartersEnergyCostGlobal() const { return _closeQuartersEnergyCostGlobal; }
	/// Gets the percentage for successfully avoiding CQC when sneaking up on the enemy (default = 0% = turned off).
	int getCloseQuartersSneakUpGlobal() const { return _closeQuartersSneakUpGlobal; }
	/// Gets the default accuracy penalty for having no LOS to the target (default = 0 is no penalty)
	int getNoLOSAccuracyPenaltyGlobal() const { return _noLOSAccuracyPenaltyGlobal; }
	/// Gets the default setting for primed grenades exploding in the inventory (default = 0 is no explosion)
	int getExplodeInventoryGlobal() const { return _explodeInventoryGlobal; }
	/// Gets the surrender mode (default = 0).
	int getSurrenderMode() const { return _surrenderMode; }
	/// Gets the bug hunt mode minimum turn requirement (default = 20).
	int getBughuntMinTurn() const { return _bughuntMinTurn; }
	/// Gets the bug hunt mode maximum remaining enemies requirement (default = 2).
	int getBughuntMaxEnemies() const { return _bughuntMaxEnemies; }
	/// Gets the bug hunt mode "VIP rank" parameter (default = 0).
	int getBughuntRank() const { return _bughuntRank; }
	/// Gets the bug hunt mode low morale threshold parameter (default = 40).
	int getBughuntLowMorale() const { return _bughuntLowMorale; }
	/// Gets the bug hunt mode time units % parameter (default = 60).
	int getBughuntTimeUnitsLeft() const { return _bughuntTimeUnitsLeft; }

	/// Is the mana feature enabled (default false)?
	bool isManaFeatureEnabled() const { return _manaEnabled; }
	/// Is the mana bar enabled for the Battlescape UI (default false)?
	bool isManaBarEnabled() const { return _manaBattleUI; }
	/// Is the mana trained as a primary skill (e.g. like firing accuracy)?
	bool isManaTrainingPrimary() const { return _manaTrainingPrimary; }
	/// Is the mana trained as a secondary skill (e.g. like strength)?
	bool isManaTrainingSecondary() const { return _manaTrainingSecondary; }
	/// Gets the mana unlock research topic (default empty)?
	const std::string &getManaUnlockResearch() const { return _manaUnlockResearch; }

	/// How much missing mana will act as "fatal wounds" and prevent the soldier from going into battle.
	int getManaWoundThreshold() const { return _manaMissingWoundThreshold; }
	/// Should a soldier's mana be fully replenished after a mission?
	bool getReplenishManaAfterMission() const { return _manaReplenishAfterMission; }

	/// How much missing health will act as "fatal wounds" and prevent the soldier from going into battle.
	int getHealthWoundThreshold() const { return _healthMissingWoundThreshold; }
	/// Should a soldier's health be fully replenished after a mission?
	bool getReplenishHealthAfterMission() const { return _healthReplenishAfterMission; }

	/// Gets the cutscene ID that should be played when the player loses due to poor economy management.
	const std::string &getLoseMoneyCutscene() const { return _loseMoney; }
	/// Gets the cutscene ID that should be played when the player loses due to poor rating.
	const std::string &getLoseRatingCutscene() const { return _loseRating; }
	/// Gets the cutscene ID that should be played when the player loses the last base.
	const std::string &getLoseDefeatCutscene() const { return _loseDefeat; }

	/// Gets the research topic required for building XCOM bases on fakeUnderwater globe textures.
	const std::string &getFakeUnderwaterBaseUnlockResearch() const { return _fakeUnderwaterBaseUnlockResearch; }
	/// Gets the research topic required for building XCOM bases.
	const std::string &getNewBaseUnlockResearch() const { return _newBaseUnlockResearch; }

	/// Gets the threshold for defining a glancing hit on a ufo during interception
	int getUfoGlancingHitThreshold() const { return _ufoGlancingHitThreshold; }
	/// Gets the parameter for drawing the width of a ufo's beam weapon based on power
	int getUfoBeamWidthParameter() const { return _ufoBeamWidthParameter; }
	/// Gets the modifier to a tractor beam's power based on a ufo's size
	int getUfoTractorBeamSizeModifier(int ufoSize) const { return _ufoTractorBeamSizeModifiers[ufoSize]; }
	/// Gets the escort range
	double getEscortRange() const;
	/// Gets the setting for drawing alien base/UFO radar circles.
	int getDrawEnemyRadarCircles() const { return _drawEnemyRadarCircles; }
	/// Should escorts join the fight against HK (automatically)? Or is only fighting one-on-one allowed?
	bool getEscortsJoinFightAgainstHK() const { return _escortsJoinFightAgainstHK; }
	/// Should hunter-killers be able to retarget every 5 seconds on slow game timers (5Sec / 1Min)?
	bool getHunterKillerFastRetarget() const { return _hunterKillerFastRetarget; }
	/// Gets the crew emergency evacuation survival chance
	int getCrewEmergencyEvacuationSurvivalChance() const { return _crewEmergencyEvacuationSurvivalChance; }
	/// Gets the pilots emergency evacuation survival chance
	int getPilotsEmergencyEvacuationSurvivalChance() const { return _pilotsEmergencyEvacuationSurvivalChance; }
	/// Should UFO preview be shown in base defense UI?
	bool showUfoPreviewInBaseDefense() const { return _showUfoPreviewInBaseDefense; }
	/// Gets how many soldiers are needed for one promotion of a given rank
	int getSoldiersPerRank(const SoldierRank rank) const { return _soldiersPerRank[(size_t)rank]; }
	/// Gets the firing accuracy needed for no bonus/penalty
	int getPilotAccuracyZeroPoint() const { return _pilotAccuracyZeroPoint; }
	/// Gets the firing accuracy impact (as percentage of distance to zero point) on pilot's aim in dogfight
	int getPilotAccuracyRange() const { return _pilotAccuracyRange; }
	/// Gets the reactions needed for no bonus/penalty
	int getPilotReactionsZeroPoint() const { return _pilotReactionsZeroPoint; }
	/// Gets the reactions impact (as percentage of distance to zero point) on pilot's dodge ability in dogfight
	int getPilotReactionsRange() const { return _pilotReactionsRange; }
	/// Gets the pilot's bravery needed for very bold approach speed
	int getPilotBraveryThresholdVeryBold() const { return _pilotBraveryThresholds[0]; }
	/// Gets the pilot's bravery needed for bold approach speed
	int getPilotBraveryThresholdBold() const { return _pilotBraveryThresholds[1]; }
	/// Gets the pilot's bravery needed for normal approach speed
	int getPilotBraveryThresholdNormal() const { return _pilotBraveryThresholds[2]; }
	/// Gets a performance bonus for a given score
	int getPerformanceBonus(int score) const { return (int)(score * _performanceBonusFactor); }
	/// Should the player have the option to sort the 'New Research' list?
	bool getEnableNewResearchSorting() const { return _enableNewResearchSorting; }
	/// Should custom categories be used in Buy/Sell/Transfer GUIs? 0=no, 1=yes, custom only, 2=both vanilla and custom.
	int getDisplayCustomCategories() const { return _displayCustomCategories; }
	/// Should weapons "inherit" categories of their ammo?
	bool getShareAmmoCategories() const { return _shareAmmoCategories; }
	/// Should distance in dogfight GUI be shown in kilometers?
	bool getShowDogfightDistanceInKm() const { return _showDogfightDistanceInKm; }
	/// Should alien inventory show full name (e.g. Sectoid Leader) or just the race (e.g. Sectoid)?
	bool getShowFullNameInAlienInventory() const { return _showFullNameInAlienInventory; }
	/// Gets the offset for alien inventory paperdoll and hand slots
	int getAlienInventoryOffsetX() const { return _alienInventoryOffsetX; }
	/// Gets the extra offset for alien inventory hand slots for 2x2 units
	int getAlienInventoryOffsetBigUnit() const { return _alienInventoryOffsetBigUnit; }
	/// Show the INFO button (where applicable) or not?
	bool getShowPediaInfoButton() const { return !_hidePediaInfoButton; }
	/// Display extra item info (accuracy modifier and power bonus) in the main pedia article?
	int getExtraNerdyPediaInfoType() const { return _extraNerdyPediaInfoType; }
	/// In debriefing, give score also for already researched alien artifacts?
	bool getGiveScoreAlsoForResearchedArtifacts() const { return _giveScoreAlsoForResearchedArtifacts; }
	/// When recovering ammo, should partially spent clip have a chance to recover as full?
	bool getStatisticalBulletConservation() const { return _statisticalBulletConservation; }
	/// Does stunning an enemy improve unit and squad morale?
	bool getStunningImprovesMorale() const { return _stunningImprovesMorale; }
	/// Gets how much TU (in percent) should be given to a unit waking up from stun at the beginning of a new turn.
	int getTURecoveryWakeUpNewTurn() const { return _tuRecoveryWakeUpNewTurn; }
	/// Gets whether or not to load base defense terrain from globe texture
	int getBaseDefenseMapFromLocation() const { return _baseDefenseMapFromLocation; }

	/// Return mod what created given rule object.
	template<typename T>
	const ModData* getModCreatingRule(const T* t) const { return _ruleCreationTracking.at(static_cast<const void*>(t)); }
	/// Return mod what last updated given rule object.
	template<typename T>
	const ModData* getModLastUpdatingRule(const T* t) const { return _ruleLastUpdateTracking.at(static_cast<const void*>(t)); }

	/// Gets the ruleset for a specific research project.
	RuleResearch *getResearch(const std::string &id, bool error = false) const;
	/// Gets the ruleset for a specific research project.
	std::vector<const RuleResearch*> getResearch(const std::vector<std::string> &id) const;
	/// Gets the ruleset for a specific research project.
	const std::map<std::string, RuleResearch *> &getResearchMap() const;
	/// Gets the list of all research projects.
	const std::vector<std::string> &getResearchList() const;
	/// Gets the ruleset for a specific manufacture project.
	RuleManufacture *getManufacture (const std::string &id, bool error = false) const;
	/// Gets the list of all manufacture projects.
	const std::vector<std::string> &getManufactureList() const;
	/// Gets the ruleset for a specific soldier bonus type.
	RuleSoldierBonus *getSoldierBonus(const std::string &id, bool error = false) const;
	/// Gets the list of all soldier bonus types.
	const std::vector<std::string> &getSoldierBonusList() const;
	/// Gets the ruleset for a specific soldier transformation project.
	RuleSoldierTransformation *getSoldierTransformation(const std::string &id, bool error = false) const;
	/// Gets the list of all soldier transformation projects.
	const std::vector<std::string> &getSoldierTransformationList() const;
	/// Gets facilities for custom bases.
	std::vector<RuleBaseFacility*> getCustomBaseFacilities(GameDifficulty diff) const;
	/// Gets a specific UfoTrajectory.
	const UfoTrajectory *getUfoTrajectory(const std::string &id, bool error = false) const;
	/// Gets the ruleset for a specific alien mission.
	const RuleAlienMission *getAlienMission(const std::string &id, bool error = false) const;
	/// Gets the ruleset for a random alien mission.
	const RuleAlienMission *getRandomMission(MissionObjective objective, size_t monthsPassed) const;
	/// Gets the list of all alien missions.
	const std::vector<std::string> &getAlienMissionList() const;
	/// Gets the alien item level table.
	const std::vector<std::vector<int> > &getAlienItemLevels() const;
	/// Gets the player starting base.
	const YAML::YamlString& getDefaultStartingBase() const;
	const YAML::YamlString& getStartingBase(GameDifficulty diff) const;
	/// Gets the game starting time.
	const GameTime &getStartingTime() const;
	/// Gets the game starting difficulty.
	int getStartingDifficulty() const { return _startingDifficulty; }
	/// Gets an MCDPatch.
	MCDPatch *getMCDPatch(const std::string &id) const;
	/// Gets the list of external Sprites.
	const std::map<std::string, std::vector<ExtraSprites *> > &getExtraSprites() const;
	/// Gets the list of custom palettes.
	const std::vector<std::string> &getCustomPalettes() const;
	/// Gets the list of external Sounds.
	const std::vector<std::pair<std::string, ExtraSounds *> > &getExtraSounds() const;
	/// Gets the list of external Strings.
	const std::map<std::string, ExtraStrings *> &getExtraStrings() const;
	/// Gets the list of StatStrings.
	const std::vector<StatString *> &getStatStrings() const;
	/// Gets the research-requirements for Psi-Lab (it's a cache for psiStrengthEval)
	const std::vector<std::string> &getPsiRequirements() const;
	/// Returns the sorted list of inventories.
	const std::vector<std::string> &getInvsList() const;
	/// Generates a new soldier.
	Soldier *genSoldier(SavedGame *save, const RuleSoldier* ruleSoldier, int nationality) const;
	/// Gets the item to be used as fuel for ships.
	std::string getAlienFuelName() const;
	/// Gets the amount of alien fuel to recover
	int getAlienFuelQuantity() const;
	/// Gets the font name.
	std::string getFontName() const;
	/// Gets the maximum radar range still considered as short.
	int getShortRadarRange() const;
	/// Gets the custom scaling (defined by the modder) applied on the facility upgrade/build time reduction calculated by the game.
	int getBuildTimeReductionScaling() const { return _buildTimeReductionScaling; }
	/// Gets what type of information should be shown in craft articles for the fuel capacity/range
	int getPediaReplaceCraftFuelWithRangeType() const;
	/// Gets information on an interface element.
	RuleInterface *getInterface(const std::string &id, bool error = true) const;
	/// Gets the ruleset for the globe.
	RuleGlobe *getGlobe() const;
	/// Gets the ruleset for the converter.
	RuleConverter *getConverter() const;
	/// Gets the list of selective files for insertion into our cat files.
	const std::map<std::string, SoundDefinition *> *getSoundDefinitions() const;
	const std::vector<MapScript*> *getMapScript(const std::string& id) const;
	const std::map<std::string, std::vector<MapScript*> > &getMapScriptsRaw() const { return _mapScripts; }
	/// Gets a video for intro/outro etc.
	RuleVideo *getVideo(const std::string &id, bool error = false) const;
	const std::map<std::string, RuleMusic *> *getMusic() const;
	const std::vector<std::string>* getArcScriptList() const;
	RuleArcScript* getArcScript(const std::string& name, bool error = false) const;
	const std::vector<std::string>* getEventScriptList() const;
	RuleEventScript* getEventScript(const std::string& name, bool error = false) const;
	const std::vector<std::string>* getEventList() const;
	RuleEvent* getEvent(const std::string& name, bool error = false) const;
	const std::vector<std::string> *getMissionScriptList() const;
	const std::vector<std::string> *getAdhocScriptList() const;
	RuleMissionScript *getMissionScript(const std::string &name, bool error = false) const;
	RuleMissionScript *getAdhocScript(const std::string &name, bool error = false) const;
	/// Get global script data.
	ScriptGlobal *getScriptGlobal() const;
	RuleResearch *getFinalResearch() const;
	RuleBaseFacility *getDestroyedFacility() const;
	const std::map<int, std::string> *getMissionRatings() const;
	const std::map<int, std::string> *getMonthlyRatings() const;
	const std::map<std::string, std::string> &getFixedUserOptions() const { return _fixedUserOptions; }
	const std::map<std::string, std::string> &getRecommendedUserOptions() const { return _recommendedUserOptions; }
	const std::vector<std::string> &getHiddenMovementBackgrounds() const;
	const std::vector<std::string> &getBaseNamesFirst() const { return _baseNamesFirst; }
	const std::vector<std::string> &getBaseNamesMiddle() const { return _baseNamesMiddle; }
	const std::vector<std::string> &getBaseNamesLast() const { return _baseNamesLast; }
	const std::vector<std::string> &getOperationNamesFirst() const { return _operationNamesFirst; }
	const std::vector<std::string> &getOperationNamesLast() const { return _operationNamesLast; }
	bool getEnableUnitResponseSounds() const { return _enableUnitResponseSounds; }
	const std::map<std::string, std::vector<int> > &getSelectUnitSounds() const { return _selectUnitSound; }
	const std::map<std::string, std::vector<int> > &getStartMovingSounds() const { return _startMovingSound; }
	const std::map<std::string, std::vector<int> > &getSelectWeaponSounds() const { return _selectWeaponSound; }
	const std::map<std::string, std::vector<int> > &getAnnoyedSounds() const { return _annoyedSound; }
	const std::vector<int> &getSelectBaseSounds() const { return _selectBaseSound; }
	const std::vector<int> &getStartDogfightSounds() const { return _startDogfightSound; }
	const std::vector<int> &getFlagByKills() const;
	StatAdjustment *getStatAdjustment(int difficulty);
	int getDefeatScore() const;
	int getDefeatFunds() const;
	bool isDemigod() const;
	const std::vector<int>& getMonthlyRatingThresholds() { return _monthlyRatingThresholds; }
	const std::vector<int>& getUfoFiringRateCoefficients() { return _ufoFiringRateCoefficients; }
	const std::vector<int>& getUfoEscapeCountdownCoefficients() { return _ufoEscapeCountdownCoefficients; }
	const std::vector<int>& getRetaliationTriggerOdds() { return _retaliationTriggerOdds; }
	const std::vector<int>& getRetaliationBaseRegionOdds() { return _retaliationBaseRegionOdds; }
	const std::vector<int>& getAliensFacingCraftOdds() { return _aliensFacingCraftOdds; }

};

}
