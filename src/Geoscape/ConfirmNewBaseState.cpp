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
#include "ConfirmNewBaseState.h"
#include "Globe.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextButton.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Region.h"
#include "../Mod/RuleRegion.h"
#include "../Savegame/Base.h"
#include "../Basescape/PlaceLiftState.h"
#include "../Menu/ErrorMessageState.h"
#include "../Engine/Options.h"
#include "../Engine/RNG.h"
#include "../Engine/Unicode.h"
#include "../Mod/RuleInterface.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoAbandonPopupUi.h" // hdHarnessDomShow/Hide (shared)
#include "../Calypso/CalypsoF21TransactionUi.h"
#include "../Calypso/CalypsoHdHarnessHostState.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Confirm New Base window.
 * Phase 46.F21: the window also stages the base name (merged transaction).
 * @param game Pointer to the core game.
 * @param base Pointer to the base to place.
 * @param globe Pointer to the Geoscape globe.
 */
ConfirmNewBaseState::ConfirmNewBaseState(Base *base, Globe *globe) : _base(base), _globe(globe), _cost(0)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 224, 96, 48, 52);
	_btnOk = new TextButton(54, 12, 48, 122);
	_btnCancel = new TextButton(54, 12, 118, 122);
	_txtCost = new Text(150, 9, 48, 66);
	_txtArea = new Text(150, 9, 48, 76);
	_edtName = new TextEdit(this, 127, 16, 64, 92);

	// Set palette
	setInterface("geoscape");

	add(_window, "genericWindow", "geoscape");
	add(_btnOk, "genericButton2", "geoscape");
	add(_btnCancel, "genericButton2", "geoscape");
	add(_txtCost, "genericText", "geoscape");
	add(_txtArea, "genericText", "geoscape");
	add(_edtName, "genericText", "geoscape");

	centerAllSurfaces();

	// Set up objects
	setWindowBackground(_window, "geoscape");

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ConfirmNewBaseState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ConfirmNewBaseState::btnOkClick, Options::keyOk);

	_btnCancel->setText(tr("STR_CANCEL_UC"));
	_btnCancel->onMouseClick((ActionHandler)&ConfirmNewBaseState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&ConfirmNewBaseState::btnCancelClick, Options::keyCancel);

	std::string area;
	for (const auto* region : *_game->getSavedGame()->getRegions())
	{
		if (region->getRules()->insideRegion(_base->getLongitude(), _base->getLatitude()))
		{
			_cost = region->getRules()->getBaseCost();
			area = tr(region->getRules()->getType());
			break;
		}
	}

	_txtCost->setText(tr("STR_COST_").arg(Unicode::formatFunding(_cost)));

	_txtArea->setText(tr("STR_AREA_").arg(area));

	// Stage a ruleset random name suggestion (committed only by Create).
	if (!_game->getMod()->getBaseNamesFirst().empty())
	{
		std::ostringstream ss;
		int pickFirst = RNG::seedless(0, _game->getMod()->getBaseNamesFirst().size() - 1);
		ss << _game->getMod()->getBaseNamesFirst().at(pickFirst);
		if (!_game->getMod()->getBaseNamesMiddle().empty())
		{
			int pickMiddle = RNG::seedless(0, _game->getMod()->getBaseNamesMiddle().size() - 1);
			ss << " " << _game->getMod()->getBaseNamesMiddle().at(pickMiddle);
		}
		if (!_game->getMod()->getBaseNamesLast().empty())
		{
			int pickLast = RNG::seedless(0, _game->getMod()->getBaseNamesLast().size() - 1);
			ss << " " << _game->getMod()->getBaseNamesLast().at(pickLast);
		}
		_edtName->setText(ss.str());
	}

	//something must be in the name before it is acceptable
	_btnOk->setVisible(!_edtName->getText().empty());

	_edtName->setFocus(true, false);
	_edtName->onChange((ActionHandler)&ConfirmNewBaseState::edtNameChange);
#ifdef __EMSCRIPTEN__
	// F21 (Phase 46.F21): physical-route configure for the merged transaction.
	Calypso::CalypsoF21TransactionUi::configure(*this, true);
#endif
}

/**
 *
 */
ConfirmNewBaseState::~ConfirmNewBaseState()
{
#ifdef __EMSCRIPTEN__
	if (_hdLayout) Calypso::hdHarnessDomHide();
	Calypso::calypsoHdHarnessClose();
	delete _hdAdapter;
	_hdAdapter = nullptr;
#endif
}

#ifdef __EMSCRIPTEN__
void ConfirmNewBaseState::resize(int &dX, int &dY)
{
	if (Calypso::CalypsoF21TransactionUi::resize(*this)) return;
	State::resize(dX, dY);
}
#endif

/**
 * Commits the merged site/cost/name transaction atomically: funds deduction,
 * SavedGame insertion, and name assignment happen together, then the state
 * stack unrolls to the Geoscape and lift placement begins (Phase 46.F21
 * review, Atomic creation contract).
 * @param action Pointer to an action.
 */
void ConfirmNewBaseState::btnOkClick(Action *)
{
	if (_edtName->getText().empty()) return;

	if (_game->getSavedGame()->getFunds() >= _cost)
	{
		_game->getSavedGame()->setFunds(_game->getSavedGame()->getFunds() - _cost);
		_game->getSavedGame()->getBases()->push_back(_base);
		_base->setName(_edtName->getText());
		_game->popState(); // pop ConfirmNewBaseState
		_game->popState(); // pop BuildNewBaseState
		_game->pushState(new PlaceLiftState(_base, _globe, false));
	}
	else
	{
		_game->pushState(new ErrorMessageState(tr("STR_NOT_ENOUGH_MONEY"), _palette, _game->getMod()->getInterface("geoscape")->getElement("genericWindow")->color, "BACK01.SCR", _game->getMod()->getInterface("geoscape")->getElement("palette")->color));
	}
}

/**
 * Cancels the transaction: the provisional base is deleted, nothing is
 * committed, and the stack returns to the Geoscape (Phase 46.F21 review).
 * @param action Pointer to an action.
 */
void ConfirmNewBaseState::btnCancelClick(Action *)
{
	_globe->onMouseOver(0);
	delete _base;
	_game->popState(); // pop ConfirmNewBaseState
	_game->popState(); // pop BuildNewBaseState
}

/**
 * Enter submits the staged name when non-empty; otherwise toggles Create.
 * @param action Pointer to an action.
 */
void ConfirmNewBaseState::edtNameChange(Action *action)
{
	if (action->getDetails()->key.keysym.sym == SDLK_RETURN ||
		action->getDetails()->key.keysym.sym == SDLK_KP_ENTER)
	{
		if (!_edtName->getText().empty())
		{
			btnOkClick(action);
		}
	}
	else
	{
		_btnOk->setVisible(!_edtName->getText().empty());
	}
}

}
