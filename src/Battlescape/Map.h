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
	SurfaceSet *_projectileSet;

	void drawUnit(UnitSprite &unitSprite, Tile *unitTile, Tile *currTile, Position tileScreenPosition, bool topLayer, BattleUnit* movingUnit = nullptr);
	void drawTerrainCPU(Surface *surface);
#ifdef __EMSCRIPTEN__
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
	};

	/// Per-atlas draw group: one glDrawArraysInstanced per atlas texture.
	struct AtlasGroup
	{
		GpuTexture*               atlas    = nullptr;
		float                     tileUVW  = 0.0f;
		float                     tileUVH  = 0.0f;
		bool                      isRgba   = false;
		std::vector<TileInstance> instances;
		// Phase 13.1: per-Z range descriptors into instances[].
		struct ZSlice { int zLevel; size_t first; size_t count; };
		std::vector<ZSlice>       zSlices;

		const ZSlice* findZSlice(int z) const {
			for (const auto& s : zSlices) if (s.zLevel == z) return &s;
			return nullptr;
		}
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

	/// Block 11.10: tile-space cursor-box overlay instance (CURSOR.PCK sprites).
	struct CursorOverlayInstance
	{
		int screenX, screenY;
		SurfaceSet* set;
		int frameIdx;
	};
	std::vector<CursorOverlayInstance> _cursorOverlayInstances;
	void drawCursorOverlayGLPass();
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
