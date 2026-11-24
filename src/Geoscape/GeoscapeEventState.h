#pragma once
/*
 * Copyright 2010-2019 OpenXcom Developers.
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
#include "../Savegame/GeoscapeEvent.h"
#include "../Engine/State.h"

namespace OpenXcom
{

namespace Calypso { class CalypsoBaseLedger; }
class TextButton;
class TextList;
class ToggleTextButton;
class Window;
class Text;
class RuleEvent;

/**
 * Displays info about a custom Geoscape event.
 */
class GeoscapeEventState : public State
{
private:
	Window *_window;
	Text *_txtTitle, *_txtMessage;
	Text *_txtItem, *_txtQuantity;
	TextButton *_btnOk;
	ToggleTextButton *_btnItemsArriving;
	TextList *_lstTransfers;

	GeoscapeEvent *_pending;
	bool _initialized = false;
	std::string _researchName;
	std::string _bonusResearchName;
	const RuleEvent &_eventRule;

	/// Helper performing event logic.
	void eventLogic(StrategicEventReceipt &receipt);
	void presentEvent();
public:
	/// Creates the GeoscapeEventState.
	GeoscapeEventState(const RuleEvent& eventRule, GeoscapeEvent *pending = nullptr);
	/// Cleans up the GeoscapeEventState.
	~GeoscapeEventState();
	/// Initializes the state.
	void init() override;
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the ItemsArriving button.
	void btnItemsArrivingClick(Action *action);
#ifdef __EMSCRIPTEN__
	void resize(int &dX, int &dY) override;
private:
	friend class Calypso::CalypsoBaseLedger;
	Calypso::CalypsoBaseLedger *_ledger = nullptr;
	TextList *_narrative = nullptr;
	int _narrativeWidth = 0, _narrativeFont = 0, _narrativeMinimumRows = 0;
#endif
};

}
