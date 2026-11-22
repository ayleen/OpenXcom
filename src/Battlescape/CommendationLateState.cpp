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
#include "CommendationLateState.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoStrategicList.h"
#endif
#include <sstream>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/SoldierDiary.h"
#include "../Engine/Options.h"
#include "../Mod/RuleCommendations.h"
#include "../Ufopaedia/Ufopaedia.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Medals screen.
 * @param soldiersMedalled List of soldiers with medals.
 */
CommendationLateState::CommendationLateState(std::vector<Soldier*> soldiersMedalled)
{
	// Create object
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(288, 16, 16, 176);
	_txtTitle = new Text(300, 16, 10, 8);
	_lstSoldiers = new TextList(288, 128, 8, 32);

	// Set palette
	setInterface("commendationsLate");

	add(_window, "window", "commendationsLate");
	add(_btnOk, "button", "commendationsLate");
	add(_txtTitle, "text", "commendationsLate");
	add(_lstSoldiers, "list", "commendationsLate");

	centerAllSurfaces();

	// Set up object
#ifndef __EMSCRIPTEN__
	setWindowBackground(_window, "commendationsLate");
#endif

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&CommendationLateState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&CommendationLateState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&CommendationLateState::btnOkClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_LOST_IN_SERVICE"));

	_lstSoldiers->setColumns(3, 114, 90, 84);
	_lstSoldiers->setSelectable(true);
	_lstSoldiers->setBackground(_window);
	_lstSoldiers->setMargin(8);
	_lstSoldiers->setFlooding(true);
	_lstSoldiers->onMouseClick((ActionHandler)&CommendationLateState::lstSoldiersMouseClick);

    /***

                                            LOST IN SERVICE

    SOLDIER NAME, RANK: ___, SCORE: ___, KILLS: ___, NUMBER OF MISSIONS: ___, DAYS WOUNDED: ___, TIMES HIT: ___
      COMMENDATION
      COMMENDATION
      COMMENDATION
      CAUSE OF DEATH: KILLED BY ALIEN_RACE ALIEN_RANK, USING WEAPON


    ***/

	const auto& commendationsList = _game->getMod()->getCommendationsList();
	bool modularCommendation;
	std::string noun;


	int row = 0;
	// Loop over dead soldiers
	for (auto* soldier : soldiersMedalled)
	{
		// Establish some base information
		_lstSoldiers->addRow(3, soldier->getName().c_str(),
								tr(soldier->getRankString()).c_str(),
								tr("STR_KILLS").arg(soldier->getDiary()->getKillTotal()).c_str());
		_lstSoldiers->setRowColor(row, _lstSoldiers->getSecondaryColor());
		_commendationsNames.push_back("");
		row++;

		// Loop over all commendation
		for (auto commIter = commendationsList.begin(); commIter != commendationsList.end();)
		{
			const auto& commType = (*commIter).first;
			const auto* commRule = (*commIter).second;

			std::ostringstream wssCommendation;
			modularCommendation = false;
			noun = "noNoun";

			// Loop over soldier's commendation
			for (auto* soldierComm : *soldier->getDiary()->getSoldierCommendations())
			{
				if (soldierComm->getType() == commType && soldierComm->isNew() && noun == "noNoun")
				{
					soldierComm->makeOld();

					if (soldierComm->getNoun() != "noNoun")
					{
						noun = soldierComm->getNoun();
						modularCommendation = true;
					}
					// Decoration level name
					int skipCounter = 0;
					int lastInt = -2;
					int thisInt = -1;
					int vectorIterator = 0;
					for (auto wtf = commRule->getCriteria()->begin()->second.begin(); wtf != commRule->getCriteria()->begin()->second.end(); ++wtf)
					{
						if (vectorIterator == soldierComm->getDecorationLevelInt() + 1)
						{
							break;
						}
						thisInt = *wtf;
						if (wtf != commRule->getCriteria()->begin()->second.begin())
						{
							--wtf;
							lastInt = *wtf;
							++wtf;
						}
						if (thisInt == lastInt)
						{
							skipCounter++;
						}
						vectorIterator++;
					}
					// Establish commendation's name
					// Medal name
					wssCommendation << "   ";
					if (modularCommendation)
					{
						wssCommendation << tr(commType).arg(tr(noun));
					}
					else
					{
						wssCommendation << tr(commType);
					}
					_lstSoldiers->addRow(3, wssCommendation.str().c_str(), "", tr(soldierComm->getDecorationLevelName(skipCounter)).c_str());
					_commendationsNames.push_back(commType);
					row++;
					break;
				}
			} // END SOLDIER COMMS LOOP

			if (noun == "noNoun")
			{
				++commIter;
			}
		} // END COMMS LOOPS
		_lstSoldiers->addRow(3, "", "", ""); // Separator
		_commendationsNames.push_back("");
		row++;
	} // END SOLDIER LOOP    
#ifdef __EMSCRIPTEN__
	_hdList = new Calypso::CalypsoStrategicList(this, nullptr, _game->getMod(), _window, _txtTitle,
		_lstSoldiers, _btnOk, Calypso::CalypsoStrategicList::Content::FallenCommendations);
	_hdList->setArticleRows(_commendationsNames);
	enableUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	_hdList->fitInput();
#endif
}

/**
 *
 */
CommendationLateState::~CommendationLateState()
{
#ifdef __EMSCRIPTEN__
	delete _hdList;
#endif
}

#ifdef __EMSCRIPTEN__
void CommendationLateState::resize(int &dx, int &dy)
{
	if (!_hdList) { State::resize(dx, dy); return; }
	_hdList->layout();
	recaptureUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	_hdList->fitInput();
}
#endif

/*
*
*/
void CommendationLateState::lstSoldiersMouseClick(Action *)
{
	const auto row = _lstSoldiers->getSelectedRow();
	if (row < _commendationsNames.size() && !_commendationsNames[row].empty())
		Ufopaedia::openArticle(_game, _commendationsNames[row]);
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void CommendationLateState::btnOkClick(Action *)
{
	_game->popState();
}

}
