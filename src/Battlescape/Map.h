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
#include "../Engine/InteractiveSurface.h"
#include "../Engine/Options.h"
#include "../Engine/Collections.h"
#include "../Mod/MapData.h"
#include "Position.h"
#include "Particle.h"
#include <map>
#include <memory>
#include <utility>
#include <vector>
#ifdef __EMSCRIPTEN__
#include "../Mod/Mod.h"
#include "../Calypso/HdUnitEmit.h"   // Phase 42 review #6: typed HdTileInstance / HdRgbaOverlayInstance
#endif

namespace OpenXcom
{

class SavedBattleGame;
class Surface;
class SurfaceSet;
class BattleUnit;
class Projectile;
#ifdef __EMSCRIPTEN__
class Shader;
class GpuTexture;
#endif
class Explosion;
class BattlescapeMessage;
class Camera;
class Timer;
class Text;
class Tile;
class UnitSprite;
class TTFFont;
class State;

enum CursorType { CT_NONE, CT_NORMAL, CT_AIM, CT_PSI, CT_WAYPOINT, CT_THROW };
enum TilePart : int;

/**
 * Helper class that returns all important data about the unit movement
 */
struct UnitWalkingOffset
{
	Position ScreenOffset;
	int NormalizedMovePhase;
	int TerrainLevelOffset;
};

/**
 * Interactive map of the battlescape.
 */
class Map : public InteractiveSurface
{
private:
	static const int SCROLL_INTERVAL = 15;
	static const int FADE_INTERVAL = 23;
	static const int NIGHT_VISION_SHADE = 4;
	static const int NIGHT_VISION_MAX_SHADE = 8;
	static const int BULLET_SPRITES = 35;
	/// Tile animation: 8 frames × 100 ms per frame (BattlescapeState::DEFAULT_ANIM_SPEED).
	static const int TILE_ANIM_FRAMES   = 8;
	static const int TILE_ANIM_FRAME_MS = 100;
	static const int TILE_ANIM_PERIOD_MS = TILE_ANIM_FRAMES * TILE_ANIM_FRAME_MS; // 800
	Timer *_scrollMouseTimer, *_scrollKeyTimer, *_obstacleTimer;
	Timer *_fadeTimer;
	int _fadeShade;
	bool _nightVisionOn;
	int _debugVisionMode;
	int _nvColor;
	Game *_game;
	SavedBattleGame *_save;
	bool _isTFTD;
	Surface *_arrow;
	Surface *_stunIndicator, *_woundIndicator, *_burnIndicator, *_shockIndicator;
	bool _anyIndicator, _isAltPressed, _isCtrlPressed;
	int _spriteWidth, _spriteHeight;
	int _selectorX, _selectorY;
	int _mouseX, _mouseY;
	CursorType _cursorType;
	int _cursorSize;
	int _cacheActiveWeaponUfopediaArticleUnlocked; // -1 = unknown, 0 = locked, 1 = unlocked
	bool _cacheIsCtrlPressed;
	Position _cacheCursorPosition;
	int _cacheHasLOS; // -1 = unknown, 0 = no LOS, 1 = has LOS
	// Realistic Accuracy cursor hit-% cache (Joy Narical's RA, from Brutal-OXCE by Xilmi).
	int _cacheAccuracy = -1; // -1 = unknown
	int _cacheAccuracyTextColor = -1; // -1 = unknown
	bool _cacheIsKneeled = false;
	bool _cacheTargetSelf = false;
	int _animFrame;
	Projectile *_projectile;
	bool _followProjectile;
	bool _projectileInFOV;
	std::list<Explosion *> _explosions;
	std::vector<std::vector<Particle>> _vaporParticlesInit;
	std::vector<std::vector<Particle>> _vaporParticles;
	bool _explosionInFOV, _launch;
	BattlescapeMessage *_message;
	Camera *_camera;
	int _visibleMapHeight;
	// Calypso: the post-composite GL overlay passes (cursor/projectile/smoke) are
	// gated to the owning battlescape state and clipped to the map viewport so the
	// underwater explosion/vapor effect never paints over the HUD or a modal menu.
	State* _overlayOwner = nullptr;
	int _hudTopYBase = -1;           // base-res Y of the HUD top (HD panel); -1 = use _visibleMapHeight
	int _savedScissorBox[4] = {0, 0, 0, 0};
	bool _savedScissorOn = false;
	std::vector<Position> _waypoints;
	bool _unitDying, _smoothCamera, _smoothingEngaged, _flashScreen;
	int _bgColor;
	bool _previewSettingArrows, _previewSettingTu, _previewSettingEnergy;
	Text *_txtAccuracy;
	Text *_txtUnitAP;    // current TU number over selected unit
	Text *_txtCursorAP;  // remaining TU after walking to cursor tile
	int _hoveredTU;      // remaining TU if selected unit walks to hovered tile (-1 = unknown)
	SurfaceSet *_projectileSet;
	TTFFont *_fontHdNumbers; // Phase 16: TTF font for HD cursor TU/AP numerals (lazy-cached, see getHdNumberFont)
	/// Timestamp (SDL_GetTicks) of the last blit() call — used by GPU overlay guards.
	Uint32 _lastDrawnTicks = 0u;
	// Presentation-only tail for a ballistic projectile that completed between
	// two browser frames. It never participates in collision or turn logic.
	struct ProjectileAfterimage
	{
		bool valid = false;
		Position origin, impact;
		int particle = -1;
		Uint32 expires = 0u;
	} _projectileAfterimage;
	/// Diagnostic counter: completed HUD image/text GPU passes for the live Map.
	unsigned _hudGlDrawCount = 0u;

	/// Phase 16: returns the HD cursor-numeral font, lazily caching it on first
	/// successful lookup.  Resilient to mid-session timing where the mod's TTF
	/// table is not yet populated when Map is constructed (first save-load
	/// after a hard page reset).  Returns nullptr only if the mod is genuinely
	/// absent — callers fall back to the bitmap path in that case.
	TTFFont *getHdNumberFont();

	/// Manual alpha-over composite of a 32bpp TTF surface onto a 32bpp ARGB
	/// destination, writing opaque output alpha (the overlay alpha is consumed as
	/// opacity at Screen::flip). Locks both surfaces (SDL_LockSurface is refcounted,
	/// so it is safe to call inside an outer destination lock). Shared by
	/// drawHdNumber and drawUnitNameplates.
	static void alphaOverARGB(SDL_Surface *src, SDL_Surface *dst, int dstX, int dstY);

	void drawHdNumber(Surface *dest, int x, int y, int value, Uint32 colorArgb);
#ifdef __EMSCRIPTEN__
	/// Phase 25 R5: floating HD nameplate + HP/TU/energy bars over each player
	/// unit (Emscripten HD path; gated by Mod::getCalypsoHudOverlay). Defined in
	/// Calypso/MapGl.cpp; declared here under the guard so a native build cannot
	/// reference a symbol that has no native definition (Phase 36).
	void drawUnitNameplates(Surface *surface);
	/// drawUnitNameplates caches (per battle): the selected unit's name surface key
	/// (rebuilt only on selection change, not every frame) and the HUD bar colours
	/// resolved from PAL_BATTLESCAPE (resolved once; the palette is constant in-battle).
	BattleUnit *_nameplateUnit = nullptr;
	std::string _nameplateName;
	bool _nameplateColoursReady = false;
	Uint8 _npTuR = 0, _npTuG = 0, _npTuB = 0;
	Uint8 _npEnR = 0, _npEnG = 0, _npEnB = 0;
	Uint8 _npHpR = 0, _npHpG = 0, _npHpB = 0;
#endif
	void drawUnit(UnitSprite &unitSprite, Tile *unitTile, Tile *currTile, Position tileScreenPosition, bool topLayer, BattleUnit* movingUnit = nullptr);
	void drawTerrainOverlayCPU(Surface *surface);
#ifdef __EMSCRIPTEN__
	friend class ItemSprite; // ditto — emits floor items into pre-composite
	void drawTerrainGPU(Surface *surface);
	void emitTilePass();
	void initTileGL();
	void drawTileGLPass();
	/// (Re)create the SSAA offscreen FBO sized (w×scale, h×scale); false if unavailable.
	bool ensureSsaaTarget(int w, int h);

	/// Per-tile GPU instance record submitted to the tile_atlas shader.
	/// Definition in src/Calypso/HdUnitEmit.h (Phase 42 review #6: shared typed
	/// with UnitSprite so the Map→UnitSprite emit seam carries typed pointers,
	/// not void*). The 8-float layout is asserted in MapGl.cpp::initTileGL.
	using TileInstance = HdTileInstance;

	/// Phase 22: per-tile instance for the runtime blend shader (tile_blend).
	/// Kept separate from TileInstance so the ~99% non-blend path is unaffected.
	struct BlendInstance
	{
		float    screenX,     screenY;       // top-left in screen pixels
		float    selfU,       selfV;         // self-surface cell UV
		float    neighbourU,  neighbourV;    // dominant-neighbour surface cell UV
		float    worldX,      worldY;        // tile grid coords — noise anchor (P14)
		uint32_t wangMask;                   // 4-bit corner mask as int (flat uint in shader, P16)
		float    shade;                      // 0..15
		float    alphaMask;                  // 0 or 1
		float    iso;                        // iso depth priority
		float    animFrameCount;
		float    feather, noiseScale, noiseAmp;  // per-instance knobs (P4)
	};

	/// Per-atlas draw group: one glDrawArraysInstanced per atlas texture.
	struct AtlasGroup
	{
		GpuTexture*               atlas    = nullptr;
		float                     tileUVW  = 0.0f;
		float                     tileUVH  = 0.0f;
		bool                      isRgba   = false;
		std::vector<TileInstance> instances;
		// Phase 13.1 + post-14: per-(Z,Y) row descriptors into instances[].
		// Y-row granularity is needed so unit emits at (z, y) can be interleaved
		// between rows for correct iso wall→unit→wall ordering. yLevel == -1
		// is the legacy "whole Z" sentinel, no longer emitted.
		struct ZSlice { int zLevel; int yLevel; size_t first; size_t count; };
		std::vector<ZSlice>       zSlices;

		const ZSlice* findZRowSlice(int z, int y) const {
			for (const auto& s : zSlices)
				if (s.zLevel == z && s.yLevel == y) return &s;
			return nullptr;
		}

		// Phase 17: hybrid overlay (RGBA HD overrides drawn after baseline).
		GpuTexture*               overlayAtlas = nullptr;
		// Phase 25 R3: tangent-space normal atlas (non-owning; owned by TileAtlasSpec).
		GpuTexture*               normalAtlas  = nullptr;  // nullptr = no relief for this dataset
		bool                      hasNormalMap = false;    // cached null-check for the hot draw loop
		// Phase 25 R6: material emissive atlas (non-owning; owned by TileAtlasSpec).
		GpuTexture*               emissiveAtlas = nullptr; // nullptr = no glow for this dataset
		bool                      hasEmissive   = false;   // cached null-check for the hot draw loop
		std::vector<TileInstance> overlayInstances;
		// Phase 20: true when the overlay PNG was written with premultiplied alpha.
		bool                      premultipliedAlpha = false;
		// Phase 20.5: per-sub-layer overlay buffers; index 0 = reserved for base
		// overlay (overlayInstances above), indices 1..N = additional sub-layers.
		std::vector<GpuTexture*>               subLayerAtlases;    // owned by TileAtlasSpec
		std::vector<std::vector<TileInstance>> subLayerInstances;  // parallel to subLayerAtlases
		// Phase 22: runtime blend bucket; drawn by tile_blend shader after the overlay pass.
		std::vector<BlendInstance>             blendInstances;
		// Phase 22 (H1): instance offsets into Map::_tileInstIBO for the dirty-gated
		// terrain upload (baseline / overlay / per-sub-layer). Recomputed whenever the
		// instance buffer is rebuilt; plain offsets (no GL handles) so they stay valid
		// across the setPalette clear()/resize() of _tileAtlasGroups.
		size_t                                 baselineOffset = 0;
		size_t                                 overlayOffset  = 0;
		std::vector<size_t>                    subLayerOffsets;
	};

	std::vector<AtlasGroup>  _tileAtlasGroups;
	Shader*                  _tileShader     = nullptr;  // palette variant (R8 + shade table)
	Shader*                  _tileShaderRgba = nullptr;  // RGBA variant (GL_LINEAR + linear shade)
	Shader*                  _blendShader    = nullptr;  // Phase 22: runtime blend (tile_blend)
	GpuTexture*              _shadeTableTex  = nullptr;
	GpuTexture*              _shadeCurveTex  = nullptr;  // Phase 22: 16×1 night shade ramp (§22.4)
	GpuTexture*              _noiseTex       = nullptr;  // Phase 22: 256×256 tileable noise
	unsigned int _tileVAO    = 0;
	unsigned int _tileVBO    = 0;  // corner quads (static)
	unsigned int _tileIBO    = 0;  // instance data (dynamic, per-frame)
	unsigned int _blendVAO   = 0;  // Phase 22: blend instance VAO
	unsigned int _blendIBO   = 0;  // Phase 22: blend instance buffer
	// Phase 22 (H1): dedicated terrain instance buffer (baseline/overlay/sub-layer).
	// Separate from _tileVAO/_tileIBO (which units still stream into every frame) so
	// terrain instance data — stable between emits — is uploaded only when dirty.
	unsigned int _tileInstVAO = 0;
	unsigned int _tileInstIBO = 0;
	std::vector<TileInstance> _tileInstUpload;     // CPU scratch concatenation; rebuilt only when dirty
	bool         _tileBuffersDirty = true;         // re-upload _tileInstIBO only after an emit / group rebuild
	bool         _tileGLInit = false;
	// SSAA offscreen target for the HD floor pass: the tile pass renders into an
	// offscreen FBO at _ssaaScale× the viewport (normal alpha-blending, unchanged),
	// then linearly downsamples (blit) into the default framebuffer. Supersampling
	// antialiases the diamond silhouette, the blend transitions AND the texture
	// without the alpha-to-coverage/blend conflict MSAA would hit. Lazily
	// (re)created to match the live viewport × scale.
	unsigned int _ssaaFBO     = 0;
	unsigned int _ssaaColorTex = 0;  // Phase 28: TEXTURE (was renderbuffer) so the
	                                 // underwater grade pass can sample the scene
	unsigned int _ssaaDepthRB = 0;
	int          _ssaaW       = 0;   // FBO width  = displayWidth  × scale
	int          _ssaaH       = 0;   // FBO height = displayHeight × scale
	int          _ssaaScale   = 2;   // supersample factor on top of display res
	                                 // (1 = render floor at native display res;
	                                 //  2 = +2× supersample — 4× fragments)
	// Phase 25 (R0): when GpuInit::hdr() is available, _ssaaColorTex is GL_RGBA16F
	// (float, range > 1.0) so bloom/god-rays/emissive can blow out past white;
	// the grade pass tonemaps HDR→LDR (u_hdr=1). Falls back to GL_RGBA8 (LDR,
	// u_hdr=0, identity tonemap) when the extension or a complete FBO is absent.
	bool         _ssaaIsHDR   = false;
	// Phase 28: underwater colour-grade post-process. The scene (SSAA texture) is
	// fed through underwater_grade.frag into the default framebuffer (this both
	// downsamples AND grades, replacing the plain SSAA blit). Fires pre-composite
	// so the HUD/cursor (CPU surface) are never tinted.
	Shader*      _gradeShader = nullptr;
	unsigned int _gradeVAO    = 0;
	unsigned int _gradeVBO    = 0;
	void drawSceneGrade();           // fullscreen grade quad: _ssaaColorTex → screen

	// Phase 25 (R1): per-source coloured emissive light. Screen-space additive
	// halos (one quad per fire-lit tile), drawn into the SSAA scene buffer AFTER
	// tiles/units but BEFORE the grade/tonemap, so under HDR the halo can exceed
	// 1.0 and the bloom threshold picks it up (a colour the LDR frame never had).
	// The engine has no RGB point lights (Tile::_light[] is a scalar uint8), so
	// the colour is composited in screen space here.
	struct EmissiveSource { float cx, cy; float intensity; };  // centre (base-res px), 0..1
	std::vector<EmissiveSource> _emissiveSources;   // rebuilt each emitTilePass()
	Shader*      _emissiveShader = nullptr;
	unsigned int _emissiveVAO    = 0;  // inline unit-quad VAO (not shared with tiles)
	unsigned int _emissiveVBO    = 0;
	void drawEmissiveGLPass();       // additive coloured halos into the SSAA buffer
	/// Fractional animation cycle position [0, 1) — set each frame, passed as u_animFrame.
	float        _animFrameGPU = 0.0f;
	/// Phase 25 R3: eased azimuth of the auto-driven relief sun. Lerps toward a
	/// per-turn target each frame so the "time of day" sweep transitions smoothly
	/// instead of snapping at turn boundaries. Per-battle (Map is recreated).
	float        _reliefSunAzimuth = 0.0f;
	/// Phase 25 R3: last SDL_GetTicks() (seconds) sampled by the relief-sun ease,
	/// so the lerp uses a real dt (FPS-independent 1 - exp(-dt/tau)). < 0 = first
	/// frame (no prior sample yet). Per-battle (Map is recreated).
	float        _reliefSunLastT  = -1.0f;
	/// Lifetime flag: reset in ~Map() so the registered GPU-pass lambda becomes a no-op.
	std::shared_ptr<bool>    _gpuAliveFlag;

	// Blocks 11.8–11.9: GPU sprite rendering (projectiles, smoke, explosions)
	Shader*      _spriteShader = nullptr;
	unsigned int _spriteVAO   = 0;
	unsigned int _spriteVBO   = 0;
	/// Unified sprite frame cache: (SurfaceSet*, frameIdx) → RGBA GpuTexture.
	std::map<std::pair<SurfaceSet*, int>, GpuTexture*> _spriteFrameCache;
	/// Phase 24 UX: RGBA UI marker textures (selection ring, reticle, …) loaded
	/// from mod PNGs, keyed by mod-relative path. White silhouettes, tinted at draw.
	std::map<std::string, GpuTexture*> _uiTexCache;
	GpuTexture* getUITexture(const std::string& relPath, int wrap = 0);  // wrap: 0=ClampToEdge,1=Repeat
	bool _spriteGLInit = false;
	void initSpriteGL();
	GpuTexture* getOrUploadSpriteFrame(SurfaceSet* set, int frameIdx);
	void drawProjectileGLPass();

	// Calypso bug 1: physical-resolution HUD text overlay (selected-unit name + stat
	// digits). The logical HUD widgets render mushy at the low resolution-menu fractions
	// because their backing surfaces are sized in LOGICAL px then stretched. These items
	// (logical widget rect + text + colour, pushed by BattlescapeState::applyHudName /
	// applyHudNumber) are re-rendered as TTF texture quads at PHYSICAL resolution, OVER the
	// composited HUD, mirroring drawCursorOverlayGLPass — so the 48px raster is GPU-downscaled
	// straight to the physical size, never through the small logical buffer.
	// fitX/Y/W/H is the EXACT logical placement rect the CPU path blitted the glyphs at
	// (computed in applyHudName/applyHudNumber); the overlay reuses it verbatim so the crisp
	// physical-res copy lands precisely over the logical text — no offset double-image.
	// Each item is owned by a fixed `slot` (name + the 4 stat digits), so its producer
	// (applyHudName / applyHudNumber) rebuilds ONLY its own item via setHudText() — no
	// global clear, so a future partial stat refresh can't accumulate duplicate digits.
	struct HudTextItem { int slot; float fitX, fitY, fitW, fitH; std::string text; Uint32 colorArgb; };
	std::vector<HudTextItem>            _hudTextItems;
	std::map<std::string, GpuTexture*>  _hudTextTexCache;   // keyed by "text#argb"
	GpuTexture* getHudTextTexture(const std::string& text, Uint32 colorArgb);
	void drawHudTextGLPass();
	/// Calypso Phase 41 (commit 4.5): dev-only scene-preview tile-coordinate
	/// readout under the cursor (§41.1c) -- drives the generic HUD text
	/// overlay above via setHudText/clearHudText, no dedicated GL pass.
	void updateScenePreviewCoordHud();

	// Calypso: physical-resolution HUD IMAGE overlay — same idea as the text overlay but for
	// the non-text HUD art (soldier portrait, HD rank insignia, stat-number ring/box). They too
	// pixelate at the low resolution-menu fractions because they go through the small logical
	// widget buffer; here their RGBA source is GPU-sampled (LINEAR) straight to the physical
	// widget rect. Fixed slots (not a cleared list) so the BattlescapeState apply* calls — which
	// fire in a different order than the text ones — each own exactly one slot, order-independent.
public:
	// HUD_TXT_VISIBLE_0..+9 are the 10 visible-enemy indicator digits (Calypso:
	// rendered crisp + scalable via the TTF GL overlay instead of the bitmap font).
	// HUD_TXT_PREVIEW_COORD (Phase 41 commit 4.5): dev-only scene-preview readout.
	enum HudTextSlot  { HUD_TXT_NAME, HUD_TXT_TU, HUD_TXT_ENERGY, HUD_TXT_HEALTH, HUD_TXT_MORALE, HUD_TXT_VISIBLE_0,
	                    HUD_TXT_PREVIEW_COORD = HUD_TXT_VISIBLE_0 + 10, HUD_TXT_COUNT };
	enum HudImageSlot { HUD_IMG_PORTRAIT, HUD_IMG_RANK, HUD_IMG_TU, HUD_IMG_ENERGY, HUD_IMG_HEALTH, HUD_IMG_MORALE, HUD_IMG_COUNT };
private:
	struct HudImageItem { float fitX, fitY, fitW, fitH; GpuTexture* tex; bool active = false; };
	HudImageItem                        _hudImageSlots[HUD_IMG_COUNT];
	std::map<std::string, GpuTexture*>  _hudImageTexCache;   // keyed by caller-supplied content/size key

	/// Smoke/fire/explosion instance collected during emitTilePass() / emitSmokeInstances().
	struct SmokeInstance
	{
		int screenX, screenY;  // blit top-left in SDL-surface space
		SurfaceSet* set;       // PCK surface set (SMOKE, X1, HIT)
		int frameIdx;          // frame index within set
		float darken;          // u_darken: 0.0=normal, 1.0=full black
		float density;         // Calypso: tile smoke 0..1 (drives murk strength)
		float seedX, seedY;    // Calypso: world tile coords — murk noise anchor
	};
	std::vector<SmokeInstance> _smokeInstances;
	void emitSmokeInstances();
	void drawSmokeGLPass();
	/// Calypso: layered tile-smoke murk, drawn PRE-composite (under the CPU HUD/menu
	/// layer, so it never covers them and isn't clipped). _murkTime freezes while the
	/// battlescape is not the top state, so the murk holds still under an open menu.
	void drawMurkGLPass();
	float _murkTime = 0.0f;

	// --- Calypso Phase 30: hit/impact FX (Срез A). All Emscripten-only. ---
	/// Transient additive flash quad at a hit voxel; drawn over the scene in
	/// drawSmokeGLPass, ages by wall-clock, self-erases.
	// kind: 0 = sharp 4-frame burst (hits / land blast); 1 = underwater vapor bubble
	// (single texture, expand→hold→collapse size curve).
	struct ImpactFlash { Position voxel; unsigned int spawnTick; unsigned int delayMs; float lifeMs; float sizeMul; float r, g, b; int kind; };
	std::vector<ImpactFlash> _impactFlashes;
	/// Per-unit sprite jolt toward bullet travel: a screen-space unit-vector +
	/// remaining anim frames. Keyed by BattleUnit::getId(). Faction-agnostic.
	struct UnitShake { float dx, dy; int framesLeft; int totalFrames; };
	std::map<int, UnitShake> _unitShakeOffset;
	/// Camera shake (decaying sine); amplitude in surface px, 0 = inactive.
	/// mutable so currentShakeOffset() (const) can settle it back to 0 on expiry.
	mutable float _shakeAmp     = 0.0f;
	unsigned int  _shakeStartMs = 0;
	float         _shakeFreq    = 46.0f;   // angular frequency (low = heavy/underwater sway)
	float         _shakeDur     = 0.30f;   // seconds
	Position currentShakeOffset() const;   // (0,0,0) when no shake is active

	// --- Calypso Phase 30 Срез B: aftermath FX (blood plume + wound-glow). ---
	/// Underwater blood plume: a drifting crimson cloud spawned when a unit's flesh
	/// is wounded; PRE-composite (under HUD), drifts up + expands + fades over ~2.2 s.
	struct BloodFx { Position tile; unsigned int spawnTick; unsigned int lifeMs; float seed; int faction; };
	std::vector<BloodFx> _bloodFx;
	// Land blood pools (depth==0) + charred-ground scorch decals are PERSISTENT (kept until
	// mission end) and now live on SavedBattleGame (CalypsoBloodPool / CalypsoScorchDecal,
	// via _save->getCalypsoBloodPools() / getCalypsoScorchDecals()), NOT here — a Map member
	// would be lost when an in-game resolution change tears down the Map. The transient
	// underwater plume (_bloodFx) stays Map-local (short-lived, fine to drop on a res change).
	void drawBloodGLPass();
	/// Residual wound-glow: a pulsing crimson glow on living wounded units, intensity
	/// from getFatalWounds(); POST-composite, scissored to the map. Stateless (derived
	/// each frame from the unit list — appears when wounded, gone when healed/dead).
	void drawWoundGlowGLPass();

	// --- Calypso explosion FX (depth-split AoE). All Emscripten-only. ---
	/// GL transient particles: sparks/debris (land), bubble-jets/foam (underwater).
	/// Screen-space ballistics anchored to a spawn voxel (the burst pans with the
	/// camera); HD colour via the sprite shader (the CPU Particle path can't do additive).
	// texCode: 0 = soft dot (particle.png — sparks/bubbles/foam/gas); 100+v = smoke-v;
	// 200+v = debris-v (rock sprites). Selects the per-particle texture in the draw pass.
	struct FxParticle { Position origin; unsigned int spawnTick; unsigned int delayMs; float lifeMs;
		float vx, vy, ax, ay; float size; float r, g, b; bool additive; int texCode;
		float opacity = 1.0f; };   // Calypso: per-particle alpha multiplier (semi-transparent smoke); default 1 = unchanged
	std::vector<FxParticle> _fxParticles;
	void drawFxParticlesGLPass();
	/// E2: underwater shockwave — an expanding radial distortion ring of the scene,
	/// applied in the grade pass (drawSceneGrade reads these, projects to UV, feeds
	/// underwater_grade.frag). Spawned on an underwater AoE blast.
	struct Shockwave { Position voxel; unsigned int spawnTick; float lifeMs; };
	std::vector<Shockwave> _shockwaves;

	/// Calypso: gating + clipping for the post-composite overlay passes (internal).
	bool overlayPassesActive() const;   // false when a modal/other state is on top
	/// Calypso bug 1: HUD-text overlay gate — true while the battlescape HUD is actually
	/// VISIBLE (battlescape is top, OR only a non-fullscreen popup sits over it). Unlike the
	/// cursor gate it survives action-menu/CANCEL/warning popups, so the name+stat digits stay
	/// crisp behind them instead of dropping to the mushy logical text. A fullscreen state
	/// (Inventory/Options/pause) covers the HUD → returns false (don't overdraw it).
	bool hudOverlayVisible() const;
	int  mapClipBottomY() const;        // base-res bottom of the visible map (above HUD)
	void beginMapScissor();             // glScissor to the map viewport
	void endMapScissor();               // restore prior scissor state

	/// Block 11.10 / Phase 16: tile-space cursor marker overlay instance.
	/// CS_RASTER:         existing sprite path (set + frameIdx).
	/// CS_MARKER_*:       SDF 4-tip animated path — set/frameIdx unused.
	enum CursorStyle : uint8_t
	{
		CS_RASTER          = 0,
		CS_MARKER_NEUTRAL  = 1,  // cyan   — empty tile, no player selected
		CS_MARKER_ALLY     = 2,  // blue   — FACTION_PLAYER unit
		CS_MARKER_ENEMY    = 3,  // orange — hostile/neutral unit
		CS_AP_RING         = 4,  // arc ring over selected unit (TU gauge, unused)
		CS_FLOOR_RING      = 5,  // iso ellipse ring on floor — player selected, empty tile
		CS_TEX_TINT        = 6,  // Phase 24: RGBA UI texture (tex) drawn tinted (tintRGB)
	};
	struct CursorOverlayInstance
	{
		int screenX, screenY;
		SurfaceSet* set;
		int frameIdx;
		CursorStyle style;
		float extraData = 0.0f;  // CS_AP_RING: arcFraction [0..1]
		GpuTexture* tex = nullptr;             // CS_TEX_TINT: source texture
		float tintR = 1.0f, tintG = 1.0f, tintB = 1.0f;  // CS_TEX_TINT: multiply tint
		float sizeMul = 1.0f;  // CS_TEX_TINT: quad size = sizeMul * tile size
		int   offY = 0;        // CS_TEX_TINT: extra screen-Y offset (over-head markers)
		float rot = 0.0f;      // CS_TEX_TINT: rotation (radians, CW) around the quad centre
	};
	std::vector<CursorOverlayInstance> _cursorOverlayInstances;
	void drawCursorOverlayGLPass();

	/// Phase 15: SDF cursor GL objects.
	Shader*      _cursorShader      = nullptr;
	unsigned int _cursorVAO         = 0;
	unsigned int _cursorVBO         = 0;   // static unit quad (4 verts × 2 floats)
	unsigned int _cursorInstanceVBO = 0;   // per-instance attrs (dynamic)
	bool         _cursorGLInit      = false;
	void initCursorGL();

	/// Phase 14.2: per-unit-atlas instance buffer for the unit GPU pass.
	/// Keyed by atlas texture pointer (one draw call per unit PCK set).
	struct UnitAtlasGroup
	{
		/// Phase 42 E1: production RGBA overlay sibling instance. Definition in
		/// src/Calypso/HdUnitEmit.h (review #6 — typed, shared with UnitSprite).
		using RgbaOverlayInstance = HdRgbaOverlayInstance;
		const Mod::UnitAtlasSpec* spec = nullptr;
		std::vector<TileInstance>  instances;
		/// zLevels[i] = map Z of instances[i]. Same length as instances.
		/// Used by drawTileGLPass to interleave unit draws between tile Z slices,
		/// so higher-Z tiles can occlude lower-Z units (e.g. submarine roof
		/// above units inside the cargo bay).
		std::vector<int>           zLevels;
		/// yLevels[i] = map Y of instances[i]. Same length as instances. Lets
		/// drawTileGLPass interleave unit draws per (Z, Y) row so walls of
		/// camera-near rows (Y > unit's Y) can occlude the unit from in front.
		std::vector<int>           yLevels;
		std::vector<TileInstance>  g0OverlayInstances;
		// Phase 42 E1: production RGBA overlay instances, one vector per overlay
		// page (indexed in lockstep with Mod::UnitAtlasSpec::rgbaOverlayPages).
		// Sized when the group is created from the spec's page count.
		std::vector<std::vector<RgbaOverlayInstance>> rgbaOverlayInstances;
	};
	std::vector<UnitAtlasGroup> _unitAtlasGroups;
	std::vector<HdUnitPainterDraw> _unitPainterScratch; // retained hot-path capacity
	std::vector<TileInstance> _unitUploadScratch;       // retained hot-path capacity
	HdUnitEmitTargets makeHdUnitEmitTargets(size_t bodyIdx, bool haveItem,
	                                      size_t itemIdx, int z, int y, int x,
	                                      int renderWidth, int renderHeight);
	// Phase 27.5: soft contact-shadow ellipse under each unit so HD sprites read
	// as planted, not floating. One shared RGBA texture (black, soft-ellipse
	// alpha) + a per-frame instance list filled in drawUnit and rendered in the
	// overlay phase (depth-occluded by the unit body, blended over the floor).
	GpuTexture*               _unitShadowTex = nullptr;
	std::vector<TileInstance> _unitShadowInst;
	void emitUnitPass();
	void drawUnitGLPass();
#endif
	int getTerrainLevel(const Position& pos, int size) const;
	int getWallShade(TilePart part, Tile* tileFrot);
	int _iconHeight, _iconWidth, _messageColor;
	int _hostileBarColor, _neutralBarColor, _borderBarColor;
	const std::vector<Uint8> *_transparencies;
	bool _showObstacles;
	bool _showInfoOnCursor;
public:
	/// Creates a new map at the specified position and size.
	Map(Game* game, int width, int height, int x, int y, int visibleMapHeight);
	/// Cleans up the map.
	~Map();
	/// Initializes the map.
	void init();
	/// Calypso: owning battlescape state — overlay passes only fire while it is on top.
	void setOverlayOwner(State* s) { _overlayOwner = s; }
	/// Calypso: base-res Y of the (HD) HUD top, so overlays/vapor clip above it.
	void setHudTopY(int y) { _hudTopYBase = y; }
	/// Calypso: read back the base-res Y of the (HD) HUD top (scissor boundary).
	/// Diagnostic/test-only consumers (calypso_hud_layout_probe) use this to
	/// assert the scissor tracks the panel top across resizes.
	int getHudTopY() const { return _hudTopYBase; }
#ifdef __EMSCRIPTEN__
	/// Calypso Phase 30: trigger hit FX at a contact point (faction-agnostic; unit
	/// may be absent for terrain/object hits). dirX/dirY = screen-space push for the
	/// unit jolt (ignored when unitId < 0). Emscripten-only.
	void triggerHitFx(Position voxelCenter, int power, int unitId, float dirX, float dirY);
	/// Calypso Phase 30: start a decaying camera shake. base amplitude in native px;
	/// freq = angular frequency (low = heavy sway), durSec = duration.
	void triggerShake(float amplitudePx, float freq = 46.0f, float durSec = 0.30f);
	/// Calypso Phase 30 (Срез B): spawn a blood plume at a wounded unit's tile.
	/// Called from TileEngine::hitUnit on a flesh-wounding hit (any faction).
	void spawnBloodFx(Position unitTile, int healthDamage, int faction);
	/// Calypso explosion FX: depth-split AoE blast — camera shake + big coloured flash
	/// + a GL particle burst, plus (underwater) a scatter of small bubble-bursts over the
	/// blast radius. Called once at blast start from ExplosionBState::init. radius = tiles.
	void triggerAoEFx(Position voxelCenter, int power, int radius, bool underwater, int damageType);
	/// Calypso bug 1: HUD text overlay handoff. Set (replace) or clear the text item owned by
	/// `slot` (HudTextSlot) — applyHudName owns HUD_TXT_NAME, applyHudNumber owns each stat slot,
	/// so each rebuilds only its own item (no global clear → no duplicate accumulation).
	void setHudText(int slot, float fitX, float fitY, float fitW, float fitH, const std::string& text, Uint32 colorArgb);
	void clearHudText(int slot);
	/// Calypso: HUD image overlay handoff. Upload (or fetch cached, keyed by `key`) the RGBA
	/// `src` and bind it to `slot` (HudImageSlot), to be drawn at PHYSICAL res over the logical
	/// widget rect (x,y,w,h). src==null or w/h<=0 clears the slot. Pass a key that changes when
	/// the content/size changes (e.g. "rank#3", "portrait#XCOM_0.SPK", "box#120x64#ffcc00").
	void setHudImage(int slot, const std::string& key, SDL_Surface* src, int x, int y, int w, int h);
	/// Clear one HUD image slot (slot art no longer applies, e.g. non-soldier unit).
	void clearHudImage(int slot);
	/// Calypso: read back the base-resolution rect (x, y, w, h) last pushed for a
	/// HUD text slot. Returns false when the slot is inactive/unknown. Diagnostic/
	/// test-only (calypso_hud_layout_probe) — used to verify the GL overlay rect
	/// stays aligned with the CPU HUD widget after a resize.
	bool getHudTextRect(int slot, int* x, int* y, int* w, int* h) const;
	/// Calypso: read back the base-resolution rect (x, y, w, h) last pushed for a
	/// HUD image slot. Returns false when the slot is inactive/unknown. Diagnostic/
	/// test-only (calypso_hud_layout_probe).
	bool getHudImageRect(int slot, int* x, int* y, int* w, int* h) const;
	/// Test/diagnostic proof that the registered HUD GPU pass reached its draw path.
	unsigned getHudGlDrawCount() const { return _hudGlDrawCount; }
#endif
	/// Handles timers.
	void think() override;
	/// Update visibility timestamp and blit.
	void blit(SDL_Surface *surface) override;
	/// Draws the surface.
	void draw() override;
	void refreshAIProgress(int progress);
	/// Sets the palette.
	void setPalette(const SDL_Color *colors, int firstcolor = 0, int ncolors = 256) override;
	void refreshHiddenMovementBackground();
	/// Special handling for mouse press.
	void mousePress(Action *action, State *state) override;
	/// Special handling for mouse release.
	void mouseRelease(Action *action, State *state) override;
	/// Special handling for mouse over
	void mouseOver(Action *action, State *state) override;
	/// Special handling for key presses.
	void keyboardPress(Action *action, State *state) override;
	/// Special handling for key releases.
	void keyboardRelease(Action *action, State *state) override;
	/// Rotates the tile frames 0-7
	void animate(bool redraw);
	/// Sets the battlescape selector position relative to mouse position.
	void setSelectorPosition(int mx, int my);
	/// Gets the currently selected position.
	void getSelectorPosition(Position *pos) const;
	/// Calculates the offset of a soldier, when it is walking in the middle of 2 tiles.
	UnitWalkingOffset calculateWalkingOffset(const BattleUnit *unit) const;
	/// Sets the 3D cursor type.
	void setCursorType(CursorType type, int size = 1);
	/// Gets the 3D cursor type.
	CursorType getCursorType() const;

	/// Sets projectile.
	void setProjectile(Projectile *projectile);
	/// Gets projectile.
	Projectile *getProjectile() const;
	/// Sets follow projectile flag.
	void setFollowProjectile(bool followProjectile) { _followProjectile = followProjectile; }
	/// Gets follow projectile flag.
	bool getFollowProjectile() const { return _followProjectile; }
	/// Gets alt pressed flag.
	bool isAltPressed() const { return _isAltPressed; }
	/// Gets ctrl pressed flag.
	bool isCtrlPressed() const { return _isCtrlPressed; }
	/// Add new vapor particle.
	void addVaporParticle(Position pos, Particle particle);
	/// Get all vapor for tile.
	Collections::Range<const Particle*> getVaporParticle(const Tile* tile, int topLayer) const;
	/// Gets explosion set.
	std::list<Explosion*> *getExplosions();

	/// Gets the pointer to the camera.
	Camera *getCamera();
	/// Mouse-scrolls the camera.
	void scrollMouse();
	/// Keyboard-scrolls the camera.
	void scrollKey();
	/// fades in/out
	void fadeShade();
	/// Get waypoints vector.
	std::vector<Position> *getWaypoints();
	/// Set mouse-buttons' pressed state.
	void setButtonsPressed(Uint8 button, bool pressed);
	/// Sets the unitDying flag.
	void setUnitDying(bool flag);
	/// Refreshes the battlescape selector after scrolling.
	void refreshSelectorPosition();
	/// Special handling for updating map height.
	void setHeight(int height) override;
	/// Special handling for updating map width.
	void setWidth(int width) override;
	/// Get the vertical position of the hidden movement screen.
	int getMessageY() const;
	/// Get the icon height.
	int getIconHeight() const;
	/// Get the icon width.
	int getIconWidth() const;
	/// Convert a map position to a sound angle.
	int getSoundAngle(const Position& pos) const;
	/// Reset the camera smoothing bool.
	void resetCameraSmoothing();
	/// Set whether the screen should "flash" or not.
	void setBlastFlash(bool flash);
	/// Check if the screen is flashing this.
	bool getBlastFlash() const;
	/// Modify shade for fading
	int reShade(Tile *tile);
	/// toggle the night-vision mode
	void enableNightVision();
	void toggleNightVision();
	void toggleDebugVisionMode();
	void persistToggles();
	/// Resets obstacle markers.
	void resetObstacles();
	/// Enables obstacle markers.
	void enableObstacles();
	/// Disables obstacle markers.
	void disableObstacles();
};

}
