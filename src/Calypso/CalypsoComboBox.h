#pragma once
/* Pure disabled-option decision shared by ComboBox and native regression tests. */
#include <cstddef>
#include <vector>

namespace OpenXcom
{
namespace Calypso
{

inline bool calypsoComboOptionEnabled(const std::vector<bool>& enabled, std::size_t index)
{
	return index >= enabled.size() || enabled[index];
}

} // namespace Calypso
} // namespace OpenXcom
