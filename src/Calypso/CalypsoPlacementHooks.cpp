#ifdef __EMSCRIPTEN__

#include "CalypsoPlacementHooks.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdRuntimeDiagnostics.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoStrategicNavigation.h"
#include "../Engine/Game.h"
#include "../Engine/State.h"
#include "../Engine/Action.h"
#include "../Engine/Screen.h"
#include "../Engine/Timer.h"
#include "../Savegame/Base.h"
#include "../Interface/Cursor.h"
#include "../Interface/TextList.h"
#include "CalypsoF09ResearchUi.h"
#include "CalypsoF10ProductionUi.h"
#include "CalypsoF14GlobalOperationsUi.h"
#include "../Basescape/GlobalResearchDiaryState.h"
#include "../Basescape/ManufactureInfoState.h"
#include "../Basescape/ManufactureState.h"
#include "../Basescape/NewManufactureListState.h"
#include "../Basescape/NewResearchListState.h"
#include "../Basescape/ResearchInfoState.h"
#include "../Basescape/ResearchState.h"
#include "../Mod/RuleResearch.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>

namespace OpenXcom
{
namespace Calypso
{

bool calypsoHdPrepareNativeBlit(const State *state)
{
	auto& overlay = CalypsoHdUiOverlay::instance();
	const bool registered = overlay.hasRegisteredState(state);
	if (registered && !overlay.logicalStateSuppressed(state, overlay.frameId()))
		++calypsoHdRuntimeDiagnostics().registeredVanillaBlits;
	const bool allowUnregisteredArticle = !registered
		&& calypsoHdHarnessAllowsUnregisteredNativeArticle(state);
	if (calypsoHarnessHostUp(calypsoHarnessSession()) && !allowUnregisteredArticle)
		return true;
	return overlay.logicalStateSuppressed(state, overlay.frameId());
}

Uint16 calypsoResearchReorderWarpDelta(const TextList *list, const Action *action)
{
	const double yScale = action && action->getYScale() > 0.0
		? action->getYScale() : 1.0;
	if (list && list->isCalypsoHdSelectionList()
		&& list->getCalypsoHdRowStride() > 0)
		return static_cast<Uint16>(std::max(1L, std::lround(
			static_cast<double>(list->getCalypsoHdRowStride()) * yScale)));
	return static_cast<Uint16>(std::max(1L, std::lround(8.0 * yScale)));
}

void calypsoResearchWarpCursor(Game *game, const Action *action, int delta)
{
	Screen *screen = game ? game->getScreen() : nullptr;
	Cursor *cursor = game ? game->getCursor() : nullptr;
	if (!screen || !cursor || !action) return;
	const int x = action->getLeftBlackBand() + action->getXMouse();
	const int y = action->getTopBlackBand() + action->getYMouse() + delta;
	const double sx = screen->getXScale() > 0 ? screen->getXScale() : 1;
	const double sy = screen->getYScale() > 0 ? screen->getYScale() : 1;
	cursor->setX(static_cast<int>((x - screen->getCursorLeftBlackBand()) / sx));
	cursor->setY(static_cast<int>((y - screen->getCursorTopBlackBand()) / sy));
	game->dispatchCalypsoMouseMotion(x, y, 0, delta);
}

std::string calypsoCaptureNewResearchSelection(NewResearchListState& state)
{
	if (!state._lstResearch->isCalypsoHdSelectionList()) return {};
	const unsigned int row = state._lstResearch->getSelectedRow();
	if (row == static_cast<unsigned int>(-1) || row >= state._projects.size()
		|| !state._projects[row]) return {};
	return state._projects[row]->getName();
}

void calypsoRestoreNewResearchSelection(NewResearchListState& state, const std::string& name)
{
	if (!state._lstResearch->isCalypsoHdSelectionList()) return;
	if (!name.empty())
	{
		for (size_t index = 0; index < state._projects.size(); ++index)
		{
			if (!state._projects[index] || state._projects[index]->getName() != name) continue;
			state._lstResearch->setSelectedRow(index);
			return;
		}
	}
	state._lstResearch->invalidateCalypsoHdSelection();
}

} // namespace Calypso

int Game::calypsoHdRouteFailedTick()
{
	if (_states.empty() || !Calypso::CalypsoHdUiOverlay::instance().routeFailedForState(_states.back()))
		return 0;
	_fastMainLoopRequester = nullptr;
	SDL_Event discarded;
	while (SDL_PollEvent(&discarded))
		if (discarded.type == SDL_QUIT) return -1;
	return 1;
}

void State::calypsoHdNavigateWorldClick(Action *)
{
	Calypso::calypsoNavigateToWorld(_game);
}

void State::calypsoHdNavigateBasesClick(Action *)
{
	Calypso::calypsoNavigateToBases(_game);
}

void State::calypsoHdNavigateOperationsClick(Action *)
{
	Calypso::calypsoNavigateToStrategicRoute(
		_game, Calypso::CalypsoStrategicRoute::Intercept);
}

void State::calypsoHdNavigateAnalyticsClick(Action *)
{
	Calypso::calypsoNavigateToStrategicRoute(
		_game, Calypso::CalypsoStrategicRoute::Graphs);
}

void State::calypsoHdNavigateArchiveClick(Action *)
{
	Calypso::calypsoNavigateToStrategicRoute(
		_game, Calypso::CalypsoStrategicRoute::Archive);
}

void State::calypsoHdNavigateSettingsClick(Action *)
{
	Calypso::calypsoNavigateToStrategicRoute(
		_game, Calypso::CalypsoStrategicRoute::Options);
}

void GlobalResearchDiaryState::resize(int &dX, int &dY)
{
	if (Calypso::CalypsoF14GlobalOperationsUi::resize(*this)) return;
	State::resize(dX, dY);
}

void ManufactureInfoState::resize(int &dX, int &dY)
{
	if (Calypso::CalypsoF10ProductionUi::resize(*this)) return;
	State::resize(dX, dY);
}

void ManufactureState::resize(int &dX, int &dY)
{
	if (Calypso::CalypsoF10ProductionUi::resize(*this)) return;
	State::resize(dX, dY);
}

void NewManufactureListState::resize(int &dX, int &dY)
{
	if (Calypso::CalypsoF10ProductionUi::resize(*this)) return;
	TouchState::resize(dX, dY);
}

void NewResearchListState::resize(int &dX, int &dY)
{
	if (Calypso::CalypsoF09ResearchUi::resize(*this)) return;
	TouchState::resize(dX, dY);
}

void ResearchInfoState::resize(int &dX, int &dY)
{
	if (Calypso::CalypsoF09ResearchUi::resize(*this)) return;
	State::resize(dX, dY);
}

void ResearchState::resize(int &dX, int &dY)
{
	if (Calypso::CalypsoF09ResearchUi::resize(*this)) return;
	State::resize(dX, dY);
}

bool ResearchState::calypsoHdUsesSummaryNoScrollArea() const
{
	return !_hdLayout;
}

void ManufactureInfoState::cancelUnstartedProductionForHdExit()
{
	if (_item == nullptr || _production == nullptr) return;
	_base->removeProduction(_production);
	_production = nullptr;
}

void ManufactureInfoState::prepareHdStrategicExit()
{
	if (!Calypso::calypsoHdExitOnce(_hdStrategicExitPrepared)) return;
	const auto owner = _item != nullptr
		? Calypso::CalypsoHdExitOwner::NewProduction
		: Calypso::CalypsoHdExitOwner::ExistingProduction;
	const auto plan = Calypso::calypsoHdExitPlan(owner);
	_timerMoreEngineer->stop();
	_timerLessEngineer->stop();
	_timerMoreUnit->stop();
	_timerLessUnit->stop();
	delete _hdAdapter;
	_hdAdapter = nullptr;
	if (plan.removeUnstartedProduction) cancelUnstartedProductionForHdExit();
	else if (plan.applyDeferredProductionOptions) applyDeferredProductionOptions();
}

void ResearchInfoState::prepareHdStrategicExit()
{
	if (!Calypso::calypsoHdExitOnce(_hdStrategicExitPrepared)) return;
	const auto owner = _rule != nullptr
		? Calypso::CalypsoHdExitOwner::NewResearch
		: Calypso::CalypsoHdExitOwner::ExistingResearch;
	const auto plan = Calypso::calypsoHdExitPlan(owner);
	_timerMore->stop();
	_timerLess->stop();
	if (plan.cancelResearchPreview) cancelPreview();
	delete _hdAdapter;
	_hdAdapter = nullptr;
}

} // namespace OpenXcom

#endif
