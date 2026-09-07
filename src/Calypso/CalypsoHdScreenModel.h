#pragma once
/*
 * Shared HD screen render model types. Pure value data: no SDL, no engine,
 * no allocation policy beyond the containers themselves. Both the Emscripten
 * shared screen renderer and natively unit-tested runtime mappers consume
 * these structures so production and harness cannot drift.
 */
#include <string>
#include <vector>
#include <utility>

#include "CalypsoBasescapeHdRuntime.h"

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoHdScreenRenderMode
{
	HarnessFullPhysical,
	GeoscapeLiveChrome,
	BasescapeLiveChrome,
	/// F01 placement mode: the same base-command-shell archetype hosted by a
	/// PlaceFacilityState. Same holder, same renderer, same geometry; only the
	/// command column is repurposed (facility details + guidance + Cancel).
	BasescapePlacementChrome
};

struct CalypsoHdScreenRect
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

struct CalypsoHdScreenActionVisual
{
	std::string id;
	std::string label;
	std::string component;
	std::string slotRole;
	std::string coordinateSpace;
	CalypsoHdScreenRect visible;
	CalypsoHdScreenRect hit;
	int focusOrder = 0;
	int zOrder = 1;
	// Emscripten live mode only: the existing logical widget remains the input owner.
	const void* widget = nullptr;
};

struct CalypsoHdScreenRegionVisual
{
	std::string id;
	CalypsoHdScreenRect rect;
};

struct CalypsoHdScreenCopy
{
	CalypsoHdScreenCopy(std::string key, std::string value)
		: key(std::move(key)), value(std::move(value)) {}
	std::string key;
	std::string value;
};

struct CalypsoHdScreenRenderModel
{
	std::string archetype;
	// F01 live base data (T14): meaningful only when archetype equals
	// "base-command-shell". All strings and placements are resolved from
	// live widgets by the holder; the renderer never queries game state.
	CalypsoBasescapeHdSnapshot baseSnapshot;
	int designWidth = 0;
	int designHeight = 0;
	bool sideBySidePreview = false;
	std::vector<CalypsoHdScreenActionVisual> actions;
	std::vector<CalypsoHdScreenRegionVisual> regions;
	std::vector<CalypsoHdScreenCopy> copy;
	std::string selectedActionId;
};

/// T12 readiness gate for the base live mode: the holder feeds a complete
/// generated-contract model in T14; until then an enabled route fails closed
/// instead of drawing a partial frame.
inline bool calypsoBasescapeHdModelReady(const CalypsoHdScreenRenderModel& model)
{
	return model.archetype == "base-command-shell"
		&& model.designWidth > 0 && model.designHeight > 0
		&& !model.actions.empty();
}

} // namespace Calypso
} // namespace OpenXcom