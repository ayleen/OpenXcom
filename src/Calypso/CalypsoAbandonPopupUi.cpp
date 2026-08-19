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
 * set (confirm dialog: protocol + warning + data-loss copy + semantic actions).
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
#include "../Savegame/SavedGame.h"

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
	ROLE_YES = 4, ROLE_NO = 5, ROLE_PROTOCOL = 6,
	ROLE_WARNING = 7, ROLE_FOOTER = 8, ROLE_DECORATION = 9
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

/// Read-only interaction snapshot of a button (F33-PARITY-008): the adapter
/// NEVER owns events; it just reads the widget's live state.
CalypsoInteractionState buttonVisualState(const TextButton* btn, const TextButton* peer)
{
	if (!btn) return CalypsoInteractionState::Rest;
	// NOTE: the Disabled interaction state is unreachable today -- no widget
	// API exposes an enabled/disabled flag (InteractiveSurface has none), and
	// the F33 confirm buttons are always enabled. When a widget gains such a
	// flag, map it here (before hover/focus) so the Disabled tokens/opacity
	// from the contract take effect.
	if (btn->isPressed()) return CalypsoInteractionState::Pressed;
	// Hover precedes the default focus: the vanilla buttons start FOCUSED
	// (InteractiveSurface ctor) -- that is the keyboard-focus default, not a
	// user interaction -- so without this priority the pointer-over state
	// could never surface (F33-PARITY-008: hover/focus/pressed never static).
	if (btn->isHovered()) return CalypsoInteractionState::Hover;
	// InteractiveSurface constructs both buttons focused. That shared legacy
	// default is not a visible keyboard focus target; only a unique focus is.
	if (btn->isFocused() && (!peer || !peer->isFocused())) return CalypsoInteractionState::Focus;
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
	s.radiusPx = 1.0f;
	s.borderWidthPx = CalypsoHdTheme::kBorderWidthPx
		+ (state == CalypsoInteractionState::Focus ? 1.0f : 0.0f);
	const std::uint32_t f33Fill = state == CalypsoInteractionState::Rest
		? (tone == CalypsoActionTone::Destructive
			? CalypsoF33AbandonGen::kDestructiveFill
			: CalypsoF33AbandonGen::kSafeFill)
		: fill;
	s.borderColorRgba = state == CalypsoInteractionState::Rest
		? (tone == CalypsoActionTone::Destructive
			? CalypsoF33AbandonGen::kDestructiveFill
			: CalypsoF33AbandonGen::kSafeBorder)
		: borderColor;
	s.fillTopRgba = f33Fill;
	s.fillBottomRgba = f33Fill;
	s.gradDirX = 0.26f;
	s.gradDirY = 1.0f;
	return s;
}

CalypsoHdPanelStyle f33WindowStyle()
{
	CalypsoHdPanelStyle s;
	s.styled = true;
	s.shape = CalypsoHdPanelShape::OpposingCutRect;
	s.cutCornerPx = CalypsoF33AbandonGen::kCutCornerPx;
	s.borderWidthPx = 1.0f;
	s.borderColorRgba = CalypsoF33AbandonGen::kFrame;
	s.fillTopRgba = CalypsoF33AbandonGen::kPanelFillTop;
	s.fillBottomRgba = CalypsoF33AbandonGen::kPanelFillBottom;
	s.gradDirX = 0.18f;
	s.gradDirY = 1.0f;
	return s;
}

CalypsoHdPanelStyle f33GlowStyle(std::uint32_t color, float radius)
{
	CalypsoHdPanelStyle s = CalypsoHdTheme::calypsoHdGlowStyle(color, radius);
	s.shape = CalypsoHdPanelShape::OpposingCutRect;
	s.cutCornerPx = CalypsoF33AbandonGen::kCutCornerPx;
	return s;
}

CalypsoHdPanelStyle warningStyle()
{
	CalypsoHdPanelStyle s;
	s.styled = true;
	s.shape = CalypsoHdPanelShape::WarningTriangle;
	s.borderWidthPx = 2.0f;
	s.borderColorRgba = CalypsoF33AbandonGen::kWarning;
	s.fillTopRgba = calypsoRgba(0, 0, 0, 0);
	s.fillBottomRgba = calypsoRgba(0, 0, 0, 0);
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
	CalypsoTtfSourceDescriptor mono;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_SAIRA_700", heading)) return;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F33_BODY", body)) return;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_MONO", mono)) return;

	const CalypsoHdPresentationMetrics& m = CalypsoHdUiOverlay::instance().frozenMetrics();
	const double sx = m.scaleX, sy = m.scaleY;
	const bool wide = _state->_hdWideLayout;
	const CalypsoF33AbandonLayout designLayout = calypsoF33AbandonLayout(
		wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
	// State::enableUiScaling has already projected the design rects into the
	// current logical canvas. Keep the raster at design resolution and project
	// the composed texture by the total design-to-physical scale, just like the
	// DOM card's CSS transform. The SDL_ttf face itself must not absorb sx/sy:
	// doing so changes natural glyph metrics before the composed projection.
	const double uiScale = designLayout.window.width > 0
		? (double)widgetRect(_state->_window).w / designLayout.window.width : 1.0;
	const double projectionScaleX = uiScale * sx;
	const double projectionScaleY = uiScale * sy;
	const std::uint64_t inst = reinterpret_cast<std::uintptr_t>(_state);

	// Opening motion (F33-PARITY-007 follow-up): a monotonic presentation
	// clock (the overlay frame counter) drives opacity 0->1 and scale
	// scaleFrom->1 with an ease-out without overshoot, using the canonical
	// contract tokens. Capture mode (motion=0) disables it -> the first
	// screenshot frame is already the settled card.
	if (!_presented)
	{
		_presented = true;
		_presentedAtFrame = CalypsoHdUiOverlay::instance().frameId();
	}
	double progress = 1.0;
	// Deterministic freeze: the capture harness can pin the presentation clock
	// at a fixed progress so a screenshot shows a stable mid-ramp frame
	// (F33.5 motion evidence). Otherwise the live clock runs while motion is
	// enabled; capture mode (motion=0) settles instantly.
	const int holdPct = calypsoHarnessSession().motionHoldPct;
	if (holdPct >= 0)
	{
		progress = std::min(1.0, (double)holdPct / 100.0);
	}
	else if (!calypsoHarnessSession().motionDisabled)
	{
		const std::uint64_t totalFrames = std::max<std::uint64_t>(1,
			(std::uint64_t)std::llround(CalypsoF33AbandonGen::kMotionDurationMs * 60.0 / 1000.0));
		const std::uint64_t frame = CalypsoHdUiOverlay::instance().frameId();
		const std::uint64_t elapsed = frame >= _presentedAtFrame ? frame - _presentedAtFrame : 0;
		progress = std::min(1.0, (double)elapsed / (double)totalFrames);
	}
	const double ease = 1.0 - (1.0 - progress) * (1.0 - progress); // ease-out, no overshoot
	const double scale = CalypsoF33AbandonGen::kMotionScaleFrom
		+ (1.0 - CalypsoF33AbandonGen::kMotionScaleFrom) * ease;
	const float opacity = (float)ease;

	// Scale every dialog part around the (full-size) window centre; the
	// backdrop stays full-canvas. Raster keys derive from the SCALED rects so
	// text re-rasterises at the in-flight size (bounded: one entry per ramp
	// frame, evicted by the LRU after the ramp).
	const CalypsoLogicalRect winFull = widgetRect(_state->_window);
	auto projectDecoration = [&](const CalypsoF33Rect& design) -> CalypsoLogicalRect
	{
		return CalypsoLogicalRect{
			winFull.x + (int)std::llround((design.x - designLayout.window.x) * uiScale),
			winFull.y + (int)std::llround((design.y - designLayout.window.y) * uiScale),
			std::max(1, (int)std::llround(design.width * uiScale)),
			std::max(1, (int)std::llround(design.height * uiScale)) };
	};
	auto motionRect = [&](const CalypsoLogicalRect& r) -> CalypsoLogicalRect
	{
		if (scale >= 1.0) return r;
		const CalypsoF33Rect scaled = calypsoF33ScaleRectAroundWindow(
			{ r.x, r.y, r.w, r.h },
			{ winFull.x, winFull.y, winFull.w, winFull.h }, scale);
		return { scaled.x, scaled.y, scaled.width, scaled.height };
	};

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
		it.rect = motionRect(r);
		it.colorRgba = style.fillTopRgba;
		it.panelStyle = style;
		it.opacity = opacity;
		it.widget = widget;
		it.claim = { kF33FamilyId, role, inst, 1u, (std::uint32_t)ord };
		it.order = { 0, 0, kF33FamilyId, inst, 0, 1u, ord, role };
		builder.add(it);
		++ord;
	};
	auto addDecoration = [&](const CalypsoLogicalRect& r, std::uint32_t color,
		std::uint32_t role)
	{
		if (r.w <= 0 || r.h <= 0) return;
		CalypsoHdItem it;
		it.kind = CalypsoHdItemKind::Panel;
		it.rect = motionRect(r);
		it.colorRgba = color;
		it.panelStyle.styled = true;
		it.panelStyle.fillTopRgba = color;
		it.panelStyle.fillBottomRgba = color;
		it.opacity = opacity;
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
		addStyled(shadow, f33GlowStyle(
			CalypsoHdTheme::kShadowGlow, CalypsoHdTheme::kShadowGlowRadiusPx), nullptr, ROLE_WINDOW);
		addStyled(w, f33GlowStyle(
			CalypsoHdTheme::kHaloGlow, CalypsoHdTheme::kHaloGlowRadiusPx), nullptr, ROLE_WINDOW);
	}

	auto addTextRect = [&](const CalypsoLogicalRect& sourceRect, const void* widget,
		const CalypsoTtfSourceDescriptor& font,
		const std::string& text, std::uint32_t color,
		CalypsoHdHAlign hA, CalypsoHdVAlign vA, int linesHint, std::uint32_t role,
		double trackingEm = 0.0, double fontSizeDesignPx = 0.0)
	{
		if (text.empty()) return;
		const CalypsoLogicalRect r = motionRect(sourceRect);
		if (r.w <= 0 || r.h <= 0) return;

		const int hint = linesHint > 0 ? linesHint : 1;
		const double designFontSize = fontSizeDesignPx > 0.0
			? fontSizeDesignPx : (double)r.h / hint;
		const int physicalPixelHeight = std::max(1,
			(int)calypsoHdRoundToInt(designFontSize));
		const int wrapWidth = (hint > 1)
			? std::max(1, (int)calypsoHdRoundToInt((double)r.w * sx)) : 0;

		CalypsoHdTextRasterKey key;
		key.source = font;
		key.physicalPixelHeight = physicalPixelHeight;
		key.text = text;
		key.wrapWidth = wrapWidth;
		key.colorRgba = color;
		key.direction = CalypsoTextDirection::LTR;
		key.horizontalScalePermille = 1000;
		key.verticalScalePermille = 1000;
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
		it.textScaleX = (float)projectionScaleX;
		it.textScaleY = (float)projectionScaleY;
		it.hAlign = hA;
		it.vAlign = vA;
		it.opacity = opacity;
		it.widget = widget;
		it.claim = { kF33FamilyId, role, inst, 1u, (std::uint32_t)ord };
		it.order = { 0, 0, kF33FamilyId, inst, 0, 1u, ord, role };
		builder.add(it);
		++ord;
	};

	auto addText = [&](Surface* widget, const CalypsoTtfSourceDescriptor& font,
		const std::string& text, std::uint32_t color,
		CalypsoHdHAlign hA, CalypsoHdVAlign vA, int linesHint, std::uint32_t role,
		double trackingEm = 0.0, double fontSizeDesignPx = 0.0)
	{
		if (!widget) return;
		addTextRect(widgetRect(widget), widget, font, text, color, hA, vA,
			linesHint, role, trackingEm, fontSizeDesignPx);
	};

	// Styled panels (SDF: translucent opposing-corner frame), then structural
	// rules, warning glyph, actions, and text. ord remains painter order.
	// text -- ord is monotonic so the order key keeps that painter order.
	addStyled(widgetRect(_state->_window), f33WindowStyle(),
		_state->_window, ROLE_WINDOW);

	const CalypsoLogicalRect statusRect = projectDecoration(designLayout.status);
	const CalypsoLogicalRect warningRect = projectDecoration(designLayout.warning);
	const CalypsoLogicalRect footerRect = projectDecoration(designLayout.footer);
	addDecoration({ statusRect.x, statusRect.y + statusRect.h - 1, statusRect.w, 1 },
		CalypsoF33AbandonGen::kDivider, ROLE_DECORATION);
	addDecoration({ footerRect.x, footerRect.y, footerRect.w, 1 },
		CalypsoF33AbandonGen::kDivider, ROLE_FOOTER);
	for (int y = footerRect.y + 10; y < footerRect.y + footerRect.h - 8; y += 8)
	{
		for (int x = footerRect.x + 12; x < widgetRect(_state->_btnNo).x - 12; x += 8)
			addDecoration({ x, y, 1, 1 }, CalypsoF33AbandonGen::kFooterDot, ROLE_DECORATION);
	}
	addStyled(warningRect, warningStyle(), nullptr, ROLE_WARNING);
	// F33-PARITY-008: buttons snapshot their live interaction state (read-only)
	// and map it to the canonical semantic tokens -- hover/focus/pressed are
	// never static, and the widgets keep every input event.
	addStyled(widgetRect(_state->_btnYes), buttonStyleFor(
		CalypsoActionTone::Destructive, buttonVisualState(_state->_btnYes, _state->_btnNo)),
		_state->_btnYes, ROLE_YES);
	addStyled(widgetRect(_state->_btnNo), buttonStyleFor(
		CalypsoActionTone::Safe, buttonVisualState(_state->_btnNo, _state->_btnYes)),
		_state->_btnNo, ROLE_NO);

	const CalypsoLogicalRect protocolTextRect = projectDecoration(
		calypsoF33ProtocolTextRect(designLayout.status));
	addTextRect(protocolTextRect, _state->_hdProtocol, mono,
		_state->_hdProtocol ? _state->_hdProtocol->getText() : std::string(),
		CalypsoF33AbandonGen::kProtocolText, CalypsoHdHAlign::Left,
		CalypsoHdVAlign::Middle, 1, ROLE_PROTOCOL, 0.10,
		_state->_hdWideLayout ? 10.0 : 9.0);
	addTextRect(warningRect, nullptr, heading, "!", CalypsoF33AbandonGen::kWarning,
		CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_WARNING, 0.0,
		_state->_hdWideLayout ? 18.0 : 16.0);
	addText(_state->_txtTitle, heading, _state->_txtTitle ? _state->_txtTitle->getText() : std::string(),
		CalypsoHdTheme::kNearWhite, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_TITLE,
		CalypsoHdTheme::kTitleTrackingEm,
		(double)designLayout.title.height * CalypsoHdTheme::kTitleFontSizeScale);

	if (_state->_hdMessage && !_state->_hdMessage->getText().empty())
	{
		const CalypsoLogicalRect r = widgetRect(_state->_hdMessage);
		if (r.w > 0 && r.h > 0)
		{
			const std::string message = _state->_hdMessage->getText();
			const double bodySizeScale = wide
				? CalypsoHdTheme::kBodyFontSizeScaleWide
				: CalypsoHdTheme::kBodyFontSizeScaleCompact;
			const double bodyWidthScale = wide
				? CalypsoHdTheme::kBodyFontWidthScaleWide
				: CalypsoHdTheme::kBodyFontWidthScaleCompact;
			const double bodyWrapMeasureScale = wide
				? CalypsoHdTheme::kBodyWrapMeasureScaleWide
				: CalypsoHdTheme::kBodyWrapMeasureScaleCompact;
			const double bodyProjectionLineHeightScale = wide
				? CalypsoHdTheme::kBodyProjectionLineHeightScaleWide
				: CalypsoHdTheme::kBodyProjectionLineHeightScaleCompact;
			const int physicalPixelHeight = std::max(1,
				(int)calypsoHdRoundToInt((double)CalypsoHdTheme::kBodyFontSizePx *
					bodySizeScale));
			// Wrapping is authored in the immutable design contract. Presentation
			// scale belongs only to the GPU quad; feeding it into this width would
			// change word breaks across DPR/viewport pairs.
			const int wrapWidth = std::max(1, designLayout.message.width);

			CalypsoHdTextRasterKey key;
			key.source = body;
			key.physicalPixelHeight = physicalPixelHeight;
			key.text = message;
			key.wrapWidth = wrapWidth;
			const double authoredLineHeight =
				(double)CalypsoHdTheme::kBodyFontSizePx * bodySizeScale
					* CalypsoHdTheme::kBodyLineHeight
					* bodyProjectionLineHeightScale;
			const double designLineHeight = authoredLineHeight;
			key.lineHeightPx = std::max(1,
				(int)calypsoHdRoundToInt(designLineHeight));
			key.lineHeightMilliPx = std::max(1,
				(int)calypsoHdRoundToInt(designLineHeight * 1000.0));
			key.horizontalScalePermille = std::max(1,
				(int)calypsoHdRoundToInt(bodyWidthScale * 1000.0));
			key.verticalScalePermille = 1000;
			key.wrapMeasureScalePermille = std::max(1,
				(int)calypsoHdRoundToInt(bodyWrapMeasureScale * 1000.0));
			key.colorRgba = CalypsoHdTheme::kNearWhite;
			key.direction = CalypsoTextDirection::LTR;

			CalypsoHdItem it;
			it.kind = CalypsoHdItemKind::Text;
			it.rect = motionRect(r);
			it.colorRgba = CalypsoHdTheme::kNearWhite;
			it.rasterKey = key;
			it.textScaleX = (float)projectionScaleX;
			// At enlarged Compact projections the generated box is two physical
			// pixels shorter than the guarded raster envelope. Preserve the authored
			// line-height and apply the minimal projection-only fit to the composed
			// texture; normal-size and Wide layouts remain at 1:1.
			const double projectionFitY = wide ? 1.0 : 0.98;
			it.textScaleY = (float)(projectionScaleY * projectionFitY);
			it.hAlign = CalypsoHdHAlign::Left;
			it.vAlign = CalypsoHdVAlign::Top;
			it.opacity = opacity;
			it.widget = _state->_hdMessage;
			it.claim = { kF33FamilyId, ROLE_MESSAGE, inst, 1u, (std::uint32_t)ord };
			it.order = { 0, 0, kF33FamilyId, inst, 0, 1u, ord, ROLE_MESSAGE };
			builder.add(it);
			++ord;
		}
	}

	addText(_state->_btnYes, heading, _state->_btnYes ? _state->_btnYes->getText() : std::string(),
		CalypsoF33AbandonGen::kDestructiveText, CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_YES,
		CalypsoHdTheme::kLabelTrackingEm,
		(double)CalypsoHdTheme::kLabelFontSizePx * (_state->_hdWideLayout
			? CalypsoHdTheme::kLabelFontSizeScaleWide
			: CalypsoHdTheme::kLabelFontSizeScaleCompact));
	addText(_state->_btnNo, heading, _state->_btnNo ? _state->_btnNo->getText() : std::string(),
		CalypsoHdTheme::kNearWhite, CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_NO,
		CalypsoHdTheme::kLabelTrackingEm,
		(double)CalypsoHdTheme::kLabelFontSizePx * (_state->_hdWideLayout
			? CalypsoHdTheme::kLabelFontSizeScaleWide
			: CalypsoHdTheme::kLabelFontSizeScaleCompact));
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
