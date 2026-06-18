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
#include "Map.h"
#include "Camera.h"
#include "UnitSprite.h"
#include "ItemSprite.h"
#include "Pathfinding.h"
#include "BattlescapeGame.h"
#include "TileEngine.h"
#include "Projectile.h"
#include "Explosion.h"
#include "BattlescapeState.h"
#include "Particle.h"
#include "../Interface/Cursor.h"
#include "../Mod/Mod.h"
#include "../Engine/TTFFont.h"
#include "../Engine/Action.h"
#include "../Engine/SurfaceSet.h"
#include "../Engine/Timer.h"
#include "../Engine/Language.h"
#include "../Engine/Palette.h"
#include "../Engine/Game.h"
#include "../Engine/Screen.h"
#include "../Engine/ShaderDraw.h"
#include "../Engine/ShaderMove.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/Tile.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/BattleItem.h"
#include "../Ufopaedia/Ufopaedia.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleInterface.h"
#include "../Mod/MapDataSet.h"
#include "../Mod/MapData.h"
#include "../Mod/Armor.h"
#include "../Mod/RuleEnviroEffects.h"
#include "BattlescapeMessage.h"
#include "../Savegame/SavedGame.h"
#include "../Interface/NumberText.h"
#include "../Interface/Text.h"
#include "../fmath.h"

#ifdef __EMSCRIPTEN__
#  include "../Engine/GpuTimer.h"
#  include "../Engine/GpuInit.h"
#  include "../Engine/Shader.h"
#  include "../Engine/GpuTexture.h"
#  include "../Engine/ShadeTableCache.h"
#  include "../Engine/RenderTarget.h"
#  include "../Engine/ShaderManager.h"
#  include <GLES3/gl3.h>
#  include <set>
#  include <vector>
#  include <cstddef>   // offsetof — instance-layout static_asserts in initTileGL
/* Phase 11.0 CPU perf gate; Phase 11.1 readback-cost probe gate.
 * Definitions live in EmscriptenHarness.cpp inside extern "C" {},
 * so forward-declarations must carry C linkage (global namespace). */
extern "C" int g_calypsoProfileBattlescape;
extern "C" int g_calypsoProfileReadback;
/* Phase-14 railings debug: one-shot tile dump flag.
 * Set to 1 by Module._calypso_dump_emit_once() before forcing a redraw;
 * emitTilePass() and Map::draw() painter pass each log every visible tile
 * and reset the flag, so production runs at zero cost. */
extern "C" int g_calypsoDumpEmit;
#endif /* __EMSCRIPTEN__ */


/*
  1) Map origin is top corner.
  2) X axis goes downright. (width of the map)
  3) Y axis goes downleft. (length of the map
  4) Z axis goes up (height of the map)

           0,0
            /\
           /  \
        y+ \  / x+
            \/

  Compass directions

         W  /\  N
           /  \
           \  /
         S  \/  E

  Unit directions

         6  /\  0
           /  \
           \  /
         4  \/  2

  Big units parts

            /\
           /0 \
          /\  /\
         /2 \/1 \
         \  /\  /
          \/3 \/
           \  /
            \/
 */

namespace OpenXcom
{

#ifdef __EMSCRIPTEN__
namespace {

/* GL state save/restore used by the readback probe. */
struct GlStateSave
{
    GLint prog = 0, vao = 0; GLboolean blend = GL_FALSE, depth = GL_FALSE;
    void save()
    {
        glGetIntegerv(GL_CURRENT_PROGRAM,      &prog);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        blend = glIsEnabled(GL_BLEND);
        depth = glIsEnabled(GL_DEPTH_TEST);
    }
    void restore()
    {
        glUseProgram((GLuint)prog);
        glBindVertexArray((GLuint)vao);
        if (blend) glEnable(GL_BLEND);     else glDisable(GL_BLEND);
        if (depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    }
};

/* ── Phase 11.1: FBO solid-colour + glReadPixels cost probe ────────────
 * Measures the minimum readback stall for Option-A GPU compositor at the
 * actual Battlescape surface size.  Activated by calypso_set_profile_readback(1).
 * Self-terminates after PROBE_FRAMES samples and logs the average. */
struct ReadbackProbe
{
    Shader       shader;
    RenderTarget fbo;
    GLuint       vao = 0u;
    bool         ready = false;
    bool         done  = false;
    int          w = 0, h = 0;
    std::vector<uint8_t> pixels;

    long long accumUs    = 0;
    unsigned  frameCount = 0;
    static constexpr unsigned PROBE_FRAMES = 30u;

    bool init(int surfW, int surfH)
    {
        if (!GpuInit::ready()) return false;
        if (!shader.loadFromEmbedded("colorquad")) return false;
        if (!fbo.create(surfW, surfH)) return false;
        float verts[] = {
            -1.f,-1.f, 0.f,0.f,  1.f,-1.f, 1.f,0.f,  -1.f, 1.f, 0.f,1.f,
            -1.f, 1.f, 0.f,1.f,  1.f,-1.f, 1.f,0.f,   1.f, 1.f, 1.f,1.f,
        };
        GLuint vbo;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0u);
        w = surfW; h = surfH;
        pixels.resize((size_t)w * h * 4);
        ready = true;
        Log(LOG_INFO) << "Map::readbackProbe: init at " << w << "x" << h;
        return true;
    }

    void probe()
    {
        if (done || !ready) return;
        GlStateSave st; st.save();
        GpuTimer t;

        t.start();
        fbo.bind();
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.2f, 0.4f, 0.8f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        shader.use();
        shader.setUniform4f("u_color", 0.2f, 0.4f, 0.8f, 1.f);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        t.stop();
        fbo.unbind();
        st.restore();

        accumUs += t.elapsedUs();
        if (++frameCount >= PROBE_FRAMES)
        {
            Log(LOG_INFO) << "Map::readbackProbe avg: "
                << (accumUs / (long long)frameCount) << " us/frame"
                << " (" << w << "x" << h << ", n=" << frameCount
                << ", FBO render + glReadPixels)";
            done = true;
        }
    }
};

static ReadbackProbe s_readbackProbe;

} // anonymous namespace
#endif /* __EMSCRIPTEN__ */

/**
 * Sets up a map with the specified size and position.
 * @param game Pointer to the core game.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 * @param visibleMapHeight Current visible map height.
 */
Map::Map(Game *game, int width, int height, int x, int y, int visibleMapHeight) : InteractiveSurface(width, height, x, y),
	_game(game), _isTFTD(false), _arrow(0), _anyIndicator(false), _isAltPressed(false), _isCtrlPressed(false),
	_selectorX(0), _selectorY(0), _mouseX(0), _mouseY(0), _cursorType(CT_NORMAL), _cursorSize(1), _animFrame(0),
	_projectile(0), _followProjectile(true), _projectileInFOV(false), _explosionInFOV(false), _launch(false), _visibleMapHeight(visibleMapHeight),
	_unitDying(false), _smoothingEngaged(false), _flashScreen(false), _bgColor(15), _projectileSet(0), _fontHdNumbers(nullptr), _showObstacles(false), _showInfoOnCursor(false)
{
	// TODO: extract to a better place later
	for (const auto& pair : Options::mods)
	{
		if (pair.second)
		{
			if (pair.first == "xcom2")
			{
				_isTFTD = true;
				break;
			}
		}
	}

	_iconHeight = _game->getMod()->getInterface("battlescape")->getElement("icons")->h;
	_iconWidth = _game->getMod()->getInterface("battlescape")->getElement("icons")->w;
	_messageColor = _game->getMod()->getInterface("battlescape")->getElement("messageWindows")->color;

	auto* itf = _game->getMod()->getInterface("battlescape")->getElement("thinkingProgressBar");
	_hostileBarColor = itf->color;
	_neutralBarColor = itf->color2;
	_borderBarColor = itf->border;

	PathPreview previewSetting = Options::battleNewPreviewPath;
	_smoothCamera = Options::battleSmoothCamera;
	if (Options::traceAI)
	{
		// turn everything on because we want to see the markers.
		previewSetting = PATH_ARROW_TU;
	}
	_previewSettingArrows = previewSetting & PATH_ARROWS;
	_previewSettingTu     = previewSetting & PATH_TU_COST;
	_previewSettingEnergy = previewSetting & PATH_ENERGY_COST;

	_save = _game->getSavedGame()->getSavedBattle();
	if ((int)(_game->getMod()->getLUTs()->size()) > _save->getDepth())
	{
		_transparencies = &_game->getMod()->getLUTs()->at(_save->getDepth());
	}
	else
	{
		const static std::vector<Uint8> dummy;
		_transparencies = &dummy;
	}

	_spriteWidth = _game->getMod()->getSurfaceSet("BLANKS.PCK")->getFrame(0)->getWidth();
	_spriteHeight = _game->getMod()->getSurfaceSet("BLANKS.PCK")->getFrame(0)->getHeight();
	_message = new BattlescapeMessage(320, (visibleMapHeight < 200)? visibleMapHeight : 200, 0, 0);
	_message->setX(_game->getScreen()->getDX());
	_message->setY((visibleMapHeight - _message->getHeight()) / 2);
	_message->setTextColor(_messageColor);
	_camera = new Camera(_spriteWidth, _spriteHeight, _save->getMapSizeX(), _save->getMapSizeY(), _save->getMapSizeZ(), this, visibleMapHeight);
	_scrollMouseTimer = new Timer(SCROLL_INTERVAL);
	_scrollMouseTimer->onTimer((SurfaceHandler)&Map::scrollMouse);
	_scrollKeyTimer = new Timer(SCROLL_INTERVAL);
	_scrollKeyTimer->onTimer((SurfaceHandler)&Map::scrollKey);
	_camera->setScrollTimer(_scrollMouseTimer, _scrollKeyTimer);
	_obstacleTimer = new Timer(2500);
	_obstacleTimer->stop();
	_obstacleTimer->onTimer((SurfaceHandler)&Map::disableObstacles);

	_showInfoOnCursor = (Options::oxceShowAccuracyOnCrosshair == 1 && Options::battleUFOExtenderAccuracy) || Options::oxceShowAccuracyOnCrosshair == 2;
	_txtAccuracy = new Text(44, 18, 0, 0);
	_txtAccuracy->setSmall();
	_txtAccuracy->setPalette(_game->getScreen()->getPalette());
	_txtAccuracy->setHighContrast(true);
	_txtAccuracy->initText(_game->getMod()->getFont("FONT_BIG"), _game->getMod()->getFont("FONT_SMALL"), _game->getLanguage());

	_txtUnitAP = new Text(44, 20, 0, 0);
	_txtUnitAP->setBig();
	_txtUnitAP->setPalette(_game->getScreen()->getPalette());
	_txtUnitAP->setHighContrast(true);
	_txtUnitAP->initText(_game->getMod()->getFont("FONT_BIG"), _game->getMod()->getFont("FONT_SMALL"), _game->getLanguage());
	_txtUnitAP->setAlign(ALIGN_CENTER);

	_txtCursorAP = new Text(44, 20, 0, 0);
	_txtCursorAP->setBig();
	_txtCursorAP->setPalette(_game->getScreen()->getPalette());
	_txtCursorAP->setHighContrast(true);
	_txtCursorAP->initText(_game->getMod()->getFont("FONT_BIG"), _game->getMod()->getFont("FONT_SMALL"), _game->getLanguage());
	_txtCursorAP->setAlign(ALIGN_CENTER);

	// Phase 16: HD cursor-numeral font is resolved lazily via getHdNumberFont()
	// (see member-initializer at top of ctor).  Capturing it here would race
	// with mod registration on first save-load.

	_hoveredTU = -1;
	_cacheActiveWeaponUfopediaArticleUnlocked = -1;
	_cacheIsCtrlPressed = false;
	_cacheCursorPosition = TileEngine::invalid;
	_cacheHasLOS = -1;

	_nightVisionOn = false;
	if (Options::oxceToggleNightVisionType == 2)
	{
		// persisted per campaign
		_nightVisionOn = _game->getSavedGame()->getToggleNightVision();
	}
	else if (Options::oxceToggleNightVisionType == 1)
	{
		// persisted per battle
		_nightVisionOn = _save->getToggleNightVision();
	}

	_debugVisionMode = 0;
	if (Options::oxceToggleBrightnessType == 2)
	{
		// persisted per campaign
		_debugVisionMode = _game->getSavedGame()->getToggleBrightness();
	}
	else if (Options::oxceToggleBrightnessType == 1)
	{
		// persisted per battle
		_debugVisionMode = _save->getToggleBrightness();
	}

	_save->setToggleNightVisionTemp(false);
	_save->setToggleNightVisionColorTemp(0);
	_save->setToggleBrightnessTemp(_debugVisionMode);

	_fadeShade = 16;
	_nvColor = 0;
	_fadeTimer = new Timer(FADE_INTERVAL);
	_fadeTimer->onTimer((SurfaceHandler)&Map::fadeShade);
	_fadeTimer->start();

	auto* enviro = _save->getEnviroEffects();
	if (enviro)
	{
		_bgColor = enviro->getMapBackgroundColor();
	}

	_stunIndicator = _game->getMod()->getSurface("FloorStunIndicator", false);
	_woundIndicator = _game->getMod()->getSurface("FloorWoundIndicator", false);
	_burnIndicator = _game->getMod()->getSurface("FloorBurnIndicator", false);
	_shockIndicator = _game->getMod()->getSurface("FloorShockIndicator", false);
	_anyIndicator = _stunIndicator || _woundIndicator || _burnIndicator || _shockIndicator;

	if (enviro)
	{
		if (!enviro->getMapShockIndicator().empty())
		{
			_shockIndicator = _game->getMod()->getSurface(enviro->getMapShockIndicator(), false);
		}
	}

	_vaporParticlesInit.resize(_camera->getMapSizeY() * _camera->getMapSizeX());
	_vaporParticles.resize(_camera->getMapSizeY() * _camera->getMapSizeX());
}

/**
 * Deletes the map.
 */
Map::~Map()
{
	delete _scrollMouseTimer;
	delete _scrollKeyTimer;
	delete _fadeTimer;
	delete _obstacleTimer;
	delete _arrow;
	delete _message;
	delete _camera;
	delete _txtAccuracy;
	delete _txtUnitAP;
	delete _txtCursorAP;
#ifdef __EMSCRIPTEN__
	_gpuAliveFlag.reset();
	delete _tileShader;     _tileShader     = nullptr;
	delete _tileShaderRgba; _tileShaderRgba = nullptr;
	delete _blendShader;    _blendShader    = nullptr;  // Phase 22
	delete _shadeTableTex;  _shadeTableTex  = nullptr;
	delete _shadeCurveTex;  _shadeCurveTex  = nullptr;  // Phase 22
	delete _noiseTex;       _noiseTex       = nullptr;   // Phase 22
	if (_tileGLInit)
	{
		glDeleteBuffers(1, &_tileVBO);
		glDeleteBuffers(1, &_tileIBO);
		glDeleteVertexArrays(1, &_tileVAO);
		if (_blendVAO) { glDeleteVertexArrays(1, &_blendVAO); _blendVAO = 0; }
		if (_blendIBO) { glDeleteBuffers(1,      &_blendIBO); _blendIBO = 0; }
		if (_tileInstVAO) { glDeleteVertexArrays(1, &_tileInstVAO); _tileInstVAO = 0; }  // Phase 22 (H1)
		if (_tileInstIBO) { glDeleteBuffers(1,      &_tileInstIBO); _tileInstIBO = 0; }  // Phase 22 (H1)
	}
	delete _spriteShader; _spriteShader = nullptr;
	for (auto& p : _spriteFrameCache) delete p.second;
	_spriteFrameCache.clear();
	if (_spriteVAO) { glDeleteVertexArrays(1, &_spriteVAO); _spriteVAO = 0; }
	if (_spriteVBO) { glDeleteBuffers(1, &_spriteVBO);      _spriteVBO = 0; }
#endif
}

/**
 * Initializes the map.
 */
void Map::init()
{
	// load the tiny arrow into a surface
	int f = Palette::blockOffset(1);     // dark yellow
	int g = Palette::blockOffset(1) + 3; // bright yellow (center highlight)
	int b = 15; // black
	int pixels[81] = { 0, 0, b, b, b, b, b, 0, 0,
	                   0, 0, b, f, g, f, b, 0, 0,
	                   0, 0, b, f, g, f, b, 0, 0,
	                   b, b, b, f, g, f, b, b, b,
	                   b, f, f, f, g, f, f, f, b,
	                   0, b, f, f, g, f, f, b, 0,
	                   0, 0, b, f, g, f, b, 0, 0,
	                   0, 0, 0, b, g, b, 0, 0, 0,
	                   0, 0, 0, 0, b, 0, 0, 0, 0 };

	_arrow = new Surface(9, 9);
	_arrow->setPalette(this->getPalette());
	_arrow->lock();
	for (int y = 0; y < 9;++y)
		for (int x = 0; x < 9; ++x)
			_arrow->setPixel(x, y, pixels[x+(y*9)]);
	_arrow->unlock();

	_projectile = 0;
	if (_save->getDepth() == 0)
	{
		_projectileSet = _game->getMod()->getSurfaceSet("Projectiles");
	}
	else
	{
		_projectileSet = _game->getMod()->getSurfaceSet("UnderwaterProjectiles");
	}
#ifdef __EMSCRIPTEN__
	if (_game->getMod()->hasHDPack() && GpuInit::ready())
	{
		_gpuAliveFlag = std::make_shared<bool>(true);
		std::weak_ptr<bool> wf = _gpuAliveFlag;
		// Phase 13.3: tile pass fires BEFORE SDL composite so HD floor renders
		// under CPU-drawn units / walls / HUD. Tile pass internally interleaves
		// unit draws via drawUnitsAtZ(z) between Z slices for correct
		// occlusion (e.g. submarine roof above units inside the cargo bay).
		_game->getScreen()->registerGPUPassPreComposite([this, wf]() {
			if (!wf.lock()) return;
			this->drawTileGLPass();
		});
		// Block 11.10: tile-space cursor overlay after tile pass, before sprites.
		_game->getScreen()->registerGPUPass([this, wf]() {
			if (!wf.lock()) return;
			this->drawCursorOverlayGLPass();
		});
		// Block 11.8: projectile pass fires after cursor overlay.
		_game->getScreen()->registerGPUPass([this, wf]() {
			if (!wf.lock()) return;
			this->drawProjectileGLPass();
		});
		// Block 11.9: smoke/explosion pass fires after projectile pass
		// (cursor registered from BattlescapeState ctor last, so order is:
		//  tiles → cursor overlay → projectiles → smoke → cursor).
		_game->getScreen()->registerGPUPass([this, wf]() {
			if (!wf.lock()) return;
			this->drawSmokeGLPass();
		});

		// Block 11.13: after context restore, zero stale VAO/VBO handles and
		// reset init flags so the next draw call recreates them via initTileGL /
		// initSpriteGL (shader C++ objects are rebuilt by ShaderManager::reuploadAll).
		// Phase 14.1: also drop unit atlas groups and delete their GpuTextures so
		// drawUnitGLPass() is a no-op until Map::setPalette() rebuilds them.
		ShaderManager::instance().registerResetCallback(_gpuAliveFlag, [this]() {
			_tileVAO = _tileVBO = _tileIBO = 0;
			_blendVAO = _blendIBO = 0;
			_tileInstVAO = _tileInstIBO = 0;   // Phase 22 (H1): recreated by initTileGL
			_tileBuffersDirty = true;          // force re-upload after context restore
			_tileGLInit = false;
			_spriteVAO = _spriteVBO = 0;
			_spriteGLInit = false;
			_cursorVAO = _cursorVBO = _cursorInstanceVBO = 0;
			_cursorGLInit = false;
			_unitAtlasGroups.clear();
			_game->getMod()->clearUnitAtlases();
		});
	}
#endif
}

/**
 * Keeps the animation timers running.
 */
void Map::think()
{
	_scrollMouseTimer->think(0, this);
	_scrollKeyTimer->think(0, this);
	_fadeTimer->think(0, this);
	_obstacleTimer->think(0, this);
}

/**
 * Update visibility timestamp and blit.
 * @param surface The surface to draw on.
 */
void Map::blit(SDL_Surface *surface)
{
	_lastDrawnTicks = SDL_GetTicks();
	InteractiveSurface::blit(surface);
}

/**
 * Draws the whole map, part by part.
 */
void Map::draw()
{
	if (!_redraw)
	{
		return;
	}

	// normally we'd call for a Surface::draw();
	// but we don't want to clear the background with colour 0, which is transparent (aka black)
	// we use colour 15 because that actually corresponds to the colour we DO want in all variations of the xcom and tftd palettes.
	// Note: un-hardcoded the color from 15 to ruleset value, default 15
	_redraw = false;
	{
#ifdef __EMSCRIPTEN__
		// Phase 13.4: in HD mode the GPU tile pass draws floor before the SDL
		// composite; make the surface fully transparent so floor pixels show through.
		const bool hdSurfaceBg = _game->getMod()->hasHDPack() && GpuInit::ready();
#else
		const bool hdSurfaceBg = false;
#endif
		if (hdSurfaceBg)
		{
			SDL_FillRect(getSurface(), nullptr,
			             SDL_MapRGBA(getSurface()->format, 0, 0, 0, 0));
		}
		else
		{
			const SDL_Color *pal = getEffectivePalette();
			Uint8 idx = (Uint8)(Palette::blockOffset(0) + _bgColor);
			if (pal)
			{
				const SDL_Color &c = pal[idx];
				SDL_FillRect(getSurface(), nullptr, SDL_MapRGB(getSurface()->format, c.r, c.g, c.b));
			}
			else
			{
				SDL_FillRect(getSurface(), nullptr, 0xFF000000u);
			}
		}
	}

	Tile *t;

	_projectileInFOV = _save->getDebugMode();
	if (_projectile)
	{
		t = _save->getTile(_projectile->getPosition(0).toTile());
		if (_save->getSide() == FACTION_PLAYER || (t && t->getVisible()))
		{
			_projectileInFOV = true;
		}
	}
	_explosionInFOV = _save->getDebugMode();
	if (!_explosions.empty())
	{
		for (auto* explosion : _explosions)
		{
			if (explosion->isBig())
			{
				_explosionInFOV = true;
				break;
			}
			t = _save->getTile(explosion->getPosition().toTile());
			if (t && t->getVisible())
			{
				_explosionInFOV = true;
				break;
			}
		}
	}

	const bool drawCond = (_save->getSelectedUnit() && _save->getSelectedUnit()->getVisible()) || _unitDying || _save->getSide() == FACTION_PLAYER || _save->getDebugMode() || _projectileInFOV || _explosionInFOV;
#ifdef __EMSCRIPTEN__
	if (g_calypsoDumpEmit) {
		Log(LOG_INFO) << "[DIAG-DRAW] drawCond=" << (drawCond?1:0)
		              << " selUnit=" << (_save->getSelectedUnit()?1:0)
		              << " selVis=" << (_save->getSelectedUnit() && _save->getSelectedUnit()->getVisible() ? 1 : 0)
		              << " unitDying=" << (_unitDying?1:0)
		              << " side=" << (int)_save->getSide()
		              << " debug=" << (_save->getDebugMode()?1:0)
		              << " projInFOV=" << (_projectileInFOV?1:0)
		              << " explInFOV=" << (_explosionInFOV?1:0)
		              << " hasHDPack=" << (_game->getMod()->hasHDPack()?1:0);
	}
#endif
	if (drawCond)
	{
#ifdef __EMSCRIPTEN__
		if (_game->getMod()->hasHDPack())
			drawTerrainGPU(this);
		else
#endif
			drawTerrainOverlayCPU(this);
	}
	else
	{
#ifdef __EMSCRIPTEN__
		// Block 11.11: GPU tile pass fires every frame from registered lambdas.
		// Clear stale instances so tiles don't overdraw the hidden-movement screen.
		if (_game->getMod()->hasHDPack() && GpuInit::ready())
		{
			for (auto& grp : _tileAtlasGroups)
			{
				grp.instances.clear();
				grp.zSlices.clear();
				grp.overlayInstances.clear();
				grp.blendInstances.clear();  // Phase 22
			}
			_cursorOverlayInstances.clear();
			_smokeInstances.clear();
			// Phase 22 (H1): terrain lists emptied — invalidate the persistent buffer so
			// the dirty-gate invariant holds (no stale offsets/data can be re-drawn).
			_tileBuffersDirty = true;
		}
#endif
		_message->blit(this->getSurface());
	}
#ifdef __EMSCRIPTEN__
	if (g_calypsoDumpEmit)
	{
		Log(LOG_INFO) << "[DUMP-EMIT] === end frame; resetting flag ===";
		g_calypsoDumpEmit = 0;
	}
#endif
}

void Map::refreshAIProgress(int progress)
{
	if (_save->getSide() == FACTION_NEUTRAL)
	{
		_message->setProgressBarColor(_neutralBarColor, _borderBarColor);
	}
	else
	{
		_message->setProgressBarColor(_hostileBarColor, _borderBarColor);
	}
	_message->setProgressValue(progress);
}

/**
 * Replaces a certain amount of colors in the surface's palette.
 * @param colors Pointer to the set of colors.
 * @param firstcolor Offset of the first color to replace.
 * @param ncolors Amount of colors to replace.
 */
void Map::setPalette(const SDL_Color *colors, int firstcolor, int ncolors)
{
	Surface::setPalette(colors, firstcolor, ncolors);
	for (auto* mds : *_save->getMapDataSets())
	{
		mds->getSurfaceset()->setPalette(colors, firstcolor, ncolors);
	}
	_message->setPalette(colors, firstcolor, ncolors);
	refreshHiddenMovementBackground();
	_message->initText(_game->getMod()->getFont("FONT_BIG"), _game->getMod()->getFont("FONT_SMALL"), _game->getLanguage());
	_message->setText(_game->getLanguage()->getString("STR_HIDDEN_MOVEMENT"), _game->getLanguage()->getString("STR_THINKING"));
#ifdef __EMSCRIPTEN__
	{
		const bool hd  = _game->getMod()->hasHDPack();
		const bool gpu = GpuInit::ready();
		const size_t mdsN = _save->getMapDataSets()->size();
		Log(LOG_INFO) << "[DIAG-SETPALETTE] hasHDPack=" << (hd?1:0)
		              << " GpuInit::ready=" << (gpu?1:0)
		              << " mapDataSets=" << mdsN
		              << " enter_hybrid=" << ((hd&&gpu)?1:0);
	}
	if (_game->getMod()->hasHDPack() && GpuInit::ready())
	{
		Mod* mod = _game->getMod();
		const auto* mdsVec = _save->getMapDataSets();
		_tileAtlasGroups.clear();
		_tileAtlasGroups.resize(mdsVec->size());
		for (size_t i = 0; i < mdsVec->size(); ++i)
		{
			MapDataSet* mds = (*mdsVec)[i];
			mod->ensureVanillaAtlas(mds, colors, ncolors);
			GpuTexture* atlas = mod->getTileAtlas(mds->getName());
			auto* spec = mod->getTileAtlasSpec(mds->getName());
			Log(LOG_INFO) << "[DIAG-SETPALETTE] mds[" << i << "]=" << mds->getName()
			              << " atlas=" << (atlas?1:0) << " spec=" << (spec?1:0)
			              << " hybrid=" << (spec && spec->hybrid ? 1 : 0)
			              << " overlayAtlas=" << (spec && spec->overlayAtlas ? 1 : 0);
			const bool baselineNone = spec && spec->baseline == Mod::BaselineMode::None;
			if (!atlas && !baselineNone) continue;
			if (!spec) continue;
			_tileAtlasGroups[i].atlas        = atlas; // nullptr for baseline:none (draw skipped)
			_tileAtlasGroups[i].tileUVW      = (float)spec->tileWidth  / (float)spec->width;
			_tileAtlasGroups[i].tileUVH      = (float)spec->tileHeight / (float)spec->height;
			_tileAtlasGroups[i].isRgba       = !spec->hybrid &&
			                                   (spec->format == Mod::TileAtlasSpec::Format::Rgba);
			// Phase 17: hybrid overlay atlas pointer; Phase 20: also set for baseline:none.
			_tileAtlasGroups[i].overlayAtlas       = (spec->hybrid || baselineNone)
			                                         ? spec->overlayAtlas : nullptr;
			_tileAtlasGroups[i].premultipliedAlpha = spec->premultipliedAlpha;
			// Phase 20.5: propagate sub-layer atlas pointers from spec.
			_tileAtlasGroups[i].subLayerAtlases    = spec->subLayerAtlases;
			_tileAtlasGroups[i].subLayerInstances.assign(spec->subLayerAtlases.size(), {});
		}
		Log(LOG_INFO) << "[DIAG-SETPALETTE] _tileAtlasGroups populated, size=" << _tileAtlasGroups.size();
		_tileBuffersDirty = true;  // Phase 22 (H1): groups rebuilt — invalidate the terrain instance buffer
		// Force setPalette on each sheet: unit PCKs aren't in ensureBattlescapeAssetPalettes'
		// list, so without this they stay 8bpp scratch with no palette mirror and the atlas
		// builder reads B-channel of unpopulated ARGB instead of palette indices.
		// PCK→ARGB promotion populates _paletteMirror; direct-loaded HD ARGB does not —
		// so getPaletteMirror() distinguishes the two after setPalette runs.
		{
			mod->clearUnitAtlases();
			std::set<std::string> built;
			auto buildIfPalette = [&](SurfaceSet* ss, const std::string& sheet) {
				if (!ss || ss->getTotalFrames() == 0 || !ss->getFrame(0)) return;
				ss->setPalette(colors, firstcolor, ncolors);
				if (!ss->getFrame(0)->getPaletteMirror()) return;  // HD ARGB — skip R8 atlas.
				mod->ensureUnitAtlas(ss, sheet, colors, ncolors);
			};
			for (const auto& armorName : mod->getArmorsList())
			{
				const Armor* armor = mod->getArmor(armorName);
				if (!armor) continue;
				const std::string& sheet = armor->getSpriteSheet();
				if (!built.insert(sheet).second) continue;
				buildIfPalette(mod->getSurfaceSet(sheet, false), sheet);
			}
			// Item hand sprites (HANDOB.PCK) also use R8 atlas.
			buildIfPalette(mod->getSurfaceSet("HANDOB.PCK", false), "HANDOB.PCK");
			// Floor item sprites (FLOOROB.PCK) — emitted via ItemSprite at item's
			// tile Z so they render between walls and units in iso order.
			buildIfPalette(mod->getSurfaceSet("FLOOROB.PCK", false), "FLOOROB.PCK");

			// Global Battlescape assets used by GPU sprite passes (Phase 11.8).
			// Must have a valid palette before getOrUploadSpriteFrame builds RGBA textures.
			buildIfPalette(mod->getSurfaceSet("Projectiles", false), "Projectiles");
			buildIfPalette(mod->getSurfaceSet("UnderwaterProjectiles", false), "UnderwaterProjectiles");
			buildIfPalette(mod->getSurfaceSet("CURSOR.PCK", false), "CURSOR.PCK");
			buildIfPalette(mod->getSurfaceSet("SMOKE.PCK", false), "SMOKE.PCK");
			buildIfPalette(mod->getSurfaceSet("HIT.PCK", false), "HIT.PCK");
			buildIfPalette(mod->getSurfaceSet("X1.PCK", false), "X1.PCK");
			buildIfPalette(mod->getSurfaceSet("Pathfinding", false), "Pathfinding");
		}
		// Invalidate sprite frame cache — palette mapping changed.
		for (auto& p : _spriteFrameCache) delete p.second;
		_spriteFrameCache.clear();
		delete _shadeTableTex; _shadeTableTex = nullptr;
		const ShadeTable* st = getShadeTable();
		// Phase 22 §22.4: 16×1 R8 shade-curve ramp for tile_blend + tile_atlas_rgba.
		// Encodes average luminance ratio lum(s) / lum(0) over palette indices 1..255
		// so the fragment shaders darken blended/HD surfaces identically to the
		// vanilla tile path (prevents night seams, DoD #10).
		// M1: ALWAYS (re)build _shadeCurveTex — defaulting to an identity ramp
		// (255 = no darkening) when no shade table is available. The RGBA/blend
		// fragment shaders render black on an unbound u_shadeCurve sampler, so a
		// valid curve must exist unconditionally; identity preserves the pre-
		// Phase-22 "undarkened HD tile" look in the degenerate (no shade table) case.
		std::vector<uint8_t> curve(16, 255u);   // identity fallback (no darkening)
		if (st)
		{
			ShadeTableCache tmp;
			_shadeTableTex = tmp.uploadGPU(st).release();
			for (int s = 0; s < 16; ++s)
			{
				double sum = 0.0; int n = 0;
				for (int idx = 1; idx < 256; ++idx)
				{
					Uint32 c0 = st->get((Uint8)idx, 0), cs = st->get((Uint8)idx, s);
					double l0 = 0.299 * ((c0 >> 16) & 0xFF) + 0.587 * ((c0 >> 8) & 0xFF) + 0.114 * (c0 & 0xFF);
					double ls = 0.299 * ((cs >> 16) & 0xFF) + 0.587 * ((cs >> 8) & 0xFF) + 0.114 * (cs & 0xFF);
					if (l0 > 1.0) { sum += ls / l0; ++n; }
				}
				double r = n ? sum / (double)n : (1.0 - s / 15.0);
				double v = r * 255.0;
				if (v < 0.0) v = 0.0; else if (v > 255.0) v = 255.0;
				curve[s] = (uint8_t)v;
			}
		}
		delete _shadeCurveTex;
		_shadeCurveTex = new GpuTexture(false, GpuTexture::Wrap::ClampToEdge, GpuTexture::Filter::Nearest);
		_shadeCurveTex->uploadR8(curve.data(), 16, 1);
	}
#endif
}

void Map::refreshHiddenMovementBackground()
{
	_message->setBackground(_game->getMod()->getSurface(_save->getHiddenMovementBackground()));
}

/**
 * Get shade of wall.
 * @param part For what wall do calculations.
 * @param tileFrot Tile of wall.
 * @return Current shade of wall.
 */
int Map::getWallShade(TilePart part, Tile* tileFrot)
{
	int shade;
	if (tileFrot->isDiscovered(O_FLOOR))
	{
		shade = reShade(tileFrot);
	}
	else
	{
		shade = 16;
	}
	if (part)
	{
		if ((tileFrot->isDoor(part) || tileFrot->isUfoDoor(part)) && tileFrot->isDiscovered(part))
		{
			Position offset =
				part == O_NORTHWALL ? Position(1,0,0) :
				part == O_WESTWALL ? Position(0,1,0) :
					throw Exception("Unsupported tile part for wall shade");

			Tile *tileBehind = _save->getTile(tileFrot->getPosition() - offset);

			shade = std::min(reShade(tileFrot), tileBehind ? tileBehind->getShade() + 5 : 16);
		}
	}
	return shade;
}

/**
 * Check two positions if have same XY cords
 */
static bool positionHaveSameXY(Position a, Position b)
{
	return a.x == b.x && a.y == b.y;
}

/**
 * Check two positions if have same XY cords
 */
static bool positionInRangeXY(Position a, Position b, int diff)
{
	return std::abs(a.x - b.x) <= diff && std::abs(a.y - b.y) <= diff;
}

namespace
{

// Phase 22 §22.5: integer hash for anti-repeat variant selection.
// Maps a 3-D grid position to a pseudo-random uint32 via nested Murmur-style mix.
static uint32_t hash3(int x, int y, int z)
{
    auto h = [](uint32_t v) -> uint32_t {
        v ^= v >> 16; v *= 0x45d9f3bU; v ^= v >> 16;
        return v;
    };
    return h((uint32_t)x ^ (h((uint32_t)y ^ h((uint32_t)z))));
}

static const int ArrowBobOffsets[8] = {0,1,2,1,0,1,2,1};

static const int ArrowColorsUFO[4]  = { 6,  3, 14, 4 }; // white,    red, blue, green
static const int ArrowColorsTFTD[4] = { 4, 11, 16, 6 }; // white, orange, blue, green

int getArrowBobForFrame(int frame)
{
	return ArrowBobOffsets[frame % 8];
}

int getShadePulseForFrame(int shade, int frame)
{
	if (shade > 7) shade = 7;
	if (shade < 2) shade = 2;
	shade += (ArrowBobOffsets[frame % 8] * 2 - 2);
	return shade;
}

}

/**
 * Draw part of unit graphic that overlap current tile.
 * @param surface
 * @param unitTile
 * @param currTile
 * @param currTileScreenPosition
 * @param shade
 * @param obstacleShade
 * @param topLayer
 */
void Map::drawUnit(UnitSprite &unitSprite, Tile *unitTile, Tile *currTile, Position currTileScreenPosition, bool topLayer, BattleUnit* movingUnit)
{
	const int tileFoorWidth = 32;
	const int tileFoorHeight = 16;
	const int tileHeight = 40;

	if (!unitTile)
	{
		return;
	}
	BattleUnit* bu = unitTile->getOverlappingUnit(_save, TUO_ALWAYS);
	Position unitOffset;
	bool unitFromBelow = false;
	bool unitFromAbove = false;
	if (bu)
	{
		if (bu != unitTile->getUnit())
		{
			unitFromBelow = true;
		}
	}
	else if (movingUnit && unitTile == currTile)
	{
		auto* upperTile = _save->getAboveTile(unitTile);
		if (upperTile && upperTile->hasNoFloor(_save))
		{
			bu = upperTile->getUnit();
		}
		if (bu != movingUnit)
		{
			return;
		}
		unitFromAbove = true;
	}
	else
	{
		return;
	}

	if (!(bu->getVisible() || _save->getDebugMode()))
	{
		return;
	}

	unitOffset.x = unitTile->getPosition().x - bu->getPosition().x;
	unitOffset.y = unitTile->getPosition().y - bu->getPosition().y;
	int part = unitOffset.x + unitOffset.y*2;

	bool moving = bu->getStatus() == STATUS_WALKING || bu->getStatus() == STATUS_FLYING;
	int bonusWidth = moving ? 0 : tileFoorWidth;
	int topMargin = 0;
	int bottomMargin = 0;

	//if unit is from below then we draw only part that in in tile
	if (unitFromBelow)
	{
		bottomMargin = -tileFoorHeight / 2;
		topMargin = tileFoorHeight;
	}
	else if (topLayer)
	{
		topMargin = 2 * tileFoorHeight;
	}
	else
	{
		const Tile *top = _save->getAboveTile(unitTile);
		if (top && top->getOverlappingUnit(_save, TUO_ALWAYS) == bu)
		{
			topMargin = -tileFoorHeight / 2;
		}
		else
		{
			topMargin = tileFoorHeight;
		}
	}

	GraphSubset mask = GraphSubset(tileFoorWidth + bonusWidth, tileHeight + topMargin + bottomMargin).offset(currTileScreenPosition.x - bonusWidth / 2, currTileScreenPosition.y - topMargin);

	if (moving)
	{
		GraphSubset leftMask = mask.offset(-tileFoorWidth/2, 0);
		GraphSubset rightMask = mask.offset(+tileFoorWidth/2, 0);
		int direction = bu->getDirection();
		Position partCurr = currTile->getPosition();
		Position partDest = bu->getDestination() + unitOffset;
		Position partLast = bu->getLastPosition() + unitOffset;
		bool isTileDestPos = positionHaveSameXY(partDest, partCurr);
		bool isTileLastPos = positionHaveSameXY(partLast, partCurr);

		if (unitFromAbove && partLast != unitTile->getPosition())
		{
			//this tile is below moving unit and it do not change levels, nothing to draw
			return;
		}

		//adjusting mask
		if (positionHaveSameXY(partLast, partDest))
		{
			if (currTile == unitTile)
			{
				//no change
			}
			else
			{
				//nothing to draw
				return;
			}
		}
		else if (isTileDestPos)
		{
			//unit is moving to this tile
			switch (direction)
			{
			case 0:
			case 1:
				mask = GraphSubset::intersection(mask, rightMask);
				break;
			case 2:
				//no change
				break;
			case 3:
				//no change
				break;
			case 4:
				//no change
				break;
			case 5:
			case 6:
				mask = GraphSubset::intersection(mask, leftMask);
				break;
			case 7:
				//nothing to draw
				return;
			}
		}
		else if (isTileLastPos)
		{
			//unit is exiting this tile
			switch (direction)
			{
			case 0:
				//no change
				break;
			case 1:
			case 2:
				mask = GraphSubset::intersection(mask, leftMask);
				break;
			case 3:
				//nothing to draw
				return;
			case 4:
			case 5:
				mask = GraphSubset::intersection(mask, rightMask);
				break;
			case 6:
				//no change
				break;
			case 7:
				//no change
				break;
			}
		}
		else
		{
			Position leftPos = partCurr + Position(-1, 0, 0);
			Position rightPos = partCurr + Position(0, -1, 0);
			if (!topLayer && (partDest.z > partCurr.z || partLast.z > partCurr.z))
			{
				//unit change layers, it will be drawn by upper layer not lower.
				return;
			}
			else if (
				(direction == 1 && (partDest == rightPos || partLast == leftPos)) ||
				(direction == 5 && (partDest == leftPos || partLast == rightPos)))
			{
				mask = GraphSubset(tileFoorWidth, tileHeight + 2 * tileFoorHeight).offset(currTileScreenPosition.x, currTileScreenPosition.y - 2 * tileFoorHeight);
			}
			else
			{
				//unit is not moving close to tile
				return;
			}
		}
	}
	else if (unitTile != currTile || unitFromAbove)
	{
		return;
	}

	Position tileScreenPosition;
	_camera->convertMapToScreen(unitTile->getPosition() + Position(0,0, (-unitFromBelow) + (+unitFromAbove)), &tileScreenPosition);
	tileScreenPosition += _camera->getMapOffset();

	//get shade helpers
	auto getTileShade = [&](Tile* tile)
	{
		return tile ? (tile->isDiscovered(O_FLOOR) ? reShade(tile) : 16) : 16;
	};
	auto getMixedTileShade = [&](Tile* tile, int heightOffset, bool below)
	{
		int shadeLower = 0;
		int shadeUpper = 0;
		if (below)
		{
			shadeLower = getTileShade(_save->getBelowTile(tile));
			shadeUpper = getTileShade(tile);
		}
		else
		{
			shadeLower = getTileShade(tile);
			shadeUpper = getTileShade(_save->getAboveTile(tile));
		}

		return Interpolate(shadeLower, shadeUpper, -heightOffset, Position::TileZ);
	};

	// draw unit
	int shade = 0;
	UnitWalkingOffset offsets = calculateWalkingOffset(bu);
	if (moving)
	{
		const Position start = bu->getPosition();
		const Position end = bu->getDestination();
		const auto minLevel = std::min(start.z, end.z); // Sint16
		const int startShade = getMixedTileShade(_save->getTile(start), start.z == minLevel ? offsets.TerrainLevelOffset : 0, false);
		const int endShade = getMixedTileShade(_save->getTile(end), end.z == minLevel ? offsets.TerrainLevelOffset : 0, false);
		shade = Interpolate(startShade, endShade, offsets.NormalizedMovePhase, 16);
	}
	else
	{
		shade = getMixedTileShade(currTile, offsets.TerrainLevelOffset, unitFromBelow);
		if (_showObstacles && unitTile->getObstacle(4))
		{
			shade = getShadePulseForFrame(shade, _animFrame);
		}
	}
	if (_debugVisionMode == 1)
	{
		shade = std::min(+NIGHT_VISION_SHADE, shade);
	}
#ifdef __EMSCRIPTEN__
	{
		const bool gpuUnitAvail = _game->getMod()->hasHDPack() && GpuInit::ready();
		const Mod::UnitAtlasSpec* gpuUnitSpec = nullptr;
		if (gpuUnitAvail)
		{
			const std::string& sheetName = bu->getArmor()->getSpriteSheet();
			gpuUnitSpec = _game->getMod()->getUnitAtlas(sheetName);
		}
		if (gpuUnitSpec && gpuUnitSpec->atlas)
		{
			// Find or create UnitAtlasGroups; use indices to avoid iterator
			// invalidation if push_back causes a vector reallocation.
			auto ensureGroup = [this](const Mod::UnitAtlasSpec* spec) -> size_t {
				for (size_t i = 0; i < _unitAtlasGroups.size(); ++i)
					if (_unitAtlasGroups[i].spec == spec) return i;
				_unitAtlasGroups.push_back({});
				_unitAtlasGroups.back().spec = spec;
				return _unitAtlasGroups.size() - 1;
			};
			const size_t bodyIdx = ensureGroup(gpuUnitSpec);
			const Mod::UnitAtlasSpec* gpuItemSpec = _game->getMod()->getUnitAtlas("HANDOB.PCK");
			const bool haveItem = gpuItemSpec && gpuItemSpec->atlas;
			const size_t itemIdx = haveItem ? ensureGroup(gpuItemSpec) : _unitAtlasGroups.size();
			// Pass currTile's (Z, Y, X) so the GPU shader can derive an iso
			// priority and use depth-test for correct iso z-ordering between
			// units / items / tiles (no per-cell bucketing needed).
			const int unitZ = currTile ? currTile->getPosition().z : 0;
			const int unitY = currTile ? currTile->getPosition().y : 0;
			const int unitX = currTile ? currTile->getPosition().x : 0;
			unitSprite.setEmitMode(
			    &_unitAtlasGroups[bodyIdx].instances,
			    haveItem ? &_unitAtlasGroups[itemIdx].instances : nullptr,
			    gpuUnitSpec,
			    gpuItemSpec,
			    unitZ, unitY, unitX,
			    &_unitAtlasGroups[bodyIdx].zLevels,
			    haveItem ? &_unitAtlasGroups[itemIdx].zLevels : nullptr,
			    &_unitAtlasGroups[bodyIdx].yLevels,
			    haveItem ? &_unitAtlasGroups[itemIdx].yLevels : nullptr
			);
			unitSprite.draw(bu, part, tileScreenPosition.x + offsets.ScreenOffset.x, tileScreenPosition.y + offsets.ScreenOffset.y, shade, mask, _isAltPressed && !_isCtrlPressed);
			unitSprite.clearEmitMode();
		}
		else
		{
			unitSprite.draw(bu, part, tileScreenPosition.x + offsets.ScreenOffset.x, tileScreenPosition.y + offsets.ScreenOffset.y, shade, mask, _isAltPressed && !_isCtrlPressed);
		}
	}
#else
	unitSprite.draw(bu, part, tileScreenPosition.x + offsets.ScreenOffset.x, tileScreenPosition.y + offsets.ScreenOffset.y, shade, mask, _isAltPressed && !_isCtrlPressed);
#endif
}

/**
 * Draw the terrain overlay into the SDL surface (CPU path).
 * In GPU mode (hasHDPack + GpuInit::ready), floors/walls/units/smoke are already
 * on GPU pre-composite passes; this function only writes the remaining overlay
 * content: front O_OBJECT, items on floor, path arrows, unit arrows, debug
 * overlays, and the flash-screen effect.  In legacy (non-HD) mode it renders
 * everything as before.
 * Keep this function as optimised as possible. It's big to minimise overhead of function calls.
 * @param surface The surface to draw on.
 */
void Map::drawTerrainOverlayCPU(Surface *surface)
{
#ifdef __EMSCRIPTEN__
	// Phase 16: default cursor to visible; suppressed below if walking in map state.
	// We reset it here every frame so that it's visible by default in menus.
	_game->getCursor()->setHidden(false);

	/* Phase 11.0: wall-clock CPU baseline for the full Battlescape render.
	 * Gated on g_calypsoProfileBattlescape — zero overhead in production. */
	const int profileBs = ::g_calypsoProfileBattlescape;
	GpuTimer bsTimer;
	if (profileBs) bsTimer.start();

	// Block 11.10: cursor-box sprites are collected into _cursorOverlayInstances
	// and rendered by drawCursorOverlayGLPass() instead of blitting to SDL surface.
	const bool gpuSpriteMode = _game->getMod()->hasHDPack() && GpuInit::ready();
	SurfaceSet* const gpuCursorSet = gpuSpriteMode
	    ? _game->getMod()->getSurfaceSet("CURSOR.PCK") : nullptr;
	const bool dumpPaint = (g_calypsoDumpEmit != 0);
	if (dumpPaint)
	{
		Log(LOG_INFO) << "[DUMP-PAINT] === begin painter pass; viewLevel="
		              << _camera->getViewLevel() << " ===";
	}
	// Phase 13.4: O_FLOOR is drawn by the GPU tile pass before SDL composite.
	const bool hdFloorMode = gpuSpriteMode;
	// Phase 14.3: O_WESTWALL, O_NORTHWALL, and back-tile O_OBJECT are also drawn
	// by the GPU tile pass (pre-composite, always behind units).  Skip CPU blits.
	const bool hdWallMode = gpuSpriteMode;
#endif

	_isAltPressed = _game->isAltPressed(true);
	_isCtrlPressed = _game->isCtrlPressed(true);
	int frameNumber = 0;
	const Surface* tmpSurface = nullptr;
	Tile *tile;
	int beginX = 0, endX = _save->getMapSizeX() - 1;
	int beginY = 0, endY = _save->getMapSizeY() - 1;
	int beginZ = 0, endZ = _save->getMapSizeZ() - 1;
	Position mapPosition, screenPosition, bulletPositionScreen, movingUnitPosition;
	int bulletLowX=16000, bulletLowY=16000, bulletLowZ=16000, bulletHighX=0, bulletHighY=0, bulletHighZ=0;
	int dummy;
	BattleUnit *movingUnit = _save->getTileEngine()->getMovingUnit();
	int tileShade, tileColor, obstacleShade;
	UnitSprite unitSprite(surface, _game->getMod(), _save, _animFrame, _save->getDepth() != 0,
		_isTFTD ? ArrowColorsTFTD[1] : ArrowColorsUFO[1], _isTFTD ? ArrowColorsTFTD[2] : ArrowColorsUFO[2]);
	ItemSprite itemSprite(surface, _game->getMod(), _save, _animFrame);

	const int halfAnimFrame = (_animFrame / 2) % 4;
	const int halfAnimFrameRest = (_animFrame % 2);

	NumberText *_numWaypid = 0;

	// if we got bullet, get the highest x and y tiles to draw it on
	if (_projectile && _explosions.empty())
	{
		int part = _projectile->getItem() ? 0 : BULLET_SPRITES-1;
		for (int i = 0; i <= part; ++i)
		{
			if (_projectile->getPosition(1-i).x < bulletLowX)
				bulletLowX = _projectile->getPosition(1-i).x;
			if (_projectile->getPosition(1-i).y < bulletLowY)
				bulletLowY = _projectile->getPosition(1-i).y;
			if (_projectile->getPosition(1-i).z < bulletLowZ)
				bulletLowZ = _projectile->getPosition(1-i).z;
			if (_projectile->getPosition(1-i).x > bulletHighX)
				bulletHighX = _projectile->getPosition(1-i).x;
			if (_projectile->getPosition(1-i).y > bulletHighY)
				bulletHighY = _projectile->getPosition(1-i).y;
			if (_projectile->getPosition(1-i).z > bulletHighZ)
				bulletHighZ = _projectile->getPosition(1-i).z;
		}
		// divide by 16 to go from voxel to tile position
		bulletLowX = bulletLowX / 16;
		bulletLowY = bulletLowY / 16;
		bulletLowZ = bulletLowZ / 24;
		bulletHighX = bulletHighX / 16;
		bulletHighY = bulletHighY / 16;
		bulletHighZ = bulletHighZ / 24;

		// if the projectile is outside the viewport - center it back on it
		_camera->convertVoxelToScreen(_projectile->getPosition(), &bulletPositionScreen);

		if (_projectileInFOV && _followProjectile)
		{
			Position newCam = _camera->getMapOffset();
			if (newCam.z != bulletHighZ) //switch level
			{
				newCam.z = bulletHighZ;
				if (_projectileInFOV)
				{
					_camera->setMapOffset(newCam);
					_camera->convertVoxelToScreen(_projectile->getPosition(), &bulletPositionScreen);
				}
			}
			if (_smoothCamera)
			{
				if (_launch)
				{
					_launch = false;
					if ((bulletPositionScreen.x < 1 || bulletPositionScreen.x > surface->getWidth() - 1 ||
						bulletPositionScreen.y < 1 || bulletPositionScreen.y > _visibleMapHeight - 1))
					{
						_camera->centerOnPosition(Position(bulletLowX, bulletLowY, bulletHighZ), false);
						_camera->convertVoxelToScreen(_projectile->getPosition(), &bulletPositionScreen);
					}
				}
				if (!_smoothingEngaged)
				{
					if (bulletPositionScreen.x < 1 || bulletPositionScreen.x > surface->getWidth() - 1 ||
						bulletPositionScreen.y < 1 || bulletPositionScreen.y > _visibleMapHeight - 1)
					{
						_smoothingEngaged = true;
					}
				}
				else
				{
					_camera->jumpXY(surface->getWidth() / 2 - bulletPositionScreen.x, _visibleMapHeight / 2 - bulletPositionScreen.y);
				}
			}
			else
			{
				bool enough;
				do
				{
					enough = true;
					if (bulletPositionScreen.x < 0)
					{
						_camera->jumpXY(+surface->getWidth(), 0);
						enough = false;
					}
					else if (bulletPositionScreen.x > surface->getWidth())
					{
						_camera->jumpXY(-surface->getWidth(), 0);
						enough = false;
					}
					else if (bulletPositionScreen.y < 0)
					{
						_camera->jumpXY(0, +_visibleMapHeight);
						enough = false;
					}
					else if (bulletPositionScreen.y > _visibleMapHeight)
					{
						_camera->jumpXY(0, -_visibleMapHeight);
						enough = false;
					}
					_camera->convertVoxelToScreen(_projectile->getPosition(), &bulletPositionScreen);
				}
				while (!enough);
			}
		}
	}

	// get corner map coordinates to give rough boundaries in which tiles to redraw are
	_camera->convertScreenToMap(0, 0, &beginX, &dummy);
	_camera->convertScreenToMap(surface->getWidth(), 0, &dummy, &beginY);
	_camera->convertScreenToMap(surface->getWidth() + _spriteWidth, surface->getHeight() + _spriteHeight, &endX, &dummy);
	_camera->convertScreenToMap(0, surface->getHeight() + _spriteHeight, &dummy, &endY);
	beginY -= (_camera->getViewLevel() * 2);
	beginX -= (_camera->getViewLevel() * 2);
	if (beginX < 0)
		beginX = 0;
	if (beginY < 0)
		beginY = 0;

	if (!_camera->getShowAllLayers())
	{
		endZ = std::min(endZ, _camera->getViewLevel());
	}
	if (_camera->getShowSingleLayer())
	{
		beginZ = _camera->getViewLevel();
		endZ = _camera->getViewLevel();
	}


	bool pathfinderTurnedOn = _save->getPathfinding()->isPathPreviewed();

	if (!_waypoints.empty() || (pathfinderTurnedOn && (_previewSettingTu || _previewSettingEnergy)))
	{
		_numWaypid = new NumberText(15, 15, 20, 30);
		_numWaypid->setPalette(getPalette());
		_numWaypid->setColor(pathfinderTurnedOn ? _messageColor + 1 : Palette::blockOffset(1));
	}

	if (movingUnit)
	{
		movingUnitPosition = movingUnit->getPosition();
	}

#ifdef __EMSCRIPTEN__
	/* Phase 11.1: readback-cost probe — FBO solid-colour + glReadPixels
	 * at actual surface dimensions.  Runs before CPU lock; GL path is
	 * independent of the SDL_Surface pixel buffer. */
	if (::g_calypsoProfileReadback)
	{
		if (!s_readbackProbe.ready && !s_readbackProbe.done)
			s_readbackProbe.init(surface->getWidth(), surface->getHeight());
		s_readbackProbe.probe();
	}
#endif

	surface->lock();
	const Position cameraPos = _camera->getMapOffset();
	for (int itZ = beginZ; itZ <= endZ; itZ++)
	{
		bool topLayer = itZ == endZ;
		for (int itY = beginY; itY < endY; itY++)
		{
			mapPosition = Position(beginX, itY, itZ);
			tile = _save->getTile(mapPosition);
			for (int itX = beginX; itX < endX; itX++, mapPosition.x++, tile++)
			{
				_camera->convertMapToScreen(mapPosition, &screenPosition);
				screenPosition += cameraPos;

				// only render cells that are inside the surface
				if (screenPosition.x > -_spriteWidth && screenPosition.x < surface->getWidth() + _spriteWidth &&
					screenPosition.y > -_spriteHeight && screenPosition.y < surface->getHeight() + _spriteHeight )
				{
					bool isUnitMovingNearby = movingUnit && positionInRangeXY(movingUnitPosition, mapPosition, 2);

					if (tile->isDiscovered(O_FLOOR))
					{
						tileShade = reShade(tile);
						obstacleShade = tileShade;
						if (_showObstacles)
						{
							if (tile->isObstacle())
							{
								obstacleShade = getShadePulseForFrame(tileShade, _animFrame);
							}
						}
					}
					else
					{
						tileShade = 16;
						obstacleShade = 16;
					}

#ifdef __EMSCRIPTEN__
					if (dumpPaint)
					{
						static const TilePart dumpParts[4] = { O_FLOOR, O_WESTWALL, O_NORTHWALL, O_OBJECT };
						for (int dpi = 0; dpi < 4; ++dpi)
						{
							TilePart dPart = dumpParts[dpi];
							if (!tile->getSprite(dPart)) continue;
							int dMcd = 0, dMds = 0;
							tile->getMapData(&dMcd, &dMds, dPart);
							MapData* dMd = tile->getMapData(dPart);
							int dShade = (dPart == O_WESTWALL || dPart == O_NORTHWALL)
							             ? getWallShade(dPart, tile) : tileShade;
							int dBigW = dMd ? dMd->getBigWall() : -1;
							int dBack = tile->isBackTileObject(dPart) ? 1 : 0;
							int dYOff = tile->getYOffset(dPart);
							const auto* dMdsVec = _save->getMapDataSets();
							std::string dMdsName = (dMds >= 0 && dMds < (int)dMdsVec->size())
							                       ? (*dMdsVec)[dMds]->getName()
							                       : std::string("?");
							Log(LOG_INFO) << "[DUMP-PAINT] z=" << itZ
							              << " y=" << itY
							              << " x=" << mapPosition.x
							              << " part=" << (int)dPart
							              << " bigW=" << dBigW
							              << " back=" << dBack
							              << " yOff=" << dYOff
							              << " shade=" << dShade
							              << " sx=" << screenPosition.x
							              << " sy=" << (screenPosition.y - dYOff)
							              << " mds=" << dMdsName
							              << " mcd=" << dMcd;
						}
					}
#endif

					tileColor = tile->getMarkerColor();

					// Draw floor — skipped in HD mode (GPU tile pass handles floor before SDL composite).
#ifdef __EMSCRIPTEN__
					if (!hdFloorMode)
#endif
					{
						tmpSurface = tile->getSprite(O_FLOOR);
						if (tmpSurface)
						{
							if (tile->getObstacle(O_FLOOR))
								Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y - tile->getYOffset(O_FLOOR), obstacleShade, false, _nvColor);
							else
								Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y - tile->getYOffset(O_FLOOR), tileShade, false, _nvColor);
						}
					}

					auto* unit = tile->getUnit();

					// Draw cursor back
					if (_cursorType != CT_NONE && _selectorX > itX - _cursorSize && _selectorY > itY - _cursorSize && _selectorX < itX+1 && _selectorY < itY+1 && !_save->getBattleState()->getMouseOverIcons())
					{
						if (_camera->getViewLevel() == itZ)
						{
							if (_cursorType != CT_AIM)
							{
#ifdef __EMSCRIPTEN__
								// Phase 15: box cursor uses SDF — emitted once in the front
								// block below.  Skip the back-half push to avoid double draw.
								if (!gpuCursorSet)
#endif
								{
									if (unit && (unit->getVisible() || _save->getDebugMode()))
										frameNumber = halfAnimFrameRest; // yellow box
									else
										frameNumber = 0; // red box
									tmpSurface = _game->getMod()->getSurfaceSet("CURSOR.PCK")->getFrame(frameNumber);
									Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y, 0);
								}
							}
							else
							{
								if (unit && (unit->getVisible() || _save->getDebugMode()))
									frameNumber = 7 + halfAnimFrame; // yellow animated crosshairs
								else
									frameNumber = 6; // red static crosshairs
#ifdef __EMSCRIPTEN__
								if (gpuCursorSet)
									_cursorOverlayInstances.push_back({screenPosition.x, screenPosition.y, gpuCursorSet, frameNumber, CS_RASTER});
								else
#endif
								{
									tmpSurface = _game->getMod()->getSurfaceSet("CURSOR.PCK")->getFrame(frameNumber);
									Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y, 0);
								}
							}
						}
						else if (_camera->getViewLevel() > itZ)
						{
							frameNumber = 2; // blue box
#ifdef __EMSCRIPTEN__
							// Lower-level marker is occluded by upper-floor tiles in the
							// CPU painter, but the GPU cursor pass runs post-composite
							// with no occlusion info — would render on top of everything
							// and produce a "two-story" cursor. Skip when GPU path active.
							if (!gpuCursorSet)
#endif
							{
								tmpSurface = _game->getMod()->getSurfaceSet("CURSOR.PCK")->getFrame(frameNumber);
								Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y, 0);
							}
						}
					}

					if (isUnitMovingNearby)
					{
						// special handling for a moving unit in background of tile.
						constexpr static Position backPos[] =
						{
							Position(0, -1, 0),
							Position(-1, -1, 0),
							Position(-1, 0, 0),
						};

						for (size_t b = 0; b < std::size(backPos); ++b)
						{
							drawUnit(unitSprite, _save->getTile(mapPosition + backPos[b]), tile, screenPosition, topLayer);
						}
					}

					// Draw walls
					{
#ifdef __EMSCRIPTEN__
						if (!hdWallMode)
						{
#endif
						// Draw west wall
						tmpSurface = tile->getSprite(O_WESTWALL);
						if (tmpSurface)
						{
							int wallShade = getWallShade(O_WESTWALL, tile);
							if (tile->getObstacle(O_WESTWALL))
								Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y - tile->getYOffset(O_WESTWALL), obstacleShade, false, _nvColor);
							else
								Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y - tile->getYOffset(O_WESTWALL), wallShade, false, _nvColor);
						}
						// Draw north wall
						tmpSurface = tile->getSprite(O_NORTHWALL);
						if (tmpSurface)
						{
							int wallShade = getWallShade(O_NORTHWALL, tile);
							if (tile->getObstacle(O_NORTHWALL))
								Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y - tile->getYOffset(O_NORTHWALL), obstacleShade, bool(tile->getSprite(O_WESTWALL)), _nvColor);
							else
								Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y - tile->getYOffset(O_NORTHWALL), wallShade, bool(tile->getSprite(O_WESTWALL)), _nvColor);
						}
#ifdef __EMSCRIPTEN__
						} // end !hdWallMode (walls)
#endif
						// Draw object
						tmpSurface = tile->getSprite(O_OBJECT);
						if (tmpSurface)
						{
							if (tile->isBackTileObject(O_OBJECT))
							{
#ifdef __EMSCRIPTEN__
								if (!hdWallMode)
#endif
								{
								if (tile->getObstacle(O_OBJECT))
									Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y - tile->getYOffset(O_OBJECT), obstacleShade, false, _nvColor);
								else
									Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y - tile->getYOffset(O_OBJECT), tileShade, false, _nvColor);
								}
							}
						}
						// draw an item on top of the floor (if any)
						BattleItem* item = tile->getTopItem();
						if (item)
						{
#ifdef __EMSCRIPTEN__
							// In HD mode emit floor item into the GPU pre-composite at
							// the item's tile Z, so it renders between walls and units
							// in iso order rather than over everything via CPU surface.
							const Mod::UnitAtlasSpec* itemAtlasSpec =
							    gpuSpriteMode ? _game->getMod()->getUnitAtlas("FLOOROB.PCK")
							                  : nullptr;
							if (itemAtlasSpec && itemAtlasSpec->atlas)
							{
								size_t idx = _unitAtlasGroups.size();
								for (size_t i = 0; i < _unitAtlasGroups.size(); ++i)
									if (_unitAtlasGroups[i].spec == itemAtlasSpec) { idx = i; break; }
								if (idx == _unitAtlasGroups.size())
								{
									_unitAtlasGroups.push_back({});
									_unitAtlasGroups.back().spec = itemAtlasSpec;
								}
								itemSprite.setEmitMode(
								    &_unitAtlasGroups[idx].instances,
								    itemAtlasSpec, itZ, itY, mapPosition.x,
								    &_unitAtlasGroups[idx].zLevels,
								    &_unitAtlasGroups[idx].yLevels);
							}
#endif
							itemSprite.draw(item,
								screenPosition.x,
								screenPosition.y + tile->getTerrainLevel(),
								tileShade
							);
#ifdef __EMSCRIPTEN__
							itemSprite.clearEmitMode();
#endif
							if (_anyIndicator)
							{
								BattleUnit *itemUnit = item->getUnit();
								if (itemUnit && itemUnit->getStatus() == STATUS_UNCONSCIOUS && itemUnit->indicatorsAreEnabled())
								{
									if (_burnIndicator && itemUnit->getFire() > 0)
									{
										_burnIndicator->blitNShade(surface,
											screenPosition.x,
											screenPosition.y + tile->getTerrainLevel(),
											tileShade);
									}
									else if (_woundIndicator && itemUnit->getFatalWounds() > 0)
									{
										_woundIndicator->blitNShade(surface,
											screenPosition.x,
											screenPosition.y + tile->getTerrainLevel(),
											tileShade);
									}
									else if (_shockIndicator && itemUnit->hasNegativeHealthRegen())
									{
										_shockIndicator->blitNShade(surface,
											screenPosition.x,
											screenPosition.y + tile->getTerrainLevel(),
											tileShade);
									}
									else if (_stunIndicator)
									{
										_stunIndicator->blitNShade(surface,
											screenPosition.x,
											screenPosition.y + tile->getTerrainLevel(),
											tileShade);
									}
								}
							}
						}
					}

					// check if we got bullet && it is in Field Of View
					if (_projectile && _projectileInFOV)
					{
						tmpSurface = nullptr;
						BattleItem* item = _projectile->getItem();
						if (item)
						{
							Position voxelPos = _projectile->getPosition();
							// draw shadow on the floor
							voxelPos.z = _save->getTileEngine()->castedShade(voxelPos);
							if (voxelPos.x / 16 >= itX &&
								voxelPos.y / 16 >= itY &&
								voxelPos.x / 16 <= itX+1 &&
								voxelPos.y / 16 <= itY+1 &&
								voxelPos.z / 24 == itZ &&
								_save->getTileEngine()->isVoxelVisible(voxelPos))
							{
								_camera->convertVoxelToScreen(voxelPos, &bulletPositionScreen);

								itemSprite.drawShadow(item,
									bulletPositionScreen.x - 16,
									bulletPositionScreen.y - 26
								);
							}

							voxelPos = _projectile->getPosition();
							// draw thrown object
							if (voxelPos.x / 16 >= itX &&
								voxelPos.y / 16 >= itY &&
								voxelPos.x / 16 <= itX+1 &&
								voxelPos.y / 16 <= itY+1 &&
								voxelPos.z / 24 == itZ &&
								_save->getTileEngine()->isVoxelVisible(voxelPos))
							{
								_camera->convertVoxelToScreen(voxelPos, &bulletPositionScreen);

								itemSprite.draw(item,
									bulletPositionScreen.x - 16,
									bulletPositionScreen.y - 26,
									tileShade
								);
							}
						}
						else
						{
#ifdef __EMSCRIPTEN__
							// Block 11.8: bullets are drawn by drawProjectileGLPass() in GPU mode.
							if (!(_game->getMod()->hasHDPack() && GpuInit::ready()))
#endif
							{
							// draw bullet on the correct tile
							if (itX >= bulletLowX && itX <= bulletHighX && itY >= bulletLowY && itY <= bulletHighY)
							{
								int begin = 0;
								int end = BULLET_SPRITES;
								int direction = 1;
								if (_projectile->isReversed())
								{
									begin = BULLET_SPRITES - 1;
									end = -1;
									direction = -1;
								}

								for (int i = begin; i != end; i += direction)
								{
									tmpSurface = _projectileSet->getFrame(_projectile->getParticle(i));
									if (tmpSurface)
									{
										Position voxelPos = _projectile->getPosition(1-i);
										// draw shadow on the floor
										voxelPos.z = _save->getTileEngine()->castedShade(voxelPos);
										if (voxelPos.x / 16 == itX &&
											voxelPos.y / 16 == itY &&
											voxelPos.z / 24 == itZ &&
											_save->getTileEngine()->isVoxelVisible(voxelPos))
										{
											_camera->convertVoxelToScreen(voxelPos, &bulletPositionScreen);
											bulletPositionScreen.x -= tmpSurface->getWidth() / 2;
											bulletPositionScreen.y -= tmpSurface->getHeight() / 2;
											Surface::blitRaw(surface, tmpSurface, bulletPositionScreen.x, bulletPositionScreen.y, 16, false, _nvColor);
										}

										// draw bullet itself
										voxelPos = _projectile->getPosition(1-i);
										if (voxelPos.x / 16 == itX &&
											voxelPos.y / 16 == itY &&
											voxelPos.z / 24 == itZ &&
											_save->getTileEngine()->isVoxelVisible(voxelPos))
										{
											_camera->convertVoxelToScreen(voxelPos, &bulletPositionScreen);
											bulletPositionScreen.x -= tmpSurface->getWidth() / 2;
											bulletPositionScreen.y -= tmpSurface->getHeight() / 2;
											Surface::blitRaw(surface, tmpSurface, bulletPositionScreen.x, bulletPositionScreen.y, 0, false, _nvColor);
										}
									}
								}
							}
							} // end CPU bullet draw
						}
					}

					//draw particle clouds
					int pixelMaskArray[] = { 0, 2, 1, 3 };
					SurfaceRaw<int> pixelMask(pixelMaskArray, 2, 2);
					const int vaporScreenOriginX = screenPosition.x + _spriteWidth / 2;
					const int vaporScreenOriginY = screenPosition.y + _spriteHeight - _spriteWidth / 2 + tile->getPosition().toVoxel().z;

					// R1.2 / Q1: vapor uses a perceptual darkening fallback on 32bpp surfaces.
					// The original palette-indexed transparency table (_transparencies) cannot
					// be applied without a destination framebuffer palette mirror — locally
					// computing transparetPtr/transparetOffsets here would just be dead state.
					// Fixing this needs a Map-side mirror (deferred); ignoring p.getColor()
					// makes vapor colour-agnostic until then.

					//draw particle clouds behind solder
					for (const Particle& p : getVaporParticle(tile, 0))
					{
						int vaporX = vaporScreenOriginX + p.getOffsetX();
						int vaporY = vaporScreenOriginY + p.getOffsetY();
						ShaderDrawFunc(
							[&](Uint32& dest, int size)
							{
								if (p.getSize() <= size)
								{
									dest = ::OpenXcom::shadeARGBCurve(dest, p.getOpacity());
								}
							},
							ShaderSurface32(this),
							ShaderMove(pixelMask, vaporX, vaporY)
						);
					}

					unit = tile->getUnit();
					// Draw soldier from this tile, below or above
					drawUnit(unitSprite, tile, tile, screenPosition, topLayer, isUnitMovingNearby ? movingUnit : nullptr);

					if (isUnitMovingNearby)
					{
						// special handling for a moving unit in foreground of tile.
						constexpr static Position frontPos[] =
						{
							Position(-1, +1, 0),
							Position(0, +1, 0),
							Position(+1, +1, 0),
							Position(+1, 0, 0),
							Position(+1, -1, 0),
						};

						for (size_t f = 0; f < std::size(frontPos); ++f)
						{
							drawUnit(unitSprite, _save->getTile(mapPosition + frontPos[f]), tile, screenPosition, topLayer);
						}
					}

					// Draw smoke/fire
#ifdef __EMSCRIPTEN__
					if (!(tile->getSmoke() && tile->isDiscovered(O_FLOOR) && _game->getMod()->hasHDPack() && GpuInit::ready()))
#endif
					if (tile->getSmoke() && tile->isDiscovered(O_FLOOR))
					{
						frameNumber = 0;
						int shade = 0;
						if (!tile->getFire())
						{
							if (_save->getDepth() > 0)
							{
								frameNumber += Mod::UNDERWATER_SMOKE_OFFSET;
							}
							else
							{
								frameNumber += Mod::SMOKE_OFFSET;
							}
							if (Mod::EXTENDED_SMOKE_OFFSET == 0)
							{
								frameNumber += int(floor((tile->getSmoke() / 6.0) - 0.1)); // see http://www.ufopaedia.org/images/c/cb/Smoke.gif
							}
							else if (Mod::EXTENDED_SMOKE_OFFSET == 1)
							{
								frameNumber += int(floor((tile->getSmoke() / 6.0) - 0.1)) * 4;
							}
							else // if (Mod::EXTENDED_SMOKE_OFFSET == 2)
							{
								frameNumber += (tile->getSmoke() - 1) / 5 * 4;
							}
							shade = tileShade;
						}

						if (halfAnimFrame + tile->getAnimationOffset() > 3)
						{
							frameNumber += halfAnimFrame + tile->getAnimationOffset() - 4;
						}
						else
						{
							frameNumber += halfAnimFrame + tile->getAnimationOffset();
						}
						tmpSurface = _game->getMod()->getSurfaceSet("SMOKE.PCK")->getFrame(frameNumber);
						Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y, shade, false, _nvColor);
					}

					//draw particle clouds on front of solder
					for (const Particle& p : getVaporParticle(tile, topLayer ? 3 : 1))
					{
						int vaporX = vaporScreenOriginX + p.getOffsetX();
						int vaporY = vaporScreenOriginY + p.getOffsetY();
						// R1.2 / Q1: see comment on the back-row loop above.
						ShaderDrawFunc(
							[&](Uint32& dest, int size)
							{
								if (p.getSize() <= size)
								{
									dest = ::OpenXcom::shadeARGBCurve(dest, p.getOpacity());
								}
							},
							ShaderSurface32(this),
							ShaderMove(pixelMask, vaporX, vaporY)
						);
					}

					// Draw Path Preview
					if (_previewSettingArrows && tile->getPreview() != -1 && tile->isDiscovered(O_FLOOR))
					{
						if (itZ > 0 && tile->hasNoFloor(_save))
						{
							tmpSurface = _game->getMod()->getSurfaceSet("Pathfinding")->getFrame(11);
							if (tmpSurface)
							{
								Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y+2, 0, false, tile->getMarkerColor());
							}
						}
						tmpSurface = _game->getMod()->getSurfaceSet("Pathfinding")->getFrame(tile->getPreview());
						if (tmpSurface)
						{
							Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y + tile->getTerrainLevel(), 0, false, tileColor);
						}
					}

					{
						// Draw object
						tmpSurface = tile->getSprite(O_OBJECT);
						if (tmpSurface)
						{
							if (!tile->isBackTileObject(O_OBJECT))
							{
#ifdef __EMSCRIPTEN__
								if (!hdWallMode)
#endif
								{
								if (tile->getObstacle(O_OBJECT))
									Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y - tile->getYOffset(O_OBJECT), obstacleShade, false, _nvColor);
								else
									Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y - tile->getYOffset(O_OBJECT), tileShade, false, _nvColor);
								}
							}
						}
					}
					// Draw cursor front
					if (_cursorType != CT_NONE && _selectorX > itX - _cursorSize && _selectorY > itY - _cursorSize && _selectorX < itX+1 && _selectorY < itY+1 && !_save->getBattleState()->getMouseOverIcons())
					{
						if (_camera->getViewLevel() == itZ)
						{
							if (_cursorType != CT_AIM)
							{
#ifdef __EMSCRIPTEN__
								// Phase 16: cursor style — faction-aware 4-tip markers or floor ring.
								if (gpuCursorSet)
								{
									CursorStyle cursorStyle;
									bool tileHasUnit = unit && (unit->getVisible() || _save->getDebugMode());
									BattleUnit *selUnit = _save->getSelectedUnit();
									bool playerWalking = (selUnit && selUnit->getFaction() == FACTION_PLAYER
									                      && !selUnit->isOut() && _cursorType == CT_NORMAL);
									if (tileHasUnit)
										// Visible unit on tile → 4-tip marker (ally=blue, enemy=orange)
										cursorStyle = (unit->getFaction() == FACTION_PLAYER) ? CS_MARKER_ALLY : CS_MARKER_ENEMY;
									else if (playerWalking)
										// Player unit selected, empty tile → floor ring
										cursorStyle = CS_FLOOR_RING;
									else
										cursorStyle = CS_MARKER_NEUTRAL;

									// Phase 16: hide neutral markers when a player is selected for movement.
									// This clarifies the focus on the target destination circle.
									if (playerWalking && cursorStyle == CS_MARKER_NEUTRAL)
									{
										// skip pushing instance
									}
									else
									{
										_cursorOverlayInstances.push_back({screenPosition.x, screenPosition.y, nullptr, 0, cursorStyle});
									}

									// Phase 16: hide the system cursor arrow when a player unit is selected
									// for movement, but ONLY if we are actually in the interactive map state.
									// This ensures the cursor remains visible in the Esc menu.
									// _game->isState() checks if the given state is the top-most one.
									bool isMapActive = _game->isState(_save->getBattleState());
									_game->getCursor()->setHidden(playerWalking && isMapActive);
								}

								// Remaining TU: when floor ring is active show it centred on the ring,
								// otherwise near the mouse cursor.
								if (_cursorType == CT_NORMAL)
								{
									BattleUnit *selUnit = _save->getSelectedUnit();
									if (selUnit && selUnit->getFaction() == FACTION_PLAYER
										&& selUnit->getBaseStats()->tu > 0)
									{
										int tuToShow = _hoveredTU;
										if (tuToShow < 0 && pathfinderTurnedOn && tile)
											tuToShow = tile->getTUMarker();
										if (tuToShow >= 0)
										{
											float frac = static_cast<float>(tuToShow) /
											             static_cast<float>(selUnit->getBaseStats()->tu);
											Uint8 colorIdx;
											if      (frac > 0.50f) colorIdx = Palette::blockOffset(Pathfinding::green  - 1) - 1;
											else if (frac > 0.25f) colorIdx = Palette::blockOffset(Pathfinding::yellow - 1) - 1;
											else                   colorIdx = Palette::blockOffset(Pathfinding::red    - 1) - 1;

											SDL_Color *pal = _game->getScreen()->getPalette();
											Uint32 argb = (0xFF000000u | ((Uint32)pal[colorIdx].r << 16) | ((Uint32)pal[colorIdx].g << 8) | (Uint32)pal[colorIdx].b);

											bool tileHasUnit = unit && (unit->getVisible() || _save->getDebugMode());
											bool showFloorRing = gpuCursorSet && !tileHasUnit && !selUnit->isOut();
											if (getHdNumberFont())
											{
												if (showFloorRing)
												{
													// Floor ring center in surface-pixel space:
													//   X = tile left + half tile width
													//   Y = tile top  + tileH - tileW/4  (≡ floorCY in UV space)
													int cx = screenPosition.x + _spriteWidth / 2;
													int cy = screenPosition.y + _spriteHeight - _spriteWidth / 4;
													// Phase 16: use HD font for AP inside the ring.
													this->drawHdNumber(surface, cx, cy - 3, tuToShow, argb);
												}
												else
												{
													// _mouseX/_mouseY are already surface-space.
													// Phase 16: use HD font for mouse-hover AP.
													this->drawHdNumber(surface, _mouseX, _mouseY - 12, tuToShow, argb);
												}
											}
											else
											{
												// Fallback to standard 8-bpp font.
												std::ostringstream ssCur;
												ssCur << tuToShow;
												_txtCursorAP->setColor(colorIdx);
												_txtCursorAP->setText(ssCur.str());
												_txtCursorAP->draw();
												if (showFloorRing)
												{
													int cx = screenPosition.x + _spriteWidth / 2;
													int cy = screenPosition.y + _spriteHeight - _spriteWidth / 4;
													_txtCursorAP->blitNShade(surface, cx - 22, cy - 3, 0);
												}
												else
												{
													_txtCursorAP->blitNShade(surface, _mouseX - 16, _mouseY - 18, 0);
												}
											}
										}
									}
								}

								if (!gpuCursorSet)
#endif
								{
									if (unit && (unit->getVisible() || _save->getDebugMode()))
										frameNumber = 3 + halfAnimFrameRest; // yellow box
									else
										frameNumber = 3; // red box
									tmpSurface = _game->getMod()->getSurfaceSet("CURSOR.PCK")->getFrame(frameNumber);
									Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y, 0);
								}
							}
							else
							{
								if (unit && (unit->getVisible() || _save->getDebugMode()))
									frameNumber = 7 + halfAnimFrame; // yellow animated crosshairs
								else
									frameNumber = 6; // red static crosshairs
#ifdef __EMSCRIPTEN__
								if (gpuCursorSet)
									_cursorOverlayInstances.push_back({screenPosition.x, screenPosition.y, gpuCursorSet, frameNumber, CS_RASTER});
								else
#endif
								{
									tmpSurface = _game->getMod()->getSurfaceSet("CURSOR.PCK")->getFrame(frameNumber);
									Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y, 0);
								}
							}

							// UFO extender accuracy: display adjusted accuracy value on crosshair in real-time.
							if (_cursorType >= CT_AIM && _showInfoOnCursor && (_cursorType != CT_THROW || !Options::oxceDisableInfoOnThrowCursor))
							{
								BattleAction *action = _save->getBattleGame()->getCurrentAction();
								const RuleItem *weapon = action->weapon->getRules();
								std::ostringstream ss;
								BattleActionAttack attack = BattleActionAttack::GetBeforeShoot(*action);
								int distanceSq = action->actor->distance3dToPositionSq(Position(itX, itY,itZ));
								int distance = (int)std::ceil(sqrt(float(distanceSq)));

								if (_cursorType == CT_AIM || _cursorType == CT_THROW)
								{
									int accuracy = BattleUnit::getFiringAccuracy(attack, _game->getMod());

									{
										int upperLimit, lowerLimit;
										int dropoff = weapon->calculateLimits(upperLimit, lowerLimit, _save->getDepth(), action->type);

										// at this point, let's assume the shot is adjusted and set the text amber.
										_txtAccuracy->setColor(Palette::blockOffset(Pathfinding::yellow - 1) - 1);

										if (distance > upperLimit)
										{
											accuracy -= (distance - upperLimit) * dropoff;
										}
										else if (distance < lowerLimit)
										{
											accuracy -= (lowerLimit - distance) * dropoff;
										}
										else
										{
											// no adjustment made? set it to green.
											_txtAccuracy->setColor(Palette::blockOffset(Pathfinding::green - 1) - 1);
										}
									}

									// Include LOS penalty for tiles in the unit's current view range
									// Don't recalculate LOS for outside of the current FOV
									int noLOSAccuracyPenalty = action->weapon->getRules()->getNoLOSAccuracyPenalty(_game->getMod());
									if (noLOSAccuracyPenalty != -1)
									{
										bool hasLOS = false;
										if (Position(itX, itY, itZ) == _cacheCursorPosition && _isCtrlPressed == _cacheIsCtrlPressed && _cacheHasLOS != -1)
										{
											// use cached result
											hasLOS = (_cacheHasLOS == 1);
										}
										else
										{
											// recalculate
											if (unit && (unit->getVisible() || _save->getDebugMode()))
											{
												hasLOS = _save->getTileEngine()->visible(action->actor, tile);
											}
											else
											{
												hasLOS = _save->getTileEngine()->isTileInLOS(action, tile, true);
											}
											// remember
											_cacheIsCtrlPressed = _isCtrlPressed;
											_cacheCursorPosition = Position(itX, itY, itZ);
											_cacheHasLOS = hasLOS ? 1 : 0;
										}

										if (!hasLOS)
										{
											accuracy = accuracy * noLOSAccuracyPenalty / 100;
											_txtAccuracy->setColor(Palette::blockOffset(Pathfinding::yellow - 1) - 1);
										}
									}

									bool outOfRange = action->type == BA_THROW
										? weapon->isOutOfThrowRange(distanceSq, _save->getDepth())
										: weapon->isOutOfRange(distanceSq);

									// zero accuracy or out of range: set it red.
									if (accuracy <= 0 || outOfRange)
									{
										accuracy = 0;
										_txtAccuracy->setColor(Palette::blockOffset(Pathfinding::red - 1) - 1);
									}
									ss << accuracy;
									ss << "%";
								}

								//TODO: merge this code with `InventoryState::calculateCurrentDamageTooltip` as 90% is same or should be same
								// display additional damage and psi-effectiveness info
								if (_isAltPressed)
								{
									// step 1: determine rule
									const RuleItem *rule;
									if (weapon->getBattleType() == BT_PSIAMP)
									{
										rule = weapon;
									}
									else if (action->weapon->needsAmmoForAction(action->type))
									{
										auto* ammo = attack.damage_item;
										if (ammo != nullptr)
										{
											rule = ammo->getRules();
										}
										else
										{
											rule = 0; // empty weapon = no rule
										}
									}
									else
									{
										rule = weapon;
									}

									// step 2: check if unlocked
									if (_cacheActiveWeaponUfopediaArticleUnlocked == -1)
									{
										_cacheActiveWeaponUfopediaArticleUnlocked = 0;
										if (_game->getSavedGame()->getMonthsPassed() == -1)
										{
											_cacheActiveWeaponUfopediaArticleUnlocked = 1; // new battle mode
										}
										else if (rule)
										{
											_cacheActiveWeaponUfopediaArticleUnlocked = 1; // assume unlocked
											ArticleDefinition *article = _game->getMod()->getUfopaediaArticle(rule->getType(), false);
											if (article && !Ufopaedia::isArticleAvailable(_game->getSavedGame(), article))
											{
												_cacheActiveWeaponUfopediaArticleUnlocked = 0; // ammo/weapon locked
											}
											if (rule->getType() != weapon->getType())
											{
												article = _game->getMod()->getUfopaediaArticle(weapon->getType(), false);
												if (article && !Ufopaedia::isArticleAvailable(_game->getSavedGame(), article))
												{
													_cacheActiveWeaponUfopediaArticleUnlocked = 0; // weapon locked
												}
											}
										}
									}

									// step 3: calculate and draw
									if (rule && _cacheActiveWeaponUfopediaArticleUnlocked == 1)
									{
										if (rule->getBattleType() == BT_PSIAMP)
										{
											float attackStrength = BattleUnit::getPsiAccuracy(attack);
											float defenseStrength = 30.0f; // indicator ignores: +victim->getArmor()->getPsiDefence(victim);

											float dis = Position::distance(action->actor->getPosition().toVoxel(), Position(itX, itY, itZ).toVoxel());
											int min = attackStrength - defenseStrength - rule->getPsiAccuracyRangeReduction(dis);
											int max = min + 55;
											if (max <= 0)
											{
												ss << "0%";
											}
											else
											{
												ss << min << "-" << max << "%";
											}
										}
										if (rule->getBattleType() != BT_PSIAMP || action->type == BA_USE)
										{
											int totalDamage = 0;
											if (weapon->getIgnoreAmmoPower())
											{
												totalDamage += weapon->getPowerBonus(attack);
												totalDamage -= weapon->getPowerRangeReduction(distance * 16);
											}
											else
											{
												totalDamage += rule->getPowerBonus(attack);
												totalDamage -= rule->getPowerRangeReduction(distance * 16);
											}
											if (totalDamage < 0) totalDamage = 0;
											if (_cursorType != CT_WAYPOINT)
												ss << "\n";
											ss << rule->getDamageType()->getRandomDamage(totalDamage, 1);
											ss << "-";
											ss << rule->getDamageType()->getRandomDamage(totalDamage, 2);
											if (rule->getDamageType()->RandomType == DRT_UFO_WITH_TWO_DICE)
												ss << "*";
										}
									}
									else
									{
										ss << "\n?-?";
									}
								}

								_txtAccuracy->setText(ss.str());
								_txtAccuracy->draw();
								_txtAccuracy->blitNShade(surface, screenPosition.x, screenPosition.y, 0);
							}
						}
						else if (_camera->getViewLevel() > itZ)
						{
							frameNumber = 5; // blue box
#ifdef __EMSCRIPTEN__
							// See cursor-back rationale above — skip the lower-level
							// marker on the GPU path to avoid the "two-story" cursor.
							if (!gpuCursorSet)
#endif
							{
								tmpSurface = _game->getMod()->getSurfaceSet("CURSOR.PCK")->getFrame(frameNumber);
								Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y, 0);
							}
						}
						if (!_isAltPressed && _cursorType > CT_AIM && _camera->getViewLevel() == itZ)
						{
							bool ignore = false;
							if (_cursorType == CT_PSI || _cursorType == CT_WAYPOINT)
							{
								BattleAction* action = _save->getBattleGame()->getCurrentAction();
								int distanceSq = action->actor->distance3dToPositionSq(Position(itX, itY, itZ));
								if (action->weapon->getRules()->isOutOfRange(distanceSq))
								{
									// weapon doesn't work at this distance, just draw a normal cursor with a red 0% hint text
									ignore = true;
									_txtAccuracy->setColor(Palette::blockOffset(Pathfinding::red - 1) - 1);
									_txtAccuracy->setText("0%");
									_txtAccuracy->draw();
									_txtAccuracy->blitNShade(surface, screenPosition.x, screenPosition.y, 0);
								}
							}
							if (!ignore)
							{
								int frame[6] = { 0, 0, 0, 11, 13, 15 };
								const int cursorFrame = frame[_cursorType] + (_animFrame / 4) % 2;
#ifdef __EMSCRIPTEN__
								if (gpuCursorSet)
									_cursorOverlayInstances.push_back({screenPosition.x, screenPosition.y, gpuCursorSet, cursorFrame, CS_RASTER});
								else
#endif
								{
									tmpSurface = _game->getMod()->getSurfaceSet("CURSOR.PCK")->getFrame(cursorFrame);
									Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y, 0);
								}
							}
						}
					}


					// Draw waypoints if any on this tile
					int waypid = 1;
					int waypXOff = 2;
					int waypYOff = 2;

					for (const auto& waypoint : _waypoints)
					{
						if (waypoint == mapPosition)
						{
							if (waypXOff == 2 && waypYOff == 2)
							{
#ifdef __EMSCRIPTEN__
								if (gpuCursorSet)
									_cursorOverlayInstances.push_back({screenPosition.x, screenPosition.y, gpuCursorSet, 7, CS_RASTER});
								else
#endif
								{
									tmpSurface = _game->getMod()->getSurfaceSet("CURSOR.PCK")->getFrame(7);
									Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y, 0);
								}
							}
							if (_save->getBattleGame()->getCurrentAction()->type == BA_LAUNCH || _save->getBattleGame()->getCurrentAction()->sprayTargeting)
							{
								_numWaypid->setValue(waypid);
								_numWaypid->draw();
								_numWaypid->blitNShade(surface, screenPosition.x + waypXOff, screenPosition.y + waypYOff, 0);

								waypXOff += waypid > 9 ? 8 : 6;
								if (waypXOff >= 26)
								{
									waypXOff = 2;
									waypYOff += 8;
								}
							}
						}
						waypid++;
					}
				}
			}
		}
	}
	if (pathfinderTurnedOn)
	{
		if (_numWaypid)
		{
			_numWaypid->setBordered(true); // give it a border for the pathfinding display, makes it more visible on snow, etc.
		}
		for (int itZ = beginZ; itZ <= endZ; itZ++)
		{
			for (int itX = beginX; itX <= endX; itX++)
			{
				for (int itY = beginY; itY <= endY; itY++)
				{
					mapPosition = Position(itX, itY, itZ);
					_camera->convertMapToScreen(mapPosition, &screenPosition);
					screenPosition += _camera->getMapOffset();

					// only render cells that are inside the surface
					if (screenPosition.x > -_spriteWidth && screenPosition.x < surface->getWidth() + _spriteWidth &&
						screenPosition.y > -_spriteHeight && screenPosition.y < surface->getHeight() + _spriteHeight )
					{
						tile = _save->getTile(mapPosition);
						if (!tile || !tile->isDiscovered(O_FLOOR) || tile->getPreview() == -1)
							continue;
						int adjustment = -tile->getTerrainLevel();
						if (_previewSettingArrows)
						{
							if (itZ > 0 && tile->hasNoFloor(_save))
							{
								tmpSurface = _game->getMod()->getSurfaceSet("Pathfinding")->getFrame(23);
								if (tmpSurface)
								{
									Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y+2, 0, false, tile->getMarkerColor());
								}
							}
							int overlay = tile->getPreview() + 12;
							tmpSurface = _game->getMod()->getSurfaceSet("Pathfinding")->getFrame(overlay);
							if (tmpSurface)
							{
								Surface::blitRaw(surface, tmpSurface, screenPosition.x, screenPosition.y - adjustment, 0, false, tile->getMarkerColor());
							}
						}

						if ((_previewSettingTu || _previewSettingEnergy) && (tile->getTUMarker() > -1 || tile->getEnergyMarker() > -1))
						{
							int off = tile->getTUMarker() > 9 ? 5 : 3;
							int offE = tile->getEnergyMarker() > 9 ? 5 : 3;
							int mcolor = _previewSettingArrows ? 0 : tile->getMarkerColor();
							if (_previewSettingArrows)
							{
								adjustment += 7;
							}
							if (_save->getSelectedUnit() && _save->getSelectedUnit()->isBigUnit())
							{
								adjustment += 1;
								if (!_previewSettingArrows)
								{
									adjustment += 7;
								}
							}
							if (_previewSettingTu)
							{
								_numWaypid->setValue(tile->getTUMarker());
								_numWaypid->draw();
								if (_previewSettingEnergy)
								{
									// TU
									_numWaypid->blitNShade(surface, screenPosition.x + 16 - off, screenPosition.y + (22 - adjustment), 0, false, mcolor);
									// and Energy
									_numWaypid->setValue(tile->getEnergyMarker());
									_numWaypid->draw();
									_numWaypid->blitNShade(surface, screenPosition.x + 16 - offE, screenPosition.y + (29 - adjustment), 0, false, mcolor);
								}
								else
								{
									// only TU
									_numWaypid->blitNShade(surface, screenPosition.x + 16 - off, screenPosition.y + (29 - adjustment), 0, false, mcolor);
								}
							}
							else if (_previewSettingEnergy)
							{
								// only Energy
								_numWaypid->setValue(tile->getEnergyMarker());
								_numWaypid->draw();
								_numWaypid->blitNShade(surface, screenPosition.x + 16 - offE, screenPosition.y + (29 - adjustment), 0, false, mcolor);
							}
						}
					}
				}
			}
		}
		if (_numWaypid)
		{
			_numWaypid->setBordered(false); // make sure we remove the border in case it's being used for missile waypoints.
		}
	}

	auto* selectedUnit = _save->getSelectedUnit();
	if (selectedUnit && (_save->getSide() == FACTION_PLAYER || _save->getDebugMode()) && selectedUnit->getPosition().z <= _camera->getViewLevel())
	{
		_camera->convertMapToScreen(selectedUnit->getPosition(), &screenPosition);
		screenPosition += _camera->getMapOffset();
		Position offset = calculateWalkingOffset(selectedUnit).ScreenOffset;
		if (selectedUnit->isBigUnit())
		{
			offset.y += 4;
		}
		offset.y += Position::TileZ - (selectedUnit->getHeight() + selectedUnit->getFloatHeight());
		if (selectedUnit->isKneeled())
		{
			offset.y -= 2;
		}
		if (this->getCursorType() != CT_NONE)
		{
			int arrowX = screenPosition.x + offset.x + (_spriteWidth / 2) - (_arrow->getWidth() / 2);
			int arrowY = screenPosition.y + offset.y - _arrow->getHeight() + getArrowBobForFrame(_animFrame);
			// Phase 16: selection arrow removed to reduce UI noise; replaced by AP number.
			// _arrow->blitNShade(surface, arrowX, arrowY, 0);

#ifdef __EMSCRIPTEN__
			if (selectedUnit->getFaction() == FACTION_PLAYER && !selectedUnit->isOut())
			{
				float arcFrac = static_cast<float>(selectedUnit->getTimeUnits()) /
				                static_cast<float>(std::max(static_cast<int>(1), static_cast<int>(selectedUnit->getBaseStats()->tu)));
				Uint8 colorIdx;
				if      (arcFrac > 0.50f) colorIdx = Palette::blockOffset(Pathfinding::green  - 1) - 1;
				else if (arcFrac > 0.25f) colorIdx = Palette::blockOffset(Pathfinding::yellow - 1) - 1;
				else                      colorIdx = Palette::blockOffset(Pathfinding::red    - 1) - 1;

				SDL_Color *pal = _game->getScreen()->getPalette();
				Uint32 argb = (0xFF000000u | ((Uint32)pal[colorIdx].r << 16) | ((Uint32)pal[colorIdx].g << 8) | (Uint32)pal[colorIdx].b);

				// Phase 16: use HD font for unit's current AP.
				if (getHdNumberFont())
				{
					this->drawHdNumber(surface, arrowX + (_arrow->getWidth() / 2), arrowY + 6, selectedUnit->getTimeUnits(), argb);
				}
				else
				{
					// Fallback to standard 8-bpp font.
					std::ostringstream ssAP;
					ssAP << selectedUnit->getTimeUnits();
					_txtUnitAP->setColor(colorIdx);
					_txtUnitAP->setText(ssAP.str());
					_txtUnitAP->draw();
					_txtUnitAP->blitNShade(surface,
						arrowX + (_arrow->getWidth() / 2) - 22,
						arrowY + 2,
						0);
				}
			}
#endif
		}
	}

	// Draw motion scanner arrows
	if (_isAltPressed && _save->getSide() == FACTION_PLAYER && this->getCursorType() != CT_NONE)
	{
		for (auto* myUnit : *_save->getUnits())
		{
			bool motionScan = myUnit->getScannedTurn() == _save->getTurn() && myUnit->getFaction() != FACTION_PLAYER && !myUnit->isOut();
			bool customMarker = myUnit->getCustomMarker() > 0 && myUnit->getFaction() == FACTION_PLAYER && !myUnit->isOut();
			if (motionScan || customMarker)
			{
				Position temp = myUnit->getPosition();
				temp.z = _camera->getViewLevel();
				_camera->convertMapToScreen(temp, &screenPosition);
				screenPosition += _camera->getMapOffset();
				Position offset;
				//calculateWalkingOffset(myUnit, &offset);
				if (myUnit->isBigUnit())
				{
					offset.y += 4;
				}
				if (motionScan)
				{
					offset.y += Position::TileZ - /*myUnit->getHeight()*/ 21; // no spoilers
				}
				else if (customMarker)
				{
					offset.y += Position::TileZ - (myUnit->getHeight() + myUnit->getFloatHeight());
				}
				if (myUnit->isKneeled())
				{
					offset.y -= 2;
				}
				if (motionScan)
				{
					_arrow->blitNShade(
						surface,
						screenPosition.x + offset.x + (_spriteWidth / 2) - (_arrow->getWidth() / 2),
						screenPosition.y + offset.y - _arrow->getHeight() + getArrowBobForFrame(_animFrame),
						0);
				}
				else if (customMarker)
				{
					Surface::blitRaw(
						surface,
						_arrow,
						screenPosition.x + offset.x + (_spriteWidth / 2) - (_arrow->getWidth() / 2),
						screenPosition.y + offset.y - _arrow->getHeight() + getArrowBobForFrame(_animFrame),
						0,
						false,
						_isTFTD ? ArrowColorsTFTD[myUnit->getCustomMarker() % 4] : ArrowColorsUFO[myUnit->getCustomMarker() % 4]);
				}
			}
		}
	}
	delete _numWaypid;

	// Draw craft deployment preview arrows
	if (_isAltPressed && _save->isPreview() && this->getCursorType() != CT_NONE)
	{
		for (auto& pos : _save->getCraftTiles())
		{
			if (pos.z == _camera->getViewLevel())
			{
				_camera->convertMapToScreen(pos, &screenPosition);
				screenPosition += _camera->getMapOffset();
				screenPosition.y += 2; // based on vanilla soldier standHeight
				_arrow->blitNShade(
					surface,
					screenPosition.x + (_spriteWidth / 2) - (_arrow->getWidth() / 2),
					screenPosition.y - _arrow->getHeight() + getArrowBobForFrame(_animFrame),
					0);
			}
		}
	}

	// check if we got big explosions
	if (_explosionInFOV)
	{
		// big explosions cause the screen to flash as bright as possible before any explosions are actually drawn.
		// this causes everything to look like EGA for a single frame.
		if (_flashScreen)
		{
			for (int x = 0, y = 0; x < surface->getWidth() && y < surface->getHeight();)
			{
				Uint8 pixel = surface->getPixel(x, y);
				if (pixel)
				{
					pixel = (pixel & 0xF0) + 1; //avoid 0 pixel
					surface->setPixelIterative(&x, &y, pixel);
				}
			}
			_flashScreen = false;
		}
		else
		{
#ifdef __EMSCRIPTEN__
			// Block 11.9: explosions rendered by drawSmokeGLPass() in GPU mode.
			if (!(_game->getMod()->hasHDPack() && GpuInit::ready()))
#endif
			for (const auto* explosion : _explosions)
			{
				_camera->convertVoxelToScreen(explosion->getPosition(), &bulletPositionScreen);
				if (explosion->isBig())
				{
					if (explosion->getCurrentFrame() >= 0)
					{
						tmpSurface = _game->getMod()->getSurfaceSet("X1.PCK")->getFrame(explosion->getCurrentFrame());
						Surface::blitRaw(surface, tmpSurface, bulletPositionScreen.x - (tmpSurface->getWidth() / 2), bulletPositionScreen.y - (tmpSurface->getHeight() / 2), 0, false, _nvColor);
					}
				}
				else if (explosion->isHit())
				{
					tmpSurface = _game->getMod()->getSurfaceSet("HIT.PCK")->getFrame(explosion->getCurrentFrame());
					Surface::blitRaw(surface, tmpSurface, bulletPositionScreen.x - 15, bulletPositionScreen.y - 25, 0, false, _nvColor);
				}
				else
				{
					tmpSurface = _game->getMod()->getSurfaceSet("SMOKE.PCK")->getFrame(explosion->getCurrentFrame());
					Surface::blitRaw(surface, tmpSurface, bulletPositionScreen.x - 15, bulletPositionScreen.y - 15, 0, false, _nvColor);
				}
			}
		}
	}

	surface->unlock();

#ifdef __EMSCRIPTEN__
	/* Phase 11.0: log avg CPU time every 30 frames (opt-in only). */
	if (profileBs)
	{
		bsTimer.stop();
		static long long s_accumUs   = 0;
		static unsigned  s_frameCount = 0;
		s_accumUs += bsTimer.elapsedUs();
		const unsigned BATCH = 30u;
		if (++s_frameCount >= BATCH)
		{
			Log(LOG_INFO) << "Map::drawTerrain avg: "
			              << (s_accumUs / (long long)s_frameCount) << " us/frame"
			              << " (" << surface->getWidth() << "x" << surface->getHeight()
			              << ", n=" << s_frameCount << ", CPU-side)";
			s_accumUs    = 0;
			s_frameCount = 0;
		}
	}
#endif
}

#ifdef __EMSCRIPTEN__
/**
 * GPU Battlescape compositor.
 * Computes the per-frame animation phase, collects TileInstance records via
 * emitTilePass(), then issues glDrawArraysInstanced for each mapDataSet atlas
 * via a registered GPU pass.
 */
void Map::drawTerrainGPU(Surface* surface)
{
	if (g_calypsoDumpEmit)
		Log(LOG_INFO) << "[DIAG-GPU] drawTerrainGPU entered, _tileAtlasGroups.size=" << _tileAtlasGroups.size();
	const Uint32 ticks = SDL_GetTicks();
	_animFrameGPU = static_cast<float>(ticks % TILE_ANIM_PERIOD_MS)
	                / static_cast<float>(TILE_ANIM_PERIOD_MS);
	emitTilePass();
	emitUnitPass();
	// drawTerrainOverlayCPU collects unit emit records (via drawUnit/setEmitMode)
	// and writes the SDL overlay (front O_OBJECT, path arrows, unit arrows, items,
	// flash-screen, debug overlays).  Floors/walls/units/smoke are already handled
	// by GPU pre-composite passes; guarded code paths in drawTerrainOverlayCPU
	// skip the CPU blits for those in HD mode.
	drawTerrainOverlayCPU(surface);
}

/**
 * Walk camera-visible tiles and build per-atlas TileInstance lists.
 */
void Map::emitTilePass()
{
	for (auto& grp : _tileAtlasGroups)
	{
		grp.instances.clear();
		grp.zSlices.clear();
		grp.overlayInstances.clear();
		grp.blendInstances.clear();
		for (auto& sv : grp.subLayerInstances) sv.clear();
	}
	_smokeInstances.clear();
	_cursorOverlayInstances.clear();
	// Phase 22 (H1): the terrain instance lists are about to be rebuilt — mark the
	// persistent _tileInstIBO stale so drawTileGLPass re-concatenates and re-uploads.
	_tileBuffersDirty = true;
	if (_tileAtlasGroups.empty())
	{
		static int emptyLogCount = 0;
		if (emptyLogCount < 3)
		{
			Log(LOG_INFO) << "[DIAG-EMIT] _tileAtlasGroups EMPTY at emitTilePass — GPU pipeline inactive";
			++emptyLogCount;
		}
		return;
	}

	const bool dumpEmit = g_calypsoDumpEmit != 0;
	if (dumpEmit)
	{
		Log(LOG_INFO) << "[DUMP-EMIT] === begin emitTilePass; viewLevel="
		              << _camera->getViewLevel() << " ===";
	}

	Mod* mod = _game->getMod();
	const auto* mdsVec = _save->getMapDataSets();

	// Determine the camera-visible tile range.
	int beginX = 0, endX = _save->getMapSizeX() - 1;
	int beginY = 0, endY = _save->getMapSizeY() - 1;
	int beginZ = 0, endZ = _save->getMapSizeZ() - 1;
	int dummy;
	_camera->convertScreenToMap(0, 0, &beginX, &dummy);
	_camera->convertScreenToMap(getWidth(), 0, &dummy, &beginY);
	_camera->convertScreenToMap(getWidth() + _spriteWidth, getHeight() + _spriteHeight, &endX, &dummy);
	_camera->convertScreenToMap(0, getHeight() + _spriteHeight, &dummy, &endY);
	beginY -= _camera->getViewLevel() * 2;
	beginX -= _camera->getViewLevel() * 2;
	beginX = std::max(beginX, 0); beginY = std::max(beginY, 0);
	endX   = std::min(endX, _save->getMapSizeX() - 1);
	endY   = std::min(endY, _save->getMapSizeY() - 1);
	if (!_camera->getShowAllLayers())
		endZ = std::min(endZ, _camera->getViewLevel());
	if (_camera->getShowSingleLayer())
	{
		beginZ = _camera->getViewLevel();
		endZ   = _camera->getViewLevel();
	}

	const int mapOffsetX    = getX();
	const int mapOffsetY    = getY();
	const Position camOff   = _camera->getMapOffset();
	const int halfAnimFrame = (_animFrame / 2) % 4;
	SurfaceSet* smokeSet = mod->getSurfaceSet("SMOKE.PCK");
	static const TilePart parts[4] = { O_FLOOR, O_WESTWALL, O_NORTHWALL, O_OBJECT };

	// Per-(Z, Y) row descriptors — track instance-buffer boundary per group at
	// each Y transition so unit emits at (z, y) can be interleaved between
	// rows for correct iso wall→unit→wall ordering.
	const size_t numGrps = _tileAtlasGroups.size();
	std::vector<size_t> rowSliceFirst(numGrps, 0);
	int prevEmitZ = -1;
	int prevEmitY = -1;

	auto flushRowSlice = [&]() {
		if (prevEmitZ < 0 || prevEmitY < 0) return;
		for (size_t gi = 0; gi < numGrps; ++gi)
		{
			size_t cnt = _tileAtlasGroups[gi].instances.size() - rowSliceFirst[gi];
			if (cnt > 0)
				_tileAtlasGroups[gi].zSlices.push_back({prevEmitZ, prevEmitY, rowSliceFirst[gi], cnt});
		}
		for (size_t gi = 0; gi < numGrps; ++gi)
			rowSliceFirst[gi] = _tileAtlasGroups[gi].instances.size();
	};

	for (int itZ = beginZ; itZ <= endZ; ++itZ)
	for (int itY = beginY; itY < endY;  ++itY)
	{
		if (itZ != prevEmitZ || itY != prevEmitY)
		{
			flushRowSlice();
			prevEmitZ = itZ;
			prevEmitY = itY;
		}

		Position mapPos(beginX, itY, itZ);
		for (int itX = beginX; itX < endX; ++itX, ++mapPos.x)
		{
			Tile* tile = _save->getTile(mapPos);
			if (!tile) continue;

			Position screenPos;
			_camera->convertMapToScreen(mapPos, &screenPos);
			screenPos += camOff;
			// Top-edge cull margin: O_OBJECT tiles can have negative yOffset
			// (e.g. submarine hull anchored at y=N but extending visually up to
			// y=N-2). Without margin, anchors near the top get culled while
			// their sprite is still on screen — leaving holes in the hull.
			// 80 px ≈ 2 tile heights, enough for typical hull/awning yOffsets.
			const int kTopOffsetMargin = 80;
			if (screenPos.x <= -_spriteWidth  || screenPos.x >= getWidth()  + _spriteWidth ||
			    screenPos.y <= -_spriteHeight - kTopOffsetMargin ||
			    screenPos.y >= getHeight() + _spriteHeight)
				continue;

			const int tileShade = tile->isDiscovered(O_FLOOR) ? reShade(tile) : 16;

			for (int pi = 0; pi < 4; ++pi)
			{
				TilePart part = parts[pi];
				if (!tile->getSprite(part)) continue;

				int mcdIdx = 0, mdsID = 0;
				tile->getMapData(&mcdIdx, &mdsID, part);
				if (mdsID < 0 || mdsID >= (int)mdsVec->size()) continue;
				if (mdsID >= (int)_tileAtlasGroups.size()) continue;
				AtlasGroup& grp = _tileAtlasGroups[mdsID];
				// baseline:none datasets have null atlas but still need overlay emit.
				if (!grp.atlas && !grp.overlayAtlas) continue;

				MapDataSet* mds = (*mdsVec)[mdsID];
				auto* spec = mod->getTileAtlasSpec(mds->getName());
				if (!spec) continue;

				// Resolve animation frame: try pckToAtlas for animated sprites,
				// fall back to frameMap primary-frame entry.
				//
				// Use the tile's per-part currentFrame (matches what
				// Tile::updateSprite() uses) instead of the global _animFrame:
				// UFO doors freeze at 0 or 7 and tiles created mid-cycle have
				// per-part counters that diverge from _animFrame, so a global
				// frame index would emit the wrong sprite for those tiles
				// (e.g. closed cargo door rendering as opening).
				int atlasTileIdx = -1;
				MapData* md = tile->getMapData(part);
				if (md)
				{
					int frameIdx = tile->getCurrentFrame(part);
					int animPCK = md->getSprite(frameIdx);
					if (animPCK > 0)
					{
						auto it = spec->pckToAtlas.find(animPCK);
						if (it != spec->pckToAtlas.end())
							atlasTileIdx = it->second;
					}
					if (atlasTileIdx < 0)
					{
						auto it = spec->frameMap.find(mcdIdx);
						if (it != spec->frameMap.end())
							atlasTileIdx = it->second;
					}
				}
				if (atlasTileIdx < 0) continue;

				const int   atlasCol = atlasTileIdx % spec->columns;
				const int   atlasRow = atlasTileIdx / spec->columns;
				const float atlasU   = atlasCol * grp.tileUVW;
				const float atlasV   = atlasRow * grp.tileUVH;

				int shade = (part == O_WESTWALL || part == O_NORTHWALL)
				            ? getWallShade(part, tile) : tileShade;
				// shade==16 = "fully undiscovered, render as fully black" in
				// CPU painter (ShadeTable::get returns _black for shade>=16).
				// We DO emit these tiles — they must occlude what's behind, just
				// like the painter's opaque-black blit does — and the fragment
				// shader paints them solid black when v_shade >= 16.
				// (Earlier `if (shade >= 16) continue;` made GPU skip canopy
				// silhouettes, leaving alien-sub interior bleed through the
				// canopy area on Battlescape view-level 2 — the railings-over-
				// canopy bug investigated in phase-14-railings-debug.md.)

				// Iso priority: closer-to-camera = larger value.
				// Layout: z*65536 + y*1024 + x*8 + part_priority.
				// y_mul (1024) > x_max*8+part_max (60*8+6=486) so y strictly
				// dominates — cells in different rows never collide. z_mul
				// (65536) > y_max*1024 = 60*1024 = 61440 likewise.
				// Part order within a cell:
				//   FLOOR=0 → walls=1 → back-tile OBJECT=2 (under units, e.g.
				//   submarine roof / hull back-walls) → floor item=3 → unit=4 →
				//   held item=5 → front-tile OBJECT=6 (over units in same cell).
				// O_OBJECT splits on tile->isBackTileObject(), matching vanilla
				// CPU painters' two-pass scheme (back-pass before unit, front
				// pass after unit). Across cells, the y/x components of prio
				// dominate, so a back-tile object in a higher-Y row still
				// renders over a front-tile object in a lower-Y row — same as
				// CPU painters (later iter draws over earlier iter).
				int partPrio;
				switch (part)
				{
					case O_FLOOR:     partPrio = 0; break;
					case O_WESTWALL:  partPrio = 1; break;
					case O_NORTHWALL: partPrio = 1; break;
					case O_OBJECT:    partPrio = tile->isBackTileObject(part) ? 2 : 6; break;
					default:          partPrio = 2; break;
				}
				const int prio = itZ * 65536 + itY * 1024 + mapPos.x * 8 + partPrio;
				const float iso = (float)prio / 2000000.0f;

				TileInstance inst;
				inst.screenX       = (float)(screenPos.x + mapOffsetX);
				inst.screenY       = (float)(screenPos.y - tile->getYOffset(part) + mapOffsetY);
				inst.atlasU        = atlasU;
				inst.atlasV        = atlasV;
				inst.shade         = (float)shade;
				inst.animFrameCount = 1.0f;
				inst.alphaMask     = 1.0f;
				inst.iso           = iso;
				grp.instances.push_back(inst);

				// Phase 17: hybrid overlay — emit a second instance with a micro iso
				// bump so the overlay draws strictly on top of the baseline in the
				// depth buffer (depthMask=OFF in drawTileGLPass overlay pass).
				// Phase 21/22: Wang lookup (bake variant UV / runtime blend bucket).
				if (grp.overlayAtlas)
				{
					TileInstance ov = inst;
					ov.iso = inst.iso + (0.5f / 2000000.0f);

					// One hdTilesByCell lookup for all per-cell work below (sub-layers,
					// Wang, anchor/zBias). Sub-layers use the *original* atlasTileIdx so
					// a Wang-variant swap does not redirect them to the wrong texel (§21.3.2).
					int atlasTileIdx_overlay = atlasTileIdx;
					bool pushToBlend = false;
					auto hdIt = spec->hdTilesByCell.find(atlasTileIdx);
					if (hdIt != spec->hdTilesByCell.end())
					{
						const Mod::HDTileSpec& ts = spec->hdTiles[hdIt->second];
						const float scaleX = (spec->tileWidth  > 0)
						    ? (float)_spriteWidth  / (float)spec->tileWidth  : 1.0f;
						const float scaleY = (spec->tileHeight > 0)
						    ? (float)_spriteHeight / (float)spec->tileHeight : 1.0f;
						const float zStep = (spec->tileHeight > 0)
						    ? 1.0f / (float)spec->tileHeight : 0.0f;
						if (ts.anchor[0] || ts.anchor[1])
						{
							ov.screenX += (float)ts.anchor[0] * scaleX;
							ov.screenY += (float)ts.anchor[1] * scaleY;
						}
						if (ts.zBias)
						{
							ov.iso += (float)ts.zBias * zStep / 2000000.0f;
						}
						// Phase 20.5: sub-layer instances (always use original atlasTileIdx UVs).
						for (size_t li = 0; li < ts.subLayers.size(); ++li)
						{
							if (li >= grp.subLayerInstances.size()) break;
							const Mod::HDTileSpec& sub = ts.subLayers[li];
							// Phase 22.7: state-gated sub-layer — skip a damage overlay
							// whose battle-state condition the tile does not currently
							// meet (e.g. a scorch layer only while the tile burns).
							if (sub.condition == Mod::HDTileSpec::COND_FIRE && tile->getFire() <= 0)
								continue;
							TileInstance sl = ov;
							if (sub.anchor[0] || sub.anchor[1])
							{
								sl.screenX += (float)sub.anchor[0] * scaleX;
								sl.screenY += (float)sub.anchor[1] * scaleY;
							}
							if (sub.zBias)
							{
								sl.iso += (float)sub.zBias * zStep / 2000000.0f;
							}
							grp.subLayerInstances[li].push_back(sl);
						}
						// Phase 21/22: Wang lookup — only on floor/object wang-eligible cells.
						if ((part == O_FLOOR || part == O_OBJECT)
						 && spec->effectiveWangTypeId(ts) >= 0
						 && spec->effectiveTilePart(ts) == part)
						{
							const auto wang = mod->computeWangMask(spec, tile, _save);
							if (wang.blend && wang.surfaceCell >= 0)
							{
								// Phase 22: runtime blend → BlendInstance bucket.
								BlendInstance b;
								b.screenX = ov.screenX;
								b.screenY = ov.screenY;
								b.iso     = ov.iso;
								b.shade   = (float)shade;
								b.alphaMask      = 1.0f;
								b.animFrameCount = 1.0f;
								// Self UV: anti-repeat variant if authored (§22.5).
								// metaCell (atlasTileIdx) stays the Wang eligibility key;
								// visualCell changes only the rendered UV.
								int visualCell = atlasTileIdx;
								if (ts.variants > 1)
									visualCell = atlasTileIdx
									    + (int)(hash3(mapPos.x, mapPos.y, mapPos.z)
									             % (uint32_t)ts.variants);
								b.selfU = (float)(visualCell % spec->columns) * grp.tileUVW;
								b.selfV = (float)(visualCell / spec->columns) * grp.tileUVH;
								// Neighbour UV: anti-repeat via neighbour tile's hash.
								int nbrCell = wang.surfaceCell;
								if (wang.matched && wang.matched->surfaceVariants > 1)
								{
									Position nPos(mapPos.x + wang.neighbourDx,
									              mapPos.y + wang.neighbourDy, mapPos.z);
									nbrCell += (int)(hash3(nPos.x, nPos.y, nPos.z)
									                  % (uint32_t)wang.matched->surfaceVariants);
								}
								b.neighbourU = (float)(nbrCell % spec->columns) * grp.tileUVW;
								b.neighbourV = (float)(nbrCell / spec->columns) * grp.tileUVH;
								b.worldX   = (float)mapPos.x;
								b.worldY   = (float)mapPos.y;
								b.wangMask = (uint32_t)wang.mask;
								b.feather    = wang.matched ? wang.matched->feather    : 0.18f;
								b.noiseScale = wang.matched ? wang.matched->noiseScale : 2.71f;
								b.noiseAmp   = wang.matched ? wang.matched->noiseAmp   : 0.35f;
								grp.blendInstances.push_back(b);
								pushToBlend = true;
							}
							else if (wang.variantCell >= 0)
							{
								// Phase 21: bake path — swap overlay UV to the variant cell.
								atlasTileIdx_overlay = wang.variantCell;
							}
						}
						// Phase 22 anti-repeat (§22.5): a uniform overlay cell scatters
						// across its pool [cell..cell+variants-1] by a stable position
						// hash — only when no bake-Wang variant already overrode it.
						if (ts.variants > 1 && atlasTileIdx_overlay == atlasTileIdx)
							atlasTileIdx_overlay = atlasTileIdx +
								(int)(hash3(mapPos.x, mapPos.y, mapPos.z) % (uint32_t)ts.variants);
					}
					if (!pushToBlend)
					{
						if (atlasTileIdx_overlay != atlasTileIdx)
						{
							const int ovCol = atlasTileIdx_overlay % spec->columns;
							const int ovRow = atlasTileIdx_overlay / spec->columns;
							ov.atlasU = (float)ovCol * grp.tileUVW;
							ov.atlasV = (float)ovRow * grp.tileUVH;
						}
						grp.overlayInstances.push_back(ov);
					}
				}

				if (dumpEmit)
				{
					int yOff = tile->getYOffset(part);
					int bigW = md ? md->getBigWall() : -1;
					int isBack = tile->isBackTileObject(part) ? 1 : 0;
					Log(LOG_INFO) << "[DUMP-EMIT] z=" << itZ
					              << " y=" << itY
					              << " x=" << mapPos.x
					              << " part=" << (int)part
					              << " bigW=" << bigW
					              << " back=" << isBack
					              << " prio=" << prio
					              << " yOff=" << yOff
					              << " shade=" << shade
					              << " sx=" << (int)inst.screenX
					              << " sy=" << (int)inst.screenY
					              << " mds=" << mds->getName()
					              << " mcd=" << mcdIdx
					              << " atlas=" << atlasTileIdx;
				}
			}

			// Block 11.9: collect tile smoke/fire instance for GPU smoke pass.
			if (smokeSet && tile->getSmoke() && tile->isDiscovered(O_FLOOR))
			{
				int frameNumber = 0;
				float gpuDarken = 0.0f;
				if (!tile->getFire())
				{
					if (_save->getDepth() > 0)
						frameNumber += Mod::UNDERWATER_SMOKE_OFFSET;
					else
						frameNumber += Mod::SMOKE_OFFSET;
					if (Mod::EXTENDED_SMOKE_OFFSET == 0)
						frameNumber += int(floor((tile->getSmoke() / 6.0) - 0.1));
					else if (Mod::EXTENDED_SMOKE_OFFSET == 1)
						frameNumber += int(floor((tile->getSmoke() / 6.0) - 0.1)) * 4;
					else
						frameNumber += (tile->getSmoke() - 1) / 5 * 4;
					gpuDarken = std::min(1.0f, tileShade * (1.0f / 15.0f));
				}
				const int anim = halfAnimFrame + tile->getAnimationOffset();
				frameNumber += (anim > 3) ? (anim - 4) : anim;

				SmokeInstance si;
				si.screenX  = screenPos.x + mapOffsetX;
				si.screenY  = screenPos.y + mapOffsetY;
				si.set      = smokeSet;
				si.frameIdx = frameNumber;
				si.darken   = gpuDarken;
				_smokeInstances.push_back(si);
			}
		}
	}

	// Commit the final row's slices.
	flushRowSlice();
}

/**
 * Phase 14.2: clear unit instance buffers before drawTerrainOverlayCPU populates them.
 */
void Map::emitUnitPass()
{
	for (auto& g : _unitAtlasGroups) {
		g.instances.clear();
		g.zLevels.clear();
		g.yLevels.clear();
	}
}

/**
 * Phase 16: Lazy resolver for the HD cursor-numeral font.  Caches the first
 * non-null lookup into _fontHdNumbers so subsequent calls are a single member
 * read.  Re-queries Mod::getTTFFont each call until the font appears — this
 * prevents the first-save-load race where Map is constructed before the mod's
 * extraTTFFonts: block has been parsed (resulting in a permanent bitmap-font
 * fallback for that Map instance).
 */
TTFFont* Map::getHdNumberFont()
{
	if (_fontHdNumbers) return _fontHdNumbers;
	_fontHdNumbers = _game->getMod()->getTTFFont("FONT_HD_NUMBERS", false);
	return _fontHdNumbers;
}

/**
 * Phase 16: Draws a numeric value centred at (x, y) via the TTF pipeline.
 * Two blits: black shadow at +1 px offset, then the coloured text on top.
 * Both calls are cache-hits after the first frame — steady-state cost is two
 * SDL_BlitSurface calls.
 */
void Map::drawHdNumber(Surface *dest, int x, int y, int value, Uint32 colorArgb)
{
	TTFFont* font = getHdNumberFont();
	if (!font) return;
	std::string s = std::to_string(value);

	SDL_Color fg     = { (Uint8)((colorArgb >> 16) & 0xFF),
	                     (Uint8)((colorArgb >>  8) & 0xFF),
	                     (Uint8)( colorArgb        & 0xFF),
	                     (Uint8)((colorArgb >> 24) & 0xFF) };
	SDL_Color shadow = { 0, 0, 0, 220 };

	SDL_Surface* shadowSurf = font->renderText(s, shadow);
	SDL_Surface* textSurf   = font->renderText(s, fg);

	SDL_Surface* destSurf = dest->getSurface();
	if (!destSurf) return;

	// Manual ARGB8888 alpha composite: SDL_BlitSurface with TTF_Blended
	// surfaces produces correct alpha compositing on a single-pass surface,
	// but our `dest` is a 32bpp ARGB intermediate — by the time it reaches
	// Screen::flip the alpha channel is consumed as opacity, so pixels with
	// src.a=0 (TTF background) overwrite dest.a=255 → final glyph is fully
	// transparent.  Manual composite writes opaque ARGB into dest.
	auto compositeARGB = [&](SDL_Surface* src, int dstX, int dstY)
	{
		if (!src || src->format->BitsPerPixel != 32 || destSurf->format->BitsPerPixel != 32)
			return;
		SDL_LockSurface(src);
		SDL_LockSurface(destSurf);
		const int srcW = src->w, srcH = src->h;
		const int dW = destSurf->w, dH = destSurf->h;
		for (int j = 0; j < srcH; ++j)
		{
			const int dy = dstY + j;
			if (dy < 0 || dy >= dH) continue;
			Uint32* srcRow = (Uint32*)((Uint8*)src->pixels + j * src->pitch);
			Uint32* dstRow = (Uint32*)((Uint8*)destSurf->pixels + dy * destSurf->pitch);
			for (int i = 0; i < srcW; ++i)
			{
				const int dx = dstX + i;
				if (dx < 0 || dx >= dW) continue;
				Uint8 sr, sg, sb, sa;
				SDL_GetRGBA(srcRow[i], src->format, &sr, &sg, &sb, &sa);
				if (sa == 0) continue;
				Uint8 dr, dg, db, da;
				SDL_GetRGBA(dstRow[dx], destSurf->format, &dr, &dg, &db, &da);
				const int a = sa;
				const int ia = 255 - a;
				const Uint8 outR = (Uint8)((sr * a + dr * ia) / 255);
				const Uint8 outG = (Uint8)((sg * a + dg * ia) / 255);
				const Uint8 outB = (Uint8)((sb * a + db * ia) / 255);
				dstRow[dx] = SDL_MapRGBA(destSurf->format, outR, outG, outB, 0xFF);
			}
		}
		SDL_UnlockSurface(destSurf);
		SDL_UnlockSurface(src);
	};

	if (shadowSurf)
		compositeARGB(shadowSurf, x - shadowSurf->w / 2 + 1, y - shadowSurf->h / 2 + 1);
	if (textSurf)
		compositeARGB(textSurf, x - textSurf->w / 2, y - textSurf->h / 2);
}

/**
 * Phase 14.2: legacy single-pass unit draw — kept as no-op since
 * drawTileGLPass now interleaves unit draws via drawUnitsAtZ().
 */
void Map::drawUnitGLPass() {}

/**
 * Draw unit instances at the given (Z, Y) row. Filters _unitAtlasGroups
 * instances by (zLevels[i], yLevels[i]) == (z, y). Called from drawTileGLPass
 * between tile (Z, Y) row slices.
 */
void Map::drawUnitsAtZY(int z, int y, Shader*& activeShader)
{
	if (!_shadeTableTex || !_shadeTableTex->isValid()) return;
	if (!_tileShader   || !_tileShader->isValid())   return;

	const float SW = (float)Options::baseXResolution;
	const float SH = (float)Options::baseYResolution;

	// Iterate groups in two passes: floor items first (FLOOROB.PCK), then unit
	// bodies + held items (everything else). Within a (Z, Y) row this gives:
	// floor items → unit bodies → unit held items, matching iso order.
	const Mod::UnitAtlasSpec* floorSpec  = _game->getMod()->getUnitAtlas("FLOOROB.PCK");

	auto drawGroup = [&](UnitAtlasGroup& g) {
		if (g.instances.empty() || !g.spec || !g.spec->atlas) return;
		if (g.zLevels.size() != g.instances.size()) return;
		if (g.yLevels.size() != g.instances.size()) return;

		std::vector<TileInstance> scratch;
		scratch.reserve(g.instances.size());
		for (size_t i = 0; i < g.instances.size(); ++i)
			if (g.zLevels[i] == z && g.yLevels[i] == y)
				scratch.push_back(g.instances[i]);
		if (scratch.empty()) return;

		if (activeShader != _tileShader)
		{
			_tileShader->use();
			_tileShader->setUniform2f("u_screenSize",    SW, SH);
			_tileShader->setUniform2f("u_tilePixelSize", (float)_spriteWidth, (float)_spriteHeight);
			_tileShader->setUniform1f("u_animFrame",     0.0f);
			_tileShader->setUniform1i("u_atlas",         0);
			_tileShader->setUniform1i("u_shadeTable",    1);
			_shadeTableTex->bind(1);
			activeShader = _tileShader;
		}

		const float uvW = (float)g.spec->tileWidth  / (float)g.spec->atlasW;
		const float uvH = (float)g.spec->tileHeight / (float)g.spec->atlasH;
		g.spec->atlas->bind(0);
		_tileShader->setUniform2f("u_tileUVSize", uvW, uvH);
		glBindBuffer(GL_ARRAY_BUFFER, _tileIBO);
		glBufferData(GL_ARRAY_BUFFER,
		             (GLsizeiptr)(scratch.size() * sizeof(TileInstance)),
		             scratch.data(),
		             GL_DYNAMIC_DRAW);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)scratch.size());
	};

	for (auto& g : _unitAtlasGroups)
		if (g.spec == floorSpec) drawGroup(g);

	for (auto& g : _unitAtlasGroups)
		if (g.spec != floorSpec) drawGroup(g);
}

/**
 * Initialise VAO/VBO/IBO for instanced tile draw and compile tile_atlas shader.
 * Called lazily on the first drawTileGLPass() invocation.
 */
void Map::initTileGL()
{
	if (_tileGLInit) return;
	_tileGLInit = true;

	// Phase 22 (M2): the VAO setup below hardcodes byte strides/offsets that assume
	// these structs are tightly packed (no padding). Lock that assumption at compile
	// time so a future field reorder/insertion fails the build instead of silently
	// desynchronising the GPU instance read.
	static_assert(sizeof(TileInstance) == 32,
	              "tile VAO stride assumes a 32-byte TileInstance (8 floats)");
	static_assert(sizeof(BlendInstance) == 64,
	              "blend VAO stride assumes a 64-byte BlendInstance (16 x 4 bytes)");
	static_assert(offsetof(BlendInstance, wangMask) == 32,
	              "blend attr loc 5 (glVertexAttribIPointer) reads wangMask at offset 32");
	static_assert(offsetof(BlendInstance, shade) == 36,
	              "blend attr loc 6 reads shade at offset 36");
	static_assert(offsetof(BlendInstance, noiseAmp) == 60,
	              "blend attr loc 12 reads noiseAmp at offset 60");

	// On context restore these are already rebuilt by ShaderManager::reuploadAll();
	// only allocate on the very first call.
	if (!_tileShader)
	{
		_tileShader = new Shader();
		if (!_tileShader->loadFromEmbedded("tile_atlas"))
		{
			Log(LOG_ERROR) << "Map::initTileGL: tile_atlas shader compile failed";
			delete _tileShader; _tileShader = nullptr;
			return;
		}
	}
	if (!_tileShaderRgba)
	{
		_tileShaderRgba = new Shader();
		if (!_tileShaderRgba->loadFromEmbedded("tile_atlas_rgba"))
		{
			Log(LOG_ERROR) << "Map::initTileGL: tile_atlas_rgba shader compile failed";
			delete _tileShaderRgba; _tileShaderRgba = nullptr;
			// Non-fatal: palette path still works; RGBA atlases just won't render.
		}
	}
	// Phase 22: runtime blend shader.
	if (!_blendShader)
	{
		_blendShader = new Shader();
		if (!_blendShader->loadFromEmbedded("tile_blend"))
		{
			Log(LOG_ERROR) << "Map::initTileGL: tile_blend shader compile failed";
			delete _blendShader; _blendShader = nullptr;
			// Non-fatal: bake path continues; blend tiles fall back to base overlay.
		}
	}
	// Phase 22: 256×256 tileable R8 noise texture (GL_REPEAT + GL_LINEAR).
	// R8 saves 75% VRAM vs RGBA8 (64 KB vs 256 KB); uploadR8 respects
	// Wrap::Repeat and Filter::Linear set on construction.
	// M2: periodic coherent value-noise — a 32-cell random lattice, smoothstep-
	// interpolated up to 256×256, lattice indices wrapped mod 32. The wrap makes
	// the field exactly periodic over 256 px, so GL_REPEAT tiles it with NO seam,
	// and smoothstep interpolation gives an organic wobble (the previous per-texel
	// white noise read as sparkle). surface-gen.py `noise` emits an equivalent PNG
	// for Python-preview parity (DoD #7); swap to that asset when authored.
	if (!_noiseTex)
	{
		constexpr int N = 256, LAT = 32;          // 256 % LAT == 0 ⇒ seamless under REPEAT
		auto lattice = [](int ix, int iy) -> float {
			uint32_t h = hash3(ix & (LAT - 1), iy & (LAT - 1), 42u);  // wrap ⇒ periodic
			return (float)(h & 0xFFFFu) / 65535.0f;
		};
		auto smooth = [](float t) { return t * t * (3.0f - 2.0f * t); };
		std::vector<uint8_t> noiseData(N * N, 0u);
		for (int ny = 0; ny < N; ++ny)
			for (int nx = 0; nx < N; ++nx)
			{
				float gx = nx * (float)LAT / (float)N, gy = ny * (float)LAT / (float)N;
				int ix = (int)gx, iy = (int)gy;
				float tx = smooth(gx - ix), ty = smooth(gy - iy);
				float a = lattice(ix,     iy),     b = lattice(ix + 1, iy);
				float c = lattice(ix,     iy + 1), d = lattice(ix + 1, iy + 1);
				float v = a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty;
				noiseData[ny * N + nx] = (uint8_t)(v * 255.0f + 0.5f);
			}
		_noiseTex = new GpuTexture(false, GpuTexture::Wrap::Repeat, GpuTexture::Filter::Linear);
		_noiseTex->uploadR8(noiseData.data(), N, N);
	}

	static const float corners[] = {
		0.f, 0.f,  1.f, 0.f,  0.f, 1.f,
		0.f, 1.f,  1.f, 0.f,  1.f, 1.f,
	};

	glGenVertexArrays(1, &_tileVAO);
	glBindVertexArray(_tileVAO);

	// attr 0 — corner (vec2, per-vertex)
	glGenBuffers(1, &_tileVBO);
	glBindBuffer(GL_ARRAY_BUFFER, _tileVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glVertexAttribDivisor(0, 0);

	// attrs 1–6 — per-instance (stride = 8 floats = 32 bytes; +1 float for iso)
	glGenBuffers(1, &_tileIBO);
	glBindBuffer(GL_ARRAY_BUFFER, _tileIBO);
	const GLsizei stride = 8 * (GLsizei)sizeof(float);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const void*)0);
	glVertexAttribDivisor(1, 1);

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(2 * sizeof(float)));
	glVertexAttribDivisor(2, 1);

	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(4 * sizeof(float)));
	glVertexAttribDivisor(3, 1);

	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(5 * sizeof(float)));
	glVertexAttribDivisor(4, 1);

	glEnableVertexAttribArray(5);
	glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(6 * sizeof(float)));
	glVertexAttribDivisor(5, 1);

	glEnableVertexAttribArray(6);
	glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(7 * sizeof(float)));
	glVertexAttribDivisor(6, 1);

	glBindVertexArray(0);

	// Phase 22 (H1): terrain instance VAO/buffer — identical TileInstance layout to
	// _tileVAO/_tileIBO, but persistent and dirty-gated. Baseline/overlay/sub-layer
	// draws re-point attrs 1–6 to a per-(group,list) byte offset within this one
	// buffer (concatenated in drawTileGLPass), so a clean frame issues zero uploads.
	// Units keep streaming into _tileIBO via drawAtlas, so they are unaffected.
	glGenVertexArrays(1, &_tileInstVAO);
	glBindVertexArray(_tileInstVAO);
	// attr 0 — corner (per-vertex, shared corner VBO)
	glBindBuffer(GL_ARRAY_BUFFER, _tileVBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glVertexAttribDivisor(0, 0);
	// attrs 1–6 — per-instance, base offset 0 here; re-pointed per sub-draw.
	glGenBuffers(1, &_tileInstIBO);
	glBindBuffer(GL_ARRAY_BUFFER, _tileInstIBO);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const void*)0);
	glVertexAttribDivisor(1, 1);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(2 * sizeof(float)));
	glVertexAttribDivisor(2, 1);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(4 * sizeof(float)));
	glVertexAttribDivisor(3, 1);
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(5 * sizeof(float)));
	glVertexAttribDivisor(4, 1);
	glEnableVertexAttribArray(5);
	glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(6 * sizeof(float)));
	glVertexAttribDivisor(5, 1);
	glEnableVertexAttribArray(6);
	glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(7 * sizeof(float)));
	glVertexAttribDivisor(6, 1);
	glBindVertexArray(0);
	_tileBuffersDirty = true;  // force a rebuild on the first draw after (re)init

	// Phase 22: blend VAO — same corner VBO, different per-instance layout.
	// BlendInstance stride = 64 bytes; uint wangMask at offset 32 needs IPointer.
	// Attribute → BlendInstance field mapping (all per-instance except loc 0):
	//   0 corner        vec2  per-vertex (reuse _tileVBO)
	//   1 screenPos     vec2  offset  0
	//   2 selfUV        vec2  offset  8
	//   3 neighbourUV   vec2  offset 16
	//   4 worldPos      vec2  offset 24
	//   5 wangMask      uint  offset 32  (IPointer — bitwise ops in frag)
	//   6 shade         float offset 36
	//   7 alphaMask     float offset 40
	//   8 iso           float offset 44
	//   9 animFrameCount float offset 48
	//  10 feather       float offset 52
	//  11 noiseScale    float offset 56
	//  12 noiseAmp      float offset 60
	glGenVertexArrays(1, &_blendVAO);
	glBindVertexArray(_blendVAO);

	// attr 0 — corner (per-vertex, shared VBO)
	glBindBuffer(GL_ARRAY_BUFFER, _tileVBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glVertexAttribDivisor(0, 0);

	// per-instance attrs 1–12 from _blendIBO
	glGenBuffers(1, &_blendIBO);
	glBindBuffer(GL_ARRAY_BUFFER, _blendIBO);
	const GLsizei bStride = 64;  // sizeof(BlendInstance)
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, bStride, (const void*)0);
	glVertexAttribDivisor(1, 1);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, bStride, (const void*)8);
	glVertexAttribDivisor(2, 1);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, bStride, (const void*)16);
	glVertexAttribDivisor(3, 1);
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, bStride, (const void*)24);
	glVertexAttribDivisor(4, 1);
	glEnableVertexAttribArray(5);
	glVertexAttribIPointer(5, 1, GL_UNSIGNED_INT, bStride, (const void*)32);
	glVertexAttribDivisor(5, 1);
	glEnableVertexAttribArray(6);
	glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, bStride, (const void*)36);
	glVertexAttribDivisor(6, 1);
	glEnableVertexAttribArray(7);
	glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, bStride, (const void*)40);
	glVertexAttribDivisor(7, 1);
	glEnableVertexAttribArray(8);
	glVertexAttribPointer(8, 1, GL_FLOAT, GL_FALSE, bStride, (const void*)44);
	glVertexAttribDivisor(8, 1);
	glEnableVertexAttribArray(9);
	glVertexAttribPointer(9, 1, GL_FLOAT, GL_FALSE, bStride, (const void*)48);
	glVertexAttribDivisor(9, 1);
	glEnableVertexAttribArray(10);
	glVertexAttribPointer(10, 1, GL_FLOAT, GL_FALSE, bStride, (const void*)52);
	glVertexAttribDivisor(10, 1);
	glEnableVertexAttribArray(11);
	glVertexAttribPointer(11, 1, GL_FLOAT, GL_FALSE, bStride, (const void*)56);
	glVertexAttribDivisor(11, 1);
	glEnableVertexAttribArray(12);
	glVertexAttribPointer(12, 1, GL_FLOAT, GL_FALSE, bStride, (const void*)60);
	glVertexAttribDivisor(12, 1);

	glBindVertexArray(0);
	Log(LOG_INFO) << "Map::initTileGL: tile rendering pipeline ready (with iso depth attr)";
}

/**
 * Issue glDrawArraysInstanced per atlas group, Z-level outer (Phase 13.2).
 * Fires before SDL composite via registerGPUPassPreComposite so HD floor
 * renders under units / walls / HUD that the CPU draws to the SDL surface.
 */
void Map::drawTileGLPass()
{
	if (!_tileGLInit) initTileGL();
	if (!_tileShader || !_tileShader->isValid()) return;
	if (!_shadeTableTex || !_shadeTableTex->isValid()) return;

	const Uint32 ticks = SDL_GetTicks();
	if (ticks - _lastDrawnTicks > 250)
	{
		// Map not blitted recently; clear all GPU instances to avoid ghosting in menus.
		for (auto& grp : _tileAtlasGroups) {
			grp.instances.clear(); grp.overlayInstances.clear();
			grp.blendInstances.clear();  // Phase 22
			for (auto& sv : grp.subLayerInstances) sv.clear();
		}
		for (auto& grp : _unitAtlasGroups) { grp.instances.clear(); grp.zLevels.clear(); grp.yLevels.clear(); }
		_cursorOverlayInstances.clear();
		_smokeInstances.clear();
		_tileBuffersDirty = true;  // Phase 22 (H1): lists emptied — next emit rebuilds the buffer
		return;
	}

	bool hasAny = false;
	for (auto& grp : _tileAtlasGroups)
		if (!grp.instances.empty()) { hasAny = true; break; }
	if (!hasAny)
	{
		for (auto& grp : _unitAtlasGroups)
			if (!grp.instances.empty()) { hasAny = true; break; }
	}
	if (!hasAny) return;

	_animFrameGPU = static_cast<float>(ticks % TILE_ANIM_PERIOD_MS)
	                / static_cast<float>(TILE_ANIM_PERIOD_MS);

	// Same coordinate convention as drawUnitGLPass — iso projection lives in
	// base-resolution pixels, GPU u_screenSize = base resolution.
	const float SW = (float)Options::baseXResolution;
	const float SH = (float)Options::baseYResolution;

	// Iso ordering is resolved on the GPU via depth-test using each instance's
	// `iso` priority (set in emit). No (Z, Y) bucketing needed — each atlas
	// group is rendered with a single instanced draw call and the depth buffer
	// sorts overlapping instances correctly.

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glClearDepthf(1.0f);
	// Reset colorMask BEFORE clear so alpha is also cleared to 1.0 (else
	// canvas alpha stays from prev frame and composites with page bg).
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClearColor(0x46/255.0f, 0x8A/255.0f, 0x9A/255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindVertexArray(_tileVAO);

	Shader* activeShader = nullptr;

	auto drawAtlas = [&](GpuTexture* atlas, float uvW, float uvH,
	                     const TileInstance* data, size_t count, bool isRgba) {
		Shader* sh = isRgba ? _tileShaderRgba : _tileShader;
		if (!sh || !sh->isValid()) return;
		// P17: explicit bind so blend draw's _blendVAO doesn't leak into this call.
		glBindVertexArray(_tileVAO);
		if (sh != activeShader)
		{
			sh->use();
			sh->setUniform2f("u_screenSize",    SW, SH);
			sh->setUniform2f("u_tilePixelSize", (float)_spriteWidth, (float)_spriteHeight);
			sh->setUniform1f("u_animFrame",     _animFrameGPU);
			sh->setUniform1i("u_atlas",         0);
			if (!isRgba)
			{
				sh->setUniform1i("u_shadeTable", 1);
				_shadeTableTex->bind(1);
			}
			else if (_shadeCurveTex && _shadeCurveTex->isValid())
			{
				// Phase 22 §22.4: shade ramp for RGBA overlay path (P5 — no night seam).
				sh->setUniform1i("u_shadeCurve", 3);
				_shadeCurveTex->bind(3);
			}
			activeShader = sh;
		}
		atlas->bind(0);
		sh->setUniform2f("u_tileUVSize", uvW, uvH);
		glBindBuffer(GL_ARRAY_BUFFER, _tileIBO);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * sizeof(TileInstance)),
		             data, GL_DYNAMIC_DRAW);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)count);
	};

	// Phase 22 (H1): rebuild the persistent terrain instance buffer only when an
	// emit (or group/context change) marked it dirty. Concatenate every group's
	// baseline + overlay + sub-layer lists into one buffer and record per-list
	// instance offsets. Clean frames skip this entirely and draw from the resident
	// buffer, so steady-state (camera still, animation via u_animFrame uniform)
	// issues zero terrain re-uploads. Units still stream into _tileIBO below.
	if (_tileBuffersDirty)
	{
		_tileInstUpload.clear();
		for (auto& grp : _tileAtlasGroups)
		{
			grp.baselineOffset = _tileInstUpload.size();
			_tileInstUpload.insert(_tileInstUpload.end(),
			                       grp.instances.begin(), grp.instances.end());
			grp.overlayOffset = _tileInstUpload.size();
			_tileInstUpload.insert(_tileInstUpload.end(),
			                       grp.overlayInstances.begin(), grp.overlayInstances.end());
			grp.subLayerOffsets.assign(grp.subLayerInstances.size(), 0);
			for (size_t li = 0; li < grp.subLayerInstances.size(); ++li)
			{
				grp.subLayerOffsets[li] = _tileInstUpload.size();
				_tileInstUpload.insert(_tileInstUpload.end(),
				                       grp.subLayerInstances[li].begin(),
				                       grp.subLayerInstances[li].end());
			}
		}
		// Unbind any VAO first so this is a pure buffer upload: GL_ARRAY_BUFFER is
		// global (not VAO state), but keeping the upload VAO-neutral means a future
		// glVertexAttribPointer added here can never silently re-point _tileVAO's
		// unit-draw attrs at _tileInstIBO.
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, _tileInstIBO);
		glBufferData(GL_ARRAY_BUFFER,
		             (GLsizeiptr)(_tileInstUpload.size() * sizeof(TileInstance)),
		             _tileInstUpload.empty() ? nullptr : _tileInstUpload.data(),
		             GL_DYNAMIC_DRAW);
		_tileBuffersDirty = false;
	}

	// Phase 22 (H1): re-point the terrain VAO's per-instance attrs (locs 1–6) to a
	// byte offset within _tileInstIBO. Caller must bind _tileInstVAO + _tileInstIBO.
	auto pointTileInstanceAttribs = [](size_t baseInstance) {
		const GLsizei  stride = 8 * (GLsizei)sizeof(float);
		const GLintptr base   = (GLintptr)(baseInstance * sizeof(TileInstance));
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(base + 0));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(base + 2 * sizeof(float)));
		glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(base + 4 * sizeof(float)));
		glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(base + 5 * sizeof(float)));
		glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(base + 6 * sizeof(float)));
		glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(base + 7 * sizeof(float)));
	};

	// Phase 22 (H1): draw one terrain list from the resident _tileInstIBO at the
	// given instance offset — no per-draw upload. Shares activeShader/uniform
	// tracking with drawAtlas (uniforms are program state, independent of the VAO).
	auto drawTileGroup = [&](GpuTexture* atlas, float uvW, float uvH,
	                         size_t baseInstance, size_t count, bool isRgba) {
		if (!atlas || count == 0) return;
		Shader* sh = isRgba ? _tileShaderRgba : _tileShader;
		if (!sh || !sh->isValid()) return;
		glBindVertexArray(_tileInstVAO);
		if (sh != activeShader)
		{
			sh->use();
			sh->setUniform2f("u_screenSize",    SW, SH);
			sh->setUniform2f("u_tilePixelSize", (float)_spriteWidth, (float)_spriteHeight);
			sh->setUniform1f("u_animFrame",     _animFrameGPU);
			sh->setUniform1i("u_atlas",         0);
			if (!isRgba)
			{
				sh->setUniform1i("u_shadeTable", 1);
				_shadeTableTex->bind(1);
			}
			else if (_shadeCurveTex && _shadeCurveTex->isValid())
			{
				sh->setUniform1i("u_shadeCurve", 3);
				_shadeCurveTex->bind(3);
			}
			activeShader = sh;
		}
		atlas->bind(0);
		sh->setUniform2f("u_tileUVSize", uvW, uvH);
		glBindBuffer(GL_ARRAY_BUFFER, _tileInstIBO);
		pointTileInstanceAttribs(baseInstance);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)count);
	};

	// Lock alpha for the entire tile-grid pass: WebGL canvas alpha < 1 at
	// edge pixels causes the page background to mix in and darken the seam.
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
	// Tiles (terrain MDS atlases) — one draw call per group, from _tileInstIBO.
	for (auto& grp : _tileAtlasGroups)
	{
		if (!grp.atlas || grp.instances.empty()) continue;
		drawTileGroup(grp.atlas, grp.tileUVW, grp.tileUVH,
		              grp.baselineOffset, grp.instances.size(), grp.isRgba);
	}
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	// Units / floor items / held items (palette unit atlases) — one draw call
	// per group; depth test handles inter-group ordering.
	for (auto& g : _unitAtlasGroups)
	{
		if (!g.spec || !g.spec->atlas || g.instances.empty()) continue;
		const float uvW = (float)g.spec->tileWidth  / (float)g.spec->atlasW;
		const float uvH = (float)g.spec->tileHeight / (float)g.spec->atlasH;
		drawAtlas(g.spec->atlas, uvW, uvH,
		          g.instances.data(), g.instances.size(), false);
	}

	// Phase 17: hybrid RGBA overlay pass — drawn after tiles and units with
	// depth-write disabled so the depth buffer (already written by baseline and
	// units) determines occlusion.  Alpha-discard in tile_atlas_rgba.frag
	// ensures vanilla cells (transparent in overlay) show through to baseline.
	glDepthMask(GL_FALSE);
	// Canvas alpha-channel protection.
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
	// Overlay needs GL_LEQUAL: iso bump (0.5/2000000 ≈ 2.5e-7) is below
	// 24-bit depth-buffer float precision, so GL_LESS treats overlay
	// fragments as equal-or-greater than baseline and rejects them at
	// shared edge pixels. LEQUAL lets the overlay (drawn after baseline)
	// claim those pixels and overwrite the palette-shaded R8 color.
	glDepthFunc(GL_LEQUAL);
	// Phase 20.4: track active blend func to avoid redundant GL calls.
	bool curPremult = false; // false = straight alpha (GL_SRC_ALPHA)
	for (auto& grp : _tileAtlasGroups)
	{
		if (!grp.overlayAtlas || grp.overlayInstances.empty()) continue;
		// Switch blend func only when premult mode changes between groups.
		if (grp.premultipliedAlpha != curPremult)
		{
			curPremult = grp.premultipliedAlpha;
			if (curPremult)
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied
			else
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // straight
		}
		drawTileGroup(grp.overlayAtlas, grp.tileUVW, grp.tileUVH,
		              grp.overlayOffset, grp.overlayInstances.size(), /*isRgba=*/true);
	}
	// Phase 22.1 §22.8: opt-in glFinish-isolated GPU timing of the blend pass.
	// The blend pass is purely additive (it does not exist without runtime blend),
	// so its isolated GPU time IS the per-frame budget delta. The leading glFinish
	// drains all prior passes; the trailing one (below) blocks until the blend
	// fragments finish, so the CPU-side GpuTimer captures the blend GPU execution
	// alone. Both syncs stall the pipeline — measurement builds only; production
	// keeps g_calypsoProfileBattlescape == 0 → zero cost.
	const bool profBlend = ::g_calypsoProfileBattlescape != 0;
	GpuTimer blendTimer;
	size_t   blendInstCount = 0;
	if (profBlend)
	{
		for (auto& grp : _tileAtlasGroups)
			if (grp.overlayAtlas && !grp.blendInstances.empty())
				blendInstCount += grp.blendInstances.size();
		glFinish();
		blendTimer.start();
	}
	// Phase 22: runtime blend draw — same depth state as overlays (depthMask off,
	// LEQUAL).  Per-group: only fire when _blendShader is available and the group
	// has pending blend instances.  Binds _blendVAO; the overlay/sub-layer
	// drawTileGroup calls around it bind _tileInstVAO at entry (and units' drawAtlas
	// binds _tileVAO), so VAO state never leaks across passes (P17).
	if (_blendShader && _blendShader->isValid()
	 && _noiseTex && _noiseTex->isValid()
	 && _shadeCurveTex && _shadeCurveTex->isValid())
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // blend shader assumes straight alpha
		curPremult = false;  // keep sub-layer premult tracking consistent
		_blendShader->use();
		_blendShader->setUniform2f("u_screenSize",    SW, SH);
		_blendShader->setUniform2f("u_tilePixelSize", (float)_spriteWidth, (float)_spriteHeight);
		_blendShader->setUniform1f("u_animFrame",     _animFrameGPU);
		_blendShader->setUniform1i("u_atlas",         0);
		_blendShader->setUniform1i("u_noise",         2);
		_blendShader->setUniform1i("u_shadeCurve",    3);
		_noiseTex->bind(2);
		_shadeCurveTex->bind(3);
		activeShader = nullptr;  // force drawAtlas to re-bind _tileShaderRgba after this

		glBindVertexArray(_blendVAO);
		for (auto& grp : _tileAtlasGroups)
		{
			if (!grp.overlayAtlas || grp.blendInstances.empty()) continue;
			grp.overlayAtlas->bind(0);
			_blendShader->setUniform2f("u_tileUVSize", grp.tileUVW, grp.tileUVH);
			glBindBuffer(GL_ARRAY_BUFFER, _blendIBO);
			glBufferData(GL_ARRAY_BUFFER,
			             (GLsizeiptr)(grp.blendInstances.size() * sizeof(BlendInstance)),
			             grp.blendInstances.data(), GL_DYNAMIC_DRAW);
			glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)grp.blendInstances.size());
		}
		glBindVertexArray(0);
	}
	if (profBlend)
	{
		glFinish();
		blendTimer.stop();
		static long long s_accumUs = 0;
		static unsigned  s_n       = 0;
		static size_t    s_inst    = 0;
		s_accumUs += blendTimer.elapsedUs();
		s_inst     = blendInstCount;
		if (++s_n >= 30u)
		{
			Log(LOG_INFO) << "Map::blendPass avg: " << (s_accumUs / (long long)s_n)
			              << " us/frame (" << SW << "x" << SH << ", "
			              << s_inst << " blend instances, n=" << s_n
			              << ", glFinish-isolated GPU)";
			s_accumUs = 0;
			s_n       = 0;
		}
	}

	// Phase 20.5: sub-layer passes (rendered after base overlay, in layer order).
	for (size_t li = 0; li < 8; ++li)  // cap at 8 sub-layers
	{
		bool drewAny = false;
		for (auto& grp : _tileAtlasGroups)
		{
			if (li >= grp.subLayerAtlases.size()) continue;
			if (!grp.subLayerAtlases[li]) continue;
			if (li >= grp.subLayerInstances.size()) continue;
			if (grp.subLayerInstances[li].empty()) continue;
			if (grp.premultipliedAlpha != curPremult)
			{
				curPremult = grp.premultipliedAlpha;
				glBlendFunc(curPremult ? GL_ONE : GL_SRC_ALPHA,
				            GL_ONE_MINUS_SRC_ALPHA);
			}
			drawTileGroup(grp.subLayerAtlases[li], grp.tileUVW, grp.tileUVH,
			              grp.subLayerOffsets.size() > li ? grp.subLayerOffsets[li] : 0,
			              grp.subLayerInstances[li].size(), /*isRgba=*/true);
			drewAny = true;
		}
		if (!drewAny) break;  // no more sub-layers after an empty index
	}
	// Restore straight alpha for subsequent passes (units, cursor, smoke).
	if (curPremult)
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthFunc(GL_LESS);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	glBindVertexArray(0);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_BLEND);

	// NOTE: instances are NOT cleared here.  Map::draw() runs on _redraw
	// (state changed) but the registered GPU pass fires every Screen::flip;
	// clearing here would empty the buffer on every frame Map::draw doesn't
	// run, causing 1-frame-on / N-frames-black flicker as the HD floor
	// disappears between emits (visible after Phase 12.5 once RGBA tiles
	// became visually distinct from the CPU vanilla layer underneath).
	//
	// emitTilePass() already clears instances at its head (line ~2196), so
	// fresh content always replaces stale.  The hidden-movement / non-
	// Battlescape suppression case is handled explicitly by Map::draw()'s
	// else-branch, which clears instances before this pass fires (~line 558).
}

/**
 * Block 11.8: Initialise VAO/VBO for the single-sprite dynamic quad and compile
 * the "textured" shader reused from the cursor pass.  Called lazily on first use.
 */
void Map::initSpriteGL()
{
	if (_spriteGLInit) return;
	_spriteGLInit = true;

	// On context restore _spriteShader is already rebuilt by ShaderManager::reuploadAll().
	if (!_spriteShader)
	{
		_spriteShader = new Shader();
		if (!_spriteShader->loadFromEmbedded("textured"))
		{
			Log(LOG_ERROR) << "Map::initSpriteGL: textured shader compile failed";
			delete _spriteShader; _spriteShader = nullptr;
			return;
		}
	}

	// 6-vertex dynamic quad: pos.xy + uv.xy (4 floats per vertex)
	glGenVertexArrays(1, &_spriteVAO);
	glGenBuffers(1, &_spriteVBO);
	glBindVertexArray(_spriteVAO);
	glBindBuffer(GL_ARRAY_BUFFER, _spriteVBO);
	glBufferData(GL_ARRAY_BUFFER, 6 * 4 * (GLsizeiptr)sizeof(float), nullptr, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0); // a_pos
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)0);
	glEnableVertexAttribArray(1); // a_uv
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), (void*)(2 * sizeof(float)));
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	Log(LOG_DEBUG) << "Map::initSpriteGL: sprite pipeline ready";
}

/**
 * Phase 15: Create VAO/VBO/instanceVBO for the SDF cursor box shader.
 * Called lazily on first draw; safe to call again after context loss
 * (reset callback has already zeroed the handles and cleared _cursorGLInit).
 */
void Map::initCursorGL()
{
	if (_cursorGLInit) return;
	_cursorGLInit = true;

	if (!_cursorShader)
	{
		_cursorShader = new Shader();
		if (!_cursorShader->loadFromEmbedded("cursor"))
		{
			Log(LOG_ERROR) << "Map::initCursorGL: cursor shader compile failed";
			delete _cursorShader; _cursorShader = nullptr;
			return;
		}
	}

	// Static unit quad: 4 corners in [-1..1] for GL_TRIANGLE_STRIP.
	static const float kQuad[4 * 2] = {
		-1.f, -1.f,
		 1.f, -1.f,
		-1.f,  1.f,
		 1.f,  1.f,
	};

	glGenVertexArrays(1, &_cursorVAO);
	glGenBuffers(1, &_cursorVBO);
	glGenBuffers(1, &_cursorInstanceVBO);

	glBindVertexArray(_cursorVAO);

	// Location 0: a_unitPos (static unit quad, no divisor)
	glBindBuffer(GL_ARRAY_BUFFER, _cursorVBO);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(kQuad), kQuad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * (GLsizei)sizeof(float), (void*)0);
	glVertexAttribDivisor(0, 0); // one corner per vertex

	// Locations 1–3: per-instance attrs (xywh, color, params), stride = 12 floats
	glBindBuffer(GL_ARRAY_BUFFER, _cursorInstanceVBO);
	glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STREAM_DRAW); // sized at draw time
	const GLsizei kStride = 12 * (GLsizei)sizeof(float);
	// a_xywh
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, kStride, (void*)(0 * sizeof(float)));
	glVertexAttribDivisor(1, 1);
	// a_color
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, kStride, (void*)(4 * sizeof(float)));
	glVertexAttribDivisor(2, 1);
	// a_params
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, kStride, (void*)(8 * sizeof(float)));
	glVertexAttribDivisor(3, 1);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	Log(LOG_DEBUG) << "Map::initCursorGL: SDF cursor pipeline ready";
}

/**
 * Blocks 11.8–11.9: Convert a palettized Surface frame from the given SurfaceSet
 * to a linear-RGBA GpuTexture and cache it keyed by (set, frameIdx).
 * Returns nullptr if the upload fails or the frame has no palette.
 */
GpuTexture* Map::getOrUploadSpriteFrame(SurfaceSet* set, int frameIdx)
{
	if (!set) return nullptr;
	const auto key = std::make_pair(set, frameIdx);
	auto it = _spriteFrameCache.find(key);
	if (it != _spriteFrameCache.end()) return it->second;

	Surface* src = set->getFrame(frameIdx);
	if (!src) return nullptr;

	const int w = src->getWidth();
	const int h = src->getHeight();
	const SDL_Color* pal = src->getEffectivePalette();
	if (!pal || w <= 0 || h <= 0) return nullptr;

	std::vector<uint8_t> rgba(static_cast<size_t>(w * h * 4), 0u);
	for (int py = 0; py < h; ++py)
	{
		for (int px = 0; px < w; ++px)
		{
			const uint8_t idx = src->getPixel(px, py);
			if (idx == 0) continue; // palette index 0 = transparent
			const SDL_Color& c = pal[idx];
			const size_t i = static_cast<size_t>((py * w + px) * 4);
			rgba[i+0] = c.r;
			rgba[i+1] = c.g;
			rgba[i+2] = c.b;
			rgba[i+3] = 255u;
		}
	}

	GpuTexture* tex = new GpuTexture(/*srgb=*/false,
	                                 GpuTexture::Wrap::ClampToEdge,
	                                 GpuTexture::Filter::Nearest);
	if (!tex->uploadRGBA(rgba.data(), w, h))
	{
		Log(LOG_WARNING) << "Map::getOrUploadSpriteFrame: upload failed ("
		                 << w << "x" << h << " frame " << frameIdx << ")";
		delete tex;
		return nullptr;
	}
	_spriteFrameCache[key] = tex;
	return tex;
}

/**
 * Block 11.10 / Phase 15: GPU post-flush pass that renders cursor-box overlays.
 *
 * Instances are collected during drawTerrainOverlayCPU() into
 * _cursorOverlayInstances with a CursorStyle tag:
 *   CS_MARKER_NEUTRAL/ALLY/ENEMY → Phase 16 SDF 4-tip animated marker pass.
 *   CS_RASTER                    → existing sprite-shader raster pass.
 */
void Map::drawCursorOverlayGLPass()
{
	if (_cursorOverlayInstances.empty() || SDL_GetTicks() - _lastDrawnTicks > 250) return;

	Screen* screen = _game->getScreen();
	const float xScale = static_cast<float>(screen->getXScale());
	const float yScale = static_cast<float>(screen->getYScale());
	const int   lbb    = screen->getCursorLeftBlackBand();
	const int   tbb    = screen->getCursorTopBlackBand();
	const int   dW     = Options::displayWidth;
	const int   dH     = Options::displayHeight;

	GLint prevProgram = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// ── 1. SDF 4-tip marker pass (CS_MARKER_NEUTRAL / ALLY / ENEMY) ────────
	if (!_cursorGLInit) initCursorGL();
	if (_cursorGLInit && _cursorShader && _cursorShader->isValid())
	{
		// Animation phase: 0→1 over the 800-ms tile animation cycle.
		const float phase = _animFrameGPU;

		// Per-instance buffer: a_xywh(4) + a_color(4) + a_params(4) = 12 floats.
		std::vector<float> buf;
		buf.reserve(_cursorOverlayInstances.size() * 12u);

		const float tileW = static_cast<float>(_spriteWidth)  * xScale;
		const float tileH = static_cast<float>(_spriteHeight) * yScale;

		for (const auto& ci : _cursorOverlayInstances)
		{
			if (ci.style == CS_RASTER) continue;

			const float dispX = static_cast<float>(ci.screenX) * xScale + static_cast<float>(lbb);
			const float dispY = static_cast<float>(ci.screenY) * yScale + static_cast<float>(tbb);

			buf.push_back(dispX); buf.push_back(dispY);
			buf.push_back(tileW); buf.push_back(tileH);

			// a_params layout: markerSize, baseFrac, animFrac, phase
			// markerSize=0.10 iso units, baseFrac=0.72, animFrac=0.08
			if (ci.style == CS_MARKER_NEUTRAL)
			{
				// cyan — empty tile
				buf.push_back(0.30f); buf.push_back(0.90f); buf.push_back(1.00f); buf.push_back(0.80f);
				buf.push_back(0.10f); buf.push_back(0.72f); buf.push_back(0.08f); buf.push_back(phase);
			}
			else if (ci.style == CS_MARKER_ALLY)
			{
				// blue — own unit (FACTION_PLAYER)
				buf.push_back(0.15f); buf.push_back(0.55f); buf.push_back(1.00f); buf.push_back(0.95f);
				buf.push_back(0.10f); buf.push_back(0.72f); buf.push_back(0.08f); buf.push_back(phase);
			}
			else if (ci.style == CS_MARKER_ENEMY)
			{
				// orange — hostile / neutral unit
				buf.push_back(1.00f); buf.push_back(0.45f); buf.push_back(0.05f); buf.push_back(0.95f);
				buf.push_back(0.10f); buf.push_back(0.72f); buf.push_back(0.08f); buf.push_back(phase);
			}
			else if (ci.style == CS_AP_RING)
			{
				// arc ring: color green/yellow/red by TU fraction.
				float arcFrac = ci.extraData;
				float r, g, b;
				if      (arcFrac > 0.50f) { r=0.15f; g=0.90f; b=0.40f; }  // green
				else if (arcFrac > 0.25f) { r=1.00f; g=0.75f; b=0.10f; }  // yellow
				else                      { r=1.00f; g=0.20f; b=0.10f; }  // red
				buf.push_back(r); buf.push_back(g); buf.push_back(b); buf.push_back(0.90f);
				// params: -innerR (ring-mode sentinel), ringWidth, arcFrac, phase
				buf.push_back(-0.38f); buf.push_back(0.08f);
				buf.push_back(arcFrac); buf.push_back(phase);
			}
			else if (ci.style == CS_FLOOR_RING)
			{
				// Warm amber-yellow iso-ellipse ring on the tile floor.
				// reduced alpha (0.50f) for better atmosphere.
				buf.push_back(1.00f); buf.push_back(0.78f); buf.push_back(0.15f); buf.push_back(0.50f);
				// v_params: x=0, y=ringWidth, z=animPhase, w=-1 (floor ring sentinel)
				buf.push_back(0.0f); buf.push_back(0.14f); buf.push_back(phase); buf.push_back(-1.0f);
			}
		}

		if (!buf.empty())
		{
			const GLsizei count = static_cast<GLsizei>(buf.size() / 12u);
			_cursorShader->use();
			_cursorShader->setUniform2f("u_screenSize",
				static_cast<float>(dW), static_cast<float>(dH));

			glBindBuffer(GL_ARRAY_BUFFER, _cursorInstanceVBO);
			glBufferData(GL_ARRAY_BUFFER,
				static_cast<GLsizeiptr>(buf.size() * sizeof(float)),
				buf.data(), GL_STREAM_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);

			glBindVertexArray(_cursorVAO);
			glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, count);
			glBindVertexArray(0);
		}
	}

	// ── 2. Raster glyph pass (CS_RASTER instances) ─────────────────────────
	{
		bool anyRaster = false;
		for (const auto& ci : _cursorOverlayInstances)
			if (ci.style == CS_RASTER) { anyRaster = true; break; }

		if (anyRaster)
		{
			if (!_spriteGLInit) initSpriteGL();
			if (_spriteGLInit && _spriteShader && _spriteShader->isValid())
			{
				auto drawQuad = [&](GpuTexture* tex, int gx, int gy, int fw, int fh)
				{
					const float dispX = static_cast<float>(gx) * xScale + static_cast<float>(lbb);
					const float dispY = static_cast<float>(gy) * yScale + static_cast<float>(tbb);
					const float dispW = static_cast<float>(fw) * xScale;
					const float dispH = static_cast<float>(fh) * yScale;

					const float ndcX0 =  2.0f * dispX / static_cast<float>(dW) - 1.0f;
					const float ndcY0 = -(2.0f * dispY / static_cast<float>(dH) - 1.0f);
					const float ndcX1 =  2.0f * (dispX + dispW) / static_cast<float>(dW) - 1.0f;
					const float ndcY1 = -(2.0f * (dispY + dispH) / static_cast<float>(dH) - 1.0f);

					const float verts[6 * 4] = {
						ndcX0, ndcY0,  0.0f, 0.0f,
						ndcX1, ndcY0,  1.0f, 0.0f,
						ndcX0, ndcY1,  0.0f, 1.0f,
						ndcX0, ndcY1,  0.0f, 1.0f,
						ndcX1, ndcY0,  1.0f, 0.0f,
						ndcX1, ndcY1,  1.0f, 1.0f,
					};
					glBindBuffer(GL_ARRAY_BUFFER, _spriteVBO);
					glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(verts), verts);
					glBindBuffer(GL_ARRAY_BUFFER, 0);

					tex->bind(0);
					glBindVertexArray(_spriteVAO);
					glDrawArrays(GL_TRIANGLES, 0, 6);
					glBindVertexArray(0);
				};

				_spriteShader->use();
				_spriteShader->setUniform1i("u_tex", 0);
				_spriteShader->setUniform1f("u_darken", 0.0f);

				for (const auto& ci : _cursorOverlayInstances)
				{
					if (ci.style != CS_RASTER) continue;
					GpuTexture* tex = getOrUploadSpriteFrame(ci.set, ci.frameIdx);
					if (!tex) continue;
					drawQuad(tex, ci.screenX, ci.screenY, tex->width(), tex->height());
				}
			}
		}
	}

	glDisable(GL_BLEND);
	glUseProgram(static_cast<GLuint>(prevProgram));
}

/**
 * Block 11.8: GPU post-flush pass that renders all visible bullet segments on top
 * of the GPU tile layer.  Thrown-item projectiles are still rendered by the CPU
 * path (ItemSprite uses palette-indexed blits that are non-trivial to replicate).
 * Called from the Screen GPU-pass loop registered in Map::init().
 */
void Map::drawProjectileGLPass()
{
	if (!_projectile || !_projectileInFOV || SDL_GetTicks() - _lastDrawnTicks > 250) return;
	if (_projectile->getItem()) return; // thrown items handled by CPU path
	if (!_spriteGLInit) initSpriteGL();
	if (!_spriteShader || !_spriteShader->isValid()) return;
	if (!_spriteVAO) return;

	Screen* screen = _game->getScreen();
	const float xScale = static_cast<float>(screen->getXScale());
	const float yScale = static_cast<float>(screen->getYScale());
	const int   lbb    = screen->getCursorLeftBlackBand();
	const int   tbb    = screen->getCursorTopBlackBand();
	const int   dW     = Options::displayWidth;
	const int   dH     = Options::displayHeight;
	const int   mapX   = getX();
	const int   mapY   = getY();

	int begin = 0, end = BULLET_SPRITES, direction = 1;
	if (_projectile->isReversed()) { begin = BULLET_SPRITES - 1; end = -1; direction = -1; }

	// Helper: upload VBO data + issue one draw call for a single sprite quad.
	auto drawQuad = [&](GpuTexture* tex, int gx, int gy, int fw, int fh, float darken)
	{
		const float dispX = static_cast<float>(gx) * xScale + static_cast<float>(lbb);
		const float dispY = static_cast<float>(gy) * yScale + static_cast<float>(tbb);
		const float dispW = static_cast<float>(fw) * xScale;
		const float dispH = static_cast<float>(fh) * yScale;

		const float ndcX0 =  2.0f * dispX / static_cast<float>(dW) - 1.0f;
		const float ndcY0 = -(2.0f * dispY / static_cast<float>(dH) - 1.0f);
		const float ndcX1 =  2.0f * (dispX + dispW) / static_cast<float>(dW) - 1.0f;
		const float ndcY1 = -(2.0f * (dispY + dispH) / static_cast<float>(dH) - 1.0f);

		const float verts[6 * 4] = {
			ndcX0, ndcY0,  0.0f, 0.0f,
			ndcX1, ndcY0,  1.0f, 0.0f,
			ndcX0, ndcY1,  0.0f, 1.0f,
			ndcX0, ndcY1,  0.0f, 1.0f,
			ndcX1, ndcY0,  1.0f, 0.0f,
			ndcX1, ndcY1,  1.0f, 1.0f,
		};
		glBindBuffer(GL_ARRAY_BUFFER, _spriteVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(verts), verts);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		tex->bind(0);
		_spriteShader->setUniform1f("u_darken", darken);
		glBindVertexArray(_spriteVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	};

	GLint prevProgram = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	_spriteShader->use();
	_spriteShader->setUniform1i("u_tex", 0);

	for (int i = begin; i != end; i += direction)
	{
		Surface* frame = _projectileSet->getFrame(_projectile->getParticle(i));
		if (!frame) continue;

		const int particleIdx = _projectile->getParticle(i);
		GpuTexture* tex = getOrUploadSpriteFrame(_projectileSet, particleIdx);
		if (!tex) continue;

		const int fw = frame->getWidth();
		const int fh = frame->getHeight();

		Position voxelPos = _projectile->getPosition(1 - i);
		if (!_save->getTileEngine()->isVoxelVisible(voxelPos)) continue;

		// Shadow: same sprite at floor level, fully darkened (darken = 1).
		Position shadowPos = voxelPos;
		shadowPos.z = _save->getTileEngine()->castedShade(voxelPos);
		if (_save->getTileEngine()->isVoxelVisible(shadowPos))
		{
			Position sp;
			_camera->convertVoxelToScreen(shadowPos, &sp);
			drawQuad(tex, sp.x + mapX - fw / 2, sp.y + mapY - fh / 2, fw, fh, 1.0f);
		}

		// Bullet: normal colour (darken = 0).
		Position bp;
		_camera->convertVoxelToScreen(voxelPos, &bp);
		drawQuad(tex, bp.x + mapX - fw / 2, bp.y + mapY - fh / 2, fw, fh, 0.0f);
	}

	glDisable(GL_BLEND);
	glUseProgram(static_cast<GLuint>(prevProgram));
}

/**
 * Block 11.9: GPU post-flush pass for smoke, fire, and explosion effects.
 *
 * Tile smoke instances were collected in emitTilePass() (stored in _smokeInstances).
 * Explosion instances are enumerated live here since _explosions changes each frame.
 * All sprites are drawn via the shared "textured" shader (same VAO/VBO as projectiles).
 * Thrown items and _flashScreen pixel-flash remain in the CPU path.
 */
void Map::drawSmokeGLPass()
{
	if (SDL_GetTicks() - _lastDrawnTicks > 250) return;
	if (!_spriteGLInit) initSpriteGL();
	if (!_spriteShader || !_spriteShader->isValid()) return;
	if (!_spriteVAO) return;

	const bool hasSmokeWork     = !_smokeInstances.empty();
	const bool hasExplosionWork = _explosionInFOV && !_flashScreen && !_explosions.empty();
	if (!hasSmokeWork && !hasExplosionWork) return;

	Screen* screen = _game->getScreen();
	const float xScale = static_cast<float>(screen->getXScale());
	const float yScale = static_cast<float>(screen->getYScale());
	const int   lbb    = screen->getCursorLeftBlackBand();
	const int   tbb    = screen->getCursorTopBlackBand();
	const int   dW     = Options::displayWidth;
	const int   dH     = Options::displayHeight;

	// Helper: issue one draw call for a sprite placed at (gx, gy) in SDL-surface coords.
	auto drawQuad = [&](GpuTexture* tex, int gx, int gy, int fw, int fh, float darken)
	{
		const float dispX = static_cast<float>(gx) * xScale + static_cast<float>(lbb);
		const float dispY = static_cast<float>(gy) * yScale + static_cast<float>(tbb);
		const float dispW = static_cast<float>(fw) * xScale;
		const float dispH = static_cast<float>(fh) * yScale;

		const float ndcX0 =  2.0f * dispX / static_cast<float>(dW) - 1.0f;
		const float ndcY0 = -(2.0f * dispY / static_cast<float>(dH) - 1.0f);
		const float ndcX1 =  2.0f * (dispX + dispW) / static_cast<float>(dW) - 1.0f;
		const float ndcY1 = -(2.0f * (dispY + dispH) / static_cast<float>(dH) - 1.0f);

		const float verts[6 * 4] = {
			ndcX0, ndcY0,  0.0f, 0.0f,
			ndcX1, ndcY0,  1.0f, 0.0f,
			ndcX0, ndcY1,  0.0f, 1.0f,
			ndcX0, ndcY1,  0.0f, 1.0f,
			ndcX1, ndcY0,  1.0f, 0.0f,
			ndcX1, ndcY1,  1.0f, 1.0f,
		};
		glBindBuffer(GL_ARRAY_BUFFER, _spriteVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(verts), verts);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		tex->bind(0);
		_spriteShader->setUniform1f("u_darken", darken);
		glBindVertexArray(_spriteVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	};

	GLint prevProgram = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	_spriteShader->use();
	_spriteShader->setUniform1i("u_tex", 0);

	// --- Tile smoke/fire instances collected by emitTilePass() ---
	for (const auto& si : _smokeInstances)
	{
		GpuTexture* tex = getOrUploadSpriteFrame(si.set, si.frameIdx);
		if (!tex) continue;
		drawQuad(tex, si.screenX, si.screenY, tex->width(), tex->height(), si.darken);
	}

	// --- Explosion effects (X1.PCK, HIT.PCK, SMOKE.PCK) ---
	if (hasExplosionWork)
	{
		Mod* mod = _game->getMod();
		SurfaceSet* x1Set    = mod->getSurfaceSet("X1.PCK");
		SurfaceSet* hitSet   = mod->getSurfaceSet("HIT.PCK");
		SurfaceSet* smokeSet = mod->getSurfaceSet("SMOKE.PCK");
		const int mapX = getX();
		const int mapY = getY();

		for (const auto* explosion : _explosions)
		{
			Position sp;
			_camera->convertVoxelToScreen(explosion->getPosition(), &sp);
			const int bsx = sp.x + mapX;
			const int bsy = sp.y + mapY;

			if (explosion->isBig())
			{
				const int f = explosion->getCurrentFrame();
				if (f < 0) continue;
				GpuTexture* tex = getOrUploadSpriteFrame(x1Set, f);
				if (!tex) continue;
				drawQuad(tex, bsx - tex->width() / 2, bsy - tex->height() / 2,
				         tex->width(), tex->height(), 0.0f);
			}
			else if (explosion->isHit())
			{
				GpuTexture* tex = getOrUploadSpriteFrame(hitSet, explosion->getCurrentFrame());
				if (!tex) continue;
				drawQuad(tex, bsx - 15, bsy - 25, tex->width(), tex->height(), 0.0f);
			}
			else
			{
				GpuTexture* tex = getOrUploadSpriteFrame(smokeSet, explosion->getCurrentFrame());
				if (!tex) continue;
				drawQuad(tex, bsx - 15, bsy - 15, tex->width(), tex->height(), 0.0f);
			}
		}
	}

	glDisable(GL_BLEND);
	glUseProgram(static_cast<GLuint>(prevProgram));
}

#endif

/**
 * Handles mouse presses on the map.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Map::mousePress(Action *action, State *state)
{
	InteractiveSurface::mousePress(action, state);
	_camera->mousePress(action, state);
}

/**
 * Handles mouse releases on the map.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Map::mouseRelease(Action *action, State *state)
{
	InteractiveSurface::mouseRelease(action, state);
	_camera->mouseRelease(action, state);
}

/**
 * Handles keyboard presses on the map.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Map::keyboardPress(Action *action, State *state)
{
	InteractiveSurface::keyboardPress(action, state);
	_camera->keyboardPress(action, state);
}

/**
 * Handles map vision toggle mode.
 */

void Map::enableNightVision()
{
	_nightVisionOn = true;
	_debugVisionMode = 0;
	persistToggles();
}

void Map::toggleNightVision()
{
	_nightVisionOn = !_nightVisionOn;
	_debugVisionMode = 0;
	persistToggles();
}

void Map::toggleDebugVisionMode()
{
	_debugVisionMode = (_debugVisionMode + 1) % 3;
	_nightVisionOn = false;
	persistToggles();
}

void Map::persistToggles()
{
	if (Options::oxceToggleNightVisionType == 2)
	{
		// persisted per campaign
		_game->getSavedGame()->setToggleNightVision(_nightVisionOn);
	}
	else if (Options::oxceToggleNightVisionType == 1)
	{
		// persisted per battle
		_save->setToggleNightVision(_nightVisionOn);
	}

	if (Options::oxceToggleBrightnessType == 2)
	{
		// persisted per campaign
		_game->getSavedGame()->setToggleBrightness(_debugVisionMode);
	}
	else if (Options::oxceToggleBrightnessType == 1)
	{
		// persisted per battle
		_save->setToggleBrightness(_debugVisionMode);
	}

	_save->setToggleBrightnessTemp(_debugVisionMode);
}

/**
 * Handles fade-in and fade-out shade modification
 * @param original tile/item/unit shade
 */

int Map::reShade(Tile *tile)
{
	// when modders just don't know where to stop...
	if (_debugVisionMode > 0)
	{
		if (_debugVisionMode == 1)
		{
			// Reaver's tests
			return tile->getShade() / 2;
		}
		// Meridian's debug helper
		return 0;
	}

	// no night vision
	if (_nvColor == 0)
	{
		return tile->getShade();
	}

	// already bright enough
	if ((tile->getShade() <= NIGHT_VISION_SHADE))
	{
		return tile->getShade();
	}

	// hybrid night vision (local)
	for (const auto* bu : *_save->getUnits())
	{
		if (bu->getFaction() == FACTION_PLAYER && !bu->isOut())
		{
			if (Position::distance2dSq(tile->getPosition(), bu->getPosition()) <= bu->getMaxViewDistanceAtDarkSquared())
			{
				return tile->getShade() > _fadeShade ? _fadeShade : tile->getShade();
			}
		}
	}

	// hybrid night vision (global)
	return std::min(+NIGHT_VISION_MAX_SHADE, tile->getShade());
}

/**
 * Handles keyboard releases on the map.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Map::keyboardRelease(Action *action, State *state)
{
	InteractiveSurface::keyboardRelease(action, state);
	_camera->keyboardRelease(action, state);
}

/**
 * Handles mouse over events on the map.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Map::mouseOver(Action *action, State *state)
{
	InteractiveSurface::mouseOver(action, state);
	_camera->mouseOver(action, state);
	_mouseX = (int)action->getAbsoluteXMouse();
	_mouseY = (int)action->getAbsoluteYMouse();
	setSelectorPosition(_mouseX, _mouseY);
}


/**
 * Sets the selector to a certain tile on the map.
 * @param mx mouse x position.
 * @param my mouse y position.
 */
void Map::setSelectorPosition(int mx, int my)
{
	int oldX = _selectorX, oldY = _selectorY;

	_camera->convertScreenToMap(mx, my + _spriteHeight/4, &_selectorX, &_selectorY);

	if (oldX != _selectorX || oldY != _selectorY)
	{
		_redraw = true;
		// Recompute remaining TU for the newly hovered tile whenever a player unit is
		// selected and NO path preview is active (calling calculate() while preview is
		// active would corrupt _path and break removePreview() later).
		_hoveredTU = -1;
#ifdef __EMSCRIPTEN__
		BattleUnit *selUnit = _save->getSelectedUnit();
		Pathfinding *pf = _save->getPathfinding();
		if (selUnit && selUnit->getFaction() == FACTION_PLAYER && !selUnit->isOut()
			&& _cursorType == CT_NORMAL && _save->getSide() == FACTION_PLAYER
			&& !pf->isPathPreviewed())
		{
			Position dest(_selectorX, _selectorY, _camera->getViewLevel());
			pf->calculate(selUnit, dest, BAM_NORMAL);
			int cost = pf->getTotalTUCost();
			if (cost > 0 && cost < 1000)
				_hoveredTU = selUnit->getTimeUnits() - cost;
		}
#endif
	}
}

/**
 * Handles animating tiles. 8 Frames per animation.
 * @param redraw Redraw the battlescape?
 */
void Map::animate(bool redraw)
{
	_save->nextAnimFrame();
	_animFrame = _save->getAnimFrame();

	// random ambient sounds
	{
		if (!_save->getAmbienceRandom().empty())
		{
			_save->decreaseCurrentAmbienceDelay();
			if (_save->getCurrentAmbienceDelay() <= 0)
			{
				_save->resetCurrentAmbienceDelay();
				_save->playRandomAmbientSound();
			}
		}
	}

	// animate tiles
	for (int i = 0; i < _save->getMapSizeXYZ(); ++i)
	{
		_save->getTile(i)->animate();
	}

	// animate vapor
	for (auto i : Collections::rangeValueLess(_vaporParticles.size()))
	{
		auto& v = _vaporParticles[i];
		int posX = i % _camera->getMapSizeX();
		int posY = i / _camera->getMapSizeX();

		Collections::removeIf(
			v,
			[&](Particle& p)
			{
				if (p.animate())
				{
					Position tileOffset = p.updateScreenPosition();
					if (tileOffset != Position(0,0,0))
					{
						addVaporParticle(Position(posX,posY,0) + tileOffset, p);
						return true;
					}
					return false;
				}
				else
				{
					return true;
				}
			}
		);
	}

	// init vapor vector
	for (auto i : Collections::rangeValueLess(_vaporParticlesInit.size()))
	{
		auto& vi = _vaporParticlesInit[i];
		auto& vDest = _vaporParticles[i];
		if (vi.empty())
		{
			continue;
		}

		if (vDest.empty())
		{
			vi.swap(vDest);
		}
		else
		{
			vDest.insert(std::begin(vDest), std::begin(vi), std::end(vi));
		}


		Collections::removeAll(vi);
	}

	for (auto& tilePar : _vaporParticles)
	{
		if (tilePar.empty())
		{
			Collections::removeAll(tilePar);
		}
		else
		{
			std::sort(std::begin(tilePar), std::end(tilePar), [](const Particle& a, const Particle& b){ return a.getLayerZ() < b.getLayerZ(); });
		}
	}

	// animate certain units (large flying units have a propulsion animation)
	for (auto* bu : *_save->getUnits())
	{
		const Position pos = bu->getPosition();

		// skip units that do not have position
		if (pos == TileEngine::invalid)
		{
			continue;
		}

		if (_save->getDepth() > 0)
		{
			bu->setFloorAbove(false);

			// make sure this unit isn't obscured by the floor above him, otherwise it looks weird.
			if (_camera->getViewLevel() > pos.z)
			{
				for (int z = std::min(_camera->getViewLevel(), _save->getMapSizeZ() - 1); z != pos.z; --z)
				{
					if (!_save->getTile(Position(pos.x, pos.y, z))->hasNoFloor(0))
					{
						bu->setFloorAbove(true);
						break;
					}
				}
			}
		}

		bu->breathe();
	}

	if (redraw) _redraw = true;
}

/**
 * Draws the rectangle selector.
 * @param pos Pointer to a position.
 */
void Map::getSelectorPosition(Position *pos) const
{
	pos->x = _selectorX;
	pos->y = _selectorY;
	pos->z = _camera->getViewLevel();
}

/**
 * Calculates the offset of a soldier, when it is walking in the middle of 2 tiles.
 * @param unit Pointer to BattleUnit.
 * @param offset Pointer to the offset to return the calculation.
 */
UnitWalkingOffset Map::calculateWalkingOffset(const BattleUnit *unit) const
{
	UnitWalkingOffset result = { };

	int offsetX[8] = { 1, 1, 1, 0, -1, -1, -1, 0 };
	int offsetY[8] = { 1, 0, -1, -1, -1, 0, 1, 1 };
	int phase = unit->getWalkingPhase() + unit->getDiagonalWalkingPhase();
	int dir = unit->getDirection();
	int midphase = 4 + 4 * (dir % 2);
	int endphase = 8 + 8 * (dir % 2);
	int size = unit->getArmor()->getSize();

	result.ScreenOffset.x = 0;
	result.ScreenOffset.y = 0;

	if (size > 1)
	{
		if (dir < 1 || dir > 5)
			midphase = endphase;
		else if (dir == 5)
			midphase = 12;
		else if (dir == 1)
			midphase = 5;
		else
			midphase = 1;
	}
	if (unit->getVerticalDirection())
	{
		midphase = 4;
		endphase = 8;
	}
	else if ((unit->getStatus() == STATUS_WALKING || unit->getStatus() == STATUS_FLYING))
	{
		if (phase < midphase)
		{
			result.ScreenOffset.x = phase * 2 * offsetX[dir];
			result.ScreenOffset.y = - phase * offsetY[dir];
		}
		else
		{
			result.ScreenOffset.x = (phase - endphase) * 2 * offsetX[dir];
			result.ScreenOffset.y = - (phase - endphase) * offsetY[dir];
		}
	}

	result.NormalizedMovePhase = endphase == 16 ? phase : phase * 2;

	// If we are walking in between tiles, interpolate it's terrain level.
	if (unit->getStatus() == STATUS_WALKING || unit->getStatus() == STATUS_FLYING)
	{
		const Position posCurr = unit->getPosition();
		const Position posDest = unit->getDestination();
		const Position posLast = unit->getLastPosition();
		if (phase < midphase)
		{
			int fromLevel = getTerrainLevel(posCurr, size);
			int toLevel = getTerrainLevel(posDest, size);
			if (posCurr.z > posDest.z)
			{
				// going down a level, so toLevel 0 becomes +24, -8 becomes  16
				toLevel += Position::TileZ*(posCurr.z - posDest.z);
			}
			else if (posCurr.z < posDest.z)
			{
				// going up a level, so toLevel 0 becomes -24, -8 becomes -16
				toLevel = -Position::TileZ*(posDest.z - posCurr.z) + abs(toLevel);
			}
			result.TerrainLevelOffset = Interpolate(fromLevel, toLevel, phase, endphase);
		}
		else
		{
			// from phase 4 onwards the unit behind the scenes already is on the destination tile
			// we have to get it's last position to calculate the correct offset
			int fromLevel = getTerrainLevel(posLast, size);
			int toLevel = getTerrainLevel(posDest, size);
			if (posLast.z > posDest.z)
			{
				// going down a level, so fromLevel 0 becomes -24, -8 becomes -32
				fromLevel -= Position::TileZ*(posLast.z - posDest.z);
			}
			else if (posLast.z < posDest.z)
			{
				// going up a level, so fromLevel 0 becomes +24, -8 becomes 16
				fromLevel = Position::TileZ*(posDest.z - posLast.z) - abs(fromLevel);
			}
			result.TerrainLevelOffset = Interpolate(fromLevel, toLevel, phase, endphase);
		}
	}
	else
	{
		result.TerrainLevelOffset = getTerrainLevel(unit->getPosition(), size);
	}
	result.ScreenOffset.y += result.TerrainLevelOffset;
	return result;
}


/**
  * Terrainlevel goes from 0 to -24. For a larger sized unit, we need to pick the highest terrain level, which is the lowest number...
  * @param pos Position.
  * @param size Size of the unit we want to get the level from.
  * @return terrainlevel.
  */
int Map::getTerrainLevel(const Position& pos, int size) const
{
	int lowestlevel = 0;

	for (int x = 0; x < size; x++)
	{
		for (int y = 0; y < size; y++)
		{
			int l = _save->getTile(pos + Position(x,y,0))->getTerrainLevel();
			if (l < lowestlevel)
				lowestlevel = l;
		}
	}

	return lowestlevel;
}

/**
 * Sets the 3D cursor to selection/aim mode.
 * @param type Cursor type.
 * @param size Size of cursor.
 */
void Map::setCursorType(CursorType type, int size)
{
	// reset cursor indicator cache
	_cacheActiveWeaponUfopediaArticleUnlocked = -1;
	_cacheIsCtrlPressed = false;
	_cacheCursorPosition = TileEngine::invalid;
	_cacheHasLOS = -1;

	_cursorType = type;
	if (_cursorType == CT_NORMAL)
		_cursorSize = size;
	else
		_cursorSize = 1;
}

/**
 * Gets the cursor type.
 * @return cursor type.
 */
CursorType Map::getCursorType() const
{
	return _cursorType;
}

/**
 * Puts a projectile sprite on the map.
 * @param projectile Projectile to place.
 */
void Map::setProjectile(Projectile *projectile)
{
	_projectile = projectile;
	if (projectile && Options::battleSmoothCamera)
	{
		_launch = true;
	}
}

/**
 * Gets the current projectile sprite on the map.
 * @return Projectile or 0 if there is no projectile sprite on the map.
 */
Projectile *Map::getProjectile() const
{
	return _projectile;
}

/**
 * Add new vapor particle.
 * @param pos Tile position of particle.
 * @param particle Particle to add.
 */
void Map::addVaporParticle(Position pos, Particle particle)
{
	if ((int)(_transparencies->size()) < (particle.getColor() + 1) * Mod::TransparenciesOpacityLevels * Mod::TransparenciesPaletteColors)
	{
		return;
	}
	if (pos.x >= _camera->getMapSizeX() || pos.y >= _camera->getMapSizeY())
	{
		return;
	}
	if (pos.x < 0 || pos.y < 0)
	{
		return;
	}

	auto& v = _vaporParticlesInit[_camera->getMapSizeX() * pos.y + pos.x];

	// as there will usually be more than one Particle, we prepare more space
	if (v.capacity() < 64)
	{
		v.reserve(64);
	}

	v.push_back(particle);
}

/**
 * Get all vapor for tile.
 * @param tile current tile.
 * @param topLayer if tile is top visible layer, if true then will return particles belongs to upper tiles.
 * @return range of particles that should be drawn.
 */
Collections::Range<const Particle*> Map::getVaporParticle(const Tile* tile, int topLayer) const
{
	Position pos = tile->getPosition();
	auto& v = _vaporParticles[_camera->getMapSizeX() * pos.y + pos.x];
	int startZ = pos.z * Particle::LayerAccuracy + (topLayer & 1);
	int endZ = startZ + Particle::LayerAccuracy / 2;
	auto* s = std::partition_point(v.data(), v.data() + v.size(), [&](const Particle& a){ return a.getLayerZ() < startZ; });
	auto* e = (topLayer & 2) ? v.data() + v.size() : std::partition_point(s, v.data() + v.size(), [&](const Particle& a){ return a.getLayerZ() < endZ; });
	return Collections::Range{ s, e };
}

/**
 * Gets a list of explosion sprites on the map.
 * @return A list of explosion sprites.
 */
std::list<Explosion*> *Map::getExplosions()
{
	return &_explosions;
}

/**
 * Gets the pointer to the camera.
 * @return Pointer to camera.
 */
Camera *Map::getCamera()
{
	return _camera;
}

/**
 * Timers only work on surfaces so we have to pass this on to the camera object.
 */
void Map::scrollMouse()
{
	_camera->scrollMouse();
}

/**
 * Timers only work on surfaces so we have to pass this on to the camera object.
 */
void Map::scrollKey()
{
	_camera->scrollKey();
}

/**
 * Modify the fade shade level if fade's in progress.
 */
void Map::fadeShade()
{
	const Uint8 *ks = SDL_GetKeyboardState(nullptr);
	bool hold = ks && ks[SDL_GetScancodeFromKey(Options::keyNightVisionHold)];
	if ((_nightVisionOn && !hold) || (!_nightVisionOn && hold))
	{
		_nvColor = Options::oxceNightVisionColor;
		_save->setToggleNightVisionTemp(true);
		_save->setToggleNightVisionColorTemp(_nvColor);
		if (_fadeShade > NIGHT_VISION_SHADE) // 0 = max brightness
		{
			--_fadeShade;
		}
	}
	else
	{
		if (_nvColor != 0)
		{
			if (_fadeShade < _save->getGlobalShade())
			{
				// gradually fade away
				++_fadeShade;
			}
			else
			{
				// and at the end turn off night vision
				_nvColor = 0;
				_save->setToggleNightVisionTemp(false);
				_save->setToggleNightVisionColorTemp(0);
			}
		}
	}
}

/**
 * Gets a list of waypoints on the map.
 * @return A list of waypoints.
 */
std::vector<Position> *Map::getWaypoints()
{
	return &_waypoints;
}

/**
 * Sets mouse-buttons' pressed state.
 * @param button Index of the button.
 * @param pressed The state of the button.
 */
void Map::setButtonsPressed(Uint8 button, bool pressed)
{
	setButtonPressed(button, pressed);
}

/**
 * Sets the unitDying flag.
 * @param flag True if the unit is dying.
 */
void Map::setUnitDying(bool flag)
{
	_unitDying = flag;
}

/**
 * Updates the selector to the last-known mouse position.
 */
void Map::refreshSelectorPosition()
{
	// Force _hoveredTU recompute on the next setSelectorPosition call by
	// temporarily invalidating the cached selector so the change-guard fires.
	_selectorX = -1;
	_selectorY = -1;
	setSelectorPosition(_mouseX, _mouseY);
}

/**
 * Special handling for setting the height of the map viewport.
 * @param height the new base screen height.
 */
void Map::setHeight(int height)
{
	Surface::setHeight(height);
	_visibleMapHeight = height - _iconHeight;
	_message->setHeight((_visibleMapHeight < 200)? _visibleMapHeight : 200);
	_message->setY((_visibleMapHeight - _message->getHeight()) / 2);
}

/**
 * Special handling for setting the width of the map viewport.
 * @param width the new base screen width.
 */
void Map::setWidth(int width)
{
	int dX = width - getWidth();
	Surface::setWidth(width);
	_message->setX(_message->getX() + dX / 2);
}

/**
 * Get the hidden movement screen's vertical position.
 * @return the vertical position of the hidden movement window.
 */
int Map::getMessageY() const
{
	return _message->getY();
}

/**
 * Get the icon height.
 */
int Map::getIconHeight() const
{
	return _iconHeight;
}

/**
 * Get the icon width.
 */
int Map::getIconWidth() const
{
	return _iconWidth;
}

/**
 * Returns the angle(left/right balance) of a sound effect,
 * based off a map position.
 * @param pos the map position to calculate the sound angle from.
 * @return the angle of the sound (280 to 440).
 */
int Map::getSoundAngle(const Position& pos) const
{
	int midPoint = getWidth() / 2;
	Position relativePosition;

	_camera->convertMapToScreen(pos, &relativePosition);
	// cap the position to the screen edges relative to the center,
	// negative values indicating a left-shift, and positive values shifting to the right.
	relativePosition.x = Clamp((relativePosition.x + _camera->getMapOffset().x) - midPoint, -midPoint, midPoint);

	// convert the relative distance to a relative increment of an 80 degree angle
	// we use +- 80 instead of +- 90, so as not to go ALL the way left or right
	// which would effectively mute the sound out of one speaker.
	// since Mix_SetPosition uses modulo 360, we can't feed it a negative number, so add 360 instead.
	return 360 + (relativePosition.x / (midPoint / 80.0));
}

/**
 * Reset the camera smoothing bool.
 */
void Map::resetCameraSmoothing()
{
	_smoothingEngaged = false;
}

/**
 * Set the "explosion flash" bool.
 * @param flash should the screen be rendered in EGA this frame?
 */
void Map::setBlastFlash(bool flash)
{
	_flashScreen = flash;

	// Meridian: no frikin flashing!!
	_flashScreen = false;
}

/**
 * Checks if the screen is still being rendered in EGA.
 * @return if we are still in EGA mode.
 */
bool Map::getBlastFlash() const
{
	return _flashScreen;
}

/**
 * Resets obstacle markers.
 */
void Map::resetObstacles(void)
{
	for (int z = 0; z < _save->getMapSizeZ(); z++)
		for (int y = 0; y < _save->getMapSizeY(); y++)
			for (int x = 0; x < _save->getMapSizeX(); x++)
			{
				Tile *tile = _save->getTile(Position(x, y, z));
				if (tile) tile->resetObstacle();
			}
	_showObstacles = false;
}

/**
 * Enables obstacle markers.
 */
void Map::enableObstacles(void)
{
	_showObstacles = true;
	if (_obstacleTimer)
	{
		_obstacleTimer->stop();
		_obstacleTimer->start();
	}
}

/**
 * Disables obstacle markers.
 */
void Map::disableObstacles(void)
{
	_showObstacles = false;
	if (_obstacleTimer)
	{
		_obstacleTimer->stop();
	}
}

}
