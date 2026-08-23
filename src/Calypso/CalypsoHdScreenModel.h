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

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoHdScreenRenderMode
{
	HarnessFullPhysical,
	GeoscapeLiveChrome
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
	int designWidth = 0;
	int designHeight = 0;
	bool sideBySidePreview = false;
	std::vector<CalypsoHdScreenActionVisual> actions;
	std::vector<CalypsoHdScreenRegionVisual> regions;
	std::vector<CalypsoHdScreenCopy> copy;
	std::string selectedActionId;
};

} // namespace Calypso
} // namespace OpenXcom