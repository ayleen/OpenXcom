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
 * F21 (Calypso, Phase 46.F21): HD adapter for BaseNameState -- the
 * first-base naming dialog (fixed-location path + logical fallback host).
 * Whole file Emscripten-only.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoF21NameLayout.h"
#include "CalypsoHdFamilyAdapter.h"

#include <cstdint>

namespace OpenXcom
{

class BaseNameState;

namespace Calypso
{

class CalypsoF21NameUi : public CalypsoHdFamilyAdapter
{
public:
	explicit CalypsoF21NameUi(BaseNameState* state) : _state(state) {}
	~CalypsoF21NameUi() override;

	const void* topState() const override;
	void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
	void collect(CalypsoHdFrameBuilder& builder) const override;

	static void configure(BaseNameState& state, bool allowPhysicalOverlay);
	static bool resize(BaseNameState& state);
	static void applyRects(BaseNameState& state, const CalypsoF21NameLayout& layout);

private:
	BaseNameState* _state;
	mutable bool _presented = false;
	mutable std::uint64_t _presentedAtFrame = 0;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
