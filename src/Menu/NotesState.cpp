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
#include "NotesState.h"
#include <algorithm>
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Calypso/CalypsoCommonRecords.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoCommonRecordsStateUi.h"
#endif
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Interface/ToggleTextButton.h"
#include "../Interface/Window.h"
#include "../Savegame/SavedGame.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Notes screen.
 * @param origin Game section that originated this state.
 */
NotesState::NotesState(OptionsOrigin origin) :
	_btnNew(nullptr), _btnKeep(nullptr), _txtOriginGeo(nullptr), _txtOriginBattle(nullptr),
	_hdFont(nullptr),
	_origin(origin), _previousSelectedRow(-1), _selectedRow(-1),
	_hdLayout(false), _hdNotesLoaded(false), _deleteRow(-1), _hdListSelection(0), _focusGeneration(0),
	_hdListPointerDown(false), _hdListDrag(false), _hdListActivationArmed(false), _hdListPressedRow(-1),
	_hdListPointerX(0.0), _hdListPointerY(0.0)
{
	_screen = false;

#ifdef __EMSCRIPTEN__
	Calypso::CalypsoNotesStateUi::createControls(*this);
#endif

	if (!_hdLayout)
	{
		_window = new Window(this, 320, 200, 0, 0, POPUP_NONE);
		_txtTitle = new Text(310, 17, 5, 7);
		_txtDelete = new Text(310, 9, 5, 23);
		_lstNotes = new TextList(288, 120, 8, 42);
		_edtNote = new TextEdit(this, 268, 9, 0, 0);
		_btnSave = new TextButton(80, 16, 60, 172);
		_btnCancel = new TextButton(80, 16, 180, 172);
		_btnDelete = new ToggleTextButton(288, 16, 16, 23);
	}

	// Set palette
	setInterface("geoscape", true, _game->getSavedGame() ? _game->getSavedGame()->getSavedBattle() : 0);

	add(_window, "window", "noteMenu");
	add(_txtTitle, "text", "noteMenu");
	add(_txtDelete, "text", "noteMenu");
	add(_lstNotes, "list", "noteMenu");
	add(_edtNote);
	add(_btnSave, "button", "noteMenu");
	add(_btnCancel, "button", "noteMenu");
	add(_btnDelete, "button", "noteMenu");
#ifdef __EMSCRIPTEN__
	Calypso::CalypsoNotesStateUi::addControls(*this);
#endif

	centerAllSurfaces();

	// Set up objects
	if (!_hdLayout)
	{
		setWindowBackground(_window, "noteMenu");
	}

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_NOTES"));

#ifdef __MOBILE__
	if (!_hdLayout)
	{
	_txtDelete->setVisible(false);
	_btnDelete->setText(tr("STR_RIGHT_CLICK_TO_DELETE"));
	}
#else
	if (!_hdLayout)
	{
		_btnDelete->setVisible(false);
		_txtDelete->setAlign(ALIGN_CENTER);
		_txtDelete->setText(tr("STR_RIGHT_CLICK_TO_DELETE"));
	}
#endif

	if (!_hdLayout)
	{
		_lstNotes->setColumns(1, 288);
	}
	_lstNotes->setSelectable(true);
	_lstNotes->setBackground(_window);
	_lstNotes->setMargin(8);
	_lstNotes->setWordWrap(true);
	// The HD list commits a row only after a click. This lets touch/mouse drags
	// remain scrolling gestures instead of opening an inline editor on press.
	if (_hdLayout)
	{
		_lstNotes->onMousePress((ActionHandler)&NotesState::lstNotesPress);
		_lstNotes->onMouseClick((ActionHandler)&NotesState::lstNotesPress);
	}
	else
		_lstNotes->onMousePress((ActionHandler)&NotesState::lstNotesPress);

	_edtNote->setColor(_lstNotes->getSecondaryColor());
	_edtNote->setVisible(false);
	_edtNote->onKeyboardPress((ActionHandler)&NotesState::edtNoteKeyPress);

	_btnSave->setText(tr("STR_SAVE_UC"));
	_btnSave->onMouseClick((ActionHandler)&NotesState::btnSaveClick);
	//_btnSave->onKeyboardPress((ActionHandler)&NotesState::btnSaveClick, Options::keyOk);

	_btnCancel->setText(tr("STR_CANCEL_UC"));
	_btnCancel->onMouseClick((ActionHandler)&NotesState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&NotesState::btnCancelClick, Options::keyCancel);

#ifdef __EMSCRIPTEN__
	Calypso::CalypsoNotesStateUi::configureControls(*this);
#endif
}

/**
 *
 */
NotesState::~NotesState()
{

}

/**
 * Refreshes the Notes state.
 */
void NotesState::init()
{
	State::init();

	if (_origin == OPT_BATTLESCAPE)
	{
		applyBattlescapeTheme("noteMenu");
	}

#ifdef __EMSCRIPTEN__
	if (Calypso::CalypsoNotesStateUi::initialize(*this)) return;
#endif
	updateList();
}

void NotesState::applyHdVisualStyle()
{
#ifdef __EMSCRIPTEN__
	Calypso::CalypsoNotesStateUi::applyVisualStyle(*this);
#endif
}

/**
 * Keeps the editor modal for unambiguous keyboard ownership without imposing a
 * two-click blur tax on mouse/touch actions. Save and outer Cancel retain their
 * distinct semantics; other outside presses first apply the row, then continue
 * through normal hit testing in the same event.
 */
void NotesState::handle(Action* action)
{
#ifdef __EMSCRIPTEN__
	if (Calypso::CalypsoNotesStateUi::handle(*this, action)) return;
#endif
	State::handle(action);
}

/**
 * Re-applies browser HD scaling while preserving the exact native fallback.
 */
void NotesState::resize(int &dX, int &dY)
{
#ifdef __EMSCRIPTEN__
	if (Calypso::CalypsoNotesStateUi::resize(*this)) return;
#endif
	State::resize(dX, dY);
}

/**
 * Updates the Notes list.
 */
void NotesState::updateList()
{
	_lstNotes->clearList();

	int color = _lstNotes->getSecondaryColor();

	for (const auto& note : _game->getSavedGame()->getUserNotes())
	{
		_lstNotes->addRow(1, note.c_str());
	}

	_lstNotes->addRow(1, tr("STR_NEW_NOTE").c_str());
	if (_origin != OPT_BATTLESCAPE)
	{
		_lstNotes->setRowColor(_lstNotes->getLastRowIndex(), color);
	}
	_lstNotes->scrollDown(true);
}

/**
 * Rebuilds the HD list from its private working copy. The SavedGame remains
 * untouched until Save is explicitly activated.
 */
void NotesState::updateHdList()
{
#ifdef __EMSCRIPTEN__
	Calypso::CalypsoNotesStateUi::updateList(*this);
#endif
}

void NotesState::updateHdListSelectionIndicator()
{
#ifdef __EMSCRIPTEN__
	Calypso::CalypsoNotesStateUi::updateSelection(*this);
#endif
}

/**
 * Starts one inline edit. Existing edits are applied before moving to another
 * row, matching direct row activation on mouse, keyboard and touch.
 */
void NotesState::beginHdEdit(int row)
{
#ifdef __EMSCRIPTEN__
	Calypso::CalypsoNotesStateUi::beginEdit(*this, row);
#endif
}

/**
 * Positions the editor from live list/scroll geometry. This deliberately runs
 * after each rebuild and responsive reflow rather than retaining stale row
 * coordinates captured at construction.
 */
void NotesState::positionHdEditor()
{
#ifdef __EMSCRIPTEN__
	Calypso::CalypsoNotesStateUi::positionEditor(*this);
#endif
}

/**
 * Registers stable, localized-control-independent focus identifiers. The
 * editor itself becomes the State modal while active, so Enter/Shift+Enter and
 * Escape retain TextEdit ownership instead of being intercepted as actions.
 */
void NotesState::rebuildHdFocus()
{
#ifdef __EMSCRIPTEN__
	Calypso::CalypsoNotesStateUi::rebuildFocus(*this);
#endif
}

/**
 * Shows confirmation or dirty state as text so neither origin nor mutation
 * state depends on palette color alone.
 */
void NotesState::updateHdStatus()
{
#ifdef __EMSCRIPTEN__
	Calypso::CalypsoNotesStateUi::updateStatus(*this);
#endif
}

void NotesState::invalidateHdEditorArea()
{
#ifdef __EMSCRIPTEN__
	Calypso::CalypsoNotesStateUi::invalidateEditorArea(*this);
#endif
}

/**
 * Applies the active editor to the private working copy. Empty existing rows
 * remain stable while editing and are filtered only by Save; an empty new row
 * is simply abandoned.
 */
void NotesState::applyHdEdit(Action* action)
{
#ifdef __EMSCRIPTEN__
	Calypso::CalypsoNotesStateUi::applyEdit(*this, action);
#endif
}

/**
 * Cancels only the active row edit; notebook-level Cancel remains separate.
 */
void NotesState::cancelHdEdit()
{
#ifdef __EMSCRIPTEN__
	Calypso::CalypsoNotesStateUi::cancelEdit(*this);
#endif
}

/**
 * Returns to the previous screen without saving anything.
 * @param action Pointer to an action.
 */
void NotesState::btnCancelClick(Action*)
{
	_game->popState();
}

/**
 * Allows to enter, edit and delete notes.
 * @param action Pointer to an action.
 */
void NotesState::lstNotesPress(Action* action)
{
#ifdef __EMSCRIPTEN__
	if (Calypso::CalypsoNotesStateUi::listPress(*this, action)) return;
#endif

	// ignore scrolling, process only LMB and RMB
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT || action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		_previousSelectedRow = _selectedRow;
		_selectedRow = _lstNotes->getSelectedRow();

		// restore previous
		if (_previousSelectedRow > -1)
		{
			_lstNotes->setCellText(_previousSelectedRow, 0, _selectedNote);
		}

		// back up current
		_selectedNote = _lstNotes->getCellText(_selectedRow, 0);
	}

	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT || _btnDelete->getPressed())
	{
		if (_edtNote->isFocused())
		{
			// cancel editing the current note
			_edtNote->setText("");
			_edtNote->setVisible(false);
			_edtNote->setFocus(false, false);
			_lstNotes->setScrolling(true);
		}
		else
		{
			// any row except for the last
			if (_selectedRow >= 0 && _selectedRow < _lstNotes->getLastRowIndex())
			{
				// delete the selected note
				_selectedNote = "";
				_lstNotes->setCellText(_selectedRow, 0, _selectedNote);
			}
		}
	}
	else if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		// temporarily set to empty during editing
		_lstNotes->setCellText(_selectedRow, 0, "");

		// set the initial text for editing
		if (_selectedRow == _lstNotes->getLastRowIndex())
		{
			_edtNote->setText("");
		}
		else
		{
			_edtNote->setText(_selectedNote);
		}
		_edtNote->setX(_lstNotes->getColumnX(0));
		_edtNote->setY(_lstNotes->getRowY(_selectedRow));
		_edtNote->setVisible(true);
		_edtNote->setFocus(true, false);
		_lstNotes->setScrolling(false);
	}
}

/**
 * Updates the currently edited note.
 * @param action Pointer to an action.
 */
void NotesState::edtNoteKeyPress(Action* action)
{
#ifdef __EMSCRIPTEN__
	if (Calypso::CalypsoNotesStateUi::editKeyPress(*this, action)) return;
#endif

	if (action->getDetails()->key.keysym.sym == SDLK_RETURN ||
		action->getDetails()->key.keysym.sym == SDLK_KP_ENTER)
	{
		// update the selected note
		_selectedNote = _edtNote->getText();
		_lstNotes->setCellText(_selectedRow, 0, _selectedNote);

		// clean up
		_edtNote->setText("");
		_edtNote->setVisible(false);
		_edtNote->setFocus(false, false);
		_lstNotes->setScrolling(true);

		// if we're adding a new note...
		if (_selectedRow == _lstNotes->getLastRowIndex())
		{
			// change color to normal
			_lstNotes->setRowColor(_lstNotes->getLastRowIndex(), _lstNotes->getColor());

			// add a new empty note
			_lstNotes->addRow(1, tr("STR_NEW_NOTE").c_str());
			if (_origin != OPT_BATTLESCAPE)
			{
				_lstNotes->setRowColor(_lstNotes->getLastRowIndex(), _lstNotes->getSecondaryColor());
			}
			_lstNotes->scrollDown(true);
		}
	}
}

/**
 * Opens a fresh inline row from the explicit touch-sized action.
 */
void NotesState::btnNewClick(Action* action)
{
#ifdef __EMSCRIPTEN__
	if (Calypso::CalypsoNotesStateUi::newClick(*this, action)) return;
#endif
}

/**
 * Deletes the row selected through the visible secondary action.
 */
void NotesState::btnDeleteClick(Action* action)
{
#ifdef __EMSCRIPTEN__
	if (Calypso::CalypsoNotesStateUi::deleteClick(*this, action)) return;
#endif
}

/**
 * Closes delete confirmation without mutating the notebook working copy.
 */
void NotesState::btnKeepClick(Action* action)
{
#ifdef __EMSCRIPTEN__
	if (Calypso::CalypsoNotesStateUi::keepClick(*this, action)) return;
#endif
}

/**
 * Saves all changes and returns to the previous screen.
 * @param action Pointer to an action.
 */
void NotesState::btnSaveClick(Action*)
{
	if (_hdLayout)
	{
		// Save is never a focused-editor no-op: apply the active row first,
		// then atomically replace the campaign notebook from the working copy.
		applyHdEdit(nullptr);
		_game->getSavedGame()->getUserNotes() = Calypso::calypsoCommitNotes(
			_workingNotes, _selectedRow, _edtNote->getVisible(), _edtNote->getText());
		_game->popState();
		return;
	}

	if (_edtNote->isFocused())
	{
		// edit still in progress
		return;
	}

	// overwrite everything, no way back :)
	auto& notes = _game->getSavedGame()->getUserNotes();
	notes.clear();
	if (_lstNotes->getTexts() > 1)
	{
		// ignore last row
		for (int i = 0; i < _lstNotes->getLastRowIndex(); ++i)
		{
			std::string note = _lstNotes->getCellText(i, 0);
			if (!note.empty())
			{
				notes.push_back(note);
			}
		}
	}

	_game->popState();
}

}
