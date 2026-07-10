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
 * Phase 41 (Calypso) -- "Six months later." interstitial (D7). Pushed by
 * CalypsoPrologueScene::makeEndState() instead of the vanilla Debriefing.
 * Full-screen black card with a centered title; any click/key creates the
 * real campaign (Calypso::finishPrologue) and hands off to the Geoscape.
 *
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 */
#ifdef __EMSCRIPTEN__

#include "../Engine/State.h"

namespace OpenXcom
{
class Text;
class Action;

class CalypsoPrologueEndState : public State
{
private:
	Text* _txtTitle;
	int _outcome;
public:
	explicit CalypsoPrologueEndState(int outcome);
	~CalypsoPrologueEndState();
	void handle(Action* action) override;
};

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
