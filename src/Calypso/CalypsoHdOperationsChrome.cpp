#ifdef __EMSCRIPTEN__

#include "CalypsoHdOperationsChrome.h"

#include "CalypsoBasescapeHdLayout.h"
#include "CalypsoHdOperationsModel.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoViewportRuntime.h"
#include "CommandCenter/CommandCenterInteraction.h"
#include "CommandCenter/CommandCenterLayout.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/State.h"
#include "../Interface/TextButton.h"
#include "../Mod/Mod.h"
#include "../Savegame/Base.h"
#include "../Savegame/GameTime.h"
#include "../Savegame/SavedGame.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace OpenXcom
{
namespace Calypso
{
bool calypsoHdOperationsRouteEnabled(const Game* game, const char* familyId)
{
	return game != nullptr && game->getMod() != nullptr
		&& (game->getMod()->isHdUiFamilyEnabled(familyId)
			|| calypsoHarnessHostUp(calypsoHarnessSession()));
}

void calypsoHdOperationsPublishHarnessVisibility()
{
	if (calypsoHarnessHostUp(calypsoHarnessSession()))
		calypsoHdHarnessDomShow();
}


CalypsoHdOperationsRect calypsoHdOperationsProjectForCurrentPresentation(
	const CalypsoHdOperationsRect& rect, int designWidth, int designHeight)
{
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	if (!metrics.valid() || metrics.scaleX <= 0.0 || metrics.scaleY <= 0.0)
		return rect;
	return calypsoHdOperationsProjectRect(
		rect, designWidth, designHeight, metrics.logicalWidth, metrics.logicalHeight,
		static_cast<int>(std::llround(metrics.contentOffsetX / metrics.scaleX)),
		static_cast<int>(std::llround(metrics.contentOffsetY / metrics.scaleY)));
}

CalypsoHdOperationsChrome::CalypsoHdOperationsChrome(State& state)
{
	const ActionHandler handlers[7] = {
		(ActionHandler)&State::calypsoHdNavigateWorldClick,
		(ActionHandler)&State::calypsoHdNavigateBasesClick,
		(ActionHandler)&State::calypsoHdNavigateOperationsClick,
		(ActionHandler)&State::calypsoHdNavigateAnalyticsClick,
		(ActionHandler)&State::calypsoHdNavigateArchiveClick,
		(ActionHandler)&State::calypsoHdNavigateSettingsClick,
		(ActionHandler)&State::calypsoHdNavigateBasesClick,
	};
	for (std::size_t index = 0; index < _buttons.size(); ++index)
	{
		TextButton* button = new TextButton(1, 1, 0, 0);
		button->setText(std::string());
		state.add(button);
		button->onMouseClick(handlers[index]);
		_buttons[index] = button;
	}
}

void CalypsoHdOperationsChrome::applyGeometry() const
{
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	const CalypsoLayoutMetrics& viewport = calypsoViewportRuntime().current();
	if (!metrics.valid() || metrics.scaleX <= 0.0 || metrics.scaleY <= 0.0)
		return;
	const int cssWidth = std::max(1, viewport.logicalWidth);
	const int cssHeight = std::max(1, viewport.logicalHeight);
	const CommandCenter::CommandCenterLayout layout = CommandCenter::computeLayout(
		CommandCenter::Size2{static_cast<float>(cssWidth), static_cast<float>(cssHeight)},
		false, CommandCenter::InsetsF{
			static_cast<float>(viewport.safeX), static_cast<float>(viewport.safeY),
			static_cast<float>(cssWidth - viewport.safeX - viewport.safeWidth),
			static_cast<float>(cssHeight - viewport.safeY - viewport.safeHeight)});
	const CalypsoBasescapeHdProjection projection = calypsoBasescapeHdProjection(
		cssWidth, cssHeight, metrics.physicalWidth, metrics.physicalHeight,
		metrics.scaleX, metrics.scaleY, metrics.contentOffsetX,
		metrics.contentOffsetY, layout.scale);
	const auto place = [&](TextButton* button, const CommandCenter::RectF& source)
	{
		if (!button) return;
		const CalypsoBasescapeHdLogicalRect rect = calypsoBasescapeHdProjectRect(
			projection, CalypsoBasescapeHdRect{
				static_cast<int>(std::lround(source.x)),
				static_cast<int>(std::lround(source.y)),
				static_cast<int>(std::lround(source.width)),
				static_cast<int>(std::lround(source.height))});
		if (button->getX() != rect.x) button->setX(rect.x);
		if (button->getY() != rect.y) button->setY(rect.y);
		if (button->getWidth() != rect.w) button->setWidth(rect.w);
		if (button->getHeight() != rect.h) button->setHeight(rect.h);
	};
	for (int index = 0; index < 5; ++index)
		place(_buttons[static_cast<std::size_t>(index)],
			CommandCenter::calypsoCcRailItemRect(layout.navigationRail, index));
	place(_buttons[5], CommandCenter::calypsoCcRailSettingsRect(layout.navigationRail));
	place(_buttons[6], layout.baseSelector);
}

void CalypsoHdOperationsChrome::populateModel(CalypsoHdOperationsModel& model) const
{
	for (TextButton* button : _buttons)
		if (button) model.suppressedWidgets.push_back(button);
	Game* game = getCurrentGame();
	const CommandCenter::CommandCenterFonts fonts =
		CommandCenter::calypsoCcResolveFonts(game ? game->getMod() : nullptr);
	if (fonts.ready)
	{
		model.headingFont = fonts.interSb;
		model.bodyFont = fonts.interR;
		model.monoFont = fonts.plexR;
	}
	if (game && game->getLanguage())
		model.baseCaption =
			std::string(game->getLanguage()->getString("STR_BASES"));
	SavedGame* save = game ? game->getSavedGame() : nullptr;
	if (!save) return;
	if (model.baseName.empty() && save->getSelectedBase() != nullptr)
		model.baseName = save->getSelectedBase()->getName();
	const GameTime* time = save->getTime();
	if (!time || !game->getLanguage()) return;
	std::ostringstream clock;
	clock << time->getHour() << ':' << std::setfill('0') << std::setw(2)
		<< time->getMinute();
	model.clockTime = clock.str();
	model.clockDate = time->getDayString(game->getLanguage()) + " "
		+ std::string(game->getLanguage()->getString(time->getMonthString())) + " "
		+ std::to_string(time->getYear());
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
