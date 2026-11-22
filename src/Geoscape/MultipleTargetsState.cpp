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
#include "MultipleTargetsState.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoBaseLedger.h"
#include "../Interface/TextList.h"
#endif
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Savegame/Target.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Savegame/Ufo.h"
#include "GeoscapeState.h"
#include "ConfirmDestinationState.h"
#include "InterceptState.h"
#include "UfoDetectedState.h"
#include "GeoscapeCraftState.h"
#include "TargetInfoState.h"
#include "../Engine/Options.h"
#include "../Engine/Action.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Multiple Targets window.
 * @param game Pointer to the core game.
 * @param targets List of targets to display.
 * @param craft Pointer to craft to retarget (NULL if none).
 * @param state Pointer to the Geoscape state.
 */
MultipleTargetsState::MultipleTargetsState(std::vector<Target*> targets, std::vector<Craft*> crafts, GeoscapeState *state, bool useCustomSound) :
	_targets(targets), _crafts(std::move(crafts)), _state(state), _useCustomSound(useCustomSound)
{
	_screen = false;

	if (_targets.size() > 1)
	{
#ifdef __EMSCRIPTEN__
		_screen = true;
		setInterface("multipleTargets");
		_window = new Window(this, 160, 120);
		add(_window, "window", "multipleTargets");
		_contacts = new TextList(136, 86);
		add(_contacts, "button", "multipleTargets");
		_contacts->setColumns(2, 82, 54);
		_contacts->setSelectable(true);
		_contacts->setBackground(_window);
		_contacts->onMouseClick((ActionHandler)&MultipleTargetsState::chooseContact);
		_contacts->onKeyboardPress((ActionHandler)&MultipleTargetsState::chooseContact, SDLK_SPACE);
		for (auto *target : _targets)
			_contacts->addRow(2, target->getName(_game->getLanguage()).c_str(), tr(target->getType()).c_str());
		_details = new TextButton(60, 16);
		_open = new TextButton(60, 16);
		_cancel = new TextButton(60, 16);
		for (auto *button : {_details, _open, _cancel}) add(button, "button", "multipleTargets");
		_details->setText(tr("STR_INFO"));
		_details->onMouseClick((ActionHandler)&MultipleTargetsState::inspectPicked);
		_open->setText(tr("STR_CAL_OPEN_CONTACT"));
		_open->onMouseClick((ActionHandler)&MultipleTargetsState::openPicked);
		_open->onKeyboardPress((ActionHandler)&MultipleTargetsState::openPicked, Options::keyOk);
		_cancel->setText(tr("STR_CANCEL"));
		_cancel->onMouseClick((ActionHandler)&MultipleTargetsState::btnCancelClick);
		_cancel->onKeyboardPress((ActionHandler)&MultipleTargetsState::btnCancelClick, Options::keyCancel);
		_hdLedger = new Calypso::CalypsoBaseLedger(*this);
#else
		int winHeight = BUTTON_HEIGHT * _targets.size() + SPACING * (_targets.size() - 1) + MARGIN * 2;
		int winY = (200 - winHeight) / 2;
		int btnY = winY + MARGIN;

		// Create objects
		_window = new Window(this, 136, winHeight, 60, winY, POPUP_VERTICAL);

		// Set palette
		setInterface("multipleTargets");

		add(_window, "window", "multipleTargets");

		// Set up objects
		setWindowBackground(_window, "multipleTargets");

		int y = btnY;
		for (size_t i = 0; i < _targets.size(); ++i)
		{
			TextButton *button = new TextButton(116, BUTTON_HEIGHT, 70, y);
			button->setText(_targets[i]->getName(_game->getLanguage()));
			button->onMouseClick((ActionHandler)&MultipleTargetsState::btnTargetClick);
			add(button, "button", "multipleTargets");

			_btnTargets.push_back(button);

			y += button->getHeight() + SPACING;
		}
		_btnTargets[0]->onKeyboardPress((ActionHandler)&MultipleTargetsState::btnCancelClick, Options::keyCancel);

		centerAllSurfaces();
#endif
	}
}

/**
 *
 */
MultipleTargetsState::~MultipleTargetsState()
{
#ifdef __EMSCRIPTEN__
	delete _hdLedger;
#endif

}

/**
 * Resets the palette and ignores the window
 * if there's only one target.
 */
void MultipleTargetsState::init()
{
	if (_targets.empty())
	{
		_game->popState();
		return;
	}
	if (_targets.size() == 1)
	{
		popupTarget(*_targets.begin());
	}
	else
	{
		State::init();
	}
}

/**
 * Displays the right popup for a specific target.
 * @param target Pointer to target.
 */
void MultipleTargetsState::popupTarget(Target *target)
{
	_game->popState();
	if (_crafts.size() == 0)
	{
		Base* b = dynamic_cast<Base*>(target);
		Craft* c = dynamic_cast<Craft*>(target);
		Ufo* u = dynamic_cast<Ufo*>(target);
		if (b != 0)
		{
			_game->pushState(new InterceptState(_state->getGlobe(), _useCustomSound, b));
		}
		else if (c != 0)
		{
			_game->pushState(new GeoscapeCraftState(c, _state->getGlobe(), 0, _useCustomSound));
		}
		else if (u != 0)
		{
			_game->pushState(new UfoDetectedState(u, _state, false, u->getHyperDetected()));
		}
		else
		{
			_game->pushState(new TargetInfoState(target, _state->getGlobe()));
		}
	}
	else
	{
		_game->pushState(new ConfirmDestinationState(_crafts, target));
	}
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void MultipleTargetsState::btnCancelClick(Action *)
{
	_game->popState();
}

/**
 * Pick a target to display.
 * @param action Pointer to an action.
 */
void MultipleTargetsState::btnTargetClick(Action *action)
{
	for (size_t i = 0; i < _btnTargets.size(); ++i)
	{
		if (action->getSender() == _btnTargets[i])
		{
			popupTarget(_targets[i]);
			break;
		}
	}
}

#ifdef __EMSCRIPTEN__
void MultipleTargetsState::chooseContact(Action *)
{
	_picked = _contacts->getSelectedRow();
	_showDetails = false;
}

void MultipleTargetsState::inspectPicked(Action *)
{
	if (_picked < _targets.size()) _showDetails = !_showDetails;
}

void MultipleTargetsState::openPicked(Action *)
{
	if (_picked < _targets.size()) popupTarget(_targets[_picked]);
}
#endif

#ifdef __EMSCRIPTEN__
void MultipleTargetsState::resize(int &dX, int &dY)
{
	if (_hdLedger) _hdLedger->refresh();
	else State::resize(dX,dY);
}
#endif

}
