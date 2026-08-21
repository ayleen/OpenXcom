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
namespace Calypso { class CalypsoF12TransferConfirmUi; }

class TextButton;
class Window;
class Text;
class TransferItemsState;
class Base;

/**
 * Window to confirm a transfer between bases.
 */
class TransferConfirmState : public State
{
#ifdef __EMSCRIPTEN__
friend class Calypso::CalypsoF12TransferConfirmUi;
#endif
private:
	TextButton *_btnCancel, *_btnOk;
	Window *_window;
	Text *_txtTitle, *_txtCost, *_txtTotal;
	Base *_base;
	TransferItemsState *_state;
public:
	/// Creates the Transfer Confirm state.
	TransferConfirmState(Base *base, TransferItemsState *state);
	/// Cleans up the Transfer Confirm state.
	~TransferConfirmState();
	/// Handler for clicking the Cancel button.
	void btnCancelClick(Action *action);
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);

#ifdef __EMSCRIPTEN__
private:
    bool _hdLayout = false;
    bool _hdWideLayout = false;
    Calypso::CalypsoF12TransferConfirmUi *_hdAdapter = nullptr;
public:
    void resize(int &dX, int &dY) override;
#endif
};

}
