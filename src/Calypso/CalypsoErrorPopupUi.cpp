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
 * Phase 46.2-HD (Calypso) -- see CalypsoErrorPopupUi.h (remediation B-Error).
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoErrorPopupUi.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <SDL.h>
#include <SDL_ttf.h>

#include "../Engine/FileMap.h"
#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Menu/ErrorMessageState.h"
#include "../Mod/Mod.h"

#include "CalypsoBevelPanel.h"
#include "CalypsoF34ErrorLayout.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdTextRasterKey.h"
#include "CalypsoHdUiModel.h"
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

// Fixed HD theme for the physical replacement (0xRRGGBBAA, packed via
// calypsoRgba so the byte order matches the rasteriser's unpack -- remediation
// B4). The bitmap fallback keeps the caller-supplied palette theme unchanged.
constexpr std::uint32_t kWindowBorderRgba    = calypsoRgba(0x74, 0xff, 0xb0);
constexpr std::uint32_t kWindowFillRgba      = calypsoRgba(0x10, 0x2a, 0x24);
constexpr std::uint32_t kIconPanelBorderRgba = calypsoRgba(0xff, 0xc1, 0x4d);
constexpr std::uint32_t kIconPanelFillRgba   = calypsoRgba(0x1b, 0x3f, 0x37);
constexpr std::uint32_t kButtonBorderRgba    = calypsoRgba(0x74, 0xff, 0xb0);
constexpr std::uint32_t kButtonFillRgba      = calypsoRgba(0x16, 0x4c, 0x3d);
constexpr std::uint32_t kGoldTextRgba        = calypsoRgba(0xff, 0xc1, 0x4d); // icon + heading
constexpr std::uint32_t kNearWhiteTextRgba   = calypsoRgba(0xe8, 0xff, 0xf5); // message + label

// F34 claim roles (stableId) within the one popup subgroup.
enum F34Role : std::uint32_t
{
	ROLE_WINDOW = 1, ROLE_BADGE = 2, ROLE_BUTTON = 3,
	ROLE_ICON = 4, ROLE_WARNING = 5, ROLE_MESSAGE = 6, ROLE_LABEL = 7
};
constexpr std::uint32_t kF34FamilyId = 34;

CalypsoLayoutClass currentF34LayoutClass()
{
	// Classify from the USABLE safe area (after insets) -- the same rect
	// applyUiScaling fits the design canvas into -- not the raw framebuffer. A
	// viewport whose raw size is Wide but whose usable area is only Compact-sized
	// must pick Compact, else a Wide design is force-squeezed into too little
	// space (external review #9).
	CalypsoBaseSafeRect safe{ 0, 0, Options::baseXResolution, Options::baseYResolution };
	(void)calypsoProjectedSafeRectForLayout(Options::baseXResolution, Options::baseYResolution, safe);
	return calypsoClassifySafeArea(safe.width, safe.height);
}

void applyRect(Surface* surface, const CalypsoF34Rect& rect)
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

CalypsoErrorPopupUi::~CalypsoErrorPopupUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoErrorPopupUi::topState() const
{
	return _state;
}

void CalypsoErrorPopupUi::collect(CalypsoHdFrameBuilder& builder) const
{
	if (!_state || !_state->_hdLayout || !_state->_game) return;

	// Let the logical popup's scale-in animation play; only take over physically
	// once the window is fully popped, so the HD popup doesn't snap in instantly
	// while the animating logical widgets are suppressed (Codex #1).
	if (_state->_window && !_state->_window->isPopupDone()) return;

	Mod* mod = _state->_game->getMod();
	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor body;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_SAIRA_700", heading)) return;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_MONO", body)) return;

	const CalypsoHdPresentationMetrics& m = CalypsoHdUiOverlay::instance().frozenMetrics();
	const double sx = m.scaleX, sy = m.scaleY;
	const std::uint64_t inst = reinterpret_cast<std::uintptr_t>(_state);
	const int border = calypsoBorderFor(_state->_hdWideLayout
		? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);

	// One atomic subgroup: the whole popup shows physically or not at all.
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
		it.claim = { kF34FamilyId, role, inst, 1u, (std::uint32_t)ord };
		it.order = { 0, 0, kF34FamilyId, inst, 0, 1u, ord, role };
		builder.add(it);
		++ord;
	};

	auto addBevel = [&](Surface* widget, std::uint32_t bcol, std::uint32_t fcol,
		std::uint32_t role)
	{
		if (!widget) return;
		const CalypsoLogicalRect r = widgetRect(widget);
		addPanel(r, bcol, widget, role); // border item carries the claim
		const CalypsoLogicalRect inner{ r.x + border, r.y + border,
			r.w - border * 2, r.h - border * 2 };
		addPanel(inner, fcol, nullptr, role); // inset fill, no separate widget
	};

	auto addText = [&](Surface* widget, const CalypsoTtfSourceDescriptor& font,
		const std::string& text, std::uint32_t color,
		CalypsoHdHAlign hA, CalypsoHdVAlign vA, int linesHint, std::uint32_t role)
	{
		if (!widget || text.empty()) return;
		const CalypsoLogicalRect r = widgetRect(widget);
		if (r.w <= 0 || r.h <= 0) return;

		const int hint = linesHint > 0 ? linesHint : 1;
		// Physical font height from the frozen vertical scale: the layout box
		// height encodes the design font proportion, so per-line box height *
		// scaleY carries the design->physical scale (never a hardcoded DPR).
		const int physicalPixelHeight = std::max(1, (int)calypsoHdRoundToInt((double)r.h / hint * sy));
		// Multi-line boxes: let SDL_ttf wrap at the physical box width (correct for
		// CJK / no-space text -- Codex #5); single-line boxes never wrap.
		const int wrapWidth = (hint > 1)
			? std::max(1, (int)calypsoHdRoundToInt((double)r.w * sx)) : 0;

		CalypsoHdTextRasterKey key;
		key.source = font;
		key.physicalPixelHeight = physicalPixelHeight;
		key.text = text;
		key.wrapWidth = wrapWidth;
		key.colorRgba = color;
		key.direction = CalypsoTextDirection::LTR;

		CalypsoHdItem it;
		it.kind = CalypsoHdItemKind::Text;
		it.rect = r;
		it.colorRgba = color;
		it.rasterKey = key;
		it.hAlign = hA;
		it.vAlign = vA;
		it.widget = widget;
		it.claim = { kF34FamilyId, role, inst, 1u, (std::uint32_t)ord };
		it.order = { 0, 0, kF34FamilyId, inst, 0, 1u, ord, role };
		builder.add(it);
		++ord;
	};

	// Panels (bevels) first, then text -- ord is monotonic so the order key
	// keeps that painter order deterministically.
	addBevel(_state->_window, kWindowBorderRgba, kWindowFillRgba, ROLE_WINDOW);
	addBevel(_state->_hdIconPanel, kIconPanelBorderRgba, kIconPanelFillRgba, ROLE_BADGE);
	addBevel(_state->_btnOk, kButtonBorderRgba, kButtonFillRgba, ROLE_BUTTON);

	addText(_state->_hdIcon, heading, _state->_hdIcon ? _state->_hdIcon->getText() : std::string(),
		kGoldTextRgba, CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_ICON);
	addText(_state->_hdWarning, body, _state->_hdWarning ? _state->_hdWarning->getText() : std::string(),
		kGoldTextRgba, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_WARNING);

	// Message: sized to the ACTUAL wrapped line count in the merged message+detail
	// box, so multi-line / localized messages are never truncated (external review
	// #6). Per-line height is capped at ~the design single-line proportion so short
	// messages don't balloon, and bounded by 1/(lines * line-skip) so the wrapped
	// surface (SDL_ttf line skip ~1.25x point size) always fits the box.
	if (_state->_txtMessage && !_state->_txtMessage->getText().empty())
	{
		const CalypsoLogicalRect r = widgetRect(_state->_txtMessage);
		if (r.w > 0 && r.h > 0)
		{
			const int lines = std::max(1, _state->_txtMessage->getNumLines());
			const double perLineFrac = std::min(0.45, 1.0 / (lines * 1.25));
			const int physicalPixelHeight = std::max(1,
				(int)calypsoHdRoundToInt((double)r.h * sy * perLineFrac));
			const int wrapWidth = std::max(1, (int)calypsoHdRoundToInt((double)r.w * sx));

			CalypsoHdTextRasterKey key;
			key.source = heading;
			key.physicalPixelHeight = physicalPixelHeight;
			key.text = _state->_txtMessage->getText();
			key.wrapWidth = wrapWidth;
			key.colorRgba = kNearWhiteTextRgba;
			key.direction = CalypsoTextDirection::LTR;

			CalypsoHdItem it;
			it.kind = CalypsoHdItemKind::Text;
			it.rect = r;
			it.colorRgba = kNearWhiteTextRgba;
			it.rasterKey = key;
			it.hAlign = CalypsoHdHAlign::Left;
			it.vAlign = CalypsoHdVAlign::Top;
			it.widget = _state->_txtMessage;
			it.claim = { kF34FamilyId, ROLE_MESSAGE, inst, 1u, (std::uint32_t)ord };
			it.order = { 0, 0, kF34FamilyId, inst, 0, 1u, ord, ROLE_MESSAGE };
			builder.add(it);
			++ord;
		}
	}

	addText(_state->_btnOk, body, _state->_btnOk ? _state->_btnOk->getText() : std::string(),
		kNearWhiteTextRgba, CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_LABEL);
}

void CalypsoErrorPopupUi::applyRects(ErrorMessageState& state, const CalypsoF34ErrorLayout& layout)
{
	applyRect(state._window, layout.window);
	applyRect(state._hdIconPanel, layout.iconPanel);
	applyRect(state._hdIcon, layout.icon);
	applyRect(state._hdWarning, layout.warning);
	// The message occupies BOTH the primary (message) and detail (messageDetail)
	// design bands as one word-wrapped region, so long / localized messages use
	// the full area instead of clipping in the small primary band and leaving the
	// detail band empty (external review #6).
	CalypsoF34Rect messageBox = layout.message;
	messageBox.height = layout.messageDetail.y + layout.messageDetail.height - layout.message.y;
	applyRect(state._txtMessage, messageBox);
	applyRect(state._btnOk, layout.acknowledge);
}

void CalypsoErrorPopupUi::configure(ErrorMessageState& state, bool allowPhysicalOverlay)
{
	state._hdLayout = state._game && state._game->getMod()
		&& state._game->getLanguage()
		&& isF34PhysicalRouteEligible(
			state._game->getMod()->isHdUiFamilyEnabled("F34"),
			state._game->getLanguage()->getTextDirection() == DIRECTION_RTL,
			!allowPhysicalOverlay);
	if (!state._hdLayout) return;

	state._hdWideLayout = currentF34LayoutClass() == CalypsoLayoutClass::Wide;
	const CalypsoF34ErrorLayout layout = calypsoF34ErrorLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);

	const Uint8 themeColor = state._txtMessage->getColor();

	// Badge: a real beveled panel with a bitmap fallback (not an invisible
	// placeholder), so a physical-unavailable frame still shows the badge.
	CalypsoBevelPanel* badge = new CalypsoBevelPanel();
	badge->setTheme(themeColor, themeColor);
	state._hdIconPanel = badge;
	state._hdIcon = new Text(1, 1, 0, 0);
	state._hdWarning = new Text(1, 1, 0, 0);
	state.add(state._hdIconPanel);
	state.add(state._hdIcon);
	state.add(state._hdWarning);

	CalypsoErrorPopupUi::applyRects(state, layout);

	state._hdIcon->setSmall();
	state._hdIcon->setColor(themeColor);
	state._hdIcon->setAlign(ALIGN_CENTER);
	state._hdIcon->setVerticalAlign(ALIGN_MIDDLE);
	state._hdIcon->setText("!");

	state._hdWarning->setSmall();
	state._hdWarning->setColor(themeColor);
	state._hdWarning->setAlign(ALIGN_LEFT);
	state._hdWarning->setVerticalAlign(ALIGN_MIDDLE);
	state._hdWarning->setWordWrap(true);
	state._hdWarning->setText(state.tr("STR_CAL_ERROR_OPERATIONAL_WARNING"));

	// Show the whole message (remediation B1: no punctuation split -- that broke
	// abbreviations / non-ASCII / space-less messages). Word-wrapped in its box.
	state._txtMessage->setAlign(ALIGN_LEFT);
	state._txtMessage->setVerticalAlign(ALIGN_TOP);
	state._txtMessage->setWordWrap(true);

	// Fit/center every design-space rect into the engine's actual logical canvas.
	state.enableUiScaling(layout.designWidth, layout.designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);

	// Create + register the snapshot-only adapter (driven at the pre-blit
	// boundary; no feeder Surface, no _surfaces reordering).
	CalypsoErrorPopupUi* adapter = new CalypsoErrorPopupUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoErrorPopupUi::resize(ErrorMessageState& state)
{
	if (!state._hdLayout) return false;

	// Recompute the Compact/Wide layout class (remediation B5/#17): a resize that
	// crosses the threshold must re-apply the matching design rects, not stay on
	// the class captured at create.
	const bool wide = currentF34LayoutClass() == CalypsoLayoutClass::Wide;
	if (wide != state._hdWideLayout)
	{
		state._hdWideLayout = wide;
		const CalypsoF34ErrorLayout layout = calypsoF34ErrorLayout(
			wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
		CalypsoErrorPopupUi::applyRects(state, layout);
		// Re-snapshot against the new design canvas -- enableUiScaling is one-shot
		// and would no-op here, replaying the stale class (external review #3).
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
