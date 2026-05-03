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

	/// Phase 16: returns the HD cursor-numeral font, lazily caching it on first
	/// successful lookup.  Resilient to mid-session timing where the mod's TTF
	/// table is not yet populated when Map is constructed (first save-load
	/// after a hard page reset).  Returns nullptr only if the mod is genuinely
	/// absent — callers fall back to the bitmap path in that case.
	TTFFont *getHdNumberFont();

	void drawHdNumber(Surface *dest, int x, int y, int value, Uint32 colorArgb);
	void drawUnit(UnitSprite &unitSprite, Tile *unitTile, Tile *currTile, Position tileScreenPosition, bool topLayer, BattleUnit* movingUnit = nullptr);
	void drawTerrainOverlayCPU(Surface *surface);
#ifdef __EMSCRIPTEN__
	friend class UnitSprite; // needs Map::TileInstance for emit targets
	friend class ItemSprite; // ditto — emits floor items into pre-composite
	void drawTerrainGPU(Surface *surface);
	void emitTilePass();
	void initTileGL();
	void drawTileGLPass();

	/// Per-tile GPU instance record submitted to the tile_atlas shader.
	struct TileInstance
	{
		float screenX, screenY;   // top-left of tile in screen pixels
		float atlasU,  atlasV;    // UV of primary frame top-left in atlas
		float shade;              // 0..15
		float animFrameCount;     // total anim frames (>=1)
		float alphaMask;          // MCD opacity flag (0 or 1)
		float iso;                // iso priority [0..1]; larger = closer to camera
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
		std::vector<TileInstance> overlayInstances;
	};

	std::vector<AtlasGroup>  _tileAtlasGroups;
	Shader*                  _tileShader     = nullptr;  // palette variant (R8 + shade table)
	Shader*                  _tileShaderRgba = nullptr;  // RGBA variant (GL_LINEAR + linear shade)
	GpuTexture*              _shadeTableTex = nullptr;
	unsigned int _tileVAO    = 0;
	unsigned int _tileVBO    = 0;  // corner quads (static)
	unsigned int _tileIBO    = 0;  // instance data (dynamic, per-frame)
	bool         _tileGLInit = false;
	/// Fractional animation cycle position [0, 1) — set each frame, passed as u_animFrame.
	float        _animFrameGPU = 0.0f;
	/// Lifetime flag: reset in ~Map() so the registered GPU-pass lambda becomes a no-op.
	std::shared_ptr<bool>    _gpuAliveFlag;

	// Blocks 11.8–11.9: GPU sprite rendering (projectiles, smoke, explosions)
	Shader*      _spriteShader = nullptr;
	unsigned int _spriteVAO   = 0;
	unsigned int _spriteVBO   = 0;
	/// Unified sprite frame cache: (SurfaceSet*, frameIdx) → RGBA GpuTexture.
	std::map<std::pair<SurfaceSet*, int>, GpuTexture*> _spriteFrameCache;
	bool _spriteGLInit = false;
	void initSpriteGL();
	GpuTexture* getOrUploadSpriteFrame(SurfaceSet* set, int frameIdx);
	void drawProjectileGLPass();

	/// Smoke/fire/explosion instance collected during emitTilePass() / emitSmokeInstances().
	struct SmokeInstance
	{
		int screenX, screenY;  // blit top-left in SDL-surface space
		SurfaceSet* set;       // PCK surface set (SMOKE, X1, HIT)
		int frameIdx;          // frame index within set
		float darken;          // u_darken: 0.0=normal, 1.0=full black
	};
	std::vector<SmokeInstance> _smokeInstances;
	void emitSmokeInstances();
	void drawSmokeGLPass();

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
	};
	struct CursorOverlayInstance
	{
		int screenX, screenY;
		SurfaceSet* set;
		int frameIdx;
		CursorStyle style;
		float extraData = 0.0f;  // CS_AP_RING: arcFraction [0..1]
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
	};
	std::vector<UnitAtlasGroup> _unitAtlasGroups;
	void emitUnitPass();
	void drawUnitGLPass();
	/// Draw unit instances at the given (Z, Y) row. activeShader is in/out so
	/// we don't rebind/reupload uniforms when called repeatedly within one
	/// drawTileGLPass loop.
	void drawUnitsAtZY(int z, int y, Shader*& activeShader);
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
