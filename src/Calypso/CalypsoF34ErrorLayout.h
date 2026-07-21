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
 * Phase 46.2-HD.5 (Calypso) -- the approved F34.ErrorPopup layout, ported
 * from the standalone `phase-46-hd-ui-pilots` engine checkout (its
 * CalypsoCommonRecords.h) so CalypsoErrorPopupUi can drive the shared HD UI
 * overlay queue without depending on that branch's own tree of F34 records
 * (Statistics/Notes are NOT ported here -- they stay on this branch's
 * existing bitmap-Font HD layout and are out of this migration's scope).
 *
 * Pure, dependency-free data (only CalypsoUiMetrics.h for CalypsoLayoutClass),
 * matching the established Calypso pure-helper convention -- NOT wrapped in
 * #ifdef __EMSCRIPTEN__, so it stays natively testable even though its only
 * caller today is Emscripten-only.
 */
#include "CalypsoUiMetrics.h"

namespace OpenXcom
{
namespace Calypso
{

/// One design-space rectangle of the F34.ErrorPopup layout.
struct CalypsoF34Rect
{
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
};

/// The complete F34.ErrorPopup layout: one design canvas (Compact 740x360 or
/// Wide 1280x720) plus every widget rectangle within it. All rectangles are in
/// that canvas's design-space px; CalypsoErrorPopupUi maps them into the
/// engine's current logical grid before submitting to the HD UI overlay.
struct CalypsoF34ErrorLayout
{
	int designWidth = 0;
	int designHeight = 0;
	CalypsoF34Rect window;        ///< popup panel fill (submitPanel)
	CalypsoF34Rect iconPanel;      ///< icon badge panel fill (submitPanel)
	CalypsoF34Rect icon;           ///< "!" glyph (submitText)
	CalypsoF34Rect warning;        ///< "OPERATIONAL WARNING" heading (submitText)
	CalypsoF34Rect message;        ///< primary message line (submitText)
	CalypsoF34Rect messageDetail;  ///< secondary detail line (submitText)
	CalypsoF34Rect acknowledge;    ///< OK button (submitText label + submitPanel fill)
};

/// The approved Compact (740x360) and Wide (1280x720) F34.ErrorPopup packages.
/// Ported verbatim from the pilot's `calypsoF34ErrorLayout` (CalypsoCommonRecords.h).
inline CalypsoF34ErrorLayout calypsoF34ErrorLayout(CalypsoLayoutClass layoutClass)
{
	if (layoutClass == CalypsoLayoutClass::Wide)
		return {1280, 720, {250, 205, 780, 310}, {286, 271, 84, 84},
			{286, 271, 84, 84}, {400, 238, 560, 30}, {400, 274, 560, 52},
			{400, 346, 560, 56}, {820, 430, 170, 60}};
	return {740, 360, {48, 57, 644, 246}, {78, 123, 58, 58},
		{78, 123, 58, 58}, {166, 82, 454, 28}, {166, 114, 454, 40},
		{166, 166, 454, 44}, {512, 235, 140, 44}};
}

} // namespace Calypso
} // namespace OpenXcom
