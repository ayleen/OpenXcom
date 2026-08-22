#pragma once
/*
 * Phase 46.4 Stage 8a — Geoscape HD runtime mapping.
 *
 * Pure, engine-independent translation of the generated strategic-command-shell
 * contract into the shared HD screen render model, plus the live-state inputs
 * the Geoscape state owns (copy strings, selection, action availability).
 * No SDL, no OpenGL, no state pushes: the same code is unit-tested natively
 * and consumed by the Emscripten production adapter.
 */
#include <string>
#include <vector>

#include "CalypsoHdScreenModel.h"
#include "Generated/CalypsoGeoscapeCommandShell.generated.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoGeoscapeHdRuntimeInput
{
	std::vector<CalypsoHdScreenCopy> copy;
	std::vector<std::string> disabledActionIds;
	std::string selectedActionId;
};

struct CalypsoGeoscapeHdRuntimeModel : public CalypsoHdScreenRenderModel
{
	std::vector<std::string> disabledActionIds;
};

inline CalypsoGeoscapeHdRuntimeModel calypsoGeoscapeHdRuntimeModel(
	const CalypsoGeoscapeCommandShellGen::CalypsoGeoscapeCommandShellGenLayout& layout,
	const CalypsoGeoscapeHdRuntimeInput& input)
{
	CalypsoGeoscapeHdRuntimeModel result;
	result.archetype = CalypsoGeoscapeCommandShellGen::kArchetype;
	result.designWidth = layout.designWidth;
	result.designHeight = layout.designHeight;
	result.selectedActionId = input.selectedActionId;

	for (int i = 0; i < layout.actionCount; ++i)
	{
		const auto& source = layout.actions[i];
		bool disabled = false;
		for (const auto& id : input.disabledActionIds)
			if (id == source.id) { disabled = true; break; }
		if (disabled)
		{
			result.disabledActionIds.push_back(source.id);
			continue;
		}
		CalypsoHdScreenActionVisual action;
		action.id = source.id;
		action.label = source.label;
		action.component = source.component;
		action.slotRole = source.slotRole;
		action.coordinateSpace = source.coordinateSpace;
		action.visible = { source.visible.x, source.visible.y, source.visible.w, source.visible.h };
		action.hit = { source.hit.x, source.hit.y, source.hit.w, source.hit.h };
		action.focusOrder = source.focusOrder;
		action.zOrder = source.zOrder;
		result.actions.push_back(action);
	}

	for (int i = 0; i < layout.regionCount; ++i)
	{
		const auto& region = layout.regions[i];
		result.regions.push_back({ region.id,
			{ region.rect.x, region.rect.y, region.rect.w, region.rect.h } });
	}

	result.copy = input.copy;
	return result;
}

} // namespace Calypso
} // namespace OpenXcom
