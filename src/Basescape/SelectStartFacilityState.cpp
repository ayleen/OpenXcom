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
#include "SelectStartFacilityState.h"
#include <algorithm>
#include "../Interface/Text.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/SavedGame.h"
#include "../Engine/Options.h"
#include "PlaceStartFacilityState.h"
#include "PlaceLiftState.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Build Facilities window.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param state Pointer to the base state to refresh.
 * @param globe Pointer to the globe to refresh.
 */
SelectStartFacilityState::SelectStartFacilityState(Base *base, State *state, Globe *globe) : BuildFacilitiesState(base, state), _globe(globe)
{
	_facilities = _game->getMod()->getCustomBaseFacilities(_game->getSavedGame()->getDifficulty());

#ifdef __EMSCRIPTEN__
	for (auto *rule : _facilities)
	{
		auto entry = std::find_if(_requirements.begin(), _requirements.end(),
			[rule](const Requirement &r) { return r.rule == rule; });
		if (entry == _requirements.end()) _requirements.push_back({rule, 1, 0});
		else ++entry->total;
	}
#endif
	_btnOk->setText(tr("STR_RESET"));
	_btnOk->onMouseClick((ActionHandler)&SelectStartFacilityState::btnOkClick);
	_btnOk->onKeyboardPress(0, Options::keyCancel);

	_lstFacilities->onMouseClick((ActionHandler)&SelectStartFacilityState::lstFacilitiesClick);
}

/**
 *
 */
SelectStartFacilityState::~SelectStartFacilityState()
{

}

/**
 * Populates the build list from the current "available" facilities.
 */
void SelectStartFacilityState::populateBuildList()
{
	_lstFacilities->clearList();
#ifdef __EMSCRIPTEN__
	if (_confirmReset)
	{
		_txtTitle->setText(tr("STR_CAL_RESET_LAYOUT"));
		_btnOk->setText(tr("STR_CANCEL"));
		_lstFacilities->addRow(1, tr("STR_CAL_KEEP_LAYOUT").c_str());
		_lstFacilities->addRow(1, tr("STR_RESET").c_str());
		return;
	}
	int placed = 0, total = 0;
	for (const auto &entry : _requirements)
	{
		placed += entry.placed;
		total += entry.total;
		const std::string row = tr("STR_CAL_START_REQUIREMENT").arg(tr(entry.rule->getType())).arg(entry.placed).arg(entry.total);
		_lstFacilities->addRow(1, row.c_str());
	}
	_txtTitle->setText(tr("STR_CAL_START_PLACED").arg(placed).arg(total));
	_btnOk->setText(tr("STR_RESET"));
#else
	for (const auto* rule : _facilities)
	{
		_lstFacilities->addRow(1, tr(rule->getType()).c_str());
	}
#endif
}

/**
 * Resets the base building.
 * @param action Pointer to an action.
 */
void SelectStartFacilityState::btnOkClick(Action *)
{
#ifdef __EMSCRIPTEN__
	_confirmReset = !_confirmReset;
	_btnOk->onKeyboardPress(_confirmReset ? (ActionHandler)&SelectStartFacilityState::btnOkClick : nullptr, Options::keyCancel);
	populateBuildList();
#else
	resetLayout();
#endif
}

#ifdef __EMSCRIPTEN__
bool SelectStartFacilityState::hdFacilityRowEnabled(size_t row) const
{
	if (_confirmReset) return row < 2;
	return row < _requirements.size() && _requirements[row].placed < _requirements[row].total;
}
#endif

void SelectStartFacilityState::resetLayout()
{
	for (auto* fac : *_base->getFacilities())
	{
		delete fac;
	}
	_base->getFacilities()->clear();
	_game->popState();
	_game->popState();
	_game->pushState(PlaceLiftState::create(_base, _globe, true));
}

/**
 * Places the selected facility.
 * @param action Pointer to an action.
 */
void SelectStartFacilityState::lstFacilitiesClick(Action *)
{
	const auto row = _lstFacilities->getSelectedRow();
#ifdef __EMSCRIPTEN__
	if (!hdFacilityRowEnabled(row)) return;
	if (_confirmReset)
	{
		if (row == 1) resetLayout();
		else btnOkClick(nullptr);
		return;
	}
	_placing = _requirements[row].rule;
	_game->pushState(new PlaceStartFacilityState(_base, this, _placing));
#else
	if (row < _facilities.size()) _game->pushState(new PlaceStartFacilityState(_base, this, _facilities[row]));
#endif
}

/**
 * Callback from PlaceStartFacilityState.
 * Removes placed facility from the list.
 */
void SelectStartFacilityState::facilityBuilt()
{
#ifdef __EMSCRIPTEN__
	auto next = std::find(_facilities.begin(), _facilities.end(), _placing);
	if (next == _facilities.end()) return;
	auto entry = std::find_if(_requirements.begin(), _requirements.end(),
		[this](const Requirement &r) { return r.rule == _placing; });
	if (entry == _requirements.end()) return;
	++entry->placed;
	_facilities.erase(next);
	_placing = nullptr;
#else
	_facilities.erase(_facilities.begin() + _lstFacilities->getSelectedRow());
#endif
	if (_facilities.empty())
	{
		_game->popState();
		_game->popState(); // return to geoscape, force timer to start.
	}
	else
	{
		populateBuildList();
	}
}

}
