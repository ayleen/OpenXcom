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
 * Phase 46.2-HD (Calypso) -- F34.Notes physical-overlay adapter.
 *
 * NotesState ALREADY has a complete HD implementation (logical resolution:
 * window, title, status, two-column TextList, inline TextEdit, action buttons,
 * origin labels, and all the interactive editing/selection logic). This adapter
 * is deliberately THIN: it does not re-implement any of that. Once per frame,
 * at the pre-blit boundary, collect() reads a CONST snapshot of those existing
 * widgets and emits their crisp physical replacements, claiming each so its
 * logical blit is suppressed -- EXCEPT the live TextEdit (_edtNote), which is
 * never claimed/emitted so caret + typing keep working, and the list row it
 * overlays while editing (skipped so the editor shows through the gap).
 *
 * A snapshot-only CalypsoHdFamilyAdapter, sibling of CalypsoStatisticsUi /
 * CalypsoErrorPopupUi. configure() just creates + registers the adapter (the
 * widgets already exist); collect() never mutates live state. Whole-file
 * Emscripten guard (Phase 36).
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdFamilyAdapter.h"

namespace OpenXcom
{
class NotesState;

namespace Calypso
{

class CalypsoNotesUi : public CalypsoHdFamilyAdapter
{
public:
	explicit CalypsoNotesUi(NotesState* state) : _state(state) {}
	~CalypsoNotesUi() override;

	// --- CalypsoHdFamilyAdapter (snapshot-only) ---
	const void* topState() const override;
	bool physicalReady() const override;
	void collect(CalypsoHdFrameBuilder& builder) const override;

	/// Create + register the adapter for an HD-enabled Notes screen (no-op when
	/// _hdLayout is false). Called at the end of NotesState's HD setup.
	static void configure(NotesState& state);

private:
	NotesState* _state = nullptr;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
