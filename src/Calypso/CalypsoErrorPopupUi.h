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
 * Phase 46.2-HD (Calypso) -- F34.ErrorPopup family adapter on the shared HD UI
 * overlay (remediation B-Error).
 *
 * A snapshot-only CalypsoHdFamilyAdapter: once per frame, at the pre-blit
 * boundary, collect() reads a CONST snapshot of the popup's widgets and emits
 * one atomic subgroup (window/badge/button bevels + icon/heading/message/label
 * text) with full claim ids + deterministic order keys + real alignment +
 * metrics-derived physical font size. It NEVER mutates live widget state. The
 * overlay commits the subgroup only if every item rasters+uploads, so a partial
 * failure falls straight back to the unmodified logical popup -- every visual
 * (including the badge, a CalypsoBevelPanel with a bitmap fallback) has a
 * complete logical rendering.
 *
 * The redesigned widgets themselves are built once in configure() (create-time,
 * the state's real F34 design); resize() re-fits them and recomputes the
 * Compact/Wide layout class. Whole-file Emscripten guard (Phase 36).
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoF34ErrorLayout.h"

namespace OpenXcom
{
class ErrorMessageState;

namespace Calypso
{

class CalypsoErrorPopupUi : public CalypsoHdFamilyAdapter
{
public:
	explicit CalypsoErrorPopupUi(ErrorMessageState* state) : _state(state) {}
	~CalypsoErrorPopupUi() override;

	// --- CalypsoHdFamilyAdapter (snapshot-only) ---
	const void* topState() const override;
	void collect(CalypsoHdFrameBuilder& builder) const override;

	// --- Entry points called from ErrorMessageState ---
	/// Build the redesigned widgets (gated on isHdUiFamilyEnabled("F34")),
	/// create the adapter instance, and register it with the overlay. A no-op
	/// that leaves the state as the legacy popup when the gate is off.
	static void configure(ErrorMessageState& state, bool allowPhysicalOverlay = true);
	/// Re-fit widgets on canvas resize and recompute the Compact/Wide layout
	/// class from the current base resolution. Returns true iff HD handled it.
	static bool resize(ErrorMessageState& state);

private:
	static void applyRects(ErrorMessageState& state, const CalypsoF34ErrorLayout& layout);

	ErrorMessageState* _state = nullptr;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
