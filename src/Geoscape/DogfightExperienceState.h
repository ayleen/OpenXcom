#pragma once
/*
 * Copyright 2010-2020 OpenXcom Developers.
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
#include "../Engine/State.h"

namespace OpenXcom
{

namespace Calypso { class CalypsoBaseLedger; }
class TextButton;
class Window;
class Text;
class TextList;

/**
 * Displays experience gained by craft pilot(s) during the current day.
 */
class DogfightExperienceState : public State
{
private:
#ifdef __EMSCRIPTEN__
	friend class Calypso::CalypsoBaseLedger;
	Calypso::CalypsoBaseLedger *_ledger = nullptr;
	void resize(int &dx, int &dy) override;
#endif
	TextButton *_btnOk;
	Window *_window;
	Text *_txtTitle, *_txtFiringAcc, *_txtReactions, *_txtBravery, *_txtPilots;
	TextList *_lstPilots;
public:
	/// Creates the DogfightExperienceState.
	DogfightExperienceState();
	/// Cleans up the DogfightExperienceState.
	~DogfightExperienceState() override;
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
};

}
