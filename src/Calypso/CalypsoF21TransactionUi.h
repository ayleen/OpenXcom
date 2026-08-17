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
 * F21 (Calypso, Phase 46.F21): HD adapter for ConfirmNewBaseState -- the
 * merged site/cost/name transaction window. Follows the Phase 46.4-F33
 * snapshot-only pattern (CalypsoAbandonPopupUi): logical widgets keep
 * layout/input/fallback; the adapter submits physical panels + TTF text to
 * the shared CalypsoHdUiOverlay queue while the family gate F21 is on.
 *
 * Whole file Emscripten-only.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoF21TransactionLayout.h"
#include "CalypsoHdFamilyAdapter.h"

#include <cstdint>

namespace OpenXcom
{

class ConfirmNewBaseState;

namespace Calypso
{

class CalypsoF21TransactionUi : public CalypsoHdFamilyAdapter
{
public:
	explicit CalypsoF21TransactionUi(ConfirmNewBaseState* state) : _state(state) {}
	~CalypsoF21TransactionUi() override;

	// --- CalypsoHdFamilyAdapter (snapshot-only) ---
	const void* topState() const override;
	void collect(CalypsoHdFrameBuilder& builder) const override;

	/// Configure the state for the physical route; no-op when ineligible.
	static void configure(ConfirmNewBaseState& state, bool allowPhysicalOverlay);
	/// Recompute the layout class on resize; false when not on the HD route.
	static bool resize(ConfirmNewBaseState& state);
	/// Apply the design-space rectangles to the state's widgets.
	static void applyRects(ConfirmNewBaseState& state, const CalypsoF21TransactionLayout& layout);

private:
	ConfirmNewBaseState* _state;

	// Opening-motion presentation clock (collect() is const).
	mutable bool _presented = false;
	mutable std::uint64_t _presentedAtFrame = 0;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
