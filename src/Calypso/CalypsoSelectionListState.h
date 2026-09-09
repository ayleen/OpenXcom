#pragma once
// Calypso HD selection-list stable-id restore across relayout (pure, natively
// testable; tested by tests/node/hd-selection-list-state.test.mjs).
//
// Single source of truth for keeping a selected stable model id across
// compact<->wide relayout, shared by all selection-list adapters (F04-F08).
// Identity is compared by value, so a reorder between layouts still resolves;
// ids are string_views borrowed from adapter-owned storage, never copied or
// allocated here. Total on empty input: an empty collection or an empty
// previous id yields no selection instead of fabricating one.
#include <cstddef>
#include <string_view>

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoSelectionListRestored
{
	bool hasSelection = false;
	std::size_t index = 0;
};

/// Restore the selected row after relayout. `selectedId` is the stable model
/// id selected before relayout, `previousIndex` its row there, `ids` the
/// stable ids of the relaid-out rows in order. Returns the matching row when
/// the id is still present; otherwise clamps `previousIndex` deterministically
/// into [0, count).
inline CalypsoSelectionListRestored calypsoSelectionListRestoreSelection(
	std::string_view selectedId, std::size_t previousIndex,
	const std::string_view* ids, std::size_t count)
{
	if (count == 0 || ids == nullptr || selectedId.empty())
	{
		return {};
	}
	for (std::size_t index = 0; index < count; ++index)
	{
		if (ids[index] == selectedId)
		{
			return {true, index};
		}
	}
	const std::size_t clamped = previousIndex < count ? previousIndex : count - 1;
	return {true, clamped};
}

} // namespace Calypso
} // namespace OpenXcom
