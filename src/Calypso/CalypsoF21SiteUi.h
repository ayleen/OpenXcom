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
 * F21 (Calypso, Phase 46.F21): HD adapter for BuildNewBaseState -- the
 * site-selection command strip + placement preview card over the live globe.
 * The globe itself is NEVER claimed; only the logical strip widgets
 * (window/title/cancel) plus the HD-only card readouts draw physically.
 *
 * Whole file Emscripten-only.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoF21SiteLayout.h"
#include "CalypsoHdFamilyAdapter.h"

#include <cstdint>

namespace OpenXcom
{

class BuildNewBaseState;

namespace Calypso
{

class CalypsoF21SiteUi : public CalypsoHdFamilyAdapter
{
public:
	explicit CalypsoF21SiteUi(BuildNewBaseState* state) : _state(state) {}
	~CalypsoF21SiteUi() override;

	// --- CalypsoHdFamilyAdapter (snapshot-only) ---
	const void* topState() const override;
	void collect(CalypsoHdFrameBuilder& builder) const override;

	/// Configure the state for the physical route; no-op when ineligible.
	static void configure(BuildNewBaseState& state, bool allowPhysicalOverlay);
	/// Recompute the layout class on resize; false when not on the HD route.
	static bool resize(BuildNewBaseState& state);
	/// Apply the design-space rectangles to the state's widgets.
	static void applyRects(BuildNewBaseState& state, const CalypsoF21SiteLayout& layout);

private:
	BuildNewBaseState* _state;

	// Fade-in presentation clock (collect() is const).
	mutable bool _presented = false;
	mutable std::uint64_t _presentedAtFrame = 0;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
