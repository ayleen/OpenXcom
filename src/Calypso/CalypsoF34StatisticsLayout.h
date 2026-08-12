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
 * Phase 46.2-HD (Calypso) -- the approved F34.Statistics layout, ported from
 * the standalone `phase-46-hd-ui-pilots` engine checkout (its
 * CalypsoCommonRecords.h :: calypsoF34StatisticsLayout) so CalypsoStatisticsUi
 * can drive the shared HD UI overlay queue without depending on that branch's
 * older F34PhysicalTextOverlay capture path. The values below are the pilot's
 * verbatim design-space rectangles for the Compact (740x360) and Wide
 * (960x540) canvases; CalypsoStatisticsUi maps them into the engine's current
 * logical grid before submitting to the HD UI overlay.
 *
 * Pure, dependency-free data (CalypsoUiMetrics.h for CalypsoLayoutClass and
 * CalypsoF34ErrorLayout.h for the shared CalypsoF34Rect), matching the
 * established Calypso pure-helper convention -- NOT wrapped in
 * #ifdef __EMSCRIPTEN__, so it stays natively testable even though its only
 * caller today is Emscripten-only.
 */
#include "CalypsoUiMetrics.h"
#include "CalypsoF34ErrorLayout.h" // shared CalypsoF34Rect definition

namespace OpenXcom
{
namespace Calypso
{

/// The complete F34.Statistics layout: one design canvas (Compact 740x360 or
/// Wide 960x540) plus every widget rectangle within it, plus the two-column
/// list metrics (label/value column widths and the per-row height). All
/// rectangles are in that canvas's design-space px; CalypsoStatisticsUi maps
/// them into the engine's current logical grid before submitting to the HD UI
/// overlay.
///
/// NOTE: a small number of the flavour-label rects ported from the pilot
/// (returnRole / returnDetail / scrollHint / footerStatus) are reserved for
/// future use; the initial overlay port instantiates the structural subset
/// (window + four bevel panels + title + list + OK + scroll buttons).
struct CalypsoF34StatisticsLayout
{
	int designWidth = 0;
	int designHeight = 0;
	CalypsoF34Rect window;         ///< outer popup panel fill (bevel)
	CalypsoF34Rect headerPanel;    ///< top header band (bevel)
	CalypsoF34Rect listPanel;      ///< list backdrop (bevel)
	CalypsoF34Rect returnPanel;    ///< return/scroll rail (bevel)
	CalypsoF34Rect footerPanel;    ///< bottom footer band (bevel)
	CalypsoF34Rect title;          ///< outcome + date heading (existing _txtTitle)
	CalypsoF34Rect recordLabel;    ///< small section caption (reserved)
	CalypsoF34Rect outcome;        ///< outcome caption (reserved)
	CalypsoF34Rect list;           ///< the stats TextList (interactive, unclaimed)
	CalypsoF34Rect acknowledge;    ///< OK button (interactive, unclaimed)
	CalypsoF34Rect scrollUp;       ///< manual scroll-up button (interactive, unclaimed)
	CalypsoF34Rect scrollDown;     ///< manual scroll-down button (interactive, unclaimed)
	CalypsoF34Rect returnRole;     ///< return role label (reserved)
	CalypsoF34Rect returnDetail;   ///< return detail label (reserved)
	CalypsoF34Rect scrollHint;     ///< scroll hint label (reserved)
	CalypsoF34Rect footerStatus;   ///< footer status label (reserved)
	int labelColumnWidth = 0;      ///< TextList column 0 (label) design width
	int valueColumnWidth = 0;      ///< TextList column 1 (value) design width
	int rowHeight = 0;             ///< TextList minimum row design height
};

/// The approved Compact (740x360) and Wide (960x540) F34.Statistics packages.
/// Ported verbatim from the pilot's `calypsoF34StatisticsLayout`
/// (CalypsoCommonRecords.h).
inline CalypsoF34StatisticsLayout calypsoF34StatisticsLayout(CalypsoLayoutClass layoutClass)
{
	if (layoutClass == CalypsoLayoutClass::Wide)
		return {960, 540, {12, 12, 936, 516}, {24, 24, 912, 64},
			{24, 100, 680, 306}, {716, 100, 220, 306}, {24, 420, 912, 96},
			{42, 32, 490, 48}, {42, 112, 450, 24}, {602, 32, 310, 48},
			{42, 144, 588, 246}, {748, 438, 168, 60}, {644, 206, 60, 60},
			{644, 274, 60, 60}, {734, 126, 184, 42}, {734, 176, 184, 88},
			{734, 320, 184, 44}, {42, 442, 674, 48}, 416, 160, 58};
	return {740, 360, {8, 8, 724, 344}, {16, 16, 708, 44},
		{16, 72, 532, 168}, {560, 72, 164, 168}, {16, 252, 708, 92},
		{28, 20, 372, 34}, {28, 78, 276, 18}, {414, 20, 294, 34},
		{28, 104, 462, 124}, {598, 284, 118, 44}, {504, 132, 44, 44},
		{504, 180, 44, 44}, {572, 84, 140, 28}, {572, 116, 140, 62},
		{572, 186, 140, 30}, {28, 268, 516, 48}, 294, 156, 44};
}

} // namespace Calypso
} // namespace OpenXcom
