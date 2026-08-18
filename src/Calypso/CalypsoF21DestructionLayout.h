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
 * F21 (Calypso): the approved BaseDestroyedState review layout (partial
 * facility loss / full destruction). Geometry comes from the canonical
 * contract (Contracts/f21-destruction.json ->
 * Generated/CalypsoF21Destruction.generated.h). Pure and natively testable.
 */
#include "CalypsoF21LayoutBase.h"
#include "Generated/CalypsoF21Destruction.generated.h"

namespace OpenXcom
{
namespace Calypso
{

static_assert(std::string_view(CalypsoF21DestructionGen::kContractVersion) ==
		std::string_view(CalypsoHdThemeGen::kContractVersion),
	"F21 destruction and theme generated contracts carry different versions; regenerate");

/// The complete F21.Destruction layout.
struct CalypsoF21DestructionLayout
{
	int designWidth = 0;
	int designHeight = 0;
	CalypsoF21Rect window;      ///< review dialog panel
	CalypsoF21Rect status;      ///< protocol strip (F33 command-card language)
	CalypsoF21Rect glyph;       ///< amber caution triangle beside the title
	CalypsoF21Rect title;       ///< heading
	CalypsoF21Rect subtitle;    ///< partial/full variant subtitle
	CalypsoF21Rect list;        ///< destroyed-facility list (partial variant)
	CalypsoF21Rect warning;     ///< permanent-loss warning band (full variant)
	CalypsoF21Rect footer;      ///< separated action band + dot field
	CalypsoF21Rect acknowledge; ///< acknowledge action
};

/// Build the F21.Destruction layout for the given class; zeroed when no entry.
inline CalypsoF21DestructionLayout calypsoF21DestructionLayout(CalypsoLayoutClass cls)
{
	CalypsoF21DestructionLayout l;
	const bool wide = cls == CalypsoLayoutClass::Wide;
	const CalypsoF21DestructionGen::CalypsoF21DestructionGenLayout* g =
		CalypsoF21DestructionGen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return l;

	l.designWidth = g->designWidth;
	l.designHeight = g->designHeight;
	l.window      = { g->window.x,      g->window.y,      g->window.w,      g->window.h };
	l.status      = { g->status.x,      g->status.y,      g->status.w,      g->status.h };
	l.glyph       = { g->glyph.x,       g->glyph.y,       g->glyph.w,       g->glyph.h };
	l.title       = { g->title.x,       g->title.y,       g->title.w,       g->title.h };
	l.subtitle    = { g->subtitle.x,    g->subtitle.y,    g->subtitle.w,    g->subtitle.h };
	l.list        = { g->list.x,        g->list.y,        g->list.w,        g->list.h };
	l.warning     = { g->warning.x,     g->warning.y,     g->warning.w,     g->warning.h };
	l.footer      = { g->footer.x,      g->footer.y,      g->footer.w,      g->footer.h };
	l.acknowledge = { g->acknowledge.x, g->acknowledge.y, g->acknowledge.w, g->acknowledge.h };
	return l;
}

/// Apply the harness-only side-by-side translation to every component.
inline void calypsoF21DestructionApplyHarnessShift(
	CalypsoF21DestructionLayout& layout, bool sideBySide)
{
	calypsoF21ApplyHarnessShift(
		{ &layout.window, &layout.status, &layout.glyph, &layout.title, &layout.subtitle,
		  &layout.list, &layout.warning, &layout.footer, &layout.acknowledge },
		sideBySide && layout.designWidth == 1280, layout.window.x);
}

} // namespace Calypso
} // namespace OpenXcom
