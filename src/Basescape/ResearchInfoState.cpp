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
#include "ResearchInfoState.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoF09ResearchUi.h"
#endif
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Savegame/Base.h"
#include "../Mod/RuleResearch.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/ResearchProject.h"
#include "../Savegame/SavedGame.h"
#include "../Interface/ArrowButton.h"
#include "../Engine/Timer.h"
#include "../Engine/RNG.h"
#include <climits>

namespace OpenXcom
{

/**
 * Initializes all the elements in the ResearchProject screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param rule A RuleResearch which will be used to create a new ResearchProject
 */
ResearchInfoState::ResearchInfoState(Base *base, RuleResearch *rule) : _base(base), _project(new ResearchProject(rule)), _rule(rule), _transaction(true)
{
	buildUi();
}

/**
 * Initializes all the elements in the ResearchProject screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param project A ResearchProject to modify
 */
ResearchInfoState::ResearchInfoState(Base *base, ResearchProject *project) : _base(base), _project(project), _rule(0), _transaction(false)
{
	buildUi();
}

/**
 * Builds dialog.
 */
void ResearchInfoState::buildUi()
{
	_screen = false;

	_window = new Window(this, 230, 140, 45, 30);
	_txtTitle = new Text(210, 17, 61, 40);

	_txtAvailableScientist = new Text(210, 9, 61, 60);
	_txtAvailableSpace = new Text(210, 9, 61, 70);
	_txtAllocatedScientist = new Text(210, 17, 61, 80);
	_txtMore = new Text(110, 17, 85, 100);
	_txtLess = new Text(110, 17, 85, 120);
	_btnCancel = new TextButton(90, 16, 61, 145);
	_btnOk = new TextButton(90, 16, 169, 145);

	_btnMore = new ArrowButton(ARROW_BIG_UP, 13, 14, 195, 100);
	_btnLess = new ArrowButton(ARROW_BIG_DOWN, 13, 14, 195, 120);

	_surfaceScientists = new InteractiveSurface(230, 140, 45, 30);
	_surfaceScientists->onMouseClick((ActionHandler)&ResearchInfoState::handleWheel, 0);

	// Set palette
	setInterface("allocateResearch");

	add(_surfaceScientists);
	add(_window, "window", "allocateResearch");
	add(_btnOk, "button2", "allocateResearch");
	add(_btnCancel, "button2", "allocateResearch");
	add(_txtTitle, "text", "allocateResearch");
	add(_txtAvailableScientist, "text", "allocateResearch");
	add(_txtAvailableSpace, "text", "allocateResearch");
	add(_txtAllocatedScientist, "text", "allocateResearch");
	add(_txtMore, "text", "allocateResearch");
	add(_txtLess, "text", "allocateResearch");
	add(_btnMore, "button1", "allocateResearch");
	add(_btnLess, "button1", "allocateResearch");

	centerAllSurfaces();

	// Set up objects
	setWindowBackground(_window, "allocateResearch");

	_txtTitle->setBig();

	_txtTitle->setText(_rule ? tr(_rule->getName()) : tr(_project->getRules()->getName()));

	_txtAllocatedScientist->setBig();

	_txtMore->setText(tr("STR_INCREASE"));
	_txtLess->setText(tr("STR_DECREASE"));

	_txtMore->setBig();
	_txtLess->setBig();

	setAssignedScientist();
	_timerMore = new Timer(250);
	_timerMore->onTimer((StateHandler)&ResearchInfoState::more);
	_timerLess = new Timer(250);
	_timerLess->onTimer((StateHandler)&ResearchInfoState::less);

	_btnOk->onMouseClick((ActionHandler)&ResearchInfoState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ResearchInfoState::btnOkClick, Options::keyOk);
	if (_rule)
	{
		_btnOk->setText(tr("STR_START_PROJECT"));
		_btnCancel->setText(tr("STR_CANCEL_UC"));
		_btnCancel->onKeyboardPress((ActionHandler)&ResearchInfoState::btnCancelClick, Options::keyCancel);
	}
	else
	{
		_btnOk->setText(tr("STR_OK"));
		_btnCancel->setText(tr("STR_CANCEL_PROJECT"));
		_btnOk->onKeyboardPress((ActionHandler)&ResearchInfoState::btnOkClick, Options::keyCancel);
	}
	_btnCancel->onMouseClick((ActionHandler)&ResearchInfoState::btnCancelClick);

	_btnMore->onMousePress((ActionHandler)&ResearchInfoState::morePress);
	_btnMore->onMouseRelease((ActionHandler)&ResearchInfoState::moreRelease);
	_btnMore->onMouseClick((ActionHandler)&ResearchInfoState::moreClick, 0);
	_btnLess->onMousePress((ActionHandler)&ResearchInfoState::lessPress);
	_btnLess->onMouseRelease((ActionHandler)&ResearchInfoState::lessRelease);
	_btnLess->onMouseClick((ActionHandler)&ResearchInfoState::lessClick, 0);

#ifdef __EMSCRIPTEN__
	Calypso::CalypsoF09ResearchUi::configure(*this);
#endif
}
void ResearchInfoState::cancelPreview()
{
	if (!_transaction.pending())
	{
		return;
	}
	if (!_transaction.cancel().applied)
	{
		return;
	}

	delete _project;
	_project = nullptr;
	_rule = nullptr;
}

void ResearchInfoState::commitPreview()
{
	if (!_transaction.pending())
	{
		return;
	}
	const auto transition = _transaction.start();
	if (!transition.applied)
	{
		return;
	}
	_base->setScientists(_base->getScientists() - transition.assigned);
	RuleResearch *rule = _rule;
	ResearchProject *preview = _project;
	int rng = RNG::generate(50, 150);
	int randomizedCost = rule->getCost() * rng / 100;
	if (rule->getCost() > 0)
	{
		randomizedCost = std::max(1, randomizedCost);
	}

	ResearchProject *project = new ResearchProject(rule, randomizedCost);
	project->setAssigned(transition.assigned);
	_base->addResearch(project);
	if (rule->isHoldingNeededItem())
	{
		_base->getStorageItems()->removeItem(rule->getNeededItem(), 1);
	}
	_game->getSavedGame()->setResearchRuleStatus(rule->getName(), RuleResearch::RESEARCH_STATUS_NORMAL);

	delete preview;
	_project = project;
	_rule = nullptr;
}

ResearchInfoState::~ResearchInfoState()
{
	if (_transaction.pending())
	{
		cancelPreview();
	}
#ifdef __EMSCRIPTEN__
	delete _hdAdapter;
	_hdAdapter = nullptr;
#endif
	delete _timerLess;
	delete _timerMore;
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void ResearchInfoState::btnOkClick(Action *)
{
	if (_transaction.pending())
	{
		commitPreview();
	}
	_game->popState();
}

/**
 * Returns to the previous screen, removing the current project from the active
 * research list.
 * @param action Pointer to an action.
 */
void ResearchInfoState::btnCancelClick(Action *)
{
	if (_transaction.pending())
	{
		cancelPreview();
	}
	else if (_project)
	{
		_base->removeResearch(_project);
		_project = nullptr;
	}
	_game->popState();
}

/**
 * Updates count of assigned/free scientists and available lab space.
 */
void ResearchInfoState::setAssignedScientist()
{
	int availableScientist = _base->getAvailableScientists();
	if (_transaction.pending())
	{
		availableScientist -= _transaction.assigned();
	}
	_txtAvailableScientist->setText(tr("STR_SCIENTISTS_AVAILABLE_UC").arg(availableScientist));
	int freeSpaceLab = _base->getFreeLaboratories();
	if (_transaction.pending())
	{
		freeSpaceLab -= _transaction.assigned();
	}
	_txtAvailableSpace->setText(tr("STR_LABORATORY_SPACE_AVAILABLE_UC").arg(freeSpaceLab));
	_txtAllocatedScientist->setText(tr("STR_SCIENTISTS_ALLOCATED").arg(_project->getAssigned()));
#ifdef __EMSCRIPTEN__
	if (_hdAdapter != nullptr)
		_hdAdapter->refresh();
#endif
}

/**
 * Increases or decreases the scientists according the mouse-wheel used.
 * @param action Pointer to an action.
 */
void ResearchInfoState::handleWheel(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP) moreByValue(Options::changeValueByMouseWheel);
	else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN) lessByValue(Options::changeValueByMouseWheel);
}
/**
 * Starts the timeMore timer.
 * @param action Pointer to an action.
 */
void ResearchInfoState::morePress(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT) _timerMore->start();
}

void ResearchInfoState::moreRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerMore->setInterval(250);
		_timerMore->stop();
	}
}

/**
 * Allocates scientists to the current project;
 * one scientist on left-click, all scientists on right-click.
 * @param action Pointer to an Action.
 */
void ResearchInfoState::moreClick(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
		moreByValue(INT_MAX);
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
		moreByValue(1);
}

/**
 * Starts the timeLess timer.
 * @param action Pointer to an Action.
 */
void ResearchInfoState::lessPress(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT) _timerLess->start();
}

/**
 * Stops the timeLess timer.
 * @param action Pointer to an Action.
 */
void ResearchInfoState::lessRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerLess->setInterval(250);
		_timerLess->stop();
	}
}

/**
 * Removes scientists from the current project;
 * one scientist on left-click, all scientists on right-click.
 * @param action Pointer to an Action.
 */
void ResearchInfoState::lessClick(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
		lessByValue(INT_MAX);
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
		lessByValue(1);
}

void ResearchInfoState::allAvailableClick(Action *)
{
	moreByValue(INT_MAX);
}

void ResearchInfoState::removeAllClick(Action *)
{
	lessByValue(INT_MAX);
}

/**
 * Adds one scientist to the project if possible.
 */
void ResearchInfoState::more()
{
	_timerMore->setInterval(50);
	moreByValue(1);
}

/**
 * Adds the given number of scientists to the project if possible.
 * @param change Number of scientists to add.
 */
void ResearchInfoState::moreByValue(int change)
{
	if (0 >= change) return;
	int freeScientist = _base->getAvailableScientists();
	int freeSpaceLab = _base->getFreeLaboratories();
	if (_transaction.pending())
	{
		freeScientist -= _transaction.assigned();
		freeSpaceLab -= _transaction.assigned();
	}
	if (freeScientist > 0 && freeSpaceLab > 0)
	{
		change = std::min(std::min(freeScientist, freeSpaceLab), change);
		_project->setAssigned(_project->getAssigned()+change);
		if (_transaction.pending())
		{
			_transaction.setAssigned(_project->getAssigned());
		}
		else
		{
			_base->setScientists(_base->getScientists()-change);
		}
		setAssignedScientist();
	}
}

/**
 * Removes one scientist from the project if possible.
 */
void ResearchInfoState::less()
{
	_timerLess->setInterval(50);
	lessByValue(1);
}

/**
 * Removes the given number of scientists from the project if possible.
 * @param change Number of scientists to subtract.
 */
void ResearchInfoState::lessByValue(int change)
{
	if (0 >= change) return;
	int assigned = _project->getAssigned();
	if (assigned > 0)
	{
		change = std::min(assigned, change);
		_project->setAssigned(assigned-change);
		if (_transaction.pending())
		{
			_transaction.setAssigned(_project->getAssigned());
		}
		else
		{
			_base->setScientists(_base->getScientists()+change);
		}
		setAssignedScientist();
	}
}

/**
 * Runs state functionality every cycle (used to update the timer).
 */
void ResearchInfoState::think()
{
	State::think();

	_timerLess->think (this, 0);
	_timerMore->think (this, 0);
}

}

#ifdef __EMSCRIPTEN__
namespace OpenXcom
{
void ResearchInfoState::resize(int &dX, int &dY)
{
	if (Calypso::CalypsoF09ResearchUi::resize(*this)) return;
	State::resize(dX, dY);
}
}
#endif
