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
 * (top command strip + generated bottom content block over the live globe).
 * The strip/action geometry comes from Contracts/f21-site.json; the block
 * comes from Contracts/f21-site-details.json. The DOM harness consumes the
 * same generated sources. Pure and natively testable.
 */
#include "CalypsoF21LayoutBase.h"
#include "Generated/CalypsoF21Site.generated.h"
#include "Generated/CalypsoF21SiteDetails.generated.h"

namespace OpenXcom
{
namespace Calypso
{

static_assert(std::string_view(CalypsoF21SiteGen::kContractVersion) ==
		std::string_view(CalypsoHdThemeGen::kContractVersion),
	"F21 site and theme generated contracts carry different versions; regenerate");
static_assert(std::string_view(CalypsoF21SiteDetailsGen::kContractVersion) ==
		std::string_view(CalypsoHdThemeGen::kContractVersion),
	"F21 site details and theme generated contracts carry different versions; regenerate");
static_assert(!CalypsoF21SiteDetailsGen::kLegacyFallback,
	"F21 generated content blocks must never fall back to the vanilla UI");
static_assert(CalypsoF21SiteDetailsGen::kFitFailureException,
	"F21 generated content blocks must fail fast when text does not fit");

/// The complete F21.Site selection layout. `window` is the full design canvas
/// (the strip and the card claim logical widgets; the globe stays logical).
struct CalypsoF21SiteLayout
{
	int designWidth = 0;
	int designHeight = 0;
	CalypsoF21Rect window;   ///< full-canvas root
	CalypsoF21Rect banner;   ///< top command strip panel
	CalypsoF21Rect status;   ///< protocol/status band within the strip
	CalypsoF21Rect title;    ///< "Select new base site" heading
	CalypsoF21Rect slot;     ///< base-slot mono readout ("Base 5 of 8")
	CalypsoF21Rect funds;    ///< current funds readout
	CalypsoF21Rect cost;     ///< region base cost readout
	CalypsoF21Rect card;     ///< generated bottom content-block panel
	CalypsoF21Rect rowDivider; ///< generated divider between the two rows
	CalypsoF21Rect columnDivider; ///< generated divider between the two columns
	CalypsoF21Rect coords;   ///< candidate coordinates readout
	CalypsoF21Rect region;   ///< candidate region readout
	CalypsoF21Rect legality; ///< legal-site status readout
	CalypsoF21Rect preview;  ///< transaction preview copy (wrapped)
	CalypsoF21Rect cancel;   ///< cancel action (additional bases only)
};

inline CalypsoF21Rect calypsoF21SiteDetailsRect(
	const CalypsoF21SiteDetailsGen::CalypsoF21SiteDetailsGenRect& rect,
	int offsetX, int offsetY)
{
	return { rect.x + offsetX, rect.y + offsetY, rect.w, rect.h };
}

/// Build the F21.Site layout for the given class; zeroed when no entry.
inline CalypsoF21SiteLayout calypsoF21SiteLayout(CalypsoLayoutClass cls)
{
	CalypsoF21SiteLayout l;
	const bool wide = cls == CalypsoLayoutClass::Wide;
	const CalypsoF21SiteGen::CalypsoF21SiteGenLayout* g = CalypsoF21SiteGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!g) return l;
	const CalypsoF21SiteDetailsGen::CalypsoF21SiteDetailsGenLayout& details =
		CalypsoF21SiteDetailsGen::kLayouts[wide ? 0 : 1];
	const int detailsX = wide ? 28 : 10;
	const int detailsBottomInset = wide ? 8 : g->designHeight - g->cancel.y;
	const int detailsY = g->designHeight - details.designHeight - detailsBottomInset;

	l.designWidth = g->designWidth;
	l.designHeight = g->designHeight;
	l.window   = { g->window.x,   g->window.y,   g->window.w,   g->window.h };
	l.banner   = { g->banner.x,   g->banner.y,   g->banner.w,   g->banner.h };
	l.status   = { g->status.x,   g->status.y,   g->status.w,   g->status.h };
	l.title    = { g->title.x,    g->title.y,    g->title.w,    g->title.h };
	l.slot     = { g->slot.x,     g->slot.y,     g->slot.w,     g->slot.h };
	l.funds    = { g->funds.x,    g->funds.y,    g->funds.w,    g->funds.h };
	l.cost     = { g->cost.x,     g->cost.y,     g->cost.w,     g->cost.h };
	l.card = calypsoF21SiteDetailsRect(details.window, detailsX, detailsY);
	l.rowDivider = calypsoF21SiteDetailsRect(details.rowDivider1, detailsX, detailsY);
	l.columnDivider = calypsoF21SiteDetailsRect(details.columnDivider1, detailsX, detailsY);
	l.coords = calypsoF21SiteDetailsRect(details.cellR1C1, detailsX, detailsY);
	l.region = calypsoF21SiteDetailsRect(details.cellR1C2, detailsX, detailsY);
	l.legality = calypsoF21SiteDetailsRect(details.cellR2C1, detailsX, detailsY);
	l.preview = calypsoF21SiteDetailsRect(details.cellR2C2, detailsX, detailsY);
	l.cancel   = { g->cancel.x,   g->cancel.y,   g->cancel.w,   g->cancel.h };
	return l;
}

} // namespace Calypso
} // namespace OpenXcom
