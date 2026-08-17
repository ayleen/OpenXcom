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
	ROLE_WINDOW = 1, ROLE_TITLE = 2, ROLE_SUBTITLE = 3, ROLE_LIST = 4,
	ROLE_WARNING = 5, ROLE_ACK = 6
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

	if (!_presented)
	{
		_presented = true;
		_presentedAtFrame = CalypsoHdUiOverlay::instance().frameId();
	}
	const CalypsoF21Motion motion(_presented, _presentedAtFrame,
		CalypsoF21DestructionGen::kMotionDurationMs, CalypsoF21DestructionGen::kMotionScaleFrom);

	builder.beginSubgroup();
	CalypsoF21Painter p{ builder, kF21FamilyId,
		reinterpret_cast<std::uintptr_t>(_state), 0, motion.opacity, motion.scale,
		CalypsoF21Rect{ _state->_window->getX(), _state->_window->getY(),
			_state->_window->getWidth(), _state->_window->getHeight() },
		m.scaleX, m.scaleY };

	const CalypsoLogicalRect canvasRect{ 0, 0,
		_state->_hdWideLayout ? 1280 : 740,
		_state->_hdWideLayout ? 720 : 360 };
	const bool harness = calypsoHarnessHostUp(calypsoHarnessSession());
	p.panel(canvasRect, harness ? calypsoRgba(0, 0, 0, 0xff) : CalypsoHdTheme::kBackdropDim,
		nullptr, ROLE_WINDOW);

	{
		const CalypsoLogicalRect w = f21WidgetRect(_state->_window);
		p.styled(CalypsoLogicalRect{ w.x - 2, w.y + 8, w.w + 4, w.h },
			CalypsoHdTheme::calypsoHdGlowStyle(CalypsoHdTheme::kShadowGlow, CalypsoHdTheme::kShadowGlowRadiusPx),
			nullptr, ROLE_WINDOW);
		p.styled(w, CalypsoHdTheme::calypsoHdGlowStyle(CalypsoHdTheme::kHaloGlow, CalypsoHdTheme::kHaloGlowRadiusPx),
			nullptr, ROLE_WINDOW);
	}

	p.styled(f21WidgetRect(_state->_window), CalypsoHdTheme::calypsoHdDialogStyle(),
		_state->_window, ROLE_WINDOW);
	p.styled(f21WidgetRect(_state->_btnOk), f21ButtonStyleFor(
		CalypsoActionTone::Destructive, f21ButtonVisualState(_state->_btnOk)),
		_state->_btnOk, ROLE_ACK);

	p.text(_state->_hdTitle, heading, _state->_hdTitle->getText(), CalypsoHdTheme::kGold,
		CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_TITLE,
		CalypsoHdTheme::kTitleTrackingEm);
	p.text(_state->_txtMessage, body, _state->_txtMessage->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Top, 2, ROLE_SUBTITLE);
	p.text(_state->_hdWarning, body, _state->_hdWarning->getText(), CalypsoHdThemeGen::kDanger,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Top, 2, ROLE_WARNING);
	p.text(_state->_btnOk, heading, _state->_btnOk->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_ACK,
		CalypsoHdTheme::kLabelTrackingEm);

	// Destroyed-facility list (partial variant): snapshot rows read-only.
	if (_state->_lstDestroyedFacilities && _state->_lstDestroyedFacilities->getVisible())
	{
		const auto& rows = _state->_lstDestroyedFacilities->getCellTextsSnapshot();
		const CalypsoF21Rect band = _state->_hdListBand;
		const int rowH = std::max(8, band.height / 6);
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

			const CalypsoLogicalRect rowRect{ band.x, band.y + i * rowH, band.width, rowH };
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
