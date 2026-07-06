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
 * Phase 38.3a (Calypso) -- Economy screen skeleton.
 *
 * Replaces FundingState under emscripten with a tabbed view: Grants,
 * Contracts, Standing. This task ships the skeleton + the Grants tab;
 * Contracts/Standing are populated by task 38.3b.
 */
#include "../Engine/State.h"

namespace OpenXcom
{

class TextButton;
class Window;
class Text;
class TextList;

/**
 * Calypso economy screen: a tabbed window that supersedes the vanilla
 * FundingState when a `calypsoEconomy:` ruleset is active.
 */
class CalypsoEconomyState : public State
{
private:
	Window *_window;
	Text *_txtTitle;
	TextButton *_btnOk;
	TextButton *_btnGrants, *_btnContracts, *_btnStanding;
	TextList *_lstMain;
	/// Holds the currently-pressed tab button for the TextButton group idiom.
	TextButton *_tabGroupHack;
	int _tab;                    // 0=Grants, 1=Contracts, 2=Standing
	std::vector<int> _contractRowId;   // Contracts tab: list row -> contract id (-1 = non-actionable)
	/// Sets _tab, updates the group's pressed button, repopulates the list.
	void showTab(int tab);
	void populateGrants();
	void populateContracts();
	void populateStanding();
	void lstMainClick(Action* action); // row click dispatcher (acts only on the Contracts tab)
public:
	/// Creates the Calypso Economy state.
	CalypsoEconomyState();
	/// Cleans up the Calypso Economy state.
	~CalypsoEconomyState() override = default;
	/// Initialises the screen (repopulates the active tab).
	void init() override;
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the Grants tab.
	void btnGrantsClick(Action *action);
	/// Handler for clicking the Contracts tab.
	void btnContractsClick(Action *action);
	/// Handler for clicking the Standing tab.
	void btnStandingClick(Action *action);
};

}

#endif /* __EMSCRIPTEN__ */
