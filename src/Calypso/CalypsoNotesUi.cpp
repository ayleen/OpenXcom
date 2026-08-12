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
 * Phase 46.2-HD (Calypso) -- see CalypsoNotesUi.h (thin physical-overlay
 * adapter over the already-HD NotesState).
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoNotesUi.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include <SDL.h>

#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Interface/ToggleTextButton.h"
#include "../Interface/Window.h"
#include "../Menu/NotesState.h"
#include "../Mod/Mod.h"

#include "CalypsoHdFontSource.h"
#include "CalypsoHdTextRasterKey.h"
#include "CalypsoHdUiModel.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoUiMetrics.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

// Fixed HD theme (0xRRGGBBAA via calypsoRgba) -- the F34 family palette shared
// with the Error/Statistics adapters.
constexpr std::uint32_t kWindowBorderRgba  = calypsoRgba(0x74, 0xff, 0xb0);
constexpr std::uint32_t kWindowFillRgba    = calypsoRgba(0x10, 0x2a, 0x24);
constexpr std::uint32_t kButtonBorderRgba  = calypsoRgba(0x74, 0xff, 0xb0);
constexpr std::uint32_t kButtonFillRgba    = calypsoRgba(0x16, 0x4c, 0x3d);
constexpr std::uint32_t kGoldTextRgba      = calypsoRgba(0xff, 0xc1, 0x4d); // title
constexpr std::uint32_t kNearWhiteTextRgba = calypsoRgba(0xe8, 0xff, 0xf5); // body/labels
constexpr std::uint32_t kPaleGreenTextRgba = calypsoRgba(0xcf, 0xe9, 0xe0); // status

enum F34Role : std::uint32_t
{
	ROLE_WINDOW = 1, ROLE_TITLE = 2, ROLE_STATUS = 3,
	ROLE_ORIGIN_GEO = 4, ROLE_ORIGIN_BATTLE = 5,
	ROLE_BTN_SAVE = 6, ROLE_BTN_CANCEL = 7, ROLE_BTN_NEW = 8,
	ROLE_BTN_DELETE = 9, ROLE_BTN_KEEP = 10, ROLE_ROW = 11
};
constexpr std::uint32_t kF34FamilyId = 34;
constexpr std::uint32_t kChromeSubgroup = 1u;
constexpr std::uint32_t kRowsSubgroup = 2u;

CalypsoLayoutClass currentF34LayoutClass()
{
	return calypsoClassifySafeArea(Options::baseXResolution, Options::baseYResolution);
}

CalypsoLogicalRect widgetRect(const Surface* s)
{
	return { s->getX(), s->getY(), s->getWidth(), s->getHeight() };
}

} // namespace

CalypsoNotesUi::~CalypsoNotesUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoNotesUi::topState() const
{
	return _state;
}

void CalypsoNotesUi::collect(CalypsoHdFrameBuilder& builder) const
{
	if (!_state || !_state->_hdLayout || !_state->_game) return;
	if (_state->_window && !_state->_window->isPopupDone()) return; // let popup play

	// While the inline editor is open, render the WHOLE screen logically (take no
	// claims). The live TextEdit sits in the logical composite; any opaque HD
	// panel drawn afterwards would cover it (Codex Notes #1). Editing is
	// transient, so a logical frame there is a clean, correct fallback -- HD
	// resumes the moment the editor closes.
	if (_state->_edtNote && _state->_edtNote->getVisible()) return;

	Mod* mod = _state->_game->getMod();
	CalypsoTtfSourceDescriptor heading;
	CalypsoTtfSourceDescriptor body;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_SAIRA_700", heading)) return;
	if (!calypsoHdResolveFontDescriptor(mod, "FONT_F34_MONO", body)) return;

	const CalypsoHdPresentationMetrics& m = CalypsoHdUiOverlay::instance().frozenMetrics();
	const double sx = m.scaleX, sy = m.scaleY;
	const std::uint64_t inst = reinterpret_cast<std::uintptr_t>(_state);
	const int border = calypsoBorderFor(currentF34LayoutClass());

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
		addPanel(inner, fcol, nullptr, role);
	};

	auto addText = [&](Surface* widget, const CalypsoTtfSourceDescriptor& font,
		const std::string& text, std::uint32_t color,
		CalypsoHdHAlign hA, CalypsoHdVAlign vA, int linesHint, std::uint32_t role)
	{
		if (!widget || text.empty()) return;
		const CalypsoLogicalRect r = widgetRect(widget);
		if (r.w <= 0 || r.h <= 0) return;
		const int hint = linesHint > 0 ? linesHint : 1;
		const int physicalPixelHeight = std::max(1,
			(int)calypsoHdRoundToInt((double)r.h / hint * sy));
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
		it.claim = { kF34FamilyId, role, inst, kChromeSubgroup, (std::uint32_t)ord };
		it.order = { 0, 0, kF34FamilyId, inst, 0, kChromeSubgroup, ord, role };
		builder.add(it);
		++ord;
	};

	// A visible action button: bevel (claims the button) + its label. Hidden
	// buttons (Delete/Keep outside the confirm flow) are skipped.
	auto addButton = [&](TextButton* btn, std::uint32_t role)
	{
		if (!btn || !btn->getVisible()) return;
		addBevel(btn, kButtonBorderRgba, kButtonFillRgba, role);
		addText(btn, body, btn->getText(), kNearWhiteTextRgba,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, role);
	};

	// ---- Chrome subgroup: window + framing text + visible buttons.
	addBevel(_state->_window, kWindowBorderRgba, kWindowFillRgba, ROLE_WINDOW);
	addText(_state->_txtTitle, heading,
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string(),
		kGoldTextRgba, CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, ROLE_TITLE);
	addText(_state->_txtDelete, body,
		_state->_txtDelete ? _state->_txtDelete->getText() : std::string(),
		kPaleGreenTextRgba, CalypsoHdHAlign::Left, CalypsoHdVAlign::Top, 2, ROLE_STATUS);
	addText(_state->_txtOriginGeo, body,
		_state->_txtOriginGeo ? _state->_txtOriginGeo->getText() : std::string(),
		kNearWhiteTextRgba, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_ORIGIN_GEO);
	addText(_state->_txtOriginBattle, body,
		_state->_txtOriginBattle ? _state->_txtOriginBattle->getText() : std::string(),
		kNearWhiteTextRgba, CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, ROLE_ORIGIN_BATTLE);
	addButton(_state->_btnSave, ROLE_BTN_SAVE);
	addButton(_state->_btnCancel, ROLE_BTN_CANCEL);
	addButton(_state->_btnNew, ROLE_BTN_NEW);
	addButton(_state->_btnDelete, ROLE_BTN_DELETE);
	addButton(_state->_btnKeep, ROLE_BTN_KEEP);

	// ---- Rows subgroup: the visible TextList rows, physically. The list stays
	// interactive (unclaimed input); the FIRST emitted row item claims it so its
	// logical rows are suppressed. BOTH columns are emitted -- column 0 (the note
	// preview) and column 1 (the selection/action marker `>`/`*`/`...`) -- so the
	// keyboard selection + row-action affordance stay visible (Codex Notes #2).
	builder.beginSubgroup();
	TextList* list = _state->_lstNotes;
	if (list && list->getTexts() > 0)
	{
		const std::size_t scroll = list->getScroll();
		const std::size_t visible = list->getVisibleRows();
		const std::size_t total = list->getTexts();
		const std::size_t lastVisible = std::min(total, scroll + visible);

		int rowStride = 0;
		if (scroll + 1 < total)
			rowStride = list->getRowY((std::size_t)(scroll + 1)) - list->getRowY(scroll);
		if (rowStride <= 0) rowStride = std::max(1, list->getTextHeight(scroll));

		const int col0X = list->getColumnX(0);
		const int col1X = list->getColumnX(1);
		const int col0W = std::max(0, col1X - col0X);
		const int listRight = list->getX() + list->getWidth();
		const int col1W = std::max(0, listRight - col1X);
		const int physRowH = std::max(1, (int)calypsoHdRoundToInt((double)rowStride * sy));

		bool listClaimAttached = false;
		int rowOrd = 0;
		auto addCell = [&](std::size_t row, int colIndex, int cellX, int cellW,
			CalypsoHdHAlign hA)
		{
			if (cellW <= 0) return;
			const std::string cell = list->getCellText(row, colIndex);
			if (cell.empty()) return;

			CalypsoHdTextRasterKey key;
			key.source = body;
			key.physicalPixelHeight = physRowH;
			key.text = cell;
			key.wrapWidth = 0;
			key.colorRgba = kNearWhiteTextRgba;
			key.direction = CalypsoTextDirection::LTR;

			CalypsoHdItem it;
			it.kind = CalypsoHdItemKind::Text;
			it.rect = { cellX, list->getRowY(row), cellW, rowStride };
			it.colorRgba = kNearWhiteTextRgba;
			it.rasterKey = key;
			it.hAlign = hA;
			it.vAlign = CalypsoHdVAlign::Middle;
			it.widget = listClaimAttached ? nullptr : (const void*)list;
			listClaimAttached = true;
			// Distinct claim visualId + order per (row, column) so no two items
			// collide on the ordering tuple.
			const std::uint32_t vid = (std::uint32_t)row * 2u + (std::uint32_t)colIndex;
			it.claim = { kF34FamilyId, ROLE_ROW, inst, kRowsSubgroup, vid };
			it.order = { 0, 0, kF34FamilyId, inst, 1, kRowsSubgroup, rowOrd, vid };
			builder.add(it);
			++rowOrd;
		};

		for (std::size_t row = scroll; row < lastVisible; ++row)
		{
			addCell(row, 0, col0X, col0W, CalypsoHdHAlign::Left);   // preview
			addCell(row, 1, col1X, col1W, CalypsoHdHAlign::Center); // marker
		}
	}
}

void CalypsoNotesUi::configure(NotesState& state)
{
	if (!state._hdLayout) return; // gate already set by NotesState's HD setup
	CalypsoNotesUi* adapter = new CalypsoNotesUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
