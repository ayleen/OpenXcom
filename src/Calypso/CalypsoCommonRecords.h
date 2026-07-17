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

/// Browser pointer events arrive in logical CSS pixels while F34 surface
/// geometry is authored in the current engine base. Keep that conversion
/// explicit at the state boundary instead of comparing the two spaces.
inline double calypsoF34LogicalPointerToBase(double pointer, int baseExtent,
	int logicalExtent)
{
	if (baseExtent <= 0 || logicalExtent <= 0) return pointer;
	return pointer * static_cast<double>(baseExtent)
		/ static_cast<double>(logicalExtent);
}

} // namespace Calypso
} // namespace OpenXcom
