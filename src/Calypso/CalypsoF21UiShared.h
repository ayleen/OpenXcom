#pragma once
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
 * F21 (Calypso, Phase 46.F21): shared collect-time helpers for the five F21
 * family adapters. Structural extraction of the per-frame lambdas the F33
 * adapter carried inline (painter-order item submission, interaction
 * snapshots, semantic button styling, opening motion) so five adapters do
 * not fork five copies. Whole-file Emscripten guard like every Calypso-only
 * implementation unit.
 */
#ifdef __EMSCRIPTEN__

#include <algorithm>
#include <cstdint>

#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/TextButton.h"

#include "CalypsoF21LayoutBase.h"
#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdInteractionState.h"
#include "CalypsoHdTextRasterKey.h"
#include "CalypsoHdTheme.h"
#include "CalypsoHdUiModel.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoUiMetrics.h"
#include "CalypsoViewportRuntime.h"

namespace OpenXcom
{
namespace Calypso
{

constexpr std::uint32_t kF21FamilyId = 21;

// --- F33 command-card visual language (Phase 46.F21 beautify pass) ---------
// The approved F33 kit, re-expressed through the shared theme tokens: the
// opposing-cut translucent frame, the cut-shaped glow pair, the amber caution
// triangle, and the mono protocol strip. Family-specific protocol COPY lives
// in the hd-pack ruleset; geometry lives in each family contract.

/// Command frame: opposing-cut corners, translucent gradient fill, 1px frame.
inline CalypsoHdPanelStyle f21WindowStyle()
{
	CalypsoHdPanelStyle s;
	s.styled = true;
	s.shape = CalypsoHdPanelShape::OpposingCutRect;
	s.cutCornerPx = 14.0f;
	s.borderWidthPx = 1.0f;
	s.borderColorRgba = CalypsoHdThemeGen::kAccentSoft;
	s.fillTopRgba = calypsoRgba(0x08, 0x19, 0x1D, 0xEB);
	s.fillBottomRgba = calypsoRgba(0x04, 0x10, 0x14, 0xE8);
	s.gradDirX = 0.18f;
	s.gradDirY = 1.0f;
	return s;
}

/// Dark inset material for grouped facts. It structures empty space without
/// competing with the outer command frame.
inline CalypsoHdPanelStyle f21InsetPanelStyle()
{
	CalypsoHdPanelStyle s;
	s.styled = true;
	s.shape = CalypsoHdPanelShape::OpposingCutRect;
	s.cutCornerPx = 8.0f;
	s.borderWidthPx = 1.0f;
	s.borderColorRgba = calypsoRgba(0x74, 0xFF, 0xB0, 0x2E);
	s.fillTopRgba = calypsoRgba(0x03, 0x11, 0x16, 0xE8);
	s.fillBottomRgba = calypsoRgba(0x02, 0x0B, 0x10, 0xE4);
	s.gradDirX = 0.2f;
	s.gradDirY = 1.0f;
	return s;
}

/// Native TextEdit material. Focus changes border thickness as well as colour
/// so the state does not rely on colour perception alone.
inline CalypsoHdPanelStyle f21InputStyle(bool focused)
{
	CalypsoHdPanelStyle s;
	s.styled = true;
	s.radiusPx = 2.0f;
	s.borderWidthPx = focused ? 2.0f : 1.0f;
	s.borderColorRgba = focused
		? CalypsoHdThemeGen::kAccent
		: calypsoRgba(0x74, 0xFF, 0xB0, 0x52);
	s.fillTopRgba = calypsoRgba(0x02, 0x0B, 0x10, 0xF2);
	s.fillBottomRgba = calypsoRgba(0x04, 0x14, 0x18, 0xEC);
	s.gradDirX = 0.15f;
	s.gradDirY = 1.0f;
	return s;
}

/// Glow (shadow/halo) in the same opposing-cut silhouette as the frame.
inline CalypsoHdPanelStyle f21GlowStyle(std::uint32_t color, float radiusPx)
{
	CalypsoHdPanelStyle s = CalypsoHdTheme::calypsoHdGlowStyle(color, radiusPx);
	s.shape = CalypsoHdPanelShape::OpposingCutRect;
	s.cutCornerPx = 14.0f;
	return s;
}

/// Amber caution triangle (stroke only; the "!" glyph is drawn over it).
inline CalypsoHdPanelStyle f21WarningGlyphStyle()
{
	CalypsoHdPanelStyle s;
	s.styled = true;
	s.shape = CalypsoHdPanelShape::WarningTriangle;
	s.borderWidthPx = 2.0f;
	s.borderColorRgba = CalypsoHdThemeGen::kGold;
	s.fillTopRgba = calypsoRgba(0, 0, 0, 0);
	s.fillBottomRgba = calypsoRgba(0, 0, 0, 0);
	return s;
}

constexpr std::uint32_t kF21DividerRgba = 0x74FFB04Du;      ///< 1px structural rules
constexpr std::uint32_t kF21ProtocolTextRgba = 0xA9D8C7FFu; ///< mono protocol copy
constexpr std::uint32_t kF21MutedBodyRgba = 0xA9D8C7D0u;    ///< readable secondary copy
constexpr std::uint32_t kF21FooterDotRgba = 0x74FFB01Fu;    ///< sparse footer dots

/// Classify the current Compact/Wide layout class from the USABLE safe area
/// (harness preview selection overrides automatic classification).
inline CalypsoLayoutClass currentF21LayoutClass()
{
	CalypsoBaseSafeRect safe{ 0, 0, Options::baseXResolution, Options::baseYResolution };
	const CalypsoViewportRuntime& runtime = calypsoViewportRuntime();
	if (runtime.hasLayout())
	{
		// Layout class is a CSS-logical policy decision. Projecting into the
		// current base framebuffer here makes a state pushed during geoscape
		// reflow observe a transient/DPR-scaled size and can select Compact on
		// an otherwise Wide viewport.
		const CalypsoLayoutMetrics& metrics = runtime.current();
		safe.x = metrics.safeX;
		safe.y = metrics.safeY;
		safe.width = metrics.safeWidth;
		safe.height = metrics.safeHeight;
	}
	else
	{
		(void)calypsoProjectedSafeRectForLayout(Options::baseXResolution, Options::baseYResolution, safe);
	}
	return calypsoHarnessEffectiveLayout(calypsoHarnessSession(), safe);
}

/// Read-only interaction snapshot (F33-PARITY-008): the adapter NEVER owns
/// events; it reads the widget's live state.
inline CalypsoInteractionState f21ButtonVisualState(const TextButton* btn)
{
	if (!btn) return CalypsoInteractionState::Rest;
	if (btn->isPressed()) return CalypsoInteractionState::Pressed;
	if (btn->isHovered()) return CalypsoInteractionState::Hover;
	if (btn->isFocused()) return CalypsoInteractionState::Focus;
	return CalypsoInteractionState::Rest;
}

/// SDF panel style for one (tone, state) presentation; keyboard focus adds
/// the non-colour thicker-ring cue.
inline CalypsoHdPanelStyle f21ButtonStyleFor(CalypsoActionTone tone, CalypsoInteractionState state)
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

/// Quiet navigation action: same focus semantics as other controls, but no
/// semantic danger fill. Cancel is not destructive.
inline CalypsoHdPanelStyle f21QuietButtonStyle(CalypsoInteractionState state)
{
	CalypsoHdPanelStyle s;
	s.styled = true;
	s.radiusPx = CalypsoHdTheme::kButtonRadiusPx;
	s.borderWidthPx = state == CalypsoInteractionState::Focus ? 2.0f : 1.0f;
	s.borderColorRgba = state == CalypsoInteractionState::Focus
		? CalypsoHdThemeGen::kAccent
		: CalypsoHdThemeGen::kAccentSoft;
	const std::uint32_t fill = state == CalypsoInteractionState::Pressed
		? calypsoRgba(0x10, 0x35, 0x31, 0xD8)
		: state == CalypsoInteractionState::Hover
			? calypsoRgba(0x0B, 0x25, 0x25, 0xC8)
			: calypsoRgba(0x05, 0x0F, 0x14, 0x9C);
	s.fillTopRgba = fill;
	s.fillBottomRgba = fill;
	return s;
}

/// Shared strategic time rail: the six speed slots and pause control sit on
/// one horizontal track in both generated wide and compact layouts.
inline CalypsoHdPanelStyle f21TimeSpeedRailStyle()
{
	CalypsoHdPanelStyle s;
	s.styled = true;
	s.radiusPx = CalypsoHdTheme::kButtonRadiusPx * 1.5f;
	s.borderWidthPx = 1.0f;
	s.borderColorRgba = CalypsoHdThemeGen::kAccentSoft;
	s.fillTopRgba = calypsoRgba(0x05, 0x0F, 0x14, 0x70);
	s.fillBottomRgba = calypsoRgba(0x05, 0x0F, 0x14, 0x18);
	s.glowRgba = CalypsoHdThemeGen::kHaloGlow;
	s.glowRadiusPx = 8.0f;
	return s;
}

inline CalypsoLogicalRect f21WidgetRect(const Surface* surface)
{
	return { surface->getX(), surface->getY(), surface->getWidth(), surface->getHeight() };
}

/// Four-sided glyph-safe rectangle inside the structural protocol band. The
/// band itself owns the border/divider; text must never share those pixels.
inline CalypsoLogicalRect f21ProtocolTextRect(const Surface* surface, bool wide)
{
	const CalypsoLogicalRect r = f21WidgetRect(surface);
	const int ix = wide
		? CalypsoHdThemeGen::kF21ProtocolInsetXWidePx
		: CalypsoHdThemeGen::kF21ProtocolInsetXCompactPx;
	const int iy = wide
		? CalypsoHdThemeGen::kF21ProtocolInsetYWidePx
		: CalypsoHdThemeGen::kF21ProtocolInsetYCompactPx;
	return { r.x + ix, r.y + iy, std::max(1, r.w - ix * 2), std::max(1, r.h - iy * 2) };
}

/// Opening-motion clock: monotonic progress 0..1 with ease-out and no
/// overshoot. Capture mode (motion=0) settles instantly; the harness can pin
/// a deterministic mid-ramp progress via motionHoldPct.
struct CalypsoF21Motion
{
	double scale = 1.0;
	float opacity = 1.0f;

	CalypsoF21Motion(bool presented, std::uint64_t presentedAtFrame,
		int durationMs, double scaleFrom)
	{
		double progress = 1.0;
		const int holdPct = calypsoHarnessSession().motionHoldPct;
		if (holdPct >= 0)
		{
			progress = std::min(1.0, (double)holdPct / 100.0);
		}
		else if (!calypsoHarnessSession().motionDisabled)
		{
			const std::uint64_t totalFrames = std::max<std::uint64_t>(1,
				(std::uint64_t)std::llround(durationMs * 60.0 / 1000.0));
			const std::uint64_t frame = CalypsoHdUiOverlay::instance().frameId();
			const std::uint64_t elapsed = frame >= presentedAtFrame ? frame - presentedAtFrame : 0;
			progress = std::min(1.0, (double)elapsed / (double)totalFrames);
		}
		const double ease = 1.0 - (1.0 - progress) * (1.0 - progress);
		scale = scaleFrom + (1.0 - scaleFrom) * ease;
		opacity = (float)ease;
	}
};

/// Per-frame painter: owns the monotonic painter-order counter and submits
/// items with uniform claim/order identities inside ONE atomic subgroup.
struct CalypsoF21Painter
{
	CalypsoHdFrameBuilder& builder;
	std::uint32_t familyId;
	std::uint64_t inst;
	int ord = 0;
	float opacity = 1.0f;
	double scale = 1.0f;
	CalypsoF21Rect window;       ///< motion centre (design space)
	double sx = 1.0, sy = 1.0;   ///< frozen presentation metrics
	CalypsoLogicalRect winLogical{ 0, 0, 0, 0 }; ///< projected window (logical)
	CalypsoF21Rect windowDesign{ 0, 0, 0, 0 };   ///< window rect in design space
	double uiScale = 1.0;        ///< logical/design scale (winLogical.w / windowDesign.width)
	double uiAspectY = 1.0;      ///< per-axis compensation for CSS-authored surfaces

	/// Project a design-space decoration rect into the current logical canvas
	/// (the same projection State::enableUiScaling applied to widget rects).
	CalypsoLogicalRect project(const CalypsoF21Rect& d) const
	{
		return CalypsoLogicalRect{
			winLogical.x + (int)std::llround((d.x - windowDesign.x) * uiScale),
			winLogical.y + (int)std::llround((d.y - windowDesign.y) * uiScale * uiAspectY),
			std::max(1, (int)std::llround(d.width * uiScale)),
			std::max(1, (int)std::llround(d.height * uiScale * uiAspectY)) };
	}

	CalypsoLogicalRect motionRect(const CalypsoLogicalRect& r) const
	{
		if (scale >= 1.0) return r;
		const CalypsoF21Rect rect{ r.x, r.y, r.w, r.h };
		const CalypsoF21Rect m = calypsoF21ScaleRectAroundWindow(rect, window, scale);
		return { m.x, m.y, m.width, m.height };
	}

	/// Structural decoration (1px rules, footer dots): flat quad, motion-aware.
	void decoration(const CalypsoLogicalRect& r, std::uint32_t color, std::uint32_t role)
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
		it.claim = { familyId, role, inst, 1u, (std::uint32_t)ord };
		it.order = { 0, 0, familyId, inst, 0, 1, ord, role };
		builder.add(it);
		++ord;
	}

	void panel(const CalypsoLogicalRect& r, std::uint32_t color,
		const void* widget, std::uint32_t role)
	{
		if (r.w <= 0 || r.h <= 0) return;
		CalypsoHdItem it;
		it.kind = CalypsoHdItemKind::Panel;
		it.rect = r;
		it.colorRgba = color;
		it.widget = widget;
		it.claim = { familyId, role, inst, 1u, (std::uint32_t)ord };
		it.order = { 0, 0, familyId, inst, 0, 1, ord, role };
		builder.add(it);
		++ord;
	}

	/// Claim a legacy widget whose visual is intentionally covered by this
	/// atomic form. The transparent item has no visible effect; its widget
	/// identity still suppresses the widget's native blit for this frame.
	void claim(const void* widget, std::uint32_t role)
	{
		if (!widget) return;
		panel(CalypsoLogicalRect{ 0, 0, 1, 1 }, calypsoRgba(0, 0, 0, 0), widget, role);
	}

	void styled(const CalypsoLogicalRect& r, const CalypsoHdPanelStyle& style,
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
		it.claim = { familyId, role, inst, 1u, (std::uint32_t)ord };
		it.order = { 0, 0, familyId, inst, 0, 1, ord, role };
		builder.add(it);
		++ord;
	}

	/// Explicit-rect text (decorations like the "!" glyph have no widget).
	void textRect(const CalypsoLogicalRect& sourceRect, const void* widget,
		const CalypsoTtfSourceDescriptor& font,
		const std::string& text, std::uint32_t color,
		CalypsoHdHAlign hA, CalypsoHdVAlign vA, int linesHint, std::uint32_t role,
		double trackingEm = 0.0, double fontSizeDesignPx = 0.0,
		bool autoWrap = true);

	void text(Surface* widget, const CalypsoTtfSourceDescriptor& font,
		const std::string& text, std::uint32_t color,
		CalypsoHdHAlign hA, CalypsoHdVAlign vA, int linesHint, std::uint32_t role,
		double trackingEm = 0.0, double fontSizeDesignPx = 0.0,
		bool autoWrap = true)
	{
		if (!widget || text.empty()) return;
		textRect(f21WidgetRect(widget), widget, font, text, color, hA, vA,
			linesHint, role, trackingEm, fontSizeDesignPx, autoWrap);
	}
};

inline void CalypsoF21Painter::textRect(const CalypsoLogicalRect& sourceRect,
	const void* widget, const CalypsoTtfSourceDescriptor& font,
	const std::string& text, std::uint32_t color,
	CalypsoHdHAlign hA, CalypsoHdVAlign vA, int linesHint, std::uint32_t role,
	double trackingEm, double fontSizeDesignPx, bool autoWrap)
{
	if (text.empty()) return;
	const CalypsoLogicalRect r = motionRect(sourceRect);
	if (r.w <= 0 || r.h <= 0) return;

	const int hint = linesHint > 0 ? linesHint : 1;
	const double designFontSize = fontSizeDesignPx > 0.0
		? fontSizeDesignPx : (double)sourceRect.h / hint;
	const double physicalScaleX = std::max(0.01, uiScale * sx);
	const double physicalScaleY = std::max(0.01, uiScale * uiAspectY * sy);
	const int physicalPixelHeight = std::max(1, (int)calypsoHdRoundToInt(
		designFontSize * physicalScaleY));
	const int wrapWidth = hint > 1
		? std::max(1, (int)calypsoHdRoundToInt((double)sourceRect.w * sx)) : 0;

	CalypsoHdTextRasterKey key;
	key.source = font;
	key.physicalPixelHeight = physicalPixelHeight;
	key.text = text;
	key.wrapWidth = wrapWidth;
	key.explicitBreaksOnly = !autoWrap;
	key.horizontalScalePermille = std::max(1, (int)calypsoHdRoundToInt(
		physicalScaleX / physicalScaleY * 1000.0));
	if (hint > 1)
	{
		// Force every F21 multi-line item through the guarded per-line
		// compositor. The stock wrapped surface has no contract-owned line skip
		// and has shaved final-line pixels in the pinned Emscripten SDL_ttf port.
		key.lineHeightPx = std::max(1, (int)calypsoHdRoundToInt(
			physicalPixelHeight * CalypsoHdThemeGen::kF21BodyLineHeight));
	}
	key.colorRgba = color;
	key.direction = CalypsoTextDirection::LTR;
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
	it.textScaleX = 1.0f;
	it.textScaleY = 1.0f;
	it.hAlign = hA;
	it.vAlign = vA;
	it.opacity = opacity;
	it.widget = widget;
	it.claim = { familyId, role, inst, 1u, (std::uint32_t)ord };
	it.order = { 0, 0, familyId, inst, 0, 1, ord, role };
	builder.add(it);
	++ord;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
