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
#include "TargetInfoState.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoBaseLedger.h"
#include "../Interface/TextList.h"
#endif
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Mod/AlienRace.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextEdit.h"
#include "../Savegame/MovingTarget.h"
#include "../Savegame/AlienBase.h"
#include "../Savegame/MissionSite.h"
#include "../Engine/Options.h"
#include "InterceptState.h"
#include "../Engine/Action.h"
#include "../Battlescape/BriefingLightState.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Target Info window.
 * @param game Pointer to the core game.
 * @param target Pointer to the target to show info from.
 * @param globe Pointer to the Geoscape globe.
 */
TargetInfoState::TargetInfoState(Target *target, Globe *globe) : _target(target), _globe(globe), _deploymentRule(0)
{
#ifdef __EMSCRIPTEN__
	_screen = true;
#else
	_screen = false;
#endif

	// Create objects
	_window = new Window(this, 192, 120, 32, 40,
#ifdef __EMSCRIPTEN__
		POPUP_NONE
#else
		POPUP_BOTH
#endif
	);
	_btnIntercept = new TextButton(160, 12, 48, 124);
	_btnInfo = new TextButton(77, 12, 48, 140);
	_btnOk = new TextButton(77, 12, 131, 140);
	_edtTitle = new TextEdit(this, 182, 32, 37, 46);
	_txtTargetted = new Text(182, 9, 37, 78);
	_txtFollowers = new Text(182, 40, 37, 88);
	_txtPenalty = new Text(160, 9, 48, 114);

	// Set palette
	setInterface("targetInfo");

	add(_window, "window", "targetInfo");
	add(_btnIntercept, "button", "targetInfo");
	add(_btnInfo, "button", "targetInfo");
	add(_btnOk, "button", "targetInfo");
	add(_edtTitle, "text2", "targetInfo");
	add(_txtTargetted, "text1", "targetInfo");
	add(_txtFollowers, "text1", "targetInfo");
	add(_txtPenalty, "text2", "targetInfo");

	centerAllSurfaces();


	// Set up objects
	setWindowBackground(_window, "targetInfo");

	_btnIntercept->setText(tr("STR_INTERCEPT"));
	_btnIntercept->onMouseClick((ActionHandler)&TargetInfoState::btnInterceptClick);

	_btnInfo->setText(tr("STR_INFO"));
	_btnInfo->onMouseClick((ActionHandler)&TargetInfoState::btnInfoClick);

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&TargetInfoState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&TargetInfoState::btnOkClick, Options::keyCancel);

	_edtTitle->setBig();
	_edtTitle->setAlign(ALIGN_CENTER);
	_edtTitle->setVerticalAlign(ALIGN_MIDDLE);
	_edtTitle->setWordWrap(true);
	_edtTitle->setText(_target->getName(_game->getLanguage()));
	_edtTitle->onChange((ActionHandler)&TargetInfoState::edtTitleChange);

	_txtTargetted->setAlign(ALIGN_CENTER);
	_txtTargetted->setText(tr("STR_TARGETTED_BY"));
	_txtFollowers->setAlign(ALIGN_CENTER);
	std::ostringstream ss;
	for (const auto* follower : *_target->getFollowers())
	{
		ss << follower->getName(_game->getLanguage()) << '\n';
	}
	_txtFollowers->setText(ss.str());

	// info
	MissionSite* m = dynamic_cast<MissionSite*>(_target);
	AlienBase* b = dynamic_cast<AlienBase*>(_target);

	if (m != 0)
	{
		_deploymentRule = _game->getMod()->getDeployment(m->getDeployment()->getType());
	}
	else if (b != 0)
	{
		AlienRace *race = _game->getMod()->getAlienRace(b->getAlienRace());
		_deploymentRule = _game->getMod()->getDeployment(race->getBaseCustomMission());
		if (!_deploymentRule) _deploymentRule = _game->getMod()->getDeployment(b->getDeployment()->getType());
	}

	if (_deploymentRule && !_deploymentRule->getAlertDescription().empty())
	{
		// all OK
	}
	else
	{
		_btnInfo->setVisible(false);
		_btnOk->setWidth(_btnOk->getX() + _btnOk->getWidth() - _btnInfo->getX());
		_btnOk->setX(_btnInfo->getX());
	}

	if (_deploymentRule && _deploymentRule->getDespawnPenalty() != 0)
	{
		_txtPenalty->setAlign(ALIGN_CENTER);
		_txtPenalty->setText(tr("STR_DESPAWN_PENALTY").arg(_deploymentRule->getDespawnPenalty()));
	}
#ifdef __EMSCRIPTEN__
	_apply = new TextButton(72, 16);
	add(_apply, "button", "targetInfo");
	_apply->setText(tr("STR_CAL_APPLY_NAME"));
	_apply->onMouseClick((ActionHandler)&TargetInfoState::applyName);
	_facts = new TextList(182, 64);
	add(_facts, "text2", "targetInfo");
	_facts->setColumns(2, 80, 102);
	_facts->setBackground(_window);
	_facts->addRow(2, tr("STR_TYPE").c_str(), tr(_target->getType()).c_str());
	_facts->addRow(2, tr("STR_CAL_CONTACT_FOLLOWERS").c_str(), std::to_string(_target->getFollowers()->size()).c_str());
	for (const auto *follower : *_target->getFollowers())
		_facts->addRow(2, tr("STR_TARGETTED_BY").c_str(), follower->getName(_game->getLanguage()).c_str());
	if (!_txtPenalty->getText().empty())
		_facts->addRow(2, tr("STR_CAL_CONTACT_PENALTY").c_str(), _txtPenalty->getText().c_str());
	_edtTitle->setFocus(false);
	_hdLedger = new Calypso::CalypsoBaseLedger(*this);
#endif
}

/**
 *
 */
TargetInfoState::~TargetInfoState()
{
#ifdef __EMSCRIPTEN__
	delete _hdLedger;
#endif

}

/**
 * Picks a craft to intercept the UFO.
 * @param action Pointer to an action.
 */
void TargetInfoState::btnInterceptClick(Action *)
{
	_game->pushState(new InterceptState(_globe, false, 0, _target));
}

/**
 * Closes the window.
 * @param action Pointer to an action.
 */
void TargetInfoState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Shows the BriefingLight screen.
 * @param action Pointer to an action.
 */
void TargetInfoState::btnInfoClick(Action *)
{
	_game->pushState(new BriefingLightState(_deploymentRule));
}

/**
 * Changes the target name.
 * @param action Pointer to an action.
 */
void TargetInfoState::edtTitleChange(Action *action)
{
#ifdef __EMSCRIPTEN__
	(void)action;
	if (_hdLedger) _hdLedger->refresh();
#else
	if (_edtTitle->getText() == _target->getDefaultName(_game->getLanguage()))
	{
		_target->setName("");
	}
	else
	{
		_target->setName(_edtTitle->getText());
	}
	if (action->getDetails()->key.keysym.sym == SDLK_RETURN ||
		action->getDetails()->key.keysym.sym == SDLK_KP_ENTER)
	{
		_edtTitle->setText(_target->getName(_game->getLanguage()));
	}
#endif
}

#ifdef __EMSCRIPTEN__
void TargetInfoState::applyName(Action *)
{
	const auto draft = _edtTitle->getText();
	_target->setName(draft == _target->getDefaultName(_game->getLanguage()) ? "" : draft);
	_edtTitle->setText(_target->getName(_game->getLanguage()));
	_edtTitle->setFocus(false);
	_hdLedger->refresh();
}

void TargetInfoState::resize(int &dX, int &dY)
{
	if (_hdLedger) _hdLedger->refresh();
	else State::resize(dX,dY);
}
#endif

}
