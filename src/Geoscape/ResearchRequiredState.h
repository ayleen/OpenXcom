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
namespace Calypso { class CalypsoF24ResearchRequiredUi; }

class Game;
class Window;
class TextButton;
class Text;
class RuleItem;

/**
 * Window shown when the player researches a weapon
 * before the respective clip.
 */
class ResearchRequiredState : public State
{
#ifdef __EMSCRIPTEN__
friend class Calypso::CalypsoF24ResearchRequiredUi;
#endif
	Window *_window;
	Text *_txtTitle;
	TextButton *_btnOk;
public:
	~ResearchRequiredState();
	/// Creates the ResearchRequired state.
	ResearchRequiredState(RuleItem *item);
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);

#ifdef __EMSCRIPTEN__
private:
    bool _hdLayout = false;
    bool _hdWideLayout = false;
    Calypso::CalypsoF24ResearchRequiredUi *_hdAdapter = nullptr;
public:
    void resize(int &dX, int &dY) override;
#endif
};

}
