#ifdef __EMSCRIPTEN__
#include "CalypsoTrainingBases.h"
#include "CalypsoBaseLedger.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Geoscape/AllocatePsiTrainingState.h"
#include "../Geoscape/AllocateTrainingState.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Savegame/Base.h"
#include "../Savegame/SavedGame.h"

namespace OpenXcom {
CalypsoTrainingBases::CalypsoTrainingBases(bool psi) : _psi(psi)
{
	setInterface("martialTraining");
	_rows = new TextList(250, 100, 0, 0);
	add(_rows, "text", "martialTraining");
	_rows->setColumns(4, 100, 50, 50, 50);
	_rows->setSelectable(true);
	_rows->onMouseClick((ActionHandler)&CalypsoTrainingBases::openBase);

	_physical = new TextButton(1, 1);
	_psionic = new TextButton(1, 1);
	_done = new TextButton(1, 1);
	for (auto *button : {_physical, _psionic, _done}) add(button, "button2", "martialTraining");
	_physical->setText(tr("STR_PHYSICAL_TRAINING"));
	_psionic->setText(tr("STR_PSIONIC_TRAINING"));
	_done->setText(tr("STR_OK"));
	_physical->onMouseClick((ActionHandler)&CalypsoTrainingBases::changeMode);
	_psionic->onMouseClick((ActionHandler)&CalypsoTrainingBases::changeMode);
	_done->onMouseClick((ActionHandler)&CalypsoTrainingBases::close);
	_done->onKeyboardPress((ActionHandler)&CalypsoTrainingBases::close, Options::keyCancel);
	populate();
	_ledger = new Calypso::CalypsoBaseLedger(*this);
}

CalypsoTrainingBases::~CalypsoTrainingBases() { delete _ledger; }

void CalypsoTrainingBases::init()
{
	State::init();
	populate();
}

void CalypsoTrainingBases::populate()
{
	_rows->clearList();
	_capacity = 0;
	_used = 0;
	for (auto *base : *_game->getSavedGame()->getBases())
	{
		const int capacity = _psi ? base->getAvailablePsiLabs() : base->getAvailableTraining();
		const int used = _psi ? base->getUsedPsiLabs() : base->getUsedTraining();
		const auto free = capacity ? std::to_string(std::max(0, capacity - used)) : std::string(tr("STR_CAL_NO_TRAINING_FACILITY"));
		_rows->addRow(4, base->getName().c_str(), std::to_string(used).c_str(), std::to_string(capacity).c_str(), free.c_str());
		_capacity += capacity;
		_used += used;
	}
	if (_ledger) _ledger->refresh();
}

void CalypsoTrainingBases::changeMode(Action *action)
{
	const bool psi = action->getSender() == _psionic;
	if (_psi == psi) return;
	_psi = psi;
	populate();
}

void CalypsoTrainingBases::openBase(Action *)
{
	const auto index = _rows->getSelectedRow();
	const auto &bases = *_game->getSavedGame()->getBases();
	if (index >= bases.size()) return;
	auto *base = bases[index];
	if (_psi && base->getAvailablePsiLabs()) _game->pushState(new AllocatePsiTrainingState(base));
	else if (!_psi && base->getAvailableTraining()) _game->pushState(new AllocateTrainingState(base));
}

void CalypsoTrainingBases::close(Action *) { _game->popState(); }

void CalypsoTrainingBases::resize(int &dx, int &dy)
{
	if (_ledger) _ledger->refresh();
	else State::resize(dx, dy);
}
}
#endif
