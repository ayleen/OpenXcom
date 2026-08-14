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
 * Phase 46.2-HD (Calypso) -- see CalypsoStatisticsUi.h.
 *
 * Structure mirrors CalypsoErrorPopupUi.cpp (the already-done F34.ErrorPopup
 * adapter): a snapshot-only CalypsoHdFamilyAdapter subclass, `new`ed +
 * registered in configure(), deleted in the state destructor; a collect() that
 * reads a const snapshot of widgets and emits panels + text with claim ids +
 * order keys + alignment; metrics-derived font size (per-line box height *
 * frozenMetrics.scaleY); colours packed via calypsoRgba (0xRRGGBBAA); and a
 * CalypsoBevelPanel-style beveled panel with a real bitmap fallback for the
 * decorative panels. The interactive TextList (`_lstStats`) and the OK/scroll
 * buttons stay UNCLAIMED so input keeps flowing to their logical widgets; only
 * the decorative panels + the title text + the visible list rows are emitted
 * physically.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoStatisticsUi.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <SDL.h>
#include <SDL_ttf.h>

#include "../Engine/Action.h"
#include "../Engine/FileMap.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Engine/Unicode.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Menu/StatisticsState.h"
#include "../Mod/Mod.h"

#include "CalypsoBevelPanel.h"
#include "CalypsoF34StatisticsLayout.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdTextRasterKey.h"
#include "CalypsoHdUiModel.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoUiMetrics.h"
#include "CalypsoViewportRuntime.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

// Fixed HD theme for the physical replacement (0xRRGGBBAA, packed via
// calypsoRgba so the byte order matches the rasteriser's unpack -- same values
// as the sibling F34.ErrorPopup adapter, since both belong to the F34 family).
constexpr std::uint32_t kWindowBorderRgba = calypsoRgba(0x74, 0xff, 0xb0);
constexpr std::uint32_t kWindowFillRgba   = calypsoRgba(0x10, 0x2a, 0x24);
constexpr std::uint32_t kPanelBorderRgba  = calypsoRgba(0x3a, 0x80, 0x66);
constexpr std::uint32_t kPanelFillRgba    = calypsoRgba(0x0e, 0x22, 0x1b);
constexpr std::uint32_t kGoldTextRgba     = calypsoRgba(0xff, 0xc1, 0x4d); // title + value column
constexpr std::uint32_t kNearWhiteTextRgba = calypsoRgba(0xe8, 0xff, 0xf5); // label column

// F34 claim roles (stableId) within the statistics subgroups.
enum F34Role : std::uint32_t
{
	ROLE_WINDOW = 1, ROLE_HEADER = 2, ROLE_LISTPANEL = 3, ROLE_RETURN = 4,
	ROLE_FOOTER = 5, ROLE_TITLE = 6,
	ROLE_ROW_LABEL = 7, ROLE_ROW_VALUE = 8
};
constexpr std::uint32_t kF34FamilyId = 34;
constexpr std::uint32_t kChromeSubgroup = 1u;
constexpr std::uint32_t kRowsSubgroup = 2u;

CalypsoLayoutClass currentF34LayoutClass()
{
	// Classify from the USABLE safe area (after insets) -- the same rect
	// applyUiScaling fits the design canvas into -- not the raw framebuffer, which
	// would pick Wide for a viewport whose usable area is only Compact-sized
	// (external review #9).
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

/// Replace the engine's in-band TOK_NL_SMALL marker (byte 0x02) with a real
/// '\n' so the HD rasteriser (SDL_ttf wrapped render) breaks the title into the
/// same two lines the logical Text widget renders. Byte-level replace is safe
/// here because TOK_NL_SMALL is a single-byte control (Unicode.h).
std::string normaliseLineBreaks(const std::string& text)
{
	std::string out = text;
	std::replace(out.begin(), out.end(), static_cast<char>(Unicode::TOK_NL_SMALL), '\n');
	return out;
}

/// Move every surface in `panels` to just after the first surface of
/// `surfaces` (i.e. behind the window background but in front of nothing else)
/// so the decorative bevels render behind the interactive content. Mirrors the
/// pilot's moveF34PanelsBehindContent helper.
void movePanelsBehindContent(std::vector<Surface*>& surfaces,
	const std::vector<Surface*>& panels)
{
	for (Surface* panel : panels)
	{
		auto it = std::find(surfaces.begin(), surfaces.end(), panel);
		if (it == surfaces.end()) continue;
		surfaces.erase(it);
	}
	// Insert at position 1 (right after the window at index 0); if the window
	// is absent for any reason, fall back to the very front of the list.
	const std::size_t insertAt = surfaces.empty() ? 0 : 1;
	surfaces.insert(surfaces.begin() + std::min(insertAt, surfaces.size()),
		panels.begin(), panels.end());
}

} // namespace

// --- Adapter ---------------------------------------------------------------

CalypsoStatisticsUi::~CalypsoStatisticsUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoStatisticsUi::topState() const
{
	return _state;
}

void CalypsoStatisticsUi::collect(CalypsoHdFrameBuilder& builder) const
{
	if (!_state || !_state->_hdLayout || !_state->_game) return;

	// Let the logical window's scale-in animation play before taking over
	// physically (Codex #1).
	if (_state->_window && !_state->_window->isPopupDone()) return;

	Mod* mod = _state->_game->getMod();
	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor mono;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_SAIRA_700", heading)) return;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_MONO", mono)) return;

	const CalypsoHdPresentationMetrics& m = CalypsoHdUiOverlay::instance().frozenMetrics();
	const double sx = m.scaleX, sy = m.scaleY;
	const std::uint64_t inst = reinterpret_cast<std::uintptr_t>(_state);
	const int border = calypsoBorderFor(_state->_hdWideLayout
		? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);

	// ---- Chrome subgroup: window + 4 bevel panels + title text. Every item
	// carries a real widget pointer so the overlay's claim/skip path suppresses
	// the matching logical draw when this subgroup commits.
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
		it.claim = { kF34FamilyId, role, inst, kChromeSubgroup, (std::uint32_t)ord };
		it.order = { 0, 0, kF34FamilyId, inst, 0, kChromeSubgroup, ord, role };
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
		CalypsoHdHAlign hA, CalypsoHdVAlign vA, int linesHint,
		std::uint32_t role)
	{
		if (!widget || text.empty()) return;
		const CalypsoLogicalRect r = widgetRect(widget);
		if (r.w <= 0 || r.h <= 0) return;

		const int hint = linesHint > 0 ? linesHint : 1;
		// Physical font height from the frozen vertical scale (per-line box
		// height * scaleY, never a hardcoded DPR).
		const int physicalPixelHeight = std::max(1,
			(int)calypsoHdRoundToInt((double)r.h / hint * sy));
		const int targetWidthPx = std::max(1, (int)calypsoHdRoundToInt((double)r.w * sx));

		// Multi-line boxes let SDL_ttf wrap at the physical box width (correct for
		// CJK / no-space text -- Codex #5); the text keeps any author-side
		// TOK_NL_SMALL already converted to '\n'. Single-line boxes never wrap.
		const int wrapWidth = (hint > 1) ? targetWidthPx : 0;

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
		it.claim = { kF34FamilyId, role, inst, kChromeSubgroup, (std::uint32_t)ord };
		it.order = { 0, 0, kF34FamilyId, inst, 0, kChromeSubgroup, ord, role };
		builder.add(it);
		++ord;
	};

	addBevel(_state->_window, kWindowBorderRgba, kWindowFillRgba, ROLE_WINDOW);
	addBevel(_state->_hdHeaderPanel, kPanelBorderRgba, kPanelFillRgba, ROLE_HEADER);
	addBevel(_state->_hdListPanel, kPanelBorderRgba, kPanelFillRgba, ROLE_LISTPANEL);
	addBevel(_state->_hdReturnPanel, kPanelBorderRgba, kPanelFillRgba, ROLE_RETURN);
	addBevel(_state->_hdFooterPanel, kPanelBorderRgba, kPanelFillRgba, ROLE_FOOTER);

	addText(_state->_txtTitle, heading,
		normaliseLineBreaks(_state->_txtTitle ? _state->_txtTitle->getText() : std::string()),
		kGoldTextRgba, CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 2, ROLE_TITLE);

	// ---- Rows subgroup: emit each visible TextList row's two cells as Text
	// items at their live row rects. The list widget itself is NOT claimed: it
	// stays interactive (scroll / wheel / selection), and collect() re-reads it
	// every frame so scrolling updates naturally. The row items are pure
	// decoration (widget=nullptr) -- the per-row logical Text objects live
	// inside TextList and are not reachable without changes that are under
	// review elsewhere, so the HD rows overlay rather than replace.
	builder.beginSubgroup();
	TextList* list = _state->_lstStats;
	if (list && list->getTexts() > 0)
	{
		const std::size_t scroll = list->getScroll();
		const std::size_t visible = list->getVisibleRows();
		const std::size_t total = list->getTexts();
		const std::size_t lastVisible = std::min(total, scroll + visible);

		// Row pitch from the live list geometry (consecutive visible rows). This
		// already reflects minimumRowHeight + font + spacing in logical px.
		int rowStride = 0;
		if (scroll + 1 < total)
			rowStride = list->getRowY(static_cast<std::size_t>(scroll + 1)) - list->getRowY(scroll);
		if (rowStride <= 0)
			rowStride = std::max(1, list->getTextHeight(scroll));

		const int labelX = list->getColumnX(0);
		const int valueX = list->getColumnX(1);
		const int labelW = std::max(0, valueX - labelX);
		const int listRight = list->getX() + list->getWidth();
		const int valueW = std::max(0, listRight - valueX);
		const int physicalRowH = std::max(1, (int)calypsoHdRoundToInt((double)rowStride * sy));

		// Claim the TextList itself (via the first emitted row item's widget) so its
		// logical ROW TEXT is suppressed when this subgroup commits and the HD rows
		// replace (not overlay) it -- no double-draw. The selector highlight, toggle
		// arrows, and scrollbar are NOT claimed away: TextList::blit still draws them
		// so selection + scroll-position feedback survives (external review #8). If
		// no row commits (empty list / raster failure) the list is unclaimed and
		// renders its logical rows as the fallback. Scroll/wheel/keyboard input is
		// unaffected by the blit skip.
		bool listClaimAttached = false;

		int rowOrd = 0;
		for (std::size_t row = scroll; row < lastVisible; ++row)
		{
			const int rowY = list->getRowY(row);
			const std::uint32_t rowBase = static_cast<std::uint32_t>(row) * 2u;

			const std::string labelText = list->getCellText(row, 0);
			if (!labelText.empty() && labelW > 0)
			{
				CalypsoHdTextRasterKey key;
				key.source = mono;
				key.physicalPixelHeight = physicalRowH;
				key.text = labelText;
				key.colorRgba = kNearWhiteTextRgba;
				key.direction = CalypsoTextDirection::LTR;

				CalypsoHdItem it;
				it.kind = CalypsoHdItemKind::Text;
				it.rect = { labelX, rowY, labelW, rowStride };
				it.colorRgba = kNearWhiteTextRgba;
				it.rasterKey = key;
				it.hAlign = CalypsoHdHAlign::Left;
				it.vAlign = CalypsoHdVAlign::Middle;
				it.widget = listClaimAttached ? nullptr : (const void*)list;
				listClaimAttached = true;
				it.claim = { kF34FamilyId, ROLE_ROW_LABEL, inst, kRowsSubgroup, rowBase };
				it.order = { 0, 0, kF34FamilyId, inst, 1, kRowsSubgroup, rowOrd, ROLE_ROW_LABEL };
				builder.add(it);
				++rowOrd;
			}

			const std::string valueText = list->getCellText(row, 1);
			if (!valueText.empty() && valueW > 0)
			{
				CalypsoHdTextRasterKey key;
				key.source = mono;
				key.physicalPixelHeight = physicalRowH;
				key.text = valueText;
				key.colorRgba = kGoldTextRgba;
				key.direction = CalypsoTextDirection::LTR;

				CalypsoHdItem it;
				it.kind = CalypsoHdItemKind::Text;
				it.rect = { valueX, rowY, valueW, rowStride };
				it.colorRgba = kGoldTextRgba;
				it.rasterKey = key;
				it.hAlign = CalypsoHdHAlign::Right;
				it.vAlign = CalypsoHdVAlign::Middle;
				it.widget = listClaimAttached ? nullptr : (const void*)list;
				listClaimAttached = true;
				it.claim = { kF34FamilyId, ROLE_ROW_VALUE, inst, kRowsSubgroup, rowBase + 1u };
				it.order = { 0, 0, kF34FamilyId, inst, 1, kRowsSubgroup, rowOrd, ROLE_ROW_VALUE };
				builder.add(it);
				++rowOrd;
			}
		}
	}
}

void CalypsoStatisticsUi::applyRects(StatisticsState& state,
	const CalypsoF34StatisticsLayout& layout)
{
	applyRect(state._window, layout.window);
	applyRect(state._hdHeaderPanel, layout.headerPanel);
	applyRect(state._hdListPanel, layout.listPanel);
	applyRect(state._hdReturnPanel, layout.returnPanel);
	applyRect(state._hdFooterPanel, layout.footerPanel);
	applyRect(state._txtTitle, layout.title);
	applyRect(state._lstStats, layout.list);
	applyRect(state._btnOk, layout.acknowledge);
	applyRect(state._btnScrollUp, layout.scrollUp);
	applyRect(state._btnScrollDown, layout.scrollDown);
}

void CalypsoStatisticsUi::rebuildList(StatisticsState& state, std::size_t scroll)
{
	const CalypsoF34StatisticsLayout layout = calypsoF34StatisticsLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
	state._lstStats->clearList();
	// Re-base the HD scale() denominator to the F34 design list width BEFORE
	// setColumns, so the authored design-space column widths (labelColumnWidth +
	// valueColumnWidth <= list.width) are not multiplied by getWidth()/280 and do
	// not push the value column past the list edge (external review #2). After
	// this, scale() = getWidth()/list.width stays proportional to the design.
	state._lstStats->rebaseNativeSize(layout.list.width, layout.list.height);
	state._lstStats->setColumns(2, layout.labelColumnWidth, layout.valueColumnWidth);
	state._lstStats->setMinimumRowHeight(layout.rowHeight);
	state.listStats(); // repopulate rows at the new column widths / row height
	state._lstStats->scrollTo(scroll);
}

void CalypsoStatisticsUi::configure(StatisticsState& state)
{
	state._hdLayout = state._game && state._game->getMod()
		&& state._game->getMod()->isHdUiFamilyEnabled("F34");
	if (!state._hdLayout) return;

	state._hdWideLayout = currentF34LayoutClass() == CalypsoLayoutClass::Wide;
	const CalypsoF34StatisticsLayout layout = calypsoF34StatisticsLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);

	const Uint8 themeColor = state._window->getColor();

	// Four decorative beveled panels with real bitmap fallbacks. They render
	// logically unless the overlay claims them (CalypsoBevelPanel::blit).
	state._hdHeaderPanel = new CalypsoBevelPanel();
	state._hdListPanel = new CalypsoBevelPanel();
	state._hdReturnPanel = new CalypsoBevelPanel();
	state._hdFooterPanel = new CalypsoBevelPanel();
	for (Surface* panel : { state._hdHeaderPanel, state._hdListPanel,
		 state._hdReturnPanel, state._hdFooterPanel })
	{
		static_cast<CalypsoBevelPanel*>(panel)->setTheme(themeColor, themeColor);
		state.add(panel);
	}
	movePanelsBehindContent(state._surfaces,
		{ state._hdHeaderPanel, state._hdListPanel,
		  state._hdReturnPanel, state._hdFooterPanel });

	// Manual scroll buttons form their own 44px rail beside TextList's native
	// scrollbar, so mouse, touch, keyboard, and wheel scrolling stay available.
	state._btnScrollUp = new TextButton(layout.scrollUp.width, layout.scrollUp.height,
		layout.scrollUp.x, layout.scrollUp.y);
	state._btnScrollDown = new TextButton(layout.scrollDown.width, layout.scrollDown.height,
		layout.scrollDown.x, layout.scrollDown.y);
	state.add(state._btnScrollUp, "button", "endGameStatistics");
	state.add(state._btnScrollDown, "button", "endGameStatistics");
	state._btnScrollUp->setText("/\\");
	state._btnScrollDown->setText("\\/");
	state._btnScrollUp->onMouseClick((ActionHandler)&StatisticsState::btnScrollUpClick);
	state._btnScrollDown->onMouseClick((ActionHandler)&StatisticsState::btnScrollDownClick);

	// The list stays fully interactive; we only reskin its surroundings and
	// re-emit its visible rows in HD each frame. Native scrollbar stays on.
	state._lstStats->setScrolling(true, 0);
	state._lstStats->setSelectable(true);
	state._lstStats->setAlign(ALIGN_RIGHT, 1);

	CalypsoStatisticsUi::applyRects(state, layout);
	CalypsoStatisticsUi::rebuildList(state, 0);

	// Fit/center every design-space rect into the engine's actual logical canvas.
	state.enableUiScaling(layout.designWidth, layout.designHeight, 1.0f);

	// Create + register the snapshot-only adapter (driven at the pre-blit
	// boundary; no feeder Surface, no _surfaces reordering for text).
	CalypsoStatisticsUi* adapter = new CalypsoStatisticsUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoStatisticsUi::resize(StatisticsState& state)
{
	if (!state._hdLayout) return false;

	// Recompute the Compact/Wide layout class: a resize that crosses the
	// threshold must re-apply the matching design rects.
	const bool wide = currentF34LayoutClass() == CalypsoLayoutClass::Wide;
	const std::size_t scroll = state._lstStats->getScroll();
	if (wide != state._hdWideLayout)
	{
		state._hdWideLayout = wide;
		const CalypsoF34StatisticsLayout layout = calypsoF34StatisticsLayout(
			wide ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
		CalypsoStatisticsUi::applyRects(state, layout);
		CalypsoStatisticsUi::rebuildList(state, scroll);
		// Re-snapshot against the new design canvas -- enableUiScaling is one-shot
		// and would no-op here, replaying the stale class (external review #3).
		state.recaptureUiScaling(layout.designWidth, layout.designHeight, 1.0f);
	}
	else
	{
		state.applyUiScaling();
	}
	return true;
}

bool CalypsoStatisticsUi::handle(StatisticsState& state, Action* action)
{
	if (!state._hdLayout || !action || !action->getDetails()) return false;
	if (action->getDetails()->type != SDL_KEYDOWN) return false;

	// The list is the primary control on this screen; the OK/scroll buttons do
	// not bind arrows/PgUp-PgDn/Home/End, so routing those keys straight to the
	// list cannot steal an activation. (Focus-coordinator routing from the
	// pilot is intentionally not ported -- the new overlay foundation keeps the
	// adapter side snapshot-only, matching CalypsoErrorPopupUi.)
	switch (action->getDetails()->key.keysym.sym)
	{
	case SDLK_UP:       scrollUp(state); break;
	case SDLK_DOWN:     scrollDown(state); break;
	case SDLK_PAGEUP:   state._lstStats->scrollUp(false, false, state._lstStats->getVisibleRows()); break;
	case SDLK_PAGEDOWN: state._lstStats->scrollDown(false, false, state._lstStats->getVisibleRows()); break;
	case SDLK_HOME:     state._lstStats->scrollUp(true); break;
	case SDLK_END:      state._lstStats->scrollDown(true); break;
	default: return false;
	}
	action->getDetails()->type = SDL_NOEVENT;
	return true;
}

void CalypsoStatisticsUi::scrollUp(StatisticsState& state)
{
	if (state._hdLayout && state._lstStats) state._lstStats->scrollUp(false);
}

void CalypsoStatisticsUi::scrollDown(StatisticsState& state)
{
	if (state._hdLayout && state._lstStats) state._lstStats->scrollDown(false);
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
