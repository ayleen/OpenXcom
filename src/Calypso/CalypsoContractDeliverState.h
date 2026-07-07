#ifdef __EMSCRIPTEN__
#pragma once
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
 * Phase 38.3c (Calypso) -- base-picker Deliver modal.
 *
 * Pops up from CalypsoEconomyState when the player clicks an Accepted
 * contract on the Contracts tab. Lists every base whose stores hold at
 * least the contract quantity of the requested item; selecting a base
 * hands off to Economy::deliver and pops back to the Economy screen.
 */
#include "../Engine/State.h"
#include <vector>

namespace OpenXcom
{

class TextButton;
class Window;
class Text;
class TextList;
class Base;

/**
 * Modal base-picker for fulfilling an Accepted contract. Lists only bases
 * that currently hold enough of the requested item to satisfy the contract.
 */
class CalypsoContractDeliverState : public State
{
private:
	int _contractId;
	Window* _window;
	Text* _txtTitle, *_txtInfo;
	TextButton* _btnCancel;
	TextList* _lstBases;
	std::vector<Base*> _eligible;   // bases holding >= qty (row-aligned with _lstBases)
	/// Rebuilds the eligible-bases list from the current save.
	void refresh();
public:
	/// Creates the Deliver modal for the given contract id.
	explicit CalypsoContractDeliverState(int contractId);
	/// Cleans up the Deliver modal.
	~CalypsoContractDeliverState() override = default;
	/// Initialises the screen (populates the base list).
	void init() override;
	/// Handler for clicking the Cancel button.
	void btnCancelClick(Action* action);
	/// Handler for clicking a base row (attempts delivery).
	void lstBaseClick(Action* action);
};

}

#endif /* __EMSCRIPTEN__ */
