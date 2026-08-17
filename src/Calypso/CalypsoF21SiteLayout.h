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
 * F21 (Calypso): the approved BuildNewBaseState site-selection layout
 * (top command strip + bottom placement card over the live globe). Geometry
 * comes from the canonical contract (Contracts/f21-site.json ->
 * Generated/CalypsoF21Site.generated.h) -- the SAME source the DOM harness
 * consumes. Pure and natively testable.
 */
#include "CalypsoF21LayoutBase.h"
#include "Generated/CalypsoF21Site.generated.h"

namespace OpenXcom
{
namespace Calypso
{

static_assert(std::string_view(CalypsoF21SiteGen::kContractVersion) ==
		std::string_view(CalypsoHdThemeGen::kContractVersion),
	"F21 site and theme generated contracts carry different versions; regenerate");

/// The complete F21.Site selection layout. `window` is the full design canvas
/// (the strip and the card claim logical widgets; the globe stays logical).
struct CalypsoF21SiteLayout
{
	int designWidth = 0;
	int designHeight = 0;
	CalypsoF21Rect window;   ///< full-canvas root
	CalypsoF21Rect banner;   ///< top command strip panel
	CalypsoF21Rect title;    ///< "Select new base site" heading
	CalypsoF21Rect slot;     ///< base-slot mono readout ("Base 5 of 8")
	CalypsoF21Rect funds;    ///< current funds readout
	CalypsoF21Rect cost;     ///< region base cost readout
	CalypsoF21Rect card;     ///< bottom placement card panel
	CalypsoF21Rect coords;   ///< candidate coordinates readout
	CalypsoF21Rect region;   ///< candidate region readout
	CalypsoF21Rect legality; ///< legal-site status readout
	CalypsoF21Rect preview;  ///< transaction preview copy (wrapped)
	CalypsoF21Rect cancel;   ///< cancel action (additional bases only)
};

/// Build the F21.Site layout for the given class; zeroed when no entry.
inline CalypsoF21SiteLayout calypsoF21SiteLayout(CalypsoLayoutClass cls)
{
	CalypsoF21SiteLayout l;
	const bool wide = cls == CalypsoLayoutClass::Wide;
	const CalypsoF21SiteGen::CalypsoF21SiteGenLayout* g = CalypsoF21SiteGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return l;

	l.designWidth = g->designWidth;
	l.designHeight = g->designHeight;
	l.window   = { g->window.x,   g->window.y,   g->window.w,   g->window.h };
	l.banner   = { g->banner.x,   g->banner.y,   g->banner.w,   g->banner.h };
	l.title    = { g->title.x,    g->title.y,    g->title.w,    g->title.h };
	l.slot     = { g->slot.x,     g->slot.y,     g->slot.w,     g->slot.h };
	l.funds    = { g->funds.x,    g->funds.y,    g->funds.w,    g->funds.h };
	l.cost     = { g->cost.x,     g->cost.y,     g->cost.w,     g->cost.h };
	l.card     = { g->card.x,     g->card.y,     g->card.w,     g->card.h };
	l.coords   = { g->coords.x,   g->coords.y,   g->coords.w,   g->coords.h };
	l.region   = { g->region.x,   g->region.y,   g->region.w,   g->region.h };
	l.legality = { g->legality.x, g->legality.y, g->legality.w, g->legality.h };
	l.preview  = { g->preview.x,  g->preview.y,  g->preview.w,  g->preview.h };
	l.cancel   = { g->cancel.x,   g->cancel.y,   g->cancel.w,   g->cancel.h };
	return l;
}

} // namespace Calypso
} // namespace OpenXcom
