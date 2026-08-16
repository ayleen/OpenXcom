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
 * Pure, dependency-free data (only CalypsoUiMetrics.h for CalypsoLayoutClass),
 * matching the established Calypso pure-helper convention -- NOT wrapped in
 * #ifdef __EMSCRIPTEN__, so it stays natively testable.
 */
#include "CalypsoUiMetrics.h"

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

/// Build the F33.Abandon layout for the given layout class.
inline CalypsoF33AbandonLayout calypsoF33AbandonLayout(CalypsoLayoutClass cls)
{
	CalypsoF33AbandonLayout l;
	if (cls == CalypsoLayoutClass::Wide)
	{
		// 1280x720 desktop: centered 600x212 dialog.
		l.designWidth = 1280;
		l.designHeight = 720;
		l.window  = { 340, 254, 600, 212 };
		l.title   = { 340, 266, 600, 34 };
		l.message = { 366, 312, 548, 84 };
		l.yes     = { 396, 408, 130, 46 };
		l.no      = { 754, 408, 130, 46 };
	}
	else
	{
		// 740x360 landscape: centered 540x196 dialog.
		l.designWidth = 740;
		l.designHeight = 360;
		l.window  = { 100, 82, 540, 196 };
		l.title   = { 100, 94, 540, 30 };
		l.message = { 124, 134, 492, 76 };
		l.yes     = { 150, 224, 116, 42 };
		l.no      = { 474, 224, 116, 42 };
	}
	return l;
}

} // namespace Calypso
} // namespace OpenXcom
