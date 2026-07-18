#pragma once

/*
 * Phase 46.2 F34 -- dependency-free behavior contracts shared by the common
 * records states.  Browser presentation lives in CalypsoCommonRecordsStateUi;
 * these helpers deliberately own no SavedGame or UI objects so native tests
 * can cover the mutation and terminal-route invariants directly.
 */

#include <cstddef>
#include <string>
#include <vector>

#include "CalypsoUiMetrics.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoMonthlyRating
{
	bool available = false;
	int value = 0;
};

inline CalypsoMonthlyRating calypsoAverageMonthlyRating(int totalScore,
	std::size_t months)
{
	if (months == 0) return {};
	return {true, totalScore / static_cast<int>(months)};
}

enum class CalypsoStatisticsReturn
{
	Memorial,
	MainMenu
};

inline CalypsoStatisticsReturn calypsoStatisticsReturn(bool terminalCampaign)
{
	return terminalCampaign ? CalypsoStatisticsReturn::MainMenu
	                        : CalypsoStatisticsReturn::Memorial;
}

/// Save applies the editor first, then removes blank rows while producing a
/// fresh notebook. The caller owns replacing SavedGame only after this result
/// is complete, so failed/cancelled UI paths cannot partially mutate a save.
inline std::vector<std::string> calypsoCommitNotes(
	std::vector<std::string> notes, int activeRow, bool editorVisible,
	const std::string& editorText)
{
	if (editorVisible && activeRow >= 0)
	{
		const std::size_t row = static_cast<std::size_t>(activeRow);
		if (row < notes.size()) notes[row] = editorText;
		else if (row == notes.size() && !editorText.empty()) notes.push_back(editorText);
	}
	std::vector<std::string> committed;
	committed.reserve(notes.size());
	for (const std::string& note : notes)
		if (!note.empty()) committed.push_back(note);
	return committed;
}

/// F34 has one visible secondary row action. Right-click is intentionally not
/// a second delete path: it must never create desktop/mobile divergence.
inline bool calypsoNotesOpenRowMenu(bool primaryActivation, bool actionColumn,
	bool existingRow)
{
	return primaryActivation && actionColumn && existingRow;
}

/// Action::getAbsoluteXMouse() and Surface geometry are both expressed in the
/// engine base. Keep hit testing in that one coordinate space.
inline bool calypsoF34PointerInColumn(double enginePointer, double columnLeft,
	double columnWidth)
{
	return columnWidth > 0.0 && enginePointer >= columnLeft
		&& enginePointer < columnLeft + columnWidth;
}

struct CalypsoF34Rect
{
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
};

struct CalypsoF34ErrorLayout
{
	int designWidth = 0;
	int designHeight = 0;
	CalypsoF34Rect window;
	CalypsoF34Rect iconPanel;
	CalypsoF34Rect icon;
	CalypsoF34Rect warning;
	CalypsoF34Rect message;
	CalypsoF34Rect messageDetail;
	CalypsoF34Rect acknowledge;
};

inline CalypsoF34ErrorLayout calypsoF34ErrorLayout(CalypsoLayoutClass layoutClass)
{
	if (layoutClass == CalypsoLayoutClass::Wide)
		return {1280, 720, {250, 205, 780, 310}, {286, 271, 84, 84},
			{286, 271, 84, 84}, {400, 242, 560, 22}, {400, 274, 560, 52},
			{400, 346, 560, 56}, {820, 430, 170, 60}};
	return {740, 360, {48, 57, 644, 246}, {78, 123, 58, 58},
		{78, 123, 58, 58}, {166, 88, 454, 18}, {166, 114, 454, 40},
		{166, 166, 454, 44}, {512, 235, 140, 44}};
}

struct CalypsoF34StatisticsLayout
{
	int designWidth = 0;
	int designHeight = 0;
	CalypsoF34Rect window;
	CalypsoF34Rect headerPanel;
	CalypsoF34Rect listPanel;
	CalypsoF34Rect returnPanel;
	CalypsoF34Rect footerPanel;
	CalypsoF34Rect title;
	CalypsoF34Rect recordLabel;
	CalypsoF34Rect outcome;
	CalypsoF34Rect list;
	CalypsoF34Rect acknowledge;
	CalypsoF34Rect scrollUp;
	CalypsoF34Rect scrollDown;
	CalypsoF34Rect returnRole;
	CalypsoF34Rect returnDetail;
	CalypsoF34Rect scrollHint;
	CalypsoF34Rect footerStatus;
	int labelColumnWidth = 0;
	int valueColumnWidth = 0;
	int rowHeight = 0;
};

constexpr int CALYPSO_F34_TEXTLIST_SCROLL_RAIL = 14;

inline CalypsoF34StatisticsLayout calypsoF34StatisticsLayout(
	CalypsoLayoutClass layoutClass)
{
	if (layoutClass == CalypsoLayoutClass::Wide)
		return {960, 540, {12, 12, 936, 516}, {24, 24, 912, 64},
			{24, 100, 680, 306}, {716, 100, 220, 306}, {24, 420, 912, 96},
			{42, 32, 490, 48}, {42, 112, 450, 24}, {602, 32, 310, 48},
			{42, 144, 588, 246}, {748, 438, 168, 60}, {644, 206, 60, 60},
			{644, 274, 60, 60}, {734, 126, 184, 42}, {734, 176, 184, 88},
			{734, 320, 184, 44}, {42, 442, 674, 48}, 416, 160, 58};
	return {740, 360, {8, 8, 724, 344}, {16, 16, 708, 44},
		{16, 72, 532, 168}, {560, 72, 164, 168}, {16, 252, 708, 92},
		{28, 20, 372, 34}, {28, 78, 276, 18}, {414, 20, 294, 34},
		{28, 104, 462, 124}, {598, 284, 118, 44}, {504, 132, 44, 44},
		{504, 180, 44, 44}, {572, 84, 140, 28}, {572, 116, 140, 62},
		{572, 186, 140, 30}, {28, 268, 516, 48}, 294, 156, 44};
}

struct CalypsoF34NotesLayout
{
	int designWidth = 0;
	int designHeight = 0;
	CalypsoF34Rect window;
	CalypsoF34Rect title;
	CalypsoF34Rect status;
	CalypsoF34Rect list;
	CalypsoF34Rect editor;
	CalypsoF34Rect save;
	CalypsoF34Rect cancel;
	CalypsoF34Rect remove;
	CalypsoF34Rect create;
	CalypsoF34Rect keep;
	CalypsoF34Rect originGeoscape;
	CalypsoF34Rect originBattlescape;
	int listMargin = 0;
	int textColumnWidth = 0;
	int actionColumnWidth = 0;
	int rowHeight = 0;
	std::size_t previewCharacters = 0;
};

inline CalypsoF34Rect calypsoF34NotesScrollControls(const CalypsoF34NotesLayout& layout)
{
	return {layout.list.x + layout.list.width, layout.list.y, 13, layout.list.height};
}

inline CalypsoF34NotesLayout calypsoF34NotesLayout(CalypsoLayoutClass layoutClass)
{
	if (layoutClass == CalypsoLayoutClass::Wide)
		return {960, 540, {12, 12, 936, 516}, {24, 24, 912, 40},
			{690, 218, 238, 96}, {24, 144, 650, 300}, {24, 144, 564, 60},
			{808, 456, 120, 60}, {690, 456, 106, 60}, {818, 330, 110, 60},
			{690, 144, 238, 60}, {690, 330, 118, 60}, {24, 72, 440, 60},
			{476, 72, 452, 60}, 8, 574, 60, 60, 78};
	return {740, 360, {8, 8, 724, 344}, {16, 16, 708, 32},
			{512, 152, 212, 38}, {16, 102, 483, 198}, {16, 102, 424, 44},
			{612, 308, 112, 44}, {512, 308, 96, 44}, {622, 198, 102, 44},
			{512, 102, 212, 44}, {512, 198, 102, 44}, {16, 52, 346, 44},
			{378, 52, 346, 44}, 8, 430, 44, 44, 52};
}

} // namespace Calypso
} // namespace OpenXcom
