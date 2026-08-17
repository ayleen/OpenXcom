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
 * F21 (Calypso): the approved BaseDefenseState instrumentation layout.
 * Geometry comes from the canonical contract (Contracts/f21-defense.json ->
 * Generated/CalypsoF21Defense.generated.h). Pure and natively testable.
 */
#include "CalypsoF21LayoutBase.h"
#include "Generated/CalypsoF21Defense.generated.h"

namespace OpenXcom
{
namespace Calypso
{

static_assert(std::string_view(CalypsoF21DefenseGen::kContractVersion) ==
		std::string_view(CalypsoHdThemeGen::kContractVersion),
	"F21 defense and theme generated contracts carry different versions; regenerate");

/// The complete F21.Defense layout.
struct CalypsoF21DefenseLayout
{
	int designWidth = 0;
	int designHeight = 0;
	CalypsoF21Rect window;   ///< defense dialog panel
	CalypsoF21Rect title;    ///< heading
	CalypsoF21Rect defenses; ///< defenses count readout
	CalypsoF21Rect ammo;     ///< ammo stores readout
	CalypsoF21Rect hitRatio; ///< hit-ratio readout
	CalypsoF21Rect phase;    ///< current-phase readout
	CalypsoF21Rect result;   ///< live result band (fire/miss/damage lines)
	CalypsoF21Rect start;    ///< Start Firing action
	CalypsoF21Rect skip;     ///< Skip to Assault action
	CalypsoF21Rect ok;       ///< End/acknowledge action (visible at BDA_END)
};

/// Build the F21.Defense layout for the given class; zeroed when no entry.
inline CalypsoF21DefenseLayout calypsoF21DefenseLayout(CalypsoLayoutClass cls)
{
	CalypsoF21DefenseLayout l;
	const bool wide = cls == CalypsoLayoutClass::Wide;
	const CalypsoF21DefenseGen::CalypsoF21DefenseGenLayout* g =
		CalypsoF21DefenseGen::layoutForDesign(wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return l;

	l.designWidth = g->designWidth;
	l.designHeight = g->designHeight;
	l.window    = { g->window.x,    g->window.y,    g->window.w,    g->window.h };
	l.title     = { g->title.x,     g->title.y,     g->title.w,     g->title.h };
	l.defenses  = { g->defenses.x,  g->defenses.y,  g->defenses.w,  g->defenses.h };
	l.ammo      = { g->ammo.x,      g->ammo.y,      g->ammo.w,      g->ammo.h };
	l.hitRatio  = { g->hitRatio.x,  g->hitRatio.y,  g->hitRatio.w,  g->hitRatio.h };
	l.phase     = { g->phase.x,     g->phase.y,     g->phase.w,     g->phase.h };
	l.result    = { g->result.x,    g->result.y,    g->result.w,    g->result.h };
	l.start     = { g->start.x,     g->start.y,     g->start.w,     g->start.h };
	l.skip      = { g->skip.x,      g->skip.y,      g->skip.w,      g->skip.h };
	l.ok        = { g->ok.x,        g->ok.y,        g->ok.w,        g->ok.h };
	return l;
}

/// Apply the harness-only side-by-side translation to every component.
inline void calypsoF21DefenseApplyHarnessShift(
	CalypsoF21DefenseLayout& layout, bool sideBySide)
{
	calypsoF21ApplyHarnessShift(
		{ &layout.window, &layout.title, &layout.defenses, &layout.ammo,
		  &layout.hitRatio, &layout.phase, &layout.result, &layout.start,
		  &layout.skip, &layout.ok },
		sideBySide && layout.designWidth == 1280, layout.window.x);
}

} // namespace Calypso
} // namespace OpenXcom
