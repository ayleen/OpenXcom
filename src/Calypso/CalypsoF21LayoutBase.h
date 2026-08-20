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
 * F21 (Calypso, Phase 46.F21): shared pure-geometry base for the four F21
 * family layouts (site / transaction / defense / destruction). Same
 * conventions as CalypsoF33AbandonLayout (Phase 46.4-F33): geometry is
 * authored in the canonical contracts and generated; this layer is pure,
 * dependency-light (CalypsoUiMetrics.h only) and NOT wrapped in
 * #ifdef __EMSCRIPTEN__, so it stays natively testable.
 */
#include "CalypsoUiMetrics.h"
#include "Generated/CalypsoHdTheme.generated.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string_view>

namespace OpenXcom
{
namespace Calypso
{

// Contract-version guard (46.4-F33.2 contract rule, shared by every F21
// family header): a consumer compiled against a stale generated pair is a
// build error, never a silent drift.
inline constexpr const char* kCalypsoF21ContractVersion = "hd.2026-08-20.19";
static_assert(std::string_view(kCalypsoF21ContractVersion) ==
		std::string_view(CalypsoHdThemeGen::kContractVersion),
	"F21 and theme generated contracts carry different versions; regenerate");

/// One design-space rectangle of any F21 layout.
struct CalypsoF21Rect
{
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
};

/// Scale a component and its position around the window centre -- both edges
/// of every rect transform around the same centre, so an opening-motion frame
/// preserves component relationships (F33 lesson: never scale sizes and then
/// independently centre).
inline CalypsoF21Rect calypsoF21ScaleRectAroundWindow(
	const CalypsoF21Rect& rect, const CalypsoF21Rect& window, double scale)
{
	const double bounded = std::max(0.0, scale);
	const double cx = window.x + window.width * 0.5;
	const double cy = window.y + window.height * 0.5;
	const int left = (int)std::llround(cx + (rect.x - cx) * bounded);
	const int top = (int)std::llround(cy + (rect.y - cy) * bounded);
	const int right = (int)std::llround(cx + (rect.x + rect.width - cx) * bounded);
	const int bottom = (int)std::llround(cy + (rect.y + rect.height - cy) * bounded);
	return { left, top, std::max(1, right - left), std::max(1, bottom - top) };
}

/// Harness-only side-by-side translation (Wide dialog shifted so the DOM
/// reference card fits on the right). Pure, so reconfiguration and the native
/// contract tests exercise the same geometry operation.
inline void calypsoF21ApplyHarnessShift(
	std::initializer_list<CalypsoF21Rect*> rects, bool sideBySide, int windowX)
{
	if (!sideBySide) return;
	const int dx = 40 - windowX;
	for (CalypsoF21Rect* r : rects) r->x += dx;
}

} // namespace Calypso
} // namespace OpenXcom
