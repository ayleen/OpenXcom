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
/*
 * Calypso - emscripten-only Geoscape task-checklist chip + panel for
 * GeoscapeState. Member declarations live in Geoscape/GeoscapeState.h inside
 * its #ifdef __EMSCRIPTEN__ section (Phase 39); bodies extracted here per the
 * BattlescapeHud.cpp precedent.
 */
#ifdef __EMSCRIPTEN__

#include "../Geoscape/GeoscapeState.h"
#include "CalypsoTutorial.h"
#include "CalypsoChecklist.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Mod/Mod.h"
#include <sstream>

namespace OpenXcom
{

void GeoscapeState::calypsoChecklistBuild()
{
	const auto& items = _game->getMod()->getCalypsoChecklist();
	int maxLines = (int)items.size();
	if (maxLines < 1) maxLines = 1;

	_btnCalTasks = new TextButton(44, 12, 2, 2);
	_calTaskWindow = new Window(this, 148, 12 + 8 * maxLines, 2, 16, POPUP_NONE);
	_calTaskText = new Text(140, 8 * maxLines, 6, 20);

	add(_btnCalTasks, "button", "geoscape");
	add(_calTaskWindow, "genericWindow", "geoscape");
	add(_calTaskText, "genericText", "geoscape");

	_btnCalTasks->setText(tr("STR_CAL_TUT_TASKS"));
	_btnCalTasks->onMouseClick((ActionHandler)&GeoscapeState::btnCalTasksClick);

	calypsoChecklistRefresh();
}

void GeoscapeState::calypsoChecklistRefresh()
{
	if (!_btnCalTasks) return;
	const auto& items = _game->getMod()->getCalypsoChecklist();

	int visibleCount = 0;
	bool active = CalypsoTutorial::get().isActive(_game) && !items.empty();
	if (active)
		for (const auto& it : items)
			if (CalypsoChecklist::isVisible(_game, it)) ++visibleCount;
	if (visibleCount == 0) active = false;

	if (!active)
	{
		_btnCalTasks->setVisible(false);
		_calTaskWindow->setVisible(false);
		_calTaskText->setVisible(false);
		return;
	}

	_btnCalTasks->setVisible(true);
	bool open = CalypsoTutorial::get().checklistOpen();
	_calTaskWindow->setVisible(open);
	_calTaskText->setVisible(open);

	std::ostringstream ss;
	bool first = true;
	for (const auto& it : items)
	{
		if (!CalypsoChecklist::isVisible(_game, it)) continue;
		if (!first) ss << "\n";
		first = false;
		ss << (CalypsoChecklist::isDone(_game, it) ? "x " : "- ") << tr(it.label);
	}
	_calTaskText->setText(ss.str());
}

void GeoscapeState::btnCalTasksClick(Action *)
{
	bool open = !CalypsoTutorial::get().checklistOpen();
	CalypsoTutorial::get().setChecklistOpen(open);
	_calTaskWindow->setVisible(open);
	_calTaskText->setVisible(open);
}

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
