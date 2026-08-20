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
 * along with OpenXcom.  If not, see <http:///www.gnu.org/licenses/>.
 */
#include "../Engine/State.h"

namespace OpenXcom
{
namespace Calypso { class CalypsoF20ConfirmCydoniaUi; }

class Craft;
class Window;
class Text;
class TextButton;

/**
 * Screen that allows the player
 * to pick a target for a craft on the globe.
 */
class ConfirmCydoniaState : public State
{
#ifdef __EMSCRIPTEN__
friend class Calypso::CalypsoF20ConfirmCydoniaUi;
#endif
private:
	Window *_window;
	Text *_txtMessage;
	TextButton *_btnNo, *_btnYes;
	Craft *_craft;
public:
	/// Creates the Select Destination state.
	ConfirmCydoniaState(Craft *craft);
	/// Cleans up the Select Destination state.
	~ConfirmCydoniaState();
	/// Handler for clicking the Cancel button.
	void btnNoClick(Action *action);
	/// Handler for clicking the Cydonia mission button.
	void btnYesClick(Action *action);


#ifdef __EMSCRIPTEN__
private:
    bool _hdLayout = false;
    bool _hdWideLayout = false;
    Calypso::CalypsoF20ConfirmCydoniaUi *_hdAdapter = nullptr;
public:
    void resize(int &dX, int &dY) override;
#endif
};

}
