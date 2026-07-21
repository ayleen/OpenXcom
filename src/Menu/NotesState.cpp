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
#include "../Calypso/CalypsoNotePreview.h"
#include "../Calypso/CalypsoNotesInput.h"
#include "../Calypso/CalypsoNotesUi.h" // Phase 46.2-HD (empty on native)
#ifdef __EMSCRIPTEN__
#include "../Mod/Mod.h"
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
	_hdLayout(false), _hdNotesLoaded(false), _deleteRow(-1), _hdListSelection(0), _focusGeneration(0)
{
	_screen = false;

#ifdef __EMSCRIPTEN__
	_hdLayout = _game && _game->getMod()
		&& _game->getMod()->isHdUiFamilyEnabled("F34");
#endif

	// Create objects
	if (_hdLayout)
	{
		_window = new Window(this, 724, 344, 8, 8, POPUP_NONE);
		_txtTitle = new Text(708, 32, 16, 16);
		_txtDelete = new Text(212, 38, 512, 152);
		_lstNotes = new TextList(490, 198, 16, 102);
		_edtNote = new TextEdit(this, 424, 44, 0, 0);
		_btnSave = new TextButton(112, 44, 612, 308);
		_btnCancel = new TextButton(96, 44, 512, 308);
		_btnDelete = new ToggleTextButton(102, 44, 622, 198);
		_btnNew = new TextButton(212, 44, 512, 102);
		_btnKeep = new TextButton(102, 44, 512, 198);
		_txtOriginGeo = new Text(346, 44, 16, 52);
		_txtOriginBattle = new Text(346, 44, 378, 52);
	}
	else
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
	if (_hdLayout)
	{
		add(_btnNew, "button", "noteMenu");
		add(_btnKeep, "button", "noteMenu");
		add(_txtOriginGeo, "text", "noteMenu");
		add(_txtOriginBattle, "text", "noteMenu");
	}

	centerAllSurfaces();

#ifdef __EMSCRIPTEN__
	if (_hdLayout)
	{
		_hdFont = _game->getMod()->getTTFFont("FONT_HD_HUD", false);
		enableUiScaling(740, 360, 1.0f);
	}
#endif

	// Set up objects
	if (!_hdLayout)
	{
		setWindowBackground(_window, "noteMenu");
	}

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_NOTES"));

	if (_hdLayout)
	{
		_txtDelete->setWordWrap(true);
		_txtDelete->setText("");
		_txtOriginGeo->setAlign(ALIGN_CENTER);
		_txtOriginGeo->setVerticalAlign(ALIGN_MIDDLE);
		std::string geoLabel = tr("STR_GEOSCAPE");
		if (_origin == OPT_GEOSCAPE) geoLabel += " · " + std::string(tr("STR_CAL_NOTES_ACTIVE"));
		_txtOriginGeo->setText(geoLabel);
		_txtOriginBattle->setAlign(ALIGN_CENTER);
		_txtOriginBattle->setVerticalAlign(ALIGN_MIDDLE);
		std::string battleLabel = tr("STR_BATTLESCAPE");
		if (_origin == OPT_BATTLESCAPE) battleLabel += " · " + std::string(tr("STR_CAL_NOTES_ACTIVE"));
		_txtOriginBattle->setText(battleLabel);
		if (_origin == OPT_GEOSCAPE)
			_txtOriginGeo->setColor(_lstNotes->getSecondaryColor());
		else
			_txtOriginBattle->setColor(_lstNotes->getSecondaryColor());
	}
#ifdef __MOBILE__
	else
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

	if (_hdLayout)
	{
		_lstNotes->setColumns(2, 430, 44);
		_lstNotes->setAlign(ALIGN_CENTER, 1);
		_lstNotes->setMinimumRowHeight(44);
	}
	else
	{
		_lstNotes->setColumns(1, 288);
	}
	_lstNotes->setSelectable(true);
	_lstNotes->setBackground(_window);
	_lstNotes->setMargin(8);
	// HD rows are fixed-height touch targets. Keep their previews to one clipped
	// line so embedded newlines/long text cannot create duplicate TextList row
	// mappings and shift hit-testing for every note below. The inline editor
	// remains multiline; the legacy list keeps its original defensive wrapping.
	_lstNotes->setWordWrap(!_hdLayout);
	_lstNotes->onMousePress((ActionHandler)&NotesState::lstNotesPress);

	_edtNote->setColor(_lstNotes->getSecondaryColor());
	_edtNote->setVisible(false);
	_edtNote->onKeyboardPress((ActionHandler)&NotesState::edtNoteKeyPress);
	if (_hdLayout)
	{
		_edtNote->setMultiline(true);
		_edtNote->setEnterPolicy(TEEP_COMMIT);
		_edtNote->setDrawBackground(true);
		_edtNote->onEnter((ActionHandler)&NotesState::applyHdEdit);
	}

	_btnSave->setText(tr("STR_SAVE_UC"));
	_btnSave->onMouseClick((ActionHandler)&NotesState::btnSaveClick);
	//_btnSave->onKeyboardPress((ActionHandler)&NotesState::btnSaveClick, Options::keyOk);

	_btnCancel->setText(tr("STR_CANCEL_UC"));
	_btnCancel->onMouseClick((ActionHandler)&NotesState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&NotesState::btnCancelClick, Options::keyCancel);

	if (_hdLayout)
	{
		_btnNew->setText(tr("STR_NEW_NOTE"));
		_btnNew->onMouseClick((ActionHandler)&NotesState::btnNewClick);
		_btnDelete->setText(tr("STR_DELETE"));
		_btnDelete->onMouseClick((ActionHandler)&NotesState::btnDeleteClick);
		_btnDelete->setVisible(false);
		_btnKeep->setText(tr("STR_CAL_NOTES_KEEP"));
		_btnKeep->onMouseClick((ActionHandler)&NotesState::btnKeepClick);
		_btnKeep->setVisible(false);
		enableCalypsoFocus();
		rebuildHdFocus();
		restoreCalypsoFocus("notes.list", _focusGeneration);
		applyHdVisualStyle();
	}

#ifdef __EMSCRIPTEN__
	// Phase 46.2-HD: attach the physical-resolution overlay adapter (no-op unless
	// _hdLayout). It only reads these HD widgets and draws crisp physical
	// replacements; all Notes logic above stays as-is.
	Calypso::CalypsoNotesUi::configure(*this);
#endif
}

/**
 *
 */
NotesState::~NotesState()
{
#ifdef __EMSCRIPTEN__
	// The adapter's destructor unregisters it from the overlay iff still active.
	delete _hdAdapter;
	_hdAdapter = nullptr;
#endif
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

	if (_hdLayout)
	{
		if (!_hdNotesLoaded)
		{
			_workingNotes = _game->getSavedGame()->getUserNotes();
			_originalNotes = _workingNotes;
			_hdNotesLoaded = true;
		}
		updateHdList();
		updateHdStatus();
		applyHdVisualStyle();
	}
	else
	{
		updateList();
	}
}

void NotesState::applyHdVisualStyle()
{
	if (!_hdLayout) return;

	// State-local dark navy/green ramp matching the approved Calypso HD shell.
	const SDL_Color hdRamp[16] = {
		{11, 51, 40, 255}, {23, 62, 49, 255}, {32, 81, 63, 255}, {14, 41, 31, 255},
		{7, 26, 21, 255}, {116, 255, 176, 255}, {3, 16, 21, 255}, {16, 42, 34, 255},
		{27, 75, 59, 255}, {38, 106, 82, 255}, {18, 55, 43, 255}, {8, 32, 25, 255},
		{159, 255, 201, 255}, {232, 255, 242, 255}, {136, 170, 160, 255}, {255, 118, 111, 255}
	};
	setStatePalette(hdRamp, 240, 16);
	// setStatePalette only updates State::_palette. These controls were already
	// added under the ruleset palette, so cascade the complete state-local
	// palette through every surface (and composite child via virtual setPalette)
	// before drawing with indices 240..255.
	for (Surface* surface : _surfaces)
	{
		surface->setPalette(getPalette());
		surface->invalidate();
	}

	_window->setThinBorder();
	_window->setColor(240);
	_window->setInnerColor(246);
	_txtTitle->setColorRGB(0xFF74FFB0u);
	_txtOriginGeo->setColorRGB(_origin == OPT_GEOSCAPE ? 0xFF74FFB0u : 0xFF88AAA0u);
	_txtOriginBattle->setColorRGB(_origin == OPT_BATTLESCAPE ? 0xFF74FFB0u : 0xFF88AAA0u);
	_txtDelete->setColorRGB(_deleteRow >= 0 ? 0xFFFF766Fu : 0xFF88AAA0u);

	TextButton* buttons[] = {_btnNew, _btnKeep, _btnDelete, _btnCancel, _btnSave};
	for (TextButton* button : buttons)
	{
		button->setColor(240);
		button->setTextColorRGB(0xFFE8FFF2u);
	}
	_btnNew->setTextColorRGB(0xFF74FFB0u);
	_btnDelete->setTextColorRGB(0xFFFF766Fu);
	_btnSave->setTextColorRGB(0xFF74FFB0u);
	_edtNote->setColor(248); // dark field; direct TTF resolves to bright ramp index 252.

#ifdef __EMSCRIPTEN__
	if (_hdFont)
	{
		_txtTitle->setTTFFont(_hdFont, 0.58f);
		_txtOriginGeo->setTTFFont(_hdFont, 0.38f);
		_txtOriginBattle->setTTFFont(_hdFont, 0.38f);
		_txtDelete->setTTFFont(_hdFont, 0.34f);
		_btnNew->setTTFFont(_hdFont, 0.38f);
		_btnKeep->setTTFFont(_hdFont, 0.36f);
		_btnDelete->setTTFFont(_hdFont, 0.34f);
		_btnCancel->setTTFFont(_hdFont, 0.34f);
		_btnSave->setTTFFont(_hdFont, 0.36f);
		_lstNotes->setTTFFont(_hdFont, 0.42f);
		_edtNote->setTTFFont(_hdFont, 0.42f);
	}
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
	const std::string focusedBefore = getCalypsoFocusedId() ? *getCalypsoFocusedId() : "";
	if (_hdLayout && !_edtNote->isFocused() && action->getDetails()->type == SDL_KEYDOWN)
	{
		const std::string* focused = getCalypsoFocusedId();
		const SDL_Keymod mod = static_cast<SDL_Keymod>(action->getDetails()->key.keysym.mod);
		if (focused && *focused == "notes.list" && (mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)) == 0)
		{
			const int last = static_cast<int>(_workingNotes.size());
			int next = _hdListSelection;
			bool recognized = true;
			switch (action->getDetails()->key.keysym.sym)
			{
			case SDLK_UP:   next = std::max(0, next - 1); break;
			case SDLK_DOWN: next = std::min(last, next + 1); break;
			case SDLK_HOME: next = 0; break;
			case SDLK_END:  next = last; break;
			default: recognized = false; break;
			}
			if (recognized)
			{
				_hdListSelection = next;
				_lstNotes->setSelectedRow(static_cast<size_t>(next));
				updateHdListSelectionIndicator();
				action->getDetails()->type = SDL_NOEVENT;
				return;
			}
		}
	}
	if (_hdLayout && _edtNote->isFocused()
		&& action->getDetails()->type == SDL_MOUSEBUTTONDOWN)
	{
		const double x = action->getAbsoluteXMouse();
		const double y = action->getAbsoluteYMouse();
		auto inside = [x, y](Surface* surface) {
			return surface && x >= surface->getX() && x < surface->getX() + surface->getWidth()
				&& y >= surface->getY() && y < surface->getY() + surface->getHeight();
		};
		if (!inside(_edtNote))
		{
			if (inside(_btnCancel))
			{
				// Preserve normal press/release/click dispatch (and avoid a
				// mouse-up leaking into the underlying state) while discarding
				// the active row before outer Cancel closes the notebook.
				cancelHdEdit();
			}
			else
			{
				// Save and all other outside actions apply the row first; the
				// same press then continues through ordinary hit testing.
				applyHdEdit(action);
			}
		}
	}
	State::handle(action);
	const std::string focusedAfter = getCalypsoFocusedId() ? *getCalypsoFocusedId() : "";
	if (_hdLayout && focusedBefore != focusedAfter) updateHdListSelectionIndicator();
}

/**
 * Re-applies browser HD scaling while preserving the exact native fallback.
 */
void NotesState::resize(int &dX, int &dY)
{
#ifdef __EMSCRIPTEN__
	if (_hdLayout)
	{
		applyUiScaling();
		positionHdEditor();
		return;
	}
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
	_lstNotes->clearList();
	for (size_t i = 0; i < _workingNotes.size(); ++i)
	{
		const bool editing = _edtNote->getVisible() && _selectedRow == static_cast<int>(i);
		const std::string preview = Unicode::convUtf32ToUtf8(Calypso::calypsoNotePreview(
			Unicode::convUtf8ToUtf32(_workingNotes[i]), 52));
		_lstNotes->addRow(2, editing ? " " : preview.c_str(), "...");
		_lstNotes->setCellColorRGB(i, 0, 0xFFE8FFF2u);
		_lstNotes->setCellColorRGB(i, 1, 0xFF74FFB0u);
	}
	_lstNotes->addRow(2, tr("STR_NEW_NOTE").c_str(), "");
	if (_origin != OPT_BATTLESCAPE)
	{
		_lstNotes->setRowColor(_lstNotes->getLastRowIndex(), _lstNotes->getSecondaryColor());
	}
	_lstNotes->setRowColorRGB(_lstNotes->getLastRowIndex(), 0xFF74FFB0u);
	_hdListSelection = std::max(0, std::min(_hdListSelection,
		static_cast<int>(_workingNotes.size())));
	_lstNotes->setSelectedRow(static_cast<size_t>(_hdListSelection));
	updateHdListSelectionIndicator();
#ifdef __EMSCRIPTEN__
	// clearList/addRow must never regress the HD list back to tiny bitmap glyphs.
	if (_hdFont) _lstNotes->setTTFFont(_hdFont, 0.42f);
#endif
	if (_selectedRow >= 0 && _selectedRow < _lstNotes->getLastRowIndex())
	{
		_lstNotes->scrollTo(static_cast<size_t>(_selectedRow));
	}
}

void NotesState::updateHdListSelectionIndicator()
{
	if (!_hdLayout || _lstNotes->getTexts() == 0) return;
	const std::string* focused = getCalypsoFocusedId();
	const bool listFocused = focused && *focused == "notes.list";
	for (size_t row = 0; row < _lstNotes->getTexts(); ++row)
	{
		const bool selected = row == static_cast<size_t>(_hdListSelection);
		const bool existing = row < _workingNotes.size();
		_lstNotes->setCellText(row, 1,
			Calypso::calypsoNotesSelectionMarker(selected, listFocused, existing));
	}
}

/**
 * Starts one inline edit. Existing edits are applied before moving to another
 * row, matching direct row activation on mouse, keyboard and touch.
 */
void NotesState::beginHdEdit(int row)
{
	if (row < 0 || row > static_cast<int>(_workingNotes.size())) return;
	if (_edtNote->isFocused()) applyHdEdit(nullptr);
	_deleteRow = -1;
	_btnDelete->setPressed(false);
	_btnDelete->setVisible(false);
	_btnKeep->setVisible(false);
	updateHdStatus();
	rebuildHdFocus();
	_selectedRow = row;
	_hdListSelection = row;
	_selectedNote = row < static_cast<int>(_workingNotes.size()) ? _workingNotes[row] : "";
	_edtNote->setText(_selectedNote);
	_edtNote->setVisible(true);
	updateHdList();
	_lstNotes->setScrolling(true);
	_lstNotes->setSelectedRow(static_cast<size_t>(row));
	positionHdEditor();
	_lstNotes->setScrolling(false);
	_edtNote->setFocus(true, true);
}

/**
 * Positions the editor from live list/scroll geometry. This deliberately runs
 * after each rebuild and responsive reflow rather than retaining stale row
 * coordinates captured at construction.
 */
void NotesState::positionHdEditor()
{
	if (!_hdLayout || !_edtNote->getVisible() || _selectedRow < 0) return;
	const size_t row = static_cast<size_t>(_selectedRow);
	const size_t scroll = _lstNotes->getScroll();
	if (row > static_cast<size_t>(_lstNotes->getLastRowIndex())) return;
	_edtNote->setX(_lstNotes->getColumnX(0));
	if (scroll <= row && scroll <= static_cast<size_t>(_lstNotes->getLastRowIndex()))
	{
		_edtNote->setY(_lstNotes->getY()
			+ _lstNotes->getRowY(row) - _lstNotes->getRowY(scroll));
	}
}

/**
 * Registers stable, localized-control-independent focus identifiers. The
 * editor itself becomes the State modal while active, so Enter/Shift+Enter and
 * Escape retain TextEdit ownership instead of being intercepted as actions.
 */
void NotesState::rebuildHdFocus()
{
	if (!_hdLayout) return;
	++_focusGeneration;
	std::vector<Calypso::CalypsoFocusBinding> bindings;
	bindings.push_back({{"notes.list", true, true}, _lstNotes, [this]() {
		const int row = Calypso::calypsoNotesActivationRow(
			_hdListSelection, static_cast<int>(_lstNotes->getSelectedRow()));
		if (row < 0 || row > static_cast<int>(_workingNotes.size())) return false;
		beginHdEdit(row);
		return true;
	}});
	bindings.push_back({{"notes.new", true, true}, _btnNew, [this]() {
		btnNewClick(nullptr);
		return true;
	}});
	bindings.push_back({{"notes.keep", _btnKeep->getVisible(), _deleteRow >= 0}, _btnKeep, [this]() {
		btnKeepClick(nullptr);
		return true;
	}});
	bindings.push_back({{"notes.delete", _btnDelete->getVisible(), _deleteRow >= 0}, _btnDelete, [this]() {
		btnDeleteClick(nullptr);
		return true;
	}});
	bindings.push_back({{"notes.cancel", true, true}, _btnCancel, [this]() {
		btnCancelClick(nullptr);
		return true;
	}});
	bindings.push_back({{"notes.save", true, true}, _btnSave, [this]() {
		btnSaveClick(nullptr);
		return true;
	}});
	(void)rebuildCalypsoFocus(std::move(bindings), _focusGeneration);
}

/**
 * Shows confirmation or dirty state as text so neither origin nor mutation
 * state depends on palette color alone.
 */
void NotesState::updateHdStatus()
{
	if (!_hdLayout) return;
	if (_deleteRow >= 0)
	{
		_txtDelete->setText(tr("STR_CAL_NOTES_DELETE_PROMPT"));
		_txtDelete->setColorRGB(0xFFFF766Fu);
	}
	else if (_workingNotes != _originalNotes)
	{
		_txtDelete->setText(tr("STR_CAL_NOTES_UNSAVED"));
		_txtDelete->setColorRGB(0xFF88AAA0u);
	}
	else
	{
		_txtDelete->setText("");
		_txtDelete->setColorRGB(0xFF88AAA0u);
	}
}

void NotesState::invalidateHdEditorArea()
{
	if (!_hdLayout) return;
	// The editor is an overlapping surface. Rebuild both owners beneath it when
	// it disappears so cached ARGB pixels cannot survive an Escape/right-click.
	_window->invalidate();
	_lstNotes->invalidate();
	_txtDelete->invalidate();
}

/**
 * Applies the active editor to the private working copy. Empty existing rows
 * remain stable while editing and are filtered only by Save; an empty new row
 * is simply abandoned.
 */
void NotesState::applyHdEdit(Action*)
{
	// TextEdit::commit deliberately clears focus before invoking onEnter, so
	// selected-row + visibility is the durable edit-active contract here.
	if (!_hdLayout || _selectedRow < 0 || !_edtNote->getVisible()) return;
	const std::string value = _edtNote->getText();
	const int row = _selectedRow;
	if (_edtNote->isFocused()) _edtNote->setFocus(false);
	_edtNote->setVisible(false);
	_edtNote->setText("");
	invalidateHdEditorArea();
	_lstNotes->setScrolling(true);
	if (row >= 0 && row < static_cast<int>(_workingNotes.size()))
	{
		_workingNotes[row] = value;
	}
	else if (row == static_cast<int>(_workingNotes.size()) && !value.empty())
	{
		_workingNotes.push_back(value);
	}
	_selectedRow = -1;
	_selectedNote.clear();
	updateHdList();
	updateHdStatus();
	rebuildHdFocus();
}

/**
 * Cancels only the active row edit; notebook-level Cancel remains separate.
 */
void NotesState::cancelHdEdit()
{
	if (!_edtNote->isFocused()) return;
	_edtNote->setFocus(false);
	_edtNote->setVisible(false);
	_edtNote->setText("");
	invalidateHdEditorArea();
	_lstNotes->setScrolling(true);
	_selectedRow = -1;
	_selectedNote.clear();
	updateHdList();
	updateHdStatus();
	rebuildHdFocus();
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
	if (_hdLayout)
	{
		const Uint8 button = action->getDetails()->button.button;
		if (button != SDL_BUTTON_LEFT && button != SDL_BUTTON_RIGHT) return;
		const int row = static_cast<int>(_lstNotes->getSelectedRow());
		if (row < 0 || row > static_cast<int>(_workingNotes.size())) return;
		_hdListSelection = row;
		updateHdListSelectionIndicator();
		// Compare in one coordinate space. Normalize the absolute pointer against
		// the scaled list itself instead of mixing it with child-row coordinates
		// rebuilt by TextList (438/490 is margin+col0 in the design geometry).
		const double localX = action->getAbsoluteXMouse() - _lstNotes->getX();
		const bool actionColumn = row < static_cast<int>(_workingNotes.size())
			&& localX >= _lstNotes->getWidth() * (438.0 / 490.0);
		if (row < static_cast<int>(_workingNotes.size())
			&& (button == SDL_BUTTON_RIGHT || actionColumn))
		{
			cancelHdEdit();
			_deleteRow = row;
			_btnDelete->setVisible(true);
			_btnKeep->setVisible(true);
			updateHdStatus();
			rebuildHdFocus();
			restoreCalypsoFocus("notes.keep", _focusGeneration);
			return;
		}
		beginHdEdit(row);
		return;
	}

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
	if (_hdLayout)
	{
		if (action->getDetails()->key.keysym.sym == SDLK_ESCAPE)
		{
			cancelHdEdit();
			// The edit owns this Escape. Prevent the notebook-level Cancel binding
			// from observing the same key event later in the surface dispatch pass.
			action->getDetails()->type = SDL_NOEVENT;
		}
		return;
	}

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
void NotesState::btnNewClick(Action*)
{
	if (_hdLayout) beginHdEdit(static_cast<int>(_workingNotes.size()));
}

/**
 * Deletes the row selected through the visible secondary action.
 */
void NotesState::btnDeleteClick(Action*)
{
	if (!_hdLayout) return;
	if (_deleteRow >= 0 && _deleteRow < static_cast<int>(_workingNotes.size()))
	{
		_workingNotes.erase(_workingNotes.begin() + _deleteRow);
	}
	_deleteRow = -1;
	_btnDelete->setPressed(false);
	_btnDelete->setVisible(false);
	_btnKeep->setVisible(false);
	updateHdStatus();
	rebuildHdFocus();
	updateHdList();
}

/**
 * Closes delete confirmation without mutating the notebook working copy.
 */
void NotesState::btnKeepClick(Action*)
{
	if (!_hdLayout) return;
	_deleteRow = -1;
	_btnDelete->setPressed(false);
	_btnDelete->setVisible(false);
	_btnKeep->setVisible(false);
	updateHdStatus();
	rebuildHdFocus();
	restoreCalypsoFocus("notes.list", _focusGeneration);
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
		auto& notes = _game->getSavedGame()->getUserNotes();
		notes.clear();
		for (const auto& note : _workingNotes)
		{
			if (!note.empty()) notes.push_back(note);
		}
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
