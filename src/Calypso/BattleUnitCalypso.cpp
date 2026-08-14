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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Calypso-specific BattleUnit behavior. Keep browser-only scripted-scene
 * implementation out of the native Savegame translation unit.
 */
#ifdef __EMSCRIPTEN__

#include "../Savegame/BattleUnit.h"
#include "../Battlescape/AIModule.h"

namespace OpenXcom
{

void BattleUnit::grantScriptedPlayerControl()
{
	_scriptedPlayerControl = true;
	_faction = FACTION_PLAYER;
	if (_currentAIState)
	{
		delete _currentAIState;
		_currentAIState = 0;
	}
}

bool BattleUnit::hasScriptedPlayerControl() const
{
	return _scriptedPlayerControl;
}

void BattleUnit::setScriptedConcealed(bool concealed)
{
	_scriptedConcealed = concealed;
	if (concealed)
		_visible = false;
}

bool BattleUnit::isScriptedConcealed() const
{
	return _scriptedConcealed;
}

void BattleUnit::prepareScriptedPlayerTurn()
{
	if (!_scriptedPlayerControl)
		return;

	// A scripted handoff is not mind control: it remains player-controlled
	// across turns, receives normal TU recovery, and must never retain a
	// neutral/hostile AI module from before the handoff.
	_faction = FACTION_PLAYER;
	if (_currentAIState)
	{
		delete _currentAIState;
		_currentAIState = 0;
	}
}

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
