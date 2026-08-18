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
 * F21 (Calypso): HD adapter for ConfirmNewBaseState -- see the header. The
 * transaction window is ONE atomic subgroup and carries the F33 command-card
 * language: opposing-cut translucent frame, cut-shaped glow pair, mono
 * protocol strip with its divider, amber caution glyph, and a separated
 * footer band with the sparse dot field.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoF21TransactionUi.h"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

#include <emscripten.h>

#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Geoscape/ConfirmNewBaseState.h"
#include "../Engine/Surface.h"
#include "../Interface/Text.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleRegion.h"
#include "../Savegame/Base.h"
#include "../Savegame/Region.h"
#include "../Savegame/SavedGame.h"

#include "CalypsoAbandonPopupUi.h"
#include "CalypsoF21UiShared.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoUiFamilies.h"
#include "CalypsoUiMetrics.h"
#include "CalypsoViewportRuntime.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

// F21 transaction claim roles (stableId) within the one atomic subgroup.
enum TransactionRole : std::uint32_t
{
	ROLE_WINDOW = 1, ROLE_STATUS = 2, ROLE_PROTOCOL = 3, ROLE_TITLE = 4,
	ROLE_SLOT = 5, ROLE_COORDS = 6, ROLE_REGION = 7, ROLE_COST = 8,
	ROLE_AFTER = 9, ROLE_NAME = 10, ROLE_HINT = 11, ROLE_FOOTER = 12,
	ROLE_CREATE = 13, ROLE_CANCEL = 14, ROLE_DECORATION = 15, ROLE_WARNING = 16,
	ROLE_FACTS = 17
};

void applyRect(Surface* surface, const CalypsoF21Rect& rect)
{
	if (!surface) return;
	surface->setX(rect.x);
	surface->setY(rect.y);
	surface->setWidth(rect.width);
	surface->setHeight(rect.height);
}

/// Candidate coordinates in degrees, one decimal, hemisphere letters.
std::string coordsText(const Base* base)
{
	const double lat = base->getLatitude() * 180.0 / M_PI;
	const double lon = base->getLongitude() * 180.0 / M_PI;
	std::ostringstream ss;
	ss << std::fixed << std::setprecision(1)
	   << std::fabs(lat) << "·" << (lat >= 0 ? "N" : "S")
	   << "  " << std::fabs(lon) << "·" << (lon >= 0 ? "E" : "W");
	return ss.str();
}

} // namespace

CalypsoF21TransactionUi::~CalypsoF21TransactionUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF21TransactionUi::topState() const
{
	return _state;
}

void CalypsoF21TransactionUi::collect(CalypsoHdFrameBuilder& builder) const
{
	if (!_state || !_state->_hdLayout || !_state->_game) return;

	// Resolve every required font BEFORE any claim: a missing font keeps the
	// complete logical window for this frame.
	Mod* mod = _state->_game->getMod();
	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor body;
	CalypsoTtfSourceDescriptor mono;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_SAIRA_700", heading)) return;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F33_BODY", body)) return;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_MONO", mono)) return;

	const CalypsoHdPresentationMetrics& m = CalypsoHdUiOverlay::instance().frozenMetrics();
	const bool wide = _state->_hdWideLayout;
	const double titlePx = (wide ? CalypsoHdThemeGen::kF21TitleWidePx : CalypsoHdThemeGen::kF21TitleCompactPx)
		* CalypsoF21TransactionGen::kEngineTextScaleTitle;
	const double dataPx = (wide ? CalypsoHdThemeGen::kF21DataWidePx : CalypsoHdThemeGen::kF21DataCompactPx)
		* CalypsoF21TransactionGen::kEngineTextScaleData;
	const double bodyPx = (wide ? CalypsoHdThemeGen::kF21BodyWidePx : CalypsoHdThemeGen::kF21BodyCompactPx)
		* CalypsoF21TransactionGen::kEngineTextScaleBody;
	const double inputPx = (wide ? CalypsoHdThemeGen::kF21InputWidePx : CalypsoHdThemeGen::kF21InputCompactPx)
		* CalypsoF21TransactionGen::kEngineTextScaleInput;
	const double actionPx = (wide ? CalypsoHdThemeGen::kF21ActionWidePx : CalypsoHdThemeGen::kF21ActionCompactPx)
		* CalypsoF21TransactionGen::kEngineTextScaleAction;
	const CalypsoF21TransactionLayout designLayout = calypsoF21TransactionLayout(
		wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);

	if (!_presented)
	{
		_presented = true;
		_presentedAtFrame = CalypsoHdUiOverlay::instance().frameId();
	}
	const CalypsoF21Motion motion(_presented, _presentedAtFrame,
		CalypsoF21TransactionGen::kMotionDurationMs, CalypsoF21TransactionGen::kMotionScaleFrom);

	// One atomic subgroup: the whole transaction window shows or not at all.
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

	// Modal dim backdrop over the live globe (fully opaque in harness mode).
	const CalypsoLogicalRect canvasRect{ 0, 0, wide ? 1280 : 740, wide ? 720 : 360 };
	const bool harness = calypsoHarnessHostUp(calypsoHarnessSession());
	p.panel(canvasRect, harness ? calypsoRgba(0, 0, 0, 0xff) : CalypsoHdTheme::kBackdropDim,
		nullptr, ROLE_WINDOW);

	// Cut-shaped glow pair under the command frame.
	p.styled(CalypsoLogicalRect{ winFull.x - 2, winFull.y + 8, winFull.w + 4, winFull.h },
		f21GlowStyle(CalypsoHdTheme::kShadowGlow, CalypsoHdTheme::kShadowGlowRadiusPx),
		nullptr, ROLE_WINDOW);
	p.styled(winFull, f21GlowStyle(CalypsoHdTheme::kHaloGlow, CalypsoHdTheme::kHaloGlowRadiusPx),
		nullptr, ROLE_WINDOW);

	// Command frame + actions. Create is the primary (safe/commit) action with
	// full interaction tokens; Cancel is the quiet action (thin translucent
	// shell, no tone fill).
	p.styled(winFull, f21WindowStyle(), _state->_window, ROLE_WINDOW);
	p.styled(f21WidgetRect(_state->_btnOk), f21ButtonStyleFor(
		CalypsoActionTone::Safe, f21ButtonVisualState(_state->_btnOk)),
		_state->_btnOk, ROLE_CREATE);
	{
		p.styled(f21WidgetRect(_state->_btnCancel), f21QuietButtonStyle(
			f21ButtonVisualState(_state->_btnCancel)), _state->_btnCancel, ROLE_CANCEL);
	}

	// Structural rules: status divider, footer divider, footer dot field.
	const CalypsoLogicalRect statusRect = p.project(designLayout.status);
	const CalypsoLogicalRect footerRect = p.project(designLayout.footer);
	const CalypsoLogicalRect glyphRect = p.project(designLayout.glyph);
	const CalypsoLogicalRect factsRect = p.project(designLayout.factsPanel);
	const CalypsoLogicalRect inputRect = p.project(designLayout.inputFrame);
	p.styled(factsRect, f21InsetPanelStyle(), nullptr, ROLE_FACTS);
	p.styled(inputRect, f21InputStyle(_state->_edtName->isFocused()),
		_state->_edtName, ROLE_NAME);
	p.decoration(p.project(designLayout.factsColumnDivider), kF21DividerRgba, ROLE_DECORATION);
	p.decoration(p.project(designLayout.factsRowDivider), kF21DividerRgba, ROLE_DECORATION);
	p.decoration(CalypsoLogicalRect{ statusRect.x, statusRect.y + statusRect.h - 1, statusRect.w, 1 },
		kF21DividerRgba, ROLE_DECORATION);
	p.decoration(CalypsoLogicalRect{ footerRect.x, footerRect.y, footerRect.w, 1 },
		kF21DividerRgba, ROLE_FOOTER);
	const CalypsoLogicalRect footerDotsRect = p.project(designLayout.footerDots);
	for (int y = footerDotsRect.y; y < footerDotsRect.y + footerDotsRect.h; y += 8)
	{
		for (int x = footerDotsRect.x; x < footerDotsRect.x + footerDotsRect.w; x += 8)
			p.decoration(CalypsoLogicalRect{ x, y, 1, 1 }, kF21FooterDotRgba, ROLE_DECORATION);
	}

	// Amber caution glyph: this transaction commits funds (stroke + "!").
	p.styled(glyphRect, f21WarningGlyphStyle(), nullptr, ROLE_WARNING);

	// Protocol strip (mono, tracked) + heading + facts + staged name.
	p.textRect(f21ProtocolTextRect(_state->_hdProtocol, wide), _state->_hdProtocol, mono,
		_state->_hdProtocol ? _state->_hdProtocol->getText() : std::string(),
		kF21ProtocolTextRgba, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_PROTOCOL,
		0.10, wide ? CalypsoHdThemeGen::kF21ProtocolWidePx : CalypsoHdThemeGen::kF21ProtocolCompactPx);
	p.text(_state->_hdTitle, heading, _state->_hdTitle->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_TITLE,
		CalypsoHdTheme::kTitleTrackingEm,
		titlePx);
	p.textRect(glyphRect, nullptr, heading, "!", CalypsoHdThemeGen::kGold,
		CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_WARNING, 0.0,
		wide ? 17.0 : 15.0);
	p.text(_state->_hdSlot, mono, _state->_hdSlot->getText(), CalypsoHdThemeGen::kAccent,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_SLOT, 0.0,
		dataPx);
	p.text(_state->_hdCoords, mono, _state->_hdCoords->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_COORDS, 0.0,
		dataPx);
	p.text(_state->_txtArea, mono, _state->_txtArea->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_REGION, 0.0,
		dataPx);
	p.text(_state->_txtCost, mono, _state->_txtCost->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_COST, 0.0,
		dataPx);
	if (_state->_hdAfter)
	{
		const SavedGame* save = _state->_game->getSavedGame();
		const int after = save->getFunds() - _state->_cost;
		const std::uint32_t color = after >= 0 ? CalypsoHdThemeGen::kAccent : CalypsoHdThemeGen::kDanger;
		p.text(_state->_hdAfter, mono, _state->_hdAfter->getText(), color,
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_AFTER, 0.0,
			dataPx);
	}
	const std::string nameDisplay = _state->_edtName->getText() + " |";
	p.textRect(CalypsoLogicalRect{ inputRect.x + (wide ? 16 : 12), inputRect.y,
		inputRect.w - (wide ? 32 : 24), inputRect.h }, _state->_edtName,
		body, nameDisplay, CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_NAME, 0.0,
		inputPx);
	p.text(_state->_hdNameHint, body, _state->_hdNameHint->getText(), kF21MutedBodyRgba,
		CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_HINT, 0.0,
		bodyPx);
	p.text(_state->_btnOk, heading, _state->_btnOk->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_CREATE,
		CalypsoHdTheme::kLabelTrackingEm,
		actionPx);
	p.text(_state->_btnCancel, heading, _state->_btnCancel->getText(), CalypsoHdTheme::kNearWhite,
		CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_CANCEL,
		CalypsoHdTheme::kLabelTrackingEm,
		actionPx);
}

void CalypsoF21TransactionUi::applyRects(ConfirmNewBaseState& state, const CalypsoF21TransactionLayout& layout)
{
	applyRect(state._window, layout.window);
	applyRect(state._hdProtocol, layout.status);
	applyRect(state._hdTitle, layout.title);
	applyRect(state._hdSlot, layout.slot);
	applyRect(state._hdCoords, layout.coords);
	applyRect(state._txtArea, layout.region);
	applyRect(state._txtCost, layout.cost);
	applyRect(state._hdAfter, layout.after);
	applyRect(state._edtName, layout.nameEdit);
	applyRect(state._hdNameHint, layout.nameHint);
	applyRect(state._btnOk, layout.create);
	applyRect(state._btnCancel, layout.cancel);
}

void CalypsoF21TransactionUi::configure(ConfirmNewBaseState& state, bool allowPhysicalOverlay)
{
	state._hdLayout = state._game && state._game->getMod()
		&& state._game->getLanguage()
		&& isF34PhysicalRouteEligible(
			state._game->getMod()->isHdUiFamilyEnabled("F21"),
			state._game->getLanguage()->getTextDirection() == DIRECTION_RTL,
			!allowPhysicalOverlay);
	if (!state._hdLayout) return;

	state._hdWideLayout = currentF21LayoutClass() == CalypsoLayoutClass::Wide;
	CalypsoF21TransactionLayout layout = calypsoF21TransactionLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
	calypsoF21TransactionApplyHarnessShift(layout,
		calypsoHarnessSession().sideBySide && state._hdWideLayout);

	// HD-only widgets (absent on the logical fallback, like the F33 message
	// band): protocol strip copy, heading, slot/coords/after readouts, hint.
	state._hdProtocol = new Text(1, 1, 0, 0);
	state.add(state._hdProtocol);
	state._hdProtocol->setText(state.tr("STR_CAL_F21_PROTOCOL_TRANSACTION"));

	state._hdTitle = new Text(1, 1, 0, 0);
	state.add(state._hdTitle);
	state._hdTitle->setText(state.tr("STR_CAL_F21_REVIEW_BASE"));

	state._hdSlot = new Text(1, 1, 0, 0);
	state.add(state._hdSlot);
	state._hdSlot->setText(state.tr("STR_CAL_F21_BASE_SLOT")
		.arg((int)state._game->getSavedGame()->getBases()->size() + 1).arg(8));

	state._hdCoords = new Text(1, 1, 0, 0);
	state.add(state._hdCoords);
	state._hdCoords->setText(state.tr("STR_CAL_F21_COORDINATES").arg(coordsText(state._base)));

	// Harness fixtures and edge-case rulesets may not resolve a region. Replace
	// the vanilla formatting label before lookup so the physical snapshot never
	// exposes raw OXCE control syntax.
	state._txtArea->setText(state.tr("STR_CAL_F21_REGION_PENDING"));
	for (const auto* region : *state._game->getSavedGame()->getRegions())
	{
		if (region->getRules()->insideRegion(state._base->getLongitude(), state._base->getLatitude()))
		{
			state._txtArea->setText(state.tr("STR_CAL_F21_REGION").arg(
				state.tr(region->getRules()->getType())));
			break;
		}
	}
	state._txtCost->setText(state.tr("STR_CAL_F21_BUILD_COST").arg(
		Unicode::formatFunding(state._cost)));

	state._hdAfter = new Text(1, 1, 0, 0);
	state.add(state._hdAfter);
	state._hdAfter->setText(state.tr("STR_CAL_F21_FUNDS_AFTER").arg(
		Unicode::formatFunding(state._game->getSavedGame()->getFunds() - state._cost)));

	state._hdNameHint = new Text(1, 1, 0, 0);
	state.add(state._hdNameHint);
	state._hdNameHint->setText(state.tr("STR_CAL_F21_NAME_STAGED"));

	// Descriptive HD action label (same handler, localized through the pack).
	state._btnOk->setText(state.tr("STR_CAL_F21_CREATE_BASE"));
	if (calypsoHarnessHostUp(calypsoHarnessSession()))
	{
		state._edtName->setText("CALYPSO DEEP");
		state._btnOk->setVisible(true);
	}

	CalypsoF21TransactionUi::applyRects(state, layout);
	state.enableUiScaling(layout.designWidth, layout.designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);

	CalypsoF21TransactionUi* adapter = new CalypsoF21TransactionUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);

	if (calypsoHarnessHostUp(calypsoHarnessSession()))
	{
		hdHarnessDomShow();
	}
}

bool CalypsoF21TransactionUi::resize(ConfirmNewBaseState& state)
{
	if (!state._hdLayout) return false;

	const bool wide = currentF21LayoutClass() == CalypsoLayoutClass::Wide;
	if (wide != state._hdWideLayout)
	{
		state._hdWideLayout = wide;
		CalypsoF21TransactionLayout layout = calypsoF21TransactionLayout(
			wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
		calypsoF21TransactionApplyHarnessShift(layout,
			calypsoHarnessSession().sideBySide && wide);
		CalypsoF21TransactionUi::applyRects(state, layout);
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
