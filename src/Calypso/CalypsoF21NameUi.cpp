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
#include "../Geoscape/BuildNewBaseState.h"
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
	ROLE_WINDOW = 1, ROLE_STATUS = 2, ROLE_PROTOCOL = 3, ROLE_TITLE = 4,
	ROLE_NAME = 5, ROLE_HINT = 6, ROLE_FOOTER = 7, ROLE_CANCEL = 8,
	ROLE_OK = 9, ROLE_DECORATION = 10, ROLE_COVERED_LEGACY = 11
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

void CalypsoF21NameUi::collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const
{
	if (!_state || !_state->_coveredSite) return;
	suppression.add(_state->_coveredSite->_window);
	suppression.add(_state->_coveredSite->_txtTitle);
	suppression.add(_state->_coveredSite->_btnCancel);
	suppression.add(_state->_coveredSite->_hdProtocol);
	suppression.add(_state->_coveredSite->_hdSlot);
	suppression.add(_state->_coveredSite->_hdFunds);
	suppression.add(_state->_coveredSite->_hdCost);
	suppression.add(_state->_coveredSite->_hdCard);
	suppression.add(_state->_coveredSite->_hdCoords);
	suppression.add(_state->_coveredSite->_hdRegion);
	suppression.add(_state->_coveredSite->_hdLegality);
	suppression.add(_state->_coveredSite->_hdPreview);
}

void CalypsoF21NameUi::collect(CalypsoHdFrameBuilder& builder) const
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
	const CalypsoF21NameLayout designLayout = calypsoF21NameLayout(
		wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);

	if (!_presented)
	{
		_presented = true;
		_presentedAtFrame = CalypsoHdUiOverlay::instance().frameId();
	}
	const CalypsoF21Motion motion(_presented, _presentedAtFrame,
		CalypsoF21NameGen::kMotionDurationMs, CalypsoF21NameGen::kMotionScaleFrom);

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

	if (_state->_coveredSite)
	{
		// BaseNameState sits above BuildNewBaseState. Claim only the covered
		// site's chrome so the globe remains visible while the old window,
		// labels, and F21-only readouts cannot leak through the new form.
		p.claim(_state->_coveredSite->_window, ROLE_COVERED_LEGACY);
		p.claim(_state->_coveredSite->_txtTitle, ROLE_COVERED_LEGACY);
		p.claim(_state->_coveredSite->_btnCancel, ROLE_COVERED_LEGACY);
		p.claim(_state->_coveredSite->_hdProtocol, ROLE_COVERED_LEGACY);
		p.claim(_state->_coveredSite->_hdSlot, ROLE_COVERED_LEGACY);
		p.claim(_state->_coveredSite->_hdFunds, ROLE_COVERED_LEGACY);
		p.claim(_state->_coveredSite->_hdCost, ROLE_COVERED_LEGACY);
		p.claim(_state->_coveredSite->_hdCard, ROLE_COVERED_LEGACY);
		p.claim(_state->_coveredSite->_hdCoords, ROLE_COVERED_LEGACY);
		p.claim(_state->_coveredSite->_hdRegion, ROLE_COVERED_LEGACY);
		p.claim(_state->_coveredSite->_hdLegality, ROLE_COVERED_LEGACY);
		p.claim(_state->_coveredSite->_hdPreview, ROLE_COVERED_LEGACY);
	}

	const CalypsoLogicalRect canvasRect{ 0, 0, designLayout.designWidth, designLayout.designHeight };
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
		const CalypsoLogicalRect inputRect = p.project(designLayout.inputFrame);
		p.styled(inputRect, f21InputStyle(_state->_edtName->isFocused()),
			_state->_edtName, ROLE_NAME);
		p.decoration(CalypsoLogicalRect{ statusRect.x, statusRect.y + statusRect.h - 1, statusRect.w, 1 },
			kF21DividerRgba, ROLE_DECORATION);
		p.decoration(CalypsoLogicalRect{ footerRect.x, footerRect.y, footerRect.w, 1 },
			kF21DividerRgba, ROLE_FOOTER);
		const int firstActionX = _state->_btnCancel && _state->_btnCancel->getVisible()
			? f21WidgetRect(_state->_btnCancel).x : f21WidgetRect(_state->_btnOk).x;
		for (int y = footerRect.y + 10; y < footerRect.y + footerRect.h - 8; y += 8)
		{
			for (int x = footerRect.x + 12; x < firstActionX - 12; x += 8)
				p.decoration(CalypsoLogicalRect{ x, y, 1, 1 }, kF21FooterDotRgba, ROLE_DECORATION);
		}
		p.textRect(f21ProtocolTextRect(_state->_hdProtocol, wide), _state->_hdProtocol, mono,
			_state->_hdProtocol ? _state->_hdProtocol->getText() : std::string(),
			kF21ProtocolTextRgba, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_PROTOCOL,
			0.10, wide ? CalypsoHdThemeGen::kF21ProtocolWidePx : CalypsoHdThemeGen::kF21ProtocolCompactPx);
	}
	if (_state->_btnCancel && _state->_btnCancel->getVisible())
	{
		p.styled(f21WidgetRect(_state->_btnCancel), f21QuietButtonStyle(
			f21ButtonVisualState(_state->_btnCancel)), _state->_btnCancel, ROLE_CANCEL);
		p.text(_state->_btnCancel, heading, _state->_btnCancel->getText(), CalypsoHdTheme::kNearWhite,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_CANCEL,
			CalypsoHdTheme::kLabelTrackingEm,
			wide ? CalypsoHdThemeGen::kF21ActionWidePx : CalypsoHdThemeGen::kF21ActionCompactPx);
	}
	if (_state->_btnOk && _state->_btnOk->getVisible())
	{
		p.styled(f21WidgetRect(_state->_btnOk), f21ButtonStyleFor(
			CalypsoActionTone::Safe, f21ButtonVisualState(_state->_btnOk)),
			_state->_btnOk, ROLE_OK);
		p.text(_state->_btnOk, heading, _state->_btnOk->getText(), CalypsoHdTheme::kNearWhite,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_OK,
			CalypsoHdTheme::kLabelTrackingEm,
			wide ? CalypsoHdThemeGen::kF21ActionWidePx : CalypsoHdThemeGen::kF21ActionCompactPx);
	}

	p.text(_state->_txtTitle, heading, _state->_txtTitle->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_TITLE,
		CalypsoHdTheme::kTitleTrackingEm,
		wide ? CalypsoHdThemeGen::kF21TitleWidePx : CalypsoHdThemeGen::kF21TitleCompactPx);
	const CalypsoLogicalRect inputRect = p.project(designLayout.inputFrame);
	const std::string nameDisplay = _state->_edtName->getText() + " |";
	p.textRect(CalypsoLogicalRect{ inputRect.x + (wide ? 16 : 12), inputRect.y,
		inputRect.w - (wide ? 32 : 24), inputRect.h }, _state->_edtName,
		body, nameDisplay, CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_NAME, 0.0,
		wide ? CalypsoHdThemeGen::kF21InputWidePx : CalypsoHdThemeGen::kF21InputCompactPx);
	p.text(_state->_hdHint, body, _state->_hdHint->getText(), kF21MutedBodyRgba,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_HINT, 0.0,
		wide ? CalypsoHdThemeGen::kF21BodyWidePx : CalypsoHdThemeGen::kF21BodyCompactPx);
}

void CalypsoF21NameUi::applyRects(BaseNameState& state, const CalypsoF21NameLayout& layout)
{
	applyRect(state._window, layout.window);
	applyRect(state._hdProtocol, layout.status);
	applyRect(state._txtTitle, layout.title);
	applyRect(state._edtName, layout.inputFrame);
	applyRect(state._hdHint, layout.inputHint);
	applyRect(state._btnCancel, layout.cancel);
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

	state._hdProtocol = new Text(1, 1, 0, 0);
	state.add(state._hdProtocol);
	state._hdProtocol->setText(state.tr("STR_CAL_F21_PROTOCOL_NAME"));
	state._txtTitle->setText(state.tr("STR_CAL_F21_NAME_TITLE"));
	state._btnCancel->setText(state.tr("STR_CANCEL_UC"));
	state._btnOk->setText(state.tr("STR_OK"));

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
