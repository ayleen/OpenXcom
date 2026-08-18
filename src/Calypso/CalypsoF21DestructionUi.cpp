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
 * F21 (Calypso): HD adapter for BaseDestroyedState -- see the header. One
 * atomic subgroup. The full-destruction variant shows the warning band; the
 * partial variant lists destroyed facilities from the live TextList
 * snapshot. Base deletion itself happens in the vanilla handler.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoF21DestructionUi.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/Surface.h"
#include "../Geoscape/BaseDestroyedState.h"
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

enum DestructionRole : std::uint32_t
{
	ROLE_WINDOW = 1, ROLE_STATUS = 2, ROLE_PROTOCOL = 3, ROLE_TITLE = 4,
	ROLE_SUBTITLE = 5, ROLE_LIST = 6, ROLE_WARNING = 7, ROLE_FOOTER = 8,
	ROLE_ACK = 9, ROLE_DECORATION = 10, ROLE_GLYPH = 11
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

CalypsoF21DestructionUi::~CalypsoF21DestructionUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF21DestructionUi::topState() const
{
	return _state;
}

void CalypsoF21DestructionUi::collect(CalypsoHdFrameBuilder& builder) const
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
	const CalypsoF21DestructionLayout designLayout = calypsoF21DestructionLayout(
		wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);

	if (!_presented)
	{
		_presented = true;
		_presentedAtFrame = CalypsoHdUiOverlay::instance().frameId();
	}
	const CalypsoF21Motion motion(_presented, _presentedAtFrame,
		CalypsoF21DestructionGen::kMotionDurationMs, CalypsoF21DestructionGen::kMotionScaleFrom);

	// Generated type scale (theme roles) with the family's Engine raster
	// calibration -- never rectangle arithmetic.
	const double titlePx = (wide ? CalypsoHdThemeGen::kF21TitleWidePx : CalypsoHdThemeGen::kF21TitleCompactPx)
		* CalypsoF21DestructionGen::kEngineTextScaleTitle;
	const double bodyPx = (wide ? CalypsoHdThemeGen::kF21BodyWidePx : CalypsoHdThemeGen::kF21BodyCompactPx)
		* CalypsoF21DestructionGen::kEngineTextScaleBody;
	const double actionPx = (wide ? CalypsoHdThemeGen::kF21ActionWidePx : CalypsoHdThemeGen::kF21ActionCompactPx)
		* CalypsoF21DestructionGen::kEngineTextScaleAction;

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
	// amber caution glyph, separated footer with the sparse dot field.
	p.styled(winFull, f21WindowStyle(), _state->_window, ROLE_WINDOW);
	const CalypsoLogicalRect glyphRect = p.project(designLayout.glyph);
	{
		const CalypsoLogicalRect statusRect = p.project(designLayout.status);
		const CalypsoLogicalRect footerRect = p.project(designLayout.footer);
		p.decoration(CalypsoLogicalRect{ statusRect.x, statusRect.y + statusRect.h - 1, statusRect.w, 1 },
			kF21DividerRgba, ROLE_DECORATION);
		p.decoration(CalypsoLogicalRect{ footerRect.x, footerRect.y, footerRect.w, 1 },
			kF21DividerRgba, ROLE_FOOTER);
		for (int y = footerRect.y + 10; y < footerRect.y + footerRect.h - 8; y += 8)
		{
			for (int x = footerRect.x + 12; x < f21WidgetRect(_state->_btnOk).x - 12; x += 8)
				p.decoration(CalypsoLogicalRect{ x, y, 1, 1 }, kF21FooterDotRgba, ROLE_DECORATION);
		}
		p.textRect(f21ProtocolTextRect(_state->_hdProtocol, wide), _state->_hdProtocol,
			mono, _state->_hdProtocol ? _state->_hdProtocol->getText() : std::string(),
			kF21ProtocolTextRgba, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_PROTOCOL,
			0.10, wide ? CalypsoHdThemeGen::kF21ProtocolWidePx : CalypsoHdThemeGen::kF21ProtocolCompactPx);
	}
	// Inset material for the destroyed-facility list — structures the partial-loss evidence per §7.4
	{
		const CalypsoLogicalRect listRect = p.project(designLayout.list);
		p.styled(listRect, f21InsetPanelStyle(), nullptr, ROLE_DECORATION);
	}
	p.styled(glyphRect, f21WarningGlyphStyle(), nullptr, ROLE_GLYPH);
	p.styled(f21WidgetRect(_state->_btnOk), f21ButtonStyleFor(
		CalypsoActionTone::Safe, f21ButtonVisualState(_state->_btnOk)),
		_state->_btnOk, ROLE_ACK);

	p.text(_state->_hdTitle, heading, _state->_hdTitle->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_TITLE,
		CalypsoHdTheme::kTitleTrackingEm, titlePx);
	p.textRect(glyphRect, nullptr, heading, "!", CalypsoHdThemeGen::kGold,
		CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_GLYPH, 0.0,
		wide ? 17.0 : 15.0);
	p.text(_state->_txtMessage, body, _state->_txtMessage->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Top, 2, ROLE_SUBTITLE, 0.0, bodyPx);
	p.text(_state->_hdWarning, body, _state->_hdWarning->getText(), CalypsoHdThemeGen::kDanger,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Top, 2, ROLE_WARNING, 0.0, bodyPx);
	p.text(_state->_btnOk, heading, _state->_btnOk->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_ACK,
		CalypsoHdTheme::kLabelTrackingEm, actionPx);

	// Destroyed-facility list (partial variant): snapshot rows read-only.
	if (_state->_lstDestroyedFacilities && _state->_lstDestroyedFacilities->getVisible())
	{
		const auto& rows = _state->_lstDestroyedFacilities->getCellTextsSnapshot();
		// Project the contract band into logical screen coordinates and keep
		// the rows motion-aware; raw design coordinates would land the text
		// off the window.
		const CalypsoLogicalRect band = p.project(designLayout.list);
		const int rowH = std::max(8, band.h / 6);
		// Spec: drop the three Berthing/etc lines one full row lower inside the band
		const int listOffset = rowH;
		int i = 0;
		for (const auto& row : rows)
		{
			if (i >= 6) break;
			std::string line;
			for (const auto* cell : row)
			{
				if (!cell) continue;
				const std::string t = cell->getText();
				if (t.empty() || t == " ") continue;
				if (!line.empty()) line += "  ";
				line += t;
			}
			if (line.empty()) continue;

			const CalypsoLogicalRect rowRect = p.motionRect(
				CalypsoLogicalRect{ band.x + 8, band.y + listOffset + i * rowH, std::max(1, band.w - 16), rowH });
			CalypsoHdItem it;
			it.kind = CalypsoHdItemKind::Text;
			it.rect = rowRect;
			it.colorRgba = CalypsoHdTheme::kNearWhite;
			const int physicalPixelHeight = std::max(1, (int)calypsoHdRoundToInt(bodyPx * uiScale * m.scaleY));
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
			it.widget = _state->_lstDestroyedFacilities;
			it.claim = { kF21FamilyId, ROLE_LIST,
				reinterpret_cast<std::uintptr_t>(_state), 1u, (std::uint32_t)(p.ord + 1) };
			it.order = { 0, 0, kF21FamilyId, reinterpret_cast<std::uintptr_t>(_state),
				0, 1, p.ord + 1, ROLE_LIST };
			builder.add(it);
			++p.ord;
			++i;
		}
	}
}

void CalypsoF21DestructionUi::applyRects(BaseDestroyedState& state, const CalypsoF21DestructionLayout& layout)
{
	applyRect(state._window, layout.window);
	applyRect(state._hdProtocol, layout.status);
	applyRect(state._hdTitle, layout.title);
	applyRect(state._txtMessage, layout.subtitle);
	applyRect(state._lstDestroyedFacilities, layout.list);
	applyRect(state._hdWarning, layout.warning);
	applyRect(state._btnOk, layout.acknowledge);
	state._hdListBand = { layout.list.x, layout.list.y, layout.list.width, layout.list.height };
}

void CalypsoF21DestructionUi::configure(BaseDestroyedState& state, bool allowPhysicalOverlay)
{
	state._hdLayout = state._game && state._game->getMod()
		&& state._game->getLanguage()
		&& isF34PhysicalRouteEligible(
			state._game->getMod()->isHdUiFamilyEnabled("F21"),
			state._game->getLanguage()->getTextDirection() == DIRECTION_RTL,
			!allowPhysicalOverlay);
	if (!state._hdLayout) return;

	state._hdWideLayout = currentF21LayoutClass() == CalypsoLayoutClass::Wide;
	CalypsoF21DestructionLayout layout = calypsoF21DestructionLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
	calypsoF21DestructionApplyHarnessShift(layout,
		calypsoHarnessSession().sideBySide && state._hdWideLayout);

	state._hdProtocol = new Text(1, 1, 0, 0);
	state.add(state._hdProtocol);
	state._hdProtocol->setText(state.tr("STR_CAL_F21_PROTOCOL_DESTRUCTION"));

	state._hdTitle = new Text(1, 1, 0, 0);
	state.add(state._hdTitle);
	state._hdTitle->setText(state._partialDestruction
		? state.tr("STR_CAL_F21_BASE_DAMAGED") : state.tr("STR_CAL_F21_BASE_DESTROYED"));

	// Full destruction: explicit permanent-loss warning before the ack.
	state._hdWarning = new Text(1, 1, 0, 0);
	state.add(state._hdWarning);
	state._hdWarning->setWordWrap(true);
	if (!state._partialDestruction)
	{
		state._hdWarning->setText(state.tr("STR_CAL_F21_DESTRUCTION_PERMANENT"));
	}

	CalypsoF21DestructionUi::applyRects(state, layout);
	state.enableUiScaling(layout.designWidth, layout.designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);

	CalypsoF21DestructionUi* adapter = new CalypsoF21DestructionUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);

	if (calypsoHarnessHostUp(calypsoHarnessSession()))
	{
		hdHarnessDomShow();
	}
}

bool CalypsoF21DestructionUi::resize(BaseDestroyedState& state)
{
	if (!state._hdLayout) return false;

	const bool wide = currentF21LayoutClass() == CalypsoLayoutClass::Wide;
	if (wide != state._hdWideLayout)
	{
		state._hdWideLayout = wide;
		CalypsoF21DestructionLayout layout = calypsoF21DestructionLayout(
			wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
		calypsoF21DestructionApplyHarnessShift(layout,
			calypsoHarnessSession().sideBySide && wide);
		CalypsoF21DestructionUi::applyRects(state, layout);
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
