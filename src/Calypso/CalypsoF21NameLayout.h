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
 * F21 (Calypso): the approved BaseNameState dialog layout (first-base
 * naming). Geometry comes from the canonical contract
 * (Contracts/f21-name.json -> Generated/CalypsoF21Name.generated.h).
 * Pure and natively testable.
 */
#include "CalypsoF21LayoutBase.h"
#include "Generated/CalypsoF21Name.generated.h"

namespace OpenXcom
{
namespace Calypso
{

static_assert(std::string_view(CalypsoF21NameGen::kContractVersion) ==
		std::string_view(CalypsoHdThemeGen::kContractVersion),
	"F21 name and theme generated contracts carry different versions; regenerate");

/// The complete F21.Name layout.
struct CalypsoF21NameLayout
{
	int designWidth = 0;
	int designHeight = 0;
	CalypsoF21Rect window;   ///< naming dialog panel
	CalypsoF21Rect status;   ///< protocol strip (F33 command-card language)
	CalypsoF21Rect title;    ///< "Base name" heading
	CalypsoF21Rect inputFrame; ///< visible inset material for the native edit
	CalypsoF21Rect nameEdit; ///< name edit field
	CalypsoF21Rect hint;     ///< staged-name hint copy
	CalypsoF21Rect footer;   ///< separated action band + dot field
	CalypsoF21Rect ok;       ///< confirm action (hidden until non-empty name)
};

/// Build the F21.Name layout for the given class; zeroed when no entry.
inline CalypsoF21NameLayout calypsoF21NameLayout(CalypsoLayoutClass cls)
{
	CalypsoF21NameLayout l;
	const bool wide = cls == CalypsoLayoutClass::Wide;
	const CalypsoF21NameGen::CalypsoF21NameGenLayout* g =
		CalypsoF21NameGen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return l;

	l.designWidth = g->designWidth;
	l.designHeight = g->designHeight;
	l.window   = { g->window.x,   g->window.y,   g->window.w,   g->window.h };
	l.status   = { g->status.x,   g->status.y,   g->status.w,   g->status.h };
	l.title    = { g->title.x,    g->title.y,    g->title.w,    g->title.h };
	l.inputFrame = { g->inputFrame.x, g->inputFrame.y, g->inputFrame.w, g->inputFrame.h };
	l.nameEdit = { g->nameEdit.x, g->nameEdit.y, g->nameEdit.w, g->nameEdit.h };
	l.hint     = { g->hint.x,     g->hint.y,     g->hint.w,     g->hint.h };
	l.footer   = { g->footer.x,   g->footer.y,   g->footer.w,   g->footer.h };
	l.ok       = { g->ok.x,       g->ok.y,       g->ok.w,       g->ok.h };
	return l;
}

/// Apply the harness-only side-by-side translation to every component.
inline void calypsoF21NameApplyHarnessShift(CalypsoF21NameLayout& layout, bool sideBySide)
{
	calypsoF21ApplyHarnessShift(
		{ &layout.window, &layout.status, &layout.title, &layout.inputFrame, &layout.nameEdit, &layout.hint, &layout.footer, &layout.ok },
		sideBySide && layout.designWidth == 1280, layout.window.x);
}

} // namespace Calypso
} // namespace OpenXcom
