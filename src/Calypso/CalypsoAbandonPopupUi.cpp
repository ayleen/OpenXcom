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
 * HD adapter for AbandonGameState. It supplies generated-contract data and
 * live widgets to the shared small-confirmation renderer.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoAbandonPopupUi.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include <SDL.h>
#include <emscripten.h>

#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Menu/AbandonGameState.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"

#include "CalypsoF33AbandonLayout.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdTheme.h"
#include "CalypsoHdUiModel.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
#include "CalypsoUiMetrics.h"
#include "CalypsoViewportRuntime.h"

namespace OpenXcom
{
namespace Calypso
{

/// F33 comparison harness: when set, the Abandon dialog is shifted into the
/// LEFT half of the Wide design canvas so the DOM reference card fits on the
/// right (see hdHarnessAbandonActive). File-local but not anonymous-namespace
/// so the harness exports below can read/write it.
bool g_harnessAbandon = false;

namespace
{

CalypsoLayoutClass currentF33LayoutClass()
{
	// Classify from the USABLE safe area (after insets) -- the same rect
	// applyUiScaling fits the design canvas into -- not the raw framebuffer.
	CalypsoBaseSafeRect safe{ 0, 0, Options::baseXResolution, Options::baseYResolution };
	(void)calypsoProjectedSafeRectForLayout(Options::baseXResolution, Options::baseYResolution, safe);
	// F33-PARITY-005: while the harness preview is active its EXPLICIT layout
	// selection overrides ordinary automatic classification and survives
	// resize; otherwise classify from the safe area.
	return calypsoHarnessEffectiveLayout(calypsoHarnessSession(), safe);
}

CalypsoF33AbandonLayout currentF33PresentationLayout(bool wide)
{
	CalypsoF33AbandonLayout layout = calypsoF33AbandonLayout(
		wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
	calypsoF33ApplyHarnessShift(layout, g_harnessAbandon);
	return layout;
}

void applyRect(Surface* surface, const CalypsoF33Rect& rect)
{
	if (!surface) return;
	surface->setX(rect.x);
	surface->setY(rect.y);
	surface->setWidth(rect.width);
	surface->setHeight(rect.height);
}

CalypsoLogicalRect widgetRect(const Surface* surface)
{
	return { surface->getX(), surface->getY(), surface->getWidth(), surface->getHeight() };
}

} // namespace

// --- Adapter ---------------------------------------------------------------

CalypsoAbandonPopupUi::~CalypsoAbandonPopupUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoAbandonPopupUi::topState() const
{
	return _state;
}

void CalypsoAbandonPopupUi::collect(CalypsoHdFrameBuilder& builder) const
{
	if (!_state || !_state->_hdLayout || !_state->_game) return;

	const bool wide = _state->_hdWideLayout;
	const CalypsoF33AbandonLayout layout = currentF33PresentationLayout(wide);
	const CalypsoLogicalRect window = widgetRect(_state->_window);
	const double uiScale = layout.window.width > 0
		? (double)window.w / layout.window.width : 1.0;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	auto project = [&](const CalypsoF33Rect& rect) -> CalypsoLogicalRect
	{
		return {
			window.x + (int)std::llround((rect.x - layout.window.x) * uiScale),
			window.y + (int)std::llround((rect.y - layout.window.y) * uiScale),
			std::max(1, (int)std::llround(rect.width * uiScale)),
			std::max(1, (int)std::llround(rect.height * uiScale))};
	};

	CalypsoSmallConfirmationModel model;
	model.familyId = CalypsoF33AbandonGen::kFamilyId;
	model.instance = _state;
	model.mod = _state->_game->getMod();
	model.wide = wide;
	model.opaqueHarnessBackdrop = g_harnessAbandon;
	model.designWidth = layout.designWidth;
	model.designHeight = layout.designHeight;
	model.window = window;
	model.status = project(layout.status);
	model.warning = project(layout.warning);
	model.title = widgetRect(_state->_txtTitle);
	model.message = widgetRect(_state->_hdMessage);
	model.footer = project(layout.footer);
	model.windowWidget = _state->_window;
	model.protocolWidget = _state->_hdProtocol;
	model.titleWidget = _state->_txtTitle;
	model.messageWidget = _state->_hdMessage;
	model.protocolText = _state->_hdProtocol ? _state->_hdProtocol->getText() : std::string();
	model.titleText = _state->_txtTitle ? _state->_txtTitle->getText() : std::string();
	model.messageText = _state->_hdMessage ? _state->_hdMessage->getText() : std::string();
	model.cutCornerPx = CalypsoF33AbandonGen::kCutCornerPx;
	model.protocolTextInsetPx = CalypsoF33AbandonGen::kProtocolTextInsetPx;
	model.panelFillTop = CalypsoF33AbandonGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF33AbandonGen::kPanelFillBottom;
	model.frameColor = CalypsoF33AbandonGen::kFrame;
	model.protocolColor = CalypsoF33AbandonGen::kProtocolText;
	model.dividerColor = CalypsoF33AbandonGen::kDivider;
	model.footerDotColor = CalypsoF33AbandonGen::kFooterDot;
	model.warningColor = CalypsoF33AbandonGen::kWarning;
	model.uiScale = uiScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.messageDesignWidth = layout.message.width;
	model.titleDesignHeight = layout.title.height;
	model.motionDurationMs = CalypsoF33AbandonGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF33AbandonGen::kMotionScaleFrom;
	model.buttons.push_back({
		_state->_btnNo, _state->_btnYes,
		_state->_btnNo ? _state->_btnNo->getText() : std::string(),
		widgetRect(_state->_btnNo), CalypsoActionTone::Safe,
		CalypsoF33AbandonGen::kSafeFill,
		CalypsoF33AbandonGen::kSafeBorder,
		CalypsoHdTheme::kNearWhite});
	model.buttons.push_back({
		_state->_btnYes, _state->_btnNo,
		_state->_btnYes ? _state->_btnYes->getText() : std::string(),
		widgetRect(_state->_btnYes), CalypsoActionTone::Destructive,
		CalypsoF33AbandonGen::kDestructiveFill,
		CalypsoF33AbandonGen::kDestructiveFill,
		CalypsoF33AbandonGen::kDestructiveText});

	calypsoCollectSmallConfirmation(builder, model, _motion);
}

void CalypsoAbandonPopupUi::applyRects(AbandonGameState& state, const CalypsoF33AbandonLayout& layout)
{
	applyRect(state._window, layout.window);
	applyRect(state._hdProtocol, layout.status);
	applyRect(state._txtTitle, layout.title);
	applyRect(state._hdMessage, layout.message);
	applyRect(state._btnYes, layout.yes);
	applyRect(state._btnNo, layout.no);
}

void CalypsoAbandonPopupUi::configure(AbandonGameState& state, bool allowPhysicalOverlay)
{
	state._hdLayout = state._game && state._game->getMod()
		&& state._game->getLanguage()
		&& isF34PhysicalRouteEligible(
			state._game->getMod()->isHdUiFamilyEnabled("F33"),
			state._game->getLanguage()->getTextDirection() == DIRECTION_RTL,
			!allowPhysicalOverlay);
	if (!state._hdLayout) return;

	state._hdWideLayout = currentF33LayoutClass() == CalypsoLayoutClass::Wide;
	// Side-by-side translation is part of the presentation layout, not a
	// one-time constructor mutation; reconfigure can toggle it at fixed class.
	CalypsoF33AbandonLayout layout = currentF33PresentationLayout(state._hdWideLayout);

	const bool ironman = state._game->getSavedGame() && state._game->getSavedGame()->isIronman();
	const bool english = Options::language == "en-US" || Options::language == "en-GB" || Options::language == "en";
	// The accepted visual contract is English. For every other LTR locale keep
	// the semantic strings supplied by AbandonGameState; they are already
	// localized and the same handlers retain the engine's keyboard ownership.
	if (english && !ironman)
	{
		state._txtTitle->setText(CalypsoF33AbandonGen::kTitle);
		state._btnYes->setText(CalypsoF33AbandonGen::kDestructiveAction);
		state._btnNo->setText(CalypsoF33AbandonGen::kSafeAction);
	}

	state._hdProtocol = new Text(1, 1, 0, 0);
	state.add(state._hdProtocol);
	state._hdProtocol->setSmall();
	state._hdProtocol->setColor(state._txtTitle->getColor());
	state._hdProtocol->setAlign(ALIGN_LEFT);
	state._hdProtocol->setVerticalAlign(ALIGN_MIDDLE);
	state._hdProtocol->setText(CalypsoF33AbandonGen::kProtocol);

	// Semantic copy is localized input, not contract fixture text. Ironman first
	// enters SaveGameState(SAVE_IRONMAN_END), so it must not claim that progress
	// is discarded without saving. The HD-only widget is absent on logical
	// fallback, exactly like the F34 badge.
	state._hdMessage = new Text(1, 1, 0, 0);
	state.add(state._hdMessage);
	state._hdMessage->setSmall();
	state._hdMessage->setColor(state._txtTitle->getColor());
	state._hdMessage->setAlign(ALIGN_LEFT);
	state._hdMessage->setVerticalAlign(ALIGN_TOP);
	state._hdMessage->setWordWrap(true);
	if (ironman)
	{
		state._hdMessage->setText(std::string(state.tr("STR_SAVE_AND_ABANDON_GAME")));
	}
	else if (english)
	{
		state._hdMessage->setText(std::string(CalypsoF33AbandonGen::kMessageLine1)
			+ "\n" + CalypsoF33AbandonGen::kMessageLine2);
	}
	else
	{
		state._hdMessage->setText(std::string(state.tr("STR_ABANDON_GAME")));
	}

	CalypsoAbandonPopupUi::applyRects(state, layout);

	// Fit/center every design-space rect into the engine's actual logical canvas.
	state.enableUiScaling(layout.designWidth, layout.designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);

	// Create + register the snapshot-only adapter (driven at the pre-blit
	// boundary; no feeder Surface, no _surfaces reordering).
	CalypsoAbandonPopupUi* adapter = new CalypsoAbandonPopupUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);

	// Raise the DOM reference card for ANY harness preview (side / overlay /
	// reference-with-engine): the controller positions it per mode. Only the
	// LEFT-HALF SHIFT above is side-by-side specific; the show is not gated on
	// it (F33.5 overlay/registration runs surfaced this).
	if (calypsoHarnessHostUp(calypsoHarnessSession()))
	{
		hdHarnessDomShow();
	}
}

bool CalypsoAbandonPopupUi::resize(AbandonGameState& state)
{
	if (!state._hdLayout) return false;

	// Recompute the Compact/Wide layout class (remediation B5/#17): a resize
	// that crosses the threshold must re-apply the matching design rects.
	const bool wide = currentF33LayoutClass() == CalypsoLayoutClass::Wide;
	if (wide != state._hdWideLayout)
	{
		state._hdWideLayout = wide;
		const CalypsoF33AbandonLayout layout = currentF33PresentationLayout(wide);
		CalypsoAbandonPopupUi::applyRects(state, layout);
		state.recaptureUiScaling(layout.designWidth, layout.designHeight, 1.0f,
			/*subtractVanillaCenter=*/false);
	}
	else
	{
		// The layout class is unchanged, but side-by-side may have toggled.
		// Reapply all rects before scaling so overlay and side modes cannot retain
		// stale shifted geometry.
		CalypsoAbandonPopupUi::applyRects(state, currentF33PresentationLayout(wide));
		state.applyUiScaling();
	}
	return true;
}

} // namespace Calypso
} // namespace OpenXcom

// --- Harness exports -------------------------------------------------------

bool OpenXcom::Calypso::hdHarnessAbandonActive()
{
	return OpenXcom::Calypso::g_harnessAbandon;
}

void OpenXcom::Calypso::calypsoHdHarnessSetSideBySide(bool on)
{
	OpenXcom::Calypso::g_harnessAbandon = on;
}

void OpenXcom::Calypso::hdHarnessDomShow()
{
	OpenXcom::Calypso::calypsoHdHarnessDomShow();
}

void OpenXcom::Calypso::hdHarnessDomHide()
{
	OpenXcom::Calypso::calypsoHdHarnessDomHide();
}

extern "C" {

EMSCRIPTEN_KEEPALIVE
void calypso_hd_harness_abandon()
{
	// Legacy entry point (kept for the current web toolbar): route through the
	// opaque-black harness host so no lower state can ever show through
	// (F33-PARITY-002). Side-by-side (Wide) is the historic comparison mode.
	OpenXcom::Calypso::calypsoHdHarnessOpen(
		OpenXcom::Calypso::CalypsoHarnessScenario::F33Abandon,
		OpenXcom::Calypso::CalypsoLayoutClass::Wide);
}

} // extern "C"

#endif // __EMSCRIPTEN__
