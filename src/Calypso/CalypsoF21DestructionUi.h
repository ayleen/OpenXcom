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
 * F21 (Calypso, Phase 46.F21): HD adapter for BaseDestroyedState -- the
 * destruction review window (partial facility loss / full destruction).
 * Whole file Emscripten-only.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoF21DestructionLayout.h"
#include "CalypsoHdFamilyAdapter.h"

#include <cstdint>

namespace OpenXcom
{

class BaseDestroyedState;

namespace Calypso
{

class CalypsoF21DestructionUi : public CalypsoHdFamilyAdapter
{
public:
	explicit CalypsoF21DestructionUi(BaseDestroyedState* state) : _state(state) {}
	~CalypsoF21DestructionUi() override;

	const void* topState() const override;
	void collect(CalypsoHdFrameBuilder& builder) const override;

	static void configure(BaseDestroyedState& state, bool allowPhysicalOverlay);
	static bool resize(BaseDestroyedState& state);
	static void applyRects(BaseDestroyedState& state, const CalypsoF21DestructionLayout& layout);

private:
	BaseDestroyedState* _state;
	mutable bool _presented = false;
	mutable std::uint64_t _presentedAtFrame = 0;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
