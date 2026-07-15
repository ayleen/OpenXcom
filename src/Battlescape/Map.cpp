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
#include "../Engine/FileMap.h"
#include <SDL_image.h>  // Phase 24: IMG_Load_RW for UI marker PNGs (getUITexture)
#include <cmath>        // Phase 24: std::sin for marching path-node animation
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
#include "../Mod/RuleDamageType.h"
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
 * Definitions live in Calypso/EmscriptenHarness.cpp inside extern "C" {},
 * so forward-declarations must carry C linkage (global namespace). */
extern "C" int g_calypsoProfileBattlescape;
extern "C" int g_calypsoProfileReadback;
/* Phase 28: underwater grade strength + beauty-FX amplitudes, set from JS via
 * calypso_set_underwater_*(); read by Map::drawSceneGrade(). */
extern "C" float g_calypsoUnderwaterStrength;
extern "C" float g_calypsoUwCaustics;
extern "C" float g_calypsoUwRefract;
extern "C" float g_calypsoUwBubbles;
extern "C" float g_calypsoUwSnow;
extern "C" float g_calypsoUwUnitBub;
extern "C" float g_calypsoUwGodray;
extern "C" float g_calypsoUwBloom;
extern "C" float g_calypsoUwBreath;
extern "C" float g_calypsoUwChroma;
extern "C" float g_calypsoUwShock;
extern "C" float g_calypsoUwEmissive;   // Phase 25 (R1): coloured emissive halo amount
extern "C" float g_calypsoTileEmissive; // Phase 25 (R6): HD material-emissive atlas multiplier
extern "C" float g_calypsoUnitShade;    // Phase 25 (R7): unit fake-AO amount (0 = off … 1 = full)
extern "C" float g_calypsoSunDir[3];    // Phase 25 (R3): tangent-space sun dir for normal relief
extern "C" int   g_calypsoSunAuto;      // Phase 25 (R3): 1 = engine drives the sun (per-turn sweep)
/* Phase-14 railings debug: one-shot tile dump flag.
 * Set to 1 by Module._calypso_dump_emit_once() before forcing a redraw;
 * emitTilePass() and Map::draw() painter pass each log every visible tile
 * and reset the flag, so production runs at zero cost. */
extern "C" int g_calypsoDumpEmit;
/* L2 (memory-reduction): JS-side SSAA scale override; 0 = use Map default.
 * Definition in Calypso/EmscriptenHarness.cpp; set via calypso_set_ssaa_scale(). */
extern "C" int g_calypsoSsaaScale;
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
#ifdef __EMSCRIPTEN__
	{
		int tileScale = _game->getMod()->getBattlescapeTileScale();
		_spriteWidth  *= tileScale;
		_spriteHeight *= tileScale;
	}
#endif
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
	_cacheAccuracy = -1;
	_cacheAccuracyTextColor = -1;

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
	delete _gradeShader;    _gradeShader    = nullptr;   // Phase 28 (was leaked)
	delete _emissiveShader; _emissiveShader = nullptr;   // Phase 25 (R1)
	if (_tileGLInit)
	{
		glDeleteBuffers(1, &_tileVBO);
		glDeleteBuffers(1, &_tileIBO);
		glDeleteVertexArrays(1, &_tileVAO);
		if (_blendVAO) { glDeleteVertexArrays(1, &_blendVAO); _blendVAO = 0; }
		if (_blendIBO) { glDeleteBuffers(1,      &_blendIBO); _blendIBO = 0; }
		if (_tileInstVAO) { glDeleteVertexArrays(1, &_tileInstVAO); _tileInstVAO = 0; }  // Phase 22 (H1)
		if (_tileInstIBO) { glDeleteBuffers(1,      &_tileInstIBO); _tileInstIBO = 0; }  // Phase 22 (H1)
		if (_gradeVAO)    { glDeleteVertexArrays(1, &_gradeVAO);    _gradeVAO = 0; }      // Phase 28 (was leaked)
		if (_gradeVBO)    { glDeleteBuffers(1,      &_gradeVBO);    _gradeVBO = 0; }      // Phase 28
		if (_emissiveVAO) { glDeleteVertexArrays(1, &_emissiveVAO); _emissiveVAO = 0; }   // Phase 25 (R1)
		if (_emissiveVBO) { glDeleteBuffers(1,      &_emissiveVBO); _emissiveVBO = 0; }   // Phase 25 (R1)
	}
	delete _spriteShader; _spriteShader = nullptr;
	for (auto& p : _spriteFrameCache) delete p.second;
	_spriteFrameCache.clear();
	for (auto& p : _hudTextTexCache) delete p.second;
	_hudTextTexCache.clear();
	for (auto& p : _hudImageTexCache) delete p.second;
	_hudImageTexCache.clear();
	for (int i = 0; i < HUD_IMG_COUNT; ++i) _hudImageSlots[i].active = false;
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
		// under CPU-drawn units / walls / HUD. Units draw in a depth-ordered
		// block within the tile pass (GPU depth test resolves occlusion, e.g. the
		// submarine roof above units inside the cargo bay).
		_game->getScreen()->registerGPUPassPreComposite([this, wf]() {
			if (!wf.lock()) return;
			this->drawTileGLPass();
		});
		// Calypso: tile-smoke murk fires PRE-composite, right after the tiles, so it
		// sits in the scene (under the CPU HUD/menu layer) — never over them, never
		// clipped to the map viewport, and held still by a frozen clock under a menu.
		_game->getScreen()->registerGPUPassPreComposite([this, wf]() {
			if (!wf.lock()) return;
			this->drawMurkGLPass();
		});
		// Calypso P30 Срез B: blood plume + pools + scorch PRE-composite (after murk, in
		// the scene). NO scissor — exactly like murk: these sit under the CPU HUD/menu
		// composite, so the HUD overdraws them at the bottom instead of a hard scissor cut
		// at the HUD's top edge. Position-anchored to blast/wound tiles, so they never
		// reach the top/side letterbox.
		_game->getScreen()->registerGPUPassPreComposite([this, wf]() {
			if (!wf.lock()) return;
			this->drawBloodGLPass();
		});
		// Block 11.10: tile-space cursor overlay — kept POST-composite (it is interactive
		// UI, wants to stay crisp over everything; scissored + menu-gated).
		_game->getScreen()->registerGPUPass([this, wf]() {
			if (!wf.lock()) return;
			if (!this->overlayPassesActive()) return;   // not over a menu
			this->beginMapScissor();                     // not over the HUD
			this->drawCursorOverlayGLPass();
			this->endMapScissor();
		});
		// Calypso Phase 41 (4.5): scene-preview coordinate readout (§41.1c).
		_game->getScreen()->registerGPUPass([this, wf]() {
			if (!wf.lock()) return;
			this->updateScenePreviewCoordHud();
		});
		// Calypso bug 1: crisp HUD name + stat digits at PHYSICAL resolution. POST-composite
		// (over the stretched logical HUD), menu-gated. No map scissor — the text lives in the
		// HUD panel, not the map viewport; each item's quad is positioned from its logical
		// widget rect via the same xScale/yScale path as the cursor.
		_game->getScreen()->registerGPUPass([this, wf]() {
			if (!wf.lock()) return;
			if (!this->hudOverlayVisible()) return;   // survives non-fullscreen popups (keeps text crisp)
			this->drawHudTextGLPass();
		});
		// Calypso P30: ALL battlescape FX (projectile, explosion flash/fireball/bubble,
		// wound-glow, particle burst) fire PRE-composite WITHOUT a scissor — exactly like
		// murk. They draw over the GL scene (tiles + units are emitted inside
		// drawTileGLPass), and the CPU HUD/menu composite then draws over them. This is
		// what stops every effect being hard-clipped at the HUD's top edge (the old POST +
		// beginMapScissor path cut them at mapClipBottomY). Order: projectile → flash/
		// fireball → wound-glow → particles.
		_game->getScreen()->registerGPUPassPreComposite([this, wf]() {
			if (!wf.lock()) return;
			this->drawProjectileGLPass();
		});
		_game->getScreen()->registerGPUPassPreComposite([this, wf]() {
			if (!wf.lock()) return;
			this->drawSmokeGLPass();
		});
		_game->getScreen()->registerGPUPassPreComposite([this, wf]() {
			if (!wf.lock()) return;
			this->drawWoundGlowGLPass();
		});
		_game->getScreen()->registerGPUPassPreComposite([this, wf]() {
			if (!wf.lock()) return;
			this->drawFxParticlesGLPass();
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
			_gradeVAO = _gradeVBO = 0;         // Phase 28: grade quad recreated by initTileGL
			_emissiveVAO = _emissiveVBO = 0;   // Phase 25 (R1): recreated by initTileGL
			_ssaaFBO = _ssaaColorTex = _ssaaDepthRB = 0;  // Phase 28: force SSAA recreate
			_ssaaW = _ssaaH = 0;
			_ssaaIsHDR = false;                // Phase 25 (R0): re-evaluated on SSAA recreate
			// Phase 25 (R0): the restored context drops enabled extensions —
			// re-request EXT_color_buffer_float so the float SSAA target survives.
			GpuInit::enableExtensions();
			_spriteVAO = _spriteVBO = 0;
			_spriteGLInit = false;
			_cursorVAO = _cursorVBO = _cursorInstanceVBO = 0;
			_cursorGLInit = false;
			_unitAtlasGroups.clear();
			_game->getMod()->clearUnitAtlases();
			// Drop cached GpuTextures whose GL handles died with the context — else
			// getUITexture / getOrUploadSpriteFrame keep returning dead textures and every
			// FX that uses them (blood pools/scorch, murk, fire, particles, …) renders
			// nothing after a resolution change. Clearing forces a lazy re-upload.
			for (auto& p : _uiTexCache)      delete p.second;
			_uiTexCache.clear();
			for (auto& p : _spriteFrameCache) delete p.second;
			_spriteFrameCache.clear();
			for (auto& p : _hudTextTexCache) delete p.second;
			_hudTextTexCache.clear();
			for (auto& p : _hudImageTexCache) delete p.second;
			_hudImageTexCache.clear();
			for (int i = 0; i < HUD_IMG_COUNT; ++i) _hudImageSlots[i].active = false;
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
			_emissiveSources.clear();   // Phase 25 (R1): drop stale halos with the rest
			// Phase 22 (H1): terrain lists emptied— invalidate the persistent buffer so
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
			// Phase 25 R3: propagate the normal atlas (non-owning; Mod owns it).
			_tileAtlasGroups[i].normalAtlas        = spec->normalAtlas;
			_tileAtlasGroups[i].hasNormalMap       = (spec->normalAtlas != nullptr);
			// Phase 25 R6: propagate the emissive atlas (non-owning; Mod owns it).
			_tileAtlasGroups[i].emissiveAtlas      = spec->emissiveAtlas;
			_tileAtlasGroups[i].hasEmissive        = (spec->emissiveAtlas != nullptr);
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
				// Phase 28: steepen the night darkening of HD overlay/blend tiles.
				// The raw luminance ratio underplays the dark vs the vanilla palette
				// walk (which also desaturates toward deep blue), so unlit tiles read
				// too light — "barely dark". gamma>1 pushes the dark end down hard
				// while leaving shade 0 (full light, r=1) untouched, so it's dark
				// where it's dark and bright only near light sources/units.
				r = std::pow(r, 2.0);
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
	const int tileFoorWidth  = _spriteWidth;
	const int tileFoorHeight = _spriteWidth / 2;
	const int tileHeight     = _spriteHeight;

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
#ifdef __EMSCRIPTEN__
	// Calypso P30: hit-jolt — nudge the sprite toward bullet travel, decaying over
	// a few anim frames. Propagates to the GPU atlas path too (same screen coords).
	{
		auto sit = _unitShakeOffset.find(bu->getId());
		if (sit != _unitShakeOffset.end() && sit->second.framesLeft > 0)
		{
			const float t = (float)sit->second.framesLeft / (float)sit->second.totalFrames;
			const float ampPx = (float)_spriteWidth * 0.14f * t;
			offsets.ScreenOffset.x += (int)std::lround(sit->second.dx * ampPx);
			offsets.ScreenOffset.y += (int)std::lround(sit->second.dy * ampPx);
		}
	}
#endif
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
		const HdUnitScalePlan scalePlan = makeHdUnitScalePlan(gpuUnitSpec,
		    gpuUnitAvail ? _game->getMod()->getUnitAtlas("HANDOB.PCK") : nullptr,
		    _spriteWidth, _spriteHeight);
		if (gpuUnitSpec && gpuUnitSpec->atlas && scalePlan.valid)
		{
			// Find or create UnitAtlasGroups; use indices to avoid iterator
			// invalidation if push_back causes a vector reallocation.
			auto ensureGroup = [this](const Mod::UnitAtlasSpec* spec) -> size_t {
				for (size_t i = 0; i < _unitAtlasGroups.size(); ++i)
					if (_unitAtlasGroups[i].spec == spec) return i;
				_unitAtlasGroups.push_back({});
				_unitAtlasGroups.back().spec = spec;
				// Phase 42 E1: size the per-page RGBA overlay instance vectors
				// to match the spec's page count so the emit path can index them.
				_unitAtlasGroups.back().rgbaOverlayInstances.resize(
				    spec ? spec->rgbaOverlayPages.size() : 0);
				return _unitAtlasGroups.size() - 1;
			};
			const size_t bodyIdx = ensureGroup(gpuUnitSpec);
			const bool haveItem = scalePlan.itemSpec && scalePlan.itemSpec->atlas;
			const size_t itemIdx = haveItem ? ensureGroup(scalePlan.itemSpec) : _unitAtlasGroups.size();
			// Pass currTile's (Z, Y, X) so the GPU shader can derive an iso
			// priority and use depth-test for correct iso z-ordering between
			// units / items / tiles (no per-cell bucketing needed).
			const int unitZ = currTile ? currTile->getPosition().z : 0;
			const int unitY = currTile ? currTile->getPosition().y : 0;
			const int unitX = currTile ? currTile->getPosition().x : 0;
			unitSprite.setEmitMode(makeHdUnitEmitTargets(bodyIdx, haveItem, itemIdx,
			    unitZ, unitY, unitX, _spriteWidth, _spriteHeight),
			    scalePlan.partOffsetScale);
			unitSprite.draw(bu, part, tileScreenPosition.x + offsets.ScreenOffset.x, tileScreenPosition.y + offsets.ScreenOffset.y, shade, mask, _isAltPressed && !_isCtrlPressed);
			unitSprite.clearEmitMode();

			// Phase 27.5: contact shadow on the unit's own tile floor. Emit once per
			// unit (skip the back/front overlap calls where currTile != unitTile).
			// Anchored to the tile floor (not the walk/fly offset) so it stays
			// planted; depth (prio 0.5 < the unit body's 4) keeps it under the body.
			if (currTile == unitTile && !unitFromBelow && !unitFromAbove)
			{
				const float shW = (float)_spriteWidth  * 0.60f;
				const float shH = (float)tileFoorHeight * 0.60f;
				const float fx  = (float)tileScreenPosition.x + _spriteWidth * 0.5f;
				const float fy  = (float)tileScreenPosition.y + (float)tileHeight - (float)tileFoorHeight * 0.5f;
				const Position pp = currTile->getPosition();
				const float prio = pp.z * 65536.0f + pp.y * 1024.0f + pp.x * 8.0f + 0.5f;
				TileInstance sh{};
				sh.screenX = fx - shW * 0.5f;
				sh.screenY = fy - shH * 0.5f;
				sh.atlasU = 0.0f; sh.atlasV = 0.0f;
				sh.shade = (float)shade;
				sh.animFrameCount = 1.0f;
				sh.alphaMask = 1.0f;
				sh.iso = prio / 2000000.0f;
				_unitShadowInst.push_back(sh);
			}
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
#ifdef __EMSCRIPTEN__
	// Phase 24 UX: the in-map custom cursor is hidden while a unit is selected for
	// movement, but over the HUD (visible-enemy buttons, item panel, …) the player
	// needs the normal system arrow. The per-tile cursor block below only runs over
	// the map, so force the arrow visible here whenever the mouse is over the icons.
	if (gpuCursorSet && _save && _save->getBattleState()
	    && (_save->getBattleState()->getMouseOverIcons()
	        || _save->getBattleState()->isMouseNearVisibleUnitButton(_mouseX, _mouseY)))
	{
		_game->getCursor()->setHidden(false);
	}
#endif
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
	Position cameraPos = _camera->getMapOffset();
#ifdef __EMSCRIPTEN__
	cameraPos += currentShakeOffset();   // Calypso P30: camera shake (CPU overlay path)
#endif
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

#ifdef __EMSCRIPTEN__
					// Phase 24 UX (Stage 1): persistent selection indicator under the
					// active player unit — a cyan floor ring (selection-ring.png) plus a
					// downward amber chevron over its head (active-unit.png) — drawn every
					// frame regardless of the cursor, so it is always clear which unit is
					// selected. Textured UI markers (white silhouettes tinted per state).
					if (gpuCursorSet && unit && _camera->getViewLevel() == itZ)
					{
						BattleUnit* selU = _save->getSelectedUnit();
						if (selU && unit == selU && selU->getFaction() == FACTION_PLAYER
						    && !selU->isOut() && (selU->getVisible() || _save->getDebugMode()))
						{
							// Cyan floor ring, centred on the tile floor.
							if (GpuTexture* ringTex = getUITexture("Resources/battlescape/ui/selection-ring.png"))
							{
								CursorOverlayInstance ci;
								ci.screenX = screenPosition.x; ci.screenY = screenPosition.y;
								ci.style = CS_TEX_TINT; ci.tex = ringTex;
								const float selPulse = 0.78f + 0.22f * std::sin(_animFrameGPU * 6.2831853f);
								ci.tintR = 0.20f * selPulse; ci.tintG = 0.88f * selPulse; ci.tintB = 1.0f * selPulse;
								ci.sizeMul = 1.35f + 0.05f * std::sin(_animFrameGPU * 6.2831853f);
								const int sz = (int)(_spriteWidth * ci.sizeMul);
								ci.offY = (_spriteHeight - _spriteWidth / 4) - sz / 2;  // floor centre
								_cursorOverlayInstances.push_back(ci);
							}
							// Amber over-head chevron with a slow bob.
							if (GpuTexture* chevTex = getUITexture("Resources/battlescape/ui/active-unit.png"))
							{
								CursorOverlayInstance ci;
								const int bob = (int)(_spriteHeight * 0.04f * (1.0f - _animFrameGPU));
								ci.screenX = screenPosition.x; ci.screenY = screenPosition.y;
								ci.style = CS_TEX_TINT; ci.tex = chevTex;
								ci.tintR = 1.0f; ci.tintG = 0.82f; ci.tintB = 0.20f;
								ci.sizeMul = 0.55f;
								ci.offY = -(int)(_spriteHeight * 0.55f) - bob;  // above the head
								_cursorOverlayInstances.push_back(ci);
							}
						}
					}
#endif

					// Draw cursor back
					if (_cursorType != CT_NONE && _selectorX > itX - _cursorSize && _selectorY > itY - _cursorSize && _selectorX < itX+1 && _selectorY < itY+1 && !_save->getBattleState()->getMouseOverIcons() && !_save->getBattleState()->isMouseNearVisibleUnitButton(_mouseX, _mouseY))
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
									// Phase 42 E1: per-page RGBA overlay instance vectors.
									_unitAtlasGroups.back().rgbaOverlayInstances.resize(
									    itemAtlasSpec ? itemAtlasSpec->rgbaOverlayPages.size() : 0);
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
#ifdef __EMSCRIPTEN__
						if (vaporY >= mapClipBottomY()) continue;   // keep vapor on the map, off the HUD
#endif
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
#ifdef __EMSCRIPTEN__
						if (vaporY >= mapClipBottomY()) continue;   // keep vapor on the map, off the HUD
#endif
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
#ifdef __EMSCRIPTEN__
						// Phase 24 UX (Stage 3): replace the flat Pathfinding arrows with an
						// HD "marching" path node — cyan if reachable with the current TU,
						// red if not, brightness wave travelling toward the destination.
						if (gpuCursorSet && _camera->getViewLevel() == itZ)
						{
							// Skip the destination tile: the hover ring (Stage 2) marks it, so
							// no arrow there. Intermediate tiles get a directional path node.
							const bool isDest = (itX == _selectorX && itY == _selectorY);
							if (!isDest)
							if (GpuTexture* t = getUITexture("Resources/battlescape/ui/path-arrow.png"))
							{
								BattleUnit* su = _save->getSelectedUnit();
								const int tuCost = tile->getTUMarker();
								const bool reachable = !su || tuCost < 0 || tuCost <= su->getTimeUnits();
								const float wave = 0.55f + 0.45f * std::sin(6.2831853f *
								    (_animFrameGPU - (float)(tuCost > 0 ? tuCost : 0) * 0.07f));
								CursorOverlayInstance ci;
								ci.screenX = screenPosition.x; ci.screenY = screenPosition.y;
								ci.style = CS_TEX_TINT; ci.tex = t;
								if (reachable) { ci.tintR = 0.20f*wave; ci.tintG = 0.88f*wave; ci.tintB = 1.0f*wave; }
								else           { ci.tintR = 1.00f*wave; ci.tintG = 0.28f*wave; ci.tintB = 0.18f*wave; }
								// Rotate the node to the movement direction
								// (screen delta = (2*offsetX, -offsetY), base == game-dir N).
								static const int oxv[8] = {1,1,1,0,-1,-1,-1,0};
								static const int oyv[8] = {1,0,-1,-1,-1,0,1,1};
								const int pdir = tile->getPreview() & 7;
								ci.rot = std::atan2(-(float)oyv[pdir], 2.0f * (float)oxv[pdir]) - std::atan2(-1.0f, 2.0f);
								ci.sizeMul = 0.5f;
								const int sz = (int)(_spriteWidth * ci.sizeMul);
								ci.offY = (_spriteHeight - _spriteWidth / 4) - sz / 2;  // floor centre
								_cursorOverlayInstances.push_back(ci);
							}
						}
						else
#endif
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
					if (_cursorType != CT_NONE && _selectorX > itX - _cursorSize && _selectorY > itY - _cursorSize && _selectorX < itX+1 && _selectorY < itY+1 && !_save->getBattleState()->getMouseOverIcons() && !_save->getBattleState()->isMouseNearVisibleUnitButton(_mouseX, _mouseY))
					{
						if (_camera->getViewLevel() == itZ)
						{
							if (_cursorType != CT_AIM)
							{
#ifdef __EMSCRIPTEN__
								// Phase 16: cursor style — faction-aware 4-tip markers or floor ring.
								if (gpuCursorSet)
								{
									bool tileHasUnit = unit && (unit->getVisible() || _save->getDebugMode());
									BattleUnit *selUnit = _save->getSelectedUnit();
									bool playerWalking = (selUnit && selUnit->getFaction() == FACTION_PLAYER
									                      && !selUnit->isOut() && _cursorType == CT_NORMAL);
									// Phase 24 UX (Stage 2): textured cursor markers (white silhouettes
									// tinted per state) replace the procedural SDF 4-tip markers.
									const bool isEnemyUnit = tileHasUnit && unit->getFaction() != FACTION_PLAYER;
									if (isEnemyUnit)
									{
										// Red lock-on reticle floating over a hostile/neutral unit.
										if (GpuTexture* t = getUITexture("Resources/battlescape/ui/reticle-enemy.png"))
										{
											CursorOverlayInstance ci;
											ci.screenX = screenPosition.x; ci.screenY = screenPosition.y;
											ci.style = CS_TEX_TINT; ci.tex = t;
											ci.tintR = 1.0f; ci.tintG = 0.27f; ci.tintB = 0.18f;
											ci.sizeMul = 0.95f;
											const int sz = (int)(_spriteWidth * ci.sizeMul);
											ci.offY = (_spriteHeight - sz) / 2 - (int)(_spriteHeight * 0.30f); // over body
											_cursorOverlayInstances.push_back(ci);
										}
									}
									else if (tileHasUnit)
									{
										// Bright pulsing hover ring on a hovered unit.
										if (GpuTexture* t = getUITexture("Resources/battlescape/ui/hover-ring.png"))
										{
											CursorOverlayInstance ci;
											ci.screenX = screenPosition.x; ci.screenY = screenPosition.y;
											ci.style = CS_TEX_TINT; ci.tex = t;
											const float dp = 0.80f + 0.20f * std::sin(_animFrameGPU * 6.2831853f);
											ci.tintR = 0.20f*dp; ci.tintG = 0.88f*dp; ci.tintB = 1.0f*dp;
											ci.sizeMul = 1.28f;
											const int sz = (int)(_spriteWidth * ci.sizeMul);
											ci.offY = (_spriteHeight - _spriteWidth / 4) - sz / 2; // floor centre
											_cursorOverlayInstances.push_back(ci);
										}
									}
									else if (playerWalking)
									{
										// Move-destination marker (ring + inward arrows): the "move here" target.
										if (GpuTexture* t = getUITexture("Resources/battlescape/ui/dest-marker.png"))
										{
											CursorOverlayInstance ci;
											ci.screenX = screenPosition.x; ci.screenY = screenPosition.y;
											ci.style = CS_TEX_TINT; ci.tex = t;
											const float dp = 0.80f + 0.20f * std::sin(_animFrameGPU * 6.2831853f);
											ci.tintR = 0.20f*dp; ci.tintG = 0.88f*dp; ci.tintB = 1.0f*dp; // unified cyan
											ci.sizeMul = 1.30f;
											const int sz = (int)(_spriteWidth * ci.sizeMul);
											ci.offY = (_spriteHeight - _spriteWidth / 4) - sz / 2; // floor centre
											_cursorOverlayInstances.push_back(ci);
										}
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
								int distance = 0;
								int targetSize = 1;
								if (unit && unit->getVisible()) targetSize = unit->getArmor()->getSize();

								// Realistic Accuracy replaces the classic crosshair number with a true chance-to-hit
								// (Joy Narical's RA, from Brutal-OXCE by Xilmi). Classic path preserved verbatim below.
								if (!Options::battleRealisticAccuracy) // Classic Accuracy
								{
								int distanceSq = action->actor->distance3dToPositionSq(Position(itX, itY,itZ));
								distance = (int)std::ceil(sqrt(float(distanceSq)));

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
									// replace accuracy number by chance-to-hit
									if (Options::useChanceToHit)
										accuracy = Projectile::getHitChance(distance, accuracy, _game->getMod()->getHitChancesTable(targetSize));
									ss << accuracy;
									ss << "%";
								}
								} // end Classic Accuracy
								else // Realistic Accuracy (Joy Narical's RA, from Brutal-OXCE by Xilmi)
								{
									const int TXT_GREEN  = Palette::blockOffset(Pathfinding::green - 1) - 1;
									const int TXT_YELLOW = Palette::blockOffset(Pathfinding::yellow - 1) - 1;
									const int TXT_RED    = Palette::blockOffset(Pathfinding::red - 1) - 1;
									const int TXT_BROWN  = Palette::blockOffset(Pathfinding::brown - 1) - 1;
									const int TXT_WHITE  = Palette::blockOffset(Pathfinding::white - 1) - 1;

									const bool isCtrlPressed = _game->isCtrlPressed(true);
									const bool isKneeled = action->actor->isKneeled();
									int accuracyInteger = 0;
									double accuracy = 0.0;
									bool targetSelf = false;
									double maxExposure = 0.0;
									double distanceFloat = 0.0;
									int maxVoxels = 0;
									bool disableRA = false;

									if (Position(itX, itY, itZ) == _cacheCursorPosition && isCtrlPressed == _cacheIsCtrlPressed && isKneeled == _cacheIsKneeled && _cacheAccuracy != -1 && _cacheAccuracyTextColor != -1)
									{
										accuracyInteger = _cacheAccuracy;
										_txtAccuracy->setColor(_cacheAccuracyTextColor);
										targetSelf = _cacheTargetSelf;
									}
									else
									{
										BattleUnit* shooterUnit = action->actor;
										const Mod::AccuracyModConfig *AccuracyMod = _game->getMod()->getAccuracyModConfig();
										int distanceVoxels = 0;

										auto* ammo = attack.damage_item;
										const RuleItem *ammoRule = (ammo != nullptr) ? ammo->getRules() : nullptr;

										bool isShotgun = ammoRule && ammoRule->getShotgunPellets() != 0 && ammoRule->getDamageType()->isDirect();
										bool isArcingShot = action->weapon->getArcingShot(action->type);
										bool isSpray = action->sprayTargeting;
										disableRA = isShotgun || isArcingShot || isSpray;

										if (unit && unit == shooterUnit)
										{
											targetSelf = true;
										}
										else
										{
											Tile *targetTile = nullptr;
											std::vector<Position> exposedVoxels;

											if (unit && unit->getVisible()) // Targeting a unit
											{
												targetSize = unit->getArmor()->getSize();
												targetTile = unit->getTile();
												exposedVoxels.reserve((1 + BattleUnit::BIG_MAX_RADIUS * 2) * TileEngine::voxelTileSize.z / 2);

												// This is needed inside getOriginVoxel() to get direction
												action->target = unit->getPosition();

												Position selectedOrigin = TileEngine::invalid;
												std::vector<BattleActionOrigin> originTypes;
												originTypes.push_back(BattleActionOrigin::CENTRE);
												if (Options::oxceEnableOffCentreShooting)
												{
													originTypes.push_back(BattleActionOrigin::LEFT);
													originTypes.push_back(BattleActionOrigin::RIGHT);
												}

												// Find shooting point with best target's exposure
												for (const auto &relPos : originTypes)
												{
													exposedVoxels.clear();
													action->relativeOrigin = relPos;
													Position origin = _save->getTileEngine()->getOriginVoxel(*action, shooterUnit->getTile());
													double exposure = _save->getTileEngine()->checkVoxelExposure(&origin, targetTile, shooterUnit, false, &exposedVoxels, nullptr, false);

													if (relPos == BattleActionOrigin::CENTRE || (int)exposedVoxels.size() > maxVoxels)
													{
														selectedOrigin = origin;
														maxVoxels = exposedVoxels.size();
														maxExposure = exposure;
													}
												}
												action->relativeOrigin = BattleActionOrigin::CENTRE; // Reset to default! It's used elsewhere
												distanceVoxels = unit->distance3dToPositionPrecise(selectedOrigin) - shooterUnit->getRadiusVoxels();
											}
											else if (shooterUnit->getTile()) // Targeting an empty tile
											{
												action->relativeOrigin = BattleActionOrigin::CENTRE;
												action->target = Position{itX, itY, itZ};
												Position origin = _save->getTileEngine()->getOriginVoxel(*action, shooterUnit->getTile());
												// NOTE (Calypso): the upstream RA path calls adjustTargetVoxelFromTileType here;
												// that TileEngine refactor is out of 34.5b scope, so we approximate the aim point
												// with the tile centre for the distance estimate (cursor display only).
												Position targetPos = action->target.toVoxel() + TileEngine::voxelTileCenter;
												distanceVoxels = Position::distance(origin, targetPos) - shooterUnit->getRadiusVoxels();
											}

											accuracy = static_cast<double>(BattleUnit::getFiringAccuracy(attack, _game->getMod()));
											distanceFloat = (double)distanceVoxels / Position::TileXY;

											int upperLimit, lowerLimit;
											int dropoff = weapon->calculateLimits(upperLimit, lowerLimit, _save->getDepth(), action->type);

											_txtAccuracy->setColor(TXT_YELLOW);
											if (distanceFloat > upperLimit)
											{
												accuracy -= (distanceFloat - upperLimit) * dropoff;
											}
											else if (distanceFloat < lowerLimit)
											{
												accuracy -= (lowerLimit - distanceFloat) * dropoff;
											}
											else
											{
												_txtAccuracy->setColor(TXT_GREEN);
											}

											int noLOSAccuracyPenalty = weapon->getNoLOSAccuracyPenalty(_game->getMod());
											if (noLOSAccuracyPenalty != -1)
											{
												bool hasLOS = false;
												if (Position(itX, itY, itZ) == _cacheCursorPosition && isCtrlPressed == _cacheIsCtrlPressed && _cacheHasLOS != -1)
												{
													hasLOS = (_cacheHasLOS == 1);
												}
												else
												{
													if (unit && (unit->getVisible() || _save->getDebugMode()))
													{
														hasLOS = _save->getTileEngine()->visible(action->actor, tile);
													}
													else
													{
														hasLOS = _save->getTileEngine()->isTileInLOS(action, tile, false);
													}
													_cacheHasLOS = hasLOS ? 1 : 0;
												}

												if (!hasLOS)
												{
													accuracy *= (double)noLOSAccuracyPenalty / 100.0;
													_txtAccuracy->setColor(TXT_YELLOW);
												}
											}

											int snipingBonus = (round(accuracy) > 100 ? round((accuracy - 100) / 2) : 0);
											bool isSniperShot = (snipingBonus > 0 && !disableRA);

											bool coverHasEffect = AccuracyMod->coverEfficiency[(int)Options::battleRealisticCoverEfficiency];
											if (unit && maxVoxels > 0 && coverHasEffect && !disableRA)
											{
												// Apply the exposure
												double coverEfficiencyCoeff = AccuracyMod->coverEfficiency[(int)Options::battleRealisticCoverEfficiency] / 100.0;
												accuracy = accuracy * coverEfficiencyCoeff * maxExposure + accuracy * (1.0 - coverEfficiencyCoeff);
											}

											accuracyInteger = round(accuracy);
											distance = round(distanceFloat);
											if (distance < 1) distance = 1;

											accuracyInteger = Projectile::getHitChance(distance, accuracyInteger, _game->getMod()->getHitChancesTable(targetSize));

											if (Options::battleRealisticImprovedAimed && isSniperShot)
											{
												accuracyInteger += snipingBonus;
											}

											int distanceSq = action->actor->distance3dToPositionSq(Position(itX, itY, itZ));
											bool outOfRange = weapon->isOutOfRange(distanceSq);

											if (isSniperShot)
											{
												_txtAccuracy->setColor(TXT_WHITE);
											}

											if (outOfRange)
											{
												accuracyInteger = 0;
												_txtAccuracy->setColor(TXT_BROWN);
											}
											else if (unit && (unit->getVisible() || _save->getDebugMode()) && maxVoxels == 0)
											{
												_txtAccuracy->setColor(TXT_BROWN);
											}

											_cacheCursorPosition = Position(itX, itY, itZ);
											_cacheIsCtrlPressed = isCtrlPressed;
											_cacheAccuracyTextColor = _txtAccuracy->getColor();
											_cacheAccuracy = accuracyInteger;
											_cacheIsKneeled = isKneeled;
											_cacheTargetSelf = targetSelf;
										}
									}

									if (isCtrlPressed && maxVoxels > 0)
									{
										int currentColor = TXT_RED;
										if (disableRA) currentColor = TXT_BROWN;
										else if (maxExposure > 0.65) currentColor = TXT_GREEN;
										else if (maxExposure > 0.35) currentColor = TXT_YELLOW;
										_txtAccuracy->setColor(currentColor);
										ss << "> " << std::round(maxExposure * 100) << "% <";
									}
									else if (targetSelf)
									{
										ss.str("");
										ss.clear();
									}
									else
									{
										ss << accuracyInteger << "%";
									}
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
								_numWaypid->setBordered(true); // OXCE, not configurable
								_numWaypid->draw();
								_numWaypid->blitNShade(surface, screenPosition.x + waypXOff, screenPosition.y + waypYOff, 0);

								waypXOff += waypid > 9 ? 10 : 6; // OXCE
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
						// Phase 24 UX (Stage 3): in GPU mode the HD marching path node (emitted
						// in the back pass) replaces these flat front arrows — skip them here.
						if (_previewSettingArrows
#ifdef __EMSCRIPTEN__
						    && !gpuCursorSet
#endif
						   )
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
							// Phase 24: marker offsets were tuned for a 32×40 tile; scale the
							// tile-relative parts with the (possibly doubled) sprite size so the
							// numbers stay centred at battlescapeTileScale > 1. At native 32×40
							// these reduce to the original 16/22/29. off/offE are glyph-width
							// nudges (native glyph) and stay unscaled.
							const int cx    = _spriteWidth / 2;          // 16 at native width 32
							const int yRowA = _spriteHeight * 22 / 40;   // 22 at native height 40
							const int yRowB = _spriteHeight * 29 / 40;   // 29 at native height 40
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
									_numWaypid->blitNShade(surface, screenPosition.x + cx - off, screenPosition.y + (yRowA - adjustment), 0, false, mcolor);
									// and Energy
									_numWaypid->setValue(tile->getEnergyMarker());
									_numWaypid->draw();
									_numWaypid->blitNShade(surface, screenPosition.x + cx - offE, screenPosition.y + (yRowB - adjustment), 0, false, mcolor);
								}
								else
								{
									// only TU
									_numWaypid->blitNShade(surface, screenPosition.x + cx - off, screenPosition.y + (yRowB - adjustment), 0, false, mcolor);
								}
							}
							else if (_previewSettingEnergy)
							{
								// only Energy
								_numWaypid->setValue(tile->getEnergyMarker());
								_numWaypid->draw();
								_numWaypid->blitNShade(surface, screenPosition.x + cx - offE, screenPosition.y + (yRowB - adjustment), 0, false, mcolor);
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
			offset.y += _spriteHeight / 10;   // scales with tileScale (4 at native sh=40)
		}
		offset.y += Position::TileZ - (selectedUnit->getHeight() + selectedUnit->getFloatHeight());
		if (selectedUnit->isKneeled())
		{
			offset.y -= _spriteHeight / 20;   // scales with tileScale (2 at native sh=40)
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
					offset.y += _spriteHeight / 10;   // scales with tileScale (4 at native sh=40)
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
					offset.y -= _spriteHeight / 20;   // scales with tileScale (2 at native sh=40)
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

	// Phase 32: fear indicator — a bobbing alarm marker over a visible, terrified civilian
	// (smart civilian whose morale has cracked), so the player can see who is about to bolt
	// or freeze. Drawn unconditionally (a persistent status, like a wound), not Alt-gated.
	if (_save->getMod()->getAISmartCivilians())
	{
		for (auto* civ : *_save->getUnits())
		{
			if (civ->isOrganicCivilian()
				&& !civ->isOut()
				&& civ->getVisible()
				&& civ->getPosition().z <= _camera->getViewLevel() // don't project through floors above
				&& civ->getMorale() < 50)
			{
				_camera->convertMapToScreen(civ->getPosition(), &screenPosition);
				screenPosition += _camera->getMapOffset();
				Position offset = calculateWalkingOffset(civ).ScreenOffset;
				offset.y += Position::TileZ - (civ->getHeight() + civ->getFloatHeight());
				if (civ->isKneeled())
				{
					offset.y -= _spriteHeight / 20;   // scales with tileScale (2 at native sh=40)
				}
				Surface::blitRaw(
					surface,
					_arrow,
					screenPosition.x + offset.x + (_spriteWidth / 2) - (_arrow->getWidth() / 2),
					screenPosition.y + offset.y - _arrow->getHeight() + getArrowBobForFrame(_animFrame),
					0,
					false,
					_isTFTD ? ArrowColorsTFTD[1] : ArrowColorsUFO[1]); // orange / red = alarm
			}
		}
	}

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

#ifdef __EMSCRIPTEN__
	// Calypso P30: age hit-jolt counters; expire spent impact flashes.
	for (auto it = _unitShakeOffset.begin(); it != _unitShakeOffset.end(); )
	{
		if (--it->second.framesLeft <= 0) it = _unitShakeOffset.erase(it);
		else ++it;
	}
	{
		const unsigned int now = SDL_GetTicks();
		_impactFlashes.erase(
			std::remove_if(_impactFlashes.begin(), _impactFlashes.end(),
				[now](const ImpactFlash& f){ return (now - f.spawnTick) >= f.delayMs + (unsigned int)f.lifeMs; }),
			_impactFlashes.end());
		_fxParticles.erase(
			std::remove_if(_fxParticles.begin(), _fxParticles.end(),
				[now](const FxParticle& p){ return (now - p.spawnTick) >= p.delayMs + (unsigned int)p.lifeMs; }),
			_fxParticles.end());
		_shockwaves.erase(
			std::remove_if(_shockwaves.begin(), _shockwaves.end(),
				[now](const Shockwave& w){ return (now - w.spawnTick) >= (unsigned int)w.lifeMs; }),
			_shockwaves.end());
	}
#endif

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
	// Phase 24: the per-phase walk step is in screen pixels and was tuned for a
	// 32×40 tile (x:y = 2:1). Scale it with the (possibly doubled) sprite size so
	// a walking unit traverses a full tile instead of crossing half of it and
	// snapping. At native 32×40 these are 2 and 1 — unchanged behaviour.
	const int stepX = _spriteWidth / 16;
	const int stepY = _spriteWidth / 32;
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
			result.ScreenOffset.x = phase * stepX * offsetX[dir];
			result.ScreenOffset.y = - phase * stepY * offsetY[dir];
		}
		else
		{
			result.ScreenOffset.x = (phase - endphase) * stepX * offsetX[dir];
			result.ScreenOffset.y = - (phase - endphase) * stepY * offsetY[dir];
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
	// Phase 24: TerrainLevelOffset is a vertical screen offset in native pixels
	// (raised floors, stairs, flight); scale it with the tile height so units sit
	// on the floor at battlescapeTileScale>1. The unscaled value is kept for shade.
	// Native 40-tall tile -> factor 1 (unchanged).
	result.ScreenOffset.y += result.TerrainLevelOffset * _spriteHeight / 40;
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
	_cacheAccuracy = -1;
	_cacheAccuracyTextColor = -1;

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
