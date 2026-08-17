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

/// Classify the current Compact/Wide layout class from the USABLE safe area
/// (harness preview selection overrides automatic classification).
inline CalypsoLayoutClass currentF21LayoutClass()
{
	CalypsoBaseSafeRect safe{ 0, 0, Options::baseXResolution, Options::baseYResolution };
	(void)calypsoProjectedSafeRectForLayout(Options::baseXResolution, Options::baseYResolution, safe);
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

inline CalypsoLogicalRect f21WidgetRect(const Surface* surface)
{
	return { surface->getX(), surface->getY(), surface->getWidth(), surface->getHeight() };
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

	CalypsoLogicalRect motionRect(const CalypsoLogicalRect& r) const
	{
		if (scale >= 1.0) return r;
		const CalypsoF21Rect rect{ r.x, r.y, r.w, r.h };
		const CalypsoF21Rect m = calypsoF21ScaleRectAroundWindow(rect, window, scale);
		return { m.x, m.y, m.width, m.height };
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

	void text(Surface* widget, const CalypsoTtfSourceDescriptor& font,
		const std::string& text, std::uint32_t color,
		CalypsoHdHAlign hA, CalypsoHdVAlign vA, int linesHint, std::uint32_t role,
		double trackingEm = 0.0)
	{
		if (!widget || text.empty()) return;
		const CalypsoLogicalRect r = motionRect(f21WidgetRect(widget));
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
		it.opacity = opacity;
		it.widget = widget;
		it.claim = { familyId, role, inst, 1u, (std::uint32_t)ord };
		it.order = { 0, 0, familyId, inst, 0, 1, ord, role };
		builder.add(it);
		++ord;
	}
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
