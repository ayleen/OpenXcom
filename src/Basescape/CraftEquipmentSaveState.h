#pragma once
/*
* Copyright 2010-2015 OpenXcom Developers.
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
#include <string>
#include "../Engine/State.h"

namespace OpenXcom
{

namespace Calypso { class CalypsoF08CraftEquipmentSaveUi; }

class TextEdit;
class TextButton;
class Window;
class Text;
class TextList;
class CraftEquipmentState;

/**
* Craft Loadout Save window that allows saving of the equipment onboard to a global template.
*/
class CraftEquipmentSaveState : public State
{
private:
	CraftEquipmentState *_parent;
	Window *_window;
	Text *_txtTitle;
	TextList *_lstLoadout;
	TextButton *_btnCancel, *_btnSave;
	TextEdit *_edtSave;
	std::string _selected;
	int _previousSelectedRow, _selectedRow;
public:
	/// Creates the Save Craft Loadout state.
	CraftEquipmentSaveState(CraftEquipmentState *parent);
	/// Cleans up the Save Craft Loadout state.
	~CraftEquipmentSaveState();
	/// Handler for clicking the Cancel button.
	void btnCancelClick(Action *action);
	/// Handler for clicking the Save button.
	void btnSaveClick(Action *action);
	/// Handler for clicking the Loadout list.
	void lstLoadoutPress(Action *action);
	/// Handler for pressing a key on the Save edit.
	void edtSaveKeyPress(Action *action);
	/// Save template.
	void saveTemplate();
#ifdef __EMSCRIPTEN__
	/// HD explicit-overwrite gate (C1): saving to an empty slot commits
	/// directly through the unchanged save path, while a non-empty slot
	/// arms first and only commits on the second Save activation for the
	/// same slot. Arming is slot-bound, so reselecting always re-arms.
	void hdSaveClickGate(Action *action);
	/// Drops an armed overwrite review and restores the Save label.
	void hdDisarmSave();
private:
	friend class Calypso::CalypsoF08CraftEquipmentSaveUi;
	bool _hdLayout = false;
	bool _hdWideLayout = false;
	Calypso::CalypsoF08CraftEquipmentSaveUi *_hdAdapter = nullptr;
	bool _hdSaveArmed = false;
	int _hdArmedRow = -1;
public:
	void resize(int &dX, int &dY) override;
#endif
};

}
