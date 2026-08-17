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
 * F21 (Calypso): HD adapter for BaseNameState -- see the header. One atomic
 * subgroup: the whole naming dialog shows physically or not at all.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoF21NameUi.h"

#include <cstdint>
#include <string>

#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/Surface.h"
#include "../Geoscape/BaseNameState.h"
#include "../Interface/Text.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextButton.h"
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

enum NameRole : std::uint32_t
{
	ROLE_WINDOW = 1, ROLE_TITLE = 2, ROLE_NAME = 3, ROLE_HINT = 4, ROLE_OK = 5
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

CalypsoF21NameUi::~CalypsoF21NameUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF21NameUi::topState() const
{
	return _state;
}

void CalypsoF21NameUi::collect(CalypsoHdFrameBuilder& builder) const
{
	if (!_state || !_state->_hdLayout || !_state->_game) return;

	Mod* mod = _state->_game->getMod();
	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor body;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_SAIRA_700", heading)) return;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F33_BODY", body)) return;

	const CalypsoHdPresentationMetrics& m = CalypsoHdUiOverlay::instance().frozenMetrics();

	if (!_presented)
	{
		_presented = true;
		_presentedAtFrame = CalypsoHdUiOverlay::instance().frameId();
	}
	const CalypsoF21Motion motion(_presented, _presentedAtFrame,
		CalypsoF21NameGen::kMotionDurationMs, CalypsoF21NameGen::kMotionScaleFrom);

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
	if (_state->_btnOk && _state->_btnOk->getVisible())
	{
		p.styled(f21WidgetRect(_state->_btnOk), f21ButtonStyleFor(
			CalypsoActionTone::Safe, f21ButtonVisualState(_state->_btnOk)),
			_state->_btnOk, ROLE_OK);
		p.text(_state->_btnOk, heading, _state->_btnOk->getText(), CalypsoHdTheme::kNearWhite,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_OK,
			CalypsoHdTheme::kLabelTrackingEm);
	}

	p.text(_state->_txtTitle, heading, _state->_txtTitle->getText(), CalypsoHdTheme::kGold,
		CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_TITLE,
		CalypsoHdTheme::kTitleTrackingEm);
	p.text(_state->_edtName, body, _state->_edtName->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_NAME);
	p.text(_state->_hdHint, body, _state->_hdHint->getText(), CalypsoHdThemeGen::kAccentSoft,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_HINT);
}

void CalypsoF21NameUi::applyRects(BaseNameState& state, const CalypsoF21NameLayout& layout)
{
	applyRect(state._window, layout.window);
	applyRect(state._txtTitle, layout.title);
	applyRect(state._edtName, layout.nameEdit);
	applyRect(state._hdHint, layout.hint);
	applyRect(state._btnOk, layout.ok);
}

void CalypsoF21NameUi::configure(BaseNameState& state, bool allowPhysicalOverlay)
{
	state._hdLayout = state._game && state._game->getMod()
		&& state._game->getLanguage()
		&& isF34PhysicalRouteEligible(
			state._game->getMod()->isHdUiFamilyEnabled("F21"),
			state._game->getLanguage()->getTextDirection() == DIRECTION_RTL,
			!allowPhysicalOverlay);
	if (!state._hdLayout) return;

	state._hdWideLayout = currentF21LayoutClass() == CalypsoLayoutClass::Wide;
	CalypsoF21NameLayout layout = calypsoF21NameLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
	calypsoF21NameApplyHarnessShift(layout,
		calypsoHarnessSession().sideBySide && state._hdWideLayout);

	state._hdHint = new Text(1, 1, 0, 0);
	state.add(state._hdHint);
	state._hdHint->setText(state.tr("STR_CAL_F21_NAME_HINT"));

	CalypsoF21NameUi::applyRects(state, layout);
	state.enableUiScaling(layout.designWidth, layout.designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);

	CalypsoF21NameUi* adapter = new CalypsoF21NameUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);

	if (calypsoHarnessHostUp(calypsoHarnessSession()))
	{
		hdHarnessDomShow();
	}
}

bool CalypsoF21NameUi::resize(BaseNameState& state)
{
	if (!state._hdLayout) return false;

	const bool wide = currentF21LayoutClass() == CalypsoLayoutClass::Wide;
	if (wide != state._hdWideLayout)
	{
		state._hdWideLayout = wide;
		CalypsoF21NameLayout layout = calypsoF21NameLayout(
			wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
		calypsoF21NameApplyHarnessShift(layout,
			calypsoHarnessSession().sideBySide && wide);
		CalypsoF21NameUi::applyRects(state, layout);
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
