#ifdef __EMSCRIPTEN__
#include "CalypsoMissionReview.h"
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdUiOverlay.h"
#include "../Engine/Action.h"
#include "../Engine/State.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Savegame/Soldier.h"

namespace OpenXcom { namespace Calypso {
CalypsoMissionReview::CalypsoMissionReview(State &state, Craft *craft, const std::string &target, Facts facts,
	TextButton *begin, TextButton *cancel, TextButton *patrol, ActionHandler tabHandler, const char *style)
	: _state(state), _craft(craft), _targetName(target), _facts(std::move(facts)),
	  _begin(begin), _cancel(cancel), _patrol(patrol), _renderer(&state, {})
{
	_brief = new TextButton(1, 1);
	_roster = new TextButton(1, 1);
	for (auto *tab : {_brief, _roster})
	{
		state.add(tab, "button", style);
		tab->onMouseClick(tabHandler);
	}
	_brief->setText(state.tr("STR_CAL_DROP_BRIEF"));
	_roster->setText(state.tr("STR_CAL_DROP_ROSTER"));
	_rows = new TextList(250, 100);
	state.add(_rows);
	_rows->setColumns(2, 125, 125);
	populate();
	_renderer.setModelProvider([this] { return model(); });
	refresh();
	CalypsoHdUiOverlay::instance().registerAdapter(&_renderer);
	calypsoHdOperationsPublishHarnessVisibility();
}

void CalypsoMissionReview::populate()
{
	_rows->clearList();
	if (!_showRoster)
	{
		for (const auto &fact : _facts) _rows->addRow(2, fact.first.c_str(), fact.second.c_str());
		return;
	}
	for (const auto *diver : *_craft->getBase()->getSoldiers())
	{
		const auto *assigned = diver->getCraft();
		const std::string status = _state.tr(assigned == _craft ? "STR_CAL_DROP_UNITS" :
			assigned ? "STR_CAL_DROP_ELSEWHERE" : "STR_CAL_DROP_HOME");
		const auto detail = std::string(_state.tr(diver->getRankString())) + " · " + status;
		_rows->addRow(2, diver->getName(true).c_str(), detail.c_str());
	}
}

void CalypsoMissionReview::changePage(Action *action)
{
	_showRoster = action->getSender() == _roster;
	populate();
	refresh();
}
void CalypsoMissionReview::refresh() { _renderer.setModel(model()); }
} }
#endif
