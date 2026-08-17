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
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoF21LayoutBase.h"
#endif

namespace OpenXcom
{

namespace Calypso { class CalypsoF21DestructionUi; }

class Base;
class Window;
class Text;
class TextButton;
class TextList;
class Ufo;

/**
 * Screen that allows the player
 * to pick a target for a craft on the globe.
 */
class BaseDestroyedState : public State
{
private:
	Window *_window;
	Text *_txtMessage;
	TextButton *_btnOk;
	TextList *_lstDestroyedFacilities;
	Base *_base;
	bool _missiles, _partialDestruction;
#ifdef __EMSCRIPTEN__
	friend class Calypso::CalypsoF21DestructionUi;
	bool _hdLayout = false;
	bool _hdWideLayout = false;
	Text* _hdProtocol = nullptr;
	Text* _hdTitle = nullptr;
	Text* _hdWarning = nullptr;
	Calypso::CalypsoF21Rect _hdListBand{ 0, 0, 0, 0 };
	Calypso::CalypsoF21DestructionUi* _hdAdapter = nullptr;
#endif
public:
	/// Creates the Select Destination state.
	BaseDestroyedState(Base *base, const Ufo* ufo, bool missiles, bool partialDestruction);
	/// Cleans up the Select Destination state.
	~BaseDestroyedState();
	/// Handler for clicking the Cydonia mission button.
	void btnOkClick(Action *action);
#ifdef __EMSCRIPTEN__
	/// Handler for resize (delegates to the HD adapter first).
	void resize(int &dX, int &dY) override;
#endif

};

}
