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
#include "Globe.h"

namespace OpenXcom
{

namespace Calypso { class CalypsoF21NameUi; }

class Base;
class BuildNewBaseState;
class Window;
class Text;
class TextEdit;
class TextButton;
class Globe;

/**
 * Window used to input a name for a new base.
 * Player's first Base uses this screen
 * additional bases use ConfirmNewBaseState
 */
class BaseNameState : public State
{
private:
	Base *_base;
	Globe *_globe;
	Window *_window;
	Text *_txtTitle;
	TextEdit *_edtName;
	TextButton *_btnOk, *_btnCancel;
	bool _first;
	bool _fixedLocation;
#ifdef __EMSCRIPTEN__
	friend class Calypso::CalypsoF21NameUi;
	BuildNewBaseState *_coveredSite = nullptr;
	bool _hdLayout = false;
	bool _hdWideLayout = false;
	Text* _hdHint = nullptr;
	Text* _hdProtocol = nullptr;
	Calypso::CalypsoF21NameUi* _hdAdapter = nullptr;
#endif
public:
	/// Creates the Base Name state.
	BaseNameState(Base *base, Globe *globe, bool first, bool fixedLocation);
	/// Cleans up the Base Name state.
	~BaseNameState();
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	void btnCancelClick(Action *action);
	/// Handler for changing text on the Name edit.
	void edtNameChange(Action *action);
#ifdef __EMSCRIPTEN__
	/// Handler for resize (delegates to the HD adapter first).
	void resize(int &dX, int &dY) override;
#endif
};

}
