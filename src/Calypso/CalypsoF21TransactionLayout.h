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
 * F21 (Calypso): the approved merged site/cost/name transaction layout
 * (ConfirmNewBaseState HD host). Geometry comes from the canonical contract
 * (Contracts/f21-transaction.json -> Generated/CalypsoF21Transaction.generated.h).
 * Pure and natively testable.
 */
#include "CalypsoF21LayoutBase.h"
#include "Generated/CalypsoF21Transaction.generated.h"

namespace OpenXcom
{
namespace Calypso
{

static_assert(std::string_view(CalypsoF21TransactionGen::kContractVersion) ==
		std::string_view(CalypsoHdThemeGen::kContractVersion),
	"F21 transaction and theme generated contracts carry different versions; regenerate");

/// The complete F21.Transaction layout.
struct CalypsoF21TransactionLayout
{
	int designWidth = 0;
	int designHeight = 0;
	CalypsoF21Rect window;   ///< transaction dialog panel
	CalypsoF21Rect status;   ///< protocol strip (F33 command-card language)
	CalypsoF21Rect glyph;    ///< amber caution triangle beside the title
	CalypsoF21Rect title;    ///< "Review new base" heading
	CalypsoF21Rect slot;     ///< base-slot mono readout ("5 / 8")
	CalypsoF21Rect coords;   ///< site coordinates readout
	CalypsoF21Rect region;   ///< site region readout
	CalypsoF21Rect cost;     ///< region cost readout
	CalypsoF21Rect after;    ///< funds-after readout
	CalypsoF21Rect nameEdit; ///< staged name edit field
	CalypsoF21Rect nameHint; ///< staged-name hint copy
	CalypsoF21Rect footer;   ///< separated action band + dot field
	CalypsoF21Rect create;   ///< primary Create Base action
	CalypsoF21Rect cancel;   ///< safe Cancel action
};

/// Build the F21.Transaction layout for the given class; zeroed when no entry.
inline CalypsoF21TransactionLayout calypsoF21TransactionLayout(CalypsoLayoutClass cls)
{
	CalypsoF21TransactionLayout l;
	const bool wide = cls == CalypsoLayoutClass::Wide;
	const CalypsoF21TransactionGen::CalypsoF21TransactionGenLayout* g =
		CalypsoF21TransactionGen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return l;

	l.designWidth = g->designWidth;
	l.designHeight = g->designHeight;
	l.window   = { g->window.x,   g->window.y,   g->window.w,   g->window.h };
	l.status   = { g->status.x,   g->status.y,   g->status.w,   g->status.h };
	l.glyph    = { g->glyph.x,    g->glyph.y,    g->glyph.w,    g->glyph.h };
	l.title    = { g->title.x,    g->title.y,    g->title.w,    g->title.h };
	l.slot     = { g->slot.x,     g->slot.y,     g->slot.w,     g->slot.h };
	l.coords   = { g->coords.x,   g->coords.y,   g->coords.w,   g->coords.h };
	l.region   = { g->region.x,   g->region.y,   g->region.w,   g->region.h };
	l.cost     = { g->cost.x,     g->cost.y,     g->cost.w,     g->cost.h };
	l.after    = { g->after.x,    g->after.y,    g->after.w,    g->after.h };
	l.nameEdit = { g->nameEdit.x, g->nameEdit.y, g->nameEdit.w, g->nameEdit.h };
	l.nameHint = { g->nameHint.x, g->nameHint.y, g->nameHint.w, g->nameHint.h };
	l.footer   = { g->footer.x,   g->footer.y,   g->footer.w,   g->footer.h };
	l.create   = { g->create.x,   g->create.y,   g->create.w,   g->create.h };
	l.cancel   = { g->cancel.x,   g->cancel.y,   g->cancel.w,   g->cancel.h };
	return l;
}

/// Apply the harness-only side-by-side translation to every component.
inline void calypsoF21TransactionApplyHarnessShift(
	CalypsoF21TransactionLayout& layout, bool sideBySide)
{
	calypsoF21ApplyHarnessShift(
		{ &layout.window, &layout.status, &layout.glyph, &layout.title, &layout.slot,
		  &layout.coords, &layout.region, &layout.cost, &layout.after,
		  &layout.nameEdit, &layout.nameHint, &layout.footer,
		  &layout.create, &layout.cancel },
		sideBySide && layout.designWidth == 1280, layout.window.x);
}

} // namespace Calypso
} // namespace OpenXcom
