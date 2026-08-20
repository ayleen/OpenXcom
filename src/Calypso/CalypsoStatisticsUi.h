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
 * Phase 46.2-HD (Calypso) -- F34.Statistics family adapter on the shared HD UI
 * overlay (sibling of CalypsoErrorPopupUi).
 *
 * A snapshot-only CalypsoHdFamilyAdapter: once per frame, at the pre-blit
 * boundary, collect() reads a CONST snapshot of the statistics screen's
 * widgets and emits two atomic subgroups:
 *   - the chrome subgroup: the outer window bevel + the four decorative bevel
 *     panels (header/list/return/footer) + the title text. Every item here
 *     carries a real widget pointer so the overlay's claim/skip path
 *     (Window::blit / Text::blit / CalypsoBevelPanel::blit) suppresses the
 *     matching logical draw exactly when the subgroup commits.
 *   - the rows subgroup: each visible TextList row's label + value cell is
 *     emitted as a Text item at its live row rect (computed from the list
 *     rect + scroll position + per-row height read off the TextList). The
 *     list widget itself (_lstStats) and the OK/scroll buttons stay UNCLAIMED
 *     so mouse / touch / keyboard / wheel scrolling and the OK activation
 *     keep working through their logical widgets; collect() re-reads the list
 *     every frame so scrolling updates naturally.
 *
 * collect() NEVER mutates live widget state. The redesigned widgets
 * themselves are built once in configure() (create-time); resize() re-fits
 * them and recomputes the Compact/Wide layout class. Whole-file Emscripten
 * guard (Phase 36).
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdFamilyAdapter.h"
#include "CalypsoF34StatisticsLayout.h"

namespace OpenXcom
{
class StatisticsState;
class Action;

namespace Calypso
{

class CalypsoStatisticsUi : public CalypsoHdFamilyAdapter
{
public:
	explicit CalypsoStatisticsUi(StatisticsState* state) : _state(state) {}
	~CalypsoStatisticsUi() override;

	// --- CalypsoHdFamilyAdapter (snapshot-only) ---
	const void* topState() const override;
	bool physicalReady() const override;
	void collect(CalypsoHdFrameBuilder& builder) const override;

	// --- Entry points called from StatisticsState ---
	/// Build the redesigned widgets (gated on isHdUiFamilyEnabled("F34")),
	/// create the adapter instance, and register it with the overlay. A no-op
	/// that leaves the state as the legacy statistics screen when the gate is
	/// off.
	static void configure(StatisticsState& state);
	/// Re-fit widgets on canvas resize and recompute the Compact/Wide layout
	/// class from the current base resolution. Returns true iff HD handled it.
	static bool resize(StatisticsState& state);
	/// Route list scroll keys (arrows / PgUp-PgDn / Home-End) to the stats
	/// TextList. Returns true iff HD consumed the action.
	static bool handle(StatisticsState& state, Action* action);
	/// Manual scroll-button handlers.
	static void scrollUp(StatisticsState& state);
	static void scrollDown(StatisticsState& state);

private:
	static void applyRects(StatisticsState& state, const CalypsoF34StatisticsLayout& layout);
	static void rebuildList(StatisticsState& state, std::size_t scroll);

	StatisticsState* _state = nullptr;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
