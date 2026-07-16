#pragma once

namespace OpenXcom
{
namespace Calypso
{

inline int calypsoNotesActivationRow(int authoritativeSelection, int hoveredRow)
{
	(void)hoveredRow;
	return authoritativeSelection;
}

inline const char *calypsoNotesSelectionMarker(
	bool selected, bool listFocused, bool existingRow)
{
	if (selected) return listFocused ? ">" : "*";
	return existingRow ? "..." : "";
}

} // namespace Calypso
} // namespace OpenXcom
