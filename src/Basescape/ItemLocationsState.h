#pragma once
/*
 * Copyright 2010-2025 OpenXcom Developers.
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
#include "../Engine/State.h"
#include <functional>
#include <vector>

namespace OpenXcom
{

class Window;
class Text;
#ifdef __EMSCRIPTEN__
namespace Calypso { class CalypsoInventoryLedger; }
#endif
class TextButton;
class ToggleTextButton;
class TextList;
class RuleItem;

/**
 * Window which displays item locations (i.e. which base has how much).
 */
class ItemLocationsState : public State
{
private:
#ifdef __EMSCRIPTEN__
	friend class Calypso::CalypsoInventoryLedger;
	Calypso::CalypsoInventoryLedger *_hdLedger = nullptr;
#endif
	Window *_window;
	Text *_txtTitle;
	Text *_txtBase, *_txtQuantity;
	TextList *_lstLocations;
	TextButton *_btnOk;
	ToggleTextButton *_btnScope;
	const RuleItem *_item;
	int _scopeTotal = 0;
	int _scopeInbound = 0;
	std::vector<std::function<void()>> _openLocation;
	void updateList();
	void lstLocationClick(Action *action);
	void btnScopeClick(Action *);
public:
#ifdef __EMSCRIPTEN__
	void resize(int &dX, int &dY) override;
#endif
	/// Creates the ItemLocationsState state.
	explicit ItemLocationsState(const RuleItem* selectedItem, bool allLocations = false);
	/// Cleans up the ItemLocationsState state
	~ItemLocationsState();
	void init() override;
	/// Handler for clicking the OK button.
	void btnOkClick(Action* action);
};
}
