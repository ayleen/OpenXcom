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
 * F33 (Calypso): the approved AbandonGameState confirmation layout, in the
 * same shape as CalypsoF34ErrorLayout (Phase 46.2-HD). One design canvas
 * (Compact 740x360 or Wide 1280x720) plus every widget rectangle within it.
 * All rectangles are in that canvas's design-space px; CalypsoAbandonPopupUi
 * maps them into the engine's current logical grid before submitting to the
 * shared HD UI overlay queue.
 *
 * Phase 46.4-F33: the geometry is NOT authored here anymore. It comes from the
 * canonical contract (src/Calypso/Contracts/f33-abandon.json ->
 * Generated/CalypsoF33Abandon.generated.h), the SAME source the DOM harness
 * consumes (F33-PARITY-004/-006). Editing geometry means editing the JSON and
 * regenerating both consumers.
 *
 * Pure, dependency-free data (only CalypsoUiMetrics.h for CalypsoLayoutClass),
 * matching the established Calypso pure-helper convention -- NOT wrapped in
 * #ifdef __EMSCRIPTEN__, so it stays natively testable.
 */
#include "CalypsoUiMetrics.h"
#include "Generated/CalypsoF33Abandon.generated.h"
#include "Generated/CalypsoHdTheme.generated.h"

#include <cstring>

// Contract-version guard (46.4-F33.2 contract rule): a consumer compiled
// against a stale generated pair is a build error, never a silent drift.
static_assert(std::strcmp(CalypsoF33AbandonGen::kContractVersion,
	CalypsoHdThemeGen::kContractVersion) == 0,
	"F33 and theme generated contracts carry different versions; regenerate");

namespace OpenXcom
{
namespace Calypso
{

/// One design-space rectangle of the F33.Abandon layout.
struct CalypsoF33Rect
{
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
};

/// The complete F33.Abandon confirmation layout.
struct CalypsoF33AbandonLayout
{
	int designWidth = 0;
	int designHeight = 0;
	CalypsoF33Rect window;    ///< confirm dialog panel fill (submitPanel)
	CalypsoF33Rect title;     ///< "ABANDON GAME?" heading (submitText)
	CalypsoF33Rect message;   ///< data-loss copy (submitText, wrapped)
	CalypsoF33Rect yes;       ///< destructive YES button (panel + label)
	CalypsoF33Rect no;        ///< safe NO button (panel + label)
};

/// Build the F33.Abandon layout for the given layout class from the canonical
/// contract. Returns a zeroed layout when the class has no contract entry.
inline CalypsoF33AbandonLayout calypsoF33AbandonLayout(CalypsoLayoutClass cls)
{
	CalypsoF33AbandonLayout l;
	const bool wide = cls == CalypsoLayoutClass::Wide;
	const CalypsoF33AbandonGen::CalypsoF33GenLayout* g = CalypsoF33AbandonGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return l;

	l.designWidth = g->designWidth;
	l.designHeight = g->designHeight;
	l.window  = { g->window.x,  g->window.y,  g->window.w,  g->window.h };
	l.title   = { g->title.x,   g->title.y,   g->title.w,   g->title.h };
	l.message = { g->message.x, g->message.y, g->message.w, g->message.h };
	l.yes     = { g->yes.x,     g->yes.y,     g->yes.w,     g->yes.h };
	l.no      = { g->no.x,      g->no.y,      g->no.w,      g->no.h };
	return l;
}

} // namespace Calypso
} // namespace OpenXcom
