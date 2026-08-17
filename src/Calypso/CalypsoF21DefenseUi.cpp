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
 * F21 (Calypso): HD adapter for BaseDefenseState -- see the header. One
 * atomic subgroup: window, readouts, the live list band, and the actions
 * show physically together or the logical window renders.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoF21DefenseUi.h"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>

#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/Surface.h"
#include "../Geoscape/BaseDefenseState.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Mod/Mod.h"

#include "CalypsoAbandonPopupUi.h"
#include "CalypsoF21UiShared.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoUiFamilies.h"
#include "CalypsoUiMetrics.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

enum DefenseRole : std::uint32_t
{
	ROLE_WINDOW = 1, ROLE_STATUS = 2, ROLE_PROTOCOL = 3, ROLE_TITLE = 4,
	ROLE_DEFENSES = 5, ROLE_AMMO = 6, ROLE_RATIO = 7, ROLE_PHASE = 8,
	ROLE_RESULT = 9, ROLE_FOOTER = 10, ROLE_START = 11, ROLE_SKIP = 12,
	ROLE_OK = 13, ROLE_DECORATION = 14
};

void applyRect(Surface* surface, const CalypsoF21Rect& rect)
{
	if (!surface) return;
	surface->setX(rect.x);
	surface->setY(rect.y);
	surface->setWidth(rect.width);
	surface->setHeight(rect.height);
}

} // namespace

CalypsoF21DefenseUi::~CalypsoF21DefenseUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF21DefenseUi::topState() const
{
	return _state;
}

void CalypsoF21DefenseUi::collect(CalypsoHdFrameBuilder& builder) const
{
	if (!_state || !_state->_hdLayout || !_state->_game) return;

	Mod* mod = _state->_game->getMod();
	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor body;
	CalypsoTtfSourceDescriptor mono;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_SAIRA_700", heading)) return;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F33_BODY", body)) return;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_MONO", mono)) return;

	const CalypsoHdPresentationMetrics& m = CalypsoHdUiOverlay::instance().frozenMetrics();
	const bool wide = _state->_hdWideLayout;
	const CalypsoF21DefenseLayout designLayout = calypsoF21DefenseLayout(
		wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);

	if (!_presented)
	{
		_presented = true;
		_presentedAtFrame = CalypsoHdUiOverlay::instance().frameId();
	}
	const CalypsoF21Motion motion(_presented, _presentedAtFrame,
		CalypsoF21DefenseGen::kMotionDurationMs, CalypsoF21DefenseGen::kMotionScaleFrom);

	// Generated type scale (theme roles) with the family's Engine raster
	// calibration -- never rectangle arithmetic.
	const double titlePx = (wide ? CalypsoHdThemeGen::kF21TitleWidePx : CalypsoHdThemeGen::kF21TitleCompactPx)
		* CalypsoF21DefenseGen::kEngineTextScaleTitle;
	const double dataPx = (wide ? CalypsoHdThemeGen::kF21DataWidePx : CalypsoHdThemeGen::kF21DataCompactPx)
		* CalypsoF21DefenseGen::kEngineTextScaleData;
	const double actionPx = (wide ? CalypsoHdThemeGen::kF21ActionWidePx : CalypsoHdThemeGen::kF21ActionCompactPx)
		* CalypsoF21DefenseGen::kEngineTextScaleAction;

	builder.beginSubgroup();
	const CalypsoLogicalRect winFull = f21WidgetRect(_state->_window);
	const double uiScale = designLayout.window.width > 0
		? (double)winFull.w / designLayout.window.width : 1.0;
	CalypsoF21Painter p{ builder, kF21FamilyId,
		reinterpret_cast<std::uintptr_t>(_state), 0, motion.opacity, motion.scale,
		CalypsoF21Rect{ winFull.x, winFull.y, winFull.w, winFull.h },
		m.scaleX, m.scaleY };
	p.winLogical = winFull;
	p.windowDesign = designLayout.window;
	p.uiScale = uiScale;

	const CalypsoLogicalRect canvasRect{ 0, 0, wide ? 1280 : 740, wide ? 720 : 360 };
	const bool harness = calypsoHarnessHostUp(calypsoHarnessSession());
	p.panel(canvasRect, harness ? calypsoRgba(0, 0, 0, 0xff) : CalypsoHdTheme::kBackdropDim,
		nullptr, ROLE_WINDOW);

	{
		p.styled(CalypsoLogicalRect{ winFull.x - 2, winFull.y + 8, winFull.w + 4, winFull.h },
			f21GlowStyle(CalypsoHdTheme::kShadowGlow, CalypsoHdTheme::kShadowGlowRadiusPx),
			nullptr, ROLE_WINDOW);
		p.styled(winFull, f21GlowStyle(CalypsoHdTheme::kHaloGlow, CalypsoHdTheme::kHaloGlowRadiusPx),
			nullptr, ROLE_WINDOW);
	}

	// F33 command-card language: cut frame, status divider + protocol strip,
	// separated footer with the sparse dot field.
	p.styled(winFull, f21WindowStyle(), _state->_window, ROLE_WINDOW);
	{
		const CalypsoLogicalRect statusRect = p.project(designLayout.status);
		const CalypsoLogicalRect footerRect = p.project(designLayout.footer);
		p.decoration(CalypsoLogicalRect{ statusRect.x, statusRect.y + statusRect.h - 1, statusRect.w, 1 },
			kF21DividerRgba, ROLE_DECORATION);
		p.decoration(CalypsoLogicalRect{ footerRect.x, footerRect.y, footerRect.w, 1 },
			kF21DividerRgba, ROLE_FOOTER);
		for (int y = footerRect.y + 10; y < footerRect.y + footerRect.h - 8; y += 8)
		{
			for (int x = footerRect.x + 12; x < f21WidgetRect(_state->_btnStart).x - 12; x += 8)
				p.decoration(CalypsoLogicalRect{ x, y, 1, 1 }, kF21FooterDotRgba, ROLE_DECORATION);
		}
		p.textRect(f21ProtocolTextRect(_state->_hdProtocol, wide), _state->_hdProtocol,
			mono, _state->_hdProtocol ? _state->_hdProtocol->getText() : std::string(),
			kF21ProtocolTextRgba,
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_PROTOCOL,
			0.10, wide ? CalypsoHdThemeGen::kF21ProtocolWidePx : CalypsoHdThemeGen::kF21ProtocolCompactPx);
	}
	if (_state->_btnStart && _state->_btnStart->getVisible())
	{
		p.styled(f21WidgetRect(_state->_btnStart), f21ButtonStyleFor(
			CalypsoActionTone::Safe, f21ButtonVisualState(_state->_btnStart)),
			_state->_btnStart, ROLE_START);
		p.text(_state->_btnStart, heading, _state->_btnStart->getText(), CalypsoHdTheme::kNearWhite,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_START,
			CalypsoHdTheme::kLabelTrackingEm, actionPx);
	}
	if (_state->_btnAbort && _state->_btnAbort->getVisible())
	{
		p.styled(f21WidgetRect(_state->_btnAbort), f21ButtonStyleFor(
			CalypsoActionTone::Destructive, f21ButtonVisualState(_state->_btnAbort)),
			_state->_btnAbort, ROLE_SKIP);
		p.text(_state->_btnAbort, heading, _state->_btnAbort->getText(), CalypsoHdTheme::kNearWhite,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_SKIP,
			CalypsoHdTheme::kLabelTrackingEm, actionPx);
	}
	if (_state->_btnOk && _state->_btnOk->getVisible())
	{
		p.styled(f21WidgetRect(_state->_btnOk), f21ButtonStyleFor(
			CalypsoActionTone::Safe, f21ButtonVisualState(_state->_btnOk)),
			_state->_btnOk, ROLE_OK);
		p.text(_state->_btnOk, heading, _state->_btnOk->getText(), CalypsoHdTheme::kNearWhite,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_OK,
			CalypsoHdTheme::kLabelTrackingEm, actionPx);
	}

	p.text(_state->_txtTitle, heading, _state->_txtTitle->getText(), CalypsoHdTheme::kGold,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_TITLE,
		CalypsoHdTheme::kTitleTrackingEm, titlePx);
	p.text(_state->_hdDefenses, mono, _state->_hdDefenses->getText(), CalypsoHdThemeGen::kAccent,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_DEFENSES, 0.0, dataPx);
	p.text(_state->_hdAmmo, mono, _state->_hdAmmo->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_AMMO, 0.0, dataPx);
	p.text(_state->_txtInit, mono, _state->_txtInit->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_RATIO, 0.0, dataPx);
	p.text(_state->_hdPhase, mono, _state->_hdPhase->getText(), CalypsoHdThemeGen::kGold,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_PHASE, 0.0, dataPx);

	// Live list band: snapshot the last visible rows read-only and render
	// them as physical text lines inside the result rect.
	if (_state->_lstDefenses)
	{
		const auto& rows = _state->_lstDefenses->getCellTextsSnapshot();
		const size_t visible = _state->_lstDefenses->getVisibleRows();
		const size_t first = rows.size() > visible ? rows.size() - visible : 0;
		const CalypsoF21Rect band = _state->_hdResultBand;
		const int rowH = std::max(8, band.height / (int)std::max<size_t>(1, visible));
		int i = 0;
		for (size_t r = first; r < rows.size() && i < (int)visible; ++r, ++i)
		{
			std::string line;
			for (const auto* cell : rows[r])
			{
				if (!cell) continue;
				const std::string t = cell->getText();
				if (t.empty() || t == " ") continue;
				if (!line.empty()) line += "  ";
				line += t;
			}
			if (line.empty()) continue;
			// Compose one physical text item per row from the band rect.
			const CalypsoLogicalRect rowRect{ band.x, band.y + i * rowH, band.width, rowH };
			// A transient rect without a live widget: use the list widget as
			// the claim owner for blit-skip purposes (the whole list is one
			// logical widget).
			CalypsoHdItem it;
			it.kind = CalypsoHdItemKind::Text;
			it.rect = rowRect;
			it.colorRgba = CalypsoHdTheme::kNearWhite;
			const int physicalPixelHeight = std::max(1, (int)calypsoHdRoundToInt((double)rowH * 0.8 * m.scaleY));
			CalypsoHdTextRasterKey key;
			key.source = mono;
			key.physicalPixelHeight = physicalPixelHeight;
			key.text = line;
			key.colorRgba = CalypsoHdTheme::kNearWhite;
			key.direction = CalypsoTextDirection::LTR;
			it.rasterKey = key;
			it.hAlign = CalypsoHdHAlign::Left;
			it.vAlign = CalypsoHdVAlign::Middle;
			it.opacity = p.opacity;
			it.widget = _state->_lstDefenses;
			it.claim = { kF21FamilyId, ROLE_RESULT,
				reinterpret_cast<std::uintptr_t>(_state), 1u, (std::uint32_t)(p.ord + 1) };
			it.order = { 0, 0, kF21FamilyId, reinterpret_cast<std::uintptr_t>(_state),
				0, 1, p.ord + 1, ROLE_RESULT };
			builder.add(it);
			++p.ord;
		}
	}
}

void CalypsoF21DefenseUi::applyRects(BaseDefenseState& state, const CalypsoF21DefenseLayout& layout)
{
	applyRect(state._window, layout.window);
	applyRect(state._hdProtocol, layout.status);
	applyRect(state._txtTitle, layout.title);
	applyRect(state._hdDefenses, layout.defenses);
	applyRect(state._hdAmmo, layout.ammo);
	applyRect(state._txtInit, layout.hitRatio);
	applyRect(state._hdPhase, layout.phase);
	applyRect(state._lstDefenses, layout.result);
	applyRect(state._btnStart, layout.start);
	applyRect(state._btnAbort, layout.skip);
	applyRect(state._btnOk, layout.ok);
	state._hdResultBand = { layout.result.x, layout.result.y,
		layout.result.width, layout.result.height };
}

void CalypsoF21DefenseUi::configure(BaseDefenseState& state, bool allowPhysicalOverlay)
{
	state._hdLayout = state._game && state._game->getMod()
		&& state._game->getLanguage()
		&& isF34PhysicalRouteEligible(
			state._game->getMod()->isHdUiFamilyEnabled("F21"),
			state._game->getLanguage()->getTextDirection() == DIRECTION_RTL,
			!allowPhysicalOverlay);
	if (!state._hdLayout) return;

	state._hdWideLayout = currentF21LayoutClass() == CalypsoLayoutClass::Wide;
	CalypsoF21DefenseLayout layout = calypsoF21DefenseLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
	calypsoF21DefenseApplyHarnessShift(layout,
		calypsoHarnessSession().sideBySide && state._hdWideLayout);

	state._hdProtocol = new Text(1, 1, 0, 0);
	state.add(state._hdProtocol);
	state._hdProtocol->setText(state.tr("STR_CAL_F21_PROTOCOL_DEFENSE"));

	state._hdDefenses = new Text(1, 1, 0, 0);
	state.add(state._hdDefenses);
	{
		std::ostringstream ss;
		ss << "DEFENSES " << state._defenses;
		state._hdDefenses->setText(ss.str());
	}
	state._hdAmmo = new Text(1, 1, 0, 0);
	state.add(state._hdAmmo);
	{
		std::ostringstream ss;
		ss << "GRAV SHIELDS " << state._gravShields;
		state._hdAmmo->setText(ss.str());
	}
	state._hdPhase = new Text(1, 1, 0, 0);
	state.add(state._hdPhase);
	state._hdPhase->setText("PHASE —");

	// Owner-approved label: Skip Firing -> Skip to Assault (same handler;
	// the routed surviving attacker still enters the base assault).
	state._btnAbort->setText(state.tr("STR_CAL_F21_SKIP_TO_ASSAULT"));

	// The legacy UFO preview bitmap does not belong to the HD composition.
	if (state._preview) state._preview->setVisible(false);

	CalypsoF21DefenseUi::applyRects(state, layout);
	state.recaptureUiScaling(layout.designWidth, layout.designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);

	CalypsoF21DefenseUi* adapter = new CalypsoF21DefenseUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);

	if (calypsoHarnessHostUp(calypsoHarnessSession()))
	{
		hdHarnessDomShow();
	}
}

bool CalypsoF21DefenseUi::resize(BaseDefenseState& state)
{
	if (!state._hdLayout) return false;

	const bool wide = currentF21LayoutClass() == CalypsoLayoutClass::Wide;
	if (wide != state._hdWideLayout)
	{
		state._hdWideLayout = wide;
		CalypsoF21DefenseLayout layout = calypsoF21DefenseLayout(
			wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
		calypsoF21DefenseApplyHarnessShift(layout,
			calypsoHarnessSession().sideBySide && wide);
		CalypsoF21DefenseUi::applyRects(state, layout);
		state.recaptureUiScaling(layout.designWidth, layout.designHeight, 1.0f,
			/*subtractVanillaCenter=*/false);
	}
	else
	{
		state.applyUiScaling();
	}
	return true;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
