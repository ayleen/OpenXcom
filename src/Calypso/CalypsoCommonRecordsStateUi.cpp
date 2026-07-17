#ifdef __EMSCRIPTEN__
/* F34 browser-only layout adapter. Do not add gameplay or save mutation here. */
#include "CalypsoCommonRecordsStateUi.h"

#include <algorithm>
#include <vector>

#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Engine/Unicode.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Interface/ToggleTextButton.h"
#include "../Interface/Window.h"
#include "../Menu/ErrorMessageState.h"
#include "../Menu/NotesState.h"
#include "../Menu/StatisticsState.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"
#include "CalypsoFocusCoordinator.h"
#include "CalypsoCommonRecords.h"
#include "CalypsoNotePreview.h"
#include "CalypsoNotesInput.h"
#include "CalypsoTutorial.h"
#include "CalypsoViewportRuntime.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

CalypsoLayoutClass currentF34LayoutClass()
{
	const CalypsoViewportRuntime& viewport = calypsoViewportRuntime();
	if (viewport.hasLayout()) return viewport.current().layoutClass;
	return calypsoClassifySafeArea(Options::baseXResolution, Options::baseYResolution);
}

void applyF34Rect(Surface* surface, const CalypsoF34Rect& rect)
{
	surface->setX(rect.x);
	surface->setY(rect.y);
	surface->setWidth(rect.width);
	surface->setHeight(rect.height);
}

int scaledF34Metric(int value, int actualExtent, int designExtent)
{
	if (value <= 0 || actualExtent <= 0 || designExtent <= 0) return value;
	return std::max(1, static_cast<int>(
		static_cast<double>(value) * actualExtent / designExtent + 0.5));
}

} // namespace

void CalypsoErrorMessageStateUi::applyLayout(ErrorMessageState& state)
{
	const CalypsoF34ErrorLayout layout = calypsoF34ErrorLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
	applyF34Rect(state._window, layout.window);
	applyF34Rect(state._txtMessage, layout.message);
	applyF34Rect(state._btnOk, layout.acknowledge);
	state.recaptureUiScaling(layout.designWidth, layout.designHeight, 1.0f, true);
}

void CalypsoErrorMessageStateUi::configure(ErrorMessageState& state)
{
	if (!state._hdLayout) return;
	// The caller's palette, background and contrast were installed before this
	// adapter runs. Only geometry, typography and semantic focus change here.
	state._hdWideLayout = currentF34LayoutClass() == CalypsoLayoutClass::Wide;
	applyLayout(state);
	state._window->setThinBorder();
	state._txtMessage->setWordWrap(true);
	state._hdFont = state._game->getMod()->getTTFFont("FONT_HD_HUD", false);
	if (state._hdFont)
	{
		state._txtMessage->setTTFFont(state._hdFont, 0.42f);
		state._btnOk->setTTFFont(state._hdFont, 0.40f);
	}
	state.enableCalypsoFocus();
	++state._focusGeneration;
	std::vector<CalypsoFocusBinding> bindings;
	bindings.push_back({{"error.acknowledge", true, true}, state._btnOk, [&state]() {
		state.btnOkClick(nullptr);
		return true;
	}});
	(void)state.rebuildCalypsoFocus(std::move(bindings), state._focusGeneration);
	state.restoreCalypsoFocus("error.acknowledge", state._focusGeneration);
	refreshAnchors(state);
}

bool CalypsoErrorMessageStateUi::resize(ErrorMessageState& state)
{
	if (!state._hdLayout) return false;
	const bool wide = currentF34LayoutClass() == CalypsoLayoutClass::Wide;
	if (wide != state._hdWideLayout)
	{
		state._hdWideLayout = wide;
		applyLayout(state);
	}
	else state.applyUiScaling();
	refreshAnchors(state);
	return true;
}

void CalypsoErrorMessageStateUi::refreshAnchors(ErrorMessageState& state)
{
	if (!state._hdLayout) return;
	CalypsoTutorial::get().anchorAll({{"error.acknowledge", state._btnOk}});
}

void CalypsoStatisticsStateUi::applyLayout(StatisticsState& state)
{
	const CalypsoF34StatisticsLayout layout = calypsoF34StatisticsLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
	applyF34Rect(state._window, layout.window);
	applyF34Rect(state._txtTitle, layout.title);
	applyF34Rect(state._lstStats, layout.list);
	applyF34Rect(state._btnOk, layout.acknowledge);
	applyF34Rect(state._btnScrollUp, layout.scrollUp);
	applyF34Rect(state._btnScrollDown, layout.scrollDown);
	state.recaptureUiScaling(layout.designWidth, layout.designHeight, 1.0f, true);
}

void CalypsoStatisticsStateUi::rebuildList(StatisticsState& state, std::size_t scroll)
{
	const CalypsoF34StatisticsLayout layout = calypsoF34StatisticsLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
	const int actualWidth = state._lstStats->getWidth();
	state._lstStats->clearList();
	state._lstStats->setColumns(2,
		scaledF34Metric(layout.labelColumnWidth, actualWidth, layout.list.width),
		scaledF34Metric(layout.valueColumnWidth, actualWidth, layout.list.width));
	state._lstStats->setMinimumRowHeight(
		scaledF34Metric(layout.rowHeight, actualWidth, layout.list.width));
	state.listStats();
	state._lstStats->scrollTo(scroll);
}

void CalypsoStatisticsStateUi::configure(StatisticsState& state)
{
	if (!state._hdLayout) return;
	state._window->setThinBorder();
	state._lstStats->setSelectable(true);
	// The scrollbar occupies x=548..561; F34's 154px manual controls begin
	// at x=562, so both hit targets remain distinct at the design resolution.
	state._lstStats->setScrolling(true, 0);
	state._btnScrollUp = new TextButton(154, 44, 562, 188);
	state._btnScrollDown = new TextButton(154, 44, 562, 236);
	state.add(state._btnScrollUp, "button", "endGameStatistics");
	state.add(state._btnScrollDown, "button", "endGameStatistics");
	state._btnScrollUp->setText(state.tr("STR_SCROLL_UP"));
	state._btnScrollDown->setText(state.tr("STR_SCROLL_DOWN"));
	state._btnScrollUp->onMouseClick((ActionHandler)&StatisticsState::btnScrollUpClick);
	state._btnScrollDown->onMouseClick((ActionHandler)&StatisticsState::btnScrollDownClick);
	state._hdFont = state._game->getMod()->getTTFFont("FONT_HD_HUD", false);
	if (state._hdFont)
	{
		state._txtTitle->setTTFFont(state._hdFont, 0.46f);
		state._lstStats->setTTFFont(state._hdFont, 0.36f);
		state._btnOk->setTTFFont(state._hdFont, 0.40f);
		state._btnScrollUp->setTTFFont(state._hdFont, 0.30f);
		state._btnScrollDown->setTTFFont(state._hdFont, 0.30f);
	}
	state._hdWideLayout = currentF34LayoutClass() == CalypsoLayoutClass::Wide;
	applyLayout(state);
	rebuildList(state, 0);
	state.enableCalypsoFocus();
	++state._focusGeneration;
	std::vector<CalypsoFocusBinding> bindings;
	bindings.push_back({{"statistics.list", true, true}, state._lstStats, []() { return true; }});
	bindings.push_back({{"statistics.return", true, true}, state._btnOk, [&state]() {
		state.btnOkClick(nullptr);
		return true;
	}});
	bindings.push_back({{"statistics.scroll.up", true, true}, state._btnScrollUp, [&state]() {
		scrollUp(state);
		return true;
	}});
	bindings.push_back({{"statistics.scroll.down", true, true}, state._btnScrollDown, [&state]() {
		scrollDown(state);
		return true;
	}});
	(void)state.rebuildCalypsoFocus(std::move(bindings), state._focusGeneration);
	state.restoreCalypsoFocus("statistics.list", state._focusGeneration);
	refreshAnchors(state);
}

bool CalypsoStatisticsStateUi::resize(StatisticsState& state)
{
	if (!state._hdLayout) return false;
	const std::size_t scroll = state._lstStats->getScroll();
	const bool wide = currentF34LayoutClass() == CalypsoLayoutClass::Wide;
	if (wide != state._hdWideLayout)
	{
		state._hdWideLayout = wide;
		applyLayout(state);
	}
	else state.applyUiScaling();
	rebuildList(state, scroll);
	refreshAnchors(state);
	return true;
}

void CalypsoStatisticsStateUi::scrollUp(StatisticsState& state)
{
	if (state._hdLayout) state._lstStats->scrollUp(false);
}

void CalypsoStatisticsStateUi::scrollDown(StatisticsState& state)
{
	if (state._hdLayout) state._lstStats->scrollDown(false);
}

void CalypsoStatisticsStateUi::refreshAnchors(StatisticsState& state)
{
	if (!state._hdLayout) return;
	CalypsoTutorial::get().anchorAll({
		{"statistics.list", state._lstStats},
		{"statistics.scroll", state._btnScrollUp, state._btnScrollDown},
		{"statistics.return", state._btnOk}
	});
}

bool CalypsoStatisticsStateUi::handle(StatisticsState& state, Action* action)
{
	if (!state._hdLayout || action->getDetails()->type != SDL_KEYDOWN) return false;
	const std::string* focused = state.getCalypsoFocusedId();
	if (!focused || *focused != "statistics.list") return false;
	switch (action->getDetails()->key.keysym.sym)
	{
	case SDLK_UP: scrollUp(state); break;
	case SDLK_DOWN: scrollDown(state); break;
	case SDLK_PAGEUP: state._lstStats->scrollUp(false, false, state._lstStats->getVisibleRows()); break;
	case SDLK_PAGEDOWN: state._lstStats->scrollDown(false, false, state._lstStats->getVisibleRows()); break;
	case SDLK_HOME: state._lstStats->scrollUp(true); break;
	case SDLK_END: state._lstStats->scrollDown(true); break;
	default: return false;
	}
	action->getDetails()->type = SDL_NOEVENT;
	return true;
}

bool CalypsoNotesStateUi::createControls(NotesState& state)
{
	state._hdLayout = state._game && state._game->getMod()
		&& state._game->getMod()->isHdUiFamilyEnabled("F34");
	if (!state._hdLayout) return false;

	state._window = new Window(&state, 724, 344, 8, 8, POPUP_NONE);
	state._txtTitle = new Text(708, 32, 16, 16);
	state._txtDelete = new Text(212, 38, 512, 152);
	state._lstNotes = new TextList(490, 198, 16, 102);
	state._edtNote = new TextEdit(&state, 424, 44, 0, 0);
	state._btnSave = new TextButton(112, 44, 612, 308);
	state._btnCancel = new TextButton(96, 44, 512, 308);
	state._btnDelete = new ToggleTextButton(102, 44, 622, 198);
	state._btnNew = new TextButton(212, 44, 512, 102);
	state._btnKeep = new TextButton(102, 44, 512, 198);
	state._txtOriginGeo = new Text(346, 44, 16, 52);
	state._txtOriginBattle = new Text(346, 44, 378, 52);
	return true;
}

void CalypsoNotesStateUi::addControls(NotesState& state)
{
	if (!state._hdLayout) return;
	state.add(state._btnNew, "button", "noteMenu");
	state.add(state._btnKeep, "button", "noteMenu");
	state.add(state._txtOriginGeo, "text", "noteMenu");
	state.add(state._txtOriginBattle, "text", "noteMenu");
}

void CalypsoNotesStateUi::configureControls(NotesState& state)
{
	if (!state._hdLayout) return;

	state._hdFont = state._game->getMod()->getTTFFont("FONT_HD_HUD", false);

	state._txtDelete->setWordWrap(true);
	state._txtDelete->setText("");
	state._txtOriginGeo->setAlign(ALIGN_CENTER);
	state._txtOriginGeo->setVerticalAlign(ALIGN_MIDDLE);
	std::string geoLabel = state.tr("STR_GEOSCAPE");
	if (state._origin == OPT_GEOSCAPE) geoLabel += " / " + std::string(state.tr("STR_CAL_NOTES_ACTIVE"));
	state._txtOriginGeo->setText(geoLabel);
	state._txtOriginBattle->setAlign(ALIGN_CENTER);
	state._txtOriginBattle->setVerticalAlign(ALIGN_MIDDLE);
	std::string battleLabel = state.tr("STR_BATTLESCAPE");
	if (state._origin == OPT_BATTLESCAPE) battleLabel += " / " + std::string(state.tr("STR_CAL_NOTES_ACTIVE"));
	state._txtOriginBattle->setText(battleLabel);
	if (state._origin == OPT_GEOSCAPE)
		state._txtOriginGeo->setColor(state._lstNotes->getSecondaryColor());
	else
		state._txtOriginBattle->setColor(state._lstNotes->getSecondaryColor());

	state._lstNotes->setAlign(ALIGN_CENTER, 1);
	state._lstNotes->setWordWrap(false);
	state._lstNotes->setScrolling(true, 0);

	state._edtNote->setMultiline(true);
	state._edtNote->setEnterPolicy(TEEP_COMMIT);
	state._edtNote->setDrawBackground(true);
	state._edtNote->onEnter((ActionHandler)&NotesState::applyHdEdit);

	state._btnNew->setText(state.tr("STR_NEW_NOTE"));
	state._btnNew->onMouseClick((ActionHandler)&NotesState::btnNewClick);
	state._btnSave->onKeyboardPress((ActionHandler)&NotesState::btnSaveClick, Options::keyOk);
	state._btnDelete->setText(state.tr("STR_DELETE"));
	state._btnDelete->onMouseClick((ActionHandler)&NotesState::btnDeleteClick);
	state._btnDelete->setVisible(false);
	state._btnKeep->setText(state.tr("STR_CAL_NOTES_KEEP"));
	state._btnKeep->onMouseClick((ActionHandler)&NotesState::btnKeepClick);
	state._btnKeep->setVisible(false);
	state._hdWideLayout = currentF34LayoutClass() == CalypsoLayoutClass::Wide;
	applyLayout(state);
	state.enableCalypsoFocus();
	rebuildFocus(state);
	state.restoreCalypsoFocus("notes.list", state._focusGeneration);
	applyVisualStyle(state);
	refreshAnchors(state);
}

void CalypsoNotesStateUi::applyLayout(NotesState& state)
{
	const CalypsoF34NotesLayout layout = calypsoF34NotesLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
	applyF34Rect(state._window, layout.window);
	applyF34Rect(state._txtTitle, layout.title);
	applyF34Rect(state._txtDelete, layout.status);
	applyF34Rect(state._lstNotes, layout.list);
	applyF34Rect(state._edtNote, layout.editor);
	applyF34Rect(state._btnSave, layout.save);
	applyF34Rect(state._btnCancel, layout.cancel);
	applyF34Rect(state._btnDelete, layout.remove);
	applyF34Rect(state._btnNew, layout.create);
	applyF34Rect(state._btnKeep, layout.keep);
	applyF34Rect(state._txtOriginGeo, layout.originGeoscape);
	applyF34Rect(state._txtOriginBattle, layout.originBattlescape);
	state.recaptureUiScaling(layout.designWidth, layout.designHeight, 1.0f, true);
	applyListMetrics(state);
}

void CalypsoNotesStateUi::applyListMetrics(NotesState& state)
{
	const CalypsoF34NotesLayout layout = calypsoF34NotesLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
	const int actualWidth = state._lstNotes->getWidth();
	state._lstNotes->setColumns(2,
		scaledF34Metric(layout.textColumnWidth, actualWidth, layout.list.width),
		scaledF34Metric(layout.actionColumnWidth, actualWidth, layout.list.width));
	state._lstNotes->setMinimumRowHeight(
		scaledF34Metric(layout.rowHeight, actualWidth, layout.list.width));
}

bool CalypsoNotesStateUi::initialize(NotesState& state)
{
	if (!state._hdLayout) return false;
	if (!state._hdNotesLoaded)
	{
		state._workingNotes = state._game->getSavedGame()->getUserNotes();
		state._originalNotes = state._workingNotes;
		state._hdNotesLoaded = true;
	}
	updateList(state);
	updateStatus(state);
	applyVisualStyle(state);
	return true;
}

void CalypsoNotesStateUi::positionEditor(NotesState& state)
{
	if (!state._hdLayout || !state._edtNote->getVisible() || state._selectedRow < 0) return;
	const size_t row = static_cast<size_t>(state._selectedRow);
	const size_t scroll = state._lstNotes->getScroll();
	if (row > static_cast<size_t>(state._lstNotes->getLastRowIndex())) return;
	state._edtNote->setX(state._lstNotes->getColumnX(0));
	if (scroll <= row && scroll <= static_cast<size_t>(state._lstNotes->getLastRowIndex()))
	{
		state._edtNote->setY(state._lstNotes->getY() + state._lstNotes->getRowY(row) - state._lstNotes->getRowY(scroll));
	}
	refreshAnchors(state);
}

void CalypsoNotesStateUi::updateStatus(NotesState& state)
{
	if (!state._hdLayout) return;
	if (state._deleteRow >= 0)
	{
		state._txtDelete->setText(state.tr("STR_CAL_NOTES_DELETE_PROMPT"));
		state._txtDelete->setColorRGB(0xFFFF766Fu);
	}
	else if (state._workingNotes != state._originalNotes)
	{
		state._txtDelete->setText(state.tr("STR_CAL_NOTES_UNSAVED"));
		state._txtDelete->setColorRGB(0xFF88AAA0u);
	}
	else
	{
		state._txtDelete->setText("");
		state._txtDelete->setColorRGB(0xFF88AAA0u);
	}
	refreshAnchors(state);
}

void CalypsoNotesStateUi::refreshAnchors(NotesState& state)
{
	if (!state._hdLayout) return;
	CalypsoTutorial& tutorial = CalypsoTutorial::get();
	tutorial.eraseAnchor("notes.edit");
	tutorial.eraseAnchor("notes.delete");
	tutorial.eraseAnchor("notes.keep");
	tutorial.anchorAll({
		{"notes.list", state._lstNotes},
		{"notes.new", state._btnNew},
		{"notes.cancel", state._btnCancel},
		{"notes.save", state._btnSave},
		{"notes.dirty", state._txtDelete}
	});
	if (state._edtNote->getVisible()) tutorial.anchorAll({{"notes.edit", state._edtNote}});
	if (state._btnDelete->getVisible()) tutorial.anchorAll({{"notes.delete", state._btnDelete}});
	if (state._btnKeep->getVisible()) tutorial.anchorAll({{"notes.keep", state._btnKeep}});
}

void CalypsoNotesStateUi::invalidateEditorArea(NotesState& state)
{
	if (!state._hdLayout) return;
	state._window->invalidate();
	state._lstNotes->invalidate();
	state._txtDelete->invalidate();
}

bool CalypsoNotesStateUi::resize(NotesState& state)
{
	if (!state._hdLayout) return false;
	const std::size_t scroll = state._lstNotes->getScroll();
	const bool wide = currentF34LayoutClass() == CalypsoLayoutClass::Wide;
	if (wide != state._hdWideLayout)
	{
		state._hdWideLayout = wide;
		applyLayout(state);
	}
	else
	{
		state.applyUiScaling();
		applyListMetrics(state);
	}
	updateList(state);
	state._lstNotes->scrollTo(scroll);
	state._lstNotes->setSelectedRow(static_cast<std::size_t>(state._hdListSelection));
	positionEditor(state);
	refreshAnchors(state);
	return true;
}

void CalypsoNotesStateUi::updateSelection(NotesState& state)
{
	if (!state._hdLayout || state._lstNotes->getTexts() == 0) return;
	const std::string* focused = state.getCalypsoFocusedId();
	const bool listFocused = focused && *focused == "notes.list";
	for (size_t row = 0; row < state._lstNotes->getTexts(); ++row)
	{
		const bool selected = row == static_cast<size_t>(state._hdListSelection);
		const bool existing = row < state._workingNotes.size();
		state._lstNotes->setCellText(row, 1,
			calypsoNotesSelectionMarker(selected, listFocused, existing));
	}
}

void CalypsoNotesStateUi::updateList(NotesState& state)
{
	const CalypsoF34NotesLayout layout = calypsoF34NotesLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
	state._lstNotes->clearList();
	for (size_t i = 0; i < state._workingNotes.size(); ++i)
	{
		const bool editing = state._edtNote->getVisible() && state._selectedRow == static_cast<int>(i);
		const std::string preview = Unicode::convUtf32ToUtf8(calypsoNotePreview(
			Unicode::convUtf8ToUtf32(state._workingNotes[i]), layout.previewCharacters));
		state._lstNotes->addRow(2, editing ? " " : preview.c_str(), "...");
		state._lstNotes->setCellColorRGB(i, 0, 0xFFE8FFF2u);
		state._lstNotes->setCellColorRGB(i, 1, 0xFF74FFB0u);
	}
	state._lstNotes->addRow(2, state.tr("STR_NEW_NOTE").c_str(), "");
	if (state._origin != OPT_BATTLESCAPE)
		state._lstNotes->setRowColor(state._lstNotes->getLastRowIndex(), state._lstNotes->getSecondaryColor());
	state._lstNotes->setRowColorRGB(state._lstNotes->getLastRowIndex(), 0xFF74FFB0u);
	state._hdListSelection = std::max(0, std::min(state._hdListSelection, static_cast<int>(state._workingNotes.size())));
	state._lstNotes->setSelectedRow(static_cast<size_t>(state._hdListSelection));
	updateSelection(state);
	if (state._hdFont) state._lstNotes->setTTFFont(state._hdFont, 0.42f);
	if (state._selectedRow >= 0 && state._selectedRow < state._lstNotes->getLastRowIndex())
		state._lstNotes->scrollTo(static_cast<size_t>(state._selectedRow));
}

void CalypsoNotesStateUi::rebuildFocus(NotesState& state)
{
	if (!state._hdLayout) return;
	++state._focusGeneration;
	std::vector<CalypsoFocusBinding> bindings;
	bindings.push_back({{"notes.list", true, true}, state._lstNotes, [&state]() {
		const int row = calypsoNotesActivationRow(state._hdListSelection,
			static_cast<int>(state._lstNotes->getSelectedRow()));
		if (row < 0 || row > static_cast<int>(state._workingNotes.size())) return false;
		state.beginHdEdit(row); return true;
	}});
	bindings.push_back({{"notes.new", true, true}, state._btnNew, [&state]() { state.btnNewClick(nullptr); return true; }});
	bindings.push_back({{"notes.keep", state._btnKeep->getVisible(), state._deleteRow >= 0}, state._btnKeep, [&state]() { state.btnKeepClick(nullptr); return true; }});
	bindings.push_back({{"notes.delete", state._btnDelete->getVisible(), state._deleteRow >= 0}, state._btnDelete, [&state]() { state.btnDeleteClick(nullptr); return true; }});
	bindings.push_back({{"notes.cancel", true, true}, state._btnCancel, [&state]() { state.btnCancelClick(nullptr); return true; }});
	bindings.push_back({{"notes.save", true, true}, state._btnSave, [&state]() { state.btnSaveClick(nullptr); return true; }});
	(void)state.rebuildCalypsoFocus(std::move(bindings), state._focusGeneration);
}

void CalypsoNotesStateUi::applyVisualStyle(NotesState& state)
{
	if (!state._hdLayout) return;
	if (state._origin == OPT_BATTLESCAPE)
	{
		// The caller chose the Battlescape high-contrast palette. Do not replace
		// it with the Geoscape HD ramp; only upgrade its text rendering.
		if (state._hdFont)
		{
			state._txtTitle->setTTFFont(state._hdFont, .58f);
			state._txtOriginGeo->setTTFFont(state._hdFont, .38f);
			state._txtOriginBattle->setTTFFont(state._hdFont, .38f);
			state._txtDelete->setTTFFont(state._hdFont, .34f);
			state._btnNew->setTTFFont(state._hdFont, .38f);
			state._btnKeep->setTTFFont(state._hdFont, .36f);
			state._btnDelete->setTTFFont(state._hdFont, .34f);
			state._btnCancel->setTTFFont(state._hdFont, .34f);
			state._btnSave->setTTFFont(state._hdFont, .36f);
			state._lstNotes->setTTFFont(state._hdFont, .42f);
			state._edtNote->setTTFFont(state._hdFont, .42f);
		}
		return;
	}
	const SDL_Color ramp[16] = {{11,51,40,255},{23,62,49,255},{32,81,63,255},{14,41,31,255},
		{7,26,21,255},{116,255,176,255},{3,16,21,255},{16,42,34,255},{27,75,59,255},{38,106,82,255},
		{18,55,43,255},{8,32,25,255},{159,255,201,255},{232,255,242,255},{136,170,160,255},{255,118,111,255}};
	state.setStatePalette(ramp, 240, 16);
	for (Surface* surface : state._surfaces) { surface->setPalette(state.getPalette()); surface->invalidate(); }
	state._window->setThinBorder(); state._window->setColor(240); state._window->setInnerColor(246);
	state._txtTitle->setColorRGB(0xFF74FFB0u);
	state._txtOriginGeo->setColorRGB(state._origin == OPT_GEOSCAPE ? 0xFF74FFB0u : 0xFF88AAA0u);
	state._txtOriginBattle->setColorRGB(state._origin == OPT_BATTLESCAPE ? 0xFF74FFB0u : 0xFF88AAA0u);
	state._txtDelete->setColorRGB(state._deleteRow >= 0 ? 0xFFFF766Fu : 0xFF88AAA0u);
	TextButton* buttons[] = {state._btnNew, state._btnKeep, state._btnDelete, state._btnCancel, state._btnSave};
	for (TextButton* button : buttons) { button->setColor(240); button->setTextColorRGB(0xFFE8FFF2u); }
	state._btnNew->setTextColorRGB(0xFF74FFB0u); state._btnDelete->setTextColorRGB(0xFFFF766Fu); state._btnSave->setTextColorRGB(0xFF74FFB0u);
	state._edtNote->setColor(248);
	if (state._hdFont) { state._txtTitle->setTTFFont(state._hdFont, .58f); state._txtOriginGeo->setTTFFont(state._hdFont, .38f); state._txtOriginBattle->setTTFFont(state._hdFont, .38f); state._txtDelete->setTTFFont(state._hdFont, .34f); state._btnNew->setTTFFont(state._hdFont, .38f); state._btnKeep->setTTFFont(state._hdFont, .36f); state._btnDelete->setTTFFont(state._hdFont, .34f); state._btnCancel->setTTFFont(state._hdFont, .34f); state._btnSave->setTTFFont(state._hdFont, .36f); state._lstNotes->setTTFFont(state._hdFont, .42f); state._edtNote->setTTFFont(state._hdFont, .42f); }
}

bool CalypsoNotesStateUi::routeInput(NotesState& state, Action* action)
{
	if (!state._hdLayout || state._edtNote->isFocused() || action->getDetails()->type != SDL_KEYDOWN) return false;
	const std::string* focused = state.getCalypsoFocusedId();
	const SDL_Keymod mod = static_cast<SDL_Keymod>(action->getDetails()->key.keysym.mod);
	if (!focused || *focused != "notes.list" || (mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)) != 0) return false;
	const int last = static_cast<int>(state._workingNotes.size());
	int next = state._hdListSelection;
	switch (action->getDetails()->key.keysym.sym)
	{
	case SDLK_UP: next = std::max(0, next - 1); break;
	case SDLK_DOWN: next = std::min(last, next + 1); break;
	case SDLK_HOME: next = 0; break;
	case SDLK_END: next = last; break;
	default: return false;
	}
	state._hdListSelection = next;
	state._lstNotes->setSelectedRow(static_cast<size_t>(next));
	updateSelection(state);
	action->getDetails()->type = SDL_NOEVENT;
	return true;
}

bool CalypsoNotesStateUi::handle(NotesState& state, Action* action)
{
	if (!state._hdLayout) return false;
	const std::string focusedBefore = state.getCalypsoFocusedId() ? *state.getCalypsoFocusedId() : "";
	if (routeInput(state, action)) return true;
	trackListGesture(state, action);
	if (state._edtNote->isFocused() && action->getDetails()->type == SDL_MOUSEBUTTONDOWN)
	{
		const double x = action->getAbsoluteXMouse();
		const double y = action->getAbsoluteYMouse();
		auto inside = [x, y](Surface* surface) {
			return surface && x >= surface->getX() && x < surface->getX() + surface->getWidth()
				&& y >= surface->getY() && y < surface->getY() + surface->getHeight();
		};
		if (!inside(state._edtNote))
		{
			if (inside(state._btnCancel))
				cancelEdit(state);
			else
				applyEdit(state, action);
		}
	}
	state.State::handle(action);
	if (action->getDetails()->type == SDL_MOUSEBUTTONUP
		&& action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		// TextList delivers its click while State::handle runs, so clear this
		// only afterwards and let listPress reject the just-finished drag.
		state._hdListPointerDown = false;
		state._hdListDrag = false;
	}
	const std::string focusedAfter = state.getCalypsoFocusedId() ? *state.getCalypsoFocusedId() : "";
	if (focusedBefore != focusedAfter) updateSelection(state);
	return true;
}

void CalypsoNotesStateUi::trackListGesture(NotesState& state, Action* action)
{
	const SDL_Event* event = action->getDetails();
	if (event->type != SDL_MOUSEBUTTONDOWN && event->type != SDL_MOUSEMOTION)
		return;

	const int rawX = event->type == SDL_MOUSEMOTION ? event->motion.x : event->button.x;
	const int rawY = event->type == SDL_MOUSEMOTION ? event->motion.y : event->button.y;
	// State has not yet dispatched this event to an InteractiveSurface, so make
	// its corrected logical coordinates available for the gesture check.
	action->setMouseAction(rawX, rawY, 0, 0);
	const double x = action->getAbsoluteXMouse();
	const double y = action->getAbsoluteYMouse();
	if (event->type == SDL_MOUSEBUTTONDOWN)
	{
		if (event->button.button != SDL_BUTTON_LEFT) return;
		state._hdListPointerDown = x >= state._lstNotes->getX()
			&& x < state._lstNotes->getX() + state._lstNotes->getWidth()
			&& y >= state._lstNotes->getY()
			&& y < state._lstNotes->getY() + state._lstNotes->getHeight();
		state._hdListDrag = false;
		state._hdListPointerX = x;
		state._hdListPointerY = y;
		return;
	}

	if (!state._hdListPointerDown || state._hdListDrag) return;
	const double dx = x - state._hdListPointerX;
	const double dy = y - state._hdListPointerY;
	// Eight logical pixels keeps an ordinary click forgiving while treating the
	// browser's touch-scroll gesture as a scroll before row activation occurs.
	if (dx * dx + dy * dy >= 64.0) state._hdListDrag = true;
}

void CalypsoNotesStateUi::beginEdit(NotesState& state, int row)
{
	if (!state._hdLayout || row < 0 || row > static_cast<int>(state._workingNotes.size())) return;
	if (state._edtNote->isFocused()) applyEdit(state, nullptr);
	state._deleteRow = -1;
	state._btnDelete->setPressed(false);
	state._btnDelete->setVisible(false);
	state._btnKeep->setVisible(false);
	updateStatus(state);
	rebuildFocus(state);
	state._selectedRow = row;
	state._hdListSelection = row;
	state._selectedNote = row < static_cast<int>(state._workingNotes.size()) ? state._workingNotes[row] : "";
	state._edtNote->setText(state._selectedNote);
	state._edtNote->setVisible(true);
	updateList(state);
	state._lstNotes->setScrolling(true);
	state._lstNotes->setSelectedRow(static_cast<size_t>(row));
	positionEditor(state);
	state._lstNotes->setScrolling(false);
	state._edtNote->setFocus(true, true);
}

void CalypsoNotesStateUi::applyEdit(NotesState& state, Action*)
{
	if (!state._hdLayout || state._selectedRow < 0 || !state._edtNote->getVisible()) return;
	const std::string value = state._edtNote->getText();
	const int row = state._selectedRow;
	if (state._edtNote->isFocused()) state._edtNote->setFocus(false);
	state._edtNote->setVisible(false);
	state._edtNote->setText("");
	invalidateEditorArea(state);
	state._lstNotes->setScrolling(true);
	if (row >= 0 && row < static_cast<int>(state._workingNotes.size()))
		state._workingNotes[row] = value;
	else if (row == static_cast<int>(state._workingNotes.size()) && !value.empty())
		state._workingNotes.push_back(value);
	state._selectedRow = -1;
	state._selectedNote.clear();
	updateList(state);
	updateStatus(state);
	rebuildFocus(state);
}

void CalypsoNotesStateUi::cancelEdit(NotesState& state)
{
	if (!state._hdLayout || !state._edtNote->isFocused()) return;
	state._edtNote->setFocus(false);
	state._edtNote->setVisible(false);
	state._edtNote->setText("");
	invalidateEditorArea(state);
	state._lstNotes->setScrolling(true);
	state._selectedRow = -1;
	state._selectedNote.clear();
	updateList(state);
	updateStatus(state);
	rebuildFocus(state);
}

bool CalypsoNotesStateUi::listPress(NotesState& state, Action* action)
{
	if (!state._hdLayout) return false;
	const Uint8 button = action->getDetails()->button.button;
	if (button != SDL_BUTTON_LEFT) return true;
	const int row = static_cast<int>(state._lstNotes->getSelectedRow());
	if (row < 0 || row > static_cast<int>(state._workingNotes.size())) return true;
	if (action->getDetails()->type == SDL_MOUSEBUTTONDOWN)
	{
		// Browser touch scrolling does not reliably produce a motion event in
		// every SDL path. Record the press here (after TextList selected its
		// pressed row) and make the release prove it is the same stationary row.
		// No edit/delete path runs on press.
		state._hdListActivationArmed = true;
		state._hdListPressedRow = row;
		state._hdListPointerX = action->getAbsoluteXMouse();
		state._hdListPointerY = action->getAbsoluteYMouse();
		return true;
	}
	if (action->getDetails()->type != SDL_MOUSEBUTTONUP)
		return true;

	const double dx = action->getAbsoluteXMouse() - state._hdListPointerX;
	const double dy = action->getAbsoluteYMouse() - state._hdListPointerY;
	const bool activate = state._hdListActivationArmed
		&& !state._hdListDrag
		&& state._hdListPressedRow == row
		&& dx * dx + dy * dy < 64.0;
	state._hdListActivationArmed = false;
	state._hdListPressedRow = -1;
	if (!activate) return true;
	state._hdListSelection = row;
	updateSelection(state);
	const double pointerX = action->getAbsoluteXMouse();
	const double actionLeft = state._lstNotes->getColumnX(1);
	const CalypsoF34NotesLayout layout = calypsoF34NotesLayout(
		state._hdWideLayout ? CalypsoLayoutClass::Wide : CalypsoLayoutClass::Compact);
	// The visible action column is explicit in each design composition. Its
	// conversion follows the TextList width, so the hit target remains aligned
	// with the marker after the composition is scaled into engine-base space.
	const double actionWidth = scaledF34Metric(layout.actionColumnWidth,
		state._lstNotes->getWidth(), layout.list.width);
	const bool actionColumn = row < static_cast<int>(state._workingNotes.size())
		&& calypsoF34PointerInColumn(pointerX, actionLeft, actionWidth);
	if (calypsoNotesOpenRowMenu(true, actionColumn,
		row < static_cast<int>(state._workingNotes.size())))
	{
		cancelEdit(state);
		state._deleteRow = row;
		state._btnDelete->setVisible(true);
		state._btnKeep->setVisible(true);
		updateStatus(state);
		rebuildFocus(state);
		state.restoreCalypsoFocus("notes.keep", state._focusGeneration);
		return true;
	}
	beginEdit(state, row);
	return true;
}

bool CalypsoNotesStateUi::editKeyPress(NotesState& state, Action* action)
{
	if (!state._hdLayout) return false;
	if (action->getDetails()->key.keysym.sym == SDLK_ESCAPE)
	{
		cancelEdit(state);
		action->getDetails()->type = SDL_NOEVENT;
	}
	return true;
}

bool CalypsoNotesStateUi::newClick(NotesState& state, Action*)
{
	if (!state._hdLayout) return false;
	beginEdit(state, static_cast<int>(state._workingNotes.size()));
	return true;
}

bool CalypsoNotesStateUi::deleteClick(NotesState& state, Action*)
{
	if (!state._hdLayout) return false;
	if (state._deleteRow >= 0 && state._deleteRow < static_cast<int>(state._workingNotes.size()))
		state._workingNotes.erase(state._workingNotes.begin() + state._deleteRow);
	state._deleteRow = -1;
	state._btnDelete->setPressed(false);
	state._btnDelete->setVisible(false);
	state._btnKeep->setVisible(false);
	updateStatus(state);
	rebuildFocus(state);
	updateList(state);
	state.restoreCalypsoFocus("notes.list", state._focusGeneration);
	return true;
}

bool CalypsoNotesStateUi::keepClick(NotesState& state, Action*)
{
	if (!state._hdLayout) return false;
	state._deleteRow = -1;
	state._btnDelete->setPressed(false);
	state._btnDelete->setVisible(false);
	state._btnKeep->setVisible(false);
	updateStatus(state);
	rebuildFocus(state);
	state.restoreCalypsoFocus("notes.list", state._focusGeneration);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
