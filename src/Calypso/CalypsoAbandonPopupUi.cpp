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
 * F33 (Calypso): HD adapter for AbandonGameState -- see CalypsoAbandonPopupUi.h.
 * Structural clone of CalypsoErrorPopupUi (Phase 46.2-HD remediation B-Error):
 * same theme constants, same bevel/text submission helpers, different widget
 * set (confirm dialog: title + data-loss message + destructive YES / safe NO).
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoAbandonPopupUi.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include <SDL.h>
#include <SDL_ttf.h>
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

#include "CalypsoF33AbandonLayout.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdInteractionState.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdTextRasterKey.h"
#include "CalypsoHdTheme.h"
#include "CalypsoHdUiModel.h"
#include "CalypsoHdUiOverlay.h"
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

// F33 claim roles (stableId) within the one confirm-dialog subgroup.
enum F33Role : std::uint32_t
{
	ROLE_WINDOW = 1, ROLE_TITLE = 2, ROLE_MESSAGE = 3,
	ROLE_YES = 4, ROLE_NO = 5
};
constexpr std::uint32_t kF33FamilyId = 33;

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

/// Read-only interaction snapshot of a button (F33-PARITY-008): the adapter
/// NEVER owns events; it just reads the widget's live state.
CalypsoInteractionState buttonVisualState(const TextButton* btn)
{
	if (!btn) return CalypsoInteractionState::Rest;
	if (btn->isPressed()) return CalypsoInteractionState::Pressed;
	if (btn->isFocused()) return CalypsoInteractionState::Focus;
	if (btn->isHovered()) return CalypsoInteractionState::Hover;
	return CalypsoInteractionState::Rest;
}

/// SDF panel style for one (tone, state) presentation, from the canonical
/// semantic tokens. Keyboard focus adds a non-colour cue: a thicker ring.
CalypsoHdPanelStyle buttonStyleFor(CalypsoActionTone tone, CalypsoInteractionState state)
{
	const CalypsoInteractionTokenPair tokens = calypsoInteractionTokenPair(tone, state);
	const std::uint32_t borderColor = (state == CalypsoInteractionState::Focus)
		? CalypsoHdThemeGen::calypsoHdThemeColorForToken(calypsoFocusRingToken(tone))
		: CalypsoHdThemeGen::calypsoHdThemeColorForToken(tokens.borderToken);
	const std::uint32_t fill = CalypsoHdThemeGen::calypsoHdThemeColorForToken(tokens.fillToken);

	CalypsoHdPanelStyle s;
	s.styled = true;
	s.radiusPx = CalypsoHdTheme::kButtonRadiusPx;
	s.borderWidthPx = CalypsoHdTheme::kBorderWidthPx
		+ (state == CalypsoInteractionState::Focus ? 1.0f : 0.0f);
	s.borderColorRgba = borderColor;
	s.fillTopRgba = fill;
	s.fillBottomRgba = fill;
	s.gradDirX = 0.26f;
	s.gradDirY = 1.0f;
	return s;
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

	// F33-PARITY-007: claim the COMPLETE physical dialog from the FIRST
	// eligible frame -- never expose the legacy POPUP_BOTH animation. The
	// logical window is claimed (blit-skipped) in the same atomic subgroup,
	// so it cannot flash underneath.
	Mod* mod = _state->_game->getMod();
	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor body;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_SAIRA_700", heading)) return;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F33_BODY", body)) return;

	const CalypsoHdPresentationMetrics& m = CalypsoHdUiOverlay::instance().frozenMetrics();
	const double sx = m.scaleX, sy = m.scaleY;
	const std::uint64_t inst = reinterpret_cast<std::uintptr_t>(_state);

	// One atomic subgroup: the whole dialog shows physically or not at all.
	builder.beginSubgroup();
	int ord = 0;

	auto addPanel = [&](const CalypsoLogicalRect& r, std::uint32_t color,
		const void* widget, std::uint32_t role)
	{
		if (r.w <= 0 || r.h <= 0) return;
		CalypsoHdItem it;
		it.kind = CalypsoHdItemKind::Panel;
		it.rect = r;
		it.colorRgba = color;
		it.widget = widget;
		it.claim = { kF33FamilyId, role, inst, 1u, (std::uint32_t)ord };
		it.order = { 0, 0, kF33FamilyId, inst, 0, 1u, ord, role };
		builder.add(it);
		++ord;
	};

	// Dim backdrop: full design canvas at 45% black, matching the DOM popup
	// overlays (tutorial/prologue/pause use rgba(0,0,0,.45)). In comparison
	// harness mode the canvas goes FULLY opaque black instead — the native
	// MainMenuState rendering behind it is noise for the side-by-side check.
	const CalypsoLogicalRect canvasRect{ 0, 0,
		_state->_hdWideLayout ? 1280 : 740,
		_state->_hdWideLayout ? 720 : 360 };
	const std::uint32_t backdrop = g_harnessAbandon
		? calypsoRgba(0x00, 0x00, 0x00, 0xff)
		: CalypsoHdTheme::kBackdropDim;
	addPanel(canvasRect, backdrop, nullptr, ROLE_WINDOW);

	auto addStyled = [&](const CalypsoLogicalRect& r, const CalypsoHdPanelStyle& style,
		const void* widget, std::uint32_t role)
	{
		if (r.w <= 0 || r.h <= 0) return;
		CalypsoHdItem it;
		it.kind = CalypsoHdItemKind::Panel;
		it.rect = r;
		it.colorRgba = style.fillTopRgba;
		it.panelStyle = style;
		it.widget = widget;
		it.claim = { kF33FamilyId, role, inst, 1u, (std::uint32_t)ord };
		it.order = { 0, 0, kF33FamilyId, inst, 0, 1u, ord, role };
		builder.add(it);
		++ord;
	};

	// Soft shadow + accent halo under the dialog (styled glow quads replace
	// the old stepped bands): shadow sits below the window, halo is centred.
	{
		const CalypsoLogicalRect w = widgetRect(_state->_window);
		const CalypsoLogicalRect shadow{ w.x - 2, w.y + 8, w.w + 4, w.h };
		addStyled(shadow, CalypsoHdTheme::calypsoHdGlowStyle(
			CalypsoHdTheme::kShadowGlow, CalypsoHdTheme::kShadowGlowRadiusPx), nullptr, ROLE_WINDOW);
		addStyled(w, CalypsoHdTheme::calypsoHdGlowStyle(
			CalypsoHdTheme::kHaloGlow, CalypsoHdTheme::kHaloGlowRadiusPx), nullptr, ROLE_WINDOW);
	}

	auto addText = [&](Surface* widget, const CalypsoTtfSourceDescriptor& font,
		const std::string& text, std::uint32_t color,
		CalypsoHdHAlign hA, CalypsoHdVAlign vA, int linesHint, std::uint32_t role,
		double trackingEm = 0.0)
	{
		if (!widget || text.empty()) return;
		const CalypsoLogicalRect r = widgetRect(widget);
		if (r.w <= 0 || r.h <= 0) return;

		const int hint = linesHint > 0 ? linesHint : 1;
		const int physicalPixelHeight = std::max(1, (int)calypsoHdRoundToInt((double)r.h / hint * sy));
		const int wrapWidth = (hint > 1)
			? std::max(1, (int)calypsoHdRoundToInt((double)r.w * sx)) : 0;

		CalypsoHdTextRasterKey key;
		key.source = font;
		key.physicalPixelHeight = physicalPixelHeight;
		key.text = text;
		key.wrapWidth = wrapWidth;
		key.colorRgba = color;
		key.direction = CalypsoTextDirection::LTR;
		// Tracking (single-line only by contract): DOM titles/labels run
		// 0.12em; body copy has none, so the default 0 keeps wrapped text on
		// SDL_ttf's layout.
		if (trackingEm > 0.0 && wrapWidth == 0)
		{
			key.letterSpacingPx = std::max(1, (int)calypsoHdRoundToInt(
				(double)physicalPixelHeight * trackingEm));
		}

		CalypsoHdItem it;
		it.kind = CalypsoHdItemKind::Text;
		it.rect = r;
		it.colorRgba = color;
		it.rasterKey = key;
		it.hAlign = hA;
		it.vAlign = vA;
		it.widget = widget;
		it.claim = { kF33FamilyId, role, inst, 1u, (std::uint32_t)ord };
		it.order = { 0, 0, kF33FamilyId, inst, 0, 1u, ord, role };
		builder.add(it);
		++ord;
	};

	// Styled panels (SDF: gradient fill, alpha borders, rounded corners), then
	// text -- ord is monotonic so the order key keeps that painter order.
	addStyled(widgetRect(_state->_window), CalypsoHdTheme::calypsoHdDialogStyle(),
		_state->_window, ROLE_WINDOW);
	// F33-PARITY-008: buttons snapshot their live interaction state (read-only)
	// and map it to the canonical semantic tokens -- hover/focus/pressed are
	// never static, and the widgets keep every input event.
	addStyled(widgetRect(_state->_btnYes), buttonStyleFor(
		CalypsoActionTone::Destructive, buttonVisualState(_state->_btnYes)),
		_state->_btnYes, ROLE_YES);
	addStyled(widgetRect(_state->_btnNo), buttonStyleFor(
		CalypsoActionTone::Safe, buttonVisualState(_state->_btnNo)),
		_state->_btnNo, ROLE_NO);

	addText(_state->_txtTitle, heading, _state->_txtTitle ? _state->_txtTitle->getText() : std::string(),
		CalypsoHdTheme::kGold, CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_TITLE,
		CalypsoHdTheme::kTitleTrackingEm);

	if (_state->_hdMessage && !_state->_hdMessage->getText().empty())
	{
		const CalypsoLogicalRect r = widgetRect(_state->_hdMessage);
		if (r.w > 0 && r.h > 0)
		{
			const int lines = std::max(1, _state->_hdMessage->getNumLines());
			const double perLineFrac = std::min(0.45, 1.0 / (lines * 1.25));
			const int physicalPixelHeight = std::max(1,
				(int)calypsoHdRoundToInt((double)r.h * sy * perLineFrac));
			const int wrapWidth = std::max(1, (int)calypsoHdRoundToInt((double)r.w * sx));

			CalypsoHdTextRasterKey key;
			key.source = body;
			key.physicalPixelHeight = physicalPixelHeight;
			key.text = _state->_hdMessage->getText();
			key.wrapWidth = wrapWidth;
			key.colorRgba = CalypsoHdTheme::kNearWhite;
			key.direction = CalypsoTextDirection::LTR;

			CalypsoHdItem it;
			it.kind = CalypsoHdItemKind::Text;
			it.rect = r;
			it.colorRgba = CalypsoHdTheme::kNearWhite;
			it.rasterKey = key;
			it.hAlign = CalypsoHdHAlign::Left;
			it.vAlign = CalypsoHdVAlign::Top;
			it.widget = _state->_hdMessage;
			it.claim = { kF33FamilyId, ROLE_MESSAGE, inst, 1u, (std::uint32_t)ord };
			it.order = { 0, 0, kF33FamilyId, inst, 0, 1u, ord, ROLE_MESSAGE };
			builder.add(it);
			++ord;
		}
	}

	addText(_state->_btnYes, heading, _state->_btnYes ? _state->_btnYes->getText() : std::string(),
		CalypsoHdTheme::kNearWhite, CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_YES,
		CalypsoHdTheme::kLabelTrackingEm);
	addText(_state->_btnNo, heading, _state->_btnNo ? _state->_btnNo->getText() : std::string(),
		CalypsoHdTheme::kNearWhite, CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_NO,
		CalypsoHdTheme::kLabelTrackingEm);
}

void CalypsoAbandonPopupUi::applyRects(AbandonGameState& state, const CalypsoF33AbandonLayout& layout)
{
	applyRect(state._window, layout.window);
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
	CalypsoF33AbandonLayout layout = calypsoF33AbandonLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);

	if (g_harnessAbandon)
	{
		// Side-by-side comparison: the layout class stays session-driven
		// (F33-PARITY-005); a Wide dialog is shifted into the left half
		// (window.x 340 -> 40) so the DOM reference card fits on the right.
		if (state._hdWideLayout)
		{
			const int dx = 40 - layout.window.x;
			layout.window.x += dx;
			layout.title.x += dx;
			layout.message.x += dx;
			layout.yes.x += dx;
			layout.no.x += dx;
		}
	}

	// Data-loss copy: an HD-only widget (the vanilla dialog has no message
	// band); absent on the logical fallback, exactly like the F34 badge.
	state._hdMessage = new Text(1, 1, 0, 0);
	state.add(state._hdMessage);
	state._hdMessage->setSmall();
	state._hdMessage->setColor(state._txtTitle->getColor());
	state._hdMessage->setAlign(ALIGN_LEFT);
	state._hdMessage->setVerticalAlign(ALIGN_TOP);
	state._hdMessage->setWordWrap(true);
	state._hdMessage->setText(state.tr("STR_CAL_ABANDON_DATA_LOSS"));

	CalypsoAbandonPopupUi::applyRects(state, layout);

	// Fit/center every design-space rect into the engine's actual logical canvas.
	state.enableUiScaling(layout.designWidth, layout.designHeight, 1.0f);

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
		const CalypsoF33AbandonLayout layout = calypsoF33AbandonLayout(
			wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
		CalypsoAbandonPopupUi::applyRects(state, layout);
		state.recaptureUiScaling(layout.designWidth, layout.designHeight, 1.0f);
	}
	else
	{
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
	EM_ASM({ if (globalThis.__calypsoHdHarnessShow) globalThis.__calypsoHdHarnessShow(); });
}

void OpenXcom::Calypso::hdHarnessDomHide()
{
	EM_ASM({ if (globalThis.__calypsoHdHarnessHide) globalThis.__calypsoHdHarnessHide(); });
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
