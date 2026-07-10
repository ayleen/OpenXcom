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
 * Phase 41 (Calypso) -- concrete-scene registration, kept separate from
 * CalypsoDirector so the director stays scene-agnostic (no knowledge of any
 * particular deployment id or scene class). One line per scene here; a second
 * scripted mission adds a second registerScene() call, nothing else.
 *
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 */
#ifdef __EMSCRIPTEN__

namespace OpenXcom
{

/// Registers every known CalypsoScene factory with CalypsoDirector. Called
/// once from main() before the game runs.
void registerCalypsoScenes();

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
