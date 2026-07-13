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
#include "OptionsBaseState.h"
#include <string>
#include <vector>

namespace OpenXcom
{

class TextButton;
class Window;
class Text;
class TextEdit;
class TextList;
class ToggleTextButton;
class TTFFont;

/**
 * Allows the player to take notes.
 */
class NotesState : public State
{
protected:
	Window* _window;
	Text* _txtTitle;
	Text* _txtDelete;
	TextList* _lstNotes;
	TextEdit* _edtNote;
	TextButton* _btnSave;
	TextButton* _btnCancel;
	ToggleTextButton* _btnDelete;
	TextButton* _btnNew;
	TextButton* _btnKeep;
	Text* _txtOriginGeo;
	Text* _txtOriginBattle;
	TTFFont* _hdFont;

	OptionsOrigin _origin;
	std::string _selectedNote;
	int _previousSelectedRow, _selectedRow;
	bool _hdLayout;
	bool _hdNotesLoaded;
	int _deleteRow;
	std::uint64_t _focusGeneration;
	std::vector<std::string> _originalNotes;
	std::vector<std::string> _workingNotes;

	/// Updates the Notes list.
	void updateList();
	/// Updates the HD working-copy list without touching the campaign save.
	void updateHdList();
	/// Starts inline editing for a working-copy row (size() means new note).
	void beginHdEdit(int row);
	/// Applies the active HD row edit to the working copy.
	void applyHdEdit(Action* action);
	/// Cancels the active HD row edit and restores the working copy.
	void cancelHdEdit();
	/// Re-anchors the active editor from current list geometry after scroll/reflow.
	void positionHdEditor();
	/// Rebuilds stable keyboard focus targets after action visibility changes.
	void rebuildHdFocus();
	/// Refreshes localized dirty/confirmation status without relying on color.
	void updateHdStatus();
	/// Applies the scalable dark Calypso HD visual language to every Notes control.
	void applyHdVisualStyle();
	/// Invalidates surfaces covered by the inline editor after it is hidden.
	void invalidateHdEditorArea();
public:
	/// Creates the Notes state.
	NotesState(OptionsOrigin origin);
	/// Cleans up the Notes state.
	~NotesState();
	/// Refreshes the Notes state.
	void init() override;
	/// Routes one-click HD actions while the inline editor owns modal input.
	void handle(Action* action) override;
	/// Re-applies the responsive HD layout on browser viewport changes.
	void resize(int &dX, int &dY) override;
	/// Handler for clicking the Cancel button.
	void btnCancelClick(Action* action);
	/// Handler for clicking the Notes list.
	void lstNotesPress(Action* action);
	/// Handler for pressing a key on the Note edit.
	void edtNoteKeyPress(Action* action);
	/// Handler for the HD New note action.
	void btnNewClick(Action* action);
	/// Handler for the visible HD row Delete action.
	void btnDeleteClick(Action* action);
	/// Handler for canceling the HD row-delete confirmation.
	void btnKeepClick(Action* action);
	/// Handler for clicking on the Save button.
	void btnSaveClick(Action* action);
};

}
