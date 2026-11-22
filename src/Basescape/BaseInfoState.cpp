/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "BaseInfoState.h"
#ifdef __EMSCRIPTEN__
#include <algorithm>
#include "../Calypso/CalypsoBaseLedger.h"
#include "../Interface/TextList.h"
#endif
#include <sstream>
#include <cmath>
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Mod/Mod.h"
#include "../Engine/Options.h"
#include "../Interface/Bar.h"
#include "../Interface/TextButton.h"
#include "../Interface/Text.h"
#include "../Interface/TextEdit.h"
#include "../Engine/Surface.h"
#include "MiniBaseView.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "MonthlyCostsState.h"
#include "TransfersState.h"
#include "StoresState.h"
#include "BasescapeState.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Base Info screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param state Pointer to the Basescape state.
 */
BaseInfoState::BaseInfoState(Base *base, BasescapeState *state) : _base(base), _state(state)
{
	// Create objects
	_bg = new Surface(320, 200, 0, 0);
	_mini = new MiniBaseView(128, 16, 182, 8);
	_btnOk = new TextButton(30, 14, 10, 180);
	_btnTransfers = new TextButton(80, 14, 46, 180);
	_btnStores = new TextButton(80, 14, 132, 180);
	_btnMonthlyCosts = new TextButton(92, 14, 218, 180);
	_edtBase = new TextEdit(this, 127, 16, 8, 8);

	_txtPersonnel = new Text(300, 9, 8, 30);
	_txtSoldiers = new Text(114, 9, 8, 41);
	_numSoldiers = new Text(40, 9, 126, 41);
	_barSoldiers = new Bar(150, 5, 166, 43);
	_txtEngineers = new Text(114, 9, 8, 51);
	_numEngineers = new Text(40, 9, 126, 51);
	_barEngineers = new Bar(150, 5, 166, 53);
	_txtScientists = new Text(114, 9, 8, 61);
	_numScientists = new Text(40, 9, 126, 61);
	_barScientists = new Bar(150, 5, 166, 63);

	_txtSpace = new Text(300, 9, 8, 72);
	_txtQuarters = new Text(114, 9, 8, 83);
	_numQuarters = new Text(40, 9, 126, 83);
	_barQuarters = new Bar(150, 5, 166, 85);
	_txtStores = new Text(114, 9, 8, 93);
	_numStores = new Text(40, 9, 126, 93);
	_barStores = new Bar(150, 5, 166, 95);
	_txtLaboratories = new Text(114, 9, 8, 103);
	_numLaboratories = new Text(40, 9, 126, 103);
	_barLaboratories = new Bar(150, 5, 166, 105);
	_txtWorkshops = new Text(114, 9, 8, 113);
	_numWorkshops = new Text(40, 9, 126, 113);
	_barWorkshops = new Bar(150, 5, 166, 115);
	if (Options::storageLimitsEnforced)
	{
		_txtContainment = new Text(114, 9, 8, 123);
		_numContainment = new Text(40, 9, 126, 123);
		_barContainment = new Bar(150, 5, 166, 125);
	}
	_txtHangars = new Text(114, 9, 8, Options::storageLimitsEnforced ? 133 : 123);
	_numHangars = new Text(40, 9, 126, Options::storageLimitsEnforced ? 133 : 123);
	_barHangars = new Bar(150, 5, 166, Options::storageLimitsEnforced ? 135 : 125);

	_txtDefense = new Text(114, 9, 8, Options::storageLimitsEnforced ? 147 : 138);
	_numDefense = new Text(40, 9, 126, Options::storageLimitsEnforced ? 147 : 138);
	_barDefense = new Bar(150, 5, 166, Options::storageLimitsEnforced ? 149 : 140);
	_txtShortRange = new Text(114, 9, 8, Options::storageLimitsEnforced ? 157 : 153);
	_numShortRange = new Text(40, 9, 126, Options::storageLimitsEnforced ? 157 : 153);
	_barShortRange = new Bar(150, 5, 166, Options::storageLimitsEnforced ? 159 : 155);
	_txtLongRange = new Text(114, 9, 8, Options::storageLimitsEnforced ? 167 : 163);
	_numLongRange = new Text(40, 9, 126, Options::storageLimitsEnforced ? 167 : 163);
	_barLongRange = new Bar(150, 5, 166, Options::storageLimitsEnforced ? 169 : 165);

	// Set palette
	setInterface("baseInfo");

	add(_bg);
	add(_mini, "miniBase", "basescape");
	add(_btnOk, "button", "baseInfo");
	add(_btnTransfers, "button", "baseInfo");
	add(_btnStores, "button", "baseInfo");
	add(_btnMonthlyCosts, "button", "baseInfo");
	add(_edtBase, "text1", "baseInfo");

	add(_txtPersonnel, "text1", "baseInfo");
	add(_txtSoldiers, "text2", "baseInfo");
	add(_numSoldiers, "numbers", "baseInfo");
	add(_barSoldiers, "personnelBars", "baseInfo");
	add(_txtEngineers, "text2", "baseInfo");
	add(_numEngineers, "numbers", "baseInfo");
	add(_barEngineers, "personnelBars", "baseInfo");
	add(_txtScientists, "text2", "baseInfo");
	add(_numScientists, "numbers", "baseInfo");
	add(_barScientists, "personnelBars", "baseInfo");

	add(_txtSpace, "text1", "baseInfo");
	add(_txtQuarters, "text2", "baseInfo");
	add(_numQuarters, "numbers", "baseInfo");
	add(_barQuarters, "facilityBars", "baseInfo");
	add(_txtStores, "text2", "baseInfo");
	add(_numStores, "numbers", "baseInfo");
	add(_barStores, "facilityBars", "baseInfo");
	add(_txtLaboratories, "text2", "baseInfo");
	add(_numLaboratories, "numbers", "baseInfo");
	add(_barLaboratories, "facilityBars", "baseInfo");
	add(_txtWorkshops, "text2", "baseInfo");
	add(_numWorkshops, "numbers", "baseInfo");
	add(_barWorkshops, "facilityBars", "baseInfo");
	if (Options::storageLimitsEnforced)
	{
		add(_txtContainment, "text2", "baseInfo");
		add(_numContainment, "numbers", "baseInfo");
		add(_barContainment, "facilityBars", "baseInfo");
	}
	add(_txtHangars, "text2", "baseInfo");
	add(_numHangars, "numbers", "baseInfo");
	add(_barHangars, "facilityBars", "baseInfo");

	add(_txtDefense, "text2", "baseInfo");
	add(_numDefense, "numbers", "baseInfo");
	add(_barDefense, "defenceBar", "baseInfo");
	add(_txtShortRange, "text2", "baseInfo");
	add(_numShortRange, "numbers", "baseInfo");
	add(_barShortRange, "detectionBars", "baseInfo");
	add(_txtLongRange, "text2", "baseInfo");
	add(_numLongRange, "numbers", "baseInfo");
	add(_barLongRange, "detectionBars", "baseInfo");

	centerAllSurfaces();

	// Set up objects
	std::ostringstream ss;
	if (Options::storageLimitsEnforced)
	{
		ss << "ALT";
	}
	ss << "BACK07.SCR";
	_game->getMod()->getSurface(ss.str())->blitNShade(_bg, 0, 0);

	_mini->setTexture(_game->getMod()->getSurfaceSet("BASEBITS.PCK"));
	_mini->setBases(_game->getSavedGame()->getBases());
	for (size_t i = 0; i < _game->getSavedGame()->getBases()->size(); ++i)
	{
		if (_game->getSavedGame()->getBases()->at(i) == _base)
		{
			_mini->setSelectedBase(i);
			break;
		}
	}
	_mini->onMouseClick((ActionHandler)&BaseInfoState::miniClick);
	_mini->onKeyboardPress((ActionHandler)&BaseInfoState::handleKeyPress);

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&BaseInfoState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&BaseInfoState::btnOkClick, Options::keyCancel);

	_btnTransfers->setText(tr("STR_TRANSFERS_UC"));
	_btnTransfers->onMouseClick((ActionHandler)&BaseInfoState::btnTransfersClick);

	_btnStores->setText(tr("STR_STORES_UC"));
	_btnStores->onMouseClick((ActionHandler)&BaseInfoState::btnStoresClick);

	_btnMonthlyCosts->setText(tr("STR_MONTHLY_COSTS"));
	_btnMonthlyCosts->onMouseClick((ActionHandler)&BaseInfoState::btnMonthlyCostsClick);

	_edtBase->setBig();
	_edtBase->onChange((ActionHandler)&BaseInfoState::edtBaseChange);

	_txtPersonnel->setText(tr("STR_PERSONNEL_AVAILABLE_PERSONNEL_TOTAL"));

	_txtSoldiers->setText(tr("STR_SOLDIERS"));

	_barSoldiers->setScale(1.0);

	_txtEngineers->setText(tr("STR_ENGINEERS"));

	_barEngineers->setScale(1.0);

	_txtScientists->setText(tr("STR_SCIENTISTS"));

	_barScientists->setScale(1.0);


	_txtSpace->setText(tr("STR_SPACE_USED_SPACE_AVAILABLE"));

	_txtQuarters->setText(tr("STR_LIVING_QUARTERS_PLURAL"));

	_barQuarters->setScale(0.5);

	_txtStores->setText(tr("STR_STORES"));

	_barStores->setScale(0.5);

	_txtLaboratories->setText(tr("STR_LABORATORIES"));

	_barLaboratories->setScale(0.5);

	_txtWorkshops->setText(tr("STR_WORK_SHOPS"));

	_barWorkshops->setScale(0.5);

	if (Options::storageLimitsEnforced)
	{
		_txtContainment->setText(tr("STR_ALIEN_CONTAINMENT"));

		_barContainment->setScale(0.5);
	}

	_txtHangars->setText(tr("STR_HANGARS"));

	_barHangars->setScale(18.0);


	_txtDefense->setText(tr("STR_DEFENSE_STRENGTH"));

	_barDefense->setScale(0.125);

	_txtShortRange->setText(tr("STR_SHORT_RANGE_DETECTION"));

	_barShortRange->setScale(25.0);

	_txtLongRange->setText(tr("STR_LONG_RANGE_DETECTION"));

	_barLongRange->setScale(25.0);
#ifdef __EMSCRIPTEN__
	_screen = true;
	_hdRows = new TextList(280, 100, 0, 0);
	add(_hdRows, "text", "baseInfo");
	_hdRows->setColumns(5, 100, 55, 45, 45, 35);
	_hdRows->setSelectable(true);
	_previousBase = new TextButton(80, 16, 0, 0);
	_nextBase = new TextButton(80, 16, 0, 0);
	add(_previousBase, "button", "baseInfo");
	add(_nextBase, "button", "baseInfo");
	_previousBase->setText(tr("STR_CAL_PREVIOUS_BASE"));
	_nextBase->setText(tr("STR_CAL_NEXT_BASE"));
	_previousBase->onMouseClick((ActionHandler)&BaseInfoState::changeBase);
	_nextBase->onMouseClick((ActionHandler)&BaseInfoState::changeBase);
	_mini->setX(-_mini->getWidth());
	_edtBase->setText(_base->getName());
	refreshHdRows();
	_hdLedger = new Calypso::CalypsoBaseLedger(*this);
#endif
}

/**
 *
 */
BaseInfoState::~BaseInfoState()
{
#ifdef __EMSCRIPTEN__
	delete _hdLedger;
#endif

}

/**
 * The player can change the selected base.
 */
void BaseInfoState::init()
{
	State::init();
	_edtBase->setText(_base->getName());

	std::ostringstream ss;
	ss << _base->getAvailableSoldiers() << ":" << _base->getTotalSoldiers();
	_numSoldiers->setText(ss.str());

	if (!Options::oxceBaseInfoScaleEnabled || _base->getTotalSoldiers() * _barSoldiers->getScale() < MAX_BAR_WIDTH)
	{
		_barSoldiers->setMax(_base->getTotalSoldiers());
		_barSoldiers->setValue(_base->getAvailableSoldiers());
	}
	else
	{
		_barSoldiers->setMax(MAX_BAR_WIDTH);
		_barSoldiers->setValue(_base->getAvailableSoldiers() * MAX_BAR_WIDTH / _base->getTotalSoldiers());
	}

	std::ostringstream ss2;
	ss2 << _base->getAvailableEngineers() << ":" << _base->getTotalEngineers();
	_numEngineers->setText(ss2.str());

	if (!Options::oxceBaseInfoScaleEnabled || _base->getTotalEngineers() * _barEngineers->getScale() < MAX_BAR_WIDTH)
	{
		_barEngineers->setMax(_base->getTotalEngineers());
		_barEngineers->setValue(_base->getAvailableEngineers());
	}
	else
	{
		_barEngineers->setMax(MAX_BAR_WIDTH);
		_barEngineers->setValue(_base->getAvailableEngineers() * MAX_BAR_WIDTH / _base->getTotalEngineers());
	}

	std::ostringstream ss3;
	ss3 << _base->getAvailableScientists() << ":" << _base->getTotalScientists();
	_numScientists->setText(ss3.str());

	if (!Options::oxceBaseInfoScaleEnabled || _base->getTotalScientists() * _barScientists->getScale() < MAX_BAR_WIDTH)
	{
		_barScientists->setMax(_base->getTotalScientists());
		_barScientists->setValue(_base->getAvailableScientists());
	}
	else
	{
		_barScientists->setMax(MAX_BAR_WIDTH);
		_barScientists->setValue(_base->getAvailableScientists() * MAX_BAR_WIDTH / _base->getTotalScientists());
	}


	std::ostringstream ss4;
	ss4 << _base->getUsedQuarters() << ":" << _base->getAvailableQuarters();
	_numQuarters->setText(ss4.str());

	if (!Options::oxceBaseInfoScaleEnabled || _base->getAvailableQuarters() * _barQuarters->getScale() < MAX_BAR_WIDTH)
	{
		_barQuarters->setMax(_base->getAvailableQuarters());
		_barQuarters->setValue(_base->getUsedQuarters());
	}
	else
	{
		_barQuarters->setMax(MAX_BAR_WIDTH);
		_barQuarters->setValue(_base->getUsedQuarters() * MAX_BAR_WIDTH / _base->getAvailableQuarters());
		_barQuarters->setScale(1.0);
	}

	std::ostringstream ss5;
	ss5 << (int)floor(_base->getUsedStores() + 0.05) << ":" << _base->getAvailableStores();
	_numStores->setText(ss5.str());

	if (!Options::oxceBaseInfoScaleEnabled || _base->getAvailableStores() * _barStores->getScale() < MAX_BAR_WIDTH)
	{
		_barStores->setMax(_base->getAvailableStores());
		_barStores->setValue((int)floor(_base->getUsedStores() + 0.05));
	}
	else
	{
		_barStores->setMax(MAX_BAR_WIDTH);
		_barStores->setValue(((int)floor(_base->getUsedStores() + 0.05)) * MAX_BAR_WIDTH / _base->getAvailableStores());
		_barStores->setScale(1.0);
	}

	std::ostringstream ss6;
	ss6 << _base->getUsedLaboratories() << ":" << _base->getAvailableLaboratories();
	_numLaboratories->setText(ss6.str());

	if (!Options::oxceBaseInfoScaleEnabled || _base->getAvailableLaboratories() * _barLaboratories->getScale() < MAX_BAR_WIDTH)
	{
		_barLaboratories->setMax(_base->getAvailableLaboratories());
		_barLaboratories->setValue(_base->getUsedLaboratories());
	}
	else
	{
		_barLaboratories->setMax(MAX_BAR_WIDTH);
		_barLaboratories->setValue(_base->getUsedLaboratories() * MAX_BAR_WIDTH / _base->getAvailableLaboratories());
		_barLaboratories->setScale(1.0);
	}

	std::ostringstream ss7;
	ss7 << _base->getUsedWorkshops() << ":" << _base->getAvailableWorkshops();
	_numWorkshops->setText(ss7.str());

	if (!Options::oxceBaseInfoScaleEnabled || _base->getAvailableWorkshops() * _barWorkshops->getScale() < MAX_BAR_WIDTH)
	{
		_barWorkshops->setMax(_base->getAvailableWorkshops());
		_barWorkshops->setValue(_base->getUsedWorkshops());
	}
	else
	{
		_barWorkshops->setMax(MAX_BAR_WIDTH);
		_barWorkshops->setValue(_base->getUsedWorkshops() * MAX_BAR_WIDTH / _base->getAvailableWorkshops());
		_barWorkshops->setScale(1.0);
	}

	if (Options::storageLimitsEnforced)
	{
		std::ostringstream ss72;
		ss72 << _base->getUsedContainment(0) << ":" << _base->getAvailableContainment(0);
		_numContainment->setText(ss72.str());

		_barContainment->setMax(_base->getAvailableContainment(0));
		_barContainment->setValue(_base->getUsedContainment(0));
	}

	std::ostringstream ss8;
	ss8 << _base->getUsedHangars() << ":" << _base->getAvailableHangars();
	_numHangars->setText(ss8.str());

	_barHangars->setMax(_base->getAvailableHangars());
	_barHangars->setValue(_base->getUsedHangars());


	std::ostringstream ss9;
	ss9 << _base->getDefenseValue();
	_numDefense->setText(ss9.str());

	_barDefense->setMax(_base->getDefenseValue());
	_barDefense->setValue(_base->getDefenseValue());
	if (Options::oxceBaseInfoDefenseScaleMultiplier != 100)
	{
		_barDefense->setScale(0.125 * Options::oxceBaseInfoDefenseScaleMultiplier / 100.0);
	}

	std::ostringstream ss10;
	int shortRangeDetection = _base->getShortRangeDetection();
	ss10 << shortRangeDetection;
	_numShortRange->setText(ss10.str());

	_barShortRange->setMax(shortRangeDetection);
	_barShortRange->setValue(shortRangeDetection);

	std::ostringstream ss11;
	int longRangeDetection = _base->getLongRangeDetection();
	ss11 << longRangeDetection;
	_numLongRange->setText(ss11.str());

	_barLongRange->setMax(longRangeDetection);
	_barLongRange->setValue(longRangeDetection);
#ifdef __EMSCRIPTEN__
	refreshHdRows();
#endif
}

/**
 * Changes the base name.
 * @param action Pointer to an action.
 */
void BaseInfoState::edtBaseChange(Action *)
{
	_base->setName(_edtBase->getText());
}

/**
 * Selects a new base to display.
 * @param action Pointer to an action.
 */
void BaseInfoState::miniClick(Action *)
{
	size_t base = _mini->getHoveredBase();
	if (base < _game->getSavedGame()->getBases()->size())
	{
		_mini->setSelectedBase(base);
		_base = _game->getSavedGame()->getBases()->at(base);
		if (_state) _state->setBase(_base);
		init();
	}
}

/**
 * Selects a new base to display.
 * @param action Pointer to an action.
 */
void BaseInfoState::handleKeyPress(Action *action)
{
	if (action->getDetails()->type == SDL_KEYDOWN)
	{
		SDLKey baseKeys[] = {
			Options::keyBaseSelect1,
			Options::keyBaseSelect2,
			Options::keyBaseSelect3,
			Options::keyBaseSelect4,
			Options::keyBaseSelect5,
			Options::keyBaseSelect6,
			Options::keyBaseSelect7,
			Options::keyBaseSelect8
		};
		int key = action->getDetails()->key.keysym.sym;
		for (size_t i = 0; i < _game->getSavedGame()->getBases()->size() && i < 8; ++i)
		{
			if (key == baseKeys[i])
			{
				_mini->setSelectedBase(i);
				_base = _game->getSavedGame()->getBases()->at(i);
				if (_state) _state->setBase(_base);
				init();
				break;
			}
		}
	}
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void BaseInfoState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Goes to the Transfers window.
 * @param action Pointer to an action.
 */
void BaseInfoState::btnTransfersClick(Action *)
{
	_game->pushState(new TransfersState(_base));
}

/**
 * Goes to the Stores screen.
 * @param action Pointer to an action.
 */
void BaseInfoState::btnStoresClick(Action *)
{
	_game->pushState(new StoresState(_base));
}

/**
 * Goes to the Monthly Costs screen.
 * @param action Pointer to an action.
 */
void BaseInfoState::btnMonthlyCostsClick(Action *)
{
	_game->pushState(new MonthlyCostsState(_base));
}

#ifdef __EMSCRIPTEN__
void BaseInfoState::changeBase(Action *action)
{
	const auto &bases = *_game->getSavedGame()->getBases();
	if (bases.empty()) return;
	auto it = std::find(bases.begin(), bases.end(), _base);
	if (it == bases.end()) return;
	const auto index = static_cast<size_t>(it - bases.begin());
	const auto next = (index + (action->getSender() == _nextBase ? 1 : bases.size() - 1)) % bases.size();
	_mini->setSelectedBase(next);
	_base = bases[next];
	if (_state) _state->setBase(_base);
	init();
}

void BaseInfoState::refreshHdRows()
{
	_hdRows->clearList();
	auto add = [&](const Text *label, bool capacity, int current, int total)
	{
		const auto excess = capacity && current > total ? std::to_string(current - total) : std::string();
		_hdRows->addRow(5, label->getText().c_str(),
			std::string(tr(capacity ? "STR_CAL_BASE_USED" : "STR_CAL_BASE_AVAILABLE")).c_str(),
			std::to_string(current).c_str(), std::to_string(total).c_str(), excess.c_str());
	};
	add(_txtSoldiers, false, _base->getAvailableSoldiers(), _base->getTotalSoldiers());
	add(_txtEngineers, false, _base->getAvailableEngineers(), _base->getTotalEngineers());
	add(_txtScientists, false, _base->getAvailableScientists(), _base->getTotalScientists());
	add(_txtQuarters, true, _base->getUsedQuarters(), _base->getAvailableQuarters());
	add(_txtStores, true, static_cast<int>(floor(_base->getUsedStores() + 0.05)), _base->getAvailableStores());
	add(_txtLaboratories, true, _base->getUsedLaboratories(), _base->getAvailableLaboratories());
	add(_txtWorkshops, true, _base->getUsedWorkshops(), _base->getAvailableWorkshops());
	if (Options::storageLimitsEnforced)
		add(_txtContainment, true, _base->getUsedContainment(0), _base->getAvailableContainment(0));
	add(_txtHangars, true, _base->getUsedHangars(), _base->getAvailableHangars());
}

void BaseInfoState::resize(int &dX, int &dY)
{
	if (_hdLedger) _hdLedger->refresh();
	else State::resize(dX, dY);
}
#endif

}
