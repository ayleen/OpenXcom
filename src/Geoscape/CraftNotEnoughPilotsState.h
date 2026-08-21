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
#include "../Engine/State.h"

namespace OpenXcom
{
namespace Calypso { class CalypsoF18NotEnoughPilotsUi; }

class TextButton;
class Window;
class Text;
class Craft;

/**
 * Window used to notify the player when
 * there are not enough pilots to pilot the craft.
 */
class CraftNotEnoughPilotsState : public State
{
#ifdef __EMSCRIPTEN__
friend class Calypso::CalypsoF18NotEnoughPilotsUi;
#endif
private:
	TextButton *_btnOk, *_btnAssignPilots;
	Window *_window;
	Text *_txtMessage;
	Craft *_craft;
public:
	/// Creates the CraftNotEnoughPilotsState state.
	CraftNotEnoughPilotsState(Craft *craft);
	/// Cleans up the CraftNotEnoughPilotsState state.
	~CraftNotEnoughPilotsState();
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the [Assign Pilots] button.
	void btnAssignPilotsClick(Action *action);

#ifdef __EMSCRIPTEN__
private:
    bool _hdLayout = false;
    bool _hdWideLayout = false;
    Calypso::CalypsoF18NotEnoughPilotsUi *_hdAdapter = nullptr;
public:
    void resize(int &dX, int &dY) override;
#endif
};

}
