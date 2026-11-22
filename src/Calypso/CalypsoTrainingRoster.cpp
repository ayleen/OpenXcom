#ifdef __EMSCRIPTEN__
#include "CalypsoTrainingRoster.h"
#include "CalypsoBaseLedger.h"
#include "../Basescape/SoldierInfoState.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Savegame/Base.h"
#include <algorithm>

namespace OpenXcom {
void CalypsoTrainingRoster::configureRoster(Base *base, const char *style, std::vector<std::string> labels,
	ActionHandler assign, ActionHandler clear)
{
	_rosterBase = base;
	_sortLabels = std::move(labels);
	_order = new TextButton(1, 1);
	_details = new TextButton(1, 1);
	_assign = new TextButton(1, 1);
	_clear = new TextButton(1, 1);
	for (auto *button : {_order, _details, _assign, _clear}) add(button, "button", style);
	_order->setText(tr("STR_SORT_BY"));
	_details->setText(tr("STR_CAL_TRAIN_DETAILS"));
	_assign->setText(tr("STR_CAL_TRAIN_ASSIGN"));
	_clear->setText(tr("STR_CAL_TRAIN_CLEAR"));
	_order->onMouseClick((ActionHandler)&CalypsoTrainingRoster::rosterOrder);
	_details->onMouseClick((ActionHandler)&CalypsoTrainingRoster::rosterDetails);
	_assign->onMouseClick(assign);
	_clear->onMouseClick(clear);
}

size_t CalypsoTrainingRoster::diverIndex() const
{
	const auto &soldiers = *_rosterBase->getSoldiers();
	return std::find(soldiers.begin(), soldiers.end(), _selectedDiver) - soldiers.begin();
}

void CalypsoTrainingRoster::rosterDetails(Action *)
{
	const auto index = diverIndex();
	if (index < _rosterBase->getSoldiers()->size()) _game->pushState(new SoldierInfoState(_rosterBase, index, true, true));
}

void CalypsoTrainingRoster::rosterOrder(Action *) { _game->pushState(new CalypsoTrainingOrder(*this)); }
CalypsoTrainingRoster::~CalypsoTrainingRoster() { delete _ledger; }

void CalypsoTrainingRoster::resize(int &dx, int &dy)
{
	if (_ledger) _ledger->refresh();
	else State::resize(dx, dy);
}

CalypsoTrainingOrder::CalypsoTrainingOrder(CalypsoTrainingRoster &roster) : _roster(roster)
{
	setInterface("martialTraining");
	_rows = new TextList(250, 100);
	add(_rows, "text", "martialTraining");
	_rows->setColumns(2, 175, 75);
	_rows->setSelectable(true);
	_rows->onMouseClick((ActionHandler)&CalypsoTrainingOrder::sort);
	_up = new TextButton(1, 1);
	_down = new TextButton(1, 1);
	_reverse = new TextButton(1, 1);
	_done = new TextButton(1, 1);
	for (auto *button : {_up, _down, _reverse, _done}) add(button, "button2", "martialTraining");
	_up->setText(tr("STR_CAL_TRAIN_UP"));
	_down->setText(tr("STR_CAL_TRAIN_DOWN"));
	_reverse->setText(tr("STR_CAL_TRAIN_REVERSE"));
	_done->setText(tr("STR_OK"));
	_up->onMouseClick((ActionHandler)&CalypsoTrainingOrder::move);
	_down->onMouseClick((ActionHandler)&CalypsoTrainingOrder::move);
	_reverse->onMouseClick((ActionHandler)&CalypsoTrainingOrder::reverse);
	_done->onMouseClick((ActionHandler)&CalypsoTrainingOrder::close);
	_done->onKeyboardPress((ActionHandler)&CalypsoTrainingOrder::close, Options::keyCancel);
	populate();
	_ledger = new Calypso::CalypsoBaseLedger(*this);
}
CalypsoTrainingOrder::~CalypsoTrainingOrder() { delete _ledger; }

void CalypsoTrainingOrder::populate()
{
	_rows->clearList();
	for (size_t i = 0; i < _roster._sortLabels.size(); ++i)
		_rows->addRow(2, _roster._sortLabels[i].c_str(), i == _roster._sortIndex() ? tr("STR_YES").c_str() : "");
}

void CalypsoTrainingOrder::sort(Action *)
{
	const auto index = _rows->getSelectedRow();
	if (index >= _roster._sortLabels.size()) return;
	_roster._applySort(index, _roster._reverseOrder);
	_game->popState();
}

void CalypsoTrainingOrder::reverse(Action *)
{
	_roster._reverseOrder = !_roster._reverseOrder;
	_ledger->refresh();
}

void CalypsoTrainingOrder::move(Action *action)
{
	const size_t index = _roster.diverIndex(), count = _roster._rosterBase->getSoldiers()->size();
	const int direction = action->getSender() == _up ? -1 : 1;
	if (index >= count || (direction < 0 ? index == 0 : index + 1 == count)) return;
	_roster._moveDiver(index, direction);
	populate();
	_ledger->refresh();
}
void CalypsoTrainingOrder::close(Action *) { _game->popState(); }
void CalypsoTrainingOrder::resize(int &dx, int &dy)
{
	if (_ledger) _ledger->refresh();
	else State::resize(dx, dy);
}
}
#endif
