#pragma once

#ifdef __EMSCRIPTEN__

namespace OpenXcom
{
class Window;
class Text;
class TextButton;
class TextList;
class Action;
class NotesState;
class ErrorMessageState;
class StatisticsState;

namespace Calypso
{

class CalypsoErrorMessageStateUi
{
public:
	static void configure(ErrorMessageState& state);
	static bool resize(ErrorMessageState& state);
	static void refreshAnchors(ErrorMessageState& state);
};

class CalypsoStatisticsStateUi
{
public:
	static void configure(StatisticsState& state);
	static bool resize(StatisticsState& state);
	static bool handle(StatisticsState& state, Action* action);
	static void scrollUp(StatisticsState& state);
	static void scrollDown(StatisticsState& state);
	static void refreshAnchors(StatisticsState& state);
};

class CalypsoNotesStateUi
{
private:
	static bool routeInput(NotesState& state, Action* action);
	static void trackListGesture(NotesState& state, Action* action);
public:
	static bool createControls(NotesState& state);
	static void addControls(NotesState& state);
	static void configureControls(NotesState& state);
	static bool initialize(NotesState& state);
	static void applyVisualStyle(NotesState& state);
	static void updateList(NotesState& state);
	static void updateSelection(NotesState& state);
	static void positionEditor(NotesState& state);
	static void rebuildFocus(NotesState& state);
	static void updateStatus(NotesState& state);
	static void invalidateEditorArea(NotesState& state);
	static bool handle(NotesState& state, Action* action);
	static bool resize(NotesState& state);
	static void beginEdit(NotesState& state, int row);
	static void applyEdit(NotesState& state, Action* action);
	static void cancelEdit(NotesState& state);
	static bool listPress(NotesState& state, Action* action);
	static bool editKeyPress(NotesState& state, Action* action);
	static bool newClick(NotesState& state, Action* action);
	static bool deleteClick(NotesState& state, Action* action);
	static bool keepClick(NotesState& state, Action* action);
	static void refreshAnchors(NotesState& state);
};

} // namespace Calypso
} // namespace OpenXcom

#endif
