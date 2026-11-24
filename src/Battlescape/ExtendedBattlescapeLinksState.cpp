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

#include "ExtendedBattlescapeLinksState.h"
#ifdef __EMSCRIPTEN__
#include "../Calypso/CalypsoStrategicList.h"
#include "../Calypso/CalypsoReportLines.h"
#endif
#include "BattlescapeGame.h"
#include "BattlescapeState.h"
#include "BriefingState.h"
#include "InfoboxState.h"
#include "Map.h"
#include "NoExperienceState.h"
#include "TurnDiaryState.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Menu/NotesState.h"
#include "../Mod/Mod.h"
#include "../Savegame/HitLog.h"
#include "../Savegame/SavedBattleGame.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the ExtendedBattlescapeLinksState screen.
 */
ExtendedBattlescapeLinksState::ExtendedBattlescapeLinksState(BattlescapeState* parent, SavedBattleGame* save) : _parent(parent), _save(save)
{
#ifdef __EMSCRIPTEN__
	_screen = true;
#else
	_screen = false;
#endif

	// Create objects
#ifdef __EMSCRIPTEN__
	_window = new Window(this, 256, 180, 32, 10);
#else
	_window = new Window(this, 256, 180, 32, 10, POPUP_BOTH);
#endif
	_txtTitle = new Text(220, 17, 50, 33);
	if (Options::oxceFatFingerLinks)
	{
		_btnTouch = new TextButton(116, 25, 44, 50);
		_btnNightVision = new TextButton(116, 25, 161, 50);
		_btnPersonalLights = new TextButton(116, 25, 44, 76);
		_btnBrightness = new TextButton(116, 25, 161, 76);
		_btnTurnDiary = new TextButton(116, 25, 44, 102);
		_btnBriefing = new TextButton(116, 25, 161, 102);
		_btnNotes = new TextButton(116, 25, 44, 128);
		_btnMusic = new TextButton(116, 25, 161, 128);
		_btnKillAll = new TextButton(116, 25, 44, 154);
		_btnOk = new TextButton(116, 25, 161, 154);
	}
	else
	{
		_btnTouch = new TextButton(220, 12, 50, 50);
		_btnNightVision = new TextButton(220, 12, 50, 63);
		_btnPersonalLights = new TextButton(220, 12, 50, 76);
		_btnBrightness = new TextButton(220, 12, 50, 89);
		_btnTurnDiary = new TextButton(220, 12, 50, 102);
		_btnBriefing = new TextButton(220, 12, 50, 115);
		_btnNotes = new TextButton(220, 12, 50, 128);
		_btnMusic = new TextButton(220, 12, 50, 141);
		_btnKillAll = new TextButton(220, 12, 50, 154);
		_btnOk = new TextButton(220, 12, 50, 167);
	}

	// Set palette
	setInterface("oxceLinks", false, _save);

	add(_window, "window", "oxceLinks");
	add(_txtTitle, "text", "oxceLinks");
	add(_btnOk, "button", "oxceLinks");

	add(_btnTouch, "button", "oxceLinks");
	add(_btnNightVision, "button", "oxceLinks");
	add(_btnPersonalLights, "button", "oxceLinks");
	add(_btnBrightness, "button", "oxceLinks");
	add(_btnTurnDiary, "button", "oxceLinks");
	add(_btnBriefing, "button", "oxceLinks");
	add(_btnNotes, "button", "oxceLinks");
	add(_btnMusic, "button", "oxceLinks");
	add(_btnKillAll, "button", "oxceLinks");

	centerAllSurfaces();

	// Set up objects
#ifndef __EMSCRIPTEN__
	setWindowBackground(_window, "oxceLinks");
#endif

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_EXTENDED_LINKS"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ExtendedBattlescapeLinksState::btnOkClick, Options::keyCancel);

	_btnTouch->setText(tr("STR_TOGGLE_TOUCH_BUTTONS"));
	_btnTouch->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnTouchClick);

	_btnNightVision->setText(tr("STR_TOGGLE_NIGHT_VISION"));
	_btnNightVision->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnNightVisionClick);

	_btnPersonalLights->setText(tr("STR_TOGGLE_PERSONAL_LIGHTING"));
	_btnPersonalLights->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnPersonalLightsClick);

	_btnBrightness->setText(tr("STR_TOGGLE_BRIGHTNESS"));
	_btnBrightness->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnBrightnessClick);

	_btnTurnDiary->setText(tr("STR_HIT_LOG"));
	_btnTurnDiary->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnTurnDiaryClick);

	_btnBriefing->setText(tr("STR_BRIEFING"));
	_btnBriefing->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnBriefingClick);

	_btnNotes->setText(Options::oxceReplaceNotesLink ? tr("STR_NO_EXPERIENCE_YET") : tr("STR_NOTES"));
	_btnNotes->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnNotesClick);

	_btnMusic->setText(tr("STR_SELECT_MUSIC_TRACK"));
	_btnMusic->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnMusicClick);

	if (Options::debug)
	{
		_btnKillAll->setText(_save->getDebugMode() ? tr("STR_DEBUG_KILL_ALL_ALIENS") : tr("STR_TOGGLE_DEBUG_MODE"));
	}
	else
	{
		_btnKillAll->setText(tr("STR_MULTI_LEVEL_VIEW"));
	}
	_btnKillAll->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnKillAllClick);

#ifndef __EMSCRIPTEN__
	applyBattlescapeTheme("oxceLinks");
#else
	_hdBody = new TextList(1, 1, 0, 0);
	add(_hdBody, "text", "oxceLinks");
	_hdBody->setColumns(2, 1, 1);
	_hdBody->setSelectable(true);
	_hdBody->setBackground(_window);
	for (TextButton *button : {_btnTouch,_btnNightVision,_btnPersonalLights,_btnBrightness,_btnTurnDiary,
		_btnBriefing,_btnNotes,_btnMusic,_btnKillAll})
	{
		if (!button->getVisible()) continue;
		_hdBody->addRow(2, button->getText().c_str(), tr("STR_CAL_BATTLE_CONTEXT").c_str());
		_routes.push_back(button);
		button->setVisible(false);
	}
	_hdBody->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::openDestination);
	_hdBody->onKeyboardPress((ActionHandler)&ExtendedBattlescapeLinksState::openDestination, Options::keyOk);
	_hdList = new Calypso::CalypsoStrategicList(this, nullptr, _game->getMod(), _window, _txtTitle, _hdBody,
		_btnOk, Calypso::CalypsoStrategicList::Content::TacticalLinks);
	enableUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	_hdList->fitInput();
#endif
}

ExtendedBattlescapeLinksState::~ExtendedBattlescapeLinksState()
{
#ifdef __EMSCRIPTEN__
	delete _hdList;
#endif
}

#ifdef __EMSCRIPTEN__
void ExtendedBattlescapeLinksState::openDestination(Action *)
{
	const auto selected = _hdBody->getSelectedRow();
	if (selected >= _routes.size()) return;
	SDL_Event event{};
	event.type = SDL_MOUSEBUTTONUP;
	event.button.button = SDL_BUTTON_LEFT;
	Action click(&event, 1, 1, 0, 0);
	click.setSender(_routes[selected]);
	_routes[selected]->mouseClick(&click, this);
}
void ExtendedBattlescapeLinksState::resize(int &dx, int &dy)
{
	if (!_hdList) { State::resize(dx, dy); return; }
	_hdList->layout();
	recaptureUiScaling(_hdList->designWidth(), _hdList->designHeight(), 1.0f, false);
	_hdList->fitInput();
}
#endif

void ExtendedBattlescapeLinksState::btnTouchClick(Action *)
{
	_game->popState();
	_parent->toggleTouchButtons(false, false);
}

void ExtendedBattlescapeLinksState::btnNightVisionClick(Action *)
{
	_game->popState();
	_parent->btnNightVisionClick(nullptr);
}

void ExtendedBattlescapeLinksState::btnPersonalLightsClick(Action *)
{
	_game->popState();
	_parent->btnPersonalLightingClick(nullptr);
}

void ExtendedBattlescapeLinksState::btnBrightnessClick(Action *)
{
	_game->popState();
	_parent->getMap()->toggleDebugVisionMode();
}

void ExtendedBattlescapeLinksState::btnTurnDiaryClick(Action *)
{
	_game->popState();
	if (Options::oxceDisableHitLog)
	{
		_game->pushState(new InfoboxState(tr("STR_THIS_FEATURE_IS_DISABLED_4")));
	}
	else
	{
		// turn diary
		_game->pushState(new TurnDiaryState(_save->getHitLog()));
	}
}

void ExtendedBattlescapeLinksState::btnBriefingClick(Action *)
{
	_game->popState();
	_game->pushState(new BriefingState(0, 0, true));
}

void ExtendedBattlescapeLinksState::btnNotesClick(Action *)
{
	_game->popState();

	if (Options::oxceReplaceNotesLink)
		_game->pushState(new NoExperienceState());
	else
		_game->pushState(new NotesState(OPT_BATTLESCAPE));
}

void ExtendedBattlescapeLinksState::btnMusicClick(Action *)
{
	_game->popState();
	_parent->btnSelectMusicTrackClick(nullptr);
}

void ExtendedBattlescapeLinksState::btnKillAllClick(Action *)
{
	_game->popState();

	if (!Options::debug)
	{
		_parent->btnShowLayersClickOrig(nullptr);
		return;
	}

	if (_save->getDebugMode())
	{
		// kill all
		_parent->debug("Influenza bacterium dispersed");
		for (auto* unit : *_save->getUnits())
		{
			if (unit->getOriginalFaction() == FACTION_HOSTILE && !unit->isOut())
			{
				unit->damage(Position(0, 0, 0), 1000, _game->getMod()->getDamageType(DT_MELEE), _save, { });
			}
		}
		_save->getBattleGame()->checkForCasualties(nullptr, BattleActionAttack{}, true, false);
		_save->getBattleGame()->handleState();
	}
	else
	{
		// enable debug mode
		_save->setDebugMode();
		_parent->debug("Debug Mode");
	}
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void ExtendedBattlescapeLinksState::btnOkClick(Action *)
{
	_game->popState();
}

}
