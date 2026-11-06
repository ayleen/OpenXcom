#pragma once

#include <cstddef>

namespace OpenXcom
{
namespace Calypso
{

/// A fixture may mutate only the replacement save created by the harness.
/// Keeping this predicate pure makes the fail-closed boundary executable in a
/// tiny native probe without constructing Game or SavedGame objects.
inline bool calypsoHarnessSaveOwned(bool leaseActive,
	const void *fixtureSave, const void *currentSave)
{
	return leaseActive && fixtureSave != nullptr && currentSave == fixtureSave;
}

} // namespace Calypso
} // namespace OpenXcom
