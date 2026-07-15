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
 * Calypso — emscripten-only HD HUD render/layout methods for BattlescapeState.
 * Extracted from Battlescape/BattlescapeState.cpp (Phase 36). Member declarations
 * remain in Battlescape/BattlescapeState.h inside its #ifdef __EMSCRIPTEN__ section.
 */
#ifdef __EMSCRIPTEN__

#include <algorithm>
#include <sstream>
#include <iomanip>
#include "../fmath.h"
#include <SDL_gfxPrimitives.h>
#include "../Battlescape/Map.h"
#include "../Battlescape/Camera.h"
#include "../Battlescape/BattlescapeState.h"
#include "../Battlescape/AbortMissionState.h"
#include "../Battlescape/TileEngine.h"
#include "../Battlescape/ActionMenuState.h"
#include "../Battlescape/SkillMenuState.h"
#include "../Battlescape/UnitInfoState.h"
#include "../Battlescape/InventoryState.h"
#include "../Battlescape/AlienInventoryState.h"
#include "../Battlescape/Pathfinding.h"
#include "../Battlescape/BattlescapeGame.h"
#include "../Battlescape/WarningMessage.h"
#include "../Battlescape/InfoboxState.h"
#include "../Battlescape/NoExperienceState.h"
#include "../Battlescape/ExperienceOverviewState.h"
#include "../Battlescape/TurnDiaryState.h"
#include "../Battlescape/DebriefingState.h"
#include "../Battlescape/MiniMapState.h"
#include "../Battlescape/BattlescapeGenerator.h"
#include "../Battlescape/BriefingState.h"
#include "../Battlescape/ExtendedBattlescapeLinksState.h"
#include "../lodepng.h"
#include "../Geoscape/SelectMusicTrackState.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Palette.h"
#include "../Engine/Surface.h"
#include "../Engine/SurfaceSet.h"
#include "../Engine/Screen.h"
#include "../Engine/Sound.h"
#include "../Engine/Action.h"
#include "../Engine/Script.h"
#include "../Engine/Logger.h"
#include "../Engine/Timer.h"
#include "../Engine/CrossPlatform.h"
#include "../Interface/Cursor.h"
#ifdef __EMSCRIPTEN__
#include "../Engine/GpuInit.h"
extern "C" void calypso_log_heap(const char *tag);  // M5: defined in Calypso/EmscriptenHarness.cpp
extern "C" int  g_calypsoTabHiddenPause;            // M6h: set by calypso_on_tab_hidden()
#endif
#include "../Interface/Text.h"
#include "../Interface/Bar.h"
#include "../Interface/BattlescapeButton.h"
#include "../Interface/NumberText.h"
#include "../Menu/CutsceneState.h"
#include "../Menu/PauseState.h"
#include "../Menu/LoadGameState.h"
#include "../Menu/SaveGameState.h"
#include "../Mod/Mod.h"
#include "../Engine/TTFFont.h"
#include "../Engine/TTFUtil.h"
#include <cmath>
#include "../Mod/RuleItem.h"
#include "../Mod/AlienDeployment.h"
#include "../Mod/Armor.h"
#include "../Mod/RuleUfo.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Calypso/CalypsoTutorial.h"
#include "../Savegame/Tile.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/BattleItem.h"
#include "../Savegame/HitLog.h"
#include "../Ufopaedia/Ufopaedia.h"
#include "../Savegame/Ufo.h"
#include "../Mod/RuleEnviroEffects.h"
#include "../Mod/RuleInterface.h"
#include "../Mod/RuleInventory.h"
#include "../Mod/RuleSoldier.h"
#include "../Mod/RuleVideo.h"
#include <algorithm>


namespace OpenXcom
{

/**
 * Calypso (Emscripten): render the visible-enemy indicator digit (i+1) into the
 * TTF GL overlay (Map HUD_TXT_VISIBLE_0+i), auto-fit + centred in the (resolution-
 * scaled) box. Crisp at every zoom and always fits, unlike the fixed-size bitmap
 * NumberText, which we keep hidden.
 */
void BattlescapeState::drawVisibleUnitDigit(int i)
{
	if (!_map || !_btnVisibleUnit[i]) return;
	_numVisibleUnit[i]->setVisible(false);   // GL digit is the sole renderer now
	const int bx = _btnVisibleUnit[i]->getX(), by = _btnVisibleUnit[i]->getY();
	const int bw = _btnVisibleUnit[i]->getWidth(), bh = _btnVisibleUnit[i]->getHeight();
	TTFFont* font = getHudFont();
	const std::string digits = std::to_string(i + 1);
	if (font)
	{
		SDL_Color white = { 255, 255, 255, 255 };
		SDL_Surface* ttf = font->renderText(digits, white);   // cached by TTFFont — do NOT free
		if (ttf && ttf->w > 0 && ttf->h > 0)
		{
			const float availW = bw * 0.8f, availH = bh * 0.8f;
			float scale = (availH / ttf->h < availW / ttf->w) ? availH / ttf->h : availW / ttf->w;
			if (scale > 1.0f) scale = 1.0f;
			int outW = (int)(ttf->w * scale + 0.5f); if (outW < 1) outW = 1;
			int outH = (int)(ttf->h * scale + 0.5f); if (outH < 1) outH = 1;
			const int ox = (bw - outW) / 2, oy = (bh - outH) / 2;
			_map->setHudText(Map::HUD_TXT_VISIBLE_0 + i, (float)(bx + ox), (float)(by + oy),
			                 (float)outW, (float)outH, digits, 0xFFFFFFFFu);
			return;
		}
	}
	_map->clearHudText(Map::HUD_TXT_VISIBLE_0 + i);
}

/**
 * Fill a 32-bit surface with a rounded rectangle: transparent outside the
 * rounded corners, an `accent` border ring of width `bw`, and a dark `fill`.
 */
static void drawRoundedBox(SDL_Surface* s, SDL_Color fill, SDL_Color accent, int radius, int bw)
{
	if (!s || s->format->BitsPerPixel != 32) return;
	const int W = s->w, H = s->h;
	if (radius * 2 > W) radius = W / 2;
	if (radius * 2 > H) radius = H / 2;
	SDL_LockSurface(s);
	const Uint32 cTrans  = SDL_MapRGBA(s->format, 0, 0, 0, 0);
	const Uint32 cAccent = SDL_MapRGBA(s->format, accent.r, accent.g, accent.b, accent.a);
	const Uint32 cFill   = SDL_MapRGBA(s->format, fill.r, fill.g, fill.b, fill.a);
	for (int y = 0; y < H; ++y)
	{
		Uint32* row = (Uint32*)((Uint8*)s->pixels + y * s->pitch);
		for (int x = 0; x < W; ++x)
		{
			const int dx = (x < radius) ? (radius - x) : (x > W - 1 - radius ? x - (W - 1 - radius) : 0);
			const int dy = (y < radius) ? (radius - y) : (y > H - 1 - radius ? y - (H - 1 - radius) : 0);
			if (dx > 0 && dy > 0)
			{
				const float dist = std::sqrt((float)(dx * dx + dy * dy));
				if (dist > radius)            { row[x] = cTrans;  continue; }
				if (dist > radius - bw)       { row[x] = cAccent; continue; }
			}
			else if (x < bw || x >= W - bw || y < bw || y >= H - bw)
			{
				row[x] = cAccent; continue;
			}
			row[x] = cFill;
		}
	}
	SDL_UnlockSurface(s);
}


void BattlescapeState::captureHudNativeGl()
{
	if (_hudCaptured || !_icons) return;
	_hudNativeIconsW = _icons->getWidth();
	_hudNativeIconsH = _icons->getHeight();
	const int ix = _icons->getX(), iy = _icons->getY();
	for (auto* surf : _surfaces)
	{
		if (surf == _map || surf == _txtDebug || surf == _portrait
			|| surf == _btnCtrl || surf == _btnAlt || surf == _btnShift
			|| surf == _btnRMB || surf == _btnMMB
			|| surf == _btnPsi || surf == _btnLaunch
			|| surf == _btnSpecial || surf == _btnSkills)
		{
			continue;
		}
		_hudNative.push_back({ surf, surf->getX() - ix, surf->getY() - iy,
		                       surf->getWidth(), surf->getHeight() });
	}
	_hudCaptured = true;
}

void BattlescapeState::layoutHudGl()
{
	if (!_hudCaptured || _hudNativeIconsW <= 0) return;
	if (Options::baseXResolution == _hudLastBaseX) return;   // nothing changed
	_hudLastBaseX = Options::baseXResolution;

	// HD panel art (its aspect differs from the vanilla 320x56 bar — it's taller),
	// so size the panel to the HD aspect and scale widget geometry NON-uniformly
	// (sx by width, sy by height) onto it. The HD layout mirrors the vanilla
	// proportional layout, so the click grid + dynamic widgets land on the art.
	Surface* panel = _game->getMod()->getSurface("CALYPSO_HUD_PANEL", false);
	const int pw = (panel && panel->getSurface()) ? panel->getSurface()->w : 0;
	const int ph = (panel && panel->getSurface()) ? panel->getSurface()->h : 0;

	const int newW = Options::baseXResolution / 2;            // ~half screen width
	const int newH = (pw > 0 && ph > 0)
	               ? (int)((float)newW * ph / pw + 0.5f)      // HD panel aspect
	               : (int)((float)newW * _hudNativeIconsH / _hudNativeIconsW + 0.5f);

	float sx = (float)newW / (float)_hudNativeIconsW; if (sx < 1.0f) sx = 1.0f;
	float sy = (float)newH / (float)_hudNativeIconsH; if (sy < 1.0f) sy = 1.0f;
	_hudScale = sx;

	const int panelX = Options::baseXResolution / 2 - newW / 2;
	const int panelY = Options::baseYResolution - newH;
	// Clip the map's overlay/vapor passes to just above the (taller) HD HUD panel.
	_map->setHudTopY(panelY);

	// Publish the HD "toggled" panel + the live panel transform so a pressed/
	// toggled BattlescapeButton can blit its own gold region (top-left aligned).
	Surface* toggled = _game->getMod()->getSurface("CALYPSO_HUD_PANEL_TOGGLED", false);
	if (toggled && toggled->getSurface())
	{
		BattlescapeButton::hudToggled = toggled->getSurface();
		BattlescapeButton::hudSrcW = toggled->getSurface()->w;
		BattlescapeButton::hudSrcH = toggled->getSurface()->h;
		BattlescapeButton::hudPanelX = panelX;
		BattlescapeButton::hudPanelY = panelY;
		BattlescapeButton::hudPanelW = newW;
		BattlescapeButton::hudPanelH = newH;
	}
	else
	{
		BattlescapeButton::hudToggled = nullptr;
	}

	for (const auto& r : _hudNative)
	{
		// _rank holds an externally-blitted sprite that resize()/clear() would
		// wipe; it is positioned (no resize) by placePos() below — skip it here.
		if (r.surf == _rank) continue;
		r.surf->setX(panelX + (int)(r.dx * sx + 0.5f));
		r.surf->setY(panelY + (int)(r.dy * sy + 0.5f));
		if (r.surf == _icons)
		{
			r.surf->setWidth(newW);
			r.surf->setHeight(newH);
		}
		else
		{
			int w = (int)(r.w * sx + 0.5f); if (w < 1) w = 1;
			int h = (int)(r.h * sy + 0.5f); if (h < 1) h = 1;
			r.surf->setWidth(w);
			r.surf->setHeight(h);
		}
	}

	// Calypso: the visible-enemy indicator column. The generic loop above scaled
	// these boxes by the HUD panel's sy (~half screen) — huge boxes, and the
	// 18*sy pitch fanned the column off-screen. Instead give them a CONSTANT
	// on-screen size + pitch: the buffer size is the target canvas px scaled by
	// baseX/displayWidth, so the stretch-to-canvas cancels and the cubes look
	// identical at every zoom level. The digit is drawn crisp + auto-fit by the
	// TTF GL overlay (drawVisibleUnitDigit) so the bitmap NumberText is hidden.
	{
		const int dispW = std::max(1, Options::displayWidth);
		const int boxW = std::max(6, 22 * Options::baseXResolution / dispW);
		const int boxH = std::max(5, 18 * Options::baseXResolution / dispW);
		const int step = std::max(boxH + 1, 21 * Options::baseXResolution / dispW);
		if (_btnVisibleUnit[0])
		{
			const int rightX = _btnVisibleUnit[0]->getX() + _btnVisibleUnit[0]->getWidth();
			const int topY   = _btnVisibleUnit[0]->getY() + _btnVisibleUnit[0]->getHeight();
			for (int i = 0; i < VISIBLE_MAX; ++i)
			{
				if (_btnVisibleUnit[i])
				{
					_btnVisibleUnit[i]->setX(rightX - boxW);
					_btnVisibleUnit[i]->setY(topY - boxH - i * step);
					_btnVisibleUnit[i]->setWidth(boxW);
					_btnVisibleUnit[i]->setHeight(boxH);
				}
				if (_numVisibleUnit[i]) _numVisibleUnit[i]->setVisible(false);  // replaced by the GL digit
			}
		}
	}

	// Explicit placement of the centre-slot dynamic widgets into the HD panel
	// (panel-normalised coords). The HD layout differs from vanilla's, so the
	// generic scale above only gets these roughly right; pin them to the slot.
	// (Bars are procedural → scale with the surface; Text/NumberText use bitmap
	// fonts that don't scale by size yet — HD font is a later step.)
	auto place = [&](Surface* s, float nx, float ny, float nw, float nh)
	{
		if (!s) return;
		// Only touch widgets that were actually captured (created + added). Some
		// HUD widgets (e.g. _barMana) are optional and may be an uninitialised
		// pointer when absent — calling through it would crash ("null function").
		bool captured = false;
		for (const auto& r : _hudNative) { if (r.surf == s) { captured = true; break; } }
		if (!captured) return;
		s->setX(panelX + (int)(nx * newW + 0.5f));
		s->setY(panelY + (int)(ny * newH + 0.5f));
		int w = (int)(nw * newW + 0.5f); if (w < 1) w = 1;
		int h = (int)(nh * newH + 0.5f); if (h < 1) h = 1;
		s->setWidth(w); s->setHeight(h);
	};
	// HD rank: a square shoulder-board plate (the HD art is 1:1). Size + position
	// it here, then (re)blit the insignia via applyHdRank AFTER the resize so
	// Surface::draw()'s clear() doesn't wipe it. _rank is skipped by the generic
	// loop above so its native 26x23 size is not forced back on.
	{
		bool cap = false;
		for (const auto& r : _hudNative) { if (r.surf == _rank) { cap = true; break; } }
		if (cap)
		{
			// New format: rank anchor moves to the far-RIGHT corner of the slot,
			// small. (The empty left cell will hold the soldier portrait.)
			int side = (int)(0.300f * newH + 0.5f); if (side < 1) side = 1;
			_rank->setX(panelX + (int)(0.775f * newW + 0.5f));
			_rank->setY(panelY + (int)(0.640f * newH + 0.5f));
			_rank->setWidth(side);
			_rank->setHeight(side);
			applyHdRank(_hudRankIndex);
		}
	}
	// Soldier portrait in the empty cell just left of the slot (panel art cell
	// ~x 0.355..0.42, y 0.625..0.94). Square; re-filled after the resize.
	if (_portrait)
	{
		int pside = (int)(0.300f * newH + 0.5f); if (pside < 1) pside = 1;
		_portrait->setX(panelX + (int)(0.344f * newW + 0.5f));
		_portrait->setY(panelY + (int)(0.640f * newH + 0.5f));
		_portrait->setWidth(pside);
		_portrait->setHeight(pside);
		applyPortrait(_save ? _save->getSelectedUnit() : nullptr);
	}
	// Target format: name across the top, bars stacked beneath it (left), a 2x2
	// number grid to the right, rank anchor in the far-right corner.
	place(_txtName,     0.435f, 0.620f, 0.250f, 0.130f);  // name, top-left of the slot
	// bars stacked directly under the name
	place(_barTimeUnits, 0.435f, 0.72f, 0.210f, 0.040f);
	place(_barEnergy,    0.435f, 0.77f, 0.210f, 0.040f);
	place(_barHealth,    0.435f, 0.82f, 0.210f, 0.040f);
	place(_barMorale,    0.435f, 0.87f, 0.210f, 0.040f);
	place(_barMana,      0.435f, 0.92f, 0.210f, 0.035f);
	// Calypso: gradient + calibrate the scale so a value of 100 ends about half a
	// number-box short of the box grid (which starts at the left column, 0.652).
	{
		const float barStartN = 0.435f, boxLeftN = 0.652f, halfBoxN = 0.0416f * 0.5f;
		const float len100N = (boxLeftN - halfBoxN) - barStartN;   // panel-norm length at value 100
		const double barScale = (double)(len100N * newW) / 100.0;
		// Only touch bars that were actually captured (created + added). _barMana is
		// optional and may be an UNINITIALISED pointer when absent — a bare `if (b)`
		// passes on garbage and setGradient()/setScale() then write out of bounds
		// (the resolution-change / restart crash). Mirror place()'s captured guard.
		auto isCaptured = [&](Surface* s) {
			for (const auto& r : _hudNative) if (r.surf == s) return true;
			return false;
		};
		Bar* gbars[] = { _barTimeUnits, _barEnergy, _barHealth, _barMorale, _barMana };
		for (Bar* b : gbars)
			if (b && isCaptured(b)) { b->setGradient(true); b->setScale(barScale); }
	}
	// 2x2 stat-number grid (coloured boxes), right of the bars, left of the rank.
	place(_numTimeUnits, 0.652f, 0.640f, 0.0416f, 0.136f);
	place(_numEnergy,    0.712f, 0.640f, 0.0416f, 0.136f);
	place(_numHealth,    0.652f, 0.790f, 0.0416f, 0.136f);
	place(_numMorale,    0.712f, 0.790f, 0.0416f, 0.136f);
	applyHudName(_save ? _save->getSelectedUnit() : nullptr);
	applyHudNumbers(_save ? _save->getSelectedUnit() : nullptr);

	// Draw the panel background scaled into _icons (32-bit ARGB in this build).
	// Prefer the HD panel; fall back to a stretched vanilla ICONS crop.
	if (pw > 0 && _icons->getSurface())
	{
		// copy (not blend) so the panel's own alpha — rounded corners + cut-outs —
		// lands in _icons verbatim; _icons then alpha-composites over the scene.
		SDL_SetSurfaceBlendMode(panel->getSurface(), SDL_BLENDMODE_NONE);
		SDL_Rect src{ 0, 0, pw, ph };
		SDL_Rect dst{ 0, 0, newW, newH };
		SDL_BlitScaled(panel->getSurface(), &src, _icons->getSurface(), &dst);
	}
	else
	{
		Surface* icons = _game->getMod()->getSurface("ICONS.PCK", false);
		if (icons && icons->getSurface() && _icons->getSurface())
		{
			SDL_Rect src{ 0, 200 - _hudNativeIconsH, _hudNativeIconsW, _hudNativeIconsH };
			SDL_Rect dst{ 0, 0, newW, newH };
			SDL_BlitScaled(icons->getSurface(), &src, _icons->getSurface(), &dst);
		}
	}
	// setWidth() set _redraw on _icons; Surface::draw() would clear() it on the
	// next blit and wipe the panel we just drew. Keep it.
	_icons->setRedraw(false);
}

void BattlescapeState::applyHdRankGl(int rankIdx)
{
	_hudRankIndex = rankIdx;
	if (!_rank || !_rank->getSurface()) return;
	_rank->clear();   // logical slot stays clear; the HD insignia is drawn by the GL overlay
	bool queued = false;
	if (rankIdx >= 0)
	{
		std::ostringstream ss;
		ss << "CALYPSO_RANK_" << rankIdx;
		Surface* hd = _game->getMod()->getSurface(ss.str(), false);
		if (hd && hd->getSurface() && _map)
		{
			// Draw the HD insignia at PHYSICAL resolution over the rank slot (crisp at the low
			// resolution-menu fractions); the source is GPU-stretched to the slot rect, replacing
			// the SDL_BlitScaled-into-tiny-logical-surface path that pixelated it.
			_map->setHudImage(Map::HUD_IMG_RANK, ss.str(), hd->getSurface(),
			                  _rank->getX(), _rank->getY(), _rank->getWidth(), _rank->getHeight());
			queued = true;
		}
	}
	if (!queued && _map) _map->clearHudImage(Map::HUD_IMG_RANK);
	_rank->setRedraw(false);
}

void BattlescapeState::applyPortraitGl(BattleUnit* unit)
{
	if (!_portrait || !_portrait->getSurface()) return;
	_portrait->clear();   // logical slot stays clear; the head is drawn by the GL overlay
	bool queued = false;
	Soldier* soldier = unit ? unit->getGeoscapeSoldier() : nullptr;
	if (soldier)
	{
		// Resolve the inventory look sprite (mirrors InventoryState / the avatar
		// branch): "<inventorySprite><gender><look+variant>.SPK", with fallbacks.
		const std::string look = soldier->getArmor()->getSpriteInventory();
		const std::string gender = soldier->getGender() == GENDER_MALE ? "M" : "F";
		Surface* surf = nullptr;
		std::string matched;
		for (int i = 0; i <= RuleSoldier::LookVariantBits; ++i)
		{
			std::ostringstream ss;
			ss << look << gender
			   << ((int)soldier->getLook() + (soldier->getLookVariant() & (RuleSoldier::LookVariantMask >> i)) * 4)
			   << ".SPK";
			surf = _game->getMod()->getSurface(ss.str(), false);
			if (surf) { matched = ss.str(); break; }
		}
		if (!surf) { surf = _game->getMod()->getSurface(look + ".SPK", false); matched = look + ".SPK"; }
		if (!surf) { surf = _game->getMod()->getSurface(look, false); matched = look; }
		if (surf && surf->getSurface() && _map)
		{
			// The look .SPK is figure-on-transparent anchored in 320x200 space;
			// crop head + shoulders (tuned to the inventory paperdoll head).
			SDL_Surface* src = surf->getSurface();
			// Head + shoulders only (measured from the rendered paperdoll: the head
			// sits at sprite ~x74..95, y47..67; widen a touch for the shoulders).
			SDL_Rect srcR{ 69, 44, 34, 36 };
			if (srcR.x + srcR.w > src->w) srcR.w = src->w - srcR.x;
			if (srcR.y + srcR.h > src->h) srcR.h = src->h - srcR.y;
			// Copy the crop 1:1 into a small ARGB surface (the SOURCE for the GL overlay), keeping
			// the original art resolution — never the (shrinking) logical slot, which is what made
			// it pixelate at low fractions. The GL quad LINEAR-samples this straight to physical.
			SDL_Surface* crop = SDL_CreateRGBSurfaceWithFormat(0, srcR.w, srcR.h, 32, SDL_PIXELFORMAT_ARGB8888);
			if (crop)
			{
				SDL_FillRect(crop, nullptr, SDL_MapRGBA(crop->format, 0, 0, 0, 0));
				SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_NONE);
				SDL_Rect d0{ 0, 0, srcR.w, srcR.h };
				SDL_BlitSurface(src, &srcR, crop, &d0);
				// Round-mask in NORMALISED UV space so it renders as a circle in the square portrait
				// slot regardless of the 34x36 source aspect; GPU LINEAR then smooths the rim edge.
				SDL_LockSurface(crop);
				for (int yy = 0; yy < crop->h; ++yy)
				{
					Uint32* row = (Uint32*)((Uint8*)crop->pixels + yy * crop->pitch);
					const float v = (yy + 0.5f) / crop->h - 0.5f;
					for (int xx = 0; xx < crop->w; ++xx)
					{
						const float u = (xx + 0.5f) / crop->w - 0.5f;
						if (u * u + v * v > 0.25f)
						{
							Uint8 rr, gg, bb, aa;
							SDL_GetRGBA(row[xx], crop->format, &rr, &gg, &bb, &aa);
							row[xx] = SDL_MapRGBA(crop->format, rr, gg, bb, 0);
						}
					}
				}
				SDL_UnlockSurface(crop);
				_map->setHudImage(Map::HUD_IMG_PORTRAIT, "portrait#" + matched, crop,
				                  _portrait->getX(), _portrait->getY(), _portrait->getWidth(), _portrait->getHeight());
				SDL_FreeSurface(crop);
				queued = true;
			}
		}
	}
	if (!queued && _map) _map->clearHudImage(Map::HUD_IMG_PORTRAIT);
	_portrait->setRedraw(false);
}

void BattlescapeState::applyHudNameGl(BattleUnit* unit)
{
	if (!_txtName || !_txtName->getSurface()) return;
	_txtName->clear();
	bool queued = false;
	TTFFont* font = getHudFont();
	if (font && unit)
	{
		std::string name = unit->getName(_game->getLanguage(), false);
		Soldier* s = unit->getGeoscapeSoldier();
		if (s && s->hasCallsign() && _save && !_save->isNameDisplay()) name = s->getCallsign();
		if (!name.empty())
		{
			SDL_Color col = { 0x7A, 0xC8, 0xFF, 0xFF };
			SDL_Surface* ttf = font->renderText(name, col);   // owned by TTFFont — do NOT free
			if (ttf && ttf->w > 0 && ttf->h > 0)
			{
				// The crisp physical-res GL overlay (Map::drawHudTextGLPass) is the SOLE renderer
				// of the name now — the logical CPU text is intentionally NOT blitted, so the mushy
				// stretched copy can never show under the overlay (incl. behind non-fullscreen
				// popups, where the overlay keeps drawing via hudOverlayVisible()). We still render
				// the TTF here purely to measure its size and pass the exact fit rect to the overlay.
				// Height-driven fit (0.72 of the field), width-capped; left-aligned, V-centred.
				const int W = _txtName->getWidth(), H = _txtName->getHeight();
				float scale = (H * 0.72f) / ttf->h;
				if (ttf->w * scale > W) scale = (float)W / ttf->w;   // never overflow the field
				int outW = (int)(ttf->w * scale + 0.5f); if (outW < 1) outW = 1;
				int outH = (int)(ttf->h * scale + 0.5f); if (outH < 1) outH = 1;
				const int ox = 0, oy = (H - outH) / 2;
				// bug 1: own only the NAME slot — no global clear (the stat-digit slots are owned by
				// applyHudNumber), so a partial refresh can't accumulate duplicate text items.
				if (_map) _map->setHudText(Map::HUD_TXT_NAME, (float)(_txtName->getX() + ox),
				                           (float)(_txtName->getY() + oy), (float)outW, (float)outH, name, 0xFF7AC8FFu);
				queued = true;
			}
		}
	}
	if (!queued && _map) _map->clearHudText(Map::HUD_TXT_NAME);
	_txtName->setRedraw(false);
}

void BattlescapeState::applyHudNumberGl(NumberText* w, int value, Uint32 accentArgb, int imgSlot, int txtSlot)
{
	if (!w || !w->getSurface()) return;
	SDL_Surface* s = w->getSurface();
	if (s->format->BitsPerPixel != 32) { w->setRedraw(false); return; }
	w->clear();   // logical widget stays clear; the ring + digits are drawn by the GL overlay
	SDL_Color accent = { (Uint8)((accentArgb >> 16) & 0xFF), (Uint8)((accentArgb >> 8) & 0xFF),
	                     (Uint8)(accentArgb & 0xFF), 0xFF };
	SDL_Color fill = { 8, 14, 22, 210 };

	// Rounded ring/box rendered at PHYSICAL pixel size, then GPU-sampled to the widget rect —
	// crisp corners at the low resolution-menu fractions (vs drawing into the tiny logical
	// surface, which the stretch then pixelated). Cached by physical size + accent colour.
	const float xs = (float)_game->getScreen()->getXScale();
	const float ys = (float)_game->getScreen()->getYScale();
	int pw = (int)(w->getWidth()  * xs + 0.5f); if (pw < 1) pw = 1; if (pw > 1024) pw = 1024;
	int ph = (int)(w->getHeight() * ys + 0.5f); if (ph < 1) ph = 1; if (ph > 1024) ph = 1024;
	SDL_Surface* box = SDL_CreateRGBSurfaceWithFormat(0, pw, ph, 32, SDL_PIXELFORMAT_ARGB8888);
	if (box && _map)
	{
		const int radius = ((ph < pw ? ph : pw) / 4);
		const int bw = (ph >= 44 ? 4 : (ph >= 22 ? 2 : 1));
		drawRoundedBox(box, fill, accent, radius, bw);
		std::ostringstream k;
		k << "box#" << pw << "x" << ph << "#" << std::hex << (accentArgb & 0xFFFFFFu);
		_map->setHudImage(imgSlot, k.str(), box, w->getX(), w->getY(), w->getWidth(), w->getHeight());
	}
	else if (_map) { _map->clearHudImage(imgSlot); }
	if (box) SDL_FreeSurface(box);   // free regardless of _map — setHudImage uploaded its own copy

	TTFFont* font = getHudFont();
	const std::string digits = std::to_string(value);
	bool txtQueued = false;
	if (font)
	{
		// digits in the exact same colour as the box/bar. The crisp GL overlay is the SOLE
		// renderer of the digits — rendered over the ring image slot. We render the TTF here only
		// to measure it and hand the exact fit rect to the overlay.
		SDL_Surface* ttf = font->renderText(digits, accent);   // owned by TTFFont — do NOT free
		if (ttf && ttf->w > 0 && ttf->h > 0)
		{
			// Min-dimension fit at 0.88 of the box, centred both ways.
			const float availW = s->w * 0.88f, availH = s->h * 0.88f;
			float scale = (availH / ttf->h < availW / ttf->w) ? availH / ttf->h : availW / ttf->w;
			if (scale > 1.0f) scale = 1.0f;
			int outW = (int)(ttf->w * scale + 0.5f); if (outW < 1) outW = 1;
			int outH = (int)(ttf->h * scale + 0.5f); if (outH < 1) outH = 1;
			const int ox = (s->w - outW) / 2, oy = (s->h - outH) / 2;
			// bug 1: own only this stat's digit slot — applyHudNumber rebuilds it each refresh, so
			// even a partial stat update (without a preceding applyHudName) can't accumulate dupes.
			if (_map) _map->setHudText(txtSlot, (float)(w->getX() + ox), (float)(w->getY() + oy),
			                           (float)outW, (float)outH, digits, 0xFF000000u | (accentArgb & 0xFFFFFFu));
			txtQueued = true;
		}
	}
	if (!txtQueued && _map) _map->clearHudText(txtSlot);
	w->setRedraw(false);
}

void BattlescapeState::applyHudNumbersGl(BattleUnit* unit)
{
	if (unit)
	{
		// box/digit colour = the exact bar colour (palette index -> RGB).
		Palette* pal = _game->getMod()->getPalette("PAL_BATTLESCAPE", false);
		auto barArgb = [&](Bar* b) -> Uint32 {
			if (!b || !pal) return 0xFFFFFFu;
			SDL_Color* c = pal->getColors(b->getColor());
			return ((Uint32)c->r << 16) | ((Uint32)c->g << 8) | (Uint32)c->b;
		};
		applyHudNumber(_numTimeUnits, unit->getTimeUnits(), barArgb(_barTimeUnits), Map::HUD_IMG_TU,     Map::HUD_TXT_TU);
		applyHudNumber(_numEnergy,    unit->getEnergy(),    barArgb(_barEnergy),    Map::HUD_IMG_ENERGY, Map::HUD_TXT_ENERGY);
		applyHudNumber(_numHealth,    unit->getHealth(),    barArgb(_barHealth),    Map::HUD_IMG_HEALTH, Map::HUD_TXT_HEALTH);
		applyHudNumber(_numMorale,    unit->getMorale(),    barArgb(_barMorale),    Map::HUD_IMG_MORALE, Map::HUD_TXT_MORALE);
	}
	else
	{
		NumberText* arr[] = { _numTimeUnits, _numEnergy, _numHealth, _numMorale };
		for (NumberText* w : arr) { if (w) { w->clear(); w->setRedraw(false); } }
		if (_map)
		{
			_map->clearHudImage(Map::HUD_IMG_TU);     _map->clearHudImage(Map::HUD_IMG_ENERGY);
			_map->clearHudImage(Map::HUD_IMG_HEALTH); _map->clearHudImage(Map::HUD_IMG_MORALE);
			_map->clearHudText(Map::HUD_TXT_TU);      _map->clearHudText(Map::HUD_TXT_ENERGY);
			_map->clearHudText(Map::HUD_TXT_HEALTH);  _map->clearHudText(Map::HUD_TXT_MORALE);
		}
	}
}

/**
 * Phase 37: register the battlescape tutorial anchor rects and fire the
 * battle-entry triggers. Extracted from BattlescapeState::init() so the
 * frozen upstream file keeps only a <=5-line hook (policy R2/R8).
 */
void BattlescapeState::calypsoTutorialBattleInit()
{
	CalypsoTutorial::get().anchorAll({
		{"bs.numTU", _numTimeUnits}, {"bs.btnKneel", _btnKneel},
		{"bs.btnEndTurn", _btnEndTurn}, {"bs.btnInventory", _btnInventory},
		{"bs.btnCenter", _btnCenter}, {"bs.btnNextSoldier", _btnNextSoldier},
		{"bs.btnAbort", _btnAbort},
		{"bs.reserveRow", _btnReserveNone, _btnReserveAuto},
		{"bs.hands", _btnLeftHandItem, _btnRightHandItem},
		{"bs.btnMapUpDown", _btnMapUp, _btnMapDown} });
	CalypsoTutorial::get().fire(_game, "battle.start");
	if (_save->getGlobalShade() >= 9)
		CalypsoTutorial::get().fire(_game, "battle.night");
}

} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
