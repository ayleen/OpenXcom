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
#include "../Engine/State.h"

namespace OpenXcom
{

class Base;
#ifdef __EMSCRIPTEN__
namespace Calypso { class CalypsoBaseLedger; }
#endif
class TextButton;
class TextEdit;
class Window;
class Text;
class TextList;

/**
 * Transfers window that displays all
 * the items currently in-transit to a base.
 */
class TransfersState : public State
{
private:
#ifdef __EMSCRIPTEN__
	friend class Calypso::CalypsoBaseLedger;
	TextButton *_hdScope = nullptr, *_hdCategory = nullptr, *_hdSort = nullptr;
	TextEdit *_hdSearch = nullptr;
	bool _allBases = false, _latestFirst = false;
	int _cargo = -1;
	void refreshShipments(Action *action);
	void configureShipmentFilters();
	Calypso::CalypsoBaseLedger *_hdLedger = nullptr;
#endif
	Base *_base;
	int _nextArrivalHours = -1;

	TextButton *_btnOk;
	Window *_window;
	Text *_txtTitle, *_txtItem, *_txtQuantity, *_txtArrivalTime;
	TextList *_lstTransfers;
public:
#ifdef __EMSCRIPTEN__
	void resize(int &dX, int &dY) override;
#endif
	/// Creates the Transfers state.
	TransfersState(Base *base);
	/// Cleans up the Transfers state.
	~TransfersState();
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
};

}
