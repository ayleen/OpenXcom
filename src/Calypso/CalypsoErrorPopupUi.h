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
 * Phase 46.2-HD.5 (Calypso) -- F34.ErrorPopup adapter for the shared HD UI
 * overlay queue (CalypsoHdUiOverlay). Replaces the standalone
 * `phase-46-hd-ui-pilots` spike (F34PhysicalTextOverlay + a persistent
 * `_ttfPhysicalOnly` widget flag) with the shared submitText/submitPanel/
 * claimWidget primitives already landed on this branch (HD.1-HD.4): no
 * private GL, no per-widget physical-only suppression flag -- every claim is
 * frame-scoped and recreated only when this frame actually queued a physical
 * replacement, so a font-resolution failure or a dormant/lost GL context
 * falls back to the complete, unmodified logical popup automatically.
 *
 * Whole-file Emscripten guard (Phase 36 placement policy).
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoF34ErrorLayout.h"

namespace OpenXcom
{

class ErrorMessageState;

namespace Calypso
{

class CalypsoErrorPopupUi
{
public:
	/// Called once from ErrorMessageState::create(), after the legacy widgets
	/// exist. No-op unless `state._hdLayout` (Mod::isHdUiFamilyEnabled("F34"))
	/// is true. Builds the extra HD-only widgets (icon/warning/detail labels,
	/// a geometry-only icon-panel placeholder), lays out every widget at the
	/// approved F34.ErrorPopup rectangles (CalypsoF34ErrorLayout) via the
	/// existing State uniform UI-scaling capture, and attaches the feeder
	/// surface that submits the physical replacement each frame.
	static void configure(ErrorMessageState& state);

	/// Called from ErrorMessageState::resize(). Returns false (caller falls
	/// through to State::resize) when the HD layout is off. The Compact/Wide
	/// layout package is fixed for the lifetime of one popup instance
	/// (deliberate simplification -- State's UI-scaling capture is a one-shot
	/// per state, see CalypsoStateUi.cpp); only the uniform fit/centering is
	/// re-applied on resize.
	static bool resize(ErrorMessageState& state);

private:
	/// Applies every widget rect from `layout` (design-space px) to the
	/// state's real and HD-only widgets. A private static member (not a free
	/// function) so it shares this class's friend access to `state`'s private
	/// members with `configure()`.
	static void applyRects(ErrorMessageState& state, const CalypsoF34ErrorLayout& layout);
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
