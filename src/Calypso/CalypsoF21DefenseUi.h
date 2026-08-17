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
 * F21 (Calypso, Phase 46.F21): HD adapter for BaseDefenseState -- the base
 * defense instrumentation window. The live TextList is snapshotted read-only
 * and rendered physically (getCellTextsSnapshot); the 250/333 ms state
 * machine, RNG ordering, and ammo rules are untouched.
 * Whole file Emscripten-only.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoF21DefenseLayout.h"
#include "CalypsoHdFamilyAdapter.h"

#include <cstdint>

namespace OpenXcom
{

class BaseDefenseState;

namespace Calypso
{

class CalypsoF21DefenseUi : public CalypsoHdFamilyAdapter
{
public:
	explicit CalypsoF21DefenseUi(BaseDefenseState* state) : _state(state) {}
	~CalypsoF21DefenseUi() override;

	const void* topState() const override;
	void collect(CalypsoHdFrameBuilder& builder) const override;

	static void configure(BaseDefenseState& state, bool allowPhysicalOverlay);
	static bool resize(BaseDefenseState& state);
	static void applyRects(BaseDefenseState& state, const CalypsoF21DefenseLayout& layout);

private:
	BaseDefenseState* _state;
	mutable bool _presented = false;
	mutable std::uint64_t _presentedAtFrame = 0;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
