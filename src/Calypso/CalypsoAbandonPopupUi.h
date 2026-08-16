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
 * F33 (Calypso): HD adapter for AbandonGameState -- the destructive session
 * exit confirmation. Follows the Phase 46.2-HD snapshot-only adapter pattern
 * of CalypsoErrorPopupUi: the logical widgets keep layout/input/fallback, and
 * this adapter submits physical-resolution panels + TTF text to the shared
 * CalypsoHdUiOverlay queue while the family gate (hdUiFamilies: F33) is on.
 *
 * Whole file Emscripten-only.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoF33AbandonLayout.h"
#include "CalypsoHdFamilyAdapter.h"

namespace OpenXcom
{

class AbandonGameState;

namespace Calypso
{

class CalypsoAbandonPopupUi : public CalypsoHdFamilyAdapter
{
public:
	explicit CalypsoAbandonPopupUi(AbandonGameState* state) : _state(state) {}
	~CalypsoAbandonPopupUi() override;

	// --- CalypsoHdFamilyAdapter (snapshot-only) ---
	const void* topState() const override;
	void collect(CalypsoHdFrameBuilder& builder) const override;

	/// Configure the state for the physical route; no-op when ineligible.
	static void configure(AbandonGameState& state, bool allowPhysicalOverlay);
	/// Recompute the layout class on resize; false when not on the HD route.
	static bool resize(AbandonGameState& state);
	/// Apply the design-space rectangles to the state's widgets.
	static void applyRects(AbandonGameState& state, const CalypsoF33AbandonLayout& layout);

private:
	AbandonGameState* _state;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
