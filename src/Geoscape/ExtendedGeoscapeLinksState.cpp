/*
 * Copyright 2010-2021 OpenXcom Developers.
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

#include "ExtendedGeoscapeLinksState.h"
#include "FundingState.h"
#include "GeoscapeState.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Engine/Options.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Unicode.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Interface/TextButton.h"
#include "../Menu/NotesState.h"
#include "../Menu/TestState.h"

#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoStrategicList.h"
#include "../Calypso/CalypsoHdUiOverlay.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the ExtendedGeoscapeLinksState screen.
 */
ExtendedGeoscapeLinksState::ExtendedGeoscapeLinksState(GeoscapeState* parent) : _parent(parent)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 256, 180, 32, 10,
#ifdef __EMSCRIPTEN__
		POPUP_NONE
#else
		POPUP_BOTH
#endif
	);
	_txtTitle = new Text(220, 17, 50, 33);
	if (Options::oxceFatFingerLinks)
	{
		_btnFunding = new TextButton(116, 25, 44, 50);
		_btnTechTree = new TextButton(116, 25, 161, 50);
		_btnGlobalResearch = new TextButton(116, 25, 44, 76);
		_btnGlobalProduction = new TextButton(116, 25, 161, 76);
		_btnUfoTracker = new TextButton(116, 25, 44, 102);
		_btnPilotExp = new TextButton(116, 25, 161, 102);
		_btnNotes = new TextButton(116, 25, 44, 128);
		_btnMusic = new TextButton(116, 25, 161, 128);
		_btnTest = new TextButton(116, 25, 44, 154);
		_btnOk = new TextButton(116, 25, 161, 154);
	}
	else
	{
		_btnFunding = new TextButton(220, 12, 50, 50);
		_btnTechTree = new TextButton(220, 12, 50, 63);
		_btnGlobalResearch = new TextButton(220, 12, 50, 76);
		_btnGlobalProduction = new TextButton(220, 12, 50, 89);
		_btnUfoTracker = new TextButton(220, 12, 50, 102);
		_btnPilotExp = new TextButton(220, 12, 50, 115);
		_btnNotes = new TextButton(220, 12, 50, 128);
		_btnMusic = new TextButton(220, 12, 50, 141);
		_btnTest = new TextButton(220, 12, 50, 154);
		_btnOk = new TextButton(220, 12, 50, 167);
	}

	// Set palette
	setInterface("oxceLinks");

	add(_window, "window", "oxceLinks");
	add(_txtTitle, "text", "oxceLinks");
	add(_btnOk, "button", "oxceLinks");

	add(_btnFunding, "button", "oxceLinks");
	add(_btnTechTree, "button", "oxceLinks");
	add(_btnGlobalResearch, "button", "oxceLinks");
	add(_btnGlobalProduction, "button", "oxceLinks");
	add(_btnUfoTracker, "button", "oxceLinks");
	add(_btnPilotExp, "button", "oxceLinks");
	add(_btnNotes, "button", "oxceLinks");
	add(_btnMusic, "button", "oxceLinks");
	add(_btnTest, "button", "oxceLinks");

	centerAllSurfaces();

	// Set up objects
	setWindowBackground(_window, "oxceLinks");

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_EXTENDED_LINKS"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ExtendedGeoscapeLinksState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ExtendedGeoscapeLinksState::btnOkClick, Options::keyCancel);

	_btnFunding->setText(tr("STR_FUNDING_UC"));
	_btnFunding->onMouseClick((ActionHandler)&ExtendedGeoscapeLinksState::btnFundingClick);

	std::string tmp = tr("STR_TECH_TREE_VIEWER");
	Unicode::upperCase(tmp);
	_btnTechTree->setText(tmp);
	_btnTechTree->onMouseClick((ActionHandler)&ExtendedGeoscapeLinksState::btnTechTreeClick);

	_btnGlobalResearch->setText(tr("STR_RESEARCH_OVERVIEW"));
	_btnGlobalResearch->onMouseClick((ActionHandler)&ExtendedGeoscapeLinksState::btnGlobalResearchClick);

	_btnGlobalProduction->setText(tr("STR_PRODUCTION_OVERVIEW"));
	_btnGlobalProduction->onMouseClick((ActionHandler)&ExtendedGeoscapeLinksState::btnGlobalProductionClick);

	tmp = tr("STR_UFO_TRACKER");
	Unicode::upperCase(tmp);
	_btnUfoTracker->setText(tmp);
	_btnUfoTracker->onMouseClick((ActionHandler)&ExtendedGeoscapeLinksState::btnUfoTrackerClick);

	tmp = tr("STR_DAILY_PILOT_EXPERIENCE");
	Unicode::upperCase(tmp);
	_btnPilotExp->setText(tmp);
	_btnPilotExp->onMouseClick((ActionHandler)&ExtendedGeoscapeLinksState::btnPilotExpClick);

	tmp = tr("STR_NOTES");
	Unicode::upperCase(tmp);
	_btnNotes->setText(tmp);
	_btnNotes->onMouseClick((ActionHandler)&ExtendedGeoscapeLinksState::btnNotesClick);

	tmp = tr("STR_SELECT_MUSIC_TRACK");
	Unicode::upperCase(tmp);
	_btnMusic->setText(tmp);
	_btnMusic->onMouseClick((ActionHandler)&ExtendedGeoscapeLinksState::btnMusicClick);

	if (Options::debug)
	{
		_btnTest->setText(tr("STR_TOGGLE_DEBUG_MODE"));
	}
	else
	{
		_btnTest->setText(tr("STR_TEST_SCREEN"));
	}
	_btnTest->onMouseClick((ActionHandler)&ExtendedGeoscapeLinksState::btnTestClick);
#ifdef __EMSCRIPTEN__
	_destinations = new TextList(220, 120);
	add(_destinations, "list", "oxceLinks");
	_destinations->setColumns(2, 120, 90);
	_destinations->setSelectable(true);
	_destinations->setBackground(_window);
	for (Surface *surface : _surfaces)
	{
		auto *button = dynamic_cast<TextButton*>(surface);
		if (!button || button == _btnOk || !button->getVisible()) continue;
		_destinations->addRow(2, button->getText().c_str(), tr("STR_GEOSCAPE").c_str());
		_routes.push_back(button);
		button->setVisible(false);
	}
	_destinations->onMouseClick((ActionHandler)&ExtendedGeoscapeLinksState::openDestination);
	_destinations->onKeyboardPress((ActionHandler)&ExtendedGeoscapeLinksState::openDestination, Options::keyOk);
	_hdList = new Calypso::CalypsoStrategicList(this, _parent, _game->getMod(), _window, _txtTitle, _destinations, _btnOk);
	enableUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
#endif
}

ExtendedGeoscapeLinksState::~ExtendedGeoscapeLinksState()
{
#ifdef __EMSCRIPTEN__
	delete _hdList;
#endif
}

#ifdef __EMSCRIPTEN__
void ExtendedGeoscapeLinksState::openDestination(Action *)
{
	const auto selected = _destinations->getSelectedRow();
	if (selected >= _routes.size()) return;
	SDL_Event event{};
	event.type = SDL_MOUSEBUTTONUP;
	event.button.button = SDL_BUTTON_LEFT;
	Action click(&event, 1.0, 1.0, 0, 0);
	click.setSender(_routes[selected]);
	_routes[selected]->mouseClick(&click, this);
}
#endif

void ExtendedGeoscapeLinksState::btnFundingClick(Action *)
{
	_game->popState();
	_game->pushState(new FundingState);
}

void ExtendedGeoscapeLinksState::btnTechTreeClick(Action *)
{
	_game->popState();
	_parent->btnTechTreeViewerClick(nullptr);
}

void ExtendedGeoscapeLinksState::btnGlobalResearchClick(Action *)
{
	_game->popState();
	_parent->btnGlobalResearchClick(nullptr);
}

void ExtendedGeoscapeLinksState::btnGlobalProductionClick(Action *)
{
	_game->popState();
	_parent->btnGlobalProductionClick(nullptr);
}

void ExtendedGeoscapeLinksState::btnUfoTrackerClick(Action *)
{
	_game->popState();
	_parent->btnUfoTrackerClick(nullptr);
}

void ExtendedGeoscapeLinksState::btnPilotExpClick(Action *)
{
	_game->popState();
	_parent->btnDogfightExperienceClick(nullptr);
}

void ExtendedGeoscapeLinksState::btnNotesClick(Action *)
{
	_game->popState();
	_game->pushState(new NotesState(OPT_GEOSCAPE));
}

void ExtendedGeoscapeLinksState::btnMusicClick(Action *)
{
	_game->popState();
	_parent->btnSelectMusicTrackClick(nullptr);
}

void ExtendedGeoscapeLinksState::btnTestClick(Action *)
{
	_game->popState();
	if (Options::debug)
	{
		_parent->btnDebugClick(nullptr);
	}
	else
	{
		_game->pushState(new TestState);
	}
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void ExtendedGeoscapeLinksState::btnOkClick(Action *)
{
	_game->popState();
}

#ifdef __EMSCRIPTEN__
void ExtendedGeoscapeLinksState::resize(int &dX, int &dY)
{
	if (!_hdList) { State::resize(dX, dY); return; }
	_hdList->layout();
	recaptureUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
}
#endif

}
