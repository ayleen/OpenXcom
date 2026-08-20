/* Generated-contract fixture for the shared full-screen HD renderer. */
#ifdef __EMSCRIPTEN__

#include <cstring>
#include <utility>

#include "../Engine/State.h"

#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdScreenRenderer.h"
#include "CalypsoHdUiOverlay.h"
#include "Generated/CalypsoGeoscapeCommandShell.generated.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

CalypsoHdScreenRect screenRect(
	const CalypsoGeoscapeCommandShellGen::CalypsoGeoscapeCommandShellGenRect& rect)
{
	return { rect.x, rect.y, rect.w, rect.h };
}

CalypsoHdScreenRenderModel geoscapeHarnessModel(bool wide)
{
	using namespace CalypsoGeoscapeCommandShellGen;
	const CalypsoGeoscapeCommandShellGenLayout& layout = kLayouts[wide ? 0 : 1];
	CalypsoHdScreenRenderModel model;
	model.archetype = kArchetype;
	model.designWidth = layout.designWidth;
	model.designHeight = layout.designHeight;
	model.selectedActionId = "time.speed.5sec";

	for (int index = 0; index < layout.regionCount; ++index)
	{
		const auto& region = layout.regions[index];
		model.regions.push_back({ region.id, screenRect(region.rect) });
	}
	for (int index = 0; index < layout.actionCount; ++index)
	{
		const auto& action = layout.actions[index];
		// Drawer coordinates live in their own coordinate space. The deterministic
		// harness captures the approved closed-drawer state.
		if (std::strcmp(action.component, "drawer-row") == 0) continue;
		model.actions.push_back({ action.id, action.label, action.component,
			action.slotRole, screenRect(action.visible), action.focusOrder, action.zOrder });
	}
	for (int index = 0; index < kFixtureCopyCount; ++index)
		model.copy.push_back({ kFixtureCopy[index].key, kFixtureCopy[index].value });
	return model;
}

class CalypsoHdScreenHarnessState final : public State
{
public:
	CalypsoHdScreenHarnessState()
	{
		_screen = false;
		const bool wide = calypsoHarnessSession().requestedLayout == CalypsoLayoutClass::Wide;
		CalypsoHdScreenRenderModel model = geoscapeHarnessModel(wide);
		model.sideBySidePreview = calypsoHarnessSession().sideBySide;
		enableUiScaling(model.designWidth, model.designHeight, 1.0f,
			/*subtractVanillaCenter=*/false);
		_renderer = new CalypsoHdScreenRenderer(this, std::move(model));
		CalypsoHdUiOverlay::instance().registerAdapter(_renderer);
		calypsoHdHarnessDomShow();
	}

	~CalypsoHdScreenHarnessState() override
	{
		delete _renderer;
		_renderer = nullptr;
		calypsoHdHarnessDomHide();
		calypsoHdHarnessClose();
	}

	void resize(int& dX, int& dY) override
	{
		(void)dX;
		(void)dY;
		const bool wide = calypsoHarnessSession().requestedLayout == CalypsoLayoutClass::Wide;
		CalypsoHdScreenRenderModel model = geoscapeHarnessModel(wide);
		model.sideBySidePreview = calypsoHarnessSession().sideBySide;
		recaptureUiScaling(model.designWidth, model.designHeight, 1.0f,
			/*subtractVanillaCenter=*/false);
		if (_renderer) _renderer->setModel(std::move(model));
	}

private:
	CalypsoHdScreenRenderer* _renderer = nullptr;
};

} // namespace

State* calypsoHdScreenHarnessCreateTarget(CalypsoHarnessScenario id)
{
	if (id != CalypsoHarnessScenario::GeoscapeHd) return nullptr;
	return new CalypsoHdScreenHarnessState();
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
