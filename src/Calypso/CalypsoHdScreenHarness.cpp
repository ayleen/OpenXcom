/* Generated-contract fixture for the shared full-screen HD renderer. */
#ifdef __EMSCRIPTEN__

#include <cstring>
#include <emscripten.h>
#include <iomanip>
#include <sstream>
#include <utility>

#include "../Engine/Game.h"
#include "../Engine/State.h"
#include "../Savegame/GameTime.h"
#include "../Savegame/SavedGame.h"

#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdScreenRenderer.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoBasescapeHdRuntime.h"
#include "Generated/CalypsoBasescapeCommandShell.generated.h"
#include "CalypsoBasescapeHdLayout.h"
#include "Generated/CalypsoGeoscapeCommandShell.generated.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

const CalypsoHdScreenRenderModel* g_activeScreenModel = nullptr;

std::string screenJsonEscape(const std::string& value)
{
	std::string escaped;
	for (unsigned char ch : value)
	{
		switch (ch)
		{
		case '"': escaped += "\\\""; break;
		case '\\': escaped += "\\\\"; break;
		case '\n': escaped += "\\n"; break;
		case '\r': escaped += "\\r"; break;
		case '\t': escaped += "\\t"; break;
		default: escaped += static_cast<char>(ch); break;
		}
	}
	return escaped;
}

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
	model.selectedActionId = kSelectedSpeed;

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
		if (std::strcmp(action.coordinateSpace, "screen") != 0) continue;
		model.actions.push_back({ action.id, action.label, action.component,
			action.slotRole, action.coordinateSpace, screenRect(action.visible),
			screenRect(action.hit), action.focusOrder, action.zOrder });
	}
	for (int index = 0; index < kFixtureCopyCount; ++index)
		model.copy.push_back({ kFixtureCopy[index].key, kFixtureCopy[index].value });
	return model;
}

CalypsoHdScreenRenderModel basescapeHarnessModel()
{
	using namespace CalypsoBasescapeCommandShellGen;
	const CalypsoBasescapeCommandShellGenLayout& layout = kLayouts[0];
	CalypsoHdScreenRenderModel model;
	model.archetype = kArchetype;
	model.designWidth = layout.designWidth;
	model.designHeight = layout.designHeight;
	for (int index = 0; index < layout.regionCount; ++index)
	{
		const auto& region = layout.regions[index];
		model.regions.push_back({ region.id,
			{ region.rect.x, region.rect.y, region.rect.w, region.rect.h } });
	}
	for (int index = 0; index < layout.actionCount; ++index)
	{
		const auto& action = layout.actions[index];
		model.actions.push_back({ action.id, action.label, action.component,
			action.slotRole, action.coordinateSpace,
			{ action.visible.x, action.visible.y, action.visible.w, action.visible.h },
			{ action.hit.x, action.hit.y, action.hit.w, action.hit.h },
			action.focusOrder, action.zOrder });
	}
	for (const auto& copy : kFixtureCopy)
		model.copy.emplace_back(copy.key, copy.value);
	CalypsoBasescapeHdSnapshot snapshot;
	const auto &session = calypsoHarnessSession();
	// Keep the historical standalone F01 fixture deterministic. V01 supplies
	// an explicit identity override when it needs parity with an R&D route.
	snapshot.baseName = session.rdF01ShellBase.empty() ? "AURORA DAWN" : session.rdF01ShellBase;
	snapshot.baseCaption = "BASES";
	if (!session.rdF01ShellTime.empty() && !session.rdF01ShellDate.empty())
	{
		snapshot.displayTime = session.rdF01ShellTime;
		snapshot.displayDate = session.rdF01ShellDate;
	}
	else
	{
		snapshot.displayTime = "12:28";
		snapshot.displayDate = "14 Jun 2040";
	}
	snapshot.region = "North Pacific";
	snapshot.funds = "$10.21M";
	snapshot.hoverFacility = "Laboratory";
	snapshot.selectedBase = 0;
	snapshot.baseCount = 1;
	const auto derived = calypsoBasescapeHdDerivedLayout(layout.designWidth, layout.designHeight,
		CalypsoBasescapeHdFitParams{});
	snapshot.deckRect = {derived.deckGrid.x, derived.deckGrid.y, derived.deckGrid.w, derived.deckGrid.h};
	snapshot.deckCell = derived.deckCell;
	snapshot.selectorRect = {derived.titleSelector.x, derived.titleSelector.y,
		derived.titleSelector.w, derived.titleSelector.h};
	snapshot.hasHoverCell = true;
	snapshot.hoverX = 1;
	snapshot.hoverY = 0;
	snapshot.hoverSizeX = 1;
	snapshot.hoverSizeY = 1;
	const char* rules[6] = {"STR_AIR_LOCK", "STR_LABORATORY", "STR_WORKSHOP",
		"STR_GENERAL_STORES", "STR_ALIEN_CONTAINMENT", "STR_SUB_PEN"};
	const int geo[6][6] = {{0, 0, 1, 1}, {1, 0, 1, 1}, {2, 0, 2, 2}, {5, 0, 1, 1}, {0, 5, 1, 1}, {4, 4, 2, 2}};
	const int times[6] = {0, 12, 0, 0, 0, 0};
	const bool disabled[6] = {false, false, false, false, true, false};
	for (int i = 0; i < 6; ++i)
	{
		CalypsoBasescapeHdFacilityVisual fac;
		fac.x = geo[i][0];
		fac.y = geo[i][1];
		fac.sizeX = geo[i][2];
		fac.sizeY = geo[i][3];
		fac.ruleType = rules[i];
		fac.buildTime = times[i];
		fac.disabled = disabled[i];
		if (i == 5)
		{
			fac.craftIndex = 0;
			fac.craftDrawn = true;
			CalypsoBasescapeHdCraftVisual craft;
			craft.name = "Barracuda-1";
			craft.away = false;
			craft.artKey = "STR_BARRACUDA";
			snapshot.crafts.push_back(std::move(craft));
		}
		snapshot.facilities.push_back(std::move(fac));
	}
	CalypsoBasescapeHdSelectorEntry entry;
	entry.name = snapshot.baseName;
	entry.selected = true;
	for (const auto& fac : snapshot.facilities)
	{
		CalypsoBasescapeHdSelectorCell cell;
		cell.x = fac.x;
		cell.y = fac.y;
		cell.sizeX = fac.sizeX;
		cell.sizeY = fac.sizeY;
		cell.built = fac.buildTime == 0;
		cell.disabled = fac.disabled;
		entry.cells.push_back(cell);
	}
	snapshot.bases.push_back(std::move(entry));
	model.baseSnapshot = std::move(snapshot);
	return model;
}

class CalypsoHdScreenHarnessState final : public State
{
public:
	explicit CalypsoHdScreenHarnessState(CalypsoHarnessScenario scenario)
		: _scenario(scenario)
	{
		_screen = false;
		CalypsoHdScreenRenderModel model = harnessModel();
		model.sideBySidePreview = calypsoHarnessSession().sideBySide;
		enableUiScaling(model.designWidth, model.designHeight, 1.0f,
			/*subtractVanillaCenter=*/false);
		_renderer = new CalypsoHdScreenRenderer(this, std::move(model));
		g_activeScreenModel = &_renderer->model();
		CalypsoHdUiOverlay::instance().registerAdapter(_renderer);
		calypsoHdHarnessDomShow();
	}

	~CalypsoHdScreenHarnessState() override
	{
		delete _renderer;
		g_activeScreenModel = nullptr;
		_renderer = nullptr;
		calypsoHdHarnessDomHide();
		calypsoHdHarnessClose();
	}

	void resize(int& dX, int& dY) override
	{
		(void)dX;
		(void)dY;
		CalypsoHdScreenRenderModel model = harnessModel();
		model.sideBySidePreview = calypsoHarnessSession().sideBySide;
		recaptureUiScaling(model.designWidth, model.designHeight, 1.0f,
			/*subtractVanillaCenter=*/false);
		if (_renderer)
		{
			_renderer->setModel(std::move(model));
			g_activeScreenModel = &_renderer->model();
		}
	}

private:
	CalypsoHdScreenRenderModel harnessModel() const
	{
		if (_scenario == CalypsoHarnessScenario::F01Basescape)
		{
			return basescapeHarnessModel();
		}
		const bool wide = calypsoHarnessSession().requestedLayout == CalypsoLayoutClass::Wide;
		return geoscapeHarnessModel(wide);
	}

	CalypsoHarnessScenario _scenario;
	CalypsoHdScreenRenderer* _renderer = nullptr;
};

} // namespace

State* calypsoHdScreenHarnessCreateTarget(CalypsoHarnessScenario id)
{
	if (id != CalypsoHarnessScenario::GeoscapeHd
		&& id != CalypsoHarnessScenario::F01Basescape)
	{
		return nullptr;
	}
	return new CalypsoHdScreenHarnessState(id);
}

bool calypsoHdScreenHarnessF01Active()
{
	return g_activeScreenModel != nullptr
		&& g_activeScreenModel->archetype == CalypsoBasescapeCommandShellGen::kArchetype;
}

} // namespace Calypso
} // namespace OpenXcom

extern "C" {

/// Read-only shell identity rendered by the active F01 full-screen fixture.
/// V01 uses this to prove that its F01 baseline carries the same base/time
/// identity as the R&D capture; it is never used for production behavior.
EMSCRIPTEN_KEEPALIVE
const char *calypso_harness_screen_snapshot()
{
	static std::string snapshot;
	const auto* model = OpenXcom::Calypso::g_activeScreenModel;
	if (!model)
	{
		snapshot.clear();
		return snapshot.c_str();
	}
	const auto& base = model->baseSnapshot;
	std::ostringstream out;
	out << "{\"screenShell\":{\"baseCaption\":\""
		<< OpenXcom::Calypso::screenJsonEscape(base.baseCaption)
		<< "\",\"baseName\":\""
		<< OpenXcom::Calypso::screenJsonEscape(base.baseName)
		<< "\",\"clockTime\":\""
		<< OpenXcom::Calypso::screenJsonEscape(base.displayTime)
		<< "\",\"clockDate\":\""
		<< OpenXcom::Calypso::screenJsonEscape(base.displayDate)
		<< "\"}}";
	snapshot = out.str();
	return snapshot.c_str();
}

} // extern "C"

#endif // __EMSCRIPTEN__
